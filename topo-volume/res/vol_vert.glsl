#version 430 core

#include "vol_global.glsl"

layout(location = 0) in vec3 pos;

out vec3 vray_dir;
out vec3 fpos;
flat out vec3 transformed_eye;

void main(void){
	gl_Position = proj * view * vol_transform * vec4(pos, 1);
	transformed_eye = (vol_inv_transform * vec4(eye_pos, 1)).xyz;
	vray_dir = pos - transformed_eye;
	fpos = gl_Position.xyz;
}

