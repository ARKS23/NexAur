#ifndef NEXAUR_PBR_IBL_HLSLI
#define NEXAUR_PBR_IBL_HLSLI

#include "pbr_brdf.hlsli"

struct NxIblSample {
    float3 diffuse;
    float3 specular;
    float3 irradiance;
    float3 prefiltered;
    float2 brdf;
};

NxIblSample NxEvaluateIbl(
    float3 base_color,
    float metallic,
    float roughness,
    float ambient_occlusion,
    float3 normal,
    float3 view_dir,
    float environment_intensity,
    float prefilter_max_lod) {
    NxIblSample result;

    const float ndotv = max(saturate(dot(normal, view_dir)), 0.0001f);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metallic);
    const float3 fresnel = NxFresnelSchlickRoughness(ndotv, f0, roughness);
    const float3 diffuse_weight = (1.0f - fresnel) * (1.0f - metallic);

    result.irradiance = max(g_irradiance_map.SampleLevel(g_environment_sampler, normal, 0.0f).rgb, 0.0f);
    result.diffuse = diffuse_weight * result.irradiance * base_color * ambient_occlusion;

    const float3 reflection_dir = reflect(-view_dir, normal);
    const float reflection_lod = roughness * max(prefilter_max_lod, 0.0f);
    result.prefiltered = max(
        g_prefiltered_environment_map.SampleLevel(g_environment_sampler, reflection_dir, reflection_lod).rgb,
        0.0f);
    result.brdf = g_brdf_lut.SampleLevel(g_environment_sampler, float2(ndotv, roughness), 0.0f).rg;
    result.specular = result.prefiltered * (f0 * result.brdf.x + result.brdf.y);

    const float intensity = max(environment_intensity, 0.0f);
    result.diffuse *= intensity;
    result.specular *= intensity;
    result.irradiance *= intensity;
    result.prefiltered *= intensity;
    return result;
}

float3 NxSamplePrefilteredEnvironmentDebug(float3 normal, float3 view_dir, float mip) {
    const float3 reflection_dir = reflect(-view_dir, normal);
    return max(g_prefiltered_environment_map.SampleLevel(g_environment_sampler, reflection_dir, max(mip, 0.0f)).rgb, 0.0f);
}

#endif
