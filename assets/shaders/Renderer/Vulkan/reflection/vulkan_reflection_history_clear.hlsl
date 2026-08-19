[[vk::binding(0, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> g_target;

struct ReflectionHistoryClearPushConstants {
    uint2 extent;
    float2 padding;
    float4 clear_value;
};

[[vk::push_constant]]
ReflectionHistoryClearPushConstants g_clear;

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID) {
    if (dispatch_id.x >= g_clear.extent.x || dispatch_id.y >= g_clear.extent.y) {
        return;
    }

    g_target[dispatch_id.xy] = g_clear.clear_value;
}
