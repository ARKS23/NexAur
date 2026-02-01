#version 460 core

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec2 TexCoords;

layout(std140, binding = 0) uniform Camera {
                mat4 u_ViewProjection;
};

uniform mat4 u_Transform;

void main() {
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Pos, 1.0);
    TexCoords = a_TexCoord;
}
