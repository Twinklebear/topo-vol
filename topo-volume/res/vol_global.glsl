#include "view_info.glsl"

layout(std140, binding = 1) uniform VolProps {
	mat4 vol_transform;
	mat4 vol_inv_transform;
	vec3 vol_dim;
	// Scale and bias to transform the volume values from
	// [vol_min, vol_max] to [0, 1] for reading from the palette
	vec2 scale_bias;
};

