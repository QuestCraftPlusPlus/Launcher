use {
    crate::surface::Surface,
    ndk_sys::_bindgen_ty_22,
    jni::{
        JavaVM, Env, jni_str,
        objects::{
            JClass,
            JObject,
            JStaticMethodID,
        },
        refs::Global,
        signature::{
            Primitive::Void,
            ReturnType, RuntimeMethodSignature
        },
    },
    glam::Vec2,
    std::sync::atomic::AtomicBool
};

pub static SHOULD_STOP_JNI: AtomicBool = AtomicBool::new(false);

pub struct JniContext {
    pub jvm: JavaVM,
    pub main_activity: Global<JObject<'static>>,
    pub jni_bridge_class: Global<JClass<'static>>,
    pub asset_manager: Global<JObject<'static>>,
    method_system_exit: JStaticMethodID,
    method_set_surface: JStaticMethodID,
    method_process_pointer_event: JStaticMethodID,
    method_request_ui_render: JStaticMethodID,
}

impl JniContext {
    pub fn new(env: &mut Env<'_>, jvm: JavaVM, main_activity: &JObject, asset_manager: &JObject) -> Self {
        let jni_bridge_class = env.find_class(jni_str!("com/qcxr/questcraft/JniBridge")).unwrap();

        let method_set_surface = env.get_static_method_id(&jni_bridge_class, jni_str!("setVulkanSurface"), RuntimeMethodSignature::from_str("(Landroid/view/Surface;II)V").unwrap().method_signature()).unwrap();
        let method_system_exit = env.get_static_method_id(&jni_bridge_class, jni_str!("performSystemExit"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();
        let method_process_pointer_event = env.get_static_method_id(&jni_bridge_class, jni_str!("processPointerEvent"), RuntimeMethodSignature::from_str("(IIFF)V").unwrap().method_signature()).unwrap();
        let method_request_ui_render = env.get_static_method_id(&jni_bridge_class, jni_str!("requestUiRender"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();

        JniContext {
            jvm,
            main_activity: env.new_global_ref(main_activity).unwrap(),
            jni_bridge_class: env.new_global_ref(jni_bridge_class).unwrap(),
            asset_manager: env.new_global_ref(asset_manager).unwrap(),
            method_system_exit,
            method_set_surface,
            method_process_pointer_event,
            method_request_ui_render
        }
    }

    pub fn system_exit(&self, env: &mut Env<'_>) {
        unsafe {
            let _ = env.call_static_method_unchecked(
                &self.jni_bridge_class,
                self.method_system_exit,
                ReturnType::Primitive(Void),
                &[]
            );
        }
    }

    pub fn request_ui_render(&self, env: &mut Env<'_>) {
        unsafe {
            let _ = env.call_static_method_unchecked(
                &self.jni_bridge_class,
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
            env.call_static_method_unchecked(
                &self.jni_bridge_class,
                self.method_set_surface,
                ReturnType::Primitive(Void),
                &[java_surface, width, height]
            ).expect("Failed to set surface");
        }
    }

    pub fn process_pointer_event(&self, env: &mut Env<'_>, pointer_id: i32, action: _bindgen_ty_22, raycast_hit_uv: Vec2) {
        let pointer_id = jni::sys::jvalue { i: pointer_id };
        let action_val = jni::sys::jvalue { i: action as _ };
        let norm_x = jni::sys::jvalue { f: raycast_hit_uv.x };
        let norm_y = jni::sys::jvalue { f: raycast_hit_uv.y };

        unsafe {
            env.call_static_method_unchecked(
                &self.jni_bridge_class,
                &self.method_process_pointer_event,
                ReturnType::Primitive(Void),
                &[pointer_id, action_val, norm_x, norm_y]
            ).expect("Failed to process pointer event");
        }
    }
}