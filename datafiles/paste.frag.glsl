#version 450 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler2D srcfb;

void main() {
	fragColor = texture(srcfb, uv);
}
