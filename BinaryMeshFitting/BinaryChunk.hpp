#pragma once

#include <unordered_map>
#include "Chunk.hpp"
#include "WorldOctreeNode.hpp"
#include "MemoryPool.h"

class BinaryChunk
{
public:
	int id;
	glm::vec3 pos;
	uint32_t dim;
	int level;
	float size;

	bool contains_mesh;
	uint32_t mesh_offset;

	uint32_t* inds;
	uint64_t* __restrict masks;
	Sampler sampler;

	SmartContainer<DualVertex> vertices;
	SmartContainer<uint32_t> mesh_indexes;

	uint32_t* samples;
	float* block_data = 0;
	MemoryPool<DualNode, 131072> node_pool;

	BinaryChunk();
	BinaryChunk(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads);
	~BinaryChunk();
	void init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads);
	void generate_samples();
	void generate_neighbor_info();
	void generate_mesh();

	__forceinline bool calculate_vertex(uint32_t next_index, DualVertex* result, glm::ivec3 xyz,  float x, float y, float z, uint8_t mask);
	__forceinline uint32_t create_vertex(uint32_t cell_index, glm::ivec3 xyz, float x, float y, float z);

	__forceinline void calculate_cell(int x, int y, int z, uint32_t index, uint8_t mask, glm::vec3 start, float delta, int z_per_y, int z_per_x);

	uint32_t encode_vertex(uint32_t x, uint32_t y, uint32_t z, uint32_t dim, uint32_t z_per_y, uint32_t y_per_x, uint32_t& out_mask);
	uint32_t encode_vertex(glm::uvec3 xyz);
	uint32_t encode_cell(uint32_t x, uint32_t y, uint32_t z, uint32_t dim);

	void copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);

	//void generate_base_mesh();

	//void generate_octree();

	//int get_internal_node_at(glm::vec3 p);

	double extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent);
};

