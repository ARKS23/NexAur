#version 450 core

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 u_Transform;

void main(){
    gl_Position = u_ViewProjection * u_Transform * vec4(aPos, 1.0);
}