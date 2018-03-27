#include "PCH.h"
#include "WorldOctree.hpp"
#include "ImplicitSampler.hpp"
#include "WorldOctree.hpp"
#include "NoiseSampler.hpp"
#include "Tables.hpp"
#include "DefaultOptions.h"
#include "MeshProcessor.hpp"

#include <iostream>
#include <iomanip>
#include <thread>

#include <omp.h>

#define DEFAULT_THREADS 4
#define DEFAULT_ITERATIONS 0
#define DEFAULT_RESOLUTION 32

__declspec(noinline) WorldProperties::WorldProperties()
{
	split_multiplier = 1.0f;
	group_multiplier = split_multiplier * 2.0f;
	size_modifier = 0.0f;
	max_level = 7;
	min_level = 1;
	num_threads = DEFAULT_THREADS;
	process_iters = DEFAULT_ITERATIONS;
	chunk_resolution = DEFAULT_RESOLUTION;
	enable_stitching = false;
}

WorldOctree::WorldOctree()
{
	using namespace std;
	//sampler = ImplicitFunctions::create_sampler(ImplicitFunctions::sphere);
	//sampler.block = ImplicitFunctions::cuboid_block;
	NoiseSamplers::create_sampler_terrain_pert_2d(&sampler);
	sampler.world_size = 256;
	focus_point = glm::vec3(0, 0, 0);
	generator_shutdown = false;

	this->properties = WorldProperties();

	v_out.scale = 4;
	i_out.scale = 4;

	cout << "Noise SIMD instruction set: " << get_simd_text() << endl << endl;
	cout << "Split properties:" << endl;
	cout << "-Split Mul:\t" << properties.split_multiplier << endl;
	cout << "-Group Mul:\t" << properties.group_multiplier << endl;
	cout << "-Size Mod:\t" << properties.size_modifier << endl;
	cout << "-Max Level:\t" << properties.max_level << endl;
	cout << "-Min Level:\t" << properties.min_level << endl << endl;
}

void destroy_world_nodes(MemoryPool<WorldOctreeNode>* pool, MemoryPool<DMCChunk>* chunk_pool, WorldOctreeNode* n)
{
	if (!n)
		return;

	for (int i = 0; i < 8; i++)
	{
		if (n->children[i] && n->children[i]->is_world_node())
		{
			destroy_world_nodes(pool, chunk_pool, (WorldOctreeNode*)n->children[i]);
		}
		n->children[i] = 0;
	}

	if (n->chunk)
	{
		chunk_pool->deleteElement(n->chunk);
		n->chunk = 0;
	}

	if (n->level > 0)
		pool->deleteElement(n);
}

WorldOctree::~WorldOctree()
{
	destroy_world_nodes(&node_pool, &chunk_pool, &octree);
	delete sampler.noise_sampler;
}

void WorldOctree::destroy_leaves()
{
	/*for (auto& n : leaves)
	{
		if (n->chunk)
			chunk_pool.deleteElement(n->chunk);
		//node_pool.deleteElement(n);
	}*/
	leaves.clear();
	v_out.count = 0;
	i_out.count = 0;
	destroy_world_nodes(&node_pool, &chunk_pool, &octree);
	chunk_pool.~MemoryPool();
	new (&chunk_pool) MemoryPool<DMCChunk>();
	node_pool.~MemoryPool();
	new (&node_pool) MemoryPool<WorldOctreeNode>();
}

void WorldOctree::init(uint32_t size)
{
	size *= 2;
	glm::vec3 pos = glm::vec3(1, 1, 1) * (float)size * -0.5f;

	// Use placement new to initialize octree because atomic has no copy constructor
	octree.~WorldOctreeNode();
	new(&octree) WorldOctreeNode(0, 0, (float)size, pos, 0);
	octree.flags = NODE_FLAGS_DIRTY | NODE_FLAGS_DRAW;
	octree.morton_code = 1;
}

void WorldOctree::split_leaves()
{
	using namespace std;
	stack<WorldOctreeNode*> check_queue;
	check_queue.push(&octree);

	cout << "Building octree...";

	leaf_count = 0;
	leaves.clear();
	v_out.count = 0;
	i_out.count = 0;

	while (!check_queue.empty())
	{
		WorldOctreeNode* n = check_queue.top();
		check_queue.pop();
		if (!node_needs_split(focus_point, n))
		{
			//n->stored_as_leaf = true;
			leaves.push_back(n);
			//renderables.push_back(n);
			continue;
		}

		split_node(n);
		for (int i = 0; i < 8; i++)
		{
			assert(n->children[i] != 0);
			check_queue.push((WorldOctreeNode*)n->children[i]);
		}
	}

	cout << "done (" << leaves.size() << " leaves)" << endl;

	next_chunk_id = 0;
	cout << "Creating chunks...";
	for (auto& n : leaves)
	{
		leaf_count++;
		create_chunk(n);
	}
	cout << "done." << endl << endl;

}

bool WorldOctree::split_node(WorldOctreeNode* n)
{
	if (!n->is_leaf())
	{
		for (int i = 0; i < 8; i++)
		{
			if (n->children[i] && n->children[i]->is_world_node())
				assert(false);
			else
				n->children[i] = 0;
		}
		n->leaf_flag = true;
	}
	assert(n->is_leaf());
	float c_size = n->size * 0.5f;
	for (int i = 0; i < 8; i++)
	{
		assert(n->children[i] == 0);
		glm::vec3 c_pos(n->pos.x + (float)Tables::MCDX[i] * c_size, n->pos.y + (float)Tables::MCDY[i] * c_size, n->pos.z + (float)Tables::MCDZ[i] * c_size);
		WorldOctreeNode* c = node_pool.newElement(0, n, n->size * 0.5f, c_pos, n->level + 1);

		uint64_t code = 0;
		code |= Tables::MCDX[i];
		code |= Tables::MCDY[i] << 1;
		code |= Tables::MCDZ[i] << 2;
		c->morton_code = (n->morton_code.code << 3) | code;
		n->children[i] = c;

		assert(n->children[i]);
	}

	n->leaf_flag = false;
	n->world_leaf_flag = false;

	return true;
}

