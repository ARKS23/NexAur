#version 450 core

out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_FragPosLightSpace; // 光源空间位置

struct Material {
    sampler2D albedoMap;
    sampler2D normalMap;
    sampler2D metallicMap;
    sampler2D roughnessMap;
    sampler2D aoMap;
};

// 定向光源
struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

// 点光源
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;

    float constant;     // 衰减常数项
    float linear;       // 衰减线性项
    float quadratic;    // 衰减二次项
};

#define MAX_POINT_LIGHTS 4  // 最大点光源数量
const float PI = 3.14159265359;
uniform int u_NumPointLights; // 控制实际启用的点光源数量

// uniform变量
uniform vec3 u_CameraPos;
uniform Material u_Material;
uniform DirLight u_DirLight;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform sampler2D u_ShadowMap; // 阴影贴图
uniform float u_Exposure = 0.55; // 曝光控制
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D   u_BrdfLUT;
uniform int u_SkyboxEnabled = 0; // 是否启用天空盒

// 余切坐标系法线计算函数
vec3 getNormalFromMap() {
    // 从法线贴图采样并转换到 [-1, 1]
    vec3 tangentNormal = texture(u_Material.normalMap, v_TexCoord).xyz * 2.0 - 1.0;

    // 利用微积分指令获取位置和UV在屏幕空间的像素级变化率
    vec3 Q1  = dFdx(v_FragPos);
    vec3 Q2  = dFdy(v_FragPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);

    // 计算切线 T 和副切线 B
    vec3 N   = normalize(v_Normal); // 来自顶点着色器的世界法线
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    
    // 构建 TBN 矩阵
    mat3 TBN = mat3(T, B, N);

    // 将切线空间法线转换到世界空间
    return normalize(TBN * tangentNormal);
}

// 阴影计算
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    float sampleRadius = 1.3;  
    for(int x = -2; x <= 2; ++x) {
        for(int y = -2; y <= 2; ++y) {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize * sampleRadius).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 25.0;
    if(projCoords.z > 1.0) shadow = 0.0;
        
    return shadow;
}

// =============================== BRDF ===============================
// 法线分布函数D (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // 避免除以0
}

// 几何遮蔽函数G (Smith)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 菲涅尔方程F (Fresnel-Schlick)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 环境光的菲涅尔
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ======================= 光源计算封装 =======================
// 计算定向光 PBR
vec3 CalculateDirLightPBR(DirLight light, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0) {
    vec3 L = normalize(-light.direction);
    vec3 H = normalize(V + L);
    vec3 radiance = light.color * light.intensity;

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       

    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; 

    float NdotL = max(dot(N, L), 0.0);        
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// 计算点光源 PBR
vec3 CalculatePointLightPBR(PointLight light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float roughness, float metallic, vec3 F0) {
    vec3 L = normalize(light.position - fragPos);
    vec3 H = normalize(V + L);
    
    // 距离衰减
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    vec3 radiance = light.color * light.intensity * attenuation;

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       

    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; 

    float NdotL = max(dot(N, L), 0.0);        
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // 1. 采样并解析材质属性
    vec3 albedo     = pow(texture(u_Material.albedoMap, v_TexCoord).rgb, vec3(2.2));
    float metallic  = texture(u_Material.metallicMap, v_TexCoord).r;
    float roughness = clamp(texture(u_Material.roughnessMap, v_TexCoord).r, 0.04, 1.0); // 防止高光走样
    float ao        = texture(u_Material.aoMap, v_TexCoord).r;

    // 2. 准备基础向量数据
    vec3 N = getNormalFromMap();
    vec3 V = normalize(u_CameraPos - v_FragPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0); // 累加所有直接光的能量
    
    // ===================================
    // 3. 计算定向光 (附带阴影)
    // ===================================
    vec3 dirLightResult = CalculateDirLightPBR(u_DirLight, N, V, albedo, roughness, metallic, F0);
    float shadow = ShadowCalculation(v_FragPosLightSpace, N, normalize(-u_DirLight.direction));
    Lo += dirLightResult * (1.0 - shadow);

    // ===================================
    // 4. 计算所有点光源 (无阴影)
    // ===================================
    int pointLightCount = min(u_NumPointLights, MAX_POINT_LIGHTS);
    for(int i = 0; i < pointLightCount; i++) {
        Lo += CalculatePointLightPBR(u_PointLights[i], N, V, v_FragPos, albedo, roughness, metallic, F0);
    }

    // ===================================
    // 环境光IBL
    // ===================================
    vec3 F_IBL = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;

    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    vec3 R = reflect(-V, N); // 视角碰到法线后的反射向量
    // 根据粗糙度在 0 到 4.0 级的 Mipmap 中进行三线性插值采样
    const float MAX_REFLECTION_LOD = 4.0; 
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;

    // 结合 BRDF LUT
    vec2 envBRDF  = texture(u_BrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD_IBL * diffuse + specular) * ao; 

    // 天空盒影响IBL
    if (u_SkyboxEnabled == 0) {
        ambient = 0.35 * albedo * ao;
    }

    // 颜色合成
    vec3 color = ambient + Lo;

    // HDR 色调映射 (曝光) 和 Gamma 校正
    color = vec3(1.0) - exp(-color * u_Exposure); 
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}