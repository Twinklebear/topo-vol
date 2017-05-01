#version 430 core

#include "vol_global.glsl"

uniform sampler3D volume;
uniform isampler3D ivolume;
uniform sampler1DArray palette;
uniform isampler3D segmentation_volume;

uniform bool has_segmentation_volume;
uniform bool isosurface;
uniform float isovalue;
uniform bool int_texture;

in vec3 vray_dir;
flat in vec3 transformed_eye;

out vec4 color;

bool segment_selected(vec3 p) {
	if (has_segmentation_volume) {
		const int segment = texture(segmentation_volume, p).r;
		return segment_selections[segment] != 0;
	}
	return true;
}

float value(vec3 p) {
	if (!int_texture) {
		return scale_bias.x * texture(volume, p).r + scale_bias.y;
	} else {
		return scale_bias.x * texture(ivolume, p).r + scale_bias.y;
	}
}

vec3 grad(vec3 p, float dt) {
	vec2 h = vec2(dt, 0.0);
	return vec3(value(p + h.xyy) - value(p - h.xyy),
		value(p + h.yxy) - value(p - h.yxy),
		value(p + h.yyx) - value(p - h.yyx)) / (2.0*h.x);
}

void main(void){
	vec3 ray_dir = normalize(vray_dir);
	vec3 light_dir = ray_dir;
	vec3 inv_dir = 1.0 / ray_dir;
	// Check for intersection against the bounding box of the volume
	vec3 box_max = vec3(1);
	vec3 box_min = vec3(0);
	vec3 tmin_tmp = (box_min - transformed_eye) * inv_dir;
	vec3 tmax_tmp = (box_max - transformed_eye) * inv_dir;
	vec3 tmin = min(tmin_tmp, tmax_tmp);
	vec3 tmax = max(tmin_tmp, tmax_tmp);
	float tenter = max(0, max(tmin.x, max(tmin.y, tmin.z)));
	float texit = min(tmax.x, min(tmax.y, tmax.z));
	if (tenter > texit){
		discard;
	}

	color = vec4(0);
	vec3 dt_vec = 1.0 / (vol_dim * abs(ray_dir));
	float dt = min(dt_vec.x, min(dt_vec.y, dt_vec.z)) * 0.25;
	vec3 p = transformed_eye + tenter * ray_dir;

	float prev;
	vec3 p_prev;
	for (float t = tenter; t < texit; t += dt){
		if (segment_selected(p)) {
			float palette_sample = value(p);
			vec4 color_sample = texture(palette, vec2(palette_sample, 0));
			color_sample.a *= pow(dt, 0.4);
			color.rgb += (1 - color.a) * color_sample.a * color_sample.rgb;
			color.a += (1 - color.a) * color_sample.a;
			if (color.a >= 0.97) {
				break;
			}
		}
		p += dt * ray_dir;
	}
}
