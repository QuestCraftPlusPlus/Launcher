#![cfg(target_os = "android")]
extern crate jni;

use {
    std::{sync::atomic::Ordering, sync::Arc, thread},
    jni::{objects::{JObject, JByteArray, JClass}, EnvUnowned, jni_mangle, sys::jboolean},
    crate::jni_state::{JniContext, SkinData},
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
mod keyboard;
mod geometry;

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
        let message = panic_info
            .payload()
            .downcast_ref::<&str>()
            .copied()
            .or_else(|| panic_info.payload().downcast_ref::<String>().map(|s| s.as_str()))
            .unwrap_or("Unknown panic payload");

        let location_info = if let Some(location) = panic_info.location() {
            format!("{}:{}:{}", location.file(), location.line(), location.column())
        } else {
            "unknown location".to_string()
        };

        let thread_name = std::thread::current().name().unwrap_or("main").to_string();
        let error = format!("Exception in thread \"{}\" at {}: {}\n", thread_name, location_info, message);

        log::error!("!! PANIC !! {}", error);
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
    _unowned_env: EnvUnowned,
) {
    jni_state::SHOULD_STOP_JNI.store(true, Ordering::Relaxed);
}

#[jni_mangle("com.qcxr.questcraft.JniBridge")]
pub fn show_keyboard(
    _unowned_env: EnvUnowned,
) {
    jni_state::KEYBOARD_HIDDEN.store(false, Ordering::Relaxed);
}

#[jni_mangle("com.qcxr.questcraft.JniBridge")]
pub fn hide_keyboard(
    _unowned_env: EnvUnowned,
) {
    jni_state::KEYBOARD_HIDDEN.store(true, Ordering::Relaxed);
}

#[jni_mangle("com.qcxr.questcraft.JniBridge")]
pub fn set_skin_image<'local>(
    mut unowned_env: EnvUnowned<'local>,
    _: JClass<'local>,
    image_bytes: JByteArray<'local>,
    slim: jboolean,
) {
    unowned_env.with_env(|env| -> jni::errors::Result<_> {
        let bytes = env.convert_byte_array(&image_bytes)?;
        *jni_state::PENDING_SKIN_IMAGE.lock().unwrap() = Some(SkinData {
            png_bytes: bytes, 
            slim
        });
        Ok(())
    }).resolve::<jni::errors::ThrowRuntimeExAndDefault>();
}