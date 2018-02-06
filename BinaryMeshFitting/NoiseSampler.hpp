#pragma once

#include "Sampler.hpp"

namespace NoiseSamplers
{
	const float noise3d(const float resolution, const glm::vec3& p);
	const void noise3d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out);

	const void terrain2d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out, float* dest_noise);
	const void terrain2d_pert_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise);

	const void terrain3d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out, float* dest_noise);
	const void terrain3d_pert_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out, float* dest_noise);

	const void windy3d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out, float* dest_noise);

	inline glm::vec3 implicit_gradient(SamplerValueFunction f, const float resolution, const glm::vec3& p, float h = 0.01f)
	{
		using namespace glm;
		float dxp = f(resolution, vec3(p.x + h, p.y, p.z));
		float dxm = f(resolution, vec3(p.x - h, p.y, p.z));
		float dyp = f(resolution, vec3(p.x, p.y + h, p.z));
		float dym = f(resolution, vec3(p.x, p.y - h, p.z));
		float dzp = f(resolution, vec3(p.x, p.y, p.z + h));
		float dzm = f(resolution, vec3(p.x, p.y, p.z - h));

		return vec3(dxp - dxm, dyp - dym, dzp - dzm);
	}

	inline void implicit_block(const SamplerValueFunction& f, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet** vectorset_out)
	{
		if (!out)
			return;
		if (!(*out))
			*out = (float*)malloc(size.x * size.y * size.z * sizeof(float));

		glm::vec3 dxyz;
		for (int x = 0; x < size.x; x++)
		{
			dxyz.x = p.x + (float)x * scale;
			for (int y = 0; y < size.y; y++)
			{
				dxyz.y = p.y + (float)y * scale;
				for (int z = 0; z < size.z; z++)
				{
					dxyz.z = p.z + (float)z * scale;
					(*out)[x * size.x * size.x + y * size.y + z] = f(resolution, dxyz);
				}
			}
		}
	}

	inline Sampler create_sampler(const SamplerValueFunction& f)
	{
		using namespace std::placeholders;
		Sampler s;
		s.value = f;
		s.gradient = std::bind(implicit_gradient, f, _1, _2, _3);
		s.noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		return s;
	}

	inline void create_sampler_noise3d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(noise3d_block, *s, _1, _2, _3, _4, _5, _6);
	}

	inline void create_sampler_terrain_2d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(terrain2d_block, *s, _1, _2, _3, _4, _5, _6, _7);
	}

	inline void create_sampler_terrain_pert_2d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(terrain2d_pert_block, *s, _1, _2, _3, _4, _5, _6, _7);
	}

	inline void create_sampler_terrain_3d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(terrain3d_block, *s, _1, _2, _3, _4, _5, _6, _7);
	}

	inline void create_sampler_terrain_pert_3d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(terrain3d_pert_block, *s, _1, _2, _3, _4, _5, _6, _7);
	}

	inline void create_sampler_windy_3d(Sampler* s)
	{
		using namespace std::placeholders;
		s->value = noise3d;
		s->gradient = std::bind(implicit_gradient, noise3d, _1, _2, _3);
		s->noise_sampler = FastNoiseSIMD::NewFastNoiseSIMD();
		s->block = std::bind(windy3d_block, *s, _1, _2, _3, _4, _5, _6, _7);
	}
}
