#pragma once

#include <unordered_map>
#include "Chunk.hpp"
#include "WorldOctreeNode.hpp"
#include "MemoryPool.h"

class CubicChunk : public Chunk
{
public:
	BinaryBlock* binary_block;
	SmartContainer<Cell> cells;
	DualNode octree;
	MemoryPool<DualNode, 8192> node_pool;

	CubicChunk();
	CubicChunk(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads);
	~CubicChunk();
	void init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads) override;
	void generate_samples(ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<FloatBlock>* float_allocator) final override;
	void generate_dual_vertices() final override;
	__forceinline void calculate_cell(glm::uvec3 xyz, uint32_t next_index, Cell* result, bool force, uint8_t mask);
	__forceinline bool calculate_dual_vertex(glm::uvec3 xyz, uint32_t next_index, DualVertex* result, bool force, uint8_t mask, glm::vec3 pos_override) final override;

	uint32_t encode_vertex(uint32_t x, uint32_t y, uint32_t z, uint32_t dim, uint32_t z_per_y, uint32_t y_per_x, uint32_t& out_mask);
	uint32_t encode_vertex(glm::uvec3 xyz);

	void calculate_valences() final override;
	uint32_t collapse_bad_cells() final override;

	void generate_base_mesh() final override;

	void generate_octree();

	void filter();

	int get_internal_node_at(glm::vec3 p);
};

