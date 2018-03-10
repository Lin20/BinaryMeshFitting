#include "PCH.h"
#include "BinaryChunk.hpp"
#include "Tables.hpp"
#include <iostream>
#include <iomanip>
#include <queue>
#include <deque>

using namespace glm;

#define EDGE_LINE(l_z, z, b) \
if(line & b) { \
if (z_block < z_count - 1 || l_z * 8 + z < 31) \
	local_masks[l_z] |= 1ull << (z * 8 + m); \
if (z > 0) \
	local_masks[l_z] |= 1ull << ((z - 1) * 8 + m + 1); \
else if (l_z > 0) \
	local_masks[l_z - 1] |= 1ull << (7 * 8 + m + 1); \
else if (z_block > 0) \
	line_masks[-1] |= 1ull << (7 * 8 + m + 1); \
}

#define RESOLUTION 256

BinaryChunk::BinaryChunk()
{
}

BinaryChunk::BinaryChunk(glm::vec3 pos, float size, int level, Sampler& sampler,
	bool produce_quads)
{
	init(pos, size, level, sampler, produce_quads);
}

BinaryChunk::~BinaryChunk()
{
	free(inds);
	inds = 0;

	free(masks);
	masks = 0;

	free(samples);
	samples = 0;
}

void BinaryChunk::init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads)
{
	this->pos = pos;
	this->dim = RESOLUTION;
	this->level = level;
	this->size = size;

	this->contains_mesh = false;
	this->mesh_offset = 0;

	this->inds = 0;
	this->sampler = sampler;
	samples = 0;
}

void BinaryChunk::generate_samples()
{
	assert(sampler.value != nullptr);
	uint32_t z_per_y_chunks = ((dim + 31)) / 32;
	uint32_t y_per_x_chunks = z_per_y_chunks * dim;
	uint32_t z_per_y = dim;
	uint32_t y_per_x = z_per_y * dim;
	bool positive = false, negative = false;
	uint32_t count = (y_per_x * dim);
	uint32_t real_count = ((z_per_y_chunks * 32) * dim * dim + 31) / 32;
	const float scale = 1.0f;

	samples = (uint32_t*)malloc(sizeof(uint32_t) * real_count);
	//memset(samples, 0, sizeof(uint32_t) * real_count);
	float delta = size / (float)dim;
	const float res = sampler.world_size;

	float* block_data = 0;
	FastNoiseVectorSet vector_set;
	sampler.block(res, pos, ivec3(dim, dim, dim), delta * scale, &block_data, &vector_set, 0);

	vec3 dxyz;
	auto f = sampler.value;
	uint32_t s0_mask, s0;
	uint32_t z_count = (dim + 32) / 32;
	bool mesh = false;
	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
		{
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				float* block_samples = block_data + x * y_per_x + y * z_per_y + z_block * 32;
				uint32_t m = 0;
				uint32_t z_max = dim - z_block * 32;
				if (z_max > 32)
					z_max = 32;

				for (uint32_t z = 0; z < z_max; z++)
				{
					float s = block_samples[z];
					if (block_samples[z] < 0.0f)
						m |= 1 << z;
				}
				if (m != 0)
				{
					samples[x * y_per_x_chunks + y * z_per_y_chunks + z_block] = m;
					if (m != 0xFFFFFFFF)
						mesh = true;
					else if (!mesh)
						negative = true;
				}
				else
				{
					if (!mesh)
						positive = true;
					samples[x * y_per_x_chunks + y * z_per_y_chunks + z_block] = 0;
				}
			}
		}
	}

	if (!mesh)
		contains_mesh = negative && positive;
	else
		contains_mesh = mesh;

	_aligned_free(block_data);
}

