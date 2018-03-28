#version 400 core
in vec3 f_mul_color;
in float log_z;
out vec4 frag_color;
void main() 
{
	frag_color = vec4(f_mul_color, 1.0);
	gl_FragDepth = log_z - 0.00001;
}