struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct PushConstants {
    float4x4 light_view_projection;
    float4x4 model;
};

[[vk::push_constant]]
PushConstants g_push_constants;

float4 VSMain(VSInput input) : SV_Position {
    const float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    return mul(g_push_constants.light_view_projection, world_position);
}
