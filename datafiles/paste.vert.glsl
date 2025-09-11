#version 450 core

layout(location=0) in vec2 point;
layout(location=1) in vec2 inUV;

layout(location=0) out vec2 uv;

void main()
{
	gl_Position = vec4(point.x, point.y, 0.5, 1.0);
	uv = inUV;
}
