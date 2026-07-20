struct VertexInput {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct XrCameraUbo
{
    column_major float4x4 view_matrices[2];
    column_major float4x4 projection_matrices[2];
};

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PushConstants {
    column_major float4x4 model_transform;
};

[[vk::binding(0, 0)]]
ConstantBuffer<XrCameraUbo> camera;

[[vk::binding(1, 0)]]
Sampler2D surface_texture;

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
    output.uv = input.uv;

    return output;
}

[shader("fragment")]
float4 fragment_main(VertexStageOutput input) : SV_Target {
    float4 color = surface_texture.Sample(input.uv);
    return color;
}