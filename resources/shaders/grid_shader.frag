#version 330 core

in vec2  TexCoords;
out vec4 FragColor;

uniform sampler2D u_tex;
uniform vec3      u_color;

void main()
{
    vec4 textureColor = texture(u_tex, TexCoords);
    if (textureColor.a < 0.1f) {
        discard;
    }
    FragColor = vec4(textureColor.rgb * u_color, textureColor.a);
}
