#version 450 core
out vec4 FragColor;

in vec3 v_LocalPos;

uniform samplerCube u_EnvironmentMap;

const float PI = 3.14159265359;

void main() {		
    // v_LocalPos 是在半球上积分的法线方向
    vec3 normal = normalize(v_LocalPos);
  
    vec3 irradiance = vec3(0.0);  
    
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up         = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0; 

    // 在法线对应的半球范围内进行球面黎曼积分
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // 球面坐标转笛卡尔坐标 (切线空间)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // 切线空间转世界空间
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

            irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    FragColor = vec4(irradiance, 1.0);
}