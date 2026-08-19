[[vk::binding(0, 0)]]
Texture2D<float4> g_reflection_surface;

[[vk::binding(1, 0)]]
Texture2D<float> g_scene_depth;

[[vk::binding(2, 0)]]
Texture2D<float> g_ssr_hit_mask;

[[vk::binding(3, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> g_raw_reflection;

[[vk::binding(4, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> g_hit_distance;

struct GpuRtInstanceRecord {
    uint geometry_index;
    uint material_index;
    uint object_flags;
    uint entity_id;
};

struct GpuRtGeometryRecord {
    uint vertex_buffer_index;
    uint index_buffer_index;
    uint vertex_stride;
    uint vertex_count;
    uint index_count;
    uint flags;
    uint reserved0;
    uint reserved1;
};

struct GpuRtMaterialRecord {
    float4 base_color_factor;
    float4 emissive_factor_normal_scale;
    float4 metallic_roughness_alpha_flags;
    uint4 texture_indices0;
    uint4 texture_indices1;
};

[[vk::binding(0, 1)]]
RaytracingAccelerationStructure g_ray_query_scene;

[[vk::binding(1, 1)]]
StructuredBuffer<GpuRtInstanceRecord> g_instance_table;

[[vk::binding(2, 1)]]
StructuredBuffer<GpuRtGeometryRecord> g_geometry_table;

[[vk::binding(3, 1)]]
StructuredBuffer<GpuRtMaterialRecord> g_material_table;

[[vk::binding(4, 1)]]
Texture2D<float4> g_material_textures[];

[[vk::binding(5, 1)]]
SamplerState g_material_sampler;

[[vk::binding(6, 1)]]
ByteAddressBuffer g_vertex_buffers[];

[[vk::binding(7, 1)]]
ByteAddressBuffer g_index_buffers[];

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

struct ReflectionTracePushConstants {
    float4x4 inverse_view_projection;
    float4 camera_position_max_distance;
    uint4 extents; // xy: source, zw: output
    float4 trace_params; // x: max roughness, y: normal bias, z: TMin, w: environment intensity
    uint4 table_counts_flags; // xyz: table counts, w: flags/debug mode
};

[[vk::push_constant]]
ReflectionTracePushConstants g_trace;

#include "../common/pbr_brdf.hlsli"

static const uint RT_TRACE_FLAG_SSR_ENABLED = 1u << 0u;
static const uint RT_TRACE_DEBUG_MODE_SHIFT = 8u;
static const uint RT_TRACE_DEBUG_INSTANCE_ID = 4u;
static const uint RT_TRACE_DEBUG_PRIMITIVE_ID = 5u;

static const uint RT_MATERIAL_BASE_COLOR_TEXTURE_BIT = 1u << 0u;
static const uint RT_MATERIAL_NORMAL_TEXTURE_BIT = 1u << 1u;
static const uint RT_MATERIAL_METALLIC_TEXTURE_BIT = 1u << 2u;
static const uint RT_MATERIAL_ROUGHNESS_TEXTURE_BIT = 1u << 3u;
static const uint RT_MATERIAL_PACKED_MR_TEXTURE_BIT = 1u << 4u;
static const uint RT_MATERIAL_AO_TEXTURE_BIT = 1u << 5u;
static const uint RT_MATERIAL_EMISSIVE_TEXTURE_BIT = 1u << 6u;

struct RtVertex {
    float3 position;
    float3 normal;
    float2 texcoord;
    float3 tangent;
    float3 bitangent;
};

struct RtMaterialSample {
    float3 base_color;
    float metallic;
    float roughness;
    float ambient_occlusion;
    float3 emissive;
    float3 normal;
};

uint2 mapSourcePixel(uint2 output_pixel) {
    const uint2 source_extent = max(g_trace.extents.xy, uint2(1u, 1u));
    const uint2 output_extent = max(g_trace.extents.zw, uint2(1u, 1u));
    const uint2 numerator = (output_pixel * 2u + 1u) * source_extent;
    return min(numerator / (output_extent * 2u), source_extent - 1u);
}

float3 reconstructWorldPosition(uint2 source_pixel, float depth) {
    const float2 source_extent = max(float2(g_trace.extents.xy), float2(1.0f, 1.0f));
    const float2 uv = (float2(source_pixel) + 0.5f) / source_extent;
    const float2 ndc = uv * 2.0f - 1.0f;
    float4 world_position = mul(
        g_trace.inverse_view_projection,
        float4(ndc, depth, 1.0f));
    const float inverse_w = abs(world_position.w) > 0.00001f ?
        rcp(world_position.w) : 0.0f;
    return world_position.xyz * inverse_w;
}

float2 signNotZero(float2 value) {
    return float2(value.x >= 0.0f ? 1.0f : -1.0f,
                  value.y >= 0.0f ? 1.0f : -1.0f);
}

float3 decodeSurfaceNormal(float2 encoded) {
    const float2 octahedral = encoded * 2.0f - 1.0f;
    float3 normal = float3(
        octahedral,
        1.0f - abs(octahedral.x) - abs(octahedral.y));
    if (normal.z < 0.0f) {
        normal.xy =
            (1.0f - abs(normal.yx)) * signNotZero(normal.xy);
    }
    return normalize(normal);
}

float3 transformDirection(float3x4 transform, float3 direction) {
    return float3(
        dot(transform[0].xyz, direction),
        dot(transform[1].xyz, direction),
        dot(transform[2].xyz, direction));
}

float3 transformNormal(float3x4 world_to_object, float3 normal) {
    return normalize(float3(
        dot(float3(world_to_object[0][0], world_to_object[1][0], world_to_object[2][0]), normal),
        dot(float3(world_to_object[0][1], world_to_object[1][1], world_to_object[2][1]), normal),
        dot(float3(world_to_object[0][2], world_to_object[1][2], world_to_object[2][2]), normal)));
}

float4 sampleMaterialTexture(uint texture_index, float2 texcoord) {
    return g_material_textures[NonUniformResourceIndex(texture_index)].SampleLevel(
        g_material_sampler,
        texcoord,
        0.0f);
}

RtVertex loadVertex(ByteAddressBuffer vertex_buffer, uint offset) {
    RtVertex vertex;
    vertex.position = asfloat(vertex_buffer.Load3(offset));
    vertex.normal = asfloat(vertex_buffer.Load3(offset + 12u));
    vertex.texcoord = asfloat(vertex_buffer.Load2(offset + 24u));
    vertex.tangent = asfloat(vertex_buffer.Load3(offset + 32u));
    vertex.bitangent = asfloat(vertex_buffer.Load3(offset + 44u));
    return vertex;
}

float3 interpolateFloat3(
    float3 vertex0,
    float3 vertex1,
    float3 vertex2,
    float3 weights) {
    return vertex0 * weights.x + vertex1 * weights.y + vertex2 * weights.z;
}

float2 interpolateFloat2(
    float2 vertex0,
    float2 vertex1,
    float2 vertex2,
    float3 weights) {
    return vertex0 * weights.x + vertex1 * weights.y + vertex2 * weights.z;
}

float3 fallbackTangent(float3 normal) {
    const float3 axis = abs(normal.z) < 0.999f ?
        float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(axis, normal));
}

RtMaterialSample sampleMaterial(
    GpuRtMaterialRecord material,
    float2 texcoord,
    float3 world_normal,
    float3 world_tangent,
    float3 world_bitangent) {
    const uint texture_flags = material.texture_indices1.w;
    RtMaterialSample result;
    result.base_color = material.base_color_factor.rgb;
    if ((texture_flags & RT_MATERIAL_BASE_COLOR_TEXTURE_BIT) != 0u) {
        result.base_color *= sampleMaterialTexture(
            material.texture_indices0.x,
            texcoord).rgb;
    }

    result.metallic = material.metallic_roughness_alpha_flags.x;
    result.roughness = material.metallic_roughness_alpha_flags.y;
    if ((texture_flags & RT_MATERIAL_PACKED_MR_TEXTURE_BIT) != 0u) {
        const float4 packed = sampleMaterialTexture(
            material.texture_indices1.x,
            texcoord);
        result.metallic *= packed.b;
        result.roughness *= packed.g;
    } else {
        if ((texture_flags & RT_MATERIAL_METALLIC_TEXTURE_BIT) != 0u) {
            result.metallic *= sampleMaterialTexture(
                material.texture_indices0.z,
                texcoord).r;
        }
        if ((texture_flags & RT_MATERIAL_ROUGHNESS_TEXTURE_BIT) != 0u) {
            result.roughness *= sampleMaterialTexture(
                material.texture_indices0.w,
                texcoord).r;
        }
    }
    result.metallic = saturate(result.metallic);
    result.roughness = clamp(result.roughness, 0.04f, 1.0f);

    result.ambient_occlusion = 1.0f;
    if ((texture_flags & RT_MATERIAL_AO_TEXTURE_BIT) != 0u) {
        result.ambient_occlusion = saturate(sampleMaterialTexture(
            material.texture_indices1.y,
            texcoord).r);
    }

    result.emissive = material.emissive_factor_normal_scale.xyz;
    if ((texture_flags & RT_MATERIAL_EMISSIVE_TEXTURE_BIT) != 0u) {
        float3 emissive_factor = result.emissive;
        if (dot(abs(emissive_factor), float3(1.0f, 1.0f, 1.0f)) <= 0.0001f) {
            emissive_factor = 1.0f;
        }
        result.emissive = sampleMaterialTexture(
            material.texture_indices1.z,
            texcoord).rgb * emissive_factor;
    }

    result.normal = world_normal;
    if ((texture_flags & RT_MATERIAL_NORMAL_TEXTURE_BIT) != 0u) {
        float3 tangent = world_tangent;
        if (dot(tangent, tangent) <= 0.000001f) {
            tangent = fallbackTangent(world_normal);
        } else {
            tangent = normalize(tangent - world_normal * dot(world_normal, tangent));
        }
        float handedness = 1.0f;
        if (dot(world_bitangent, world_bitangent) > 0.000001f) {
            handedness = dot(cross(world_normal, tangent), world_bitangent) < 0.0f ?
                -1.0f : 1.0f;
        }
        const float3 bitangent = normalize(cross(world_normal, tangent) * handedness);
        float3 tangent_normal = sampleMaterialTexture(
            material.texture_indices0.y,
            texcoord).xyz * 2.0f - 1.0f;
        tangent_normal.xy *= max(material.emissive_factor_normal_scale.w, 0.0f);
        result.normal = normalize(
            tangent * tangent_normal.x +
            bitangent * tangent_normal.y +
            world_normal * tangent_normal.z);
    }
    return result;
}

float3 evaluateHitIbl(RtMaterialSample material, float3 view_direction) {
    const float ndotv = max(saturate(dot(material.normal, view_direction)), 0.0001f);
    const float3 f0 = lerp(
        float3(0.04f, 0.04f, 0.04f),
        material.base_color,
        material.metallic);
    const float3 fresnel = NxFresnelSchlickRoughness(
        ndotv,
        f0,
        material.roughness);
    const float3 diffuse_weight = (1.0f - fresnel) * (1.0f - material.metallic);
    const float3 irradiance = max(
        g_irradiance_map.SampleLevel(
            g_environment_sampler,
            material.normal,
            0.0f).rgb,
        0.0f);
    const float3 diffuse =
        diffuse_weight * irradiance * material.base_color * material.ambient_occlusion;

    const float3 reflection_direction = reflect(-view_direction, material.normal);
    const float3 prefiltered = max(
        g_prefiltered_environment_map.SampleLevel(
            g_environment_sampler,
            reflection_direction,
            material.roughness * 8.0f).rgb,
        0.0f);
    const float2 brdf = g_brdf_lut.SampleLevel(
        g_environment_sampler,
        float2(ndotv, material.roughness),
        0.0f).rg;
    const float3 specular = prefiltered * (f0 * brdf.x + brdf.y);
    return (diffuse + specular) * max(g_trace.trace_params.w, 0.0f) +
        max(material.emissive, 0.0f);
}

float3 hashIdentifier(uint identifier) {
    identifier ^= identifier >> 16u;
    identifier *= 0x7feb352du;
    identifier ^= identifier >> 15u;
    identifier *= 0x846ca68bu;
    identifier ^= identifier >> 16u;
    return float3(
        (float)(identifier & 255u),
        (float)((identifier >> 8u) & 255u),
        (float)((identifier >> 16u) & 255u)) / 255.0f;
}

void writeMiss(uint2 output_pixel) {
    g_raw_reflection[output_pixel] = 0.0f;
    g_hit_distance[output_pixel] = 0.0f;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID) {
    const uint2 output_extent = g_trace.extents.zw;
    if (any(dispatch_id.xy >= output_extent)) {
        return;
    }

    const uint2 output_pixel = dispatch_id.xy;
    const uint2 source_pixel = mapSourcePixel(output_pixel);
    const float depth = g_scene_depth.Load(int3(source_pixel, 0));
    const float4 surface = g_reflection_surface.Load(int3(source_pixel, 0));
    const float max_roughness = max(g_trace.trace_params.x, 0.04f);
    if (depth >= 0.99999f ||
        surface.a <= 0.0001f ||
        surface.b > max_roughness ||
        !all(isfinite(surface))) {
        writeMiss(output_pixel);
        return;
    }

    const bool ssr_enabled =
        (g_trace.table_counts_flags.w & RT_TRACE_FLAG_SSR_ENABLED) != 0u;
    if (ssr_enabled) {
        const float ssr_confidence = g_ssr_hit_mask.Load(int3(source_pixel, 0));
        if (isfinite(ssr_confidence) && ssr_confidence > 0.001f) {
            writeMiss(output_pixel);
            return;
        }
    }

    const float3 world_position = reconstructWorldPosition(source_pixel, depth);
    float3 world_normal = decodeSurfaceNormal(surface.rg);
    const float3 camera_position = g_trace.camera_position_max_distance.xyz;
    const float3 to_camera = camera_position - world_position;
    if (!all(isfinite(world_position)) ||
        !all(isfinite(world_normal)) ||
        dot(to_camera, to_camera) <= 0.0000001f) {
        writeMiss(output_pixel);
        return;
    }
    if (dot(world_normal, to_camera) < 0.0f) {
        world_normal = -world_normal;
    }

    const float3 incident_direction = normalize(world_position - camera_position);
    const float3 ray_direction = normalize(reflect(incident_direction, world_normal));
    RayDesc ray;
    ray.Origin = world_position + world_normal * max(g_trace.trace_params.y, 0.0f);
    ray.TMin = max(g_trace.trace_params.z, 0.0001f);
    ray.Direction = ray_direction;
    ray.TMax = max(g_trace.camera_position_max_distance.w, ray.TMin + 0.0001f);

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
    query.TraceRayInline(g_ray_query_scene, RAY_FLAG_NONE, 0xffu, ray);
    while (query.Proceed()) {
    }
    if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT) {
        writeMiss(output_pixel);
        return;
    }

    const uint instance_index = query.CommittedInstanceID();
    const uint instance_count = g_trace.table_counts_flags.x;
    const uint geometry_count = g_trace.table_counts_flags.y;
    const uint material_count = g_trace.table_counts_flags.z;
    if (instance_index == 0u || instance_index >= instance_count) {
        writeMiss(output_pixel);
        return;
    }

    const GpuRtInstanceRecord instance = g_instance_table[instance_index];
    if (instance.geometry_index == 0u ||
        instance.geometry_index >= geometry_count ||
        instance.material_index == 0u ||
        instance.material_index >= material_count) {
        writeMiss(output_pixel);
        return;
    }

    const GpuRtGeometryRecord geometry = g_geometry_table[instance.geometry_index];
    const uint primitive_index = query.CommittedPrimitiveIndex();
    if (geometry.vertex_stride < 56u ||
        geometry.index_count % 3u != 0u ||
        primitive_index >= geometry.index_count / 3u ||
        geometry.vertex_buffer_index == 0u ||
        geometry.vertex_buffer_index >= geometry_count ||
        geometry.index_buffer_index == 0u ||
        geometry.index_buffer_index >= geometry_count) {
        writeMiss(output_pixel);
        return;
    }

    ByteAddressBuffer index_buffer =
        g_index_buffers[NonUniformResourceIndex(geometry.index_buffer_index)];
    ByteAddressBuffer vertex_buffer =
        g_vertex_buffers[NonUniformResourceIndex(geometry.vertex_buffer_index)];
    const uint index_offset = primitive_index * 12u;
    const uint3 vertex_indices = uint3(
        index_buffer.Load(index_offset),
        index_buffer.Load(index_offset + 4u),
        index_buffer.Load(index_offset + 8u));
    if (any(vertex_indices >= geometry.vertex_count)) {
        writeMiss(output_pixel);
        return;
    }

    const RtVertex vertex0 = loadVertex(
        vertex_buffer,
        vertex_indices.x * geometry.vertex_stride);
    const RtVertex vertex1 = loadVertex(
        vertex_buffer,
        vertex_indices.y * geometry.vertex_stride);
    const RtVertex vertex2 = loadVertex(
        vertex_buffer,
        vertex_indices.z * geometry.vertex_stride);
    const float2 committed_barycentrics = query.CommittedTriangleBarycentrics();
    const float3 weights = float3(
        1.0f - committed_barycentrics.x - committed_barycentrics.y,
        committed_barycentrics.x,
        committed_barycentrics.y);
    if (any(weights < -0.0001f) || !all(isfinite(weights))) {
        writeMiss(output_pixel);
        return;
    }

    const float3 object_normal = interpolateFloat3(
        vertex0.normal,
        vertex1.normal,
        vertex2.normal,
        weights);
    const float3 object_tangent = interpolateFloat3(
        vertex0.tangent,
        vertex1.tangent,
        vertex2.tangent,
        weights);
    const float3 object_bitangent = interpolateFloat3(
        vertex0.bitangent,
        vertex1.bitangent,
        vertex2.bitangent,
        weights);
    const float2 texcoord = interpolateFloat2(
        vertex0.texcoord,
        vertex1.texcoord,
        vertex2.texcoord,
        weights);
    if (!all(isfinite(object_normal)) ||
        !all(isfinite(object_tangent)) ||
        !all(isfinite(object_bitangent)) ||
        !all(isfinite(texcoord)) ||
        dot(object_normal, object_normal) <= 0.0000001f) {
        writeMiss(output_pixel);
        return;
    }

    const float3x4 object_to_world = query.CommittedObjectToWorld3x4();
    const float3x4 world_to_object = query.CommittedWorldToObject3x4();
    float3 hit_normal = transformNormal(world_to_object, normalize(object_normal));
    float3 hit_tangent = transformDirection(object_to_world, object_tangent);
    float3 hit_bitangent = transformDirection(object_to_world, object_bitangent);
    const float3 hit_view_direction = normalize(-ray_direction);
    if (dot(hit_normal, hit_view_direction) < 0.0f) {
        hit_normal = -hit_normal;
    }

    const GpuRtMaterialRecord material_record =
        g_material_table[instance.material_index];
    RtMaterialSample material = sampleMaterial(
        material_record,
        texcoord,
        hit_normal,
        hit_tangent,
        hit_bitangent);
    if (dot(material.normal, hit_view_direction) < 0.0f) {
        material.normal = -material.normal;
    }
    float3 radiance = evaluateHitIbl(material, hit_view_direction);
    if (!all(isfinite(radiance))) {
        radiance = 0.0f;
    }
    radiance = max(radiance, 0.0f);

    const float confidence = saturate(surface.a);
    const uint debug_mode = g_trace.table_counts_flags.w >> RT_TRACE_DEBUG_MODE_SHIFT;
    if (debug_mode == RT_TRACE_DEBUG_INSTANCE_ID) {
        radiance = hashIdentifier(instance_index);
    } else if (debug_mode == RT_TRACE_DEBUG_PRIMITIVE_ID) {
        radiance = hashIdentifier(primitive_index + 1u);
    }

    const float committed_distance = query.CommittedRayT();
    g_raw_reflection[output_pixel] = float4(radiance, confidence);
    g_hit_distance[output_pixel] = float4(
        isfinite(committed_distance) ? max(committed_distance, 0.0f) : 0.0f,
        0.0f,
        0.0f,
        0.0f);
}
