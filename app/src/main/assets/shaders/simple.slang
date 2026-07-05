

[shader("vertex")]
float4 vertex_main(uint vertex_id: SV_VertexID) : SV_Position {
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    return float4(uv * 2.0 - 1.0, 0.5, 1.0);
}

[shader("fragment")]
float4 fragment_main(float4 pos : SV_Position) : SV_Target {
    return float4(0.0, 1.0, 0.0, 1.0);
}