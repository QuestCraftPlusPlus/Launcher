use vk_graph::driver::descriptor_set::{DescriptorSet, DescriptorSetBinding, DescriptorSetInfo, DescriptorSetUpdateInfo};
use {
    std::{collections::HashMap, sync::Arc},
    bytemuck::{bytes_of, Pod, Zeroable},
    glam::{Quat, Vec3, Vec4, Mat4},
    gltf::{image::Format, Node},
    log::info,
    serde::Deserialize,
    vk_graph::{
        driver::{
            ash::vk::{self, BufferUsageFlags, IndexType},
            buffer::Buffer,
            device::Device,
            graphics::{DepthStencilInfo, GraphicsPipeline},
            sync::AccessType,
            image::{Image, ImageInfoBuilder},
        },
        Graph, LoadOp, StoreOp,
        pool::hash::HashPool,
    },
    crate::render::renderer::DrawPayload
};

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
pub struct DrawIndexedIndirectCommand {
    pub index_count: u32,
    pub instance_count: u32,
    pub first_index: u32,
    pub vertex_offset: i32,
    pub first_instance: u32,
}

pub struct GltfPrimitive {
    pub material_index: Option<usize>,

    pub first_index: u32,
    pub index_count: u32,
    pub base_vertex: i32,
    pub has_skinning_data: bool,

    pub cpu_vertex_buffer: Option<Vec<Vertex>>,
    pub cpu_index_buffer: Option<Vec<u32>>,
}

pub struct GltfMesh {
    pub name: String,
    pub special: bool,
    pub primitives: Vec<GltfPrimitive>
}

pub struct Material {
    pub base_color_texture_index: Option<usize>,
    pub metallic_roughness_texture_index: Option<usize>,
    pub normal_texture_index: Option<usize>,
    pub base_color_factor: [f32; 4],
    pub double_sided: bool,
}

pub type NodeIndex = usize;

#[derive(Clone)]
pub struct GltfNode {
    pub translation: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
    pub global_transform: Mat4,
    pub mesh_index: Option<usize>,
    pub skin_index: Option<usize>,
    pub children: Vec<NodeIndex>,
}

pub struct GltfAsset {
    pub identifier: String,
    pub culled_pipeline: Arc<GraphicsPipeline>,
    pub no_cull_pipeline: Arc<GraphicsPipeline>,

    pub bind_pose_nodes: Vec<GltfNode>,
    pub roots: Vec<NodeIndex>,
    pub meshes: Vec<GltfMesh>,
    pub textures: Vec<Arc<Image>>,
    pub specials: Vec<NodeIndex>,
    pub skins: Vec<Skin>,
    pub animations: Vec<GltfAnimation>,
    pub total_joint_count: u32,

    pub vertex_buffer: Arc<Buffer>,
    pub index_buffer: Arc<Buffer>,
    pub indirect_buffer: Arc<Buffer>,
    pub culled_draw_count: u32,
    pub no_cull_draw_count: u32,

    pub node_extras: HashMap<NodeIndex, Extras>,

    base_instance_data: Vec<PushConstants>,
    instance_node_map: Vec<(NodeIndex, bool)>,
}

impl GltfAsset {
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

