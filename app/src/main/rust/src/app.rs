use std::thread::sleep;
use std::time::Duration;
use glam::{Mat4, Quat, Vec2, Vec3};
use jni::signature::Primitive::Void;
use log::info;
use ndk_sys::{AMOTION_EVENT_ACTION_DOWN, AMOTION_EVENT_ACTION_MOVE, AMOTION_EVENT_ACTION_UP};
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
        signature::{ReturnType}
    },
    ndk::asset::AssetManager,
    ndk_sys::AAssetManager,
    openxr as xr,
};
use crate::input::{ExtractedInputs};
use crate::stage::Stage;
use crate::render::renderer::{PollError, Renderer, WaitError};
use crate::scene::scene::Scene;
use crate::surface::{Surface, SurfaceManager, SurfaceTexture};

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

    let mut context = XrContext::new(Arc::clone(&ctx));
    let mut renderer = Renderer::new(&context);
    let input = InputState::new(&context.instance, &context.session.session);
    let scene = Scene::load(&context.instance.device, &asset_manager);
    // if you're looking how to load assets n shit, it's in the scene ^^

    let mut surface = Surface::new(env, 1920, 1080);
    let java_surface = jni::sys::jvalue { l: surface.java_surface.as_raw() };
    let width = jni::sys::jvalue { i: 1920 };
    let height = jni::sys::jvalue { i: 1080 };
    unsafe {
        env.call_static_method_unchecked(&ctx.main_activity_class, ctx.method_set_surface, ReturnType::Primitive(Void), &[
            java_surface, width, height
        ]).expect("Failed to set surface");
    }

    let surface_index = scene.surface_index.expect("Scene mesh must contain a node tagged as a surface! (see custom properties in Blender)");
    let surface_node = &scene.assets.gltf_scene.nodes[surface_index];
    let surface_mesh =
        &scene.assets.gltf_scene.meshes[surface_node.mesh_index.expect("Scene node tagged as surface doesn't contain a mesh (what are we supposed to render the texture onto??)")].primitives[0];
    let surface_transform = surface_node.global_transform;

    let surface_manager = SurfaceManager::new(&asset_manager, &context.instance.device, surface_mesh);

    let spawn = scene.spawn_point.unwrap_or(Mat4::IDENTITY);
    let (_, rotation, translation) = spawn.to_scale_rotation_translation();
    let mut stage = Stage::new(translation, rotation, 1.0f32);

    let mut old_surface_textures = Vec::new();
    let mut last_surface_texture: Option<Arc<SurfaceTexture>> = None;
    let mut previous_inputs: Option<ExtractedInputs> = None;

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

        old_surface_textures.retain(|texture: &Arc<SurfaceTexture>| {
            Arc::strong_count(&texture.image) > 1
        });

        unsafe {
            let _ = env.call_static_method_unchecked(
                &ctx.main_activity_class,
                ctx.method_request_ui_render,
                ReturnType::Primitive(Void),
                &[],
            );
        } // hope to fuck this finishes before we need the texture

        let inputs = input.extract(&context.session.session, &context.stage, active_frame.predicted_display_time);

        let (_, views) = context.session.session.locate_views(
            ViewConfigurationType::PRIMARY_STEREO,
            active_frame.predicted_display_time,
            &context.stage,
        ).expect("Failed to locate tracking views");

        let left_eye_rot: mint::Quaternion<f32> = views[0].pose.orientation.into();
        let right_eye_rot: mint::Quaternion<f32> = views[1].pose.orientation.into();
        let head_rotation = <mint::Quaternion<f32> as Into<Quat>>::into(left_eye_rot.into()).slerp(right_eye_rot.into(), 0.5);

        let left_eye_pos: mint::Vector3<f32> = views[0].pose.position.into();
        let right_eye_pos: mint::Vector3<f32> = views[1].pose.position.into();
        let eye_pos = (<mint::Vector3<f32> as Into<Vec3>>::into(left_eye_pos.into()) + <mint::Vector3<f32> as Into<Vec3>>::into(right_eye_pos.into())) * 0.5;

        stage.move_relative(inputs.movement, head_rotation, delta_time);

        let surface_texture = surface.update_texture(context.instance.device.clone(), &context.instance.android_hardware_buffer);
        if let Some(surface_texture) = surface_texture {
            last_surface_texture = Some(Arc::new(surface_texture));
        }

        let world_to_stage = stage.world_to_stage_matrix();
        let stage_to_world = world_to_stage.inverse();
        let eye_pos = stage_to_world.transform_point3(eye_pos);

        let mut pointer_transform = None;

        if let Some(ref transform) = inputs.right_hand_matrix {
            let transform = stage_to_world * transform * Mat4::from_translation(Vec3::new(0.0,0.0,0.127));
            let hit = surface_manager.raycast_uv(transform, surface_transform, -Vec3::Z);

            if let Some(hit) = hit {
                let uv = hit.uv;
                pointer_transform = Some(Mat4::from_scale_rotation_translation(
                    Vec3::ONE,
                    Quat::from_rotation_arc(Vec3::Y, hit.world_normal.normalize()),
                    hit.world_position
                ));

                if let Some(previous_inputs) = previous_inputs {
                    publish_inputs_for_pointer(env, &ctx, 0, previous_inputs.right_click_state, inputs.right_click_state, uv);
                }
            }
        }

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

        let result = renderer.draw(&mut context, active_frame, payload, |graph, draw_payload| {
            scene.record(graph, draw_payload);
            if let Some(ref transform) = inputs.left_hand_matrix {
                let transform = stage_to_world * transform;
                scene.record_controller(graph, draw_payload, &transform, None);
            }
            if let Some(ref last_surface_texture) = last_surface_texture {
                surface_manager.record_with_transform(graph, last_surface_texture.image.clone(), draw_payload.camera_ubo, draw_payload.swapchain_image, draw_payload.depth_image, surface_transform);
            }
            if let Some(ref transform) = pointer_transform {
                scene.record_pointer(graph, draw_payload, transform);
            }
            if let Some(ref transform) = inputs.right_hand_matrix {
                let transform = stage_to_world * transform;
                let mut ray_transform = None;
                if pointer_transform.is_some() {
                    ray_transform = Some(billboard_ray_transform(&transform, &eye_pos));
                }

                scene.record_controller(graph, draw_payload, &transform, ray_transform);
            }
        });
        if result.is_err() {
            log::error!("Fatal error during draw, exiting: {:?}", result.err());
            break;
        }

        previous_inputs = Some(inputs);
        if let Some(ref last_surface_texture) = last_surface_texture {
            old_surface_textures.push(Arc::clone(last_surface_texture));
        }
    }
    info!("Exiting...");

    // unsafe { // does it make sense to call this on exit?
    //     let _ = env.call_static_method_unchecked(
    //         &ctx.main_activity_class,
    //         ctx.method_system_exit,
    //         ReturnType::Primitive(Void),
    //         &[]
    //     );
    // }
}

