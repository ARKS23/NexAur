#version 450 core

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

uniform mat4 u_Transform;
uniform mat4 u_LightSpaceMatrix; // 光源空间矩阵

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_FragPosLightSpace; // 光源空间位置

void main() {
    // 计算世界坐标，光照距离计算时需要使用到
    v_FragPos = vec3(u_Transform * vec4(a_Pos, 1.0));
    
    // 处理法线形变，后续可以优化到C++中计算逆转置矩阵
    mat3 normalMatrix = transpose(inverse(mat3(u_Transform)));
    v_Normal = normalMatrix * a_Normal;

    // 传递纹理坐标
    v_TexCoord = a_TexCoord;

    // 计算光源空间坐标
    v_FragPosLightSpace = u_LightSpaceMatrix * vec4(v_FragPos, 1.0);

    gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
}