#version 460 core

in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube u_Skybox;
uniform float u_Exposure = 0.52;

void main() {
    // 获取环境光真实的线性颜色
    vec3 envColor = texture(u_Skybox, TexCoords).rgb;
    
    // HDR 色调映射
    envColor = vec3(1.0) - exp(-envColor * u_Exposure); 
    
    // Gamma 校正
    envColor = pow(envColor, vec3(1.0/2.2));
    
    FragColor = vec4(envColor, 1.0);
}