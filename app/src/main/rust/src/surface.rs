use jni::objects::JObject;
use jni::Env;
use ndk_sys::{media_status_t, AHardwareBuffer, AImageReader, AImageReader_acquireLatestImage, AImageReader_delete, AImageReader_getWindow, AImageReader_newWithUsage, AImage_delete, AImage_getHardwareBuffer, ANativeWindow_toSurface};

pub struct Surface<'a> {
    pub image_reader: *mut AImageReader,
    pub java_surface: JObject<'a>
}

impl<'a> Surface<'a> {
    pub fn new(env: &mut Env<'a>, width: i32, height: i32) -> Self {
        let image_reader = unsafe {
            let mut image_reader = std::ptr::null_mut();
            let status = AImageReader_newWithUsage(
                width,
                height,
                0x00000023, // this is format YUV_420_888, see https://developer.android.com/reference/android/graphics/ImageFormat for constant values
                1u64 << 8, // this is usage GPU_SAMPLED_IMAGE, see https://developer.android.com/ndk/reference/group/a-hardware-buffer for constant values
                4,
                &mut image_reader
            );
            if status != media_status_t::AMEDIA_OK {
                Err(status)
            } else {
                Ok(image_reader)
            }
        }.expect("Failed to create AImageReader for Surface");

        let native_window = unsafe {
            let mut window = std::ptr::null_mut();
            let status = AImageReader_getWindow(image_reader, &mut window);
            if status != media_status_t::AMEDIA_OK {
                Err(status)
            } else {
                Ok(window)
            }
        }.expect("Failed to get window for Surface");

        let java_surface = unsafe {
            let java_surface = ANativeWindow_toSurface(env.get_raw() as _, native_window);
            JObject::from_raw(env, java_surface)
        };

        Self {
            image_reader,
            java_surface,
        }
    }

    pub fn acquire_latest_buffer(&self) -> Result<*mut AHardwareBuffer, media_status_t> {
        unsafe {
            let mut image = std::ptr::null_mut();
            let status = AImageReader_acquireLatestImage(self.image_reader, &mut image);
            if status != media_status_t::AMEDIA_OK || image.is_null() {
                return Err(status);
            }

            let mut hardware_buffer = std::ptr::null_mut();
            AImage_getHardwareBuffer(image, &mut hardware_buffer);

            AImage_delete(image);

            if hardware_buffer.is_null() {
                Err(media_status_t::AMEDIA_ERROR_UNKNOWN)
            } else {
                Ok(hardware_buffer)
            }
        }
    }
}

impl<'a> Drop for Surface<'a> {
    fn drop(&mut self) {
        unsafe {
            if !self.image_reader.is_null() {
                AImageReader_delete(self.image_reader);
            }
        }
    }
}