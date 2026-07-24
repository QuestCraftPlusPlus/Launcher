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
    float4 base_color_factor;
    int4 material;
    // int base_color_idx;
    // int metallic_roughness_idx;
    // int normal_map_idx;
    // int pad0;
};

[[vk::binding(0, 0)]]
ConstantBuffer<XrCameraUbo> camera;

[[vk::binding(1, 0)]]
StructuredBuffer<PushConstants, Std430DataLayout> draw_data;

[[vk::binding(2, 0)]]
Sampler2D texture_heap[8192];


// todo: unhardcode these, also probably make them unnecessary or disableable or something
static const float3 LIGHT_DIR = normalize(float3(0.5, 1.0, -0.5));
static const float3 LIGHT_COLOR = float3(1.0, 1.0, 0.95);
static const float3 AMBIENT_COLOR = float3(0.03, 0.03, 0.04);
static const float PI = 3.14159265359;

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

    float4x4 final_transform = mul(transform, pc.model_transform);
    float4 world_pos = mul(final_transform, float4(input.position, 1.0));
    float4 view_pos = mul(camera.view_matrices[view_id], world_pos);

    output.sv_position = mul(camera.projection_matrices[view_id], view_pos);
    output.world_pos = world_pos.xyz;
    output.uv = input.uv;

    float3x3 normal_matrix = (float3x3)final_transform;
    output.world_normal = normalize(mul(normal_matrix, input.normal));
    output.world_tangent = normalize(mul(normal_matrix, input.tangent));
    output.world_bitangent = normalize(mul(normal_matrix, input.bitangent));

    output.material_base_color_idx = pc.material.x;
    output.material_mr_idx = pc.material.y;
    output.material_normal_idx = pc.material.z;
    output.base_color_factor = pc.base_color_factor;

    return output;
}

float3 EvaluateSpecularBRDF(float3 N, float3 V, float3 L, float roughness, float3 F0, out float3 F) {
    float3 H = normalize(V + L);
    float NoV = max(dot(N, V), 0.001);
    float NoL = max(dot(N, L), 0.001);
    float NoH = max(dot(N, H), 0.0);
    float LoH = max(dot(L, H), 0.0);

    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NoH * NoH * (a2 - 1.0) + 1.0);
    float D = a2 / (PI * denom * denom);

    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    float G = 0.5 / (GGXV + GGXL);

    F = F0 + (1.0 - F0) * pow(1.0 - LoH, 5.0);

    return D * G * F;
}

[shader("fragment")]
float4 fragment_main(VertexStageOutput input) : SV_Target {
    float4 albedo = input.base_color_factor;
    if (input.material_base_color_idx >= 0) {
        float4 tex_albedo = texture_heap[NonUniformResourceIndex(input.material_base_color_idx)].Sample(input.uv);
        clip(tex_albedo.a - 0.2);
        albedo *= tex_albedo;
    }

    float roughness = 1.0;
    float metalness = 1.0;
    if (input.material_mr_idx >= 0) {
        float4 mr_sample = texture_heap[NonUniformResourceIndex(input.material_mr_idx)].Sample(input.uv);
        roughness = mr_sample.g;
        metalness = mr_sample.b;
    }

    float3 N = normalize(input.world_normal);
    if (input.material_normal_idx >= 0) {
        float3 T = normalize(input.world_tangent);
        float3 B = normalize(input.world_bitangent);
        float3x3 TBN = float3x3(T, B, N);

        float3 normal_sample = texture_heap[NonUniformResourceIndex(input.material_normal_idx)].Sample(input.uv).xyz * 2.0 - 1.0;
        N = normalize(mul(normal_sample, TBN));
    }

    float3 V = normalize(-input.world_pos);
    float3 L = LIGHT_DIR;

    float3 F0 = float3(0.04);
    F0 = lerp(F0, albedo.xyz, metalness);

    float3 F;
    float3 specular = EvaluateSpecularBRDF(N, V, L, roughness, F0, F);
    float3 kd = (1.0 - F) * (1.0 - metalness);
    float3 diffuse = kd * albedo.xyz / PI;

    float NoL = max(dot(N, L), 0.0);
    float3 direct_lighting = (diffuse + specular) * LIGHT_COLOR * NoL;
    float3 ambient_lighting = albedo.xyz * AMBIENT_COLOR;

    float3 final_color = direct_lighting + ambient_lighting;

    final_color = final_color / (final_color + float3(1.0));
    final_color = pow(final_color, float3(1.0 / 2.2));

    return float4(final_color, albedo.a);
}