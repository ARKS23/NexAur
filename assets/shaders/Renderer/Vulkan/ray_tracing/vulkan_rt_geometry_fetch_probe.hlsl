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

[[vk::binding(2, 0)]]
StructuredBuffer<GpuRtGeometryRecord> g_geometry_table;

[[vk::binding(6, 0)]]
ByteAddressBuffer g_vertex_buffers[];

[[vk::binding(7, 0)]]
ByteAddressBuffer g_index_buffers[];

struct GeometryFetchProbeResult {
    uint index0;
    uint index1;
    uint index2;
    uint valid;
};

[[vk::binding(8, 0)]]
RWStructuredBuffer<GeometryFetchProbeResult> g_probe_result;

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID) {
    const GpuRtGeometryRecord geometry = g_geometry_table[dispatch_id.x];
    GeometryFetchProbeResult result = (GeometryFetchProbeResult)0;
    if (geometry.vertex_stride < 12u ||
        geometry.index_count < 3u ||
        geometry.index_count % 3u != 0u) {
        g_probe_result[dispatch_id.x] = result;
        return;
    }

    ByteAddressBuffer index_buffer =
        g_index_buffers[NonUniformResourceIndex(geometry.index_buffer_index)];
    ByteAddressBuffer vertex_buffer =
        g_vertex_buffers[NonUniformResourceIndex(geometry.vertex_buffer_index)];
    result.index0 = index_buffer.Load(0u);
    result.index1 = index_buffer.Load(4u);
    result.index2 = index_buffer.Load(8u);
    const uint vertex_offset = result.index0 * geometry.vertex_stride;
    const float3 vertex_position = asfloat(vertex_buffer.Load3(vertex_offset));
    result.valid = result.index0 < geometry.vertex_count &&
        result.index1 < geometry.vertex_count &&
        result.index2 < geometry.vertex_count &&
        all(isfinite(vertex_position)) ? 1u : 0u;
    g_probe_result[dispatch_id.x] = result;
}
