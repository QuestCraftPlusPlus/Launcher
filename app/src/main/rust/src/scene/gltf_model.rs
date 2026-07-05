use std::collections::HashMap;
use std::sync::Arc;
use bytemuck::{bytes_of, Pod, Zeroable};
use glam::Mat4;
use gltf::image::Format;
use gltf::Node;
use log::{info};
use serde::Deserialize;
use vk_graph::driver::ash::vk::{BufferUsageFlags, IndexType};
use vk_graph::driver::buffer::Buffer;
use vk_graph::driver::device::Device;
use vk_graph::driver::graphics::{DepthStencilInfo, GraphicsPipeline};
use vk_graph::driver::sync::AccessType;
use vk_graph::{Graph, LoadOp, StoreOp};
use vk_graph::driver::ash::vk;
use vk_graph::driver::image::{Image, ImageInfoBuilder};
use vk_graph::pool::hash::HashPool;

pub struct GltfPrimitive {
    pub vertex_buffer: Arc<Buffer>,
    pub index_buffer: Arc<Buffer>,
    pub index_count: u32,
    pub material_index: Option<usize>,
}

pub struct GltfMesh {
    pub name: String,
    pub primitives: Vec<GltfPrimitive>
}

pub struct Material {
    pub base_color_texture_index: Option<usize>,
    pub metallic_roughness_texture_index: Option<usize>,
    pub normal_texture_index: Option<usize>,
    pub base_color_factor: [f32; 4],
}

pub type NodeIndex = usize;

pub struct GltfNode {
    pub local_transform: Mat4,
    pub global_transform: Mat4,
    pub mesh_index: Option<usize>,
    pub children: Vec<NodeIndex>,
}

pub struct GltfScene {
    pub identifier: String,
    pub pipeline: Arc<GraphicsPipeline>, // the simple GLTF workflow we're working with (i.e., only simple material gltf objects are supported) allows us to be stupid and use a single pipeline per scene (technically could be global but global state management aeugh)
    pub nodes: Vec<GltfNode>,
    pub roots: Vec<NodeIndex>,
    pub meshes: Vec<GltfMesh>,
    pub materials: Vec<Arc<Material>>,
    pub textures: Vec<Arc<Image>>,
    pub specials: Vec<NodeIndex>,

    pub node_extras: HashMap<NodeIndex, Extras>,
}

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
struct PushConstants {
    model_transform: Mat4,
    base_color_idx: i32,
    metallic_roughness_idx: i32,
    normal_map_idx: i32,
    base_color_factor: [f32; 4],
    _pad: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct Vertex {
    position: [f32; 3],
    uv: [f32; 2],
    normal: [f32; 3],
    tangent: [f32; 3],
    bitangent: [f32; 3],
}

#[derive(Debug, Deserialize)]
struct Extras {
    #[serde(rename = "is_ui_surface")]
    pub is_ui_surface: Option<i32>,
    #[serde(rename = "is_spawnpoint")]
    pub is_spawnpoint: Option<i32>,
}

impl GltfScene {
    pub fn record(&self, graph: &mut Graph,  camera_ubo: &vk_graph::node::BufferLeaseNode, swapchain_image: &vk_graph::node::ImageNode, depth_image: &vk_graph::node::ImageLeaseNode) {
        self.record_with_transform(graph, camera_ubo, swapchain_image, depth_image, Mat4::IDENTITY);
    }

