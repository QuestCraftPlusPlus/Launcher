use std::borrow::Cow;
use std::collections::HashMap;
use std::sync::Arc;
use bytemuck::cast_slice;
use ndk::asset::AssetManager;
use vk_graph::driver::ash::vk;
use vk_graph::driver::buffer::BufferInfo;
use vk_graph::driver::device::Device;
use vk_graph::driver::graphics::{BlendInfo, GraphicsPipeline, GraphicsPipelineInfo};
use vk_graph::driver::image::{Image, ImageInfo};
use vk_graph::driver::shader::Shader;
use vk_graph::driver::sync::AccessType;
use vk_graph::{Graph, LoadOp, StoreOp};
use vk_graph::driver::ash::vk::ShaderStageFlags;
use vk_graph::node::AnyImageNode;
use vk_graph::pool::hash::HashPool;
use vk_graph::pool::{Lease, Pool};

pub struct Egui {
    pub ctx: egui::Context,

    textures: HashMap<egui::TextureId, Arc<Lease<Image>>>,
    cache: HashPool,
    ppl: GraphicsPipeline,
    next_tex_id: u64,
    user_textures: HashMap<egui::TextureId, AnyImageNode>
}

impl Egui {
    pub fn new(device: &Device, asset_manager: &AssetManager) -> Self {
        let ppl = {
            let mut asset = asset_manager.open(c"shaders/egui.spv").expect("Failed to load 'egui' shader");
            let spv_bytes = asset.buffer().unwrap();

            GraphicsPipeline::create(
                device,
                GraphicsPipelineInfo::builder()
                    .blend(BlendInfo {
                        blend_enable: true,
                        src_color_blend_factor: vk::BlendFactor::ONE,
                        dst_color_blend_factor: vk::BlendFactor::ONE_MINUS_SRC_ALPHA,
                        color_blend_op: vk::BlendOp::ADD,
                        src_alpha_blend_factor: vk::BlendFactor::ONE,
                        dst_alpha_blend_factor: vk::BlendFactor::ONE,
                        alpha_blend_op: vk::BlendOp::ADD,
                        color_write_mask: vk::ColorComponentFlags::R
                            | vk::ColorComponentFlags::G
                            | vk::ColorComponentFlags::B
                            | vk::ColorComponentFlags::A,
                    })
                    .cull_mode(vk::CullModeFlags::NONE),
                [
                    Shader::builder()
                        .entry_name("vertex_main")
                        .stage(ShaderStageFlags::VERTEX)
                        .vertex_input(
                            [
                                vk::VertexInputBindingDescription {
                                    binding: 0,
                                    stride: size_of::<egui::epaint::Vertex>() as u32,
                                    input_rate: vk::VertexInputRate::VERTEX,
                                }
                            ], [
                                vk::VertexInputAttributeDescription {
                                    location: 0,
                                    binding: 0,
                                    format: vk::Format::R32G32_SFLOAT,
                                    offset: 0,
                                },
                                vk::VertexInputAttributeDescription {
                                    location: 1,
                                    binding: 0,
                                    format: vk::Format::R32G32_SFLOAT,
                                    offset: 8,
                                },
                                vk::VertexInputAttributeDescription {
                                    location: 2,
                                    binding: 0,
                                    format: vk::Format::R32_UINT,
                                    offset: 16,
                                }
                            ]
                        )
                        .spirv(spv_bytes),
                    Shader::builder()
                        .entry_name("fragment_main")
                        .stage(ShaderStageFlags::FRAGMENT)
                        .spirv(spv_bytes)
                ]
            ).expect("Failed to create egui pipeline")
        };

        let ctx = egui::Context::default();

        Self {
            ppl,
            ctx,
            textures: HashMap::default(),
            cache: HashPool::new(device),
            next_tex_id: 0,
            user_textures: HashMap::default()
        }
    }

