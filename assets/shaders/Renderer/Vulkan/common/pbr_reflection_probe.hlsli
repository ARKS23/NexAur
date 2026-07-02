#ifndef NEXAUR_PBR_REFLECTION_PROBE_HLSLI
#define NEXAUR_PBR_REFLECTION_PROBE_HLSLI

#include "pbr_brdf.hlsli"

struct NxReflectionProbeSample {
    float3 specular;
    float influence;
};

float NxReflectionProbeInfluence(float3 world_position, float3 center, float3 extents, float blend_distance) {
    const float3 local_distance = abs(world_position - center);
    const float3 edge_distance = extents - local_distance;
    const float min_edge_distance = min(edge_distance.x, min(edge_distance.y, edge_distance.z));
    if (min_edge_distance < 0.0f) {
        return 0.0f;
    }

    if (blend_distance <= 0.0001f) {
        return 1.0f;
    }

    return saturate(min_edge_distance / blend_distance);
}

float3 NxBoxProjectReflection(float3 reflection_dir, float3 world_position, float3 center, float3 extents) {
    float3 safe_dir = reflection_dir;
    safe_dir.x = abs(safe_dir.x) < 0.0001f ? (safe_dir.x < 0.0f ? -0.0001f : 0.0001f) : safe_dir.x;
    safe_dir.y = abs(safe_dir.y) < 0.0001f ? (safe_dir.y < 0.0f ? -0.0001f : 0.0001f) : safe_dir.y;
    safe_dir.z = abs(safe_dir.z) < 0.0001f ? (safe_dir.z < 0.0f ? -0.0001f : 0.0001f) : safe_dir.z;

    const float3 box_min = center - extents;
    const float3 box_max = center + extents;
    const float3 t_min = (box_min - world_position) / safe_dir;
    const float3 t_max = (box_max - world_position) / safe_dir;
    const float3 t_far = max(t_min, t_max);
    const float hit_distance = min(t_far.x, min(t_far.y, t_far.z));
    const float3 hit_position = world_position + reflection_dir * max(hit_distance, 0.0f);
    return normalize(hit_position - center);
}

NxReflectionProbeSample NxEvaluateReflectionProbeSpecular(
    float3 base_color,
    float metallic,
    float roughness,
    float3 normal,
    float3 view_dir,
    float3 world_position,
    float3 probe_center,
    float3 probe_extents,
    float blend_distance,
    float intensity,
    bool box_projection,
    float prefilter_max_lod) {
    NxReflectionProbeSample result;
    result.specular = 0.0f;
    result.influence = NxReflectionProbeInfluence(world_position, probe_center, probe_extents, blend_distance);
    if (result.influence <= 0.0f || intensity <= 0.0f) {
        return result;
    }

    const float ndotv = max(saturate(dot(normal, view_dir)), 0.0001f);
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metallic);
    float3 reflection_dir = reflect(-view_dir, normal);
    if (box_projection) {
        reflection_dir = NxBoxProjectReflection(reflection_dir, world_position, probe_center, probe_extents);
    }

    const float reflection_lod = roughness * max(prefilter_max_lod, 0.0f);
    const float3 prefiltered = max(
        g_reflection_probe_prefiltered_environment_map.SampleLevel(
            g_reflection_probe_sampler,
            reflection_dir,
            reflection_lod).rgb,
        0.0f);
    const float2 brdf = g_reflection_probe_brdf_lut.SampleLevel(
        g_reflection_probe_sampler,
        float2(ndotv, roughness),
        0.0f).rg;
    result.specular = prefiltered * (f0 * brdf.x + brdf.y) * max(intensity, 0.0f);
    return result;
}

#endif
