#version 450 core

layout(location = 0) in vec3 uvl;
layout(location = 1) in flat ivec4 props; //segnum, texnum1, texnum2, overlay rotation

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

	//float z = -gl_FragCoord.z / gl_FragCoord.w;
  float z = sqrt(-gl_FragCoord.z);
	//float z = gl_FragCoord.z * gl_FragCoord.z;
  if (isnan(z) || isinf(z)) {
    fragColor = vec4(1,1,0,1);
    return;
  }
    

	if (z <= 0 || z >= 1) {
		fragColor = vec4(0, z, 1-z, 1);
		//fragColor = vec4(0, gl_FragCoord.z, 0, 1);
		return;
	}

	fragColor = vec4(1,0,z,1);
	return;

	/*

	uint cindex;
	
	vec2 textureSize;
	vec2 tindex;
	vec2 uvInTex;
	vec2 uv;
	
	if (props.z >= 0) {
	
		textureSize = 1 / numTextures.zw;
		tindex = vec2(mod(props.z, numTextures.z), floor(props.z / numTextures.z));
		uvInTex = vec2(tindex.x + fract(uvl.x), tindex.y + fract(uvl.y));
		
		switch (props.w) {
			case 0: break;
			case 1: uvInTex = vec2(-uvInTex.y, uvInTex.x); break;
			case 2: uvInTex = -uvInTex; break;
			case 3: uvInTex = vec2(uvInTex.y, -uvInTex.x); break;
		}
		
		uv = textureSize * uvInTex + tindex;
		cindex = uint(texture(topTexpage, uv).r);
		if (cindex == 254)
			discard;
		else if (cindex < 254) {
			paletteColor color = palette[cindex];
			fragColor = vec4(color.r, color.g, color.b, 1) * uvl.z;
			return;
		}
		
	}
	
	textureSize = 1 / numTextures.xy;
	tindex = vec2(mod(props.y, numTextures.x), floor(props.y / numTextures.x));
	uvInTex = vec2(tindex.x + fract(uvl.x), tindex.y + fract(uvl.y));
	uv = textureSize * uvInTex + tindex;

	cindex = uint(texture(botTexpage, uv).r);
	if (cindex >= 254)
		discard;
		
	paletteColor color = palette[cindex];
	fragColor = vec4(color.r, color.g, color.b, 1) * uvl.z;
	
	*/

}





	/*if ((props.w & 1) != 0)
		uvTop = vec2(-uvTop.y, uvTop.x);
	if ((props.w & 2) != 0)
		uvTop = -uvTop;*/