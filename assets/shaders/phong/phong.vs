#version 450 core

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

uniform mat4 u_Transform;

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    // 计算世界坐标，光照距离计算时需要使用到
    v_FragPos = vec3(u_Transform * vec4(a_Pos, 1.0));
    
    // 处理法线形变，后续可以优化到C++中计算逆转置矩阵
    mat3 normalMatrix = transpose(inverse(mat3(u_Transform)));
    v_Normal = normalMatrix * a_Normal;

    // 传递纹理坐标
    v_TexCoord = a_TexCoord;

    gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
}