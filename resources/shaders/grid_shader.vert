#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_texCoords;
// layout (location = 2) in vec3 aNormal;

// out vec3 Normal;
out vec2 TexCoords;
out vec3 FragPos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * u_view * u_model * vec4(a_pos, 1.0);

    // remove the effect of wrongly scaling the normal vectors
    // Normal = mat3(transpose(inverse(u_model))) * aNormal;

    TexCoords = a_texCoords;

    FragPos = vec3(u_model * vec4(a_pos, 1));
}