    pub fn new(identifier: String, mut asset: ndk::asset::Asset, device: &Device, culled_pipeline: Arc<GraphicsPipeline>, no_cull_pipeline: Arc<GraphicsPipeline>) -> Self {
        let (document, buffers, images) = gltf::import_slice(asset.buffer().unwrap()).expect("Failed to parse GLTF asset");

        let mut scene_nodes = Vec::new();
        let mut scene_roots = Vec::new();
        let mut scene_meshes = Vec::new();
        let mut scene_materials = Vec::new();
        let scene_textures: Vec<Arc<Image>>;
        let mut scene_specials = Vec::new();
        let mut node_extras = HashMap::new();
        let mut gltf_node_map: HashMap<usize, NodeIndex> = HashMap::new();

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
        scene_textures = uploaded_textures;

        for material in document.materials() {
            let pbr = material.pbr_metallic_roughness();

            let base_color_texture_index = pbr.base_color_texture().map(|tex_info| tex_info.texture().source().index());
            let metallic_roughness_texture_index = pbr.metallic_roughness_texture().map(|tex_info| tex_info.texture().source().index());
            let normal_texture_index = material.normal_texture().map(|tex_info| tex_info.texture().source().index());
            let base_color_factor = pbr.base_color_factor();

            info!("Base Color Factor: {:?}", base_color_factor);

            scene_materials.push(Arc::new(Material {
                base_color_texture_index,
                metallic_roughness_texture_index,
                normal_texture_index,
                base_color_factor,
                double_sided: material.double_sided()
            }));
        }

        let mut all_vertices: Vec<Vertex> = Vec::new();
        let mut all_indices: Vec<u32> = Vec::new();
        for mesh in document.meshes() {
            let mut extras = None;
            if let Some(raw_extras) = mesh.extras() {
                if let Ok(ui_props) = serde_json::from_str::<Extras>(raw_extras.get()) {
                    extras = Some(ui_props);
                }
            }
            let store_cpu_side_data = {
                if let Some(ref extras) = extras && extras.is_ui_surface.map_or(false, |val| val == 1) {
                    true
                } else {
                    false
                }
            };

            let mut prims = Vec::new();
            for prim in mesh.primitives() {
                let reader = prim.reader(|buffer| Some(&buffers[buffer.index()]));
                let positions = reader.read_positions();
                let indices = reader.read_indices();
                let tex_coords = reader.read_tex_coords(0);
                let normals = reader.read_normals();
                let tangents = reader.read_tangents();
                let joint_weights = reader.read_weights(0);
                let joint_indices = reader.read_joints(0);

                if positions.is_none() || indices.is_none() || tex_coords.is_none() || normals.is_none() {
                    log::warn!("Mesh {} has primitive with missing data", mesh.name().unwrap_or("unknown"));
                    continue;
                }

                let positions = positions.unwrap();
                let indices = indices.unwrap();
                let tex_coords = tex_coords.unwrap();
                let normals = normals.unwrap();

                let vertex_count = positions.len();

                let tangent_vecs: Vec<[f32; 4]> = if let Some(t) = tangents {
                    t.collect()
                } else {
                    vec![[1.0, 0.0, 0.0, 1.0]; vertex_count]
                };

                let mut vertices = Vec::with_capacity(vertex_count);

                let has_skinning_data = joint_indices.is_some() && joint_weights.is_some();
                if joint_indices.is_none() || joint_weights.is_none() {
                    log::warn!("Primitive in mesh {} has no JOINTS_0/WEIGHTS_0, skinning will be a no-op", mesh.name().unwrap_or("unknown"));
                }

                let joint_data: Vec<([u32; 4], [f32; 4])> = match (joint_indices, joint_weights) {
                    (Some(indices), Some(weights)) => indices.into_u16()
                        .zip(weights.into_f32())
                        .map(|(idx, w)| ([idx[0] as u32, idx[1] as u32, idx[2] as u32, idx[3] as u32], w))
                        .collect(),
                    _ => vec![([0u32; 4], [0.0f32; 4]); vertex_count],
                };

                let zipped = positions
                    .zip(tex_coords.into_f32())
                    .zip(normals)
                    .zip(tangent_vecs)
                    .zip(joint_data);

                for ((((pos, uv), norm), tang), (joint_indices, joint_weights)) in zipped {
                    let n = Vec3::from_array(norm).normalize();
                    let t = Vec3::new(tang[0], tang[1], tang[2]).normalize();
                    let b = n.cross(t).normalize() * tang[3];

                    vertices.push(Vertex {
                        position: pos,
                        uv,
                        normal: n.to_array(),
                        tangent: t.to_array(),
                        bitangent: b.to_array(),
                        joint_indices,
                        joint_weights,
                    });
                }

                let mut indices: Vec<u32> = indices.into_u32().collect();

                Self::fix_triangle_winding(&vertices, &mut indices);

                let base_vertex = all_vertices.len() as i32;
                let first_index = all_indices.len() as u32;
                let index_count = indices.len() as u32;

                let material_index = prim.material().index();
                prims.push(GltfPrimitive {
                    material_index,
                    index_count,
                    first_index,
                    base_vertex,
                    has_skinning_data,
                    cpu_vertex_buffer: if store_cpu_side_data { Some(vertices.clone()) } else { None },
                    cpu_index_buffer: if store_cpu_side_data { Some(indices.clone()) } else { None },
                });

                all_vertices.extend(vertices);
                all_indices.extend(indices);
            }

            scene_meshes.push(GltfMesh {
                name: mesh.name().unwrap_or("unnamed").to_string(),
                special: extras.is_some_and(|e| {e.is_ui_surface.is_some() || e.is_spawnpoint.is_some()}),
                primitives: prims
            });
        }

        info!("all_vertices: {}, all_indices: {}", all_vertices.len(), all_indices.len());
        let vertex_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::VERTEX_BUFFER,
            bytemuck::cast_slice(&all_vertices),
        ).unwrap());

        let index_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::INDEX_BUFFER,
            bytemuck::cast_slice(&all_indices),
        ).unwrap());

        let gltf_scene = document.default_scene().expect("GLTF must have a default scene");
        for node in gltf_scene.nodes() {
            let root_idx = Self::load_node(&mut scene_nodes, &mut scene_specials, &mut node_extras, &mut gltf_node_map, &node);
            scene_roots.push(root_idx);
        }

        Self::update_scene_transforms(&mut scene_nodes, &scene_roots);

        let scene_skins = Self::load_skins(&document, &buffers, &gltf_node_map);
        let scene_animations = Self::load_animations(&document, &buffers, &gltf_node_map);
        let total_joint_count: u32 = scene_skins.iter().map(|s| s.joint_nodes.len() as u32).sum();
        info!("skins: {}, animations: {}, total joints: {}", scene_skins.len(), scene_animations.len(), total_joint_count);

        let (base_instance_data, instance_node_map, culled_commands, no_cull_commands) =
            Self::build_draw_data(&scene_nodes, &scene_meshes, &scene_materials, &scene_skins);

        let culled_draw_count = culled_commands.len() as u32;
        let no_cull_draw_count = no_cull_commands.len() as u32;

        info!("instance_data: {}, culled draws: {}, no-cull draws: {}", base_instance_data.len(), culled_draw_count, no_cull_draw_count);

        let mut all_commands = culled_commands;
        all_commands.extend(no_cull_commands);

        let indirect_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::STORAGE_BUFFER,
            bytemuck::cast_slice(&all_commands),
        ).expect("Failed to create indirect buffer"));

        GltfAsset {
            identifier,
            culled_pipeline,
            no_cull_pipeline,
            bind_pose_nodes: scene_nodes,
            roots: scene_roots,
            meshes: scene_meshes,
            textures: scene_textures,
            specials: scene_specials,
            skins: scene_skins,
            animations: scene_animations,
            total_joint_count,
            vertex_buffer,
            index_buffer,
            indirect_buffer,
            culled_draw_count,
            no_cull_draw_count,
            node_extras,
            base_instance_data,
            instance_node_map,
        }
    }

    pub fn find_animation(&self, clip_name: &str) -> Option<usize> {
        self.animations.iter().position(|a| a.name == clip_name)
    }

    pub fn animation_names(&self) -> impl Iterator<Item = &str> {
        self.animations.iter().map(|a| a.name.as_str())
    }

    fn stride(interpolation: Interpolation) -> usize {
        match interpolation {
            Interpolation::CubicSpline => 3,
            _ => 1,
        }
    }

    fn find_keyframe_indices(times: &[f32], t: f32) -> (usize, usize, f32) {
        if times.len() <= 1 || t <= times[0] {
            return (0, 0, 0.0);
        }
        let last = times.len() - 1;
        if t >= times[last] {
            return (last, last, 0.0);
        }

        let next = times.partition_point(|&time| time <= t);
        let prev = next - 1;
        let span = times[next] - times[prev];
        let factor = if span > 0.0 { (t - times[prev]) / span } else { 0.0 };
        (prev, next, factor)
    }

    fn hermite_vec3(p0: Vec3, m0: Vec3, p1: Vec3, m1: Vec3, t: f32) -> Vec3 {
        let (t2, t3) = (t * t, t * t * t);
        let h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        let h10 = t3 - 2.0 * t2 + t;
        let h01 = -2.0 * t3 + 3.0 * t2;
        let h11 = t3 - t2;
        p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11
    }

    fn hermite_vec4(p0: Vec4, m0: Vec4, p1: Vec4, m1: Vec4, t: f32) -> Vec4 {
        let (t2, t3) = (t * t, t * t * t);
        let h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        let h10 = t3 - 2.0 * t2 + t;
        let h01 = -2.0 * t3 + 3.0 * t2;
        let h11 = t3 - t2;
        p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11
    }

    fn sample_vec3(times: &[f32], values: &[Vec3], t: f32, interp: Interpolation) -> Vec3 {
        let stride = Self::stride(interp);
        let (prev, next, factor) = Self::find_keyframe_indices(times, t);

        match interp {
            Interpolation::Step => values[prev * stride],
            Interpolation::Linear => {
                if prev == next { values[prev] } else { values[prev].lerp(values[next], factor) }
            }
            Interpolation::CubicSpline => {
                if prev == next {
                    values[prev * stride + 1]
                } else {
                    let dt = times[next] - times[prev];
                    let p0 = values[prev * stride + 1];
                    let m0 = values[prev * stride + 2] * dt;
                    let p1 = values[next * stride + 1];
                    let m1 = values[next * stride] * dt;
                    Self::hermite_vec3(p0, m0, p1, m1, factor)
                }
            }
        }
    }

    fn sample_quat(times: &[f32], values: &[Quat], t: f32, interp: Interpolation) -> Quat {
        let stride = Self::stride(interp);
        let (prev, next, factor) = Self::find_keyframe_indices(times, t);

        match interp {
            Interpolation::Step => values[prev * stride],
            Interpolation::Linear => {
                if prev == next { values[prev] } else { values[prev].slerp(values[next], factor) }
            }
            Interpolation::CubicSpline => {
                if prev == next {
                    values[prev * stride + 1]
                } else {
                    let dt = times[next] - times[prev];
                    let to_v4 = |q: Quat| Vec4::new(q.x, q.y, q.z, q.w);
                    let p0 = to_v4(values[prev * stride + 1]);
                    let m0 = to_v4(values[prev * stride + 2]) * dt;
                    let p1 = to_v4(values[next * stride + 1]);
                    let m1 = to_v4(values[next * stride]) * dt;
                    let blended = Self::hermite_vec4(p0, m0, p1, m1, factor);
                    Quat::from_xyzw(blended.x, blended.y, blended.z, blended.w).normalize()
                }
            }
        }
    }

    fn sample_animation(nodes: &mut Vec<GltfNode>, clip: &GltfAnimation, time: f32) {
        for channel in &clip.channels {
            let node = &mut nodes[channel.target_node];

            if let Some(translations) = &channel.translations {
                node.translation = Self::sample_vec3(&channel.times, translations, time, channel.interpolation);
            }
            if let Some(rotations) = &channel.rotations {
                node.rotation = Self::sample_quat(&channel.times, rotations, time, channel.interpolation);
            }
            if let Some(scales) = &channel.scales {
                node.scale = Self::sample_vec3(&channel.times, scales, time, channel.interpolation);
            }
        }
    }

    fn load_skins(
        document: &gltf::Document,
        buffers: &[gltf::buffer::Data],
        gltf_to_local: &HashMap<usize, NodeIndex>,
    ) -> Vec<Skin> {
        let mut skins = Vec::new();
        let mut joint_matrix_offset = 0u32;

        for skin in document.skins() {
            let reader = skin.reader(|buffer| Some(&buffers[buffer.index()]));

            let joint_nodes: Vec<NodeIndex> = skin.joints()
                .map(|joint_node| *gltf_to_local.get(&joint_node.index())
                    .expect("skin references a joint node outside the default scene graph"))
                .collect();

            let inverse_bind_matrices: Vec<Mat4> = match reader.read_inverse_bind_matrices() {
                Some(ibm) => ibm.map(|m| Mat4::from_cols_array_2d(&m)).collect(),
                None => vec![Mat4::IDENTITY; joint_nodes.len()],
            };

            let skeleton_root = skin.skeleton().and_then(|n| gltf_to_local.get(&n.index()).copied());

            let joint_count = joint_nodes.len() as u32;
            skins.push(Skin {
                joint_nodes,
                inverse_bind_matrices,
                skeleton_root,
                joint_matrix_offset,
            });
            joint_matrix_offset += joint_count;
        }

        skins
    }

    fn load_animations(
        document: &gltf::Document,
        buffers: &[gltf::buffer::Data],
        gltf_to_local: &HashMap<usize, NodeIndex>,
    ) -> Vec<GltfAnimation> {
        let mut animations = Vec::new();

        for anim in document.animations() {
            let mut channels = Vec::new();
            let mut duration = 0.0f32;

            for channel in anim.channels() {
                let Some(&target_node) = gltf_to_local.get(&channel.target().node().index()) else {
                    continue;
                };

                let reader = channel.reader(|buffer| Some(&buffers[buffer.index()]));

                let Some(times_iter) = reader.read_inputs() else { continue };
                let times: Vec<f32> = times_iter.collect();
                if let Some(&last) = times.last() {
                    duration = duration.max(last);
                }

                let interpolation = match channel.sampler().interpolation() {
                    gltf::animation::Interpolation::Linear => Interpolation::Linear,
                    gltf::animation::Interpolation::Step => Interpolation::Step,
                    gltf::animation::Interpolation::CubicSpline => Interpolation::CubicSpline,
                };

                let Some(outputs) = reader.read_outputs() else { continue };
                let (translations, rotations, scales) = match outputs {
                    gltf::animation::util::ReadOutputs::Translations(t) => {
                        (Some(t.map(Vec3::from_array).collect()), None, None)
                    }
                    gltf::animation::util::ReadOutputs::Rotations(r) => {
                        (None, Some(r.into_f32().map(Quat::from_array).collect()), None)
                    }
                    gltf::animation::util::ReadOutputs::Scales(s) => {
                        (None, None, Some(s.map(Vec3::from_array).collect()))
                    }
                    gltf::animation::util::ReadOutputs::MorphTargetWeights(_) => {
                        continue;
                    }
                };

                channels.push(AnimationChannel {
                    target_node,
                    times,
                    translations,
                    rotations,
                    scales,
                    interpolation,
                });
            }

            animations.push(GltfAnimation {
                name: anim.name().unwrap_or("unnamed").to_string(),
                channels,
                duration,
            });
        }

        animations
    }

    fn build_draw_data(
        nodes: &Vec<GltfNode>,
        meshes: &Vec<GltfMesh>,
        materials: &Vec<Arc<Material>>,
        skins: &Vec<Skin>,
    ) -> (Vec<PushConstants>, Vec<(NodeIndex, bool)>, Vec<DrawIndexedIndirectCommand>, Vec<DrawIndexedIndirectCommand>) {
        let mut instance_data = Vec::new();
        let mut instance_node_map = Vec::new();
        let mut culled_commands = Vec::new();
        let mut no_cull_commands = Vec::new();

        for (node_idx, node) in nodes.iter().enumerate() {
            let Some(mesh_idx) = node.mesh_index else { continue };
            let mesh = &meshes[mesh_idx];
            if mesh.special {
                continue;
            }

            for primitive in &mesh.primitives {
                let is_skinned = node.skin_index.is_some() && primitive.has_skinning_data;
                let skin_joint_offset = if is_skinned {
                    node.skin_index.map(|idx| skins[idx].joint_matrix_offset as i32).unwrap_or(-1)
                } else {
                    -1
                };

                let (base_color_idx, metallic_roughness_idx, normal_map_idx, base_color_factor, double_sided) =
                    if let Some(material_idx) = primitive.material_index {
                        let material = &materials[material_idx];
                        (
                            material.base_color_texture_index.map(|i| i as i32).unwrap_or(-1),
                            material.metallic_roughness_texture_index.map(|i| i as i32).unwrap_or(-1),
                            material.normal_texture_index.map(|i| i as i32).unwrap_or(-1),
                            material.base_color_factor,
                            material.double_sided,
                        )
                    } else {
                        (-1, -1, -1, [1.0, 1.0, 1.0, 1.0], false)
                    };

                let first_instance = instance_data.len() as u32;

                instance_data.push(PushConstants {
                    model_transform: if is_skinned { Mat4::IDENTITY } else { node.global_transform },
                    material: ShaderMaterial {
                        base_color_idx,
                        metallic_roughness_idx,
                        normal_map_idx,
                        _pad: 0,
                    },
                    base_color_factor,
                    skin_joint_offset,
                    _pad: [0; 3],
                });
                instance_node_map.push((node_idx, is_skinned));

                let command = DrawIndexedIndirectCommand {
                    index_count: primitive.index_count,
                    instance_count: 1,
                    first_index: primitive.first_index,
                    vertex_offset: primitive.base_vertex,
                    first_instance,
                };

                if double_sided {
                    no_cull_commands.push(command);
                } else {
                    culled_commands.push(command);
                }
            }
        }

        (instance_data, instance_node_map, culled_commands, no_cull_commands)
    }

    fn load_node(
        nodes: &mut Vec<GltfNode>,
        specials: &mut Vec<NodeIndex>,
        node_extras: &mut HashMap<NodeIndex, Extras>,
        gltf_to_local: &mut HashMap<usize, NodeIndex>,
        node: &Node,
    ) -> NodeIndex {
        let (translation, rotation, scale) = node.transform().decomposed();
        let mesh_index = node.mesh().map(|m| m.index());
        let skin_index = node.skin().map(|s| s.index());

        if let Some(mesh) = node.mesh() {
            if let Some(extras) = mesh.extras() {
                let json_str = extras.get();
                if let Ok(ui_props) = serde_json::from_str::<Extras>(json_str) {
                    if ui_props.is_ui_surface.is_some() || ui_props.is_grab_bar.is_some() {
                        info!("Found special mesh: {:?}", node.name().or(node.mesh().and_then(|m| m.name())));
                        specials.push(nodes.len());
                        node_extras.insert(nodes.len(), ui_props);
                    }
                }
            }
        }

        if let Some(extras) = node.extras() {
            let json_str = extras.get();
            if let Ok(ui_props) = serde_json::from_str::<Extras>(json_str) {
                if ui_props.is_spawnpoint.is_some() {
                    info!("Found spawn point node: {:?}", node.name().or(node.mesh().and_then(|m| m.name())));
                    specials.push(nodes.len());
                    node_extras.insert(nodes.len(), ui_props);
                }
            }
        }

        let new_node = GltfNode {
            translation: Vec3::from_array(translation),
            rotation: Quat::from_array(rotation),
            scale: Vec3::from_array(scale),
            global_transform: Mat4::IDENTITY,
            mesh_index,
            skin_index,
            children: Vec::new()
        };

        let node_idx = nodes.len();
        gltf_to_local.insert(node.index(), node_idx);
        nodes.push(new_node);

        let child_indices: Vec<NodeIndex> = node.children()
            .map(|child| Self::load_node(nodes, specials, node_extras, gltf_to_local, &child))
            .collect();

        nodes[node_idx].children = child_indices;

        node_idx
    }

    fn fix_triangle_winding(vertices: &[Vertex], indices: &mut [u32]) {
        for tri in indices.chunks_exact_mut(3) {
            let [a, b, c] = [tri[0] as usize, tri[1] as usize, tri[2] as usize];
            let (pa, pb, pc) = (
                Vec3::from(vertices[a].position),
                Vec3::from(vertices[b].position),
                Vec3::from(vertices[c].position),
            );

            let face_normal = (pb - pa).cross(pc - pa);
            let avg_normal = Vec3::from(vertices[a].normal)
                + Vec3::from(vertices[b].normal)
                + Vec3::from(vertices[c].normal);

            if face_normal.dot(avg_normal) < 0.0 {
                tri.swap(0, 2);
            }
        }
    }

    pub fn update_scene_transforms(nodes: &mut Vec<GltfNode>, roots: &Vec<NodeIndex>) {
        let coordinate_correction = Mat4::from_scale(glam::vec3(1.0, 1.0, 1.0));

        for root_idx in roots {
            Self::update_node_transform(nodes, *root_idx, coordinate_correction);
        }
    }

    fn update_node_transform(nodes: &mut Vec<GltfNode>, idx: NodeIndex, parent_transform: Mat4) {
        let (current_global, children) = {
            let node = &mut nodes[idx];
            let local = Mat4::from_scale_rotation_translation(node.scale, node.rotation, node.translation);
            node.global_transform = parent_transform * local;

            (node.global_transform, node.children.clone())
        };

        for child_idx in children {
            Self::update_node_transform(nodes, child_idx, current_global);
        }
    }
}

