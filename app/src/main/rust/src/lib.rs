#![cfg(target_os = "android")]
extern crate jni;

use jni::objects::JClass;
use {
    std::{sync::atomic::Ordering, sync::Arc, thread},
    jni::{objects::JObject, EnvUnowned, jni_mangle},
    crate::jni_state::JniContext,
};

mod jni_state;
mod app;
mod input;
mod render;
mod scene;
mod instance;
mod stage;
mod xr_util;
mod surface;
mod egui;

#[jni_mangle("com.qcxr.questcraft.JniBridge")]
pub fn start<'local>(
    mut unowned_env: EnvUnowned<'local>,
    _: JClass<'local>,
    main_activity: JObject<'local>,
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

        let ctx = Arc::new(JniContext::new(env, jvm, &main_activity, &asset_manager));

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

#[jni_mangle("com.qcxr.questcraft.JniBridge")]
pub fn stop(
    mut _unowned_env: EnvUnowned,
) {
    jni_state::SHOULD_STOP_JNI.store(true, Ordering::Relaxed);
}