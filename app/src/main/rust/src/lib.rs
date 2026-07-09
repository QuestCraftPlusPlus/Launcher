#![cfg(target_os = "android")]
extern crate jni;

use std::{sync::atomic::Ordering, sync::Arc, thread};
use jni::{jni_str, objects::JObject, signature::RuntimeMethodSignature, EnvUnowned};
use crate::jni_state::JniContext;

mod jni_state;
mod app;
mod input;
pub mod render;
mod scene;
mod instance;
mod stage;
mod xr_util;
mod surface;

#[unsafe(no_mangle)]
#[allow(non_snake_case)]
pub extern "C" fn Java_com_qcxr_questcraft_MainActivity_start<'local>(
    mut unowned_env: EnvUnowned<'local>,
    main_activity: JObject<'local>,
    xr_activity_input: JObject<'local>,
    asset_manager: JObject<'local>
) {
    android_logger::init_once(
        android_logger::Config::default()
            .with_max_level(log::LevelFilter::Trace)
            .with_tag("QCXRRust")
    );

    log::info!("Hello World!");
    std::panic::set_hook(Box::new(|panic_info| {
        let payload = panic_info.payload();
        let message = if let Some(s) = payload.downcast_ref::<&str>() {
            *s
        } else if let Some(s) = payload.downcast_ref::<String>() {
            s.as_str()
        } else {
            "Weird panic payload"
        };

        let location = panic_info.location()
            .map(|loc| format!("{}:{}:{}", loc.file(), loc.line(), loc.column()))
            .unwrap_or_else(|| "unknown location".to_string());

        log::error!("!! PANIC !! at [{}]: {}", location, message);
    }));

    unowned_env.with_env(|env| -> jni::errors::Result<_> {
        log::info!("Owned the env");
        let jvm = env.get_java_vm().unwrap();

        let main_activity_class = env.get_object_class(&main_activity).unwrap();
        let xr_input_class = env.get_object_class(&xr_activity_input).unwrap();

        let method_set_surface = env.get_static_method_id(&main_activity_class, jni_str!("setVulkanSurface"), RuntimeMethodSignature::from_str("(Landroid/view/Surface;II)V").unwrap().method_signature()).unwrap();
        let method_system_exit = env.get_static_method_id(&main_activity_class, jni_str!("performSystemExit"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();
        let method_process_pointer_event = env.get_static_method_id(&xr_input_class, jni_str!("processPointerEvent"), RuntimeMethodSignature::from_str("(IIFF)V").unwrap().method_signature()).unwrap();
        let method_request_ui_render = env.get_static_method_id(&main_activity_class, jni_str!("requestUiRender"), RuntimeMethodSignature::from_str("()V").unwrap().method_signature()).unwrap();

        let ctx = Arc::new(JniContext {
            jvm,
            main_activity: env.new_global_ref(main_activity).unwrap(),
            main_activity_class: env.new_global_ref(main_activity_class).unwrap(),
            xr_input_class: env.new_global_ref(xr_input_class).unwrap(),
            asset_manager: env.new_global_ref(asset_manager).unwrap(),
            method_system_exit,
            method_set_surface,
            method_process_pointer_event,
            method_request_ui_render
        });

        let thread_ctx = ctx.clone();
        thread::Builder::new().name("rustrenderthread".to_string()).spawn(move || {
            log::info!("Started the main thread");
            thread_ctx.jvm.attach_current_thread(|env| {
                log::info!("Attached the jvm to this thread");
                unsafe {
                    let ctx = thread_ctx.clone();
                    let ndk_asset_manager_ptr = ndk_sys::AAssetManager_fromJava(env.get_raw() as _, **ctx.asset_manager);
                    app::main_loop(env, ctx, ndk_asset_manager_ptr);
                }
                Ok::<(), jni::errors::Error>(())
            }).expect("Failed to attach thread.");
        }).unwrap();

        Ok(())
    }).resolve::<jni::errors::ThrowRuntimeExAndDefault>();
}

#[unsafe(no_mangle)]
#[allow(non_snake_case)]
pub extern "C" fn Java_com_qcxr_questcraft_MainActivity_stop(
    mut _unowned_env: EnvUnowned,
) {
    jni_state::SHOULD_STOP_JNI.store(true, Ordering::Relaxed);
}