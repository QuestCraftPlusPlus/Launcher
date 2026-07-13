use {
    crate::{
        render::renderer,
        scene::gltf_model::{self, GltfPrimitive}
    },
    bytemuck::{Pod, Zeroable, bytes_of},
    glam::{Mat4, Vec2, Vec3},
    jni::{Env, objects::JObject, refs::Global},
    ndk::asset::AssetManager,
    ndk_sys::{AHardwareBuffer, AHardwareBuffer_acquire, AHardwareBuffer_release, AImageReader, AImageReader_acquireLatestImage, AImageReader_delete, AImageReader_getWindow, AImageReader_newWithUsage, AImage_delete, AImage_getHardwareBuffer, ANativeWindow_toSurface, media_status_t},
    std::sync::Arc,
    vk_graph::{
        driver::{
            ash::vk::{self, Format, ImageViewType, IndexType, PrimitiveTopology},
            device::Device,
            graphics::{DepthStencilInfo, GraphicsPipeline, GraphicsPipelineInfo},
            image::{Image, ImageInfo, ImageViewInfoBuilder, SampleCount},
            shader::{SamplerInfoBuilder, Shader},
            sync::AccessType
        },
        Graph, LoadOp, StoreOp
    }
};

pub struct SurfaceTexture {
    pub image: Arc<Image>,
    pub memory: vk::DeviceMemory,
    pub hw_buffer: *mut AHardwareBuffer,
    device: Arc<Device>,
}

impl Drop for SurfaceTexture {
    fn drop(&mut self) {
        unsafe {
            self.device.destroy_image(self.image.handle, None);
            self.device.free_memory(self.memory, None);
            AHardwareBuffer_release(self.hw_buffer);
        }
    }
}

pub struct Surface {
    pub extent: vk::Extent3D,
    pub image_reader: *mut AImageReader,
    pub java_surface: Global<JObject<'static>>,
}

fn find_memory_type_index(device: &Device, type_bits: u32, properties: vk::MemoryPropertyFlags) -> u32 {
    let mem_properties = unsafe { device.physical.instance.get_physical_device_memory_properties(device.physical.handle) };
    for i in 0..mem_properties.memory_type_count {
        if (type_bits & (1 << i)) != 0 && mem_properties.memory_types[i as usize].property_flags.contains(properties) {
            return i;
        }
    }
    panic!("Failed to find suitable memory type for AHardwareBuffer");
}

