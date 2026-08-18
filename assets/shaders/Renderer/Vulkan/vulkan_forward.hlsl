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
    float3 world_tangent : TEXCOORD3;
    float3 world_bitangent : TEXCOORD4;
    float view_depth : TEXCOORD5;
    float4 previous_position : TEXCOORD6;
};

struct PushConstants {
    float4x4 model;
    float4 previous_model_rows[3];
};

[[vk::push_constant]]
PushConstants g_push_constants;

struct FrameGlobals {
    float4x4 view_projection;
    float4x4 previous_view_projection;
    float4x4 shadow_light_view_projection[4];
    float4 camera_position_environment_intensity;
    float4 directional_direction_intensity;
    float4 directional_color_point_count;
    float4 ambient_color_intensity;
    float4 shadow_params; // x: enabled, y: strength, z: bias, w: shadow map size
    float4 ray_query_shadow_params; // x: mode, y: max distance, z: normal bias, w: direction bias
    float4 shadow_quality_params; // x: filter mode, y: radius, z: normal bias, w: slope bias
    float4 shadow_pcss_params; // x: light radius, y: search radius, z: min radius, w: max radius
    float4 shadow_cascade_splits; // xyz/w: view-space far split depth per cascade
    float4 shadow_cascade_params; // x: enabled, y: cascade count, z: debug overlay, w: reserved
    float4 shadow_cascade_colors[4];
    float4 ibl_debug_params; // x: debug mode, y: debug prefilter mip, z: prefilter max lod, w: reserved
    float4x4 view_matrix;
    float4x4 point_shadow_view_projection[24];
    float4 point_shadow_params; // x: enabled, y: map size, z: constant bias, w: normal bias
    float4 point_shadow_quality_params; // x: filter radius, y: shadowed light count, zw: reserved
    float4 contact_shadow_params; // x: enabled, y: intensity, z: max distance, w: thickness
    float4x4 rect_shadow_view_projection[4];
    float4 rect_shadow_params; // x: enabled, y: map size, z: constant bias, w: normal bias
    float4 rect_shadow_quality_params; // x: filter radius, y: shadowed light count, z: strength, w: reserved
    float4 rect_shadow_pcss_params; // x: enabled, y: light radius, z: search radius, w: min filter radius
    float4 rect_shadow_pcss_quality_params; // x: max filter radius, y: blocker taps, z: filter taps, w: reserved
    float4 rect_light_params; // x: enabled, y: count, z: ltc specular enabled, w: reserved
    float4 rect_light_ltc_params; // x: specular scale, y: debug ltc only, zw: reserved
    float4 reflection_probe_center_intensity; // xyz: center, w: intensity
    float4 reflection_probe_extents_blend; // xyz: box extents, w: blend distance
    float4 reflection_probe_params; // x: enabled, y: box projection, z: probe prefilter max lod, w: reserved
    float4 reflection_probe_diffuse_params; // x: enabled, y: intensity, zw: reserved
};

float3x3 BuildNormalMatrix(float3x3 model_matrix) {
    const float3 column0 = cross(model_matrix[1], model_matrix[2]);
    const float3 column1 = cross(model_matrix[2], model_matrix[0]);
    const float3 column2 = cross(model_matrix[0], model_matrix[1]);
    const float determinant = dot(model_matrix[0], column0);
    if (abs(determinant) <= 0.000001f) {
        return model_matrix;
    }

    float3x3 normal_matrix;
    normal_matrix[0] = column0 / determinant;
    normal_matrix[1] = column1 / determinant;
    normal_matrix[2] = column2 / determinant;
    return normal_matrix;
}

struct PointLightData {
    float4 position_intensity;
    float4 color;
    float4 attenuation;
    float4 shadow; // x: slot, y: range, z: strength, w: enabled
};

