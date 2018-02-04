#pragma once

#include "Sampler.hpp"
#include "WorldOctreeNode.hpp"
#include "MemoryPool.h"
#include "CubicChunk.hpp"
#include "SmartContainer.hpp"
#include "GLChunk.hpp"
#include "ColorMapper.hpp"
#include "WorldWatcher.hpp"
#include "ResourceAllocator.hpp"
#include "ChunkBlocks.hpp"

#include <list>
#include <stack>
#include <mutex>

struct WorldProperties
{
	float split_multiplier;
	float group_multiplier;
	float size_modifier;
	int max_level;
	int min_level;
	int num_threads;
	int process_iters;
	int chunk_resolution;

	__declspec(noinline) WorldProperties();
};

class WorldOctree
{
public:
	Sampler sampler;
	WorldProperties properties;
	WorldOctreeNode octree;
	MemoryPool<WorldOctreeNode, 65536> node_pool;
	MemoryPool<CubicChunk, 65536> chunk_pool;
	std::list<WorldOctreeNode*> leaves;
	SmartContainer<DualVertex> v_out;
	SmartContainer<uint32_t> i_out;
	GLChunk stitch_chunk;
	GLChunk outline_chunk;
	int next_chunk_id;

	uint32_t leaf_count;
	glm::vec3 focus_point;

	ColorMapper color_mapper;

	std::stack<WorldOctreeNode*> generate_queue;
	std::mutex generate_mutex;

	WorldWatcher watcher;
	ResourceAllocator<GLChunk> gl_allocator;

	WorldOctree();
	~WorldOctree();

	void destroy_leaves();
	void init(uint32_t size);
	void split_leaves();
	bool split_node(WorldOctreeNode* n);
	bool group_node(WorldOctreeNode* n);
	bool node_needs_split(const glm::vec3& center, WorldOctreeNode* n);
	bool node_needs_group(const glm::vec3& center, WorldOctreeNode* n);
	void create_chunk(WorldOctreeNode* n);
	double extract_all();
	double color_all();
	double process_all();
	double upload_all();
	void upload_batch(SmartContainer<WorldOctreeNode*>& batch);
	void generate_outline(SmartContainer<WorldOctreeNode*>& batch);
	CubicChunk* get_chunk_id_at(glm::vec3 p);

	void init_updates(glm::vec3 focus_pos);
	void update(glm::vec3 pos);
	void process_from_render_thread();

private:
	void extract_samples(SmartContainer<WorldOctreeNode*>& batch);
	void extract_filter(SmartContainer<WorldOctreeNode*>& batch);
	void extract_dual_vertices(SmartContainer<WorldOctreeNode*>& batch);
	void extract_octrees(SmartContainer<WorldOctreeNode*>& batch);
	void extract_base_meshes(SmartContainer<WorldOctreeNode*>& batch);
	void extract_copy_vis(SmartContainer<WorldOctreeNode*>& batch);
	void extract_stitches(SmartContainer<WorldOctreeNode*>& batch);

	static void stitch_cell(OctreeNode* n, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);
	static void stitch_faces(OctreeNode* n[2], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);
	static void stitch_edges(OctreeNode* n[4], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);
	static void stitch_indexes(OctreeNode* n[4], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);

	void update_leaves();
};