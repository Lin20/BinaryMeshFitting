#include "PCH.h"
#include "ImplicitSampler.hpp"

#define BLOCK(f) \
		if (!out) \
			return; \
			if (!(*out)) \
				*out = (float*)_aligned_malloc(size.x * size.y * size.z * sizeof(float), 16); \
			glm::vec3 dxyz; \
			for (int x = 0; x < size.x; x++) \
			{ \
				dxyz.x = p.x + (float)x * scale; \
				for (int y = 0; y < size.y; y++) \
				{ \
					dxyz.y = p.y + (float)y * scale; \
					for (int z = 0; z < size.z; z++) \
					{ \
						dxyz.z = p.z + (float)z * scale; \
						(*out)[x * size.y * size.z + y * size.z + z] = f(resolution, dxyz); \
					} \
				} \
			}

namespace ImplicitFunctions
{
	const float torus_z(const float resolution, const glm::vec3& p)
	{
		const float r1 = resolution / 4.0f;
		const float r2 = resolution / 10.0f;
		float q_x = fabsf(sqrtf(p[0] * p[0] + p[1] * p[1])) - r1;
		float len = sqrtf(q_x * q_x + p[2] * p[2]);
		return -(len - r2);
	}

	const void torus_z_block(const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out)
	{
		BLOCK(torus_z);
	}

	const void cuboid_block(const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out)
	{
		BLOCK(cuboid);
	}

	const float sphere(const float resolution, const glm::vec3& p)
	{
		const float r = resolution * 0.25f;
		return -(glm::length(p) - r);
	}

	const float cuboid(const float resolution, const glm::vec3 & p)
	{
		using namespace glm;
		float r = resolution / 8.0f;
		vec3 local = p;
		vec3 d(fabsf(local.x) - r, fabsf(local.y) - r, fabsf(local.z) - r);
		float m = fmaxf(d.x, fmaxf(d.y, d.z));
		return -fminf(m, length(d));
	}

	const float plane_y(const float resolution, const glm::vec3 & p)
	{
		return -p.y + p.x + p.z;
	}

	glm::vec3 get_intersection(glm::vec3 v0, glm::vec3 v1, float s0, float s1)
	{
		return v0 - s0 * (v1 - v0) / (s1 - s0);
	}
}