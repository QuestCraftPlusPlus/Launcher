struct VertexInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    uint4 joint_indices : JOINT_INDICES;
    float4 joint_weights : JOINT_WEIGHTS;
};

struct XrCameraUbo
{
    column_major float4x4 view_matrices[2];
    column_major float4x4 projection_matrices[2];
};

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float3 world_pos : POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation int material_base_color_idx : BLENDINDICES0;
    nointerpolation int material_mr_idx : BLENDINDICES1;
    nointerpolation int material_normal_idx : BLENDINDICES2;
    float4 base_color_factor : COLOR0;
};

struct PushConstants {
    column_major float4x4 model_transform;
    float4 base_color_factor;
    int4 material;
    int4 skin_data;
};

[[vk::binding(0, 0)]]
ConstantBuffer<XrCameraUbo> camera;

[[vk::binding(1, 0)]]
StructuredBuffer<PushConstants, Std430DataLayout> draw_data;

[[vk::binding(2, 0)]]
StructuredBuffer<float4x4, Std430DataLayout> joint_matrices;

[[vk::binding(3, 0)]]
Sampler2D texture_heap[8192];

[shader("vertex")]
VertexStageOutput vertex_main(
    uniform column_major float4x4 transform,
    VertexInput input,
    uint view_id: SV_ViewID,
    uint instance_id: SV_VulkanInstanceID
)
{
    VertexStageOutput output;
    PushConstants pc = draw_data[instance_id];

    float4 local_pos = float4(input.position, 1.0);

    if (pc.skin_data.x >= 0) {
        int offset = pc.skin_data.x;

        float4x4 skin_matrix =
            input.joint_weights.x * joint_matrices[offset + input.joint_indices.x] +
            input.joint_weights.y * joint_matrices[offset + input.joint_indices.y] +
            input.joint_weights.z * joint_matrices[offset + input.joint_indices.z] +
            input.joint_weights.w * joint_matrices[offset + input.joint_indices.w];

        local_pos = mul(skin_matrix, local_pos);
    }

    float4x4 final_transform = mul(transform, pc.model_transform);
    float4 world_pos = mul(final_transform, local_pos);
    float4 view_pos = mul(camera.view_matrices[view_id], world_pos);

    output.sv_position = mul(camera.projection_matrices[view_id], view_pos);
    output.world_pos = world_pos.xyz;
    output.uv = input.uv;

    output.material_base_color_idx = pc.material.x;
    output.material_mr_idx = pc.material.y;
    output.material_normal_idx = pc.material.z;
    output.base_color_factor = pc.base_color_factor;

    return output;
}

[shader("fragment")]
float4 fragment_main(VertexStageOutput input) : SV_Target {
    float4 albedo = float4(1.0);
    if (input.material_base_color_idx >= 0) {
        float2 dx = ddx(input.uv);
        float2 dy = ddy(input.uv);

        float2 rgss_offsets[4] = {
            float2(-0.125, -0.375),
            float2( 0.375, -0.125),
            float2(-0.375,  0.125),
            float2( 0.125,  0.375)
        };

        float4 accumulated_color = float4(0.0);
        float accumulated_alpha = 0.0;

        [unroll]
        for (int i = 0; i < 4; i++) {
            float2 uv_offset = dx * rgss_offsets[i].x + dy * rgss_offsets[i].y;
            float4 sample_val = texture_heap[NonUniformResourceIndex(input.material_base_color_idx)].SampleGrad(input.uv + uv_offset, dx, dy);

            accumulated_color.xyz += sample_val.xyz * sample_val.a;
            accumulated_color.a += sample_val.a;
        }

        float4 tex_albedo;
        if (accumulated_color.a > 0.0) {
            tex_albedo.xyz = accumulated_color.xyz / accumulated_color.a;
            tex_albedo.a = accumulated_color.a * 0.25;
        } else {
            tex_albedo = float4(0.0);
        }

        albedo *= tex_albedo;
    }

    float3 final_color = albedo.xyz;

    return float4(final_color, albedo.a);
}