#version 330 core
layout (location = 0) in vec2 aPos;   

uniform mat4 uMVP;
uniform vec2 uPos;   
uniform vec2 uSize; 

void main()
{
    vec2 p = uPos + aPos * uSize;
    gl_Position = uMVP * vec4(p.xy, 0.0, 1.0);
}       