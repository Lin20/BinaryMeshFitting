#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <vector>
#include <glm/glm.hpp>
#include "ThreadDebug.hpp"
#include "SmartContainer.hpp"
#include "ResourceAllocator.hpp"
#include "ChunkBlocks.hpp"

class ChunkGenerator : public ThreadDebug
{
public:
	ChunkGenerator();
	~ChunkGenerator();

	void init(class WorldOctree* _world);
	void update();
	void stop();

	void process_queue();
	void add_batch(SmartContainer<class WorldOctreeNode*>& batch);

	std::mutex _mutex;

	ResourceAllocator<BinaryBlock> binary_allocator;
	ResourceAllocator<FloatBlock> float_allocator;
	ResourceAllocator<VerticesIndicesBlock> vi_allocator;

private:
	class WorldOctree* world;
	std::thread _thread;
	std::atomic<bool> _stop;

	std::vector<class WorldOctreeNode*> queue;

	bool update_still_needed(class WorldOctreeNode* n);
	void generate_chunk(class WorldOctreeNode* n);

	void extract_samples(std::vector<class WorldOctreeNode*>& batch, ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<FloatBlock>* float_allocator);
	void extract_filter(std::vector<class WorldOctreeNode*>& batch);
	void extract_dual_vertices(std::vector<class WorldOctreeNode*>& batch);
	void extract_octrees(std::vector<class WorldOctreeNode*>& batch);
	void extract_base_meshes(std::vector<class WorldOctreeNode*>& batch);
	void extract_copy_vis(std::vector<class WorldOctreeNode*>& batch);
	void extract_stitches(std::vector<class WorldOctreeNode*>& batch);
	void extract_format_meshes(std::vector<class WorldOctreeNode*>& batch);
};
