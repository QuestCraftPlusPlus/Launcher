use egui::{Button, Event, PointerButton, TextStyle};
use egui::WidgetText::Text;
use glam::Vec2;
use jni::Env;
use log::info;
use {
    crate::{
        egui::Egui,
        render::renderer::{DrawPayload, MSAA_COUNT},
        scene::gltf_model::{GltfAsset, GltfInstance, Vertex},
        stage::Stage,
        surface::SurfaceManager,
    },
    egui::{Frame, Pos2, RawInput, Rect},
    glam::{Mat4, Quat, Vec3},
    ndk::asset::AssetManager,
    std::sync::Arc,
    vk_graph::{
        Graph,
        driver::{
            ash::vk::{self, CullModeFlags, Format, PrimitiveTopology},
            buffer::Buffer,
            device::Device,
            graphics::{BlendInfo, GraphicsPipeline, GraphicsPipelineInfo},
            image::ImageInfo,
            shader::{SamplerInfoBuilder, Shader},
        },
        pool::{Pool, lazy::LazyPool},
    },
};
use crate::jni_state::JniContext;
use crate::keyboard::KeyDef::Char;

#[derive(Clone, Copy)]
pub enum KeyDef {
    Char(&'static str, &'static str), // lowercase, capital
    Tab,
    CapsLock,
    Backspace,
    Enter,
    Shift,
    Space,
    Modifier(&'static str),
}

pub struct Key {
    pub def: KeyDef,
    pub width: f32,
}
const fn key(def: KeyDef, width: f32) -> Key {
    Key { def, width }
}
const ROW_UNITS: f32 = 15.0;

pub struct KeyboardLayout { // ANSI
    pub name: &'static str,
    pub row1: &'static [Key],
    pub row2: &'static [Key],
    pub row3: &'static [Key],
    pub row4: &'static [Key],
    pub row5: &'static [Key],
}

impl KeyboardLayout {
    pub const QWERTY: KeyboardLayout = KeyboardLayout{
        name: "QWERTY",
        row1: &[
            key(Char("`","~"), 1.0), key(Char("1", "!"), 1.0), key(Char("2", "@"), 1.0), key(Char("3","#"), 1.0), key(Char("4", "$"), 1.0), key(Char("5", "%"), 1.0), key(Char("6", "^"), 1.0), key(Char("7",  "&"), 1.0), key(Char("8", "*"), 1.0), key(Char("9", "("), 1.0), key(Char("0", ")"), 1.0), key(Char("-", "_"), 1.0), key(Char("=", "+"), 1.0), key(KeyDef::Backspace, 2.0),
        ],
        row2: &[
            key(KeyDef::Tab, 1.5), key(Char("q", "Q"), 1.0), key(Char("w", "W"), 1.0), key(Char("e", "E"), 1.0), key(Char("r", "R"), 1.0), key(Char("t", "T"), 1.0), key(Char("y", "Y"), 1.0), key(Char("u", "U"), 1.0), key(Char("i", "I"), 1.0), key(Char("o", "O"), 1.0), key(Char("p", "P"), 1.0), key(Char("[", "{"), 1.0), key(Char("]", "}"), 1.0), key(Char("\\", "|"), 1.5),
        ],
        row3: &[
            key(KeyDef::CapsLock, 1.75), key(Char("a", "A"), 1.0), key(Char("s", "S"), 1.0), key(Char("d", "D"), 1.0), key(Char("f", "F"), 1.0), key(Char("g", "G"), 1.0), key(Char("h", "H"), 1.0), key(Char("j", "J"), 1.0), key(Char("k", "K"), 1.0), key(Char("l", "L"), 1.0), key(Char(";", ":"), 1.0), key(Char("'", "\""), 1.0), key(KeyDef::Enter, 2.25),
        ],
        row4: &[
            key(KeyDef::Shift, 2.25), key(Char("z", "Z"), 1.0), key(Char("x", "X"), 1.0), key(Char("c", "C"), 1.0), key(Char("v", "V"), 1.0), key(Char("b", "B"), 1.0), key(Char("n", "N"), 1.0), key(Char("m", "M"), 1.0), key(Char(",", "<"), 1.0), key(Char(".", ">"), 1.0), key(Char("/", "?"), 1.0), key(KeyDef::Shift, 2.75),
        ],
        row5: &[
            key(KeyDef::Modifier(""), 1.25), key(KeyDef::Modifier(""), 1.25), key(KeyDef::Modifier(""), 1.25), key(KeyDef::Space, 6.25), key(KeyDef::Modifier(""), 1.25), key(KeyDef::Modifier(""), 1.25), key(KeyDef::Modifier(""), 1.25), key(KeyDef::Modifier(""), 1.25),
        ],
    };
}

pub struct Keyboard {
    pool: LazyPool,
    mesh_asset: Arc<GltfAsset>,
    mesh_instance: GltfInstance,
    resolution: vk::Extent2D,
    jni_context: Arc<JniContext>,
    pending_input: RawInput,

