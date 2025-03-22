//ISB's original screen flip shader
#version 450 core

layout(location=0) in vec2 point;

layout(location=0) out vec2 uv;

void main()
{
	gl_Position = vec4(point.x, point.y, 0.5, 1.0);
	uv = point / 2 + vec2(0.5, 0.5);
	uv.y = 1 - uv.y;
}
