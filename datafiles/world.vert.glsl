//ISB's original screen flip shader
#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 uvlIn;
layout(location = 2) in ivec4 propsIn;

layout(location = 0) out vec3 uvl;
layout(location = 1) out ivec4 props; //segnum, texnum1, texnum2, overlay rotation

layout(set = 1, binding = 0) uniform animUBO {
	mat4 animation;
};
layout(set = 1, binding = 1) uniform modelUBO {
	mat4 model;
};
layout(set = 1, binding = 2) uniform viewUBO {
	mat4 view;
};
layout(set = 1, binding = 3) uniform projUBO {
	mat4 projection;
};

const mat4 coordinateScale = mat4(
	0.1, 0.0, 0.0, 0.0,
	0.0, 0.1, 0.0, 0.0,
	0.0, 0.0, 0.1, 0.0,
	0.0, 0.0, 0.0, 0.1
);

void main()
{
	vec4 pos = view[3];
	mat4 viewRotation = view;
	viewRotation[3] = vec4(0,0,0,1);
	
	mat4 viewTranslation = mat4(1);
	viewTranslation[3] = pos;
	
	gl_Position = projection * viewRotation * viewTranslation * model * animation * vec4(position, 1.0);
	uvl = uvlIn;
	props = propsIn;
}