    pub surface_manager: SurfaceManager,
    pub primitive_transform: Mat4,
    pub transform: Mat4,
    pub hidden: bool,
    pub layout: KeyboardLayout,
    pub is_shift: bool,
}

impl Keyboard {
    pub fn new(device: &Device, camera_buffers: &Vec<Arc<Buffer>>, asset_manager: &AssetManager, jni_context: Arc<JniContext>, resolution: vk::Extent2D) -> Self {
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
                                format: Format::R32G32B32_SFLOAT,
                                offset: 0,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 1,
                                binding: 0,
                                format: Format::R32G32_SFLOAT,
                                offset: 12,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 2,
                                binding: 0,
                                format: Format::R32G32B32_SFLOAT,
                                offset: 20,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 3,
                                binding: 0,
                                format: Format::R32G32B32_SFLOAT,
                                offset: 32,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 4,
                                binding: 0,
                                format: Format::R32G32B32_SFLOAT,
                                offset: 44,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 5,
                                binding: 0,
                                format: Format::R32G32B32A32_UINT,
                                offset: 56,
                            },
                            vk::VertexInputAttributeDescription {
                                location: 6,
                                binding: 0,
                                format: Format::R32G32B32A32_SFLOAT,
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
                .samples(MSAA_COUNT)
                .cull_mode(CullModeFlags::BACK), gltf_unlit_translucent_shaders.clone()).expect("Failed to create gltf_unlit_translucent pipeline")
        );
        let gltf_unlit_translucent_no_cull_pipeline = Arc::new(
            GraphicsPipeline::create(device, gltf_unlit_translucent_pipeline.info().into_builder()
                .cull_mode(CullModeFlags::NONE), gltf_unlit_translucent_shaders).expect("Failed to create gltf_unlit_translucent_no_cull")
        );
        let mesh_asset = Arc::new(GltfAsset::new("Keyboard".to_string(), asset_manager.open(c"meshes/keyboard.glb").unwrap(), device, gltf_unlit_translucent_pipeline, gltf_unlit_translucent_no_cull_pipeline));
        let mesh_instance = GltfInstance::new(mesh_asset.clone(), device, camera_buffers);
        let primitive_node = &mesh_instance.nodes[mesh_instance.find_surface_index().expect("Failed to find keyboard surface")];
        let primitive_transform = primitive_node.global_transform.clone();

        let surface_manager = SurfaceManager::new(asset_manager, device, mesh_asset.clone(), [primitive_node.mesh_index.expect("Keyboard surface does not have a mesh"), 0]);

        let resolution = vk::Extent2D {
            width: resolution.width,
            height: resolution.height,
        };
        info!("Created keyboard of size: {:?}", resolution);