    fn bind_and_update_textures(
        &mut self,
        deltas: &egui::TexturesDelta,
        graph: &mut Graph,
    ) -> HashMap<egui::TextureId, AnyImageNode> {
        let mut bound_tex = deltas
            .set
            .iter()
            .map(|(id, delta)| {
                let pixels = match &delta.image {
                    egui::ImageData::Color(image) => {
                        assert_eq!(image.width() * image.height(), image.pixels.len());
                        Cow::Borrowed(&image.pixels)
                    }
                };

                let tmp_buf = {
                    let mut buf = self
                        .cache
                        .resource(BufferInfo::host_mem(
                            (pixels.len() * delta.image.bytes_per_pixel()) as u64,
                            vk::BufferUsageFlags::TRANSFER_SRC,
                        ))
                        .expect("Missing egui texture upload buffer");
                    buf.copy_from_slice(0, cast_slice(&pixels));
                    graph.bind_resource(buf)
                };

                if let Some(pos) = delta.pos {
                    let image =
                        graph.bind_resource(self.textures.remove(id).expect("missing texture"));

                    graph
                        .begin_cmd()
                        .debug_name("copy buffer to image")
                        .copy_buffer_to_image(
                            tmp_buf,
                            image,
                            [vk::BufferImageCopy {
                                buffer_offset: 0,
                                buffer_row_length: delta.image.width() as u32,
                                buffer_image_height: delta.image.height() as u32,
                                image_offset: vk::Offset3D {
                                    x: pos[0] as i32,
                                    y: pos[1] as i32,
                                    z: 0,
                                },
                                image_extent: vk::Extent3D {
                                    width: delta.image.width() as u32,
                                    height: delta.image.height() as u32,
                                    depth: 1,
                                },
                                image_subresource: vk::ImageSubresourceLayers {
                                    aspect_mask: vk::ImageAspectFlags::COLOR,
                                    mip_level: 0,
                                    base_array_layer: 0,
                                    layer_count: 1,
                                },
                            }],
                        )
                        .end_cmd();
                    (*id, AnyImageNode::from(image))
                } else {
                    let image = graph.bind_resource(
                        self.cache
                            .resource(ImageInfo::image_2d(
                                delta.image.width() as u32,
                                delta.image.height() as u32,
                                vk::Format::R8G8B8A8_UNORM,
                                vk::ImageUsageFlags::SAMPLED | vk::ImageUsageFlags::TRANSFER_DST,
                            ))
                            .expect("Missing egui texture image"),
                    );

                    graph.copy_buffer_to_image(tmp_buf, image);
                    (*id, AnyImageNode::from(image))
                }
            })
            .collect::<HashMap<_, _>>();

        for (id, image) in self.textures.drain() {
            bound_tex.insert(id, AnyImageNode::from(graph.bind_resource(image)));
        }

        for (id, node) in self.user_textures.drain() {
            bound_tex.insert(id, node);
        }

        bound_tex
    }

    fn unbind_and_free(
        &mut self,
        bound_tex: HashMap<egui::TextureId, AnyImageNode>,
        graph: &mut Graph,
        deltas: &egui::TexturesDelta,
    ) {
        for (id, tex) in bound_tex.iter() {
            if let AnyImageNode::Pooled(tex) = tex
                && let egui::TextureId::Managed(_) = *id
            {
                self.textures.insert(*id, graph.resource(*tex).clone());
            }
        }

        for id in deltas.free.iter() {
            self.textures.remove(id);
        }

        self.next_tex_id = 0;
    }