    pub fn record_with_transform(&self, graph: &mut Graph, camera_ubo: &vk_graph::node::BufferLeaseNode, swapchain_image: &vk_graph::node::ImageNode, depth_image: &vk_graph::node::ImageLeaseNode, transform: Mat4) {
        let mut flat_draw_calls = Vec::new();

        for (node_idx, node) in self.nodes.iter().enumerate() {
            if let Some(mesh_idx) = node.mesh_index {
                if self.specials.contains(&node_idx) {
                    continue;
                }

                let mesh = &self.meshes[mesh_idx];

                for primitive in &mesh.primitives {
                    let (base_color_idx, metallic_roughness_idx, normal_map_idx, base_color_factor) =
                        if let Some(material_idx) = primitive.material_index {
                            let material = &self.materials[material_idx];
                            (
                                material.base_color_texture_index.map(|idx| idx as i32).unwrap_or(-1),
                                material.metallic_roughness_texture_index.map(|idx| idx as i32).unwrap_or(-1),
                                material.normal_texture_index.map(|idx| idx as i32).unwrap_or(-1),
                                material.base_color_factor,
                            )
                        } else {
                            (-1, -1, -1, [1.0, 1.0, 1.0, 1.0])
                        };

                    let push_consts = PushConstants {
                        model_transform: transform * node.global_transform,
                        base_color_idx,
                        metallic_roughness_idx,
                        normal_map_idx,
                        base_color_factor,
                        _pad: 0,
                    };

                    let v_node = graph.bind_resource(primitive.vertex_buffer.clone());
                    let i_node = graph.bind_resource(primitive.index_buffer.clone());

                    flat_draw_calls.push((push_consts, primitive.index_count, v_node, i_node));
                }
            }
        }

        let mut cmd_builder = graph
            .begin_cmd()
            .debug_name(format!("Scene {}", self.identifier))
            .bind_pipeline(&*self.pipeline)
            .multiview(crate::render::renderer::VIEW_MASK, 0)
            .shader_resource_access(0, *camera_ubo, AccessType::VertexShaderReadUniformBuffer)
            .color_attachment_image(0, *swapchain_image, LoadOp::Load, StoreOp::Store)
            .depth_stencil(DepthStencilInfo::DEPTH_WRITE_LESS)
            .depth_stencil_attachment_image(*depth_image, LoadOp::Load, StoreOp::Store);

        for (idx, texture) in self.textures.iter().enumerate() {
            let image_node = cmd_builder.bind_resource(texture);
            cmd_builder.set_shader_resource_access(
                (1, [idx as u32]),
                image_node,
                AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer,
            );
        }

        for (_, _, v_node, i_node) in &flat_draw_calls {
            cmd_builder = cmd_builder
                .resource_access(*i_node, AccessType::IndexBuffer)
                .resource_access(*v_node, AccessType::VertexBuffer);
        }

        cmd_builder.record_cmd(move |cmd| {
            for (push_consts, index_count, v_node, i_node) in flat_draw_calls {
                cmd.bind_index_buffer(i_node, 0, IndexType::UINT32)
                    .bind_vertex_buffer(0, v_node, 0)
                    .push_constants(0, bytes_of(&push_consts))
                    .draw_indexed(index_count, 1, 0, 0, 0);
            }
        })
            .end_cmd();
    }

    #[inline]
    fn convert_to_vk_format(gltf_format: Format, color: bool) -> vk::Format {
        match (gltf_format, color) {
            (Format::R8, true) | (Format::R8G8, true) | (Format::R8G8B8, true) | (Format::R8G8B8A8, true) => {
                vk::Format::R8G8B8A8_SRGB
            }

            // 3c shit is still upgraded to 4c because android
            (Format::R8, false) => vk::Format::R8_UNORM,
            (Format::R8G8, false) => vk::Format::R8G8_UNORM,
            (Format::R8G8B8, false) | (Format::R8G8B8A8, false) => vk::Format::R8G8B8A8_UNORM,
            (Format::R16, _) => vk::Format::R16_SFLOAT,
            (Format::R16G16, _) => vk::Format::R16G16_SFLOAT,
            (Format::R16G16B16, _) | (Format::R16G16B16A16, _) => vk::Format::R16G16B16A16_SFLOAT,
            _ => unreachable!()
        }
    }