        Self {
            pool: LazyPool::new(device),
            surface_manager,
            mesh_asset,
            mesh_instance,
            primitive_transform,
            resolution,
            jni_context,
            layout: KeyboardLayout::QWERTY,
            transform: Mat4::IDENTITY,
            hidden: true,
            is_shift: false,
            pending_input: RawInput::default(),
        }
    }

    pub fn adjust_position(&mut self, stage: &Stage, user_height: f32) {
        let ideal_user_head_pos = Vec3::new(0.,user_height,0.);

        let local_translation = Vec3::new(0.0, ideal_user_head_pos.y - 0.50, 0.80);
        let stage_translation = stage.position + local_translation;
        let keyboard_to_user = ideal_user_head_pos - local_translation;
        let tilt_angle = keyboard_to_user.y.atan2(-keyboard_to_user.z);
        let stage_rotation = stage.rotation * Quat::from_rotation_x(tilt_angle);

        self.transform = Mat4::from_rotation_translation(stage_rotation, stage_translation);
    }

    pub fn publish_inputs(&mut self, previous_click_state: bool, current_click_state: bool, uv: Vec2) {
        let pos = Pos2 {
            x: uv.x * self.resolution.width as f32,
            y: uv.y * self.resolution.height as f32,
        };

        self.pending_input.events.push(Event::PointerMoved(pos));

        if current_click_state && !previous_click_state {
            self.pending_input.events.push(Event::PointerButton {
                pos,
                button: PointerButton::Primary,
                pressed: true,
                modifiers: Default::default(),
            });
            
            self.pending_input.events.push(Event::PointerButton {
                pos,
                button: PointerButton::Primary,
                pressed: false,
                modifiers: Default::default(),
            });
        }
    }

    pub fn publish_pointer_leave(&mut self) {
        self.pending_input.events.push(Event::PointerGone);
    }

    fn keyboard_height(available_width: f32) -> f32 {
        let unit = available_width / ROW_UNITS;
        let gap = (unit * 0.08).clamp(2.0, 8.0);
        let key_height = unit * 0.9;
        let row_count = 5.0;
        row_count * key_height + (row_count - 1.0) * gap
    }

    pub fn record(&mut self, env: &mut Env<'_>, egui: &mut Egui, graph: &mut Graph, draw_payload: &DrawPayload) {
        if self.hidden { return }
        let target = graph.bind_resource(
            self.pool.resource(ImageInfo::image_2d(
                self.resolution.width,
                self.resolution.height,
                Format::R8G8B8A8_UNORM,
                vk::ImageUsageFlags::COLOR_ATTACHMENT | vk::ImageUsageFlags::SAMPLED
            ).into_builder()).unwrap().with_debug_name("keyboard render target")
        );

        let dims = Pos2{x: self.resolution.width as f32, y: self.resolution.height as f32};
        let mut input = std::mem::take(&mut self.pending_input);
        input.screen_rect = Some(Rect { min: Pos2{x: 0., y: 0.}, max: dims } );

        egui.run(input, target, graph, |ui| {
            let frame = Frame::NONE;
            egui::Window::new("Keyboard")
                .frame(frame)
                .resizable(false)
                .vscroll(true)
                .movable(false)
                .title_bar(false)
                .default_size([self.resolution.width as f32, self.resolution.height as f32])
                .default_pos([0., 0.])
                .show(ui, |ui| {
                    ui.style_mut().text_styles.get_mut(&TextStyle::Button).expect("missing button text style??").size = 36.0;
                    let rows = [self.layout.row1, self.layout.row2, self.layout.row3, self.layout.row4, self.layout.row5];

                    let unit = ui.available_width() / ROW_UNITS;
                    let gap = (unit * 0.08).clamp(2.0, 8.0);
                    let key_height = unit * 0.9;
                    let row_count = rows.len() as f32;
                    let content_height = row_count * key_height + (row_count - 1.0) * gap;

                    let top_pad = ((ui.available_height() - content_height) / 2.0).max(0.0);
                    ui.add_space(top_pad);
                    ui.spacing_mut().item_spacing = egui::vec2(gap, gap);

                    ui.vertical(|ui| {
                        for row in rows {
                            ui.horizontal(|ui| {
                                for k in row {
                                    let width = unit * k.width - gap;
                                    match k.def {
                                        Char(lower, upper) => {
                                            let label_text = if self.is_shift { upper } else { lower };
                                            let label = Text(label_text.to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label)).clicked() {
                                                self.jni_context.send_text(env, label_text);
                                            }
                                        }
                                        KeyDef::Tab => {
                                            let label = Text("Tab".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label)).clicked() {
                                                self.jni_context.send_text(env, "\t");
                                            }
                                        }
                                        KeyDef::CapsLock => {
                                            let label = Text("Caps".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label).selected(self.is_shift)).clicked() {
                                                self.is_shift = !self.is_shift;
                                            }
                                        }
                                        KeyDef::Backspace => {
                                            let label = Text("⌫".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label)).clicked() {
                                                self.jni_context.delete_character(env);
                                            }
                                        }
                                        KeyDef::Enter => {
                                            let label = Text("Enter".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label)).clicked() {
                                                self.jni_context.send_text(env, "\n");
                                            }
                                        }
                                        KeyDef::Shift => {
                                            let label = Text("⇧ Shift".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label).selected(self.is_shift)).clicked() {
                                                self.is_shift = !self.is_shift;
                                            }
                                        }
                                        KeyDef::Space => {
                                            let label = Text("Space".to_string()).text_style(TextStyle::Button);
                                            if ui.add_sized([width, key_height], Button::new(label)).clicked() {
                                                self.jni_context.send_text(env, " ");
                                            }
                                        }
                                        KeyDef::Modifier(label) => {
                                            if label.is_empty() {
                                                ui.add_space(width)
                                            } else {
                                                let label = Text(label.to_string()).text_style(TextStyle::Button);
                                                ui.add_enabled_ui(false, |ui| {
                                                    ui.add_sized([width, key_height], Button::new(label));
                                                });
                                            }
                                        }
                                    }
                                }
                            });
                        }
                    });
                });
        });

        self.mesh_instance.record_with_transform(graph, draw_payload, &self.transform);
        self.surface_manager.record_with_transform_bound(graph, target.into(), draw_payload, &(self.transform * self.primitive_transform));
    }
}