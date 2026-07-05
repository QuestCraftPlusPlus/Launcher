use std::env::var;
use std::ffi::CStr;
use std::io::{stderr, IsTerminal};
use std::process::{abort, id};
use std::slice;
use std::sync::Arc;
use std::thread::{current, panicking, park};
use log::{info, logger, warn, Level, Metadata};
use vk_graph::driver::ash::ext;
use vk_graph::driver::ash::vk::{ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_NAME, KHR_EXTERNAL_MEMORY_NAME};
use vk_graph::driver::DriverError;
use {
    log::{debug, error},
    openxr as xr,
    std::{
        ffi::{c_void, c_char},
        fmt::{Debug, Formatter},
        mem::transmute,
        ops::Deref
    },
    vk_graph::driver::{
        ash::{
            self,
            vk::{self, Handle as _},
        },
        device::Device,
        instance::Instance,
        physical_device::PhysicalDevice,
    },
};
use crate::jni_state::JniContext;

pub struct XrInstance {
    pub device: Device,
    event_buf: xr::EventDataBuffer,
    instance: xr::Instance,
    system: xr::SystemId,
}

impl XrInstance {
    const XR_TARGET_VK_VERSION: xr::Version = xr::Version::new(1, 2, 0);
    const VK_TARGET_VERSION: u32 = vk::make_api_version(
        0,
        Self::XR_TARGET_VK_VERSION.major() as _,
        Self::XR_TARGET_VK_VERSION.minor() as _,
        0,
    );

