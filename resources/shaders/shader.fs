#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D tex;
uniform vec3 color;

void main()
{
    // point lights
    vec4 textureColor = texture(tex, TexCoords);
    if (textureColor.a < 0.1f)
        discard;
    FragColor = vec4(textureColor.rgb * color, textureColor.a);
    //------------------------
}
