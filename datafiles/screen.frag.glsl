//ISB's original screen flip shader
#version 450 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

struct paletteColor {
	float r;
	float g;
	float b;
};

layout(std140, set = 2, binding = 0) readonly buffer paletteBuffer {
	paletteColor palette[];
};

layout(set = 2, binding = 0) uniform sampler2D srcfb;

void main() {
	uint index = uint(texture(srcfb, uv).r);
	paletteColor color = palette[index];
	fragColor = vec4(color.r, color.g, color.b, 1);
	//fragColor = vec4(index, index / 255.0, 1, 1);
}
