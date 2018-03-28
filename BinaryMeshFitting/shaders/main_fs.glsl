#version 400 core

in vec3 f_normal;
in vec3 f_color;
in vec3 f_mul_color;
in vec3 f_ec_pos;
in float f_smooth_shading;
in float f_specular_power;
in float log_z;
out vec4 frag_color;

void main()
{
	vec3 normal;
	if (f_smooth_shading != 0.0)
		normal = f_normal;
	else
		normal = normalize(cross(dFdx(f_ec_pos), dFdy(f_ec_pos)));

	float d = dot(normalize(-vec3(0.1, -1.0, 0.5)), normal);
	float m = mix(0.2, 1.0, d * 0.5 + 0.5);
	float s = (f_specular_power > 0.0 ? pow(max(0.0, d), f_specular_power) : 0.0);

	vec3 color = vec3(0.3, 0.3, 0.5);
	vec3 color2 = vec3(0.1, 0.1, 0.25);
	vec3 result = f_color * m + f_color * s;
	frag_color = vec4(result, 1.0);

	gl_FragDepth = log_z;
}