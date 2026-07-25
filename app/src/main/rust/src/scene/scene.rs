use {
    crate::{
        render::renderer::{self, DrawPayload},
        input::Hand,
        scene::{
            gltf_model::{GltfScene, NodeIndex, Vertex}
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
    pub gltf_scene: Arc<GltfScene>,
    pub skin: RwLock<Option<Skin>>,
    left_controller_scene: Arc<GltfScene>,
    right_controller_scene: Arc<GltfScene>,
    slim_left_controller_scene: Arc<GltfScene>,
    slim_right_controller_scene: Arc<GltfScene>,
    
    ray_scene: Arc<GltfScene>,
    pointer_scene: Arc<GltfScene>,
}

pub struct Scene {
    pub assets: Assets,

    pub spawn_point: Option<Mat4>,
    pub surface_index: Option<NodeIndex>
}

impl Scene {
    pub fn load(device: &Device, asset_manager: &AssetManager) -> Self {
        let gltf_unlit_shaders = {
            let mut asset = asset_manager.open(c"shaders/gltf_unlit.spv").expect("Failed to load 'gltf_unlit' shader");
            let spv_bytes = asset.buffer().unwrap();

            [
                Shader::builder()
                    .entry_name("vertex_main")
                    .stage(vk::ShaderStageFlags::VERTEX)
                    .image_sampler((0, 2), SamplerInfoBuilder::default()
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
                    .image_sampler((0, 2), SamplerInfoBuilder::default()
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

        let simple_gltf_scene = {
            let asset = asset_manager.open(c"meshes/scene.glb").expect("Failed to load 'scene.glb'");
            Arc::new(GltfScene::new("Test".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };

        let left_controller_scene = {
            let asset = asset_manager.open(c"meshes/left_controller.glb").expect("Failed to load 'left_controller.glb'");
            Arc::new(GltfScene::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let right_controller_scene = {
            let asset = asset_manager.open(c"meshes/right_controller.glb").expect("Failed to load 'right_controller.glb'");
            Arc::new(GltfScene::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };

        let slim_left_controller_scene = {
            let asset = asset_manager.open(c"meshes/slim_left_controller.glb").expect("Failed to load 'slim_left_controller.glb'");
            Arc::new(GltfScene::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };
        let slim_right_controller_scene = {
            let asset = asset_manager.open(c"meshes/slim_right_controller.glb").expect("Failed to load 'slim_right_controller.glb'");
            Arc::new(GltfScene::new("Controller".to_string(), asset, device, gltf_unlit_pipeline.clone(), gltf_unlit_no_cull_pipeline.clone()))
        };

        let ray_scene = {
            let asset = asset_manager.open(c"meshes/ray.glb").expect("Failed to load 'ray.glb'");
            Arc::new(GltfScene::new("Ray".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone(), gltf_unlit_translucent_no_cull_pipeline.clone()))
        };

        let pointer_scene = {
            let asset = asset_manager.open(c"meshes/pointer.glb").expect("Failed to load 'pointer.glb'");
            Arc::new(GltfScene::new("Pointer".to_string(), asset, device, gltf_unlit_translucent_pipeline.clone(), gltf_unlit_translucent_no_cull_pipeline.clone()))
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
            skin: RwLock::new(None),
            left_controller_scene,
            right_controller_scene,
            slim_left_controller_scene,
            slim_right_controller_scene,
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
        self.assets.gltf_scene.record(graph, draw_payload);
    }

    pub fn record_controller(&self, graph: &mut Graph, draw_payload: &DrawPayload, hand: Hand, controller_matrix: &Mat4, ray_transform: Option<Mat4>) {
        let skin = self.assets.skin.read().unwrap();
        let scene = match skin.as_ref().map_or(SkinType::Wide, |skin| { skin.skin_type }) {
            SkinType::Wide => match hand {
                Hand::Left => self.assets.left_controller_scene.clone(),
                Hand::Right => self.assets.right_controller_scene.clone()
            },
            SkinType::Slim => match hand {
                Hand::Left => self.assets.slim_left_controller_scene.clone(),
                Hand::Right => self.assets.slim_right_controller_scene.clone()
            }
        };

        if let Some(ref skin) = *skin {
            scene.record_with_transform_override_texture(graph, draw_payload, controller_matrix, skin.texture.clone());
        } else {
            scene.record_with_transform(graph, draw_payload, controller_matrix);
        }
        if let Some(ray_transform) = ray_transform {
            self.assets.ray_scene.record_with_transform(graph, draw_payload, &ray_transform);
        }
    }

    pub fn record_pointer(&self, graph: &mut Graph, draw_payload: &DrawPayload, pointer_matrix: &Mat4) {
        self.assets.pointer_scene.record_with_transform(graph, draw_payload, pointer_matrix);
    }
}