void BinaryChunk::generate_neighbor_info()
{
	if (!contains_mesh)
		return;

	uint32_t dimm1 = dim - 1;
	uint32_t count = dim * dim * dim;
	uint32_t z_per_y = ((dim + 31)) / 32;
	uint32_t y_per_x = z_per_y * dim;

	uint32_t z_per_y8 = (dimm1 + 8) / 8;
	uint32_t y_per_x8 = z_per_y8 * dim;
	uint32_t count8 = z_per_y8 * y_per_x8 * dim;

	inds = (uint32_t*)malloc(sizeof(uint32_t) * count);
	memset(inds, 0xFFFFFFFF, sizeof(uint32_t) * count);

	masks = (uint64_t*)malloc(sizeof(uint64_t) * count8);
	//memset(masks, 0, sizeof(uint64_t) * count8);

	uint32_t z_count = (dim + 31) / 32;

	for (uint32_t x = 0; x < dimm1; x++)
	{
		for (uint32_t y = 0; y < dimm1; y++)
		{
			const int m = 0;
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				uint32_t line = samples[x * y_per_x + y * z_per_y + z_block];
				uint64_t* __restrict line_masks = masks + x * y_per_x8 + y * z_per_y8 + z_block * 4;
				if (line == 0)
				{
					line_masks[0] = 0;
					line_masks[1] = 0;
					line_masks[2] = 0;
					line_masks[3] = 0;
					continue;
				}
				uint64_t local_masks[4] = { 0, 0, 0, 0 };
				EDGE_LINE(0, 0, 0x1);
				EDGE_LINE(0, 1, 0x2);
				if (z_block * 32 + 1 < dim)
				{
					EDGE_LINE(0, 2, 0x4);
					EDGE_LINE(0, 3, 0x8);
					EDGE_LINE(0, 4, 0x10);
					EDGE_LINE(0, 5, 0x20);
					EDGE_LINE(0, 6, 0x40);
					EDGE_LINE(0, 7, 0x80);
					EDGE_LINE(1, 0, 0x100);
					EDGE_LINE(1, 1, 0x200);
					EDGE_LINE(1, 2, 0x400);
					EDGE_LINE(1, 3, 0x800);
					EDGE_LINE(1, 4, 0x1000);
					EDGE_LINE(1, 5, 0x2000);
					EDGE_LINE(1, 6, 0x4000);
					EDGE_LINE(1, 7, 0x8000);
					EDGE_LINE(2, 0, 0x10000);
					EDGE_LINE(2, 1, 0x20000);
					EDGE_LINE(2, 2, 0x40000);
					EDGE_LINE(2, 3, 0x80000);
					EDGE_LINE(2, 4, 0x100000);
					EDGE_LINE(2, 5, 0x200000);
					EDGE_LINE(2, 6, 0x400000);
					EDGE_LINE(2, 7, 0x800000);
					EDGE_LINE(3, 0, 0x1000000);
					EDGE_LINE(3, 1, 0x2000000);
					EDGE_LINE(3, 2, 0x4000000);
					EDGE_LINE(3, 3, 0x8000000);
					EDGE_LINE(3, 4, 0x10000000);
					EDGE_LINE(3, 5, 0x20000000);
					EDGE_LINE(3, 6, 0x40000000);
					EDGE_LINE(3, 7, 0x80000000);
				}
				//if (local_masks[0] != 0)
				line_masks[0] = local_masks[0];
				//if (local_masks[1] != 0)
				line_masks[1] = local_masks[1];
				//if (local_masks[2] != 0)
				line_masks[2] = local_masks[2];
				//if (local_masks[3] != 0)
				line_masks[3] = local_masks[3];
			}
		}
	}

	for (uint32_t x = 0; x < dimm1; x++)
	{
		for (uint32_t y = 0; y < dimm1; y++)
		{
			const int m = 2;
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				uint32_t line = samples[x * y_per_x + (y + 1) * z_per_y + z_block];
				if (line == 0)
					continue;
				uint64_t* __restrict line_masks = masks + x * y_per_x8 + y * z_per_y8 + z_block * 4;
				uint64_t local_masks[4] = { 0, 0, 0, 0 };
				EDGE_LINE(0, 0, 0x1);
				EDGE_LINE(0, 1, 0x2);
				if (z_block * 32 + 1 < dim)
				{
					EDGE_LINE(0, 2, 0x4);
					EDGE_LINE(0, 3, 0x8);
					EDGE_LINE(0, 4, 0x10);
					EDGE_LINE(0, 5, 0x20);
					EDGE_LINE(0, 6, 0x40);
					EDGE_LINE(0, 7, 0x80);
					EDGE_LINE(1, 0, 0x100);
					EDGE_LINE(1, 1, 0x200);
					EDGE_LINE(1, 2, 0x400);
					EDGE_LINE(1, 3, 0x800);
					EDGE_LINE(1, 4, 0x1000);
					EDGE_LINE(1, 5, 0x2000);
					EDGE_LINE(1, 6, 0x4000);
					EDGE_LINE(1, 7, 0x8000);
					EDGE_LINE(2, 0, 0x10000);
					EDGE_LINE(2, 1, 0x20000);
					EDGE_LINE(2, 2, 0x40000);
					EDGE_LINE(2, 3, 0x80000);
					EDGE_LINE(2, 4, 0x100000);
					EDGE_LINE(2, 5, 0x200000);
					EDGE_LINE(2, 6, 0x400000);
					EDGE_LINE(2, 7, 0x800000);
					EDGE_LINE(3, 0, 0x1000000);
					EDGE_LINE(3, 1, 0x2000000);
					EDGE_LINE(3, 2, 0x4000000);
					EDGE_LINE(3, 3, 0x8000000);
					EDGE_LINE(3, 4, 0x10000000);
					EDGE_LINE(3, 5, 0x20000000);
					EDGE_LINE(3, 6, 0x40000000);
					EDGE_LINE(3, 7, 0x80000000);
				}
				if (local_masks[0] != 0)
					line_masks[0] |= local_masks[0];
				if (local_masks[1] != 0)
					line_masks[1] |= local_masks[1];
				if (local_masks[2] != 0)
					line_masks[2] |= local_masks[2];
				if (local_masks[3] != 0)
					line_masks[3] |= local_masks[3];
			}
		}
	}

	for (uint32_t x = 0; x < dimm1; x++)
	{
		for (uint32_t y = 0; y < dimm1; y++)
		{
			const int m = 4;
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				uint32_t line = samples[(x + 1) * y_per_x + y * z_per_y + z_block];
				if (line == 0)
					continue;
				uint64_t* __restrict line_masks = masks + x * y_per_x8 + y * z_per_y8 + z_block * 4;
				uint64_t local_masks[4] = { 0, 0, 0, 0 };
				EDGE_LINE(0, 0, 0x1);
				EDGE_LINE(0, 1, 0x2);
				if (z_block * 32 + 1 < dim)
				{
					EDGE_LINE(0, 2, 0x4);
					EDGE_LINE(0, 3, 0x8);
					EDGE_LINE(0, 4, 0x10);
					EDGE_LINE(0, 5, 0x20);
					EDGE_LINE(0, 6, 0x40);
					EDGE_LINE(0, 7, 0x80);
					EDGE_LINE(1, 0, 0x100);
					EDGE_LINE(1, 1, 0x200);
					EDGE_LINE(1, 2, 0x400);
					EDGE_LINE(1, 3, 0x800);
					EDGE_LINE(1, 4, 0x1000);
					EDGE_LINE(1, 5, 0x2000);
					EDGE_LINE(1, 6, 0x4000);
					EDGE_LINE(1, 7, 0x8000);
					EDGE_LINE(2, 0, 0x10000);
					EDGE_LINE(2, 1, 0x20000);
					EDGE_LINE(2, 2, 0x40000);
					EDGE_LINE(2, 3, 0x80000);
					EDGE_LINE(2, 4, 0x100000);
					EDGE_LINE(2, 5, 0x200000);
					EDGE_LINE(2, 6, 0x400000);
					EDGE_LINE(2, 7, 0x800000);
					EDGE_LINE(3, 0, 0x1000000);
					EDGE_LINE(3, 1, 0x2000000);
					EDGE_LINE(3, 2, 0x4000000);
					EDGE_LINE(3, 3, 0x8000000);
					EDGE_LINE(3, 4, 0x10000000);
					EDGE_LINE(3, 5, 0x20000000);
					EDGE_LINE(3, 6, 0x40000000);
					EDGE_LINE(3, 7, 0x80000000);
				}
				if (local_masks[0] != 0)
					line_masks[0] |= local_masks[0];
				if (local_masks[1] != 0)
					line_masks[1] |= local_masks[1];
				if (local_masks[2] != 0)
					line_masks[2] |= local_masks[2];
				if (local_masks[3] != 0)
					line_masks[3] |= local_masks[3];
			}
		}
	}

	for (uint32_t x = 0; x < dimm1; x++)
	{
		for (uint32_t y = 0; y < dimm1; y++)
		{
			const int m = 6;
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				uint32_t line = samples[(x + 1) * y_per_x + (y + 1) * z_per_y + z_block];
				if (line == 0)
					continue;
				uint64_t* __restrict line_masks = masks + x * y_per_x8 + y * z_per_y8 + z_block * 4;
				uint64_t local_masks[4] = { 0, 0, 0, 0 };
				EDGE_LINE(0, 0, 0x1);
				EDGE_LINE(0, 1, 0x2);
				if (z_block * 32 + 1 < dim)
				{
					EDGE_LINE(0, 2, 0x4);
					EDGE_LINE(0, 3, 0x8);
					EDGE_LINE(0, 4, 0x10);
					EDGE_LINE(0, 5, 0x20);
					EDGE_LINE(0, 6, 0x40);
					EDGE_LINE(0, 7, 0x80);
					EDGE_LINE(1, 0, 0x100);
					EDGE_LINE(1, 1, 0x200);
					EDGE_LINE(1, 2, 0x400);
					EDGE_LINE(1, 3, 0x800);
					EDGE_LINE(1, 4, 0x1000);
					EDGE_LINE(1, 5, 0x2000);
					EDGE_LINE(1, 6, 0x4000);
					EDGE_LINE(1, 7, 0x8000);
					EDGE_LINE(2, 0, 0x10000);
					EDGE_LINE(2, 1, 0x20000);
					EDGE_LINE(2, 2, 0x40000);
					EDGE_LINE(2, 3, 0x80000);
					EDGE_LINE(2, 4, 0x100000);
					EDGE_LINE(2, 5, 0x200000);
					EDGE_LINE(2, 6, 0x400000);
					EDGE_LINE(2, 7, 0x800000);
					EDGE_LINE(3, 0, 0x1000000);
					EDGE_LINE(3, 1, 0x2000000);
					EDGE_LINE(3, 2, 0x4000000);
					EDGE_LINE(3, 3, 0x8000000);
					EDGE_LINE(3, 4, 0x10000000);
					EDGE_LINE(3, 5, 0x20000000);
					EDGE_LINE(3, 6, 0x40000000);
					EDGE_LINE(3, 7, 0x80000000);
				}
				if (local_masks[0] != 0)
					line_masks[0] |= local_masks[0];
				if (local_masks[1] != 0)
					line_masks[1] |= local_masks[1];
				if (local_masks[2] != 0)
					line_masks[2] |= local_masks[2];
				if (local_masks[3] != 0)
					line_masks[3] |= local_masks[3];
			}
		}
	}
}

