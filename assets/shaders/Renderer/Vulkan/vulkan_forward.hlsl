struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float3 world_position : TEXCOORD1;
    float3 world_normal : TEXCOORD2;
    float4 shadow_position : TEXCOORD3;
};

struct PushConstants {
    float4x4 model;
    float4x4 normal_matrix;
};

[[vk::push_constant]]
PushConstants g_push_constants;

struct FrameGlobals {
    float4x4 view_projection;
    float4x4 shadow_light_view_projection;
    float4 camera_position_environment_intensity;
    float4 directional_direction_intensity;
    float4 directional_color_point_count;
    float4 ambient_color_intensity;
    float4 shadow_params; // x: enabled, y: strength, z: bias, w: shadow map size
};

struct PointLightData {
    float4 position_intensity;
    float4 color;
    float4 attenuation;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameGlobals> g_frame;

[[vk::binding(1, 0)]]
StructuredBuffer<PointLightData> g_point_lights;

[[vk::binding(2, 0)]]
Texture2D<float> g_shadow_map;

[[vk::binding(3, 0)]]
SamplerState g_shadow_sampler;

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
    output.position = mul(g_frame.view_projection, world_position);
    output.world_position = world_position.xyz;
    output.world_normal = normalize(mul((float3x3)g_push_constants.normal_matrix, input.normal));
    output.texcoord = input.texcoord;
    output.shadow_position = mul(g_frame.shadow_light_view_projection, world_position);
    return output;
}

static const float PI = 3.14159265359f;

float DistributionGGX(float3 normal, float3 half_vector, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, half_vector), 0.0f);
    float ndoth2 = ndoth * ndoth;

    float denominator = ndoth2 * (a2 - 1.0f) + 1.0f;
    denominator = PI * denominator * denominator;
    return a2 / max(denominator, 0.000001f);
}

float GeometrySchlickGGX(float ndotv, float roughness) {
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return ndotv / max(ndotv * (1.0f - k) + k, 0.000001f);
}

float GeometrySmith(float3 normal, float3 view_dir, float3 light_dir, float roughness) {
    float ndotv = max(dot(normal, view_dir), 0.0f);
    float ndotl = max(dot(normal, light_dir), 0.0f);
    return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

float3 FresnelSchlick(float cos_theta, float3 f0) {
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

float3 EvaluateDirectLight(
    float3 base_color,
    float metallic,
    float roughness,
    float3 normal,
    float3 view_dir,
    float3 light_dir,
    float3 radiance) {
    float3 half_vector = normalize(view_dir + light_dir);
    float ndotl = max(dot(normal, light_dir), 0.0f);
    if (ndotl <= 0.0f) {
        return 0.0f;
    }

    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metallic);
    float normal_distribution = DistributionGGX(normal, half_vector, roughness);
    float geometry = GeometrySmith(normal, view_dir, light_dir, roughness);
    float3 fresnel = FresnelSchlick(max(dot(half_vector, view_dir), 0.0f), f0);

    float3 numerator = normal_distribution * geometry * fresnel;
    float denominator = max(4.0f * max(dot(normal, view_dir), 0.0f) * ndotl, 0.000001f);
    float3 specular = numerator / denominator;

    float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * base_color / PI;
    return (diffuse + specular) * radiance * ndotl;
}

float EvaluateShadowVisibility(float4 shadow_position) {
    if (g_frame.shadow_params.x < 0.5f || shadow_position.w <= 0.0f) {
        return 1.0f;
    }

    float3 projected = shadow_position.xyz / shadow_position.w;
    float2 uv = projected.xy * 0.5f + 0.5f;
    float current_depth = projected.z;

    if (uv.x < 0.0f || uv.x > 1.0f ||
        uv.y < 0.0f || uv.y > 1.0f ||
        current_depth < 0.0f || current_depth > 1.0f) {
        return 1.0f;
    }

    const float bias = max(g_frame.shadow_params.z, 0.0f);
    const float strength = saturate(g_frame.shadow_params.y);
    const float texel_size = 1.0f / max(g_frame.shadow_params.w, 1.0f);

    float shadow = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            const float2 sample_uv = uv + float2((float)x, (float)y) * texel_size;
            const float closest_depth = g_shadow_map.SampleLevel(g_shadow_sampler, sample_uv, 0.0f);
            shadow += current_depth - bias > closest_depth ? 1.0f : 0.0f;
        }
    }

    shadow /= 9.0f;
    return lerp(1.0f, 1.0f - strength, shadow);
}

float4 PSMain(VSOutput input) : SV_Target0 {
    float4 base_color = g_base_color_texture.Sample(g_base_color_sampler, input.texcoord) * g_material.base_color_factor;
    if (g_material.factors.w > 0.5f && g_material.factors.w < 1.5f) {
        clip(base_color.a - g_material.factors.z);
    }

    float metallic = saturate(g_material.factors.x);
    float roughness = clamp(g_material.factors.y, 0.04f, 1.0f);
    float3 normal = normalize(input.world_normal);
    float3 view_dir = normalize(g_frame.camera_position_environment_intensity.xyz - input.world_position);

    float3 lit_color = g_frame.ambient_color_intensity.rgb * g_frame.ambient_color_intensity.w * base_color.rgb;

    float3 directional_light_dir = normalize(-g_frame.directional_direction_intensity.xyz);
    float3 directional_radiance = g_frame.directional_color_point_count.rgb * g_frame.directional_direction_intensity.w;
    const float directional_shadow = EvaluateShadowVisibility(input.shadow_position);
    lit_color += directional_shadow * EvaluateDirectLight(
        base_color.rgb,
        metallic,
        roughness,
        normal,
        view_dir,
        directional_light_dir,
        directional_radiance);

    uint point_light_count = (uint)g_frame.directional_color_point_count.w;
    [loop]
    for (uint index = 0; index < point_light_count; ++index) {
        PointLightData light = g_point_lights[index];
        float3 light_vector = light.position_intensity.xyz - input.world_position;
        float distance_to_light = length(light_vector);
        float3 point_light_dir = light_vector / max(distance_to_light, 0.0001f);
        float attenuation = 1.0f / max(
            light.attenuation.x +
            light.attenuation.y * distance_to_light +
            light.attenuation.z * distance_to_light * distance_to_light,
            0.0001f);
        float3 radiance = light.color.rgb * light.position_intensity.w * attenuation;

        lit_color += EvaluateDirectLight(
            base_color.rgb,
            metallic,
            roughness,
            normal,
            view_dir,
            point_light_dir,
            radiance);
    }

    return float4(lit_color, base_color.a);
}
