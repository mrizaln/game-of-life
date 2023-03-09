#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uTex;
uniform sampler2D uState;
uniform vec3 uColor;

uniform int uLength;
uniform int uWidth;

void main()
{
    // point lights
    vec4 textureColor = texture(uTex, TexCoords) * texture(uState, TexCoords/vec2(uLength, uWidth));
    // vec4 textureColor = texture(state, TexCoords);
    if (textureColor.a < 0.1f)
        discard;
    FragColor = vec4(textureColor.rgb * uColor, textureColor.a);
    //------------------------
}
