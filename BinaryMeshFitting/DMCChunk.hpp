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
#include "WorldOctreeNode.hpp"
#include "MemoryPool.h"
#include "HashMap.hpp"

class DMCChunk
{
public:
	bool pem;
	int id;
	int level;
	uint32_t dim;
	float size;
	glm::vec3 pos;
	float snap_threshold;

	bool contains_mesh;
	uint32_t mesh_offset;
	uint64_t parent_code;

	Sampler sampler;

	VerticesIndicesBlock* vi;

	IsoVertexBlock* density_block;
	BinaryBlock* binary_block;
	DMC_CellsBlock* cell_block;
	IndexesBlock* indexes_block;

	DMCNode octree;
	union children_pool
	{
		MemoryPool<DMCNode> node_pool;
		DMCNode children[8];

		children_pool() : node_pool() {}
		~children_pool() {}
	};
	children_pool octree_children;
	SmartContainer<DMCNode*> nodes;

	glm::vec3 overlap_pos;
	glm::vec3 bound_start;
	float bound_size;

	DMCChunk();
	DMCChunk(glm::vec3 pos, float size, int level, Sampler& sampler, uint64_t parent_code);
	~DMCChunk();

	// Main pipeline
	void init(glm::vec3 pos, float size, int level, Sampler& sampler, uint64_t parent_code);
	void label_grid(ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<IsoVertexBlock>* density_allocator, ResourceAllocator<NoiseBlock>* noise_allocator, float overlap);
	void label_edges(ResourceAllocator<VerticesIndicesBlock>* vi_allocator, ResourceAllocator<DMC_CellsBlock>* cell_allocator, ResourceAllocator<IndexesBlock>* inds_allocator, ResourceAllocator<IsoVertexBlock>* density_allocator, ResourceAllocator<MasksBlock>* masks_allocator);
	void snap_verts();
	void polygonize();
	void polygonize_cell(DMC_Cell& _c, int x, int y, int z, int dim, SmartContainer<DualVertex>& verts, SmartContainer<uint32_t>& inds);
	void copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);

	// Sub procedures
	void calculate_cell(int x, int y, int z, uint32_t next_v_index, uint8_t mask, DMC_Cell& dest, int dim);
	void calculate_isovertex(int x0, int y0, int z0, int x1, int y1, int z1, int index, int dim, DMC_Isovertex& out);
	DualVertex calculate_dual_vertex(DMC_Isovertex& in);

	void generate_octree();

	int get_internal_node_at(glm::vec3 p);

	double extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent);
};

