#include "PCH.h"
#include "ChunkGenerator.hpp"
#include "WorldOctree.hpp"
#include "WorldOctreeNode.hpp"
#include "MeshProcessor.hpp"
#include "DefaultOptions.h"
#include "DMCChunk.hpp"
#include <iostream>

ChunkGenerator::ChunkGenerator() : ThreadDebug("ChunkGenerator")
{
	this->world = 0;
}

ChunkGenerator::~ChunkGenerator()
{
	world->generator_shutdown = true;
}

void ChunkGenerator::init(WorldOctree* _world)
{
	this->world = _world;
	this->stitcher.init();
}

void ChunkGenerator::process_queue(SmartContainer<WorldOctreeNode*>& batch)
{
	int count = (int)batch.count;
	{
		std::unique_lock<std::mutex> c_lock(world->chunk_mutex);
		for (int i = 0; i < count; i++)
		{
			if (!batch[i]->chunk)
				generate_chunk(batch[i]);
		}
	}

	extract_chunk(batch);
	/*extract_samples(batch);
	extract_dual_vertices(batch);
	extract_octrees(batch);
	extract_base_meshes(batch);
	extract_format_meshes(batch);*/

	/*for (int i = 0; i < count; i++)
	{
		batch[i]->generation_stage = GENERATION_STAGES_NEEDS_UPLOAD;
	}*/

	/*if (stitcher.stage == STITCHING_STAGES_READY)
	{
		std::unique_lock<std::mutex> stitcher_lock(stitcher._mutex);
		stitcher.stitch_all(&world->octree);
		stitcher.format();
		stitcher.stage = STITCHING_STAGES_NEEDS_UPLOAD;
	}*/

	//assert(stitcher.stage == STITCHING_STAGES_READY);
}

bool ChunkGenerator::update_still_needed(WorldOctreeNode* n)
{
	return true;
	if (!n->parent)
		return true;
	WorldOctreeNode* p = (WorldOctreeNode*)n->parent;
	if (!(p->flags & NODE_FLAGS_SPLIT))
		return true;

	glm::vec3 focus_pos = world->watcher.focus_pos;
	return world->node_needs_split(focus_pos, p);
}

void ChunkGenerator::generate_chunk(WorldOctreeNode* n)
{
	world->create_chunk(n);
}

void ChunkGenerator::extract_chunk(SmartContainer<class WorldOctreeNode*>& batch)
{
	int count = (int)batch.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
		{
			if (update_still_needed(batch[i]))
			{
				batch[i]->chunk->label_grid(&binary_allocator, &isovertex_allocator, &noise_allocator);

				batch[i]->chunk->label_edges(&vi_allocator, &cell_allocator, &inds_allocator, &isovertex_allocator);

				batch[i]->chunk->generate_octree();
				if (!(batch[i]->flags & NODE_FLAGS_GROUP))
				{
					memcpy(batch[i]->children, batch[i]->chunk->octree.children, sizeof(OctreeNode*) * 8);
					batch[i]->leaf_flag = false;
				}

				batch[i]->chunk->polygonize();

				binary_allocator.free_element(batch[i]->chunk->binary_block);
				batch[i]->chunk->binary_block = 0;
				isovertex_allocator.free_element(batch[i]->chunk->density_block);
				batch[i]->chunk->density_block = 0;
				cell_allocator.free_element(batch[i]->chunk->cell_block);
				batch[i]->chunk->cell_block = 0;
				inds_allocator.free_element(batch[i]->chunk->indexes_block);
				batch[i]->chunk->indexes_block = 0;
			}
		}

		batch[i]->format(&gl_allocator);

		batch[i]->generation_stage = GENERATION_STAGES_NEEDS_UPLOAD;
	}


}

void ChunkGenerator::extract_samples(SmartContainer<class WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating samples...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
		{
			if (update_still_needed(batch[i]))
			{
				batch[i]->chunk->label_grid(&binary_allocator, &isovertex_allocator, &noise_allocator);
			}
		}
	}

	/*for (auto& n : leaves)
	{
	n->chunk->generate_samples();
	}*/
	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_filter(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Filtering data...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		//if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
		//	batch[i]->chunk->filter();
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_dual_vertices(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Calculating dual vertices...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
				batch[i]->chunk->label_edges(&vi_allocator, &cell_allocator, &inds_allocator, &isovertex_allocator);
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_octrees(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating octrees...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
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
				//else
				//	cout << "WARNING: Octree is leaf." << endl;
			}
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_base_meshes(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating base meshes...";
	clock_t start_clock = clock();

	const int iters = world->properties.process_iters;
	bool smooth_normals = SMOOTH_NORMALS;
	Sampler& sampler = world->sampler;

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			if (batch[i]->generation_stage == GENERATION_STAGES_GENERATING)
			{
				batch[i]->chunk->polygonize();
				if (!iters || !batch[i]->chunk->contains_mesh || !batch[i]->chunk->vi->vertices.count || !batch[i]->chunk->vi->mesh_indexes.count)
					continue;

				/*auto& v_out = batch[i]->chunk->vi->vertices;
				auto& i_out = batch[i]->chunk->vi->mesh_indexes;
				Processing::MeshProcessor<4> mp(true, SMOOTH_NORMALS);
				mp.init(batch[i]->chunk->vi->vertices, batch[i]->chunk->vi->mesh_indexes, sampler);

				//if (iters > 0)
				//	mp.collapse_bad_quads();
				if (!QUADS)
				{
					v_out.count = 0;
					i_out.count = 0;
					mp.flush_to_tris(v_out, i_out);
					Processing::MeshProcessor<3> nmp = Processing::MeshProcessor<3>(true, SMOOTH_NORMALS);
					if (iters > 0)
					{
						nmp.init(v_out, i_out, sampler);
						nmp.optimize_dual_grid(iters);
						nmp.optimize_primal_grid(false, false);
						v_out.count = 0;
						i_out.count = 0;
						nmp.flush(v_out, i_out);
					}
				}
				else
				{
					mp.optimize_dual_grid(iters);
					mp.optimize_primal_grid(false, false);
					v_out.count = 0;
					i_out.count = 0;
					mp.flush(v_out, i_out);
				}*/

			}
		}
	}

	double ms = clock() - start_clock;
	//cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void ChunkGenerator::extract_copy_vis(SmartContainer<WorldOctreeNode*>& batch)
{
}

void ChunkGenerator::extract_stitches(SmartContainer<WorldOctreeNode*>& batch)
{
}

void ChunkGenerator::extract_format_meshes(SmartContainer<class WorldOctreeNode*>& batch)
{
	using namespace std;
	//cout << "-Generating base meshes...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			batch[i]->format(&gl_allocator);
		}
	}

	double ms = clock() - start_clock;
}