    pub fn new(ctx: &Arc<JniContext>) -> Result<Self, InstanceCreateError> {
        let platform_info = unsafe {
            xr::AndroidPlatformInfo::new(ctx.jvm.get_raw() as *mut c_void, ctx.main_activity.as_ref().as_raw() as *mut c_void)
        };

        #[cfg(feature = "linked")]
        let xr_entry = xr::Entry::linked(&platform_info);

        #[cfg(not(feature = "linked"))]
        let xr_entry = unsafe { xr::Entry::load(&platform_info) }.map_err(|err| {
            debug!("OpenXR loader unavailable: {err}");

            InstanceCreateError::OpenXRUnsupported
        })?;

        let available_extensions = xr_entry.enumerate_extensions().unwrap();
        let mut required_extensions = xr::ExtensionSet::default();
        required_extensions.khr_vulkan_enable2 = true;
        required_extensions.khr_android_create_instance = true;
        required_extensions.ext_debug_utils = available_extensions.ext_debug_utils;

        let app_info = xr::ApplicationInfo {
            api_version: xr::Version::new(1, 1, 53),
            application_name: "questcraft",
            application_version: 1,
            engine_name: "questcraft",
            engine_version: 1,
        };
        let xr_instance = xr_entry
            .create_instance(&app_info, &required_extensions, &[], &platform_info)
            .map_err(|err| {
                error!("Unable to create OpenXR instance: {err}");

                InstanceCreateError::OpenXRUnsupported
            })?;

        let xr::InstanceProperties {
            runtime_name,
            runtime_version,
        } = xr_instance.properties().map_err(|err| {
            error!("OpenXR instance properties: {err}");

            InstanceCreateError::OpenXRUnsupported
        })?;

        debug!(
            "loaded OpenXR runtime: {} {}",
            runtime_name, runtime_version
        );

        let system = xr_instance
            .system(xr::FormFactor::HEAD_MOUNTED_DISPLAY)
            .map_err(|err| {
                error!("OpenXR system: {err}");

                InstanceCreateError::OpenXRUnsupported
            })?;
        if !xr_instance
            .enumerate_environment_blend_modes(system, xr::ViewConfigurationType::PRIMARY_STEREO)
            .unwrap_or_default()
            .contains(&xr::EnvironmentBlendMode::OPAQUE)
        {
            error!("OpenXR opaque blend mode not supported");

            return Err(InstanceCreateError::OpenXRUnsupported);
        }

        let xr::vulkan::Requirements {
            max_api_version_supported,
            min_api_version_supported,
        } = xr_instance
            .graphics_requirements::<xr::Vulkan>(system)
            .map_err(|err| {
                error!("OpenXR vulkan requirements: {err}");

                InstanceCreateError::OpenXRUnsupported
            })?;

        debug!(
            "OpenXR Vulkan requirements: min={}, max={}",
            min_api_version_supported, max_api_version_supported
        );

        if min_api_version_supported > Self::XR_TARGET_VK_VERSION
            || max_api_version_supported.major() < Self::XR_TARGET_VK_VERSION.major()
        {
            error!(
                "OpenXR runtime requires Vulkan version > {}, <= {}",
                min_api_version_supported,
                max_api_version_supported
            );

            return Err(InstanceCreateError::VulkanUnsupported);
        }


        let vk_extensions = [
            // vk::EXT_DEBUG_UTILS_NAME.as_ptr() // this s#!% crashes my headset for some reason

        ];
        let vk_layers = [];

        let app_info = vk::ApplicationInfo::default()
            .application_version(app_info.application_version)
            .engine_version(app_info.engine_version)
            .application_name(c"questcraft")
            .engine_name(c"questcraft")
            .api_version(Self::VK_TARGET_VERSION);
        let create_info = vk::InstanceCreateInfo::default()
            .application_info(&app_info)
            .enabled_extension_names(&vk_extensions)
            .enabled_layer_names(&vk_layers);
        unsafe {
            let entry = ash::Entry::load().map_err(|err| {
                error!("Vulkan entry point: {err}");

                InstanceCreateError::VulkanUnsupported
            })?;

            let get_instance_proc_addr = {
                assert_ne!(entry.static_fn().get_instance_proc_addr as *const (), std::ptr::null(), "vkGetInstanceProcAddr is null!");

                type Fn<T> =
                unsafe extern "system" fn(T, *const c_char) -> Option<unsafe extern "system" fn()>;
                type AshFn = Fn<vk::Instance>;
                type OpenXrFn = Fn<xr::sys::platform::VkInstance>;
                transmute::<AshFn, OpenXrFn>(entry.static_fn().get_instance_proc_addr)
            };

            info!("Available Vulkan instance extensions:");
            let extension_props = entry.enumerate_instance_extension_properties(None).unwrap();
            for ext in extension_props {
                info!("\t{}", str::from_utf8(&ext.extension_name).unwrap_or_default());
            }

            let vk_instance = {
                let vk_instance = xr_instance
                    .create_vulkan_instance(
                        system,
                        get_instance_proc_addr,
                        &create_info as *const vk::InstanceCreateInfo as *const xr::sys::platform::VkInstanceCreateInfo,
                    )
                    .map_err(|err| {
                        error!("OpenXR unable to create Vulkan instance: {err:?}");

                        InstanceCreateError::OpenXRUnsupported
                    })?
                    .map_err(vk::Result::from_raw)
                    .map_err(|err| {
                        error!("Vulkan instance create: {err}");

                        InstanceCreateError::VulkanUnsupported
                    })?;
                let vk_instance = vk::Instance::from_raw(vk_instance as _);

                Instance::try_from_entry(entry, vk_instance).map_err(|err| {
                    error!("Vulkan instance load: {err}");

                    InstanceCreateError::VulkanUnsupported
                })?
            };

            let physical_device = vk::PhysicalDevice::from_raw(
                xr_instance
                    .vulkan_graphics_device(system, vk_instance.handle().as_raw() as _)
                    .map_err(|err| {
                        error!("OpenXR unable to create Vulkan graphics device: {err}");

                        InstanceCreateError::OpenXRUnsupported
                    })? as _,
            );
            let extension_props = vk_instance.enumerate_device_extension_properties(physical_device).unwrap();
            info!("Available Vulkan device extensions:");
            for ext in extension_props {
                info!("\t{}", str::from_utf8(&ext.extension_name).unwrap_or_default());
            }

            let physical_device = PhysicalDevice::try_from_ash(&vk_instance, physical_device)
                .map_err(|err| {
                    error!("Vulkan physical device: {err}");

                    InstanceCreateError::VulkanUnsupported
                })?;

            let ash_device = physical_device
                .create_ash_device(|create_info| {
                    { // this looks silly but is like my only actual way to modify the features 1.2 struct
                        let mut curr = create_info.p_next as *mut vk::BaseInStructure;
                        while !curr.is_null() {
                            if (*curr).s_type == vk::StructureType::PHYSICAL_DEVICE_VULKAN_1_2_FEATURES {
                                let v12_features = curr as *mut vk::PhysicalDeviceVulkan12Features;

                                (*v12_features).descriptor_indexing = vk::TRUE;
                                (*v12_features).shader_sampled_image_array_non_uniform_indexing = vk::TRUE;
                                (*v12_features).descriptor_binding_sampled_image_update_after_bind = vk::TRUE;
                                (*v12_features).descriptor_binding_partially_bound = vk::TRUE;
                                (*v12_features).runtime_descriptor_array = vk::TRUE;

                                break;
                            }
                            curr = (*curr).p_next as *mut vk::BaseInStructure;
                        }
                    }

                    let existing_extensions = unsafe {
                        let count = create_info.enabled_extension_count as usize;
                        if count > 0 && !create_info.pp_enabled_extension_names.is_null() {
                            slice::from_raw_parts(create_info.pp_enabled_extension_names, count)
                        } else {
                            &[]
                        }
                    };
                    let extensions = [
                        KHR_EXTERNAL_MEMORY_NAME.as_ptr() as *const c_char,
                        ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_NAME.as_ptr() as *const c_char,
                    ];

                    let combined: Vec<*const c_char> = existing_extensions
                        .iter()
                        .copied()
                        .chain(extensions.iter().copied())
                        .collect();

                    // combine them somehow?
                    let create_info = create_info.enabled_extension_names(&*combined);

                    let device = xr_instance
                        .create_vulkan_device(
                            system,
                            get_instance_proc_addr,
                            physical_device.handle.as_raw() as _,
                            &create_info as *const _ as *const _,
                        )
                        .map_err(|err| {
                            error!("OpenXR unable to create Vulkan device: {err}");

                            vk::Result::ERROR_INITIALIZATION_FAILED
                        })?
                        .map_err(vk::Result::from_raw)?;
                    let device = vk::Device::from_raw(device as _);

                    Ok(ash::Device::load(vk_instance.fp_v1_0(), device))
                })
                .map_err(|err| {
                    error!("Vulkan device: {err}");

                    InstanceCreateError::VulkanUnsupported
                })?;
            let device = Device::try_from_ash(ash_device, physical_device).map_err(|err| {
                error!("Vulkan device: {err}");

                InstanceCreateError::VulkanUnsupported
            })?;
            let event_buf = xr::EventDataBuffer::new();

            Ok(Self {
                device,
                event_buf,
                instance: xr_instance,
                system,
            })
        }

    }

