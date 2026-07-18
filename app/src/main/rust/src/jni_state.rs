use glam::Vec2;
use jni::Env;
use jni::signature::Primitive::Void;
use jni::signature::ReturnType;
use ndk_sys::{_bindgen_ty_22, AMOTION_EVENT_ACTION_MOVE};
use {
    std::sync::atomic::AtomicBool,
    jni::{
        JavaVM,
        refs::Global,
        objects::{
            JObject,
            JClass,
            JStaticMethodID
        }
    }
};
use crate::surface::Surface;

pub static SHOULD_STOP_JNI: AtomicBool = AtomicBool::new(false);

pub struct JniContext {
    pub jvm: JavaVM,
    pub main_activity: Global<JObject<'static>>,
    pub main_activity_class: Global<JClass<'static>>,
    pub xr_input_class: Global<JClass<'static>>,
    pub asset_manager: Global<JObject<'static>>,
    pub method_system_exit: JStaticMethodID,
    pub method_set_surface: JStaticMethodID,
    pub method_process_pointer_event: JStaticMethodID,
    pub method_request_ui_render: JStaticMethodID,
}

impl JniContext {
    pub fn system_exit(&self, env: &mut Env<'_>) {
        unsafe {
            let _ = env.call_static_method_unchecked(
                &self.main_activity_class,
                self.method_system_exit,
                ReturnType::Primitive(Void),
                &[]
            );
        }
    }

    pub fn request_ui_render(&self, env: &mut Env<'_>) {
        unsafe {
            let _ = env.call_static_method_unchecked(
                &self.main_activity_class,
                self.method_request_ui_render,
                ReturnType::Primitive(Void),
                &[],
            );
        }
    }

    pub fn set_surface(&self, env: &mut Env<'_>, surface: &Surface) {
        let java_surface = jni::sys::jvalue { l: surface.java_surface.as_raw() };
        let width = jni::sys::jvalue { i: surface.extent.width as _ };
        let height = jni::sys::jvalue { i: surface.extent.height as _ };
        unsafe {
            env.call_static_method_unchecked(&self.main_activity_class, self.method_set_surface, ReturnType::Primitive(Void), &[
                java_surface, width, height
            ]).expect("Failed to set surface");
        }
    }

    pub fn process_pointer_event(&self, env: &mut Env<'_>, pointer_id: i32, action: _bindgen_ty_22, raycast_hit_uv: Vec2) {
        let pointer_id = jni::sys::jvalue { i: pointer_id };
        let action_val = jni::sys::jvalue { i: action as _ };
        let norm_x = jni::sys::jvalue { f: raycast_hit_uv.x };
        let norm_y = jni::sys::jvalue { f: raycast_hit_uv.y };

        unsafe {
            env.call_static_method_unchecked(
                &self.xr_input_class,
                &self.method_process_pointer_event,
                ReturnType::Primitive(Void),
                &[pointer_id, action_val, norm_x, norm_y]
            ).expect("Failed to process pointer event");
        }
    }
}