use crate::render::renderer::DrawPayload;
use crate::scene::gltf_model;
use glam::{Mat4, Vec3};
use ndk::asset::AssetManager;
use std::sync::Arc;
use vk_graph::driver::ash::vk;
use vk_graph::driver::ash::vk::{CullModeFlags, PrimitiveTopology};
use vk_graph::driver::device::Device;
use vk_graph::driver::graphics::{BlendInfo, GraphicsPipeline, GraphicsPipelineInfo};
use vk_graph::driver::shader::{SamplerInfoBuilder, Shader};
use vk_graph::Graph;
use crate::render::renderer;
use crate::scene::gltf_model::NodeIndex;

pub struct Assets {
    pub gltf_scene: Arc<gltf_model::GltfScene>,
    controller_scene: Arc<gltf_model::GltfScene>,
    ray_scene: Arc<gltf_model::GltfScene>,
    pointer_scene: Arc<gltf_model::GltfScene>,
}

pub struct Scene {
    pub assets: Assets,

    pub spawn_point: Option<Mat4>,
    pub surface_index: Option<NodeIndex>
}

impl Scene {
    pub fn load(device: &Device, asset_manager: &AssetManager) -> Self {
        let gltf_pipeline = Arc::new({
            let mut asset = asset_manager.open(c"shaders/gltf.spv").expect("Failed to load 'gltf' shader");
            let spv_bytes = asset.buffer().unwrap();

            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .samples(renderer::MSAA_COUNT)
                .cull_mode(CullModeFlags::NONE), [
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
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 0,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 1,
                                     binding: 0,
                                     format: vk::Format::R32G32_SFLOAT,
                                     offset: 12,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 2,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 20,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 3,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 32,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 4,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 44,
                                 },
                             ])
                         .spirv(spv_bytes),
                     Shader::builder()
                         .entry_name("fragment_main")
                         .stage(vk::ShaderStageFlags::FRAGMENT)
                         .spirv(spv_bytes),
                ]).expect("Failed to create gltf pipeline")
        });

        let gltf_unlit_pipeline = Arc::new({
            let mut asset = asset_manager.open(c"shaders/gltf_unlit.spv").expect("Failed to load 'gltf_unlit' shader");
            let spv_bytes = asset.buffer().unwrap();

            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .samples(renderer::MSAA_COUNT)
                .cull_mode(CullModeFlags::NONE), [
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
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 0,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 1,
                                     binding: 0,
                                     format: vk::Format::R32G32_SFLOAT,
                                     offset: 12,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 2,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 20,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 3,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 32,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 4,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 44,
                                 },
                             ])
                         .spirv(spv_bytes),
                     Shader::builder()
                         .entry_name("fragment_main")
                         .stage(vk::ShaderStageFlags::FRAGMENT)
                         .spirv(spv_bytes),
                 ]).expect("Failed to create gltf_unlit pipeline")
        });

        let gltf_unlit_translucent_pipeline = Arc::new({
            let mut asset = asset_manager.open(c"shaders/gltf_unlit_translucent.spv").expect("Failed to load 'gltf_unlit_translucent' shader");
            let spv_bytes = asset.buffer().unwrap();

            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .samples(renderer::MSAA_COUNT)
                .blend(BlendInfo::ALPHA)
                .cull_mode(CullModeFlags::NONE), [
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
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 0,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 1,
                                     binding: 0,
                                     format: vk::Format::R32G32_SFLOAT,
                                     offset: 12,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 2,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 20,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 3,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 32,
                                 },
                                 vk::VertexInputAttributeDescription {
                                     location: 4,
                                     binding: 0,
                                     format: vk::Format::R32G32B32_SFLOAT,
                                     offset: 44,
                                 },
                             ])
                         .spirv(spv_bytes),
                     Shader::builder()
                         .entry_name("fragment_main")
                         .stage(vk::ShaderStageFlags::FRAGMENT)
                         .spirv(spv_bytes),
                ]).expect("Failed to create gltf_unlit pipeline")
        });


        let simple_gltf_scene = {
            let asset = asset_manager.open(c"meshes/scene.glb").expect("Failed to load 'scene.glb'");
            Arc::new(gltf_model::GltfScene::new("Test".to_string(), asset, device, gltf_unlit_pipeline.clone()))
        };

        let controller_scene = {
            let asset = asset_manager.open(c"meshes/controller.glb").expect("Failed to load 'controller.glb'");
            Arc::new(gltf_model::GltfScene::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone()))
        };

        let ray_scene = {
            let asset = asset_manager.open(c"meshes/ray.glb").expect("Failed to load 'ray.glb'");
            Arc::new(gltf_model::GltfScene::new("Ray".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone()))
        };

        let pointer_scene = {
            let asset = asset_manager.open(c"meshes/pointer.glb").expect("Failed to load 'pointer.glb'");
            Arc::new(gltf_model::GltfScene::new("Pointer".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone()))
        };

        let spawn_matrix = simple_gltf_scene.find_spawnpoint_transform();
        if let Some(spawn_matrix) = &spawn_matrix {
            let (_, _, translation) = spawn_matrix.to_scale_rotation_translation();
            log::info!("Spawn point found at position coordinates: {:?}", translation);
        } else {
            log::warn!("No special spawn points defined within the scene's asset metadata maps.");
        }

        let surface_index = simple_gltf_scene.find_surface_index();
        if let Some(surface_index) = &surface_index {
            log::info!("Surface index found at index: {:?}", surface_index);
        } else {
            log::warn!("No special surface index defined within the scene's asset metadata maps.");
        }

        let assets = Assets {
            gltf_scene: simple_gltf_scene,
            controller_scene,
            ray_scene,
            pointer_scene,
        };
        Scene {
            assets,
            spawn_point: spawn_matrix,
            surface_index,
        }
    }

    pub fn record(&self, graph: &mut Graph, draw_payload: &DrawPayload) {
        self.assets.gltf_scene.record(graph, draw_payload.camera_ubo, draw_payload.swapchain_image, draw_payload.depth_image);
    }

    pub fn record_controller(&self, graph: &mut Graph, draw_payload: &DrawPayload, controller_matrix: &Mat4, ray_transform: Option<Mat4>) {
        self.assets.controller_scene.record_with_transform(graph, draw_payload.camera_ubo, draw_payload.swapchain_image, draw_payload.depth_image, controller_matrix);
        if let Some(ray_transform) = ray_transform {
            self.assets.ray_scene.record_with_transform(graph, draw_payload.camera_ubo, draw_payload.swapchain_image, draw_payload.depth_image, &ray_transform);
        }
    }

    pub fn record_pointer(&self, graph: &mut Graph, draw_payload: &DrawPayload, pointer_matrix: &Mat4) {
        self.assets.pointer_scene.record_with_transform(graph, draw_payload.camera_ubo, draw_payload.swapchain_image, draw_payload.depth_image, pointer_matrix);
    }
}