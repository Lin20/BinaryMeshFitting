#include "PCH.h"
#include "ChunkGenerator.hpp"
#include "WorldOctree.hpp"
#include "WorldOctreeNode.hpp"
#include <iostream>

ChunkGenerator::ChunkGenerator() : ThreadDebug("ChunkGenerator")
{
	this->world = 0;
	this->_stop = false;
}

ChunkGenerator::~ChunkGenerator()
{
	stop();
	world->generator_shutdown = true;
}

void ChunkGenerator::init(WorldOctree* _world)
{
	this->world = _world;
	_thread = std::thread(std::bind(&ChunkGenerator::update, this));
}

void ChunkGenerator::update()
{
	std::cout.flush();
	print() << "Initialized thread." << std::endl;

	glm::vec3 pos;
	const int ms_frequency = 10;
	SmartContainer<WorldOctreeNode*> dirty_batch;
	SmartContainer<WorldOctreeNode*> generate_batch;
	while (!_stop)
	{
		auto now = std::chrono::system_clock::now();

		process_queue();

	End:
		auto elapsed = std::chrono::system_clock::now() - now;
		auto millis = ms_frequency - (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		if (millis > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(millis));
	}
}

void ChunkGenerator::stop()
{
	print() << "Stopping...";
	if (_thread.joinable())
	{
		_stop = true;
		_thread.join();
		std::cout << "done." << std::endl;
	}
	else
	{
		std::cout << "already stopped." << std::endl;
	}
}

void ChunkGenerator::process_queue()
{
	std::vector<WorldOctreeNode*> local_queue;
	{
		std::unique_lock<std::mutex> lock(_mutex);
		local_queue = queue;
		queue.clear();
	}

	// Trim the local queue
	int count = (int)local_queue.size();
	for (int i = 0; i < count; i++)
	{
		WorldOctreeNode* n = local_queue[i];
		if (n->generation_stage == GENERATION_STAGES_GENERATOR_ACKNOWLEDGED)
		{
			if (update_still_needed(n))
			{
				n->generation_stage = GENERATION_STAGES_GENERATING;
			}
			else
			{
				local_queue.erase(local_queue.begin() + i);
				i--;
			}
		}
		else if (n->generation_stage == GENERATION_STAGES_NEEDS_FORMAT)
		{
			continue;
		}
		else
		{
			print() << "WARNING: Node found with incorrect generation stage!" << std::endl;
			local_queue.erase(local_queue.begin() + i);
			i--;
		}
	}

	count = (int)local_queue.size();
	for (int i = 0; i < count; i++)
	{
		if (local_queue[i]->generation_stage == GENERATION_STAGES_GENERATING)
			generate_chunk(local_queue[i]);
	}

	extract_samples(local_queue, &binary_allocator, &float_allocator);
	extract_dual_vertices(local_queue);
	extract_octrees(local_queue);
	extract_base_meshes(local_queue);
	extract_format_meshes(local_queue);

	for (int i = 0; i < count; i++)
	{
		local_queue[i]->generation_stage = GENERATION_STAGES_NEEDS_UPLOAD;
	}
}

void ChunkGenerator::add_batch(SmartContainer<WorldOctreeNode*>& batch)
{
	std::unique_lock<std::mutex> lock(_mutex);

	int count = (int)batch.count;
	for (int i = 0; i < count; i++)
	{
		WorldOctreeNode* n = batch[i];
		int flags = n->flags;
		int stage = n->generation_stage;
		if (stage == GENERATION_STAGES_GENERATOR_QUEUED)
		{
			n->generation_stage = GENERATION_STAGES_GENERATOR_ACKNOWLEDGED;
			n->flags |= NODE_FLAGS_GENERATING;

			queue.push_back(n);
		}
		else if (stage == GENERATION_STAGES_NEEDS_FORMAT && (flags & NODE_FLAGS_GENERATING))
		{
			queue.push_back(n);
		}
		else
			print() << "Ignoring batch item." << std::endl;
	}
}

bool ChunkGenerator::update_still_needed(WorldOctreeNode* n)
{
	return true;
}

void ChunkGenerator::generate_chunk(WorldOctreeNode* n)
{
	world->create_chunk(n);
}

void ChunkGenerator::extract_samples(std::vector<class WorldOctreeNode*>& batch, ResourceAllocator<BinaryBlock>* binary_allocator, ResourceAllocator<FloatBlock>* float_allocator)
{
	using namespace std;
	//cout << "-Generating samples...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
			batch[i]->chunk->generate_samples(binary_allocator, float_allocator);
	}

	/*for (auto& n : leaves)
	{
	n->chunk->generate_samples();
	}*/
	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_filter(std::vector<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Filtering data...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
			batch[i]->chunk->filter();
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_dual_vertices(std::vector<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Calculating dual vertices...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
				batch[i]->chunk->generate_dual_vertices(&vi_allocator);
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_octrees(std::vector<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating octrees...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
			{
				batch[i]->chunk->generate_octree();
				if (!batch[i]->chunk->octree.is_leaf())
				{
					memcpy(batch[i]->children, batch[i]->chunk->octree.children, sizeof(OctreeNode*) * 8);
					batch[i]->leaf_flag = false;
				}
				else
					cout << "WARNING: Octree is leaf." << endl;
			}
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_base_meshes(std::vector<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating base meshes...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
				batch[i]->chunk->generate_base_mesh(&vi_allocator);
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_copy_vis(std::vector<WorldOctreeNode*>& batch)
{
}

void ChunkGenerator::extract_stitches(std::vector<WorldOctreeNode*>& batch)
{
}

void ChunkGenerator::extract_format_meshes(std::vector<class WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating base meshes...";
	clock_t start_clock = clock();

	int count = (int)batch.size();
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			batch[i]->format(&world->gl_allocator);
		}
	}

	double ms = clock() - start_clock;
}