bool WorldOctree::group_node(WorldOctreeNode* n)
{
	float c_size = n->size * 0.5f;
	for (int i = 0; i < 8; i++)
	{
		if (n->children[i])
		{
			//((WorldOctreeNode*)n->children[i])->remove_as_leaf = true;
			//((WorldOctreeNode*)n->children[i])->destroy = true;
		}
		n->children[i] = 0;
	}

	for (int i = 0; i < 8; i++)
	{
		n->children[i] = n->chunk->octree.children[i];
	}

	n->leaf_flag = false;
	n->world_leaf_flag = true;
	//n->stored_as_leaf = false;
	//n->destroy = false;
	//n->remove_as_leaf = false;

	return true;
}

bool WorldOctree::node_needs_split(const glm::vec3& center, WorldOctreeNode* n)
{
	using namespace glm;
	if (n->level >= properties.max_level)
		return false;
	if (n->level < properties.min_level)
		return true;

	float d = distance(n->middle, center);
	return d < n->size * properties.split_multiplier + properties.size_modifier + n->size * 0.5f;
}

bool WorldOctree::node_needs_group(const glm::vec3& center, WorldOctreeNode* n)
{
	using namespace glm;
	if (n->level < properties.min_level)
		return false;
	if (n->level > properties.max_level)
		return true;

	float d = distance(n->middle, center);
	return d > n->size * properties.group_multiplier + properties.size_modifier + n->size * 0.5f;
}

void WorldOctree::create_chunk(WorldOctreeNode* n)
{
	n->chunk = chunk_pool.newElement();
	n->chunk->init(n->pos, n->size, n->level, sampler, n->morton_code.code, properties.max_level);
	n->chunk->dim = properties.chunk_resolution;
	n->chunk->id = next_chunk_id++;
}

void WorldOctree::upload_batch(SmartContainer<WorldOctreeNode*>& batch)
{
	//gl_chunk.init(true, true);
	//gl_chunk.set_data(v_out, i_out, FLAT_QUADS, SMOOTH_NORMALS);

	int count = (int)batch.count;
	for (int i = 0; i < count; i++)
	{
		//batch[i]->upload(&gl_allocator);
	}
}

void WorldOctree::generate_outline(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;

	cout << "Generating outline...";
	clock_t start_clock = clock();

	outline_chunk.init(false, false);
	SmartContainer<glm::vec3> v_pos;
	SmartContainer<uint32_t> inds;

	int count = (int)batch.count;
	for (int i = 0; i < count; i++)
	{
		batch[i]->generate_outline(v_pos, inds);
	}

	outline_chunk.set_data(v_pos, inds);

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

DMCChunk* WorldOctree::get_chunk_id_at(glm::vec3 p)
{
	for (auto& n : leaves)
	{
		if (n->chunk)
		{
			float s = n->chunk->size;
			glm::vec3 cp = n->chunk->pos;
			if (p.x >= cp.x && p.y >= cp.y && p.z >= cp.z && p.x < cp.x + s && p.y < cp.y + s && p.z < cp.z + s)
			{
				return n->chunk;
			}
		}
	}

	return 0;
}

void WorldOctree::init_updates(glm::vec3 focus_pos)
{
	watcher.init(this, focus_pos);
}

void WorldOctree::process_from_render_thread()
{
	// Renderables mutex is already locked from the main render loop

	const int MAX_DELETES = 1500;
	const int MAX_UPLOADS = 1000;

	int upload_count = 0;
	WorldOctreeNode* n = watcher.renderables_head;
	while (n)
	{
		int flags = n->flags;
		int stage = n->generation_stage;
		if (stage == GENERATION_STAGES_NEEDS_UPLOAD)
		{
			// If there is still one more to be uploaded, abandon the loop.
			// This way we can upload the max chunks in the unlikely case there are exactly that many chunks to be uploaded
			if (upload_count == MAX_UPLOADS)
			{
				upload_count++;
				break;
			}
			n->generation_stage = GENERATION_STAGES_UPLOADING;
			n->upload();

			if (!FAST_GROUPING)
			{
				watcher.generator.vi_allocator.free_element(n->chunk->vi);
				n->chunk->vi = 0;
			}
			n->generation_stage = GENERATION_STAGES_DONE;
			if (n->gl_chunk)
				n->gl_chunk->reset_data();
			if (n->gl_chunk && n->gl_chunk->p_count > 0 && n->gl_chunk->v_count > 0)
			{
				upload_count++;
			}
			else if (!upload_count)
				upload_count++;
		}
		n = n->renderable_next;
	}

	if (!upload_count || (upload_count > 0 && upload_count <= MAX_UPLOADS))
	{
		// Notify the main watcher thread that the uploading has finished so it can move on
		watcher.upload_cv.notify_one();
	}

	if (watcher.generator.stitcher.stage == STITCHING_STAGES_NEEDS_UPLOAD)
	{
		watcher.generator.stitcher.stage = STITCHING_STAGES_UPLOADING;
		watcher.generator.stitcher.upload();
		watcher.generator.stitcher.stage = STITCHING_STAGES_READY;
	}
}