void BinaryChunk::generate_mesh()
{
	uint32_t dimm1 = dim - 1;
	uint32_t count = dim * dim * dim;
	uint32_t z_per_y = ((dim + 31)) / 32;
	uint32_t y_per_x = z_per_y * dim;

	uint32_t z_per_y8 = (dimm1 + 8) / 8;
	uint32_t y_per_x8 = z_per_y8 * dim;
	uint32_t count8 = z_per_y8 * y_per_x8 * dim;

	float delta = size / (float)dim;

	vertices.scale = 4;
	//vertices.resize(16384);
	DualVertex temp;
	for (uint32_t x = 0; x < dim - 1; x++)
	{
		for (uint32_t y = 0; y < dim - 1; y++)
		{
			for (uint32_t z = 0; z < dim - 1; z += 8)
			{
				uint64_t mask = masks[x * y_per_x8 + y * z_per_y8 + z / 8];
				if (mask != 0 && mask != 0xFFFFFFFFFFFFFFFF)
				{
					for (int sub_z = 0; sub_z < 8 && z + sub_z < dim - 1; sub_z++)
					{
						uint32_t index = x * dim * dim + y * dim + z + sub_z;
						uint8_t sub_mask = (mask & 0xFF);
						if (sub_mask != 0 && sub_mask != 255)
						{
							//calculate_vertex(uvec3(x, y, z + sub_z), (uint32_t)vertices.count, &temp, false, sub_mask, vec3(0,0,0));
							//inds[index] = (uint32_t)vertices.count;
							//vertices.push_back(temp);
							calculate_cell(x, y, z + sub_z, index, sub_mask, pos, delta, z_per_y, y_per_x);
						}
						mask >>= 8;
					}
				}
				else
				{
				}
			}
		}
	}
	vertices.shrink();
}

