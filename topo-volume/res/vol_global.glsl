#include "view_info.glsl"

layout(std140, binding = 1) uniform VolProps {
	mat4 vol_transform;
	mat4 vol_inv_transform;
	vec3 vol_dim;
	// Scale and bias to transform the volume values from
	// [vol_min, vol_max] to [0, 1] for reading from the palette
	vec2 scale_bias;
};

layout(std430, binding = 2) buffer ChosenSegmentations {
	int num_segmentations;
	// A segment is marked with a 1 if it's selected, 0 if not.
	// The palette is which palette in the palette array the segment should use.
	// the buffer is interleaved [segment selected, segment palette,...]
	int segment_selections[];
};
	

