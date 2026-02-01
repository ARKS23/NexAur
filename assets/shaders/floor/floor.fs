#version 460 core

out vec4 FragColor;

in vec3 v_Normal;
in vec2 v_TexCoord;

uniform sampler2D u_DiffuseMap;

void main() {
    FragColor = texture(u_DiffuseMap, v_TexCoord);
}