impl Surface {
    pub fn new(env: &mut Env<'_>, width: u32, height: u32) -> Self {
        let image_reader = unsafe {
            let mut image_reader = std::ptr::null_mut();
            let status = AImageReader_newWithUsage(
                width as i32,
                height as i32,
                0x00000001, // this is format RBGA_8888, see https://developer.android.com/reference/android/graphics/ImageFormat for constant values
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
        let java_surface = env.new_global_ref(&java_surface).expect("Failed to create global reference to Java Surface");

        Self {
            extent: vk::Extent3D {
                width: width as _, height: height as _, depth: 1
            },
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
            AHardwareBuffer_acquire(hardware_buffer);
            AImage_delete(image);

            Ok(hardware_buffer)
        }
    }

    pub fn update_texture(&mut self, device: Arc<Device>, hw_buffer_loader: &vk_graph::driver::ash::android::external_memory_android_hardware_buffer::Device) -> Option<SurfaceTexture> {
        let hw_buffer = match self.acquire_latest_buffer() {
            Ok(buf) => buf,
            Err(_) => {
                return None;
            }
        };

        unsafe {
            let mut format_properties = vk::AndroidHardwareBufferFormatPropertiesANDROID::default();
            let mut properties = vk::AndroidHardwareBufferPropertiesANDROID::default();
            properties.p_next = &mut format_properties as *mut _ as *mut std::ffi::c_void;
            hw_buffer_loader.get_android_hardware_buffer_properties(hw_buffer as *const _, &mut properties).expect("Failed to get Android hardware buffer properties");

            let target_format = format_properties.format;

            let mut external_image_info = vk::ExternalMemoryImageCreateInfo::default()
                .handle_types(vk::ExternalMemoryHandleTypeFlags::ANDROID_HARDWARE_BUFFER_ANDROID);
            let mut external_format_info = vk::ExternalFormatANDROID::default();
            if target_format == vk::Format::UNDEFINED {
                if format_properties.external_format == 0 {
                    AHardwareBuffer_release(hw_buffer);
                    return None;
                }
                external_format_info = external_format_info.external_format(format_properties.external_format);
                external_image_info.p_next = &mut external_format_info as *mut _ as *mut std::ffi::c_void;
            }

            let temp_image_info = vk::ImageCreateInfo::default()
                .image_type(vk::ImageType::TYPE_2D)
                .format(target_format)
                .extent(self.extent)
                .mip_levels(1)
                .array_layers(1)
                .samples(SampleCount::Type1.into())
                .tiling(vk::ImageTiling::OPTIMAL)
                .usage(vk::ImageUsageFlags::SAMPLED)
                .sharing_mode(vk::SharingMode::EXCLUSIVE)
                .initial_layout(vk::ImageLayout::UNDEFINED)
                .push_next(&mut external_image_info);

            let temp_image = match device.create_image(&temp_image_info, None) {
                Ok(img) => img,
                Err(_) => {
                    AHardwareBuffer_release(hw_buffer);
                    return None;
                }
            };

            let mut dedicated_requirements = vk::MemoryDedicatedRequirements::default();
            let mut memory_requirements2 = vk::MemoryRequirements2::default().push_next(&mut dedicated_requirements);
            let image_requirements_info = vk::ImageMemoryRequirementsInfo2::default().image(temp_image);
            device.get_image_memory_requirements2(&image_requirements_info, &mut memory_requirements2);

            let supported_memory_types = properties.memory_type_bits & memory_requirements2.memory_requirements.memory_type_bits;

            let mut import_buffer_info = vk::ImportAndroidHardwareBufferInfoANDROID::default().buffer(hw_buffer as *mut _);
            let mut dedicated_alloc_info = vk::MemoryDedicatedAllocateInfo::default().image(temp_image);

            let memory_allocate_info = vk::MemoryAllocateInfo::default()
                .allocation_size(memory_requirements2.memory_requirements.size)
                .memory_type_index(find_memory_type_index(
                    &*device, supported_memory_types, vk::MemoryPropertyFlags::DEVICE_LOCAL
                ))
                .push_next(&mut import_buffer_info)
                .push_next(&mut dedicated_alloc_info);

            let temp_memory = match device.allocate_memory(&memory_allocate_info, None) {
                Ok(mem) => mem,
                Err(_) => {
                    device.destroy_image(temp_image, None);
                    AHardwareBuffer_release(hw_buffer);
                    return None;
                }
            };

            device.bind_image_memory(temp_image, temp_memory, 0).unwrap();

            let wrapped_temp_image = Image::from_raw(&device, temp_image, ImageInfo::builder()
                .alloc_dedicated(dedicated_requirements.prefers_dedicated_allocation == vk::TRUE || dedicated_requirements.requires_dedicated_allocation == vk::TRUE)
                .array_layer_count(temp_image_info.array_layers)
                .mip_level_count(temp_image_info.mip_levels)
                .width(self.extent.width).height(self.extent.height).depth(self.extent.depth)
                .flags(temp_image_info.flags)
                .format(temp_image_info.format)
                .host_readable(false)
                .host_writable(false)
                .sample_count(SampleCount::Type1)
                .sharing_mode(vk::SharingMode::EXCLUSIVE)
                .tiling(vk::ImageTiling::OPTIMAL)
                .image_type(vk::ImageType::TYPE_2D)
                .usage(vk::ImageUsageFlags::SAMPLED)
                .build());

            Some(SurfaceTexture {
                image: Arc::new(wrapped_temp_image),
                memory: temp_memory,
                hw_buffer,
                device: device.clone(),
            })
        }
    }
}

impl Drop for Surface {
    fn drop(&mut self) {
        unsafe {
            if !self.image_reader.is_null() {
                AImageReader_delete(self.image_reader);
            }
        }
    }
}

pub struct SurfaceManager<'a> {
    pub pipeline: Arc<GraphicsPipeline>,
    pub primitive: &'a GltfPrimitive,
}

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
pub struct SurfacePushConstants {
    model_transform: Mat4,
}

pub struct RaycastHit {
    pub uv: Vec2,
    pub world_position: Vec3,
    pub world_normal: Vec3,
}

impl<'a> SurfaceManager<'a> {
    pub fn new(asset_manager: &AssetManager, device: &Device, primitive: &'a GltfPrimitive) -> Self {
        let pipeline = Arc::new({
            let mut asset = asset_manager.open(c"shaders/gltf_surface.spv").expect("Failed to load 'gltf_surface' shader");
            let spv_bytes = asset.buffer().unwrap();

            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .samples(renderer::MSAA_COUNT)
                .cull_mode(vk::CullModeFlags::NONE), [
                Shader::builder()
                    .entry_name("vertex_main")
                    .stage(vk::ShaderStageFlags::VERTEX)
                    .image_sampler((0, 1), SamplerInfoBuilder::default()
                        .min_filter(vk::Filter::NEAREST)
                        .mag_filter(vk::Filter::NEAREST)
                        .address_mode_u(vk::SamplerAddressMode::REPEAT)
                        .address_mode_v(vk::SamplerAddressMode::REPEAT)
                        .address_mode_w(vk::SamplerAddressMode::REPEAT)
                        .build())
                    .vertex_input(
                        [
                            vk::VertexInputBindingDescription {
                                binding: 0,
                                stride: size_of::<gltf_model::Vertex>() as u32,
                                input_rate: vk::VertexInputRate::VERTEX,
                            }
                        ],
                        [
                            vk::VertexInputAttributeDescription {
                                location: 0,
                                binding: 0,
                                format: Format::R32G32B32_SFLOAT,
                                offset: 0,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 1,
                                binding: 0,
                                format: Format::R32G32_SFLOAT,
                                offset: 12,
                            },
                        ])
                    .spirv(spv_bytes),
                Shader::builder()
                    .entry_name("fragment_main")
                    .stage(vk::ShaderStageFlags::FRAGMENT)
                    .spirv(spv_bytes),
            ]).expect("Failed to create graphics pipeline")
        });

