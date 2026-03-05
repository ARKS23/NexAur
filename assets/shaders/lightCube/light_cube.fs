#version 450 core

out vec4 FragColor;

uniform vec3 u_Color;

void main() {
    // 仅用于调试查看立方体颜色
    float maxVal = max(u_Color.r, max(u_Color.g, u_Color.b));
    vec3 visualColor = u_Color / maxVal;
    FragColor = vec4(visualColor, 1.0); // // 比如 (33, 24, 250) -> (0.13, 0.09, 1.0)
}