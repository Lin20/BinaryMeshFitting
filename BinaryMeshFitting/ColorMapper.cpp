#include "PCH.h"
#include "ColorMapper.hpp"

using namespace glm;

ColorMapper::ColorMapper()
{
	noise_context = FastNoiseSIMD::NewFastNoiseSIMD();
}

ColorMapper::~ColorMapper()
{
}

void ColorMapper::generate_colors(SmartContainer<DualVertex>& verts)
{
	if (!verts.count)
		return;

	float* noise = 0;
	get_noise(verts, &noise);
	map_noise(verts, noise);

	FastNoiseSIMD::FreeNoiseSet(noise);
}

void ColorMapper::get_noise(SmartContainer<DualVertex>& verts, float** out)
{
	int count = (int)verts.count;

	FastNoiseVectorSet vectors;
	vectors.SetSize(count);

	const float scale = 1.0f;

	for (int i = 0; i < count; i++)
	{
		vectors.xSet[i] = verts[i].p.x * scale;
		vectors.ySet[i] = verts[i].p.y * scale;
		vectors.zSet[i] = verts[i].p.z * scale;
	}

	*out = FastNoiseSIMD::GetEmptySet(count, 1, 1);

	noise_context->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	noise_context->SetFractalOctaves(4);
	noise_context->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	noise_context->FillNoiseSet(*out, &vectors);
}

void ColorMapper::map_noise(SmartContainer<DualVertex>& verts, float* noise)
{
	int count = (int)verts.count;

	for (int i = 0; i < count; i++)
	{
		float n = noise[i] * 4.0f;
		verts[i].color = hsl_to_rgb((n + 1.0f) * 0.5f * 360.0f, 0.72f, 1.0f);
	}
}

glm::vec3 ColorMapper::hsl_to_rgb(float h, float s, float v)
{
	float hh, p, q, t, ff;
	int i;
	float r = 0, g = 0, b = 0;

	if (s <= 0.0f) {       // < is bogus, just shuts up warnings
		r = v;
		g = v;
		b = v;
		return vec3(r, g, b);
	}
	hh = h;
	//if (hh < 0.0f) hh += 360.0f;
	hh = fmodf(fabsf(hh), 360.0f);
	hh /= 60.0f;
	i = (int)hh;
	ff = hh - i;
	p = v * (1.0f - s);
	q = v * (1.0f - (s * ff));
	t = v * (1.0f - (s * (1.0f - ff)));

	switch (i) {
	case 0:
		r = v;
		g = t;
		b = p;
		break;
	case 1:
		r = q;
		g = v;
		b = p;
		break;
	case 2:
		r = p;
		g = v;
		b = t;
		break;

	case 3:
		r = p;
		g = q;
		b = v;
		break;
	case 4:
		r = t;
		g = p;
		b = v;
		break;
	case 5:
	default:
		r = v;
		g = p;
		b = q;
		break;
	}

	return vec3(r, g, b);
}
