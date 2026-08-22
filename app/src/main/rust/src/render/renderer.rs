use std::path::Path;
use std::sync::Arc;
use log::warn;
use vk_graph::driver::buffer::Buffer;
use vk_graph::node::{AnyBufferNode, AnyImageNode};
use {
    crate::{
        app::XrContext,
        render::{
            renderer::RenderingError::{FailedToEndStream, Sleeping},
            Swapchain, XrInstance
        }
    },
    bytemuck::{Pod, Zeroable},
    glam::{vec4, Mat4, Quat, Vec3},
    openxr::{self as xr, EnvironmentBlendMode},
    std::{
        iter::repeat_with,
    },
    vk_graph::{
        cmd::ClearColorValue,
        driver::{
            ash::vk::{self},
            device::Device,
            fence::Fence,
            image::{ImageInfo, SampleCount},
        },
        pool::{
            lazy::LazyPool,
            Pool,
        },
        Graph
    }
};

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum RenderingError {
    DriverError,
    Sleeping,
    FailedToEndStream
}

pub struct Renderer<'a> {
    pub resolution: vk::Extent2D,

    pub pool: LazyPool,
    pub frame_in_flight: usize,
    max_frames_in_flight: usize,
    swapchain_queues: Box<[Option<Fence>]>,
    swapchain_rect: xr::Rect2Di,
    fixture_path: &'a Path,
    capture_fixture: bool,
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum AcquireError {
    DriverError,
}
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum PollError {
    Exiting, LossPending, Recenter
}
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum WaitError {
    DriverError, Sleeping
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Pod, Zeroable)]
pub struct XrCameraUBO {
    pub view_matrices: [Mat4; 2],
    pub projection_matrices: [Mat4; 2],
}

pub struct ActiveFrame {
    pub predicted_display_time: openxr::Time,
    pub swapchain_image_index: u32,
}

pub struct FramePayload<'a> {
    pub camera_buffer: &'a Arc<Buffer>,
    pub xr_views: &'a [openxr::View],
}

pub struct DrawPayload<'a> {
    pub frame_in_flight: usize,
    pub camera_ubo: &'a AnyBufferNode,
    pub color_target: &'a AnyImageNode,
    pub depth_target: &'a AnyImageNode,
}

pub const VIEW_MASK: u32 = !(!0 << 2);
pub const MSAA_COUNT: SampleCount = SampleCount::Type4;

impl<'a> Renderer<'a> {
    pub fn begin_frame(&mut self, xr_context: &mut XrContext) -> Result<ActiveFrame, RenderingError> {
        if !xr_context.session.running {
            return Err(Sleeping);
        }

        let frame_state = xr_context.session.wait_frame().map_err(|err| {
            if err == WaitError::Sleeping { Sleeping } else { RenderingError::DriverError }
        })?;
        let (_, swapchain_image_index) = xr_context.swapchain.acquire_image().unwrap();

        Ok(ActiveFrame{
            predicted_display_time: frame_state.predicted_display_time,
            swapchain_image_index,
        })
    }

    pub fn request_fixture(&mut self) {
        self.capture_fixture = true;
    }

