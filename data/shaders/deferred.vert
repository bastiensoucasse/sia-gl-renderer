#version 410 core

in vec4 vtx_position;

void main()
{
    gl_Position = vec4(vtx_position.x * 2 - 1, vtx_position.y * 2 - 1, 0, 1);
}