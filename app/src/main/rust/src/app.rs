use std::thread::sleep;
use std::time::Duration;
use glam::{Mat4, Quat, Vec3};
use log::info;
use openxr::{EnvironmentBlendMode, FrameState, ViewConfigurationType};
use vk_graph::driver::ash::vk;
use {
    std::{
        time::Instant,
        ptr::NonNull,
        sync::{
            atomic::Ordering,
            Arc,
        }
    },
    crate::{
        jni_state::{self, JniContext},
        render::{renderer, Swapchain},
        input::{InputState},
        instance::XrInstance,
    },
    jni::{
        Env,
        signature::{Primitive, ReturnType}
    },
    ndk::asset::AssetManager,
    ndk_sys::AAssetManager,
    openxr as xr,
};
use crate::stage::Stage;
use crate::render::renderer::{PollError, Renderer, WaitError};
use crate::scene::scene::Scene;

pub struct XrSession {
    pub(crate) running: bool,
    pub session: xr::Session<xr::Vulkan>,
    pub frame_wait: xr::FrameWaiter,
    pub frame_stream: xr::FrameStream<xr::Vulkan>,
}

impl XrSession {
    pub fn poll(&mut self, instance: &mut XrInstance) -> Result<(), PollError> {
        while let Some(event) = XrInstance::poll_event(instance).unwrap() {
            use xr::Event::*;
            match event {
                SessionStateChanged(e) => {
                    log::debug!("entered state {:?}", e.state());
                    match e.state() {
                        xr::SessionState::READY => {
                            self.session
                                .begin(ViewConfigurationType::PRIMARY_STEREO)
                                .unwrap();
                            self.running = true;
                        }
                        xr::SessionState::STOPPING => {
                            self.session.end().unwrap();
                            self.running = false;
                        }
                        xr::SessionState::EXITING => {
                            return Err(PollError::Exiting)
                        }
                        xr::SessionState::LOSS_PENDING => {
                            return Err(PollError::LossPending)
                        }
                        _ => {}
                    }
                }
                InstanceLossPending(_) => {
                    return Err(PollError::LossPending)
                }
                EventsLost(e) => {
                    log::error!("lost {} events", e.lost_event_count());
                }
                _ => {}
            }
        };
        Ok(())
    }

    pub fn wait_frame(&mut self) -> Result<FrameState, WaitError> {
        let xr_frame_state = self.frame_wait.wait().map_err(|_err| {
            WaitError::DriverError
        })?;

        self.frame_stream.begin().unwrap();

        if !xr_frame_state.should_render {
            self.frame_stream.end(
                xr_frame_state.predicted_display_time,
                EnvironmentBlendMode::OPAQUE,
                &[]
            ).unwrap();
            return Err(WaitError::Sleeping)
        }

        Ok(xr_frame_state)
    }
}

pub struct XrContext {
    pub instance: XrInstance,
    pub queue_family_index: u32, // this is like primarily graphics, but it makes sense to force it into the "xr context" because the xr context is inherently tied to it
    pub session: XrSession,
    pub swapchain: Swapchain,
    pub stage: xr::Space,
}

impl XrContext {
    pub fn new(ctx: Arc<JniContext>) -> Self {
        let instance = XrInstance::new(&ctx).expect("Failed to create OpenXR instance");
        let device = XrInstance::device(&instance);
        let queue_family_index = renderer::device_queue_family_index(device, vk::QueueFlags::GRAPHICS | vk::QueueFlags::TRANSFER)
            .unwrap();

        let (session, frame_wait, frame_stream) =
            XrInstance::create_session(&instance, queue_family_index, 0).unwrap();

        let swapchain = Swapchain::new(&instance, &session);

        let stage = session
            .create_reference_space(xr::ReferenceSpaceType::STAGE, xr::Posef::IDENTITY)
            .unwrap();

        XrContext {
            instance,
            queue_family_index,
            session: XrSession {
                session,
                frame_wait,
                frame_stream,
                running: true,
            },
            swapchain,
            stage,
        }
    }
}

pub fn main_loop(env: &mut Env<'_>, ctx: Arc<JniContext>, raw_asset_manager: *mut AAssetManager) {
    info!("Hello from main loop!");
    let asset_manager = unsafe { AssetManager::from_ptr(NonNull::new(raw_asset_manager).expect("Null asset manager")) };

    info!("Initializing context...");
    let mut context = XrContext::new(Arc::clone(&ctx));
    info!("Initializing renderer...");
    let mut renderer = Renderer::new(&context);
    info!("Initializing input...");
    let input = InputState::new(&context.instance, &context.session.session);
    info!("Initializing scene...");
    let scene = Scene::load(&context.instance.device, &asset_manager);
    // if you're looking how to load assets n shit, it's in the scene ^^

    let spawn = scene.spawn_point.unwrap_or(Mat4::IDENTITY);
    let (_, rotation, translation) = spawn.to_scale_rotation_translation();
    let mut stage = Stage::new(translation, rotation, 1.0f32);

    info!("Starting!!");
    let mut last_frame_time = Instant::now();
    while !jni_state::SHOULD_STOP_JNI.load(Ordering::Relaxed) {
        let now = Instant::now();
        let delta_time = now.duration_since(last_frame_time).as_secs_f32();
        last_frame_time = now;

        context.session.poll(&mut context.instance).expect("Failed to poll session");

        let active_frame = match renderer.begin_frame(&mut context) {
            Ok(frame) => frame,
            Err(err) => {
                match err {
                    renderer::RenderingError::Sleeping => {
                        sleep(Duration::from_millis(100));
                        log::trace!("sleeping...")
                    },
                    _ => { panic!("{:?}", err) }
                }
                continue;
            }
        };

        let inputs = input.extract(&context.session.session, &context.stage, active_frame.predicted_display_time);

        let (_, views) = context.session.session.locate_views(
            ViewConfigurationType::PRIMARY_STEREO,
            active_frame.predicted_display_time,
            &context.stage,
        ).expect("Failed to locate tracking views");

        let left_eye_rot: mint::Quaternion<f32> = views[0].pose.orientation.into();
        let right_eye_rot: mint::Quaternion<f32> = views[1].pose.orientation.into();
        let head_rotation = <mint::Quaternion<f32> as Into<Quat>>::into(left_eye_rot.into()).slerp(right_eye_rot.into(), 0.5);

        stage.move_relative(inputs.movement, head_rotation, delta_time);

        let world_to_stage = stage.world_to_stage_matrix();
        let stage_to_world = world_to_stage.inverse();

        let payload = renderer::FramePayload {
            view_matrices: [
                renderer::view_transform(views[0]) * world_to_stage,
                renderer::view_transform(views[1]) * world_to_stage,
            ],
            projection_matrices: [
                renderer::projection_transform(views[0]),
                renderer::projection_transform(views[1]),
            ],
            predicted_display_time: active_frame.predicted_display_time,
            xr_views: &views,
        };

        renderer.draw(&mut context, active_frame, payload, |graph, draw_payload| {
            scene.record(graph, draw_payload);
            if let Some(transform) = inputs.left_hand_matrix {
                scene.record_controller(graph, draw_payload, stage_to_world * transform);
            }
            if let Some(transform) = inputs.right_hand_matrix {
                scene.record_controller(graph, draw_payload, stage_to_world * transform);
            }
        }).expect("Fatal error in draw");
    }
    info!("Exiting...");

    unsafe {
        let _ = env.call_static_method_unchecked(
            &ctx.main_activity_class,
            ctx.method_system_exit,
            ReturnType::Primitive(Primitive::Void),
            &[]
        );
    }
}