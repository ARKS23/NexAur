struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct FrameGlobals {
    float4x4 view_projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameGlobals> g_frame;

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(g_frame.view_projection, float4(input.position, 1.0f));
    output.color = input.color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0 {
    return input.color;
}
