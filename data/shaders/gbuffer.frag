#version 410 core

uniform float specular_coef;

in vec3 colorV;
in vec3 normalV;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;

void main()
{
    out_color = vec4(colorV, specular_coef);
    out_normal = vec4(normalize(normalV), gl_FragCoord.z);
}
