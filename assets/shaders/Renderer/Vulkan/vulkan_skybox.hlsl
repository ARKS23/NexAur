struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 clip_xy : TEXCOORD1;
};

struct SkyboxPushConstants {
    float4x4 inverse_view_projection;
    float4 background_color_intensity;
    float4 camera_position_use_environment;
};

[[vk::push_constant]]
SkyboxPushConstants g_skybox;

[[vk::binding(0, 0)]]
TextureCube<float4> g_environment_map;

[[vk::binding(4, 0)]]
SamplerState g_environment_sampler;

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
    output.clip_xy = position;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    const float intensity = max(g_skybox.background_color_intensity.w, 0.0f);
    if (g_skybox.camera_position_use_environment.w > 0.5f) {
        const float4 far_position = mul(
            g_skybox.inverse_view_projection,
            float4(input.clip_xy, 1.0f, 1.0f));
        const float3 world_position = far_position.xyz / max(abs(far_position.w), 0.000001f);
        const float3 direction = normalize(world_position - g_skybox.camera_position_use_environment.xyz);
        const float3 color = max(g_environment_map.SampleLevel(g_environment_sampler, direction, 0.0f).rgb, 0.0f) * intensity;
        return float4(color, 1.0f);
    }

    const float vertical = saturate(input.uv.y);
    const float gradient = lerp(0.75f, 1.35f, smoothstep(0.0f, 1.0f, vertical));
    const float3 color = g_skybox.background_color_intensity.rgb * gradient * intensity;
    return float4(color, 1.0f);
}
