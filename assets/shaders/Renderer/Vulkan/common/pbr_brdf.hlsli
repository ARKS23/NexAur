#ifndef NEXAUR_PBR_BRDF_HLSLI
#define NEXAUR_PBR_BRDF_HLSLI

static const float NX_PI = 3.14159265359f;

float NxDistributionGGX(float3 normal, float3 half_vector, float roughness) {
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float ndoth = max(dot(normal, half_vector), 0.0f);
    const float ndoth2 = ndoth * ndoth;

    float denominator = ndoth2 * (a2 - 1.0f) + 1.0f;
    denominator = NX_PI * denominator * denominator;
    return a2 / max(denominator, 0.000001f);
}

float NxGeometrySchlickGGX(float ndotv, float roughness) {
    const float r = roughness + 1.0f;
    const float k = (r * r) / 8.0f;
    return ndotv / max(ndotv * (1.0f - k) + k, 0.000001f);
}

float NxGeometrySmith(float3 normal, float3 view_dir, float3 light_dir, float roughness) {
    const float ndotv = max(dot(normal, view_dir), 0.0f);
    const float ndotl = max(dot(normal, light_dir), 0.0f);
    return NxGeometrySchlickGGX(ndotv, roughness) * NxGeometrySchlickGGX(ndotl, roughness);
}

float3 NxFresnelSchlick(float cos_theta, float3 f0) {
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

float3 NxFresnelSchlickRoughness(float cos_theta, float3 f0, float roughness) {
    const float3 grazing = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0);
    return f0 + (grazing - f0) * pow(saturate(1.0f - cos_theta), 5.0f);
}

float3 NxEvaluateDirectLight(
    float3 base_color,
    float metallic,
    float roughness,
    float3 normal,
    float3 view_dir,
    float3 light_dir,
    float3 radiance) {
    const float3 half_vector = normalize(view_dir + light_dir);
    const float ndotl = max(dot(normal, light_dir), 0.0f);
    if (ndotl <= 0.0f) {
        return 0.0f;
    }

    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), base_color, metallic);
    const float normal_distribution = NxDistributionGGX(normal, half_vector, roughness);
    const float geometry = NxGeometrySmith(normal, view_dir, light_dir, roughness);
    const float3 fresnel = NxFresnelSchlick(max(dot(half_vector, view_dir), 0.0f), f0);

    const float3 numerator = normal_distribution * geometry * fresnel;
    const float denominator = max(4.0f * max(dot(normal, view_dir), 0.0f) * ndotl, 0.000001f);
    const float3 specular = numerator / denominator;

    const float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * base_color / NX_PI;
    return (diffuse + specular) * radiance * ndotl;
}

#endif
