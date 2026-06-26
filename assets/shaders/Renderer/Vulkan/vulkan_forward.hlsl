struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    float2 texcoord : TEXCOORD0;
};

struct PushConstants {
    float4x4 view_projection;
    float4x4 model;
};

[[vk::push_constant]]
PushConstants g_push_constants;

struct MaterialConstants {
    float4 base_color_factor;
    float4 factors; // x: metallic, y: roughness, z: alpha cutoff, w: alpha mode
};

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialConstants> g_material;

[[vk::binding(1, 1)]]
Texture2D<float4> g_base_color_texture;

[[vk::binding(2, 1)]]
SamplerState g_base_color_sampler;

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    output.position = mul(g_push_constants.view_projection, world_position);
    output.normal = normalize(mul((float3x3)g_push_constants.model, input.normal));
    output.texcoord = input.texcoord;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float4 base_color = g_base_color_texture.Sample(g_base_color_sampler, input.texcoord) * g_material.base_color_factor;
    if (g_material.factors.w > 0.5f && g_material.factors.w < 1.5f) {
        clip(base_color.a - g_material.factors.z);
    }

    return base_color;
}