    pub fn draw<F>(&mut self, xr_context: &mut XrContext, active_frame: ActiveFrame, payload: FramePayload, record_commands: F) -> Result<(), RenderingError> where
        F: FnOnce(&mut Graph, &DrawPayload) {
        #[cfg(feature = "profiled")]
        profiling::scope!("Frame");
        let mut graph = Graph::default();

        let swapchain_image = graph.bind_resource(Swapchain::image(&xr_context.swapchain, active_frame.swapchain_image_index as _));
        let depth_target = graph.bind_resource(
            self.pool.resource(ImageInfo::image_2d_array(
                self.resolution.width,
                self.resolution.height,
                 2,
                vk::Format::D32_SFLOAT,
                vk::ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT | vk::ImageUsageFlags::TRANSIENT_ATTACHMENT
            ).into_builder().sample_count(MSAA_COUNT)).unwrap().with_debug_name("main depth image")
        );

        let camera_ubo_node = graph.bind_resource(payload.camera_buffer);

        graph.clear_color_image(swapchain_image, ClearColorValue::WHITE_ALPHA_ONE);
        graph.clear_depth_stencil_image(depth_target, 1.0, 0);

        let draw_payload = DrawPayload {
            frame_in_flight: self.frame_in_flight,
            camera_ubo: &camera_ubo_node.into(),
            color_target: &swapchain_image.into(),
            depth_target: &depth_target.into(),
        };

        record_commands(&mut graph, &draw_payload);

        if self.capture_fixture {
            self.capture_fixture = false;
            graph.export_fixture(
                self.fixture_path.join("fixture_binary"),
                self.fixture_path.join("fixture.md")
            ).expect("Failed to capture fixture.");
            log::info!("Wrote fixture to {:?}", self.fixture_path);
        }

        let swapchain = &mut xr_context.swapchain;
        swapchain.wait_image(xr::Duration::INFINITE).unwrap();
        let swapchain_queue = graph.finalize().queue_submit(&mut self.pool, xr_context.queue_family_index, 0).expect("Failure during render graph finalization");
        swapchain.release_image().unwrap();
        self.swapchain_queues[active_frame.swapchain_image_index as usize] = Some(swapchain_queue);

        xr_context.session.frame_stream.end(
            active_frame.predicted_display_time,
            EnvironmentBlendMode::OPAQUE,
            &[&xr::CompositionLayerProjection::new()
                .space(&xr_context.stage_space)
                .views(&[
                    xr::CompositionLayerProjectionView::new().pose(payload.xr_views[0].pose).fov(payload.xr_views[0].fov).sub_image(xr::SwapchainSubImage::new().swapchain(&xr_context.swapchain).image_array_index(0).image_rect(self.swapchain_rect)),
                    xr::CompositionLayerProjectionView::new().pose(payload.xr_views[1].pose).fov(payload.xr_views[1].fov).sub_image(xr::SwapchainSubImage::new().swapchain(&xr_context.swapchain).image_array_index(1).image_rect(self.swapchain_rect)),
                ])
            ]
        ).map_err(|_| FailedToEndStream)?;
        self.frame_in_flight = (self.frame_in_flight + 1) % self.max_frames_in_flight;
        #[cfg(feature = "profiled")]
        profiling::finish_frame!();

        Ok(())
    }

    pub fn new(xr_context: &XrContext, fixture_path: &'a Path) -> Self {
        let resolution = xr_context.swapchain.resolution();
        let swapchain_rect = xr::Rect2Di {
            offset: xr::Offset2Di { x: 0, y: 0 },
            extent: xr::Extent2Di {
                width: resolution.width as _,
                height: resolution.height as _,
            }
        };

        let pool = LazyPool::new(XrInstance::device(&xr_context.instance));
        let swapchain_image_count = xr_context.swapchain.images().len();
        let swapchain_queues = repeat_with(|| None)
            .take(swapchain_image_count)
            .collect::<Box<_>>();

        Renderer {
            resolution,
            pool,
            frame_in_flight: 0,
            max_frames_in_flight: swapchain_image_count,
            swapchain_queues,
            swapchain_rect,
            fixture_path,
            capture_fixture: false
        }
    }
}

pub fn projection_transform(view: xr::View) -> Mat4 {
    let l = view.fov.angle_left.tan();
    let r = view.fov.angle_right.tan();
    let d = view.fov.angle_down.tan();
    let u = view.fov.angle_up.tan();

    let w = r - l;
    let h = d - u;

    let near = 0.01;
    let far = 100.0;

    Mat4::from_cols(
        vec4(2.0 / w, 0.0, 0.0, 0.0),
        vec4(0.0, 2.0 / h, 0.0, 0.0),
        vec4((r + l) / w, (u + d) / h, -(far + near) / (far - near), -1.0),
        vec4(0.0, 0.0, -(far * (near + near)) / (far - near), 0.0),
    )
}

fn view_position(view: xr::View) -> Vec3 {
    Vec3::from(mint::Vector3::from(view.pose.position))
}

pub fn view_transform(view: xr::View) -> Mat4 {
    let orientation = Quat::from(mint::Quaternion::from(view.pose.orientation));
    let translation = Vec3::from(mint::Vector3::from(view.pose.position));

    let pose_matrix = Mat4::from_rotation_translation(orientation, translation);

    pose_matrix.inverse()
}

pub fn device_queue_family_index(device: &Device, flags: vk::QueueFlags) -> Option<u32> {
    device
        .physical
        .queue_families
        .iter()
        .enumerate()
        .find(|(_, properties)| properties.queue_flags.contains(flags))
        .map(|(index, _)| index as u32)
}
