#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
// layout (location = 2) in vec3 aNormal;

// out vec3 Normal;
out vec2 TexCoords;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;


void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // remove the effect of wrongly scaling the normal vectors
    // Normal = mat3(transpose(inverse(model))) * aNormal;

    TexCoords = aTexCoords;

    FragPos = vec3(model * vec4(aPos, 1));
}
