struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint instance_id : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
    nointerpolation int entity_id : ENTITYID0;
};

struct PushConstants {
    float4x4 view_projection;
    float4x4 model;
};

[[vk::push_constant]]
PushConstants g_push_constants;

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 world_position = mul(g_push_constants.model, float4(input.position, 1.0f));
    output.position = mul(g_push_constants.view_projection, world_position);
    // D11 passes entity id through vkCmdDrawIndexed(firstInstance). Keep DXC's default SV_InstanceID mapping.
    output.entity_id = int(input.instance_id);
    return output;
}

int PSMain(VSOutput input) : SV_Target0 {
    return input.entity_id;
}
