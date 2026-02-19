#version 330 core

layout (location = 0) in vec2 aPoint;

void main()
{
    gl_Position = vec4(aPoint.x, aPoint.y, 0.0, 1.0);
}
