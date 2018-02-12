#include "PCH.h"
#include "CubicChunk.hpp"
#include "Tables.hpp"
#include <iostream>
#include <queue>
#include <deque>

using namespace glm;

#define MORTON_CODING 0

#define DC_LINE(ox,oy,oz,m) \
	s0 = encode_vertex(xyz.x + (ox), xyz.y + (oy), xyz.z + (oz), dimp1, z_per_y, y_per_x, s0_mask); \
	if (samples[s0] & s0_mask) \
		mask |= m;

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

CubicChunk::CubicChunk() : Chunk()
{
	cell_block = 0;
}

CubicChunk::CubicChunk(glm::vec3 pos, float size, int level, Sampler& sampler,
	bool produce_quads) : Chunk(pos, size, level, sampler, produce_quads)
{
	cell_block = 0;
}

CubicChunk::~CubicChunk()
{
	//free(samples);
	//samples = 0;
}

void CubicChunk::init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads)
{
	Chunk::init(pos, size, level, sampler, produce_quads);
	binary_block = 0;
	//samples = 0;
}

void CubicChunk::generate_samples(ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<FloatBlock>* float_allocator)
{
	assert(sampler.value != nullptr);
	uint32_t dimp1 = dim + 1;
	uint32_t dim_h = (dimp1 + 2) / 2;
	uint32_t z_per_y_chunks = ((dim + 32)) / 32;
	uint32_t y_per_x_chunks = z_per_y_chunks * dimp1;
	uint32_t z_per_y = dim + 1;
	uint32_t y_per_x = z_per_y * dimp1;
	bool positive = false, negative = false;
	uint32_t count = (y_per_x * dimp1);
	uint32_t real_count = ((z_per_y_chunks * 32) * dimp1 * dimp1 + 31) / 32;
	const float scale = 1.0f;

	binary_block = binary_allocator->new_element();
	binary_block->init(dimp1 * dimp1 * dimp1, real_count);
	//samples = (uint32_t*)malloc(sizeof(uint32_t) * real_count);
	//memset(samples, 0, sizeof(uint32_t) * real_count);
	float delta = size / (float)dim;
	const float res = sampler.world_size;
	FloatBlock* float_block = float_allocator->new_element();
	float_block->init(dimp1 * dimp1 * dimp1, dimp1 * dimp1);

	sampler.block(res, pos, ivec3(dimp1, dimp1, dimp1), delta * scale, &float_block->data, &float_block->vectorset, float_block->dest_noise);

	vec3 dxyz;
	auto f = sampler.value;
	uint32_t s0_mask, s0;
	uint32_t z_count = (dim + 32) / 32;
	bool mesh = false;
	for (uint32_t x = 0; x < dimp1; x++)
	{
		for (uint32_t y = 0; y < dimp1; y++)
		{
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				float* block_samples = float_block->data + x * y_per_x + y * z_per_y + z_block * 32;
				uint32_t m = 0;
				uint32_t z_max = dimp1 - z_block * 32;
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
					binary_block->data[x * y_per_x_chunks + y * z_per_y_chunks + z_block] = m;
					if (m != 0xFFFFFFFF)
						mesh = true;
					else if (!mesh)
						negative = true;
				}
				else
				{
					if (!mesh)
						positive = true;
					binary_block->data[x * y_per_x_chunks + y * z_per_y_chunks + z_block] = 0;
				}
				//s0 = encode_vertex(x, y, z, dim_h, z_per_y, y_per_x, s0_mask);
				//float s = f(res, dxyz);
				//float s = block_data[x * dimp1 * dimp1 + y * dimp1 + z];
				//if (s > 0)
				//	positive = true;
				//else
				//{
				//	samples[s0] |= s0_mask;
				//	negative = true;
				//}
			}
		}
	}

	if (!mesh)
		contains_mesh = negative && positive;
	else
		contains_mesh = mesh;

	float_allocator->free_element(float_block);
}

