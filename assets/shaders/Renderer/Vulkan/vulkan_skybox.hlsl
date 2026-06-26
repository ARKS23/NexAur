struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct SkyboxPushConstants {
    float4 background_color_intensity;
};

[[vk::push_constant]]
SkyboxPushConstants g_skybox;

VSOutput VSMain(uint vertex_id : SV_VertexID) {
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    VSOutput output;
    const float2 position = positions[vertex_id];
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * 0.5f + 0.5f;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    const float vertical = saturate(input.uv.y);
    const float gradient = lerp(0.75f, 1.35f, smoothstep(0.0f, 1.0f, vertical));
    const float intensity = max(g_skybox.background_color_intensity.w, 0.0f);
    const float3 color = g_skybox.background_color_intensity.rgb * gradient * intensity;
    return float4(color, 1.0f);
}
