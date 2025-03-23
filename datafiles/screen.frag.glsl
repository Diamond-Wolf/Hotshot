//ISB's original screen flip shader
#version 450 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

struct paletteColor {
	float r;
	float g;
	float b;
};

layout(set = 2, binding = 0) uniform sampler2D srcfb;

layout(std140, set = 2, binding = 1) readonly buffer paletteBuffer {
	paletteColor palette[];
};

void main() {
	uint index = uint(texture(srcfb, uv).r);
	paletteColor color = palette[index];
	fragColor = vec4(color.r, color.g, color.b, 1);
}
