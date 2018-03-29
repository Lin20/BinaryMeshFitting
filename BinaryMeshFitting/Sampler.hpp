#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <FastNoiseSIMD.h>
#include <string>

class SamplerProperties
{
public:
	int thread_id;

	inline SamplerProperties() : thread_id(0) {}
	inline SamplerProperties(int _thread_id) : thread_id(_thread_id) {}
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
	FastNoiseSIMD* noise_samplers[8];

	inline Sampler() { for (int i = 0; i < 8; i++) noise_samplers[i] = 0; }
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