__forceinline bool BinaryChunk::calculate_vertex(uint32_t next_index, DualVertex* result, glm::ivec3 xyz, float x, float y, float z, uint8_t mask)
{
	result->valence = 0;
	result->init_valence = 0;
	result->index = next_index;
	result->xyz = xyz;
	result->edge_mask = 0;
	result->mask = mask;
	result->s = 0;
	result->edge_mask = 0;

	result->p = vec3(x, y, z);
	result->n = vec3(0, 0, 0);
	result->color = vec3(1, 1, 1);

	return true;
}

uint32_t BinaryChunk::create_vertex(uint32_t cell_index, glm::ivec3 xyz, float x, float y, float z)
{
	DualVertex dv;
	calculate_vertex((uint32_t)vertices.count, &dv, xyz, x, y, z, 0);
	vertices.push_back(dv);
	inds[cell_index] = dv.index;
	return dv.index;
}

void BinaryChunk::calculate_cell(int x, int y, int z, uint32_t index, uint8_t mask, glm::vec3 start, float delta, int z_per_y, int z_per_x)
{
	bool solid = (mask & 1);
	bool solid_right = (mask >> 4) & 1;
	bool solid_up = (mask >> 2) & 1;
	bool solid_forward = (mask >> 1) & 1;
	if (solid == solid_right && solid_right == solid_up && solid_up == solid_forward)
		return;

	float dx = start.x + (float)x * delta;
	float dy = start.y + (float)y * delta;
	float dz = start.z + (float)z * delta;

	uint32_t c, x1y0z0 = 0, x0y1z0 = 0, x1y1z0 = 0, x1y1z1 = 0, x1y0z1 = 0, x0y0z1 = 0, x0y1z1 = 0;
	uint32_t out_mask;

	if (solid != solid_right)
	{
		c = encode_cell(x + 1, y, z, dim);
		x1y0z0 = inds[c];
		if ((int)x1y0z0 < 0)
			x1y0z0 = create_vertex(c, ivec3(x + 1, y, z), dx + delta, dy, dz);
	}

	if (solid != solid_up)
	{
		c = encode_cell(x, y + 1, z, dim);
		x0y1z0 = inds[c];
		if ((int)x0y1z0 < 0)
			x0y1z0 = create_vertex(c, ivec3(x, y + 1, z), dx, dy + delta, dz);
	}

	if (solid != solid_right || solid != solid_up)
	{
		c = encode_cell(x + 1, y + 1, z, dim);
		x1y1z0 = inds[c];
		if ((int)x1y1z0 < 0)
			x1y1z0 = create_vertex(c, ivec3(x, y, z + 1), dx + delta, dy + delta, dz);
	}

	c = encode_cell(x + 1, y + 1, z + 1, dim);
	x1y1z1 = inds[c];
	if ((int)x1y1z1 < 0)
		x1y1z1 = create_vertex(c, ivec3(x + 1, y + 1, z + 1), dx + delta, dy + delta, dz + delta);

	if (solid != solid_right || solid != solid_forward)
	{
		c = encode_cell(x + 1, y, z + 1, dim);
		x1y0z1 = inds[c];
		if ((int)x1y0z1 < 0)
			x1y0z1 = create_vertex(c, ivec3(x + 1, y, z + 1), dx + delta, dy, dz + delta);
	}

	if (solid != solid_forward)
	{
		c = encode_cell(x, y, z + 1, dim);
		x0y0z1 = inds[c];
		if ((int)x0y0z1 < 0)
			x0y0z1 = create_vertex(c, ivec3(x, y, z + 1), dx, dy, dz + delta);
	}

	if (solid != solid_up || solid != solid_forward)
	{
		c = encode_cell(x, y + 1, z + 1, dim);
		x0y1z1 = inds[c];
		if ((int)x0y1z1 < 0)
			x0y1z1 = create_vertex(c, ivec3(x, y + 1, z + 1), dx, dy + delta, dz + delta);
	}

	// Right face
	if (solid && !solid_right)
	{
		mesh_indexes.push_back(x1y0z0);
		mesh_indexes.push_back(x1y1z0);
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x1y0z1);

		vertices[x1y0z0].init_valence++;
		vertices[x1y1z0].init_valence++;
		vertices[x1y1z1].init_valence++;
		vertices[x1y0z1].init_valence++;
	}
	else if (!solid && solid_right)
	{
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x1y1z0);
		mesh_indexes.push_back(x1y0z0);
		mesh_indexes.push_back(x1y0z1);

		vertices[x1y1z1].init_valence++;
		vertices[x1y1z0].init_valence++;
		vertices[x1y0z0].init_valence++;
		vertices[x1y0z1].init_valence++;
	}

	// Top face
	if (!solid && solid_up)
	{
		mesh_indexes.push_back(x0y1z0);
		mesh_indexes.push_back(x1y1z0);
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x0y1z1);

		vertices[x0y1z0].init_valence++;
		vertices[x1y1z0].init_valence++;
		vertices[x1y1z1].init_valence++;
		vertices[x0y1z1].init_valence++;
	}
	else if (solid && !solid_up)
	{
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x1y1z0);
		mesh_indexes.push_back(x0y1z0);
		mesh_indexes.push_back(x0y1z1);

		vertices[x1y1z1].init_valence++;
		vertices[x1y1z0].init_valence++;
		vertices[x0y1z0].init_valence++;
		vertices[x0y1z1].init_valence++;
	}

	// Front face
	if (solid && !solid_forward)
	{
		mesh_indexes.push_back(x0y0z1);
		mesh_indexes.push_back(x1y0z1);
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x0y1z1);

		vertices[x0y0z1].init_valence++;
		vertices[x1y0z1].init_valence++;
		vertices[x1y1z1].init_valence++;
		vertices[x0y1z1].init_valence++;
	}
	else if (!solid && solid_forward)
	{
		mesh_indexes.push_back(x1y1z1);
		mesh_indexes.push_back(x1y0z1);
		mesh_indexes.push_back(x0y0z1);
		mesh_indexes.push_back(x0y1z1);

		vertices[x1y1z1].init_valence++;
		vertices[x1y0z1].init_valence++;
		vertices[x0y0z1].init_valence++;
		vertices[x0y1z1].init_valence++;
	}
}

