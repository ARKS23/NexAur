[[vk::binding(0, 0)]]
Texture2D<float4> g_hdr_scene_color;

[[vk::binding(1, 0)]]
SamplerState g_scene_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    VSOutput output;

    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    float2 uvs[3] = {
        float2(0.0f, 0.0f),
        float2(0.0f, 2.0f),
        float2(2.0f, 0.0f)
    };

    output.position = float4(positions[vertex_id], 0.0f, 1.0f);
    output.uv = uvs[vertex_id];
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float4 hdr_color = g_hdr_scene_color.SampleLevel(g_scene_sampler, input.uv, 0.0f);
    return float4(saturate(hdr_color.rgb), saturate(hdr_color.a));
}