void CubicChunk::generate_dual_vertices(ResourceAllocator<VerticesIndicesBlock>* vi_allocator, ResourceAllocator<CellsBlock>* cell_allocator, ResourceAllocator<IndexesBlock>* inds_allocator)
{
	if (!contains_mesh)
		return;

	if (!vi)
	{
		vi = vi_allocator->new_element();
		vi->init();
	}
	if (!cell_block)
	{
		cell_block = cell_allocator->new_element();
		cell_block->init();
	}

	uint32_t dimp1 = dim + 1;

	uint32_t count = dim * dim * dim;
	uint32_t countp1 = dimp1 * dimp1 * dimp1;
	uint32_t z_per_y = ((dim + 32)) / 32;
	uint32_t y_per_x = z_per_y * dimp1;

	uint32_t z_per_y8 = (dim + 8) / 8;
	uint32_t y_per_x8 = z_per_y8 * dimp1;
	uint32_t count8 = z_per_y8 * y_per_x8 * dim;

	if (!inds_block)
	{
		inds_block = inds_allocator->new_element();
		inds_block->init(count);
	}
	auto& inds = inds_block->inds;

	//inds = (uint32_t*)malloc(sizeof(uint32_t) * count);
	//memset(inds, 0xFFFFFFFF, sizeof(uint32_t) * count);

	uint64_t* __restrict masks = (uint64_t*)malloc(sizeof(uint64_t) * count8);
	//memset(masks, 0, sizeof(uint64_t) * count8);

	uint32_t* samples = binary_block->data;

	uint32_t z_count = (dim + 32) / 32;

	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
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

	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
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

	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
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

	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
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

	//cells.scale = 4;
	//cells.resize(4096);
	//vertices.resize(dim * dim * 16);
	Cell temp;
	auto& cells = cell_block->cells;
	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
		{
			for (uint32_t z = 0; z < dim; z += 8)
			{
				uint64_t mask = masks[x * y_per_x8 + y * z_per_y8 + z / 8];
				if (mask != 0 && mask != 0xFFFFFFFFFFFFFFFF)
				{
					for (int sub_z = 0; sub_z < 8 && z + sub_z < dim; sub_z++)
					{
						uint32_t index = x * dim * dim + y * dim + z + sub_z;
						uint8_t sub_mask = (mask & 0xFF);
						if (sub_mask != 0 && sub_mask != 255)
						{
							calculate_cell(uvec3(x, y, z + sub_z), (uint32_t)vi->vertices.count, &temp, false, sub_mask);
							inds[index] = (uint32_t)cells.count;
							cells.push_back(temp);
							//calculate_dual_vertex(uvec3(x, y, z + sub_z), (uint32_t)vertices.count, &temp, false, sub_mask);
							//inds[index] = (uint32_t)vertices.count;
							//vertices.push_back(temp);
						}
						else if (z + sub_z < dim)
							inds[index] = -1;
						mask >>= 8;
					}
				}
				else
				{
					inds[x * dim * dim + y * dim + z + 0] = -1;
					if (z + 1 < dim)
					{
						inds[x * dim * dim + y * dim + z + 1] = -1;
						inds[x * dim * dim + y * dim + z + 2] = -1;
						inds[x * dim * dim + y * dim + z + 3] = -1;
						inds[x * dim * dim + y * dim + z + 4] = -1;
						inds[x * dim * dim + y * dim + z + 5] = -1;
						inds[x * dim * dim + y * dim + z + 6] = -1;
						if (z + 7 < dim)
							inds[x * dim * dim + y * dim + z + 7] = -1;
					}
				}
			}
		}
	}
	//vertices.shrink();
	//cells.shrink();

	free(masks);
}

