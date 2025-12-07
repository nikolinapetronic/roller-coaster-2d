#version 330 core

layout(location = 0) in vec2 inPos;  // pozicija tacke pruge

void main()
{
    // tacke su vec date u NDC koordinatama (-1, 1)
    gl_Position = vec4(inPos, 0.0, 1.0);
}