struct RectLightData {
    float4 position_intensity;
    float4 right_width;
    float4 up_height;
    float4 normal_range;
    float4 color_flags; // xyz: color, w: two sided
    float4 shadow; // x: slot, y: strength, z: enabled, w: reserved
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameGlobals> g_frame;

[[vk::binding(1, 0)]]
StructuredBuffer<PointLightData> g_point_lights;

[[vk::binding(2, 0)]]
Texture2DArray<float> g_shadow_map;

[[vk::binding(3, 0)]]
SamplerState g_shadow_sampler;

[[vk::binding(4, 0)]]
Texture2DArray<float> g_point_shadow_map;

[[vk::binding(5, 0)]]
SamplerState g_point_shadow_sampler;

[[vk::binding(6, 0)]]
StructuredBuffer<RectLightData> g_rect_lights;

[[vk::binding(7, 0)]]
Texture2DArray<float> g_rect_shadow_map;

[[vk::binding(8, 0)]]
SamplerState g_rect_shadow_sampler;

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

[[vk::binding(0, 3)]]
TextureCube<float4> g_reflection_probe_environment_map;

[[vk::binding(1, 3)]]
TextureCube<float4> g_reflection_probe_irradiance_map;

[[vk::binding(2, 3)]]
TextureCube<float4> g_reflection_probe_prefiltered_environment_map;

[[vk::binding(3, 3)]]
Texture2D<float4> g_reflection_probe_brdf_lut;

[[vk::binding(4, 3)]]
SamplerState g_reflection_probe_sampler;

#if defined(NEXAUR_ENABLE_RAY_QUERY_DEBUG) || defined(NEXAUR_ENABLE_RAY_QUERY_SHADOW)
[[vk::binding(0, 4)]]
RaytracingAccelerationStructure g_ray_query_scene;
#endif

#include "common/pbr_brdf.hlsli"
#include "common/pbr_material.hlsli"
#include "common/pbr_ibl.hlsli"
#include "common/pbr_reflection_probe.hlsli"
#include "common/shadow_sampling.hlsli"
#include "common/area_light_ltc.hlsli"

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    float3x3 model_matrix = (float3x3)g_push_constants.model;
    float3x3 normal_matrix = BuildNormalMatrix(model_matrix);
    output.position = mul(g_frame.view_projection, world_position);
    const float4 object_position = float4(input.position, 1.0f);
    const float3 previous_world_position = float3(
        dot(g_push_constants.previous_model_rows[0], object_position),
        dot(g_push_constants.previous_model_rows[1], object_position),
        dot(g_push_constants.previous_model_rows[2], object_position));
    output.previous_position = mul(
        g_frame.previous_view_projection,
        float4(previous_world_position, 1.0f));
    output.world_position = world_position.xyz;
    output.world_normal = normalize(mul(normal_matrix, input.normal));
    output.world_tangent = mul(model_matrix, input.tangent);
    output.world_bitangent = mul(model_matrix, input.bitangent);
    output.texcoord = input.texcoord;
    output.view_depth = max(-mul(g_frame.view_matrix, world_position).z, 0.0f);
    return output;
}

uint ResolveIblDebugMode() {
    return (uint)max(g_frame.ibl_debug_params.x, 0.0f);
}

float3 EncodeNormalDebug(float3 normal) {
    return normal * 0.5f + 0.5f;
}

