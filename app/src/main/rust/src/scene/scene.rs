use vk_graph::driver::buffer::Buffer;
use {
    crate::{
        render::renderer::{self, DrawPayload},
        input::Hand,
        scene::{
            gltf_model::{GltfAsset, GltfInstance, NodeIndex, Vertex}
        },
    },
    glam::Mat4,
    ndk::asset::AssetManager,
    std::sync::{
        Arc,
        RwLock
    },
    vk_graph::{
        Graph,
        driver::{
            ash::vk::{self, CullModeFlags, PrimitiveTopology},
            device::Device,
            graphics::{BlendInfo, GraphicsPipeline, GraphicsPipelineInfo},
            shader::{SamplerInfoBuilder, Shader},
            image::{Image}
        },
    }
};

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
pub enum SkinType {
    Wide,
    Slim
}

pub struct Skin {
    pub texture: Arc<Image>,
    pub skin_type: SkinType
}

pub struct Assets {
    pub skin: RwLock<Option<Skin>>,

    pub scene_asset: Arc<GltfAsset>,
    pub scene_instance: GltfInstance,

    pub animated_asset: Arc<GltfAsset>,
    pub animated_instance: GltfInstance,

    // these _asset fields don't really need to be here because GltfInstance holds a reference. However, I don't really want to delete them in case we want to add animations to them (which would require an accessible reference to the asset)
    left_controller_scene_asset: Arc<GltfAsset>,
    pub left_controller_scene_instance: GltfInstance,
    right_controller_scene_asset: Arc<GltfAsset>,
    pub right_controller_scene_instance: GltfInstance,
    slim_left_controller_scene_asset: Arc<GltfAsset>,
    pub slim_left_controller_scene_instance: GltfInstance,
    slim_right_controller_scene_asset: Arc<GltfAsset>,
    pub slim_right_controller_scene_instance: GltfInstance,
    
    ray_scene_asset: Arc<GltfAsset>,
    ray_scene_instance: GltfInstance,
    pointer_scene_asset: Arc<GltfAsset>,
    pointer_scene_instance: GltfInstance,
}

pub struct Scene {
    pub assets: Assets,

    pub spawn_point: Option<Mat4>,
    pub surface_index: Option<NodeIndex>
}