__forceinline void CubicChunk::calculate_cell(glm::uvec3 xyz, uint32_t next_index, Cell* result, bool force, uint8_t mask)
{
	int n_verts = Tables::NumVertices[mask];
	assert(n_verts > 0);

	result->index = 0x1000;
	result->mask = mask;
	result->xyz = xyz;
	result->v_map[0] = -1;
	result->v_map[1] = -1;
	result->v_map[2] = -1;
	result->v_map[3] = -1;

	uint16_t edge_mask = 0;
	for (int i = 0; i < 12; i++)
	{
		int v0 = Tables::TEdgePairs[i][0];
		int v1 = Tables::TEdgePairs[i][1];

		int c0 = (mask >> v0) & 1;
		int c1 = (mask >> v1) & 1;
		if (c0 == c1)
			continue;

		edge_mask |= 1 << i;
	}
	result->edge_mask = edge_mask;

	int v_index = 0;
	uint32_t edge_map = 0;
	bool boundary = force || xyz.x <= 0 || xyz.y <= 0 || xyz.z <= 0 || xyz.x >= dim - 1 || xyz.y >= dim - 1 || xyz.z >= dim - 1;
	for (int i = 0; i < 22; i++)
	{
		int e = Tables::EdgeTable[mask][i];
		if (e == -1)
		{
			v_index++;
			if (boundary)
				break;
			continue;
		}
		if (e == -2)
		{
			v_index++;
			//assert(v_index == n_verts);
			break;
		}
		assert(((edge_map >> (e * 2)) & 3) == 0);
		if (((edge_map >> (e * 2)) & 3) != 0)
			printf("Hmm\n");
		edge_map |= (v_index) << (e * 2);
	}

	//assert(v_index == n_verts);

	for (int i = 0; i < v_index; i++)
	{
		DualVertex v;
		calculate_dual_vertex(xyz, (uint32_t)vi->vertices.count, &v, false, mask, glm::vec3(0, 0, 0));
		result->v_map[i] = (uint32_t)vi->vertices.count;
		vi->vertices.push_back(v);
	}

	result->edge_map = edge_map;
}

__forceinline bool CubicChunk::calculate_dual_vertex(glm::uvec3 xyz, uint32_t next_index, DualVertex* result, bool force, uint8_t mask, glm::vec3 pos_override)
{
	result->valence = 0;
	result->init_valence = 0;
	result->index = next_index;
	result->xyz = xyz;
	result->edge_mask = 0;
	result->mask = mask;
	result->s = 0;

	/*uint16_t edge_mask = 0;
	for (int i = 0; i < 12; i++)
	{
		int v0 = Tables::TEdgePairs[i][0];
		int v1 = Tables::TEdgePairs[i][1];

		int c0 = (mask >> v0) & 1;
		int c1 = (mask >> v1) & 1;
		if (c0 == c1)
			continue;

		edge_mask |= 1 << i;
	}
	result->edge_mask = edge_mask;*/
	result->edge_mask = 0;

	if (!force)
		result->p = pos + vec3((float)xyz.x + 0.5f, (float)xyz.y + 0.5f, (float)xyz.z + 0.5f) * size / (float)dim;
	else
		result->p = pos_override;
	result->n = vec3(0, 0, 0);
	result->color = vec3(1, 1, 1);

	/*float y = result->p.y;
	if (y >= 200.0f)
		result->color = vec3(1, 1, 1);
	else if (y >= 120.0f)
		result->color = vec3(0.3f, 1.0f, 0.0f);
	else if(y >= 45.0f)
		result->color = vec3(0.5f, 0.35f, 0.125f);
	else
		result->color = vec3(0.1f, 0.1f, 0.1f);*/

	return true;
}

uint32_t CubicChunk::encode_vertex(uint32_t x, uint32_t y, uint32_t z, uint32_t dim, uint32_t z_per_y, uint32_t y_per_x, uint32_t& out_mask)
{
	uint32_t index = (z / 32) + (y * z_per_y) + (x * y_per_x);
	out_mask = 1 << (z % 32);
	return index;
}

uint32_t CubicChunk::encode_vertex(glm::uvec3 xyz)
{
	return 0;
	//return encode_vertex(xyz[0], xyz[1], xyz[2]);
}

