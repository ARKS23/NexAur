#version 450 core

out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_FragPosLightSpace; // 光源空间位置

struct Material {
    sampler2D diffuse;  // 漫反射贴图
    sampler2D specular; // 高光贴图
    float shininess;    // 反光度
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

// 最大点光源数量
#define MAX_POINT_LIGHTS 4

// uniform变量
uniform vec3 u_CameraPos;
uniform Material u_Material;
uniform DirLight u_DirLight;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform vec3 u_AmbientLight;
uniform sampler2D u_ShadowMap; // 阴影贴图
uniform float u_Exposure = 0.52; // 曝光控制

// 控制实际启用的点光源数量
uniform int u_NumPointLights;

// 函数前向声明
vec3 CalculateDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir);

void main() {
    // 归一化输入的法线和视线方向
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);

    // 1. 计算全局环境光 (Ambient)
    vec3 albedo = pow(vec3(texture(u_Material.diffuse, v_TexCoord)), vec3(2.2)); // 从漫反射贴图获取基础颜色
    vec3 ambient = u_AmbientLight * albedo;
    
    // 2. 累加所有光源的直接光照
    vec3 result = ambient;
    
    // 计算方向光
    vec3 dirLightResult = CalculateDirLight(u_DirLight, norm, viewDir);

    // 计算阴影
    float shadow = ShadowCalculation(v_FragPosLightSpace, norm, normalize(-u_DirLight.direction));
    
    // 将阴影应用到方向光结果中
    result += (1.0 - shadow) * dirLightResult;

    // 计算点光源
    int pointLightCount = min(u_NumPointLights, MAX_POINT_LIGHTS);
    for(int i = 0; i < pointLightCount; i++) {
        result += CalculatePointLight(u_PointLights[i], norm, v_FragPos, viewDir);    
    }

    // HDR色调映射和Gamma校正
    vec3 mapped = vec3(1.0) - exp(-result * u_Exposure); // 色调映射
    mapped = pow(mapped, vec3(1.0/2.2)); // Gamma    
    
    FragColor = vec4(mapped, 1.0);
}

vec3 CalculateDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction); // 定向光的光线方向
    
    // 光源到达表面的总能量
    vec3 radiance = light.color * light.intensity;

    // 漫反射 (Diffuse)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 texColor = pow(vec3(texture(u_Material.diffuse, v_TexCoord)), vec3(2.2));
    vec3 diffuse = diff * radiance * texColor;
    
    // 镜面高光 (Specular)
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Material.shininess);
    vec3 specular = spec * radiance * vec3(texture(u_Material.specular, v_TexCoord));
    
    return (diffuse + specular);
}

vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // 计算衰减
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // 光源到达表面的总能量 (考虑了距离衰减)
    vec3 radiance = light.color * light.intensity * attenuation;
    
    // 漫反射
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 texColor = pow(vec3(texture(u_Material.diffuse, v_TexCoord)), vec3(2.2));
    vec3 diffuse = diff * radiance * texColor;
    
    // 镜面高光
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Material.shininess);
    vec3 specular = spec * radiance * vec3(texture(u_Material.specular, v_TexCoord));
    
    return (diffuse + specular);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // 1. 透视除法，转换到 NDC 坐标 (-1 到 1)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // 2. 变换到 0 到 1 的范围，用于UV采样和深度比较
    projCoords = projCoords * 0.5 + 0.5;
    
    // 3. 当前片段的深度
    float currentDepth = projCoords.z;
    
    // 4. 解决阴影失真(Shadow Acne)的偏移量(Bias)
    // 表面角度越陡，偏移量需要越大
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    // 5. PCF软阴影采样
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    float sampleRadius = 1.3;  // 采样半径
    // 对周围 5x5 个像素进行采样求平均
    for(int x = -2; x <= 2; ++x) {
        for(int y = -2; y <= 2; ++y) {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize * sampleRadius).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 25.0;
    
    // 6. 强制处理视锥体远平面之外的区域（超出直射光范围的算作无阴影）
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}