#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aInstancePos;

uniform mat4 ViewProjection;
uniform vec3 Color;
out vec3 color;

void main()
{
    vec3 worldPosition = aPos + aInstancePos;

    gl_Position = ViewProjection * vec4(worldPosition, 1.0);
    color = Color;
}