#version 400 core
attribute vec3 vertex_position;
uniform mat4 projection;
uniform mat4 view;
uniform vec3 mul_color;
uniform vec3 camera_pos;
uniform vec3 chunk_pos;

out vec3 f_mul_color;
out float log_z;

void main()
{
	f_mul_color = mul_color;
	gl_Position = projection * view * vec4(vertex_position + chunk_pos - camera_pos, 1);
	const float near = 0.00001;
	const float far = 10000.0;
	const float C = 0.001;
	const float FC = 1.0 / log(far * C + 1.0);
	log_z = log(gl_Position.w * C + 1.0) * FC;
	gl_Position.z = (2.0 * log_z - 1.0) * gl_Position.w;
}