fn publish_inputs_for_pointer(env: &mut Env<'_>, ctx: &JniContext, pointer: i32, previous_click_state: bool, current_click_state: bool, raycast_hit_uv: Vec2) {
    let pointer_id = jni::sys::jvalue { i: pointer };
    let action_val = jni::sys::jvalue { i: AMOTION_EVENT_ACTION_MOVE as _ };
    let norm_x = jni::sys::jvalue { f: raycast_hit_uv.x };
    let norm_y = jni::sys::jvalue { f: raycast_hit_uv.y };

    unsafe {
        env.call_static_method_unchecked(
            &ctx.xr_input_class,
            &ctx.method_process_pointer_event,
            ReturnType::Primitive(Void),
            &[pointer_id, action_val, norm_x, norm_y]
        ).expect("Failed to process pointer event");
    }

    let action = if !previous_click_state && current_click_state {
        Some(AMOTION_EVENT_ACTION_DOWN)
    } else if previous_click_state && !current_click_state {
        Some(AMOTION_EVENT_ACTION_UP)
    } else {
        None
    };

    if let Some(action) = action {
        let pointer_id = jni::sys::jvalue { i: pointer };
        let action_val = jni::sys::jvalue { i: action as _ };
        let norm_x = jni::sys::jvalue { f: raycast_hit_uv.x };
        let norm_y = jni::sys::jvalue { f: raycast_hit_uv.y };

        unsafe {
            env.call_static_method_unchecked(
                &ctx.xr_input_class,
                &ctx.method_process_pointer_event,
                ReturnType::Primitive(Void),
                &[pointer_id, action_val, norm_x, norm_y]
            ).expect("Failed to process pointer event");
        }
    }
}

fn billboard_ray_transform(controller_transform: &Mat4, eye_pos: &Vec3) -> Mat4 {
    let controller_pos = controller_transform.transform_point3(Vec3::ZERO);
    let controller_forward = controller_transform.transform_vector3(-Vec3::Z).normalize();

    let look_rot = Quat::from_rotation_arc_colinear(-Vec3::Z, controller_forward);

    let current_up = look_rot * Vec3::Y;

    let to_view = (eye_pos - controller_pos).normalize();
    let target_up = controller_forward.cross(to_view).cross(controller_forward).normalize();

    let twist_rot = Quat::from_rotation_arc(current_up, target_up);
    let final_rotation = twist_rot * look_rot;
    let (scale, _, _) = controller_transform.to_scale_rotation_translation();

    Mat4::from_scale_rotation_translation(
        scale,
        final_rotation,
        controller_pos
    )
}