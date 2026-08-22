use {
    crate::surface::AndroidSurface,
    ndk_sys::_bindgen_ty_22,
    jni::{
        JavaVM, Env, jni_str, jni_sig,
        objects::{
            JClass,
            JObject,
            JString,
            JStaticMethodID,
        },
        refs::Global,
        signature::{
            Primitive::Void,
            ReturnType, RuntimeMethodSignature
        },
    },
    glam::Vec2,
    std::{
        path::PathBuf,
        sync::{
            Mutex,
            atomic::AtomicBool
        }
    }
};

pub static SHOULD_STOP_JNI: AtomicBool = AtomicBool::new(false);
pub static PENDING_SKIN_IMAGE: Mutex<Option<SkinData>> = Mutex::new(None);
pub static KEYBOARD_HIDDEN: AtomicBool = AtomicBool::new(true);

pub struct SkinData {
    pub png_bytes: Vec<u8>,
    pub slim: bool
}

pub struct JniContext {
    pub jvm: JavaVM,
    pub main_activity: Global<JObject<'static>>,
    pub jni_bridge_class: Global<JClass<'static>>,
    pub asset_manager: Global<JObject<'static>>,
    method_system_exit: JStaticMethodID,
    method_set_surface: JStaticMethodID,
    method_process_pointer_event: JStaticMethodID,
    method_request_ui_render: JStaticMethodID,
    method_send_text: JStaticMethodID,
    method_delete_character: JStaticMethodID,
}

impl JniContext {
    pub fn new(env: &mut Env<'_>, jvm: JavaVM, main_activity: &JObject, asset_manager: &JObject) -> Self {
        let jni_bridge_class = env.find_class(jni_str!("com/qcxr/questcraft/JniBridge")).unwrap();

        let method_set_surface = env.get_static_method_id(&jni_bridge_class, jni_str!("setVulkanSurface"), RuntimeMethodSignature::from_str("(Landroid/view/Surface;II)V").unwrap().method_signature()).unwrap();
        let method_system_exit = env.get_static_method_id(&jni_bridge_class, jni_str!("performSystemExit"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();
        let method_process_pointer_event = env.get_static_method_id(&jni_bridge_class, jni_str!("processPointerEvent"), RuntimeMethodSignature::from_str("(IIFF)V").unwrap().method_signature()).unwrap();
        let method_request_ui_render = env.get_static_method_id(&jni_bridge_class, jni_str!("requestUiRender"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();
        let method_send_text = env.get_static_method_id(&jni_bridge_class, jni_str!("sendText"), RuntimeMethodSignature::from_str("(Ljava/lang/String;)V").unwrap().method_signature()).unwrap();
        let method_delete_character = env.get_static_method_id(&jni_bridge_class, jni_str!("deleteCharacter"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();

        JniContext {
            jvm,
            main_activity: env.new_global_ref(main_activity).unwrap(),
            jni_bridge_class: env.new_global_ref(jni_bridge_class).unwrap(),
            asset_manager: env.new_global_ref(asset_manager).unwrap(),
            method_system_exit,
            method_set_surface,
            method_process_pointer_event,
            method_request_ui_render,
            method_send_text,
            method_delete_character,
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

    pub fn set_surface(&self, env: &mut Env<'_>, surface: &AndroidSurface) {
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

    pub fn send_text(&self, env: &mut Env<'_>, text: &str) {
        let text_jstring = env.new_string(text).unwrap();
        let text_jstring = jni::sys::jvalue { l: text_jstring.as_raw() };
        unsafe {
            env.call_static_method_unchecked(
                &self.jni_bridge_class,
                &self.method_send_text,
                ReturnType::Primitive(Void),
                &[text_jstring]
            ).expect("Failed to send text");
        }
    }

    pub fn delete_character(&self, env: &mut Env<'_>) {
        unsafe {
            env.call_static_method_unchecked(
                &self.jni_bridge_class,
                &self.method_delete_character,
                ReturnType::Primitive(Void),
                &[],
            ).expect("Failed to delete character");
        }
    }

    pub fn get_internal_files_dir(
        &self,
        env: &mut Env<'_>
    ) -> Result<PathBuf, Box<dyn std::error::Error>> {
        let file_obj = env
            .call_method(
                self.main_activity.as_obj(),
                jni_str!("getFilesDir"),
                jni_sig!("()Ljava/io/File;"),
                &[],
            )?
            .l()?;

        let path_jstring = env
            .call_method(
                file_obj,
                jni_str!("getAbsolutePath"),
                jni_sig!("()Ljava/lang/String;"),
                &[],
            )?
            .l()?;

        let path_rust_string = unsafe { JString::from_raw(env, path_jstring.as_raw()) }.to_string();

        Ok(PathBuf::from(path_rust_string))
    }
}