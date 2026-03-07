#version 450 core
out vec4 FragColor;

in vec3 v_LocalPos;

uniform sampler2D u_EquirectangularMap; // 你用stb_image加载的高清HDR矩形图

const vec2 invAtan = vec2(0.1591, 0.3183);

// 球面坐标转UV公式
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {		
    vec2 uv = SampleSphericalMap(normalize(v_LocalPos)); // 将3D方向转为2D UV
    vec3 color = texture(u_EquirectangularMap, uv).rgb;
    
    FragColor = vec4(color, 1.0);
}