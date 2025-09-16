#version 450 core

layout(location=0) in vec2 point;

void main()
{
	gl_Position = vec4(point.x, point.y, 0.5, 1.0);
}