float3 EvaluateIblDebugColor(
    uint mode,
    NxMaterialSample material,
    NxIblSample ibl,
    float3 view_dir,
    float reflection_probe_influence,
    float3 reflection_probe_specular,
    float3 reflection_probe_diffuse) {
    switch (mode) {
        case 1:
            return ibl.diffuse;
        case 2:
            return ibl.specular;
        case 3:
            return ibl.diffuse + ibl.specular;
        case 4:
            return EncodeNormalDebug(material.normal);
        case 5:
            return float3(material.metallic, material.metallic, material.metallic);
        case 6:
            return float3(material.roughness, material.roughness, material.roughness);
        case 7:
            return float3(material.ambient_occlusion, material.ambient_occlusion, material.ambient_occlusion);
        case 8:
            return material.emissive;
        case 9:
            return ibl.irradiance;
        case 10:
            return NxSamplePrefilteredEnvironmentDebug(
                material.normal,
                view_dir,
                min(g_frame.ibl_debug_params.y, max(g_frame.ibl_debug_params.z, 0.0f))) *
                max(g_frame.camera_position_environment_intensity.w, 0.0f);
        case 11:
            return float3(ibl.brdf.r, ibl.brdf.g, 0.0f);
        case 12:
            return float3(
                reflection_probe_influence,
                reflection_probe_influence,
                reflection_probe_influence);
        case 13:
            return reflection_probe_specular * reflection_probe_influence;
        case 14:
            return reflection_probe_diffuse * reflection_probe_influence;
        case 15:
            return material.base_color.rgb;
        default:
            return 0.0f;
    }
}

float Luminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

#if defined(NEXAUR_ENABLE_RAY_QUERY_DEBUG) || defined(NEXAUR_ENABLE_RAY_QUERY_SHADOW)
bool NxTraceRayQueryDirectionalOcclusion(
    float3 world_position,
    float3 world_normal,
    float3 light_direction) {
    const float3 safe_normal = NxSafeNormalizeShadowVector(
        world_normal,
        float3(0.0f, 1.0f, 0.0f));
    const float3 safe_light_direction = NxSafeNormalizeShadowVector(
        light_direction,
        float3(0.0f, 1.0f, 0.0f));
    const float normal_bias = max(g_frame.ray_query_shadow_params.z, 0.0f);
    const float direction_bias = max(g_frame.ray_query_shadow_params.w, 0.0f);
    const float ray_distance = max(g_frame.ray_query_shadow_params.y, 0.001f);
    const float3 origin = world_position +
        safe_normal * normal_bias +
        safe_light_direction * direction_bias;

    RayQuery<
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
    RayDesc ray;
    ray.Origin = origin;
    ray.TMin = 0.001f;
    ray.Direction = safe_light_direction;
    ray.TMax = ray_distance;
    query.TraceRayInline(
        g_ray_query_scene,
        RAY_FLAG_NONE,
        0xff,
        ray);
    while (query.Proceed()) {
    }

    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

float NxEvaluateRayQueryDirectionalShadow(
    float3 world_position,
    float3 world_normal,
    float3 light_direction) {
    if (g_frame.shadow_params.x < 0.5f ||
        g_frame.ray_query_shadow_params.x < 0.5f) {
        return 1.0f;
    }

    return NxTraceRayQueryDirectionalOcclusion(
        world_position,
        world_normal,
        light_direction) ?
        1.0f - saturate(g_frame.shadow_params.y) :
        1.0f;
}
#endif

float EvaluateSsrSurfaceMask(NxMaterialSample material, float3 view_dir) {
    const float smoothness = 1.0f - material.roughness;
    const float smoothness_weight = smoothstep(0.30f, 0.80f, smoothness);
    if (smoothness_weight <= 0.0001f) {
        return 0.0f;
    }

    const float n_dot_v = saturate(dot(material.normal, view_dir));
    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), material.base_color.rgb, material.metallic);
    const float3 fresnel = f0 + (1.0f - f0) * pow(1.0f - n_dot_v, 5.0f);
    const float dielectric_boost = lerp(4.0f, 1.25f, material.metallic);
    return saturate(smoothness_weight * Luminance(fresnel) * dielectric_boost);
}

#if defined(NEXAUR_FORWARD_MRT)
struct ForwardMrtOutput {
    float4 scene_color : SV_Target0;
    float4 reflection_surface : SV_Target1;
    float4 fallback_specular : SV_Target2;
    float2 motion_vector : SV_Target3;
};

