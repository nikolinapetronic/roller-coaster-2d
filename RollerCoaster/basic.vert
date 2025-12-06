#version 330 core

layout(location = 0) in vec2 inPos;  // pozicija verteksa
layout(location = 1) in vec2 aTex;  // koordinate teksture

out vec2 TexCoord;                  // proslijedicemo u fragment shader

uniform vec2 uOffset;               // pomjeraj
uniform float uAngle;               // ugao

void main()
{
    TexCoord = aTex;

    // rotacija
    float s = sin(uAngle);
    float c = cos(uAngle);

    vec2 rotated = vec2(
        inPos.x * c - inPos.y * s,
        inPos.x * s + inPos.y * c
    );

    // translacija
    vec2 finalPos = rotated + uOffset;

    gl_Position = vec4(finalPos, 0.0, 1.0);
}