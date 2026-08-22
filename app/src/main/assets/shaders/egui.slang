struct VertexInput {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    uint color : COLOR;
}

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
}

struct PushConstants {
    float2 screen_size;
}

[[vk::binding(0, 0)]]
Sampler2D font_sampler_lle;

float4 linear_from_srgba(float4 srgba) {
    bool4 cutoff = srgba < 0.04045;
    float4 higher = pow((srgba + 0.055) / 1.055, 2.4);
    float4 lower = srgba / 12.92;

    return lerp(higher, lower, float4(cutoff));
}

float4 srgba_from_linear(float4 linear) {
    bool4 cutoff = linear < 0.0031308;
    float4 higher = 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
    float4 lower = linear * 12.92;

    return lerp(higher, lower, float4(cutoff));
}

[shader("vertex")]
VertexOutput vertex_main(
    uniform PushConstants pc,
    VertexInput input
) {
    VertexOutput output;
    float2 pos = (input.pos / pc.screen_size) * 2.0 - 1.0;
    output.position = float4(pos, 0.0, 1.0);

    output.color = linear_from_srgba(unpackUnorm4x8ToFloat(input.color));
    output.uv = input.uv;

    return output;
}

[shader("fragment")]
float4 fragment_main(VertexOutput input) {
    return srgba_from_linear(input.color * font_sampler_lle.Sample(input.uv));
}