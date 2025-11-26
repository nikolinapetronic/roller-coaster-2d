#version 330 core

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uTex;  // tekstura koju crtamo

void main()
{
    FragColor = texture(uTex, TexCoord);
}