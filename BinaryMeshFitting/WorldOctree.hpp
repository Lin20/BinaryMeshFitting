#pragma once

#include "Sampler.hpp"
#include "WorldOctreeNode.hpp"
#include "MemoryPool.h"
#include "DMCChunk.hpp"
#include "SmartContainer.hpp"
#include "GLChunk.hpp"
#include "ColorMapper.hpp"
#include "WorldWatcher.hpp"
#include "ResourceAllocator.hpp"
#include "ChunkBlocks.hpp"
#include "NoiseSampler.hpp"

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
	bool enable_stitching;
	float overlap;
	bool boundary_processing;

	__declspec(noinline) WorldProperties();
};

class WorldOctree
{
public:
	Sampler sampler;
	WorldProperties properties;
	WorldOctreeNode octree;
	MemoryPool<WorldOctreeNode> node_pool;
	MemoryPool<DMCChunk> chunk_pool;
	std::list<WorldOctreeNode*> leaves;
	SmartContainer<DualVertex> v_out;
	SmartContainer<uint32_t> i_out;
	GLChunk outline_chunk;
	int next_chunk_id;
	NoiseSamplers::NoiseSamplerProperties noise_properties;

	uint32_t leaf_count;
	glm::vec3 focus_point;

	ColorMapper color_mapper;

	WorldWatcher watcher;

	std::atomic<bool> generator_shutdown;
	std::mutex chunk_mutex;

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
	void upload_batch(SmartContainer<WorldOctreeNode*>& batch);
	void generate_outline(SmartContainer<WorldOctreeNode*>& batch);
	DMCChunk* get_chunk_id_at(glm::vec3 p);

	void init_updates(glm::vec3 focus_pos);
	void process_from_render_thread();

private:
};