pub struct GltfInstance {
    pub asset: Arc<GltfAsset>,
    pub nodes: Vec<GltfNode>,

    descriptor_sets: Vec<DescriptorSet>,

    instance_data: Vec<PushConstants>,
    instance_buffer: Arc<Buffer>,
    joint_matrix_buffer: Arc<Buffer>,
}

impl GltfInstance {
    pub fn new(asset: Arc<GltfAsset>, device: &Device, camera_buffers: &Vec<Arc<Buffer>>) -> Self {
        let nodes = asset.bind_pose_nodes.clone();
        let instance_data = asset.base_instance_data.clone();

        let instance_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::STORAGE_BUFFER,
            bytemuck::cast_slice(&instance_data),
        ).expect("Failed to create instance buffer"));

        let joint_matrix_seed = vec![Mat4::IDENTITY; asset.total_joint_count.max(1) as usize];
        let joint_matrix_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::STORAGE_BUFFER,
            bytemuck::cast_slice(&joint_matrix_seed),
        ).expect("Failed to create joint matrix buffer"));

        let mut descriptor_sets = Vec::new();

        for camera_buffer in camera_buffers {
            let mut descriptors = Vec::new();
            descriptors.push(DescriptorSetUpdateInfo::buffer(0, camera_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(1, &instance_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(2, &joint_matrix_buffer));

            for (idx, texture) in asset.textures.iter().enumerate() {
                descriptors.push(DescriptorSetUpdateInfo::image(DescriptorSetBinding { binding: 3, array_element: idx as u32 }, texture));
            }

            descriptor_sets.push(DescriptorSet::alloc_and_update(
                &*asset.culled_pipeline,
                DescriptorSetInfo::builder().set(0).build(),
                descriptors
            ).expect("Failed to allocate and update descriptor set 0"));
        }

        GltfInstance {
            asset,
            nodes,
            descriptor_sets,
            instance_data,
            instance_buffer,
            joint_matrix_buffer,
        }
    }
    
    pub fn override_textures(&mut self, texture: &Arc<Image>, camera_buffers: &Vec<Arc<Buffer>>) {
        let mut descriptor_sets = Vec::new();

        for camera_buffer in camera_buffers {
            let mut descriptors = Vec::new();
            descriptors.push(DescriptorSetUpdateInfo::buffer(0, camera_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(1, &self.instance_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(2, &self.joint_matrix_buffer));

            for (idx, _) in self.asset.textures.iter().enumerate() {
                descriptors.push(DescriptorSetUpdateInfo::image(DescriptorSetBinding { binding: 3, array_element: idx as u32 }, texture));
            }

            descriptor_sets.push(DescriptorSet::alloc_and_update(
                &*self.asset.culled_pipeline,
                DescriptorSetInfo::builder().set(0).build(),
                descriptors
            ).expect("Failed to allocate and update descriptor set 0"));
        }
        
        self.descriptor_sets = descriptor_sets;
    }
    
    // note: this isn't really normally necessary to use, but it's here in case we'd ever want to reset the textures for whatever reason.
    pub fn recreate_descriptor_sets(&mut self, camera_buffers: &Vec<Arc<Buffer>>) {
        let mut descriptor_sets = Vec::new();

        for camera_buffer in camera_buffers {
            let mut descriptors = Vec::new();
            descriptors.push(DescriptorSetUpdateInfo::buffer(0, camera_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(1, &self.instance_buffer));
            descriptors.push(DescriptorSetUpdateInfo::buffer(2, &self.joint_matrix_buffer));

            for (idx, texture) in self.asset.textures.iter().enumerate() {
                descriptors.push(DescriptorSetUpdateInfo::image(DescriptorSetBinding { binding: 3, array_element: idx as u32 }, texture));
            }

            descriptor_sets.push(DescriptorSet::alloc_and_update(
                &*self.asset.culled_pipeline,
                DescriptorSetInfo::builder().set(0).build(),
                descriptors
            ).expect("Failed to allocate and update descriptor set 0"));
        }

        self.descriptor_sets = descriptor_sets;
    }

    pub fn create_animation_player(&self, clip_name: &str, looping: bool) -> Option<AnimationPlayer> {
        self.asset.find_animation(clip_name).map(|idx| AnimationPlayer::new(idx, looping))
    }

    pub fn animate(&mut self, device: &Device, player: &AnimationPlayer) {
        if let Some(clip) = self.asset.animations.get(player.clip_index) {
            GltfAsset::sample_animation(&mut self.nodes, clip, player.time);
        }
        GltfAsset::update_scene_transforms(&mut self.nodes, &self.asset.roots);
        self.upload_joint_matrices(device);
        self.refresh_instance_buffer(device);
    }

    fn upload_joint_matrices(&mut self, device: &Device) {
        let count = self.asset.total_joint_count.max(1) as usize;
        let mut joint_matrices = vec![Mat4::IDENTITY; count];

        for skin in &self.asset.skins {
            for (i, (&joint_node, ibm)) in skin.joint_nodes.iter().zip(&skin.inverse_bind_matrices).enumerate() {
                joint_matrices[skin.joint_matrix_offset as usize + i] =
                    self.nodes[joint_node].global_transform * *ibm;
            }
        }

        self.joint_matrix_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::STORAGE_BUFFER,
            bytemuck::cast_slice(&joint_matrices),
        ).expect("Failed to upload joint matrices"));
    }

    fn refresh_instance_buffer(&mut self, device: &Device) {
        for (data, (node_idx, is_skinned)) in self.instance_data.iter_mut().zip(self.asset.instance_node_map.iter()) {
            data.model_transform = if *is_skinned {
                Mat4::IDENTITY
            } else {
                self.nodes[*node_idx].global_transform
            };
        }

        self.instance_buffer = Arc::new(Buffer::create_from_slice(
            device,
            BufferUsageFlags::STORAGE_BUFFER,
            bytemuck::cast_slice(&self.instance_data),
        ).expect("Failed to refresh instance buffer"));
    }

    pub fn record(&self, graph: &mut Graph, draw_payload: &DrawPayload) {
        self.record_with_transform(graph, draw_payload, &Mat4::IDENTITY);
    }

    pub fn record_with_transform(&self, graph: &mut Graph, draw_payload: &DrawPayload, transform: &Mat4) {
        #[cfg(feature = "profiled")]
        profiling::function_scope!();

        let asset = &self.asset;
        let v_node = graph.bind_resource(asset.vertex_buffer.clone());
        let i_node = graph.bind_resource(asset.index_buffer.clone());
        let instance_node = graph.bind_resource(self.instance_buffer.clone());
        let indirect_node = graph.bind_resource(asset.indirect_buffer.clone());
        let joint_node = graph.bind_resource(self.joint_matrix_buffer.clone());

        let scene_transform = *transform;
        let stride = size_of::<DrawIndexedIndirectCommand>() as vk::DeviceSize;
        let no_cull_offset = stride * asset.culled_draw_count as vk::DeviceSize;

        if asset.culled_draw_count > 0 {
            let mut cmd_builder = graph
                .begin_cmd()
                .debug_name(format!("Scene {} (culled)", asset.identifier))
                .bind_pipeline(&*asset.culled_pipeline)
                .bind_descriptor_set(&self.descriptor_sets[draw_payload.frame_in_flight])
                .multiview(crate::render::renderer::VIEW_MASK, 0)
                .resource_access(*draw_payload.camera_ubo, AccessType::VertexShaderReadUniformBuffer)
                .resource_access(instance_node, AccessType::VertexShaderReadOther)
                .resource_access(joint_node, AccessType::VertexShaderReadOther)
                .color_attachment_image(0, *draw_payload.color_target, LoadOp::Load, StoreOp::Store)
                .depth_stencil(DepthStencilInfo::DEPTH_WRITE_LESS)
                .depth_stencil_attachment_image(*draw_payload.depth_target, LoadOp::Load, StoreOp::Store)
                .resource_access(i_node, AccessType::IndexBuffer)
                .resource_access(v_node, AccessType::VertexBuffer)
                .resource_access(indirect_node, AccessType::IndirectBuffer);

            for texture in &asset.textures {
                let image_node = cmd_builder.bind_resource(texture);
                cmd_builder.set_resource_access(
                    image_node,
                    AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer,
                );
            }

            let draw_count = asset.culled_draw_count;
            cmd_builder.record_cmd(move |cmd| {
                cmd.bind_index_buffer(i_node, 0, IndexType::UINT32)
                    .bind_vertex_buffer(0, v_node, 0)
                    .push_constants(0, bytes_of(&scene_transform))
                    .draw_indexed_indirect(indirect_node, 0, draw_count, stride as u32);
            }).end_cmd();
        }

        if asset.no_cull_draw_count > 0 {
            let mut cmd_builder = graph
                .begin_cmd()
                .debug_name(format!("Scene {} (no-cull)", asset.identifier))
                .bind_pipeline(&*asset.no_cull_pipeline)
                .bind_descriptor_set(&self.descriptor_sets[draw_payload.frame_in_flight])
                .multiview(crate::render::renderer::VIEW_MASK, 0)
                .resource_access(*draw_payload.camera_ubo, AccessType::VertexShaderReadUniformBuffer)
                .resource_access(instance_node, AccessType::VertexShaderReadOther)
                .resource_access(joint_node, AccessType::VertexShaderReadOther)
                .color_attachment_image(0, *draw_payload.color_target, LoadOp::Load, StoreOp::Store)
                .depth_stencil(DepthStencilInfo::DEPTH_WRITE_LESS)
                .depth_stencil_attachment_image(*draw_payload.depth_target, LoadOp::Load, StoreOp::Store)
                .resource_access(i_node, AccessType::IndexBuffer)
                .resource_access(v_node, AccessType::VertexBuffer)
                .resource_access(indirect_node, AccessType::IndirectBuffer);

            for texture in &asset.textures {
                let image_node = cmd_builder.bind_resource(texture);
                cmd_builder.set_resource_access(
                    image_node,
                    AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer,
                );
            }

            let draw_count = asset.no_cull_draw_count;
            cmd_builder.record_cmd(move |cmd| {
                cmd.bind_index_buffer(i_node, 0, IndexType::UINT32)
                    .bind_vertex_buffer(0, v_node, 0)
                    .push_constants(0, bytes_of(&scene_transform))
                    .draw_indexed_indirect(indirect_node, no_cull_offset, draw_count, stride as u32);
            }).end_cmd();
        }
    }

    /// extras helpers
    pub fn find_spawnpoint_transform(&self) -> Option<Mat4> {
        self.asset.specials.iter().find_map(|&idx| {
            if let Some(extras) = self.asset.node_extras.get(&idx) {
                if extras.is_spawnpoint == Some(1) {
                    return Some(self.nodes[idx].global_transform)
                }
            }
            None
        })
    }

    pub fn find_surface_index(&self) -> Option<NodeIndex> {
        self.asset.specials.iter().find_map(|&idx| {
            if let Some(extras) = self.asset.node_extras.get(&idx) {
                if extras.is_ui_surface == Some(1) {
                    return Some(idx)
                }
            }
            None
        })
    }
    
    pub fn find_grab_bar_index(&self) -> Option<NodeIndex> {
        self.asset.specials.iter().find_map(|&idx| {
            if let Some(extras) = self.asset.node_extras.get(&idx) {
                if extras.is_grab_bar == Some(1) {
                    return Some(idx)
                }
            }
            None
        })
    }
}

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
struct ShaderMaterial {
    base_color_idx: i32,
    metallic_roughness_idx: i32,
    normal_map_idx: i32,
    _pad: i32,
}

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
struct PushConstants {
    model_transform: Mat4,
    base_color_factor: [f32; 4],
    material: ShaderMaterial,
    skin_joint_offset: i32,
    _pad: [i32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct Vertex {
    pub position: [f32; 3],
    pub uv: [f32; 2],
    pub normal: [f32; 3],
    pub tangent: [f32; 3],
    pub bitangent: [f32; 3],
    pub joint_indices: [u32; 4],
    pub joint_weights: [f32; 4],
}

pub struct Skin {
    pub joint_nodes: Vec<NodeIndex>,
    pub inverse_bind_matrices: Vec<Mat4>,
    pub skeleton_root: Option<NodeIndex>,
    pub joint_matrix_offset: u32,
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Copy, Clone)]
pub enum Interpolation { Linear, Step, CubicSpline }

pub struct AnimationChannel {
    pub target_node: NodeIndex,
    pub times: Vec<f32>,
    pub translations: Option<Vec<Vec3>>,
    pub rotations: Option<Vec<Quat>>,
    pub scales: Option<Vec<Vec3>>,
    pub interpolation: Interpolation,
}

pub struct GltfAnimation {
    pub name: String,
    pub channels: Vec<AnimationChannel>,
    pub duration: f32,
}

pub struct AnimationPlayer {
    pub clip_index: usize,
    pub time: f32,
    pub looping: bool,
}

impl AnimationPlayer {
    pub fn new(clip_index: usize, looping: bool) -> Self {
        Self { clip_index, time: 0.0, looping }
    }

    pub fn advance(&mut self, dt: f32, clip: &GltfAnimation) {
        if clip.duration <= 0.0 {
            return;
        }
        self.time += dt;
        if self.looping {
            self.time %= clip.duration;
        } else {
            self.time = self.time.min(clip.duration);
        }
    }
}

#[derive(Debug, Deserialize)]
pub struct Extras {
    #[serde(rename = "is_ui_surface")]
    pub is_ui_surface: Option<i32>,
    #[serde(rename = "is_spawnpoint")]
    pub is_spawnpoint: Option<i32>,
    #[serde(rename = "is_grab_bar")]
    pub is_grab_bar: Option<i32>,
}