void CubicChunk::calculate_valences()
{
	if (!contains_mesh)
		return;

	//assert(vertices.count > 0);
	const int masks[] = { 3, 7, 11 };
	const int de[][3][3] = {
		{ { 0, 0, 1 },{ 0, 1, 0 },{ 0, 1, 1 } },
	{ { 0, 0, 1 },{ 1, 0, 0 },{ 1, 0, 1 } },
	{ { 0, 1, 0 },{ 1, 0, 0 },{ 1, 1, 0 } }
	};

	auto& vertices = vi->vertices;
	auto& inds = inds_block->inds;

	vi->mesh_indexes.prepare(vertices.count * 6);
	size_t v_count = vertices.count;
	uint32_t v_inds[4];
	for (uint32_t i = 0; i < v_count; i++)
	{
		DualVertex dv = vertices[i];
		uvec3 xyz = dv.xyz;
		v_inds[0] = dv.index;
		for (int i = 0; i < 3; i++)
		{
			if (((dv.edge_mask >> masks[i]) & 1) != 0)
			{
				if (xyz.x + de[i][0][0] >= dim || xyz.x + de[i][1][0] >= dim || xyz.x + de[i][2][0] >= dim)
					continue;
				if (xyz.y + de[i][0][1] >= dim || xyz.y + de[i][1][1] >= dim || xyz.y + de[i][2][1] >= dim)
					continue;
				if (xyz.z + de[i][0][2] >= dim || xyz.z + de[i][1][2] >= dim || xyz.z + de[i][2][2] >= dim)
					continue;

				v_inds[1] = inds[(xyz.x + de[i][0][0]) * dim * dim + (xyz.y + de[i][0][1]) * dim + (xyz.z + de[i][0][2])];
				v_inds[2] = inds[(xyz.x + de[i][1][0]) * dim * dim + (xyz.y + de[i][1][1]) * dim + (xyz.z + de[i][1][2])];
				v_inds[3] = inds[(xyz.x + de[i][2][0]) * dim * dim + (xyz.y + de[i][2][1]) * dim + (xyz.z + de[i][2][2])];

				if (quads)
				{
					vertices[v_inds[0]].valence++;
					vertices[v_inds[1]].valence++;
					vertices[v_inds[2]].valence++;
					vertices[v_inds[3]].valence++;
				}
				else
				{
					vertices[v_inds[0]].valence += 2;
					vertices[v_inds[1]].valence += 1;
					vertices[v_inds[2]].valence += 1;
					vertices[v_inds[3]].valence += 2;
				}
			}
		}
	}
}

uint32_t CubicChunk::collapse_bad_cells()
{
	if (!contains_mesh)
		return 0;

	uint32_t bad_count = 0;
	size_t v_count = vi->vertices.count;
	uint32_t v_inds[4];

	const int masks[] = { 3, 7, 11 };
	const int de[][3][3] = {
		{ { 0, 0, 1 },{ 0, 1, 0 },{ 0, 1, 1 } },
	{ { 0, 0, 1 },{ 1, 0, 0 },{ 1, 0, 1 } },
	{ { 0, 1, 0 },{ 1, 0, 0 },{ 1, 1, 0 } }
	};

	auto& vertices = vi->vertices;
	auto& inds = inds_block->inds;

	for (uint32_t i = 0; i < v_count; i++)
	{
		DualVertex& dv = vertices[i];
		if (dv.valence == 3)
		{
			v_inds[0] = dv.index;
			uvec3 xyz = dv.xyz;
			for (int i = 0; i < 3; i++)
			{
				if (((dv.edge_mask >> masks[i]) & 1) != 0)
				{
					if (xyz.x + de[i][0][0] >= dim || xyz.x + de[i][1][0] >= dim || xyz.x + de[i][2][0] >= dim)
						continue;
					if (xyz.y + de[i][0][1] >= dim || xyz.y + de[i][1][1] >= dim || xyz.y + de[i][2][1] >= dim)
						continue;
					if (xyz.z + de[i][0][2] >= dim || xyz.z + de[i][1][2] >= dim || xyz.z + de[i][2][2] >= dim)
						continue;

					v_inds[1] = inds[(xyz.x + de[i][0][0]) * dim * dim + (xyz.y + de[i][0][1]) * dim + (xyz.z + de[i][0][2])];
					v_inds[2] = inds[(xyz.x + de[i][1][0]) * dim * dim + (xyz.y + de[i][1][1]) * dim + (xyz.z + de[i][1][2])];
					v_inds[3] = inds[(xyz.x + de[i][2][0]) * dim * dim + (xyz.y + de[i][2][1]) * dim + (xyz.z + de[i][2][2])];

					DualVertex& other = vertices[v_inds[3]];

					dv.p = other.p;
					dv.index = other.index;
					inds[encode_cell(dv.xyz)] = other.index;
					dv.valence = 0;
					other.valence = 0;
				}
			}
			bad_count++;
		}
		dv.valence = 0;
	}

	return bad_count;
}

