#version 330 core

out vec4 o_frag_colour;

uniform vec4 u_colour;

void main()
{
    o_frag_colour = u_colour;
} 