float2 EncodeSurfaceNormal(float3 normal) {
    const float3 safe_normal = normalize(normal);
    const float denominator = max(abs(safe_normal.x) + abs(safe_normal.y) + abs(safe_normal.z), 0.00001f);
    float2 encoded = safe_normal.xy / denominator;
    if (safe_normal.z < 0.0f) {
        encoded = (1.0f - abs(encoded.yx)) * sign(encoded.xy);
    }
    return encoded * 0.5f + 0.5f;
}

ForwardMrtOutput BuildForwardMrtOutput(
    float3 scene_color,
    NxMaterialSample material,
    NxIblSample ibl,
    VSOutput input,
    float3 view_dir,
    float reflection_mask) {
    ForwardMrtOutput output;
    output.scene_color = float4(scene_color, reflection_mask);
    output.reflection_surface = float4(
        EncodeSurfaceNormal(material.normal),
        saturate(material.roughness),
        reflection_mask);
    output.fallback_specular = float4(max(ibl.specular, 0.0f), 1.0f);
    const float current_w = max(abs(input.position.w), 0.00001f);
    const float previous_w = max(abs(input.previous_position.w), 0.00001f);
    const float2 current_uv = input.position.xy / current_w * 0.5f + 0.5f;
    const float2 previous_uv = input.previous_position.xy / previous_w * 0.5f + 0.5f;
    output.motion_vector = current_uv - previous_uv;
    return output;
}
#endif

float3 EvaluateRectLight(
    NxMaterialSample material,
    float3 world_position,
    float3 view_dir,
    RectLightData light) {
    const float shadow_visibility = NxEvaluateRectShadowVisibility(
        world_position,
        material.normal,
        light);

    if (g_frame.rect_light_params.z < 0.5f) {
        return shadow_visibility * NxEvaluateRectLightFallbackDirect(material, world_position, view_dir, light);
    }

    const float3 diffuse = NxEvaluateRectLightDiffuseApprox(material, world_position, view_dir, light);
    const float3 specular = NxEvaluateRectLightSpecularLtc(
        material,
        world_position,
        view_dir,
        light,
        g_frame.rect_light_ltc_params.x);
    if (g_frame.rect_light_ltc_params.y > 0.5f) {
        return shadow_visibility * specular;
    }
    return shadow_visibility * (diffuse + specular);
}

