#version 450 core
layout (location = 0) in vec3 a_Pos;
layout (location = 2) in vec2 a_TexCoord;

out vec2 TexCoords;

void main() {
    TexCoords = a_TexCoord;
    gl_Position = vec4(a_Pos, 1.0); // 直接输出NDC坐标不使用各种矩阵系
}