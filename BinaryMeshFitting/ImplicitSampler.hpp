#pragma once

#include "Sampler.hpp"

namespace ImplicitFunctions
{
	const float torus_z(const float resolution, const glm::vec3& p);
	const void torus_z_block(const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out);
	const float sphere(const float resolution, const glm::vec3& p);
	const float cuboid(const float resolution, const glm::vec3& p);
	const float plane_y(const float resolution, const glm::vec3& p);

	inline void implicit_block(const SamplerValueFunction& f, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out)
	{
		if (!out)
			return;
		if (!(*out))
			*out = (float*)_aligned_malloc(size.x * size.y * size.z * sizeof(float), 16);

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

	inline Sampler create_sampler(const SamplerValueFunction& f)
	{
		using namespace std::placeholders;
		Sampler s;
		s.value = f;
		s.block = std::bind(implicit_block, f, _1, _2, _3, _4, _5);
		s.gradient = std::bind(implicit_gradient, f, _1, _2, _3);
		return s;
	}

	glm::vec3 get_intersection(glm::vec3 v0, glm::vec3 v1, float s0, float s1);
}