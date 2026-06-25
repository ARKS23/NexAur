struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL0;
};

struct PushConstants {
    float4x4 view_projection;
    float4x4 model;
};

[[vk::push_constant]]
PushConstants g_push_constants;

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    output.position = mul(g_push_constants.view_projection, world_position);
    output.normal = normalize(mul((float3x3)g_push_constants.model, input.normal));
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float3 normal_color = normalize(input.normal) * 0.5f + 0.5f;
    return float4(lerp(float3(0.35f, 0.62f, 0.95f), normal_color, 0.35f), 1.0f);
}
