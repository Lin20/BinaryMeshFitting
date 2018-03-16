#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <FastNoiseSIMD.h>
#include <string>

class SamplerProperties
{
public:
	inline SamplerProperties() {}
	virtual ~SamplerProperties() {};
};

//typedef std::function<float(const float world_size, const glm::vec3& p)> SamplerValueFunction;
typedef const float(*SamplerValueFunction)(const float world_size, const glm::vec3& p);
typedef std::function<void(const float world_size, const glm::vec3& p, const glm::ivec3& size, const float scale, void** out, FastNoiseVectorSet* vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)> SamplerBlockFunction;
typedef std::function<glm::vec3(const float world_size, const glm::vec3& p, float h)> SamplerGradientFunction;

struct Sampler
{
	float world_size;
	SamplerValueFunction value;
	SamplerBlockFunction block;
	SamplerGradientFunction gradient;
	FastNoiseSIMD* noise_sampler;

	inline Sampler() : noise_sampler(0) {}
	inline virtual ~Sampler() {}
};

inline std::string get_simd_text()
{
	switch (FastNoiseSIMD::GetSIMDLevel())
	{
	case 1:
		return "SSE2";
	case 2:
		return "SSE4.1";
	case 3:
		return "AVX2/FMA3";
	case 4:
		return "AVX512";
	case 5:
		return "ARM NEON";
	default:
		return "None";
	}
}
