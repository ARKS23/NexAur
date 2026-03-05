#version 450 core

out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

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

// 控制实际启用的点光源数量
uniform int u_NumPointLights;

// 函数前向声明
vec3 CalculateDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {
    // 归一化输入的法线和视线方向
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);

    // 1. 计算全局环境光 (Ambient)
    vec3 ambient = u_AmbientLight * vec3(texture(u_Material.diffuse, v_TexCoord));
    
    // 2. 累加所有光源的直接光照
    vec3 result = ambient;
    
    // 计算方向光
    result += CalculateDirLight(u_DirLight, norm, viewDir);
    
    // 计算点光源
    int pointLightCount = min(u_NumPointLights, MAX_POINT_LIGHTS);
    for(int i = 0; i < pointLightCount; i++) {
        result += CalculatePointLight(u_PointLights[i], norm, v_FragPos, viewDir);    
    }    
    
    FragColor = vec4(result, 1.0);
}

vec3 CalculateDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction); // 定向光的光线方向
    
    // 光源到达表面的总能量
    vec3 radiance = light.color * light.intensity;

    // 漫反射 (Diffuse)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * radiance * vec3(texture(u_Material.diffuse, v_TexCoord));
    
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
    vec3 diffuse = diff * radiance * vec3(texture(u_Material.diffuse, v_TexCoord));
    
    // 镜面高光
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Material.shininess);
    vec3 specular = spec * radiance * vec3(texture(u_Material.specular, v_TexCoord));
    
    return (diffuse + specular);
}