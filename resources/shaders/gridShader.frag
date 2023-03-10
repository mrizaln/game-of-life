#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec3 uColor;

void main()
{
    vec4 textureColor = texture(uTex, TexCoords);
    if (textureColor.a < 0.1f)
        discard;
    FragColor = vec4(textureColor.rgb * uColor, textureColor.a);
}