void CubicChunk::generate_base_mesh(ResourceAllocator<VerticesIndicesBlock>* vi_allocator)
{
	if (!contains_mesh)
		return;

	if (!vi)
	{
		vi = vi_allocator->new_element();
		vi->init();
	}

	//assert(vertices.count > 0);
	const int edge_masks[] = { 3, 7, 11 };
	const int de[][3][3] = {
		{ { 0, 0, 1 },{ 0, 1, 0 },{ 0, 1, 1 } },
	{ { 0, 0, 1 },{ 1, 0, 0 },{ 1, 0, 1 } },
	{ { 0, 1, 0 },{ 1, 0, 0 },{ 1, 1, 0 } }
	};

	const int edge_maps[][3] =
	{
		{ 2, 1, 0 },
		{ 6, 5, 4 },
		{ 10, 9, 8 }
	};

	auto& vertices = vi->vertices;
	auto& mesh_indexes = vi->mesh_indexes;
	auto& cells = cell_block->cells;
	auto& inds = inds_block->inds;

	mesh_indexes.prepare_exact(vertices.count * 6);
	size_t c_count = cells.count;
	uint32_t c_inds[4];
	for (uint32_t i = 0; i < c_count; i++)
	{
		Cell& c = cells[i];
		if (c.xyz.x == 27 && c.xyz.y == 5 && c.xyz.z == 0)
		{
			//printf("F");
		}
		// TODO: Get xyz from cell vertices
		uvec3 xyz = c.xyz;
		c_inds[0] = i;
		for (int i = 0; i < 3; i++)
		{
			if (((c.edge_mask >> edge_masks[i]) & 1) != 0)
			{
				if (xyz.x + de[i][0][0] >= dim || xyz.x + de[i][1][0] >= dim || xyz.x + de[i][2][0] >= dim)
					continue;
				if (xyz.y + de[i][0][1] >= dim || xyz.y + de[i][1][1] >= dim || xyz.y + de[i][2][1] >= dim)
					continue;
				if (xyz.z + de[i][0][2] >= dim || xyz.z + de[i][1][2] >= dim || xyz.z + de[i][2][2] >= dim)
					continue;

				c_inds[1] = inds[(xyz.x + de[i][0][0]) * dim * dim + (xyz.y + de[i][0][1]) * dim + (xyz.z + de[i][0][2])];
				c_inds[2] = inds[(xyz.x + de[i][1][0]) * dim * dim + (xyz.y + de[i][1][1]) * dim + (xyz.z + de[i][1][2])];
				c_inds[3] = inds[(xyz.x + de[i][2][0]) * dim * dim + (xyz.y + de[i][2][1]) * dim + (xyz.z + de[i][2][2])];

				if (c_inds[1] == -1 || c_inds[2] == -1 || c_inds[3] == -1)
					continue;

				/*if (c_inds[0] == c_inds[1] || c_inds[0] == c_inds[2] || c_inds[0] == c_inds[3] || c_inds[1] == c_inds[2] || c_inds[1] == c_inds[3] || c_inds[2] == c_inds[3])
					continue;*/

				bool flip = (c.mask >> Tables::TEdgePairs[edge_masks[i]][0]) & 1;
				if (i == 1)
					flip = !flip;

				uint32_t v_inds[4];
				v_inds[0] = c.v_map[(c.edge_map >> (2 * edge_masks[i])) & 3];
				Cell t_cells[3] = { cells[c_inds[1]], cells[c_inds[2]], cells[c_inds[3]] };
				v_inds[1] = cells[c_inds[1]].v_map[(t_cells[0].edge_map >> (2 * edge_maps[i][0])) & 3];
				v_inds[2] = cells[c_inds[2]].v_map[(t_cells[1].edge_map >> (2 * edge_maps[i][1])) & 3];
				v_inds[3] = cells[c_inds[3]].v_map[(t_cells[2].edge_map >> (2 * edge_maps[i][2])) & 3];

				vertices[v_inds[0]].valence++;
				vertices[v_inds[1]].valence++;
				vertices[v_inds[2]].valence++;
				vertices[v_inds[3]].valence++;

				if (quads)
				{
					if (!flip)
					{
						mesh_indexes.push_back(v_inds[0]);
						mesh_indexes.push_back(v_inds[1]);
						mesh_indexes.push_back(v_inds[3]);
						mesh_indexes.push_back(v_inds[2]);
					}
					else
					{
						mesh_indexes.push_back(v_inds[3]);
						mesh_indexes.push_back(v_inds[1]);
						mesh_indexes.push_back(v_inds[0]);
						mesh_indexes.push_back(v_inds[2]);
					}
				}
				else
				{
					if (!flip)
					{
						if (v_inds[0] != v_inds[1] && v_inds[1] != v_inds[3] && v_inds[3] != v_inds[0] && v_inds[0] != -1 && v_inds[1] != -1 && v_inds[3] != -1)
						{
							mesh_indexes.push_back(v_inds[0]);
							mesh_indexes.push_back(v_inds[1]);
							mesh_indexes.push_back(v_inds[3]);
							vertices[v_inds[0]].init_valence++;
							vertices[v_inds[1]].init_valence++;
							vertices[v_inds[3]].init_valence++;
						}

						if (v_inds[3] != v_inds[2] && v_inds[2] != v_inds[0] && v_inds[0] != v_inds[3] && v_inds[3] != -1 && v_inds[2] != -1 && v_inds[0] != -1)
						{
							mesh_indexes.push_back(v_inds[3]);
							mesh_indexes.push_back(v_inds[2]);
							mesh_indexes.push_back(v_inds[0]);
							vertices[v_inds[3]].init_valence++;
							vertices[v_inds[2]].init_valence++;
							vertices[v_inds[0]].init_valence++;
						}
					}
					else
					{
						if (v_inds[0] != v_inds[1] && v_inds[1] != v_inds[3] && v_inds[3] != v_inds[0] && v_inds[0] != -1 && v_inds[1] != -1 && v_inds[3] != -1)
						{
							mesh_indexes.push_back(v_inds[3]);
							mesh_indexes.push_back(v_inds[1]);
							mesh_indexes.push_back(v_inds[0]);
							vertices[v_inds[3]].init_valence++;
							vertices[v_inds[1]].init_valence++;
							vertices[v_inds[0]].init_valence++;
						}

						if (v_inds[3] != v_inds[2] && v_inds[2] != v_inds[0] && v_inds[0] != v_inds[3] && v_inds[3] != -1 && v_inds[2] != -1 && v_inds[0] != -1)
						{
							mesh_indexes.push_back(v_inds[0]);
							mesh_indexes.push_back(v_inds[2]);
							mesh_indexes.push_back(v_inds[3]);
							vertices[v_inds[0]].init_valence++;
							vertices[v_inds[2]].init_valence++;
							vertices[v_inds[3]].init_valence++;
						}
					}
				}

				if (!quads)
				{
					/*vertices[v_inds[0]].init_valence += 3;
					vertices[v_inds[1]].init_valence += 2;
					vertices[v_inds[2]].init_valence += 2;
					vertices[v_inds[3]].init_valence += 3;*/
				}
				else
				{
					vertices[v_inds[0]].init_valence += 1;
					vertices[v_inds[1]].init_valence += 1;
					vertices[v_inds[2]].init_valence += 1;
					vertices[v_inds[3]].init_valence += 1;
				}

			}
		}
	}
}

uint32_t get_code(ivec3 p, int level)
{
	uint32_t code = 0;
	int lsh = 0;
	for (int i = 0; i < level; i++)
	{
		uint32_t local_code = ((p.x % 2) << 2) | ((p.y % 2) << 1) | (p.z % 2);
		code |= (local_code << lsh);
		lsh += 3;
		p.x >>= 1;
		p.y >>= 1;
		p.z >>= 1;
	}
	code |= (1 << lsh);
	return code;
}

uint32_t get_code(Cell& c, int level)
{
	return get_code(c.xyz, level);
}

ivec3 get_xyz_from_code(uint32_t code, int max_level)
{
	ivec3 xyz(0, 0, 0);
	int mul = 1;
	for (int i = 0; i < max_level; i++)
	{
		xyz.x += ((code >> 2) & 1) * mul;
		xyz.y += ((code >> 1) & 1) * mul;
		xyz.z += ((code >> 0) & 1) * mul;
		code >>= 3;
		mul <<= 1;
	}

	return xyz;
}

void CubicChunk::generate_octree()
{
	int next_id = 0;
	octree = DualNode(this, next_id++, size, pos, ivec3(0, 0, 0), 0, dim, 0);
	octree.code = 0;
	octree.leaf_flag = false;
	if (!contains_mesh)
	{
		int i_size = dim / 2;
		float c_size = size * 0.5f;
		int c_level = 1;

		for (int i = 0; i < 8; i++)
		{
			ivec3 cxyz = ivec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * (int)i_size;
			octree.children[i] = node_pool.newElement(this, next_id++, c_size, pos + vec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * c_size, cxyz, c_level, i_size, nullptr);
		}
		return;
	}

	using namespace std;
	unordered_map<uint32_t, DualNode*> octree_nodes;
	auto& cells = cell_block->cells;

	float delta = size / (float)dim;
	int leaf_level = (int)log2((float)(dim));
	deque<DualNode*> check_nodes;
	queue<DualNode*> fill_nodes;

	uint32_t c_count = (uint32_t)cells.count;
	for (uint32_t i = 0; i < c_count; i++)
	{
		Cell* c = &cells[i];
		if (c->xyz.x == 0 || c->xyz.y == 0 || c->xyz.z == 0 || c->xyz.x == dim - 1 || c->xyz.y == dim - 1 || c->xyz.z == dim - 1)
		{
			ivec3 xyz = c->xyz;
			uint32_t code = get_code(xyz, leaf_level);
			assert(get_xyz_from_code(code, leaf_level) == xyz);
			DualNode* node = node_pool.newElement(this, next_id++, delta, pos + vec3(xyz) * delta, xyz, leaf_level, 1, c);
			node->code = code;
			octree_nodes[code] = node;
			//if(next_id == 330)
			check_nodes.push_back(node);
		}
	}

	/*for (int x = 0; x < dim; x++)
	{
		for (int y = 0; y < dim; y++)
		{
			for (int z = 0; z < dim; z++)
			{
				if (x != 0 && x != dim - 1 && y != 0 && y != dim - 1 && z != 0 && z != dim - 1)
					continue;

				ivec3 xyz(x, y, z);
				uint32_t code = get_code(xyz, leaf_level);
				assert(get_xyz_from_code(code, leaf_level) == xyz);
				DualNode* node = node_pool.newElement(this, 0, delta, pos + vec3(xyz) * delta, xyz, leaf_level, 1, nullptr);
				node->code = code;
				octree_nodes[code] = node;
				check_nodes.push_back(node);
			}
		}
	}*/

	fill_nodes.push(&octree);

	int node_count = 0;
	while (!check_nodes.empty())
	{
		node_count++;
		DualNode* next = check_nodes.front();
		assert(next);
		check_nodes.pop_front();

		int c_shift = leaf_level - next->level;
		int local_code = (((next->xyz.x >> c_shift) % 2) << 2) | (((next->xyz.y >> c_shift) % 2) << 1) | ((next->xyz.z >> c_shift) % 2);

		if (next->level == 1)
		{
			next->parent = &octree;
		}
		else
		{
			int shift = leaf_level - next->level + 1;
			uint32_t parent_code = next->code >> 3;
			DualNode* parent = octree_nodes[parent_code];
			if (parent)
			{
				assert(!parent->is_leaf());
				next->parent = parent;
			}
			else
			{
				ivec3 n = (next->xyz >> shift) << shift;
				vec3 n_pos = pos + vec3(n >> shift) * next->size * 2.0f;
				parent = node_pool.newElement(this, next_id++, next->size * 2.0f, n_pos, n, next->level - 1, next->i_size * 2, nullptr);
				parent->code = parent_code;
				next->parent = parent;
				octree_nodes[parent_code] = parent;
				fill_nodes.push(parent);
				parent->leaf_flag = false;

				if (parent->level > 0)
					check_nodes.push_back(parent);
			}
		}

		if (next->parent->is_leaf())
		{
			ivec3 pxyz = ((DualNode*)next->parent)->xyz;
			for (int i = 0; i < 8; i++)
			{
				if (i != local_code && !next->parent->children[i])
				{
					//next->parent->children[i] = node_pool.newElement(this, 0, next->size, next->parent->pos + vec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * next->size, pxyz + ivec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * (int)next->i_size, next->level, next->i_size, nullptr);
					//((DualNode*)next->parent->children[i])->code = 0;
				}
			}
		}

		next->parent->children[local_code] = next;
	}

	while (!fill_nodes.empty())
	{
		DualNode* next = fill_nodes.front();
		assert(next);
		fill_nodes.pop();
		int i_size = next->i_size / 2;
		if (next->xyz.x < dim - i_size * 2 && next->xyz.y < dim - i_size * 2 && next->xyz.z < dim - i_size * 2 && next->xyz.x > 0 && next->xyz.y > 0 && next->xyz.z > 0)
			continue;

		int c_shift = leaf_level - next->level - 1;

		float c_size = next->size * 0.5f;
		int c_level = next->level + 1;

		for (int i = 0; i < 8; i++)
		{
			if (!next->children[i])
			{
				ivec3 cxyz = next->xyz + ivec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * (int)i_size;
				/*if (cxyz.x + i_size > dim)
					cxyz.x -= i_size;
				if (cxyz.y + i_size > dim)
					cxyz.y -= i_size;
				if (cxyz.z + i_size > dim)
					cxyz.z -= i_size;*/

				int local_code = (((cxyz.x >> c_shift) % 2) << 2) | (((cxyz.y >> c_shift) % 2) << 1) | ((cxyz.z >> c_shift) % 2);
				//if (!next->children[local_code] || i_size != 1 || true)
				next->children[i] = node_pool.newElement(this, next_id++, c_size, next->pos + vec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * c_size, cxyz, c_level, i_size, nullptr);
				//else
				//	next->children[i] = next->children[local_code];
			}
		}
	}
}