    fn draw_primitive(
        &mut self,
        shapes: Vec<egui::epaint::ClippedShape>,
        bound_tex: &HashMap<egui::TextureId, AnyImageNode>,
        graph: &mut Graph,
        target: impl Into<AnyImageNode>,
    ) {
        let target = target.into();
        let target_info = graph.resource(target).info;
        for egui::ClippedPrimitive {
            clip_rect,
            primitive,
        } in self.ctx.tessellate(shapes, self.ctx.pixels_per_point())
        {
            match primitive {
                egui::epaint::Primitive::Mesh(mesh) => {
                    if mesh.vertices.is_empty() || mesh.indices.is_empty() {
                        continue;
                    }
                    let texture = bound_tex
                        .get(&mesh.texture_id)
                        .expect("missing egui mesh texture");

                    let idx_buf = {
                        let mut buf = self
                            .cache
                            .resource(BufferInfo::host_mem(
                                (mesh.indices.len() * 4) as u64,
                                vk::BufferUsageFlags::INDEX_BUFFER,
                            ))
                            .expect("missing egui index buffer");
                        buf.copy_from_slice(0, cast_slice(&mesh.indices));
                        buf
                    };
                    let idx_buf = graph.bind_resource(idx_buf);

                    let vert_buf = {
                        let mut buf = self
                            .cache
                            .resource(BufferInfo::host_mem(
                                (mesh.vertices.len() * size_of::<egui::epaint::Vertex>())
                                    as u64,
                                vk::BufferUsageFlags::VERTEX_BUFFER,
                            ))
                            .expect("missing egui vertex buffer");
                        buf.copy_from_slice(0, cast_slice(&mesh.vertices));
                        buf
                    };
                    let vert_buf = graph.bind_resource(vert_buf);

                    #[repr(C)]
                    #[derive(Copy, Clone, bytemuck::Pod, bytemuck::Zeroable)]
                    struct PushConstants {
                        screen_size: [f32; 2],
                    }

                    let pixels_per_point = self.ctx.pixels_per_point();

                    let push_constants = PushConstants {
                        screen_size: [
                            target_info.width as f32 / pixels_per_point,
                            target_info.height as f32 / pixels_per_point,
                        ],
                    };

                    let num_indices = mesh.indices.len() as u32;

                    let x = (clip_rect.min.x * pixels_per_point) as i32;
                    let y = (clip_rect.min.y * pixels_per_point) as i32;

                    let width = ((clip_rect.max.x - clip_rect.min.x) * pixels_per_point) as u32;
                    let height = ((clip_rect.max.y - clip_rect.min.y) * pixels_per_point) as u32;

                    graph
                        .begin_cmd()
                        .debug_name("Egui pass")
                        .bind_pipeline(&self.ppl)
                        .resource_access(idx_buf, AccessType::IndexBuffer)
                        .resource_access(vert_buf, AccessType::VertexBuffer)
                        .shader_resource_access(0, *texture, AccessType::FragmentShaderReadOther)
                        .color_attachment_image(0, target, LoadOp::Load, StoreOp::Store)
                        .record_cmd(move |cmd| {
                            cmd.bind_index_buffer(idx_buf, 0, vk::IndexType::UINT32)
                                .bind_vertex_buffer(0, vert_buf, 0)
                                .push_constants(0, cast_slice(&[push_constants]))
                                .set_scissor(
                                    0,
                                    &[vk::Rect2D {
                                        offset: vk::Offset2D { x, y },
                                        extent: vk::Extent2D { width, height },
                                    }],
                                )
                                .draw_indexed(num_indices, 1, 0, 0, 0);
                        });
                }
                _ => panic!("Primitive callback not yet supported."),
            }
        }
    }

    pub fn run(
        &mut self,
        raw_input: egui::RawInput,
        target: impl Into<AnyImageNode>,
        graph: &mut Graph,
        ui_fn: impl FnMut(&mut egui::Ui),
    ) -> egui::FullOutput {
        let full_output = self.ctx.run_ui(raw_input, ui_fn);

        let deltas = &full_output.textures_delta;

        let bound_tex = self.bind_and_update_textures(deltas, graph);

        self.draw_primitive(full_output.shapes.clone(), &bound_tex, graph, target);

        self.unbind_and_free(bound_tex, graph, deltas);

        full_output
    }

    pub fn register_texture(&mut self, tex: impl Into<AnyImageNode>) -> egui::TextureId {
        let id = egui::TextureId::User(self.next_tex_id);
        self.next_tex_id += 1;
        self.user_textures.insert(id, tex.into());
        id
    }
}