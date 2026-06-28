struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float3 world_position : TEXCOORD1;
    float3 world_normal : TEXCOORD2;
    float4 shadow_position : TEXCOORD3;
    float3 world_tangent : TEXCOORD4;
    float3 world_bitangent : TEXCOORD5;
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
    float4 emissive_factor_normal_scale; // xyz: emissive factor, w: normal scale
    float4 factors; // x: metallic, y: roughness, z: alpha cutoff, w: alpha mode
    float4 texture_flags; // x: base color, y: normal, z: metallic, w: roughness
    float4 texture_flags2; // x: packed MR, y: AO, z: emissive, w: AO strength
};

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialConstants> g_material;

[[vk::binding(1, 1)]]
Texture2D<float4> g_base_color_texture;

[[vk::binding(2, 1)]]
Texture2D<float4> g_normal_texture;

[[vk::binding(3, 1)]]
Texture2D<float4> g_metallic_texture;

[[vk::binding(4, 1)]]
Texture2D<float4> g_roughness_texture;

[[vk::binding(5, 1)]]
Texture2D<float4> g_metallic_roughness_texture;

[[vk::binding(6, 1)]]
Texture2D<float4> g_ao_texture;

[[vk::binding(7, 1)]]
Texture2D<float4> g_emissive_texture;

[[vk::binding(8, 1)]]
SamplerState g_material_sampler;

[[vk::binding(0, 2)]]
TextureCube<float4> g_environment_map;

[[vk::binding(1, 2)]]
TextureCube<float4> g_irradiance_map;

[[vk::binding(2, 2)]]
TextureCube<float4> g_prefiltered_environment_map;

[[vk::binding(3, 2)]]
Texture2D<float4> g_brdf_lut;

[[vk::binding(4, 2)]]
SamplerState g_environment_sampler;

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    float3x3 normal_matrix = (float3x3)g_push_constants.normal_matrix;
    output.position = mul(g_frame.view_projection, world_position);
    output.world_position = world_position.xyz;
    output.world_normal = normalize(mul(normal_matrix, input.normal));
    output.world_tangent = mul(normal_matrix, input.tangent);
    output.world_bitangent = mul(normal_matrix, input.bitangent);
    output.texcoord = input.texcoord;
    output.shadow_position = mul(g_frame.shadow_light_view_projection, world_position);
    return output;
}

static const float PI = 3.14159265359f;
static const float MAX_REFLECTION_LOD = 7.0f;

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

float3 FresnelSchlickRoughness(float cos_theta, float3 f0, float roughness) {
    const float3 grazing = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0);
    return f0 + (grazing - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

bool HasTexture(float flag) {
    return flag > 0.5f;
}

float3 FallbackTangent(float3 normal) {
    float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(up, normal));
}

float3 ResolveNormal(VSOutput input) {
    float3 normal = normalize(input.world_normal);
    if (!HasTexture(g_material.texture_flags.y)) {
        return normal;
    }

    float3 tangent = input.world_tangent;
    if (dot(tangent, tangent) <= 0.000001f) {
        tangent = FallbackTangent(normal);
    } else {
        tangent = normalize(tangent - normal * dot(normal, tangent));
    }

    float3 bitangent = input.world_bitangent;
    if (dot(bitangent, bitangent) <= 0.000001f) {
        bitangent = normalize(cross(normal, tangent));
    } else {
        bitangent = normalize(bitangent - normal * dot(normal, bitangent));
    }

    float3 tangent_normal = g_normal_texture.Sample(g_material_sampler, input.texcoord).xyz * 2.0f - 1.0f;
    tangent_normal.xy *= max(g_material.emissive_factor_normal_scale.w, 0.0f);
    return normalize(
        tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        normal * tangent_normal.z);
}

float4 ResolveBaseColor(float2 texcoord) {
    float4 base_color = g_material.base_color_factor;
    if (HasTexture(g_material.texture_flags.x)) {
        base_color *= g_base_color_texture.Sample(g_material_sampler, texcoord);
    }
    return base_color;
}