    pub fn new(identifier: String, mut asset: ndk::asset::Asset, device: &Device, pipeline: Arc<GraphicsPipeline>) -> Self {
        let (document, buffers, images) = gltf::import_slice(asset.buffer().unwrap()).expect("Failed to parse GLTF asset");

        let mut scene = GltfScene {
            identifier,
            pipeline,
            nodes: Vec::new(),
            roots: Vec::new(),
            meshes: Vec::new(),
            materials: Vec::new(),
            textures: Vec::new(),
            specials: Vec::new(),
            node_extras: HashMap::new(),
        };

        let mut texture_is_color = vec![false; document.textures().count()];

        for material in document.materials() {
            let pbr = material.pbr_metallic_roughness();

            if let Some(tex_info) = pbr.base_color_texture() {
                texture_is_color[tex_info.texture().source().index()] = true;
            }
            if let Some(tex_info) = material.emissive_texture() {
                texture_is_color[tex_info.texture().source().index()] = true;
            }
        }

        let mut graph = Graph::default();
        let mut uploaded_textures = Vec::new();
        for (idx, gltf_image) in images.iter().enumerate() {
            let is_color = texture_is_color.get(idx).copied().unwrap_or(false);
            let pixel_data: Vec<u8> = match gltf_image.format {
                Format::R8G8B8 => {
                    let pixel_count = gltf_image.pixels.len() / 3;
                    let mut rgba_buffer = Vec::with_capacity(pixel_count * 4);
                    for chunk in gltf_image.pixels.chunks_exact(3) {
                        rgba_buffer.push(chunk[0]);
                        rgba_buffer.push(chunk[1]);
                        rgba_buffer.push(chunk[2]);
                        rgba_buffer.push(255);
                    }
                    rgba_buffer
                }
                Format::R16G16B16 => {
                    let pixel_count = gltf_image.pixels.len() / 6;
                    let mut rgba_buffer = Vec::with_capacity(pixel_count * 8);
                    for chunk in gltf_image.pixels.chunks_exact(6) {
                        rgba_buffer.extend_from_slice(&chunk[0..6]);
                        rgba_buffer.extend_from_slice(&[0x00, 0x3C]);
                    }
                    rgba_buffer
                }
                Format::R8 if is_color => {
                    let pixel_count = gltf_image.pixels.len();
                    let mut rgba_buffer = Vec::with_capacity(pixel_count * 4);
                    for &luminance in &gltf_image.pixels {
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(255);
                    }
                    rgba_buffer
                }
                Format::R8G8 if is_color => {
                    let pixel_count = gltf_image.pixels.len() / 2;
                    let mut rgba_buffer = Vec::with_capacity(pixel_count * 4);
                    for chunk in gltf_image.pixels.chunks_exact(2) {
                        let luminance = chunk[0];
                        let alpha = chunk[1];
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(luminance);
                        rgba_buffer.push(alpha);
                    }
                    rgba_buffer
                }
                _ => gltf_image.pixels.clone(),
            };

            let final_pixels = if matches!(gltf_image.format, Format::R8G8B8 | Format::R16G16B16)
                || (matches!(gltf_image.format, Format::R8 | Format::R8G8) && is_color)
            {
                &pixel_data
            } else {
                &gltf_image.pixels
            };

            let gpu_image_info = ImageInfoBuilder::default()
                .width(gltf_image.width)
                .height(gltf_image.height)
                .depth(1)
                .format(Self::convert_to_vk_format(gltf_image.format, is_color))
                .usage(vk::ImageUsageFlags::SAMPLED | vk::ImageUsageFlags::TRANSFER_DST);

            let image = Arc::new(Image::create(device, gpu_image_info).unwrap());
            uploaded_textures.push(image.clone());

            let image_node = graph.bind_resource(&image);
            let image_buf = graph.bind_resource(Buffer::create_from_slice(
                device,
                BufferUsageFlags::TRANSFER_SRC,
                final_pixels.as_slice()
            ).unwrap());
            graph.copy_buffer_to_image(image_buf, image_node);
        }
        graph.finalize().queue_submit(&mut HashPool::new(device), 0, 0).expect("Failed to upload images to GPU");
        scene.textures = uploaded_textures;

        for material in document.materials() {
            let pbr = material.pbr_metallic_roughness();

            let base_color_texture_index = pbr.base_color_texture().map(|tex_info| tex_info.texture().source().index());
            let metallic_roughness_texture_index = pbr.metallic_roughness_texture().map(|tex_info| tex_info.texture().source().index());
            let normal_texture_index = material.normal_texture().map(|tex_info| tex_info.texture().source().index());
            let base_color_factor = pbr.base_color_factor();

            info!("Base Color Factor: {:?}", base_color_factor);

            scene.materials.push(Arc::new(Material {
                base_color_texture_index,
                metallic_roughness_texture_index,
                normal_texture_index,
                base_color_factor,
            }));
        }

        for mesh in document.meshes() {
            let mut prims = Vec::new();
            for prim in mesh.primitives() {
                let reader = prim.reader(|buffer| Some(&buffers[buffer.index()]));
                let positions = reader.read_positions();
                let indices = reader.read_indices();
                let tex_coords = reader.read_tex_coords(0);
                let normals = reader.read_normals();
                let tangents = reader.read_tangents();

                if positions.is_none() || indices.is_none() || tex_coords.is_none() || normals.is_none() {
                    log::warn!("Mesh {} has primitive with missing data", mesh.name().unwrap_or("unknown"));
                    continue;
                }

                let positions = positions.unwrap();
                let indices = indices.unwrap();
                let tex_coords = tex_coords.unwrap();
                let normals = normals.unwrap();

                let tangent_vecs: Vec<[f32; 4]> = if let Some(t) = tangents {
                    t.collect()
                } else {
                    vec![[1.0, 0.0, 0.0, 1.0]; positions.len()]
                };

                let mut vertices = Vec::with_capacity(positions.len());

                let zipped = positions
                    .zip(tex_coords.into_f32())
                    .zip(normals)
                    .zip(tangent_vecs);

                for (((pos, uv), norm), tang) in zipped {
                    let n = glam::Vec3::from_array(norm).normalize();
                    let t = glam::Vec3::new(tang[0], tang[1], tang[2]).normalize();
                    let b = n.cross(t).normalize() * tang[3];

                    vertices.push(Vertex {
                        position: pos,
                        uv,
                        normal: n.to_array(),
                        tangent: t.to_array(),
                        bitangent: b.to_array(),
                    });
                }

                let indices: Vec<u32> = indices.into_u32().collect();

                let material_index = prim.material().index();
                let vertex_buffer = Arc::new(Buffer::create_from_slice(&device, BufferUsageFlags::VERTEX_BUFFER, bytemuck::cast_slice(&vertices)).unwrap());
                let index_buffer = Arc::new(Buffer::create_from_slice(&device, BufferUsageFlags::INDEX_BUFFER, bytemuck::cast_slice(&indices)).unwrap());
                prims.push(GltfPrimitive { vertex_buffer, index_buffer, index_count: indices.len() as u32, material_index });
            }

            scene.meshes.push(GltfMesh { name: mesh.name().unwrap_or("unnamed").to_string(), primitives: prims });
        }

        let gltf_scene = document.default_scene().expect("GLTF must have a default scene");
        for node in gltf_scene.nodes() {
            let root_idx = scene.load_node(&node);
            scene.roots.push(root_idx);
        }

        scene.update_scene_transforms();

        scene
    }

