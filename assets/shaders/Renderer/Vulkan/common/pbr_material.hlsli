#ifndef NEXAUR_PBR_MATERIAL_HLSLI
#define NEXAUR_PBR_MATERIAL_HLSLI

struct NxMaterialSample {
    float4 base_color;
    float metallic;
    float roughness;
    float ambient_occlusion;
    float3 emissive;
    float3 normal;
};

bool NxHasTexture(float flag) {
    return flag > 0.5f;
}

float3 NxFallbackTangent(float3 normal) {
    const float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(up, normal));
}

float3 NxResolveNormal(VSOutput input) {
    float3 normal = normalize(input.world_normal);
    if (!NxHasTexture(g_material.texture_flags.y)) {
        return normal;
    }

    float3 tangent = input.world_tangent;
    if (dot(tangent, tangent) <= 0.000001f) {
        tangent = NxFallbackTangent(normal);
    } else {
        tangent = normalize(tangent - normal * dot(normal, tangent));
    }

    const float3 input_bitangent = input.world_bitangent;
    float handedness = 1.0f;
    if (dot(input_bitangent, input_bitangent) > 0.000001f) {
        handedness = dot(cross(normal, tangent), input_bitangent) < 0.0f ? -1.0f : 1.0f;
    }

    float3 bitangent = cross(normal, tangent) * handedness;
    if (dot(bitangent, bitangent) <= 0.000001f) {
        tangent = NxFallbackTangent(normal);
        bitangent = cross(normal, tangent) * handedness;
    }
    bitangent = normalize(bitangent);

    float3 tangent_normal = g_normal_texture.Sample(g_material_sampler, input.texcoord).xyz * 2.0f - 1.0f;
    tangent_normal.xy *= max(g_material.emissive_factor_normal_scale.w, 0.0f);
    return normalize(
        tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        normal * tangent_normal.z);
}

float4 NxResolveBaseColor(float2 texcoord) {
    float4 base_color = g_material.base_color_factor;
    if (NxHasTexture(g_material.texture_flags.x)) {
        base_color *= g_base_color_texture.Sample(g_material_sampler, texcoord);
    }
    return base_color;
}

float NxResolveMetallic(float2 texcoord) {
    float metallic = g_material.factors.x;
    if (NxHasTexture(g_material.texture_flags2.x)) {
        metallic *= g_metallic_roughness_texture.Sample(g_material_sampler, texcoord).b;
    } else if (NxHasTexture(g_material.texture_flags.z)) {
        metallic *= g_metallic_texture.Sample(g_material_sampler, texcoord).r;
    }
    return saturate(metallic);
}

float NxResolveRoughness(float2 texcoord) {
    float roughness = g_material.factors.y;
    if (NxHasTexture(g_material.texture_flags2.x)) {
        roughness *= g_metallic_roughness_texture.Sample(g_material_sampler, texcoord).g;
    } else if (NxHasTexture(g_material.texture_flags.w)) {
        roughness *= g_roughness_texture.Sample(g_material_sampler, texcoord).r;
    }
    return clamp(roughness, 0.04f, 1.0f);
}

float NxResolveAmbientOcclusion(float2 texcoord) {
    if (!NxHasTexture(g_material.texture_flags2.y)) {
        return 1.0f;
    }

    const float ao = g_ao_texture.Sample(g_material_sampler, texcoord).r;
    return lerp(1.0f, ao, saturate(g_material.texture_flags2.w));
}

float3 NxResolveEmissive(float2 texcoord) {
    float3 emissive_factor = g_material.emissive_factor_normal_scale.xyz;
    if (!NxHasTexture(g_material.texture_flags2.z)) {
        return emissive_factor;
    }

    if (dot(abs(emissive_factor), float3(1.0f, 1.0f, 1.0f)) <= 0.0001f) {
        emissive_factor = float3(1.0f, 1.0f, 1.0f);
    }

    return g_emissive_texture.Sample(g_material_sampler, texcoord).rgb * emissive_factor;
}

NxMaterialSample NxSampleMaterial(VSOutput input) {
    NxMaterialSample material;
    material.base_color = NxResolveBaseColor(input.texcoord);
    material.metallic = NxResolveMetallic(input.texcoord);
    material.roughness = NxResolveRoughness(input.texcoord);
    material.ambient_occlusion = NxResolveAmbientOcclusion(input.texcoord);
    material.emissive = NxResolveEmissive(input.texcoord);
    material.normal = NxResolveNormal(input);
    return material;
}

#endif