float ResolveMetallic(float2 texcoord) {
    float metallic = g_material.factors.x;
    if (HasTexture(g_material.texture_flags2.x)) {
        metallic *= g_metallic_roughness_texture.Sample(g_material_sampler, texcoord).b;
    } else if (HasTexture(g_material.texture_flags.z)) {
        metallic *= g_metallic_texture.Sample(g_material_sampler, texcoord).r;
    }
    return saturate(metallic);
}

float ResolveRoughness(float2 texcoord) {
    float roughness = g_material.factors.y;
    if (HasTexture(g_material.texture_flags2.x)) {
        roughness *= g_metallic_roughness_texture.Sample(g_material_sampler, texcoord).g;
    } else if (HasTexture(g_material.texture_flags.w)) {
        roughness *= g_roughness_texture.Sample(g_material_sampler, texcoord).r;
    }
    return clamp(roughness, 0.04f, 1.0f);
}

float ResolveAmbientOcclusion(float2 texcoord) {
    if (!HasTexture(g_material.texture_flags2.y)) {
        return 1.0f;
    }

    const float ao = g_ao_texture.Sample(g_material_sampler, texcoord).r;
    return lerp(1.0f, ao, saturate(g_material.texture_flags2.w));
}

float3 ResolveEmissive(float2 texcoord) {
    float3 emissive_factor = g_material.emissive_factor_normal_scale.xyz;
    if (!HasTexture(g_material.texture_flags2.z)) {
        return emissive_factor;
    }

    if (dot(abs(emissive_factor), float3(1.0f, 1.0f, 1.0f)) <= 0.0001f) {
        emissive_factor = float3(1.0f, 1.0f, 1.0f);
    }

    return g_emissive_texture.Sample(g_material_sampler, texcoord).rgb * emissive_factor;
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

float3 EvaluateIBL(
    float3 base_color,
    float metallic,
    float roughness,
    float ambient_occlusion,
    float3 normal,
    float3 view_dir) {
    const float ndotv = max(dot(normal, view_dir), 0.0f);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metallic);
    const float3 fresnel = FresnelSchlickRoughness(ndotv, f0, roughness);
    const float3 diffuse_weight = (1.0f - fresnel) * (1.0f - metallic);

    const float3 irradiance = max(g_irradiance_map.SampleLevel(g_environment_sampler, normal, 0.0f).rgb, 0.0f);
    const float3 diffuse = irradiance * base_color;

    const float3 reflection_dir = reflect(-view_dir, normal);
    const float reflection_lod = roughness * MAX_REFLECTION_LOD;
    const float3 prefiltered = max(
        g_prefiltered_environment_map.SampleLevel(g_environment_sampler, reflection_dir, reflection_lod).rgb,
        0.0f);
    const float2 brdf = g_brdf_lut.SampleLevel(g_environment_sampler, float2(ndotv, roughness), 0.0f).rg;
    const float3 specular = prefiltered * (fresnel * brdf.x + brdf.y);

    return (diffuse_weight * diffuse + specular) *
        ambient_occlusion *
        max(g_frame.camera_position_environment_intensity.w, 0.0f);
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
    float4 base_color = ResolveBaseColor(input.texcoord);
    if (g_material.factors.w > 0.5f && g_material.factors.w < 1.5f) {
        clip(base_color.a - g_material.factors.z);
    }

    float metallic = ResolveMetallic(input.texcoord);
    float roughness = ResolveRoughness(input.texcoord);
    float ambient_occlusion = ResolveAmbientOcclusion(input.texcoord);
    float3 emissive = ResolveEmissive(input.texcoord);
    float3 normal = ResolveNormal(input);
    float3 view_dir = normalize(g_frame.camera_position_environment_intensity.xyz - input.world_position);

    float3 lit_color = EvaluateIBL(
        base_color.rgb,
        metallic,
        roughness,
        ambient_occlusion,
        normal,
        view_dir);

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

    return float4(lit_color + emissive, base_color.a);
}
