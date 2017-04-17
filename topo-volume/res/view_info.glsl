layout(std140, binding = 0) uniform Viewing {
	mat4 proj, view;
	// Note that these will be padded out to be vec4's
	vec3 eye_pos;
};

