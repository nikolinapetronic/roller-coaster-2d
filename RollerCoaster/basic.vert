#version 330 core

layout(location = 0) in vec2 inPos;  // pozicija verteksa
layout(location = 1) in vec2 aTex;  // koordinate teksture

out vec2 TexCoord;                  // proslijedicemo u fragment shader

uniform vec2 uOffset;               // pomjeraj

void main()
{
    gl_Position = vec4(inPos.xy + uOffset, 0.0, 1.0);
    TexCoord = aTex;
}