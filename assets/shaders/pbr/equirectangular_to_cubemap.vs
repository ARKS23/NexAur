#version 450 core
layout (location = 0) in vec3 a_Pos;

out vec3 v_LocalPos; // 传出局部坐标，作为采样方向向量

uniform mat4 u_ViewProjection;

void main() {
    v_LocalPos = a_Pos;
    // 强制投影，不发生世界位移
    gl_Position = u_ViewProjection * vec4(v_LocalPos, 1.0);
}