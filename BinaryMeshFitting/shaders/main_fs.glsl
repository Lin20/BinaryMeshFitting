#version 400 core

in vec3 f_normal;
in vec3 f_color;
in vec3 f_mul_color;
in vec3 f_ec_pos;
in float f_smooth_shading;
in float f_specular_power;
in float log_z;
in vec4 f_world_pos;
in float f_chunk_depth;
out vec4 frag_color;

uniform sampler2D rock_texture;
uniform sampler2D rock2_texture;
uniform sampler2D grass_texture;
uniform sampler3D noise_texture;

vec4 noise_octaves(vec2 v, int octaves, float scale, float pers)
{
	vec4 n = vec4(0);
	float maxAmp = 0;
    float amp = 1.0;
    float freq = scale;
    float noise = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		  n += texture(noise_texture, vec3(v.xy * freq, 1.0)) * amp;
        maxAmp += amp;
        amp *= pers;
        freq *= 2.0;
	}

	n /= maxAmp;

	return n;
}

vec4 noise_octaves(vec3 v, int octaves, float scale, float pers)
{
	vec4 n = vec4(0);
	float maxAmp = 0;
    float amp = 1.0;
    float freq = scale;
    float noise = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		  n += texture(noise_texture, v * freq) * amp;
        maxAmp += amp;
        amp *= pers;
        freq *= 2.0;
	}

	n /= maxAmp;

	return n;
}

vec3 tri_tex(vec3 pos, vec3 normal, sampler2D tex, float scale)
{
	vec3 blending = normalize(normal);
	blending = blending * blending;
	float b = (blending.x + blending.y + blending.z);
	blending /= b;

	vec3 coords = pos.xyz * scale;
	vec4 xaxis = texture2D( tex, coords.yz);
	vec4 yaxis = texture2D( tex, coords.xz);
	vec4 zaxis = texture2D( tex, coords.xy);
	vec4 color = xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;

	return color.xyz;
}

vec3 get_color(vec3 pos, float d)
{
  const vec3 ROCK = vec3(0.73, 0.7, 0.71);
  const vec3 GRASS = vec3(0.3, 0.8, 0.1);
  const vec3 DIRT = vec3(0.5, 0.4, 0.25);

  float n = noise_octaves(pos * 0.0015, 4, 1.0, 0.5).x;
 // return vec3(n);

  if(d >= n * 0.5 + 1.0)
  return GRASS;
  if(d >= 0.4 + n * 0.2)
  return DIRT;
  return ROCK;
}

void main()
{
  vec3 normal;
  if (f_smooth_shading != 0.0)
    normal = f_normal;
  else
    normal = normalize(cross(dFdx(f_ec_pos), dFdy(f_ec_pos)));

  float d = dot(normalize(-vec3(0.1, -1.0, 0.5)), normal);
  float m = mix(0.0, 1.0, d * 0.5 + 0.5);
  float s = (f_specular_power > 0.0 ? pow(max(0.0, d), f_specular_power) : 0.0);

  float up = dot(normal, vec3(0, 1, 0));
  //up *= noise_octaves(f_world_pos.xz * 0.002f, 4, 1.0f, 0.5f) * 1.5 + 0.2;
  //up = clamp(up, 0.0f, 1.0f);

  

  vec3 coords0 = f_world_pos.xyz;
  coords0.xz += noise_octaves(f_world_pos.xyz * 0.0005, 4, 1.0, 0.5).xy * 64.0;
  vec3 color0 = tri_tex(coords0, normal, rock_texture, 0.0025f);
  //vec3 color0_grass = tri_tex(f_world_pos.xyz, normal, grass_texture, 0.0025f);
  //color0 = mix(color0, color0_grass, up);

  vec3 color1 = tri_tex(f_world_pos.xyz * 0.5f, normal, rock2_texture, 1.0f);
  //vec3 color1_grass = tri_tex(f_world_pos.xyz, normal, grass_texture, 0.5f);
  //color1 = mix(color1, color1_grass, up);

  float scale = f_chunk_depth / 64.0f;
  scale = clamp(scale + 0.1f, 0.0, 1.0);

  //vec3 color = get_color(f_world_pos.xyz, up);
  //color = f_color;
  vec3 color = f_color;//mix(color0, color1, scale);

  vec3 result = color * m + color * s;
  frag_color = vec4(result, 1.0);

	gl_FragDepth = log_z;
}