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