    #[inline]
    pub fn create_session(
        this: &Self,
        queue_family_index: u32,
        queue_index: u32,
    ) -> xr::Result<(
        xr::Session<xr::Vulkan>,
        xr::FrameWaiter,
        xr::FrameStream<xr::Vulkan>,
    )> {
        unsafe {
            this.instance.create_session::<xr::Vulkan>(
                this.system,
                &xr::vulkan::SessionCreateInfo {
                    instance: this.device.physical.instance.handle().as_raw() as _,
                    physical_device: this.device.physical.handle.as_raw() as _,
                    device: this.device.handle().as_raw() as _,
                    queue_family_index,
                    queue_index,
                },
            )
        }
    }

    pub fn device(this: &Self) -> &Device {
        &this.device
    }

    #[inline]
    pub fn enumerate_view_configuration_views(
        this: &Self,
        ty: xr::ViewConfigurationType,
    ) -> xr::Result<Vec<xr::ViewConfigurationView>> {
        this.enumerate_view_configuration_views(this.system, ty)
    }

    #[inline]
    pub fn poll_event(this: &mut Self) -> xr::Result<Option<xr::Event<'_>>> {
        this.instance.poll_event(&mut this.event_buf)
    }

    #[allow(dead_code)]
    pub fn system(this: &Self) -> xr::SystemId {
        this.system
    }
}

impl Debug for XrInstance {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.write_str("Instance")
    }
}

impl Deref for XrInstance {
    type Target = xr::Instance;

    fn deref(&self) -> &Self::Target {
        &self.instance
    }
}

#[derive(Debug)]
pub enum InstanceCreateError {
    OpenXRUnsupported,
    VulkanUnsupported,
}