uint32_t BinaryChunk::encode_vertex(uint32_t x, uint32_t y, uint32_t z, uint32_t dim, uint32_t z_per_y, uint32_t y_per_x, uint32_t& out_mask)
{
	uint32_t index = (z / 32) + (y * z_per_y) + (x * y_per_x);
	out_mask = 1 << (z % 32);
	return index;
}

uint32_t BinaryChunk::encode_vertex(glm::uvec3 xyz)
{
	return 0;
	//return encode_vertex(xyz[0], xyz[1], xyz[2]);
}

uint32_t BinaryChunk::encode_cell(uint32_t x, uint32_t y, uint32_t z, uint32_t dim)
{
	return x * dim * dim + y * dim + z;
}

void BinaryChunk::copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	mesh_offset = v_out.count;
	size_t start = i_out.count;
	v_out.push_back(vertices);
	i_out.push_back(mesh_indexes);

	for (size_t i = start; i < i_out.count; i++)
	{
		i_out[i] += mesh_offset;
	}
}

double BinaryChunk::extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent)
{
	using namespace std;
	double temp_ms = 0, total_ms = 0;
	if (!silent)
		cout << "Extracting base mesh." << endl << "--dim: " << dim << endl << "--level: " << level << endl << "--size: " << setiosflags(ios::fixed) << setprecision(2) << size << endl;

	// Samples
	if (!silent)
		cout << "-Generating samples...";
	clock_t start_clock = clock();
	generate_samples();
	total_ms += clock() - start_clock;
	if (!silent)
		cout << "done (" << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Neighbor info
	if (!silent)
		cout << "-Generating neighbor info...";
	start_clock = clock();
	generate_neighbor_info();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Mesh
	if (!silent)
		cout << "-Generating mesh...";
	start_clock = clock();
	generate_mesh();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Vertex/index output
	if (!silent)
		cout << "-Copying vertices...";
	start_clock = clock();
	copy_verts_and_inds(v_out, i_out);
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Final
	if (!silent)
		cout << "Complete in " << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << " ms. " << vertices.count << " verts, " << (mesh_indexes.count / 4) << " prims." << endl << endl;

	return total_ms;
}
