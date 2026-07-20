struct VertexInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
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
    float3 world_normal : NORMAL;
    float3 world_tangent : TANGENT;
    float3 world_bitangent : BITANGENT;
    nointerpolation int material_base_color_idx : BLENDINDICES0;
    nointerpolation int material_mr_idx : BLENDINDICES1;
    nointerpolation int material_normal_idx : BLENDINDICES2;
    float4 base_color_factor : COLOR0;
};

struct PushConstants {
    column_major float4x4 model_transform;
    int base_color_idx;
    int metallic_roughness_idx;
    int normal_map_idx;
    float4 base_color_factor;
};

[[vk::binding(0, 0)]]
ConstantBuffer<XrCameraUbo> camera;

[[vk::binding(1, 0)]]
Sampler2D texture_heap[8192];

[shader("vertex")]
VertexStageOutput vertex_main(
    uniform PushConstants pc,
    VertexInput input,
    uint viewID : SV_ViewID
)
{
    VertexStageOutput output;

    float4 world_pos = mul(pc.model_transform, float4(input.position, 1.0));
    float4 view_pos = mul(camera.view_matrices[viewID], world_pos);

    output.sv_position = mul(camera.projection_matrices[viewID], view_pos);
    output.world_pos = world_pos.xyz;
    output.uv = input.uv;

    float3x3 normal_matrix = (float3x3)pc.model_transform;
    output.world_normal = normalize(mul(normal_matrix, input.normal));
    output.world_tangent = normalize(mul(normal_matrix, input.tangent));
    output.world_bitangent = normalize(mul(normal_matrix, input.bitangent));

    output.material_base_color_idx = pc.base_color_idx;
    output.material_mr_idx = pc.metallic_roughness_idx;
    output.material_normal_idx = pc.normal_map_idx;
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
            float4 sample_val = texture_heap[input.material_base_color_idx].Sample(input.uv + uv_offset);

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