use {
    glam::Vec3,
    openxr as xr,
};

pub fn pose_to_matrix(pose: &xr::Posef) -> glam::Mat4 {
    let pos: Vec3 = <mint::Vector3<f32>>::from(pose.position).into();
    let rot = <mint::Quaternion<f32> as Into<glam::Quat>>::into(pose.orientation.into());
    glam::Mat4::from_rotation_translation(rot, pos) 
}