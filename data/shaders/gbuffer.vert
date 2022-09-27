#version 410 core

uniform mat4 projection_matrix;
uniform mat4 view_matrix;
uniform mat4 model_matrix;
uniform mat3 normal_matrix;

in vec4 vtx_position;
in vec3 vtx_color;
in vec3 vtx_normal;

out vec3 colorV;
out vec3 normalV;

void main()
{
    vec4 view_vtx = view_matrix * model_matrix * vtx_position;

    colorV = vtx_color;
    normalV = normalize(normal_matrix * vtx_normal);

    gl_Position = projection_matrix * view_vtx;
}
