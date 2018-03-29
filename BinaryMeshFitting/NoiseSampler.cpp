#include "PCH.h"
#include "NoiseSampler.hpp"

#define SET_OUT(_off, _val) \
char* f_out = ((char*)((*out))) + offset + (_off) * stride; \
*((float*)f_out) = (_val); \

void NOISE_BLOCK(uint32_t size_x, uint32_t size_y, uint32_t size_z, float p_x, float p_y, float p_z, float scale, float** out, FastNoiseVectorSet* vectorset_out)
{
	if (!out)
		return;
	//if (!(*out))
	//	*out = FastNoiseSIMD::GetEmptySet(size_x, size_y, size_z);
	//if (vectorset_out->sampleSizeX != size_x || vectorset_out->sampleSizeY != size_y || vectorset_out->sampleSizeZ != size_z)
	//	vectorset_out->SetSize(size_x * size_y * size_z);
	vectorset_out->sampleScale = 0;
	int index = 0;
	float dx, dy, dz;
	for (int ix = 0; ix < size_x; ix++)
	{
		dx = (float)ix * scale + p_x;
		for (int iy = 0; iy < size_y; iy++)
		{
			dy = (float)iy * scale + p_y;
			for (int iz = 0; iz < size_z; iz++)
			{
				dz = (float)iz * scale + p_z;
				vectorset_out->xSet[index] = dx;
				vectorset_out->ySet[index] = dy;
				vectorset_out->zSet[index] = dz;
				index++;
			}
		}
	}
}

void NOISE_BLOCK_OFFSET_XZ(uint32_t size_x, uint32_t size_y, uint32_t size_z, float p_x, float p_y, float p_z, float scale, float** out, FastNoiseVectorSet* vectorset_out, float* off_x, float* off_z, float off_x_scale, float off_z_scale)
{
	if (!out)
		return;
	//if (!(*out))
	//	*out = FastNoiseSIMD::GetEmptySet(size_x, size_y, size_z);
	//vectorset_out->SetSize(size_x * size_y * size_z);
	vectorset_out->sampleScale = 0;
	int index = 0;
	float dx, dy, dz;
	for (int ix = 0; ix < size_x; ix++)
	{
		dx = (float)ix * scale + p_x;
		for (int iy = 0; iy < size_y; iy++)
		{
			dy = (float)iy * scale + p_y;
			for (int iz = 0; iz < size_z; iz++)
			{
				dz = (float)iz * scale + p_z;
				//dx += off_x[ix * size_y * size_z + iy * size_z + iz] * off_x_scale;
				dz += off_z[ix * size_y * size_z + iy * size_z + iz] * off_z_scale;
				vectorset_out->xSet[index] = dx;
				vectorset_out->ySet[index] = dy;
				vectorset_out->zSet[index] = dz;
				index++;
			}
		}
	}
}

void NOISE_BLOCK_OFFSET_XYZ(uint32_t size_x, uint32_t size_y, uint32_t size_z, float p_x, float p_y, float p_z, float scale, float** out, FastNoiseVectorSet* vectorset_out, float* off_x, float* off_y, float* off_z, float off_x_scale, float off_y_scale, float off_z_scale)
{
	if (!out)
		return;
	if (!(*out))
		*out = FastNoiseSIMD::GetEmptySet(size_x, size_y, size_z);
	vectorset_out->SetSize(size_x * size_y * size_z);
	vectorset_out->sampleScale = 0;
	int index = 0;
	float dx, dy, dz;
	for (int ix = 0; ix < size_x; ix++)
	{
		dx = (float)ix * scale + p_x;
		for (int iy = 0; iy < size_y; iy++)
		{
			dy = (float)iy * scale + p_y;
			for (int iz = 0; iz < size_z; iz++)
			{
				dz = (float)iz * scale + p_z;
				dx += off_x[ix * size_y * size_z + iy * size_z + iz] * off_x_scale;
				dy += off_y[ix * size_y * size_z + iy * size_z + iz] * off_y_scale;
				dz += off_z[ix * size_y * size_z + iy * size_z + iz] * off_z_scale;
				vectorset_out->xSet[index] = dx;
				vectorset_out->ySet[index] = dy;
				vectorset_out->zSet[index] = dz;
				index++;
			}
		}
	}
}


const float NoiseSamplers::noise3d(const float resolution, const glm::vec3& p)
{
	return 0;
}