    fn load_node(&mut self, node: &Node) -> NodeIndex {
        let transform = node.transform().matrix();
        let local_transform = Mat4::from_cols_array_2d(&transform);

        let mesh_index = node.mesh().map(|m| m.index());

        if let Some(mesh) = node.mesh() {
            if let Some(extras) = mesh.extras() {
                let json_str = extras.get();
                if let Ok(ui_props) = serde_json::from_str::<Extras>(json_str) {
                    if ui_props.is_ui_surface.is_some() {
                        info!("Found UI surface mesh: {:?}", node.name().or(node.mesh().and_then(|m| m.name())));
                        self.specials.push(self.nodes.len());
                        self.node_extras.insert(self.nodes.len(), ui_props);
                    }
                }
            }
        }

        if let Some(extras) = node.extras() {
            let json_str = extras.get();
            if let Ok(ui_props) = serde_json::from_str::<Extras>(json_str) {
                if ui_props.is_spawnpoint.is_some() {
                    info!("Found spawn point node: {:?}", node.name().or(node.mesh().and_then(|m| m.name())));
                    self.specials.push(self.nodes.len());
                    self.node_extras.insert(self.nodes.len(), ui_props);
                }
            }
        }

        let new_node = GltfNode {
            local_transform,
            global_transform: Mat4::IDENTITY,
            mesh_index,
            children: Vec::new()
        };

        let node_idx = self.nodes.len();
        self.nodes.push(new_node);

        let child_indices: Vec<NodeIndex> = node.children()
            .map(|child| self.load_node(&child))
            .collect();

        self.nodes[node_idx].children = child_indices;

        node_idx
    }

    pub fn update_scene_transforms(&mut self) {
        let coordinate_correction = Mat4::from_scale(glam::vec3(1.0, 1.0, 1.0));

        for root_idx in self.roots.clone() {
            self.update_node_transform(root_idx, coordinate_correction);
        }
    }

    fn update_node_transform(&mut self, idx: NodeIndex, parent_transform: Mat4) {
        let (current_global, children) = {
            let node = &mut self.nodes[idx];
            node.global_transform = parent_transform * node.local_transform;

            (node.global_transform, node.children.clone())
        };

        for child_idx in children {
            self.update_node_transform(child_idx, current_global);
        }
    }

    /// extras helpers
    pub fn find_spawnpoint_transform(&self) -> Option<Mat4> {
        self.specials.iter().find_map(|&idx| {
            if let Some(extras) = self.node_extras.get(&idx) {
                if extras.is_spawnpoint == Some(1) {
                    return Some(self.nodes[idx].global_transform)
                }
            }
            None
        })
    }
}