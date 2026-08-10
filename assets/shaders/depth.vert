#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 4) in mat4 aInstanceMatrix;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform bool uUseInstancing;

void main()
{
    mat4 world = uUseInstancing ? aInstanceMatrix : model;
    gl_Position = lightSpaceMatrix * world * vec4(aPos, 1.0);
}
