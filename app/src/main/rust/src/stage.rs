use glam::{Mat4, Quat, Vec3};

pub struct Stage {
    pub position: Vec3,
    pub rotation: Quat,
    pub movement_speed: f32,
}

impl Stage {
    pub fn new(position: Vec3, rotation: Quat, movement_speed: f32) -> Self {
        Self {
            position,
            rotation,
            movement_speed,
        }
    }

    pub fn move_relative(&mut self, movement: [f32; 3], head_rotation: Quat, delta_time: f32) {
        let speed = self.movement_speed * delta_time;
        let local_move = Vec3::new(movement[0], movement[1], movement[2]);
        
        let (yaw, _, _) = head_rotation.to_euler(glam::EulerRot::YXZ);
        let flat_head_rotation = Quat::from_rotation_y(yaw);

        let world_space_move = self.rotation * flat_head_rotation * local_move;
        self.position += world_space_move * speed;
    }

    pub fn world_to_stage_matrix(&self) -> Mat4 {
        Mat4::from_rotation_translation(self.rotation, self.position).inverse()
    }
}