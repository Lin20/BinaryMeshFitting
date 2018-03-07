#include "PCH.h"
#include "NoiseSampler.hpp"

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

const void NoiseSamplers::noise3d_block(const Sampler& sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float** out, FastNoiseVectorSet* vectorset_out)
{
	NOISE_BLOCK(size.x, size.y, size.z, p.x, p.y, p.z, scale, out, vectorset_out);

	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_sampler->SetFractalOctaves(3);
	sampler.noise_sampler->FillNoiseSet(*out, vectorset_out);
}

const void NoiseSamplers::terrain2d_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise)
{
	const float g_scale = 0.25f;
	const float ym = 0.5f;
	NOISE_BLOCK(size.x, 1, size.z, p.x * g_scale, 0, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_sampler->SetFractalOctaves(20);
	sampler.noise_sampler->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	sampler.noise_sampler->FillNoiseSet(dest_noise, vectorset_out);

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
				float n = dest_noise[ix * size.z + iz] * resolution * 0.125f;
				(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain2d_pert_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3& size, const float scale, float ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise)
{
	const float g_scale = 1.0f;
	const float ym = 0.5f;
	NOISE_BLOCK(size.x, 1, size.z, p.x * g_scale, 0, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_sampler->SetPerturbType(FastNoiseSIMD::PerturbType::GradientFractal);
	sampler.noise_sampler->SetPerturbFractalOctaves(20);
	sampler.noise_sampler->SetPerturbAmp(0.707f);
	sampler.noise_sampler->SetPerturbFrequency(0.5f);
	sampler.noise_sampler->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
	sampler.noise_sampler->FillNoiseSet(dest_noise, vectorset_out);

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
				float n = dest_noise[ix * size.z + iz] * resolution * 0.125f;
				(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain3d_block(const Sampler & sampler, const float resolution, const glm::vec3 & p, const glm::ivec3 & size, const float scale, float ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise)
{
	const float g_scale = 0.15f;
	const float ym = 0.5f;
	NOISE_BLOCK(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_sampler->SetFractalOctaves(4);
	sampler.noise_sampler->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
	sampler.noise_sampler->FillNoiseSet(dest_noise, vectorset_out);

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
				float n = dest_noise[ix * size.y * size.z + iy * size.z + iz] * resolution * 0.1f;
				(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::terrain3d_pert_block(const Sampler & sampler, const float resolution, const glm::vec3 & p, const glm::ivec3 & size, const float scale, float ** out, FastNoiseVectorSet * vectorset_out, float* dest_noise)
{
	const float g_scale = 0.25f;
	const float ym = 0.5f;
	NOISE_BLOCK(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &dest_noise, vectorset_out);

	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::ValueFractal);
	sampler.noise_sampler->SetPerturbType(FastNoiseSIMD::PerturbType::GradientFractal);
	sampler.noise_sampler->SetFractalOctaves(8);
	sampler.noise_sampler->SetPerturbAmp(0.707f);
	sampler.noise_sampler->SetPerturbFrequency(0.25f);
	sampler.noise_sampler->SetFractalType(FastNoiseSIMD::FractalType::RigidMulti);
	sampler.noise_sampler->FillNoiseSet(dest_noise, vectorset_out);

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
				float n = dest_noise[ix * size.y * size.z + iy * size.z + iz] * resolution * 0.25f;
				(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
			}
		}
	}

	//_aligned_free(dest_noise);
}

const void NoiseSamplers::windy3d_block(const Sampler & sampler, const float resolution, const glm::vec3& p, const glm::ivec3 & size, const float scale, float ** out, FastNoiseVectorSet* vectorset_out, float* dest_noise)
{
	const float g_scale = 0.25f;
	const float ym = 0.5f;
	const int wind_octaves = 1;
	const float wind_scale = 0.01f;
	const float wind_perc = 1.0f;

	float* noise_offset_x = 0;
	float* noise_offset_y = 0;
	float* final_noise = 0;
	FastNoiseVectorSet x_vectorset;
	FastNoiseVectorSet y_vectorset;
	NOISE_BLOCK(size.x, size.y, size.z, p.x * wind_scale, p.y * wind_scale, p.z * wind_scale, scale * wind_scale, &noise_offset_x, &x_vectorset);
	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_sampler->SetFractalOctaves(wind_octaves);
	sampler.noise_sampler->FillNoiseSet(noise_offset_x, &x_vectorset);

	NOISE_BLOCK(size.x, size.y, size.z, p.x * wind_scale, p.y * wind_scale, p.z * wind_scale, scale * wind_scale, &noise_offset_y, &y_vectorset);
	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_sampler->SetFractalOctaves(wind_octaves);
	sampler.noise_sampler->FillNoiseSet(noise_offset_y, &y_vectorset);

	NOISE_BLOCK_OFFSET_XZ(size.x, size.y, size.z, p.x * g_scale, p.y * g_scale, p.z * g_scale, scale * g_scale, &final_noise, vectorset_out, noise_offset_x, noise_offset_y, wind_perc, wind_perc);
	sampler.noise_sampler->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
	sampler.noise_sampler->SetFractalOctaves(4);
	//sampler.noise_sampler->SetFractalType(FastNoiseSIMD::FractalType::Billow);
	sampler.noise_sampler->FillNoiseSet(final_noise, vectorset_out);



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
				(*out)[ix * size.y * size.z + iy * size.z + iz] = -dy - n;
			}
		}
	}

	_aligned_free(final_noise);
	_aligned_free(noise_offset_x);
	_aligned_free(noise_offset_y);
}