impl Scene {
    pub fn load(device: &Device, camera_buffers: &Vec<Arc<Buffer>>, asset_manager: &AssetManager) -> Self {
        let gltf_unlit_shaders = {
            let mut asset = asset_manager.open(c"shaders/gltf_unlit.spv").expect("Failed to load 'gltf_unlit' shader");
            let spv_bytes = asset.buffer().unwrap();

            [
                Shader::builder()
                    .entry_name("vertex_main")
                    .stage(vk::ShaderStageFlags::VERTEX)
                    .image_sampler((0, 3), SamplerInfoBuilder::default()
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
                                stride: size_of::<Vertex>() as u32,
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
                            vk::VertexInputAttributeDescription {
                                location: 5,
                                binding: 0,
                                format: vk::Format::R32G32B32A32_UINT,
                                offset: 56,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 6,
                                binding: 0,
                                format: vk::Format::R32G32B32A32_SFLOAT,
                                offset: 72,
                            },
                        ])
                    .spirv(spv_bytes),
                Shader::builder()
                    .entry_name("fragment_main")
                    .stage(vk::ShaderStageFlags::FRAGMENT)
                    .spirv(spv_bytes),
            ]
        };
        let gltf_unlit_pipeline = Arc::new(
            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .cull_mode(CullModeFlags::BACK)
                .samples(renderer::MSAA_COUNT), gltf_unlit_shaders.clone()).expect("Failed to create gltf_unlit pipeline")
        );
        let gltf_unlit_no_cull_pipeline = Arc::new(
            GraphicsPipeline::create(device, gltf_unlit_pipeline.info().into_builder()
                .cull_mode(CullModeFlags::NONE), gltf_unlit_shaders).expect("Failed to create gltf_unlit_no_cull pipeline")
        );

        let gltf_unlit_translucent_shaders = {
            let mut asset = asset_manager.open(c"shaders/gltf_unlit_translucent.spv").expect("Failed to load 'gltf_unlit_translucent' shader");
            let spv_bytes = asset.buffer().unwrap();

            [
                Shader::builder()
                    .entry_name("vertex_main")
                    .stage(vk::ShaderStageFlags::VERTEX)
                    .image_sampler((0, 3), SamplerInfoBuilder::default()
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
                                stride: size_of::<Vertex>() as u32,
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
                            vk::VertexInputAttributeDescription {
                                location: 5,
                                binding: 0,
                                format: vk::Format::R32G32B32A32_UINT,
                                offset: 56,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 6,
                                binding: 0,
                                format: vk::Format::R32G32B32A32_SFLOAT,
                                offset: 72,
                            },
                        ])
                    .spirv(spv_bytes),
                Shader::builder()
                    .entry_name("fragment_main")
                    .stage(vk::ShaderStageFlags::FRAGMENT)
                    .spirv(spv_bytes),
            ]
        };
        let gltf_unlit_translucent_pipeline = Arc::new(
            GraphicsPipeline::create(device, GraphicsPipelineInfo::builder()
                .topology(PrimitiveTopology::TRIANGLE_LIST)
                .blend(BlendInfo::ALPHA)
                .samples(renderer::MSAA_COUNT)
                .cull_mode(CullModeFlags::BACK), gltf_unlit_translucent_shaders.clone()).expect("Failed to create gltf_unlit_translucent pipeline")
        );
        let gltf_unlit_translucent_no_cull_pipeline = Arc::new(
            GraphicsPipeline::create(device, gltf_unlit_translucent_pipeline.info().into_builder()
                .cull_mode(CullModeFlags::NONE), gltf_unlit_translucent_shaders).expect("Failed to create gltf_unlit_translucent_no_cull")
        );

        let scene_asset = {
            let asset = asset_manager.open(c"meshes/scene.glb").expect("Failed to load 'scene.glb'");
            Arc::new(GltfAsset::new("Test".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let scene_instance = GltfInstance::new(scene_asset.clone(), device, camera_buffers);

        let animated_asset = {
            let asset = asset_manager.open(c"meshes/animated.glb").expect("Failed to load 'animated.glb'");
            Arc::new(GltfAsset::new("Animated".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone(), gltf_unlit_translucent_no_cull_pipeline.clone()))
        };
        let animated_instance = GltfInstance::new(animated_asset.clone(), device, camera_buffers);

        let left_controller_scene_asset = {
            let asset = asset_manager.open(c"meshes/left_controller.glb").expect("Failed to load 'left_controller.glb'");
            Arc::new(GltfAsset::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let left_controller_scene_instance = GltfInstance::new(left_controller_scene_asset.clone(), device, camera_buffers);
        let right_controller_scene_asset = {
            let asset = asset_manager.open(c"meshes/right_controller.glb").expect("Failed to load 'right_controller.glb'");
            Arc::new(GltfAsset::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let right_controller_scene_instance = GltfInstance::new(right_controller_scene_asset.clone(), device, camera_buffers);

        let slim_left_controller_scene_asset = {
            let asset = asset_manager.open(c"meshes/slim_left_controller.glb").expect("Failed to load 'slim_left_controller.glb'");
            Arc::new(GltfAsset::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let slim_left_controller_scene_instance = GltfInstance::new(slim_left_controller_scene_asset.clone(), device, camera_buffers);
        let slim_right_controller_scene_asset = {
            let asset = asset_manager.open(c"meshes/slim_right_controller.glb").expect("Failed to load 'slim_right_controller.glb'");
            Arc::new(GltfAsset::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let slim_right_controller_scene_instance = GltfInstance::new(slim_right_controller_scene_asset.clone(), device, camera_buffers);

        let ray_scene_asset = {
            let asset = asset_manager.open(c"meshes/ray.glb").expect("Failed to load 'ray.glb'");
            Arc::new(GltfAsset::new("Ray".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone(), gltf_unlit_translucent_no_cull_pipeline.clone()))
        };
        let ray_scene_instance = GltfInstance::new(ray_scene_asset.clone(), device, camera_buffers);

        let pointer_scene_asset = {
            let asset = asset_manager.open(c"meshes/pointer.glb").expect("Failed to load 'pointer.glb'");
            Arc::new(GltfAsset::new("Pointer".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone(), gltf_unlit_translucent_no_cull_pipeline.clone()))
        };
        let pointer_scene_instance = GltfInstance::new(pointer_scene_asset.clone(), device, camera_buffers);

        let spawn_matrix = scene_instance.find_spawnpoint_transform();
        if let Some(spawn_matrix) = &spawn_matrix {
            let (_, _, translation) = spawn_matrix.to_scale_rotation_translation();
            log::info!("Spawn point found at position coordinates: {:?}", translation);
        } else {
            log::warn!("No special spawn points defined within the scene's asset metadata maps.");
        }

        let surface_index = scene_instance.find_surface_index();
        if let Some(surface_index) = &surface_index {
            log::info!("Surface index found at index: {:?}", surface_index);
        } else {
            log::warn!("No special surface index defined within the scene's asset metadata maps.");
        }

        let assets = Assets {
            skin: RwLock::new(None),
            scene_asset,
            scene_instance,
            animated_asset,
            animated_instance,
            left_controller_scene_asset,
            left_controller_scene_instance,
            right_controller_scene_asset,
            right_controller_scene_instance,
            slim_left_controller_scene_asset,
            slim_left_controller_scene_instance,
            slim_right_controller_scene_asset,
            slim_right_controller_scene_instance,
            ray_scene_asset,
            ray_scene_instance,
            pointer_scene_asset,
            pointer_scene_instance,
        };
        Scene {
            assets,
            spawn_point: spawn_matrix,
            surface_index,
        }
    }

    pub fn record(&self, graph: &mut Graph, draw_payload: &DrawPayload) {
        self.assets.scene_instance.record(graph, draw_payload);
    }

    pub fn record_controller(&self, graph: &mut Graph, draw_payload: &DrawPayload, hand: Hand, controller_matrix: &Mat4, ray_transform: Option<Mat4>) {
        let skin = self.assets.skin.read().unwrap();
        let scene = match skin.as_ref().map_or(SkinType::Wide, |skin| { skin.skin_type }) {
            SkinType::Wide => match hand {
                Hand::Left => &self.assets.left_controller_scene_instance,
                Hand::Right => &self.assets.right_controller_scene_instance
            },
            SkinType::Slim => match hand {
                Hand::Left => &self.assets.slim_left_controller_scene_instance,
                Hand::Right => &self.assets.slim_right_controller_scene_instance
            }
        };


        scene.record_with_transform(graph, draw_payload, controller_matrix);
        if let Some(ray_transform) = ray_transform {
            self.assets.ray_scene_instance.record_with_transform(graph, draw_payload, &ray_transform);
        }
    }

    pub fn record_pointer(&self, graph: &mut Graph, draw_payload: &DrawPayload, pointer_matrix: &Mat4) {
        self.assets.pointer_scene_instance.record_with_transform(graph, draw_payload, pointer_matrix);
    }
}