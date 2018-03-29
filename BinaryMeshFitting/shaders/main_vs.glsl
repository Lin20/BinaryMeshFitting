#version 400 core

attribute vec3 vertex_position;
attribute vec3 vertex_normal;
attribute vec3 vertex_color;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 mul_color;
uniform float smooth_shading;
uniform float specular_power;
uniform vec3 camera_pos;
uniform vec4 chunk_pos;
uniform float chunk_depth;

out vec3 f_normal;
out vec3 f_color;
out vec3 f_mul_color;
out vec3 f_ec_pos;
out float f_smooth_shading;
out float f_specular_power;
out float log_z;
out vec4 f_world_pos;
out float f_chunk_depth;

void main()
{
	f_normal = normalize(vertex_normal);
	f_color = vertex_normal;
	f_mul_color = mul_color;
	f_smooth_shading = smooth_shading;
	f_specular_power = specular_power;
	f_ec_pos = vertex_position;

	vec3 world_pos = vertex_position * chunk_pos.w + chunk_pos.xyz;
	
	gl_Position = projection * view * vec4(world_pos - camera_pos, 1);
	f_world_pos = vec4(world_pos, chunk_pos.w);
	f_chunk_depth = chunk_depth;

	const float far = 1000.0;
	const float C = 0.01;
	const float FC = 1.0f / log(far * C + 1.0);
	log_z = log(gl_Position.w * C + 1.0) * FC;
	gl_Position.z = (2.0 * log_z - 1.0) * gl_Position.w;
}