const void NoiseSamplers::noise3d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, void** out, FastNoiseVectorSet* vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	/*NOISE_BLOCK(size.x, size.y, size.z, p.x, p.y, p.z, scale, out, vectorset_out);

	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(3);
	sampler.noise_samplers[thread_index]->FillNoiseSet(*out, vectorset_out);*/
}

const void NoiseSamplers::terrain2d_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, void ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	const float g_scale = 1.0f;
	const float nm = 64.0f;
	int thread_index = (properties ? properties->thread_id : 0);
	NOISE_BLOCK(size.x, 1, size.z, p.x * g_scale, 0, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(12);
	sampler.noise_samplers[thread_index]->SetFractalGain(0.5f);
	sampler.noise_samplers[thread_index]->SetFractalLacunarity(2.0f);
	sampler.noise_samplers[thread_index]->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	sampler.noise_samplers[thread_index]->FillNoiseSet(dest_noise, vectorset_out);

	if (!(*out))
		*out = (float*)_aligned_malloc(sizeof(float) * size.x * size.y * size.z, 16);

	float dx, dy, dz;
	for (int ix = 0; ix < size.x; ix++)
	{
		for (int iy = 0; iy < size.y; iy++)
		{
			dy = ((float)iy * scale + p.y) * g_scale;
			for (int iz = 0; iz < size.z; iz++)
			{
				float n = dest_noise[ix * size.z + iz];
				//(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n * nm;
				SET_OUT(ix * size.y * size.z + iy * size.z + iz, -dy - n * nm);
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain2d_pert_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, void ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	const float g_scale = properties ? ((NoiseSamplerProperties*)properties)->g_scale : 0.25f;
	const float nm = properties ? ((NoiseSamplerProperties*)properties)->height : 64.0f;
	NOISE_BLOCK(size.x, 1, size.z, p.x * g_scale, 0, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	int thread_index = (properties ? properties->thread_id : 0);
	int octaves = properties ? ((NoiseSamplerProperties*)properties)->octaves : 20;
	float amp = properties ? ((NoiseSamplerProperties*)properties)->amp : 1.0f;
	float freq = properties ? ((NoiseSamplerProperties*)properties)->frequency : 0.5f;
	float gain = properties ? ((NoiseSamplerProperties*)properties)->gain : 0.5f;

	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_samplers[thread_index]->SetPerturbType(FastNoiseSIMD::PerturbType::GradientFractal);
	sampler.noise_samplers[thread_index]->SetPerturbFractalOctaves(octaves);
	sampler.noise_samplers[thread_index]->SetPerturbAmp(amp);
	sampler.noise_samplers[thread_index]->SetPerturbFrequency(freq);
	sampler.noise_samplers[thread_index]->SetPerturbFractalGain(gain);
	sampler.noise_samplers[thread_index]->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	sampler.noise_samplers[thread_index]->FillNoiseSet(dest_noise, vectorset_out);

	if (!(*out))
		*out = (float*)_aligned_malloc(sizeof(float) * size.x * size.y * size.z, 16);

	float* f_out = (float*)*out;

	float dx, dy, dz;
	uint32_t ind = 0;
	for (int ix = 0; ix < size.x; ix++)
	{
		for (int iy = 0; iy < size.y; iy++)
		{
			dy = ((float)iy * scale + p.y) * g_scale;
			for (int iz = 0; iz < size.z; iz++)
			{
				float n = dest_noise[ix * size.z + iz];
				//dy = roundf(dy * 4.0f) / 4.0f;
				//(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n * nm;
				//SET_OUT(ind, -dy - n * nm);
				f_out[ind] = -dy - n * nm;
				ind++;
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain3d_block(const Sampler & sampler, const float resolution, const glm::vec3 & p, const glm::ivec3 & size, const float scale, void ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	const float g_scale = 0.15f;
	const float ym = 0.5f;
	int thread_index = (properties ? properties->thread_id : 0);
	NOISE_BLOCK(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(4);
	sampler.noise_samplers[thread_index]->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
	sampler.noise_samplers[thread_index]->FillNoiseSet(dest_noise, vectorset_out);

	if (!(*out))
		*out = (float*)_aligned_malloc(sizeof(float) * size.x * size.y * size.z, 16);

	float dx, dy, dz;
	for (int ix = 0; ix < size.x; ix++)
	{
		for (int iy = 0; iy < size.y; iy++)
		{
			dy = ((float)iy * scale + p.y) * g_scale * ym;
			for (int iz = 0; iz < size.z; iz++)
			{
				float n = dest_noise[ix * size.y * size.z + iy * size.z + iz];
				//(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
				SET_OUT(ix * size.y * size.z + iy * size.z + iz, -dy - n);
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain3d_pert_block(const Sampler & sampler, const float resolution, const glm::vec3 & p, const glm::ivec3 & size, const float scale, void ** out, FastNoiseVectorSet* vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	const float g_scale = 0.15f;
	const float nm = 48.0f;
	int thread_index = (properties ? properties->thread_id : 0);
	NOISE_BLOCK(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_samplers[thread_index]->SetPerturbType(FastNoiseSIMD::PerturbType::GradientFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(8);
	sampler.noise_samplers[thread_index]->SetPerturbAmp(1.0f);
	sampler.noise_samplers[thread_index]->SetPerturbFrequency(0.05f);
	sampler.noise_samplers[thread_index]->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
	sampler.noise_samplers[thread_index]->FillNoiseSet(dest_noise, vectorset_out);

	if (!(*out))
		*out = (float*)_aligned_malloc(sizeof(float) * size.x * size.y * size.z, 16);

	float dx, dy, dz;
	for (int ix = 0; ix < size.x; ix++)
	{
		for (int iy = 0; iy < size.y; iy++)
		{
			dy = ((float)iy * scale + p.y) * g_scale;
			for (int iz = 0; iz < size.z; iz++)
			{
				float n = dest_noise[ix * size.y * size.z + iy * size.z + iz];
				//(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
				SET_OUT(ix * size.y * size.z + iy * size.z + iz, -dy - n * nm);
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::windy3d_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3 & size, const float scale, void ** out, FastNoiseVectorSet* vectorset_out, float* dest_noise, int offset, int stride, SamplerProperties* properties)
{
	const float g_scale = 0.25f;
	const float ym = 0.5f;
	const int wind_octaves = 1;
	const float wind_scale = 0.01f;
	const float wind_perc = 1.0f;
	int thread_index = (properties ? properties->thread_id : 0);

	float* noise_offset_x = 0;
	float* noise_offset_y = 0;
	float* final_noise = 0;
	FastNoiseVectorSet x_vectorset;
	FastNoiseVectorSet y_vectorset;
	NOISE_BLOCK(size.x, size.y, size.z, p.x * wind_scale, p.y * wind_scale, p.z * wind_scale, scale * wind_scale, &noise_offset_x, &x_vectorset);
	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(wind_octaves);
	sampler.noise_samplers[thread_index]->FillNoiseSet(noise_offset_x, &x_vectorset);

	NOISE_BLOCK(size.x, size.y, size.z, p.x * wind_scale, p.y * wind_scale, p.z * wind_scale, scale * wind_scale, &noise_offset_y, &y_vectorset);
	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(wind_octaves);
	sampler.noise_samplers[thread_index]->FillNoiseSet(noise_offset_y, &y_vectorset);

	NOISE_BLOCK_OFFSET_XZ(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &final_noise, vectorset_out, noise_offset_x, noise_offset_y, wind_perc, wind_perc);
	sampler.noise_samplers[thread_index]->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_samplers[thread_index]->SetFractalOctaves(4);
	//sampler.noise_samplers[thread_index]->SetFractalType(FastNoiseSIMD::FractalType::Billow);
	sampler.noise_samplers[thread_index]->FillNoiseSet(final_noise, vectorset_out);



	if (!(*out))
		*out = (float*)_aligned_malloc(sizeof(float) * size.x * size.y * size.z, 16);

	float dx, dy, dz;
	for (int ix = 0; ix < size.x; ix++)
	{
		for (int iy = 0; iy < size.y; iy++)
		{
			dy = ((float)iy * scale + p.y) * g_scale * ym;
			for (int iz = 0; iz < size.z; iz++)
			{
				float n = final_noise[ix * size.y * size.z + iy * size.z + iz] * resolution * 0.5f;
				//(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
				SET_OUT(ix * size.y * size.z + iy * size.z + iz, -dy - n);
			}
		}
	}

	_aligned_free(final_noise);
	_aligned_free(noise_offset_x);
	_aligned_free(noise_offset_y);
}
