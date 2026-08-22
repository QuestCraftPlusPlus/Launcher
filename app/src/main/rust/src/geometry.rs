use glam::{Vec2, Vec3};
use crate::scene::gltf_model::Vertex;

#[derive(Debug, PartialEq, Copy, Clone)]
pub struct RaycastHit {
    pub uv: Vec2,
    pub position: Vec3,
    pub normal: Vec3,
    pub distance: f32,
}

impl RaycastHit {
    pub fn closest_hit(hits: &[Option<RaycastHit>]) -> Option<(&RaycastHit, usize)> {
        let hit = hits.iter()
            .filter_map(|hit| hit.as_ref())
            .min_by(|a,b| a.distance.total_cmp(&b.distance));
        if hit == None {
            return None
        }
        let index = hits.iter().position(|hit| hit.is_some()).unwrap();
        Some((hit.unwrap(), index))
    }
}

// this uses the Moller-Trumbore intersection algorithm
// all inputs should be in the same space, the outputs will be in the same space
pub fn raycast_uv(vertices: &Vec<Vertex>, indices: &Vec<u32>, ray_origin: Vec3, ray_direction: Vec3) -> Option<RaycastHit> {
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
        Some(RaycastHit {
            uv: closest_uv,
            position: closest_local_pos,
            normal: closest_local_normal,
            distance: closest_t,
        })
    } else {
        None
    }
}