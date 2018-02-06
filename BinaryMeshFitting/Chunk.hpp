#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#define GLM_FORCE_NO_CTOR_INIT
#define GLM_FORCE_INLINE
#include <glm/glm.hpp>
#include <vector>

#include "Vertices.hpp"
#include "SmartContainer.hpp"
#include "Sampler.hpp"
#include "ResourceAllocator.hpp"
#include "ChunkBlocks.hpp"

class Chunk
{
public:
	int id;
	glm::vec3 pos;
	uint32_t dim;
	int level;
	float size;
	bool quads;
	bool dirty;

	bool contains_mesh;
	uint32_t mesh_offset;

	Sampler sampler;

	VerticesIndicesBlock* vi;
	IndexesBlock* inds_block;

	Chunk();
	Chunk(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads);
	virtual ~Chunk() = 0;
	virtual void init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads);
	virtual void generate_samples(ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<FloatBlock>* float_allocator) = 0;
	virtual void generate_dual_vertices(ResourceAllocator<VerticesIndicesBlock>* vi_allocator, ResourceAllocator<CellsBlock>* cell_allocator, ResourceAllocator<IndexesBlock>* inds_allocator) = 0;
	virtual bool calculate_dual_vertex(glm::uvec3 xyz, uint32_t next_index, DualVertex* result, bool force, uint8_t mask, glm::vec3 pos_override) = 0;

	virtual uint32_t encode_vertex(glm::uvec3 xyz);
	virtual uint32_t encode_cell(glm::uvec3 xyz);

	virtual void generate_base_mesh(ResourceAllocator<VerticesIndicesBlock>* vi_allocator) = 0;
	virtual void calculate_valences() = 0;
	virtual uint32_t collapse_bad_cells() = 0;
	virtual void copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);

	double extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent);
};
