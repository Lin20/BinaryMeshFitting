#include "PCH.h"
#include "DMCChunk.hpp"
#include "Tables.hpp"
#include "NoiseSampler.hpp"
#include <iostream>
#include <queue>
#include "MCTable.h"

using namespace glm;

#define MORTON_CODING 0

#define DC_LINE(ox,oy,oz,m) \
	s0 = encode_vertex(xyz.x + (ox), xyz.y + (oy), xyz.z + (oz), dimp1, z_per_y, y_per_x, s0_mask); \
	if (samples[s0] & s0_mask) \
		mask |= m;

#define EDGE_LINE(l_z, z, b) \
if(line & b) { \
if (z_block < z_count || l_z * 8 + z < 32) \
	local_masks[l_z] |= 1ull << (z * 8 + m); \
if (z > 0) \
	local_masks[l_z] |= 1ull << ((z - 1) * 8 + m + 1); \
else if (l_z > 0) \
	local_masks[l_z - 1] |= 1ull << (7 * 8 + m + 1); \
else if (z_block > 0) \
	line_masks[-1] |= 1ull << (7 * 8 + m + 1); \
}

#define EDGE_V(xoff, yoff, zoff, e) cell_block->cells[indexes_block->inds[(x + (xoff)) * dim * dim + (y + (yoff)) * dim + (z + (zoff))]].edges[e].iso_vertex.index

#define RESOLUTION 32

DMCChunk::DMCChunk()
{
	cell_block = 0;
	density_block = 0;
}

DMCChunk::DMCChunk(glm::vec3 pos, float size, int level, Sampler& sampler)
{
	init(pos, size, level, sampler);
}

DMCChunk::~DMCChunk()
{
	if (contains_mesh)
	{
		octree_children.node_pool.~MemoryPool();
	}
	else if (!octree.leaf_flag)
	{

	}
}

void DMCChunk::init(glm::vec3 pos, float size, int level, Sampler& sampler)
{
	this->pos = pos;
	this->dim = RESOLUTION;
	this->size = size;
	this->level = level;
	this->pem = false;

	this->contains_mesh = false;
	this->mesh_offset = 0;

	this->sampler = sampler;
	this->vi = 0;
	this->cell_block = 0;
	this->indexes_block = 0;
	this->density_block = 0;
	this->binary_block = 0;
	this->octree.leaf_flag = true;
}

