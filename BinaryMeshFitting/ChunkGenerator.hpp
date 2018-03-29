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
#include "WorldStitcher.hpp"

class ChunkGenerator : public ThreadDebug
{
public:
	ChunkGenerator();
	~ChunkGenerator();

	void init(class WorldOctree* _world);

	void process_queue(SmartContainer<WorldOctreeNode*>& batch);

	std::mutex _mutex;
	std::condition_variable _cv;

	ResourceAllocator<GLChunk> gl_allocator;
	ResourceAllocator<DensityBlock> density_allocator;
	ResourceAllocator<BinaryBlock> binary_allocator;
	ResourceAllocator<MasksBlock> masks_allocator;
	ResourceAllocator<VerticesIndicesBlock> vi_allocator;
	ResourceAllocator<DMC_CellsBlock> cell_allocator;
	ResourceAllocator<IndexesBlock> inds_allocator;
	ResourceAllocator<IsoVertexBlock> isovertex_allocator;
	ResourceAllocator<NoiseBlock> noise_allocator;

	WorldStitcher stitcher;

private:
	class WorldOctree* world;

	std::vector<class WorldOctreeNode*> queue;

	bool update_still_needed(class WorldOctreeNode* n);
	void generate_chunk(class WorldOctreeNode* n);

	void extract_chunk(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_samples(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_filter(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_dual_vertices(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_octrees(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_base_meshes(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_copy_vis(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_stitches(SmartContainer<class WorldOctreeNode*>& batch);
	void extract_format_meshes(SmartContainer<class WorldOctreeNode*>& batch);
};
