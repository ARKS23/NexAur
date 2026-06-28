struct FullscreenVSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

FullscreenVSOutput FullscreenTriangleVS(uint vertex_id) {
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    float2 uvs[3] = {
        float2(0.0f, 0.0f),
        float2(0.0f, 2.0f),
        float2(2.0f, 0.0f)
    };

    FullscreenVSOutput output;
    output.position = float4(positions[vertex_id], 0.0f, 1.0f);
    output.uv = uvs[vertex_id];
    return output;
}