        Self {
            primitive,
            pipeline,
        }
    }

    // this uses the Moller-Trumbore intersection algorithm
    pub fn raycast_uv(&self, controller_world_matrix: Mat4, surface_world_matrix: Mat4, ray_direction: Vec3) -> Option<RaycastHit> {
        let vertices = self.primitive.cpu_vertex_buffer.as_ref()?;
        let indices = self.primitive.cpu_index_buffer.as_ref()?;

        let ray_origin_world = controller_world_matrix.transform_point3(Vec3::ZERO);
        let ray_direction_world = controller_world_matrix.transform_vector3(ray_direction).normalize();

        let world_to_mesh_local = surface_world_matrix.inverse();
        let ray_origin = world_to_mesh_local.transform_point3(ray_origin_world);
        let ray_direction = world_to_mesh_local.transform_vector3(ray_direction_world).normalize();

        let mut closest_t = f32::INFINITY;

        let mut closest_local_pos = Vec3::ZERO;
        let mut closest_local_normal = Vec3::ZERO;
        let mut closest_uv = Vec2::ZERO;
        let mut hit_found = false;

        for chunk in indices.chunks_exact(3) {
            let v0 = &vertices[chunk[0] as usize];
            let v1 = &vertices[chunk[1] as usize];
            let v2 = &vertices[chunk[2] as usize];

            let p0 = Vec3::from_array(v0.position);
            let p1 = Vec3::from_array(v1.position);
            let p2 = Vec3::from_array(v2.position);

            let edge1 = p1 - p0;
            let edge2 = p2 - p0;
            let h = ray_direction.cross(edge2);
            let a = edge1.dot(h);

            if a.abs() < f32::EPSILON { continue; }

            let f = 1.0 / a;
            let s = ray_origin - p0;
            let u = f * s.dot(h);

            if u < 0.0 || u > 1.0 { continue; }

            let q = s.cross(edge1);
            let v = f * ray_direction.dot(q);

            if v < 0.0 || u + v > 1.0 { continue; }

            if u + v > 1.0 { continue; }

            let t = f * edge2.dot(q);

            if t > f32::EPSILON && t < closest_t {
                closest_t = t;
                hit_found = true;

                let w0 = 1.0 - u - v;
                let w1 = u;
                let w2 = v;

                let uv0 = Vec2::from_array(v0.uv);
                let uv1 = Vec2::from_array(v1.uv);
                let uv2 = Vec2::from_array(v2.uv);
                closest_uv = uv0 * w0 + uv1 * w1 + uv2 * w2;

                closest_local_pos = ray_origin + ray_direction * t;

                let n0 = Vec3::from_array(v0.normal);
                let n1 = Vec3::from_array(v1.normal);
                let n2 = Vec3::from_array(v2.normal);
                closest_local_normal = (n0 * w0 + n1 * w1 + n2 * w2).normalize();
            }
        }

        if hit_found {
            let world_position = surface_world_matrix.transform_point3(closest_local_pos);
            let normal_matrix = world_to_mesh_local.transpose();
            let world_normal = normal_matrix.transform_vector3(closest_local_normal).normalize();

            Some(RaycastHit {
                uv: closest_uv,
                world_position,
                world_normal,
            })
        } else {
            None
        }
    }

    pub fn record_with_transform(&self, graph: &mut Graph, surface_texture: Arc<Image>, camera_ubo: &vk_graph::node::BufferLeaseNode, swapchain_image: &vk_graph::node::ImageNode, depth_image: &vk_graph::node::ImageLeaseNode, transform: Mat4) {
        let push_consts = SurfacePushConstants { model_transform: transform };

        let srgb_texture_view = ImageViewInfoBuilder::default()
            .aspect_mask(vk::ImageAspectFlags::COLOR)
            .view_type(ImageViewType::TYPE_2D)
            .format(Format::R8G8B8A8_SRGB)
            .array_layer_count(1)
            .mip_level_count(1)
            .build();

        let image_node = graph.bind_resource(surface_texture);
        let index_node = graph.bind_resource(self.primitive.index_buffer.clone());
        let vertex_node = graph.bind_resource(self.primitive.vertex_buffer.clone());
        let index_count = self.primitive.index_count;

        let mut cmd_graph = graph.begin_cmd()
            .debug_name("Surface")
            .bind_pipeline(&*self.pipeline)
            .multiview(renderer::VIEW_MASK, 0)
            .shader_resource_access(0, *camera_ubo, AccessType::VertexShaderReadUniformBuffer)
            .color_attachment_image(0, *swapchain_image, LoadOp::Load, StoreOp::Store)
            .depth_stencil(DepthStencilInfo::DEPTH_WRITE_LESS)
            .depth_stencil_attachment_image(*depth_image, LoadOp::Load, StoreOp::Store);

        cmd_graph.set_shader_subresource_access((0,1), image_node, srgb_texture_view, AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
        cmd_graph
            .resource_access(index_node, AccessType::IndexBuffer)
            .resource_access(vertex_node, AccessType::VertexBuffer)
            .record_cmd(move |cmd| {
                cmd.bind_index_buffer(index_node, 0, IndexType::UINT32)
                    .bind_vertex_buffer(0, vertex_node, 0)
                    .push_constants(0, bytes_of(&push_consts))
                    .draw_indexed(index_count, 1, 0, 0, 0);
        }).end_cmd();
    }
}