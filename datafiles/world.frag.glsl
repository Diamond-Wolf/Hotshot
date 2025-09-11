#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 1) in flat ivec4 props; //segnum, texnum1, texnum2, overlay rotation
layout(location = 2) in vec4 colormod;

layout(location = 0) out vec4 fragColor;

struct paletteColor {
	float r;
	float g;
	float b;
};

layout(set = 2, binding = 0) uniform sampler2D topTexpage;
layout(set = 2, binding = 1) uniform sampler2D botTexpage;

layout(std140, set = 2, binding = 2) readonly buffer paletteBuffer {
	paletteColor palette[];
};
// TODO Need portal list

layout(set = 3, binding = 0) uniform textureUBO {
	vec4 numTextures; //xy = bottom, zw = top
};

void main() {

	// TODO Check portals

	uint cindex;
	
	vec2 textureSize;
	vec2 tindex;
	vec2 uvInTex;
	vec2 finalUV;
	
	if (props.z >= 0) {
	
		textureSize = 1 / numTextures.zw;
		tindex = vec2(mod(props.z, numTextures.z), floor(props.z / numTextures.z));
		uvInTex = vec2(tindex.x + fract(uv.x), tindex.y + fract(uv.y));
		
		switch (props.w) {
			case 0: break;
			case 1: uvInTex = vec2(-uvInTex.y, uvInTex.x); break;
			case 2: uvInTex = -uvInTex; break;
			case 3: uvInTex = vec2(uvInTex.y, -uvInTex.x); break;
		}
		
		finalUV = textureSize * uvInTex + tindex;
		cindex = uint(texture(topTexpage, finalUV).r);
		if (cindex == 254)
			discard;
		else if (cindex < 254) {
			paletteColor color = palette[cindex];
			fragColor = vec4(color.r * colormod.r, color.g * colormod.g, color.b * colormod.b, colormod.a);
			return;
		}
		
	}
	
	if (props.y >= 0) {
	
		textureSize = 1 / numTextures.xy;
		tindex = vec2(mod(props.y, numTextures.x), floor(props.y / numTextures.x));
		uvInTex = vec2(tindex.x + fract(uv.x), tindex.y + fract(uv.y));
		finalUV = textureSize * uvInTex + tindex;

		cindex = uint(texture(botTexpage, finalUV).r);
		if (cindex >= 254)
			discard;
		
		paletteColor color = palette[cindex];
		fragColor = vec4(color.r * colormod.r, color.g * colormod.g, color.b * colormod.b, colormod.a);
	
	} else {
		fragColor = colormod;
	}
		
}