#ifdef NEXAUR_ENABLE_RAY_QUERY_DEBUG
float4 PSMain(VSOutput input) : SV_Target0 {
    const float3 directional_light_dir = normalize(
        -g_frame.directional_direction_intensity.xyz);
    const bool hit = NxTraceRayQueryDirectionalOcclusion(
        input.world_position,
        input.world_normal,
        directional_light_dir);
    return hit ?
        float4(1.0f, 0.05f, 0.05f, 1.0f) :
        float4(0.05f, 1.0f, 0.05f, 1.0f);
}
#else
#if defined(NEXAUR_FORWARD_MRT)
ForwardMrtOutput PSMain(VSOutput input) {
#else
float4 PSMain(VSOutput input) : SV_Target0 {
#endif
    NxMaterialSample material = NxSampleMaterial(input);
    if (g_material.factors.w > 0.5f && g_material.factors.w < 1.5f) {
        clip(material.base_color.a - g_material.factors.z);
    }

    float3 view_dir = normalize(g_frame.camera_position_environment_intensity.xyz - input.world_position);
    NxIblSample ibl = NxEvaluateIbl(
        material.base_color.rgb,
        material.metallic,
        material.roughness,
        material.ambient_occlusion,
        material.normal,
        view_dir,
        g_frame.camera_position_environment_intensity.w,
        g_frame.ibl_debug_params.z);

    float reflection_probe_influence = 0.0f;
    float3 reflection_probe_specular = 0.0f;
    float3 reflection_probe_diffuse = 0.0f;
    if (g_frame.reflection_probe_params.x > 0.5f) {
        const NxReflectionProbeSample reflection_probe = NxEvaluateReflectionProbeSpecular(
            material.base_color.rgb,
            material.metallic,
            material.roughness,
            material.normal,
            view_dir,
            input.world_position,
            g_frame.reflection_probe_center_intensity.xyz,
            max(g_frame.reflection_probe_extents_blend.xyz, float3(0.05f, 0.05f, 0.05f)),
            g_frame.reflection_probe_extents_blend.w,
            g_frame.reflection_probe_center_intensity.w,
            g_frame.reflection_probe_params.y > 0.5f,
            g_frame.reflection_probe_params.z);
        reflection_probe_influence = reflection_probe.influence;
        reflection_probe_specular = reflection_probe.specular;
        if (g_frame.reflection_probe_center_intensity.w > 0.0001f) {
            ibl.specular = lerp(ibl.specular, reflection_probe_specular, reflection_probe_influence);
        }
        if (g_frame.reflection_probe_diffuse_params.x > 0.5f) {
            reflection_probe_diffuse = NxEvaluateReflectionProbeDiffuse(
                material.base_color.rgb,
                material.metallic,
                material.roughness,
                material.ambient_occlusion,
                material.normal,
                view_dir,
                g_frame.reflection_probe_diffuse_params.y);
            ibl.diffuse = lerp(ibl.diffuse, reflection_probe_diffuse, reflection_probe_influence);
        }
    }

    const uint ibl_debug_mode = ResolveIblDebugMode();
    if (ibl_debug_mode != 0u) {
        const float reflection_mask = EvaluateSsrSurfaceMask(material, view_dir);
        const float3 debug_color = EvaluateIblDebugColor(
            ibl_debug_mode,
            material,
            ibl,
            view_dir,
            reflection_probe_influence,
            reflection_probe_specular,
            reflection_probe_diffuse);
#if defined(NEXAUR_FORWARD_MRT)
        return BuildForwardMrtOutput(debug_color, material, ibl, input, view_dir, reflection_mask);
#else
        return float4(debug_color, reflection_mask);
#endif
    }

    float3 lit_color = ibl.diffuse + ibl.specular;

    float3 directional_light_dir = normalize(-g_frame.directional_direction_intensity.xyz);
    float3 directional_radiance = g_frame.directional_color_point_count.rgb * g_frame.directional_direction_intensity.w;
    #ifdef NEXAUR_ENABLE_RAY_QUERY_SHADOW
    const float directional_shadow = NxEvaluateRayQueryDirectionalShadow(
        input.world_position,
        input.world_normal,
        directional_light_dir);
    #else
    const float directional_shadow = NxEvaluateShadowVisibility(
        input.world_position,
        material.normal,
        directional_light_dir,
        input.view_depth);
    #endif
    lit_color += directional_shadow * NxEvaluateDirectLight(
        material.base_color.rgb,
        material.metallic,
        material.roughness,
        material.normal,
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
        const float point_shadow = NxEvaluatePointShadowVisibility(
            input.world_position,
            material.normal,
            light.position_intensity.xyz,
            light.shadow);

        lit_color += point_shadow * NxEvaluateDirectLight(
            material.base_color.rgb,
            material.metallic,
            material.roughness,
            material.normal,
            view_dir,
            point_light_dir,
            radiance);
    }

    if (g_frame.rect_light_params.x > 0.5f) {
        const uint rect_light_count = (uint)g_frame.rect_light_params.y;
        [loop]
        for (uint index = 0; index < rect_light_count; ++index) {
            lit_color += EvaluateRectLight(
                material,
                input.world_position,
                view_dir,
                g_rect_lights[index]);
        }
    }

    lit_color = NxApplyCascadeDebugOverlay(lit_color, input.view_depth);

    const float reflection_mask = EvaluateSsrSurfaceMask(material, view_dir);
#if defined(NEXAUR_FORWARD_MRT)
    return BuildForwardMrtOutput(
        lit_color + material.emissive,
        material,
        ibl,
        input,
        view_dir,
        reflection_mask);
#else
    return float4(lit_color + material.emissive, reflection_mask);
#endif
}
#endif
