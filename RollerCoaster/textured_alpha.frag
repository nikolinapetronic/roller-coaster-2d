#version 330 core

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uTex;  // tekstura koju crtamo

uniform float uAlpha; // za kontrolu providnosti

void main()
{
    vec4 texColor = texture(uTex, TexCoord);

    // skaliramo alfa kanal 
    texColor.a *= uAlpha;

    FragColor = texColor;
}