void CubicChunk::filter()
{
	uint32_t dimp1 = dim + 1;
	uint32_t dim_h = (dimp1 + 2) / 2;
	uint32_t z_per_y = ((dim + 32)) / 32;
	uint32_t y_per_x = z_per_y * dimp1;
	uint32_t z_count = (dim + 31) / 32;
	uint32_t* samples = binary_block->data;

	for (uint32_t x = 1; x < dim - 1; x++)
	{
		for (uint32_t y = 1; y < dim - 1; y++)
		{
			for (uint32_t z = 1; z < dim - 1; z++)
			{
				int n_count = 0;
				uint32_t at_mask = 0;
				uint32_t v_at = encode_vertex(x, y, z, dim, z_per_y, y_per_x, at_mask);

				uint32_t n, mask;
				// back
				if (samples[encode_vertex(x, y, z - 1, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				// front
				if (samples[encode_vertex(x, y, z + 1, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				// below
				if (samples[encode_vertex(x, y - 1, z, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				// above
				if (samples[encode_vertex(x, y + 1, z, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				// left
				if (samples[encode_vertex(x - 1, y, z, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				// right
				if (samples[encode_vertex(x + 1, y, z, dim, z_per_y, y_per_x, mask)] & mask)
					n_count++;
				//if (n_count == 1)
				//	samples[v_at] ^= at_mask;
			//	else if (n_count == 5)
				//	samples[v_at] |= at_mask;
			}
			/*for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				uint32_t m = 0;
				uint32_t z_max = dimp1 - z_block * 32;
				if (z_max > 32)
					z_max = 32;
				int neighbor_count = 0;
				uint32_t z = (z_block > 0 ? 0 : 1);
				for (; z < z; z++)
				{
					samples[x * y_per_x_chunks + y * z_per_y_chunks + z_block] = m;
				}
			}*/
		}
	}
}

int CubicChunk::get_internal_node_at(glm::vec3 p)
{
	OctreeNode* n = octree.get_node_at(p);
	if (n)
		return n->index;
	return -1;
}
