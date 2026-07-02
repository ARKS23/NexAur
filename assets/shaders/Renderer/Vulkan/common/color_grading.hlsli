float3 NxApplyWhiteBalance(float3 color, float temperature, float tint) {
    const float safe_temperature = clamp(temperature, -1.0f, 1.0f);
    const float safe_tint = clamp(tint, -1.0f, 1.0f);
    const float3 balance = float3(
        1.0f + 0.12f * safe_temperature + 0.05f * safe_tint,
        1.0f - 0.08f * safe_tint,
        1.0f - 0.12f * safe_temperature + 0.05f * safe_tint);
    return color * max(balance, 0.0f);
}

float3 NxApplyContrast(float3 color, float contrast) {
    return (color - 0.5f) * max(contrast, 0.0f) + 0.5f;
}

float3 NxApplySaturation(float3 color, float saturation) {
    const float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    return lerp(float3(luminance, luminance, luminance), color, max(saturation, 0.0f));
}

float3 NxApplyBlackWhitePoint(float3 color, float black_point, float white_point) {
    const float safe_black = clamp(black_point, 0.0f, 0.95f);
    const float safe_white = max(white_point, safe_black + 0.001f);
    return saturate((color - safe_black) / (safe_white - safe_black));
}

float3 NxApplyVignette(
    float3 color,
    float2 uv,
    float intensity,
    float radius,
    float softness) {
    const float safe_intensity = saturate(intensity);
    if (safe_intensity <= 0.0f) {
        return color;
    }

    const float safe_radius = saturate(radius);
    const float safe_softness = max(softness, 0.001f);
    const float distance_to_center = distance(uv, float2(0.5f, 0.5f)) * 1.41421356f;
    const float edge_weight = smoothstep(safe_radius, safe_radius + safe_softness, distance_to_center);
    return color * (1.0f - safe_intensity * edge_weight);
}

float3 NxApplyColorGrading(
    float3 color,
    float2 uv,
    uint enabled,
    float contrast,
    float saturation,
    float temperature,
    float tint,
    float black_point,
    float white_point,
    float vignette_intensity,
    float vignette_radius,
    float vignette_softness) {
    if (enabled == 0u) {
        return saturate(color);
    }

    color = NxApplyBlackWhitePoint(color, black_point, white_point);
    color = NxApplyWhiteBalance(color, temperature, tint);
    color = NxApplyContrast(color, contrast);
    color = NxApplySaturation(color, saturation);
    color = NxApplyVignette(color, uv, vignette_intensity, vignette_radius, vignette_softness);
    return saturate(color);
}