void DMCChunk::label_grid(ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<IsoVertexBlock>* density_allocator, ResourceAllocator<NoiseBlock>* noise_allocator)
{
	bool positive = false, negative = false;

	assert(sampler.value != nullptr);
	uint32_t dimp1 = dim + 1;
	uint32_t z_per_y_chunks = ((dim + 31)) / 32;
	uint32_t y_per_x_chunks = z_per_y_chunks * dim;
	uint32_t z_per_y = dim;
	uint32_t y_per_x = z_per_y * dim;
	uint32_t real_count = ((z_per_y_chunks * 32) * dim * dim + 31) / 32;

	binary_block = binary_allocator->new_element();
	binary_block->init(dim * dim * dim, real_count);

	float delta = size / (float)dim;
	const float scale = 1.0f;
	const float res = sampler.world_size;
	density_block = density_allocator->new_element();
	density_block->init(dim * dim * dim);

	NoiseBlock* noise_block = noise_allocator->new_element();
	noise_block->init(dim * dim);

	NoiseSamplers::NoiseSamplerProperties properties;
	properties.level = this->level;

	sampler.block(res, pos + vec3(delta * 0.5f, delta * 0.5f, delta * 0.5f), ivec3(dim, dim, dim), delta * scale, (void**)&density_block->data, &noise_block->vectorset, noise_block->dest_noise, sizeof(uint32_t), sizeof(DMC_Isovertex), 0);

	vec3 dxyz;
	auto f = sampler.value;
	uint32_t s0_mask, s0;
	uint32_t z_count = (dim + 31) / 32;
	bool mesh = false;
	for (uint32_t x = 0; x < dim; x++)
	{
		float dx = pos.x + (float)x * delta + delta * 0.5f;
		for (uint32_t y = 0; y < dim; y++)
		{
			float dy = pos.y + (float)y * delta + delta * 0.5f;
			for (uint32_t z_block = 0; z_block < z_count; z_block++)
			{
				DMC_Isovertex* block_samples = density_block->data + x * y_per_x + y * z_per_y + z_block * 32;
				uint32_t m = 0;
				uint32_t z_max = dim - z_block * 32;
				if (z_max > 32)
					z_max = 32;

				for (uint32_t z = 0; z < z_max; z++)
				{
					float dz = pos.z + (float)(z_block * 32 + z) * delta + delta * 0.5f;
					float s = block_samples[z].value;
					if (s < 0.0f)
						m |= 1 << z;
					block_samples[z].position = vec3(dx, dy, dz);
					block_samples[z].index = -1;
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
			}
		}
	}

	if (!mesh)
		contains_mesh = negative && positive;
	else
		contains_mesh = mesh;

	noise_allocator->free_element(noise_block);
	noise_block = 0;
}

void DMCChunk::label_edges(ResourceAllocator<VerticesIndicesBlock>* vi_allocator, ResourceAllocator<DMC_CellsBlock>* cell_allocator, ResourceAllocator<IndexesBlock>* inds_allocator, ResourceAllocator<IsoVertexBlock>* density_allocator)
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

	uint32_t count = dim * dim * dim;
	uint32_t z_per_y = (dim + 31) / 32;
	uint32_t y_per_x = z_per_y * dim;

	uint32_t z_per_y8 = (dim + 7) / 8;
	uint32_t y_per_x8 = z_per_y8 * dim;
	uint32_t count8 = z_per_y8 * y_per_x8 * dim;

	uint64_t* __restrict masks = (uint64_t*)malloc(sizeof(uint64_t) * count8);
	//memset(masks, 0, sizeof(uint64_t) * count8);

	uint32_t* samples = binary_block->data;

	uint32_t z_count = (dim + 31) / 32;

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
		for (uint32_t y = 0; y < dim - 1; y++)
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

	for (uint32_t x = 0; x < dim - 1; x++)
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

	for (uint32_t x = 0; x < dim - 1; x++)
	{
		for (uint32_t y = 0; y < dim - 1; y++)
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

	indexes_block = inds_allocator->new_element();
	indexes_block->init(dim * dim * dim);

	auto& cells = cell_block->cells;
	auto& inds = indexes_block->inds;

	int local_dim = dim;

	DMC_Cell temp;
	for (uint32_t x = 0; x < local_dim; x++)
	{
		for (uint32_t y = 0; y < local_dim; y++)
		{
			for (uint32_t z = 0; z < local_dim; z += 8)
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
							calculate_cell(x, y, z + sub_z, (uint32_t)vi->vertices.count, sub_mask, temp, local_dim);
							inds[index] = (uint32_t)cells.count;
							cells.push_back(temp);

							if (temp.edges[0].grid_v1 != -1)
								vi->vertices.push_back(calculate_dual_vertex(temp.edges[0].iso_vertex));
							if (temp.edges[1].grid_v1 != -1)
								vi->vertices.push_back(calculate_dual_vertex(temp.edges[1].iso_vertex));
							if (temp.edges[2].grid_v1 != -1)
								vi->vertices.push_back(calculate_dual_vertex(temp.edges[2].iso_vertex));

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
	//if (USE_DENSITIES)
	{
		//density_allocator->free_element(density_block);
		//density_block = 0;
	}
}

void DMCChunk::snap_verts()
{
}

void DMCChunk::polygonize()
{
	if (!contains_mesh)
		return;

	int count = (int)cell_block->cells.count;
	int dim = (int)this->dim;
	auto& inds = this->vi->mesh_indexes;

	for (int i = 0; i < count; i++)
	{
		DMC_Cell& cell = cell_block->cells[i];
		int x = cell.edges[0].grid_v0 / dim / dim, y = cell.edges[0].grid_v0 / dim % dim, z = cell.edges[0].grid_v0 % dim;
		if (x >= dim - 1 || y >= dim - 1 || z >= dim - 1)
			continue;

		assert(cell.mask != 0 && cell.mask != 255);

		polygonize_cell(cell, cell.edges[0].grid_v0 / dim / dim, cell.edges[0].grid_v0 / dim % dim, cell.edges[0].grid_v0 % dim, dim, inds);
	}
}

void DMCChunk::polygonize_cell(DMC_Cell& _c, int x, int y, int z, int dim, SmartContainer<uint32_t>& inds)
{
	DMC_ImmediateCell cell;
	cell.mask = _c.mask;
	int edgemap = MarchingCubes::edge_map[cell.mask];

	cell.iso_verts[0] = _c.edges[0].iso_vertex.index;
	if (edgemap & (1 << 1))
		cell.iso_verts[1] = EDGE_V(0, 0, 1, 0);
	if (edgemap & (1 << 2))
		cell.iso_verts[2] = EDGE_V(0, 1, 0, 0);
	if (edgemap & (1 << 3))
		cell.iso_verts[3] = EDGE_V(0, 1, 1, 0);

	cell.iso_verts[4] = _c.edges[1].iso_vertex.index;
	if (edgemap & (1 << 5))
		cell.iso_verts[5] = EDGE_V(0, 0, 1, 1);
	if (edgemap & (1 << 6))
		cell.iso_verts[6] = EDGE_V(1, 0, 0, 1);
	if (edgemap & (1 << 7))
		cell.iso_verts[7] = EDGE_V(1, 0, 1, 1);

	cell.iso_verts[8] = _c.edges[2].iso_vertex.index;
	if (edgemap & (1 << 9))
		cell.iso_verts[9] = EDGE_V(0, 1, 0, 2);
	if (edgemap & (1 << 10))
		cell.iso_verts[10] = EDGE_V(1, 0, 0, 2);
	if (edgemap & (1 << 11))
		cell.iso_verts[11] = EDGE_V(1, 1, 0, 2);

	for (int i = 0; i < 16; i++)
	{
		int e = MarchingCubes::tri_table[cell.mask][i];
		if (e == -1)
			break;
		inds.push_back(cell.iso_verts[e]);
	}
}

void DMCChunk::copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	if (!contains_mesh)
		return;
	mesh_offset = v_out.count;
	size_t start = i_out.count;
	v_out.push_back(vi->vertices);
	i_out.push_back(vi->mesh_indexes);

	for (size_t i = start; i < i_out.count; i++)
	{
		i_out[i] += mesh_offset;
	}
}

void DMCChunk::calculate_cell(int x, int y, int z, uint32_t next_v_index, uint8_t mask, DMC_Cell& dest, int dim)
{
	assert(mask != 0 && mask != 255);

	dest.mask = mask;

	// X axis
	if ((mask & 1) != ((mask >> 4) & 1) && x + 1 < dim)
	{
		DMC_Edge e;
		e.snapped = false;
		e.grid_v0 = x * dim * dim + y * dim + z;
		e.grid_v1 = (x + 1) * dim * dim + y * dim + z;
		e.length = 0.0f;

		calculate_isovertex(x, y, z, x + 1, y, z, next_v_index++, dim, e.iso_vertex);

		dest.edges[0] = e;
	}
	else
	{
		dest.edges[0].grid_v0 = x * dim * dim + y * dim + z;
		dest.edges[0].grid_v1 = -1;
	}

	// Y axis
	if ((mask & 1) != ((mask >> 2) & 1) && y + 1 < dim)
	{
		DMC_Edge e;
		e.snapped = false;
		e.grid_v0 = x * dim * dim + y * dim + z;
		e.grid_v1 = x * dim * dim + (y + 1) * dim + z;
		e.length = 0.0f;

		calculate_isovertex(x, y, z, x, y + 1, z, next_v_index++, dim, e.iso_vertex);

		dest.edges[1] = e;
	}
	else
	{
		dest.edges[1].grid_v0 = x * dim * dim + y * dim + z;
		dest.edges[1].grid_v1 = -1;
	}

	// Z axis
	if ((mask & 1) != ((mask >> 1) & 1) && z + 1 < dim)
	{
		DMC_Edge e;
		e.snapped = false;
		e.grid_v0 = x * dim * dim + y * dim + z;
		e.grid_v1 = x * dim * dim + y * dim + z + 1;
		e.length = 0.0f;

		calculate_isovertex(x, y, z, x, y, z + 1, next_v_index++, dim, e.iso_vertex);

		dest.edges[2] = e;
	}
	else
	{
		dest.edges[2].grid_v0 = x * dim * dim + y * dim + z;
		dest.edges[2].grid_v1 = -1;
	}
}

__forceinline vec3 _get_intersection(vec3& v0, vec3& v1, float s0, float s1, float isolevel)
{
	float mu = (isolevel - s0) / (s1 - s0);
	vec3 delta_v = (v1 - v0) * mu;
	return delta_v + v0;
}

void DMCChunk::calculate_isovertex(int x0, int y0, int z0, int x1, int y1, int z1, int index, int dim, DMC_Isovertex& out)
{
	out.index = index;
	DMC_Isovertex& v0 = density_block->data[x0 * dim * dim + y0 * dim + z0];
	DMC_Isovertex& v1 = density_block->data[x1 * dim * dim + y1 * dim + z1];
	out.position = _get_intersection(v0.position, v1.position, v0.value, v1.value, 0.0f);
	out.value = 0.0f;
}

DualVertex DMCChunk::calculate_dual_vertex(DMC_Isovertex& in)
{
	DualVertex dv;
	dv.index = in.index;
	dv.p = in.position;
	dv.color = vec3(1, 1, 1);

	return dv;
}

float get_sample(int x, int y, int z, int dim, int size, IsoVertexBlock* density_block)
{
	if (!density_block)
		return 0.0f;

	size /= 2;
	return density_block->data[(x + size) * dim * dim + (y + size) * dim + (z + size)].value;
}

void DMCChunk::generate_octree()
{
	if (!octree.leaf_flag)
		return;
	int next_id = 0;
	octree = DMCNode(this, size, pos, ivec3(0, 0, 0), 0, dim, 0.0f);
	octree.leaf_flag = false;
	int local_dim = dim;

	if (!contains_mesh)
	{
		int i_size = dim / 2;
		float c_size = size * 0.5f;
		int c_level = 1;

		for (int i = 0; i < 8; i++)
		{
			ivec3 cxyz = ivec3(Tables::MCDX[i], Tables::MCDY[i], Tables::MCDZ[i]) * (int)i_size;
			octree_children.children[i] = DMCNode(this, c_size, pos + vec3(Tables::MCDX[i], Tables::MCDY[i], Tables::MCDZ[i]) * c_size, cxyz, c_level, i_size, get_sample(cxyz.x, cxyz.y, cxyz.z, local_dim, i_size, density_block));
			octree.children[i] = &octree_children.children[i];
		}
		return;
	}

	using namespace std;
	queue<DMCNode*> next_split;
	next_split.push(&octree);
	int one_count = 0;


	while (!next_split.empty())
	{
		DMCNode* n = next_split.front();
		assert(n->i_size > 1);

		int i_size = n->i_size / 2;
		float c_size = n->size * 0.5f;
		int c_level = n->level + 1;
		n->leaf_flag = false;
		for (int i = 0; i < 8; i++)
		{
			ivec3 cxyz = n->xyz + ivec3(Tables::MCDX[i], Tables::MCDY[i], Tables::MCDZ[i]) * (int)i_size;
			if (cxyz.x == 0 || cxyz.y == 0 || cxyz.z == 0 || cxyz.x == local_dim - i_size || cxyz.y == local_dim - i_size || cxyz.z == local_dim - i_size)
			{
				float s = 0.0f;
				if (i_size == 1)
				{
					s = get_sample(cxyz.x, cxyz.y, cxyz.z, local_dim, i_size, density_block);
					one_count++;
				}
				n->children[i] = octree_children.node_pool.newElement(this, c_size, n->pos + vec3(Tables::MCDX[i], Tables::MCDY[i], Tables::MCDZ[i]) * c_size, cxyz, c_level, i_size, s);
				if (i_size > 1)
					next_split.push((DMCNode*)n->children[i]);
			}
			else
				n->children[i] = 0;
		}

		next_split.pop();
	}
}

int DMCChunk::get_internal_node_at(glm::vec3 p)
{
	return 0;
}

double DMCChunk::extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent)
{
	using namespace std;
	double temp_ms = 0, total_ms = 0;
	if (!silent)
		cout << "Extracting DMC chunk." << endl << "--dim: " << dim << endl << "--size: " << setiosflags(ios::fixed) << setprecision(2) << size << endl << "--pem: " << (pem ? "yes" : "no") << endl;

	ResourceAllocator<BinaryBlock> binary_allocator;
	ResourceAllocator<IsoVertexBlock> isovertex_allocator;
	ResourceAllocator<NoiseBlock> noise_allocator;
	ResourceAllocator<VerticesIndicesBlock> vi_allocator;
	ResourceAllocator<DMC_CellsBlock> cell_allocator;
	ResourceAllocator<IndexesBlock> indexes_allocator;

	if (!silent)
		cout << "-Labeling grid...";
	clock_t start_clock = clock();
	label_grid(&binary_allocator, &isovertex_allocator, &noise_allocator);
	total_ms += clock() - start_clock;
	if (!silent)
		cout << "done (" << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	if (!silent)
		cout << "-Labeling edges...";
	start_clock = clock();
	label_edges(&vi_allocator, &cell_allocator, &indexes_allocator, &isovertex_allocator);
	double delta = clock() - start_clock;
	total_ms += delta;
	if (!silent)
		cout << "done (" << (int)(delta / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	if (!silent)
		cout << "-Polygonizing...";
	start_clock = clock();
	polygonize();
	delta = clock() - start_clock;
	total_ms += delta;
	if (!silent)
		cout << "done (" << (int)(delta / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	if (!silent)
		cout << "-Formatting mesh...";
	start_clock = clock();
	copy_verts_and_inds(v_out, i_out);
	delta = clock() - start_clock;
	total_ms += delta;
	if (!silent)
		cout << "done (" << (int)(delta / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Final
	if (!silent)
		cout << "Complete in " << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << " ms. " << v_out.count << " verts, " << (i_out.count / 3) << " prims." << endl << endl;

	return total_ms;
}
