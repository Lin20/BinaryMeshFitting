#include "PCH.h"
#include "WorldWatcher.hpp"
#include "WorldOctree.hpp"
#include "WorldOctreeNode.hpp"
#include <iostream>

WorldWatcher::WorldWatcher() : ThreadDebug("WorldWatcher")
{
	this->world = 0;
	this->_stop = false;
}

WorldWatcher::~WorldWatcher()
{
	stop();

}

void WorldWatcher::init(WorldOctree* _world, glm::vec3 focus_pos)
{
	this->world = _world;
	this->focus_pos = focus_pos;
	renderables_head = &_world->octree;
	renderables_tail = &_world->octree;
	renderables_count = 1;
	_thread = std::thread(std::bind(&WorldWatcher::update, this));

	generator.init(_world);
}

void WorldWatcher::update()
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
		{
			std::unique_lock<std::mutex> lock(_mutex);
			pos = focus_pos;
		}

		dirty_batch.count = 0;
		generate_batch.count = 0;
		if (generator.stitcher.stage == STITCHING_STAGES_READY)
		{
			bool enable_stitching = world->properties.enable_stitching;
			{
				std::unique_lock<std::mutex> renderables_lock(renderables_mutex);
				check_leaves(dirty_batch);
				process_batch(dirty_batch, generate_batch);
			}
			if (generate_batch.count > 0)
			{
				std::cout << "Generating " << generate_batch.count << " chunks." << std::endl;
				generator.process_queue(generate_batch);
				if (enable_stitching)
				{
					generator.stitcher.stitch_all(&world->octree);
					generator.stitcher.format();
				}

				{
					std::unique_lock<std::mutex> renderables_lock(renderables_mutex);
					upload_cv.wait(renderables_lock);

					post_process_batch(dirty_batch);
					if (enable_stitching)
						generator.stitcher.stage = STITCHING_STAGES_NEEDS_UPLOAD;
				}
			}
		}

	End:
		auto elapsed = std::chrono::system_clock::now() - now;
		auto millis = ms_frequency - (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		if (millis > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(millis));


		last_focus_pos = pos;
	}
}

void WorldWatcher::stop()
{
	print() << "Stopping...";
	if (_thread.joinable())
	{
		_stop = true;
		{
			std::unique_lock<std::mutex> renderables_lock(renderables_mutex);
			upload_cv.notify_one();
		}
		_thread.join();
		std::cout << "done." << std::endl;
	}
	else
	{
		std::cout << "already stopped." << std::endl;
	}
}

void WorldWatcher::check_leaves(SmartContainer<class WorldOctreeNode*>& batch_out)
{
	WorldOctreeNode* n = renderables_head;
	while (n)
	{
		if (world->node_needs_split(focus_pos, n))
		{
			handle_split_check(n, batch_out);
		}
		else if (n->world_leaf_flag && n->parent && world->node_needs_group(focus_pos, (WorldOctreeNode*)n->parent))
		{
			handle_group_check((WorldOctreeNode*)n->parent, batch_out);
		}
		n = n->renderable_next;
	}
}

void WorldWatcher::handle_split_check(WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& batch_out)
{
	int flags = n->flags;
	int stage = n->generation_stage;
	if (n->world_leaf_flag && !(flags & NODE_FLAGS_SPLIT) && !(flags & NODE_FLAGS_DRAW_CHILDREN) && (flags & NODE_FLAGS_DRAW) && !(flags & NODE_FLAGS_GROUP) && !(flags & NODE_FLAGS_SUPERCEDED))
	{
		n->flags |= NODE_FLAGS_SPLIT;
		if (n->flags & NODE_FLAGS_GROUP)
			n->flags ^= NODE_FLAGS_GROUP;
		n->force_chunk_octree = false;
		batch_out.push_back(n);
	}
}

void WorldWatcher::handle_group_check(WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& batch_out)
{
	int flags = n->flags;
	int stage = n->generation_stage;
	if (!n->world_leaf_flag && !(flags & NODE_FLAGS_GROUP) && (flags & NODE_FLAGS_SUPERCEDED))
	{
		bool can_group = true;
		for (int i = 0; i < 8; i++)
		{
			WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
			if (!c || !c->world_leaf_flag)
			{
				can_group = false;
				break;
			}
		}
		if (can_group)
		{
			n->flags |= NODE_FLAGS_GROUP;
			n->force_chunk_octree = true;
			batch_out.push_back(n);
		}
	}
}

void WorldWatcher::handle_dangling_check(WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& batch_out)
{
	int flags = n->flags;
	int stage = n->generation_stage;

	if (flags & NODE_FLAGS_DRAW_CHILDREN)
	{
		int stitch_stage = generator.stitcher.stage;

		for (int i = 0; i < 8; i++)
		{
			WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
			c->flags |= NODE_FLAGS_DRAW;
		}
		n->flags &= ~NODE_FLAGS_DRAW;
		n->flags &= ~NODE_FLAGS_DRAW_CHILDREN;
		n->flags |= NODE_FLAGS_SUPERCEDED;
		n->force_chunk_octree = false;
		if (n->gl_chunk)
		{
			world->gl_allocator.free_element(n->gl_chunk);
			n->gl_chunk = 0;
		}
		n->generation_stage = GENERATION_STAGES_NEEDS_FORMAT;
		batch_out.push_back(n);
	}
	else if ((flags & NODE_FLAGS_SUPERCEDED) && (flags & NODE_FLAGS_GENERATING))
	{
		bool can_group = true;

		for (int i = 0; i < 8; i++)
		{
			WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
			int flags = c->flags;
			if (flags != NODE_FLAGS_DRAW || c->generation_stage != GENERATION_STAGES_DONE)
			{
				can_group = false;
				break;
			}
		}

		if (can_group)
		{
			{
				std::unique_lock<std::mutex> c_lock(world->chunk_mutex);
				for (int i = 0; i < 8; i++)
				{
					WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
					unlink_renderable(c);
					generator.binary_allocator.free_element(c->chunk->binary_block);
					generator.vi_allocator.free_element(c->chunk->vi);
					generator.cell_allocator.free_element(c->chunk->cell_block);
					generator.inds_allocator.free_element(c->chunk->inds_block);
					world->gl_allocator.free_element(c->gl_chunk);
					world->chunk_pool.deleteElement(c->chunk);
					world->node_pool.deleteElement(c);
				}
			}

			n->flags |= NODE_FLAGS_DRAW;
			n->flags ^= NODE_FLAGS_SUPERCEDED;
			n->flags ^= NODE_FLAGS_GENERATING;
			n->force_chunk_octree = false;

			world->group_node(n);
		}
	}
}

void WorldWatcher::process_batch(SmartContainer<class WorldOctreeNode*>& batch_in, SmartContainer<class WorldOctreeNode*>& batch_out)
{
	int count = (int)batch_in.count;
	for (int i = 0; i < count; i++)
	{
		WorldOctreeNode* n = batch_in[i];
		int flags = n->flags;
		int stage = n->generation_stage;

		if (flags & NODE_FLAGS_DESTROY)
		{
			unlink_renderable(n);
		}
		else if (flags & NODE_FLAGS_SPLIT)
		{
			split_node(n, batch_out);
			//n->flags ^= NODE_FLAGS_SPLIT;
		}
		else if (flags & NODE_FLAGS_GROUP)
		{
			group_node_1(n, batch_out);
			//n->flags ^= NODE_FLAGS_GROUP;
		}
	}
}

void WorldWatcher::post_process_batch(SmartContainer<class WorldOctreeNode*>& batch_in)
{
	int count = (int)batch_in.count;
	for (int i = 0; i < count; i++)
	{
		WorldOctreeNode* n = batch_in[i];
		int flags = n->flags;
		int stage = n->generation_stage;

		if (flags & NODE_FLAGS_SPLIT)
		{
			for (int i = 0; i < 8; i++)
			{
				WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
				c->flags |= NODE_FLAGS_DRAW;
			}
			n->flags &= ~NODE_FLAGS_DRAW;
			n->flags |= NODE_FLAGS_SUPERCEDED;
			if (n->gl_chunk)
			{
				world->gl_allocator.free_element(n->gl_chunk);
				n->gl_chunk = 0;
			}
			n->generation_stage = GENERATION_STAGES_DONE;
			n->flags ^= NODE_FLAGS_SPLIT;
			unlink_renderable(n);
		}
		else if (flags & NODE_FLAGS_GROUP)
		{
			{
				std::unique_lock<std::mutex> c_lock(world->chunk_mutex);
				for (int i = 0; i < 8; i++)
				{
					WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
					unlink_renderable(c);
					generator.binary_allocator.free_element(c->chunk->binary_block);
					generator.vi_allocator.free_element(c->chunk->vi);
					generator.cell_allocator.free_element(c->chunk->cell_block);
					generator.inds_allocator.free_element(c->chunk->inds_block);
					world->gl_allocator.free_element(c->gl_chunk);
					world->chunk_pool.deleteElement(c->chunk);
					world->node_pool.deleteElement(c);
				}
			}

			n->flags |= NODE_FLAGS_DRAW;
			n->flags &= ~(NODE_FLAGS_SUPERCEDED | NODE_FLAGS_GENERATING);

			world->group_node(n);

			n->flags ^= NODE_FLAGS_GROUP;
		}
	}
}

void WorldWatcher::split_node(WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& generate_batch_out)
{
	//n->flags &= ~NODE_FLAGS_DRAW;
	//n->flags |= NODE_FLAGS_DRAW_CHILDREN;

	//n->force_chunk_octree = true;
	world->split_node(n);

	{
		for (int i = 0; i < 8; i++)
		{
			WorldOctreeNode* c = (WorldOctreeNode*)n->children[i];
			if (!c)
			{
				print() << "ERROR! NON-EXISTENT CHILD!" << std::endl;
			}
			//assert(c);
			c->flags = NODE_FLAGS_DIRTY;
			c->generation_stage = GENERATION_STAGES_GENERATING;
			generate_batch_out.push_back(c);
			push_back_renderable(c);
		}
	}
}

void WorldWatcher::group_node_1(WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& generate_batch_out)
{
	//n->flags |= NODE_FLAGS_GENERATING;
	//n->force_chunk_octree = true;

	generate_batch_out.push_back(n);
	push_back_renderable(n);
}

void WorldWatcher::process_stitching(SmartContainer<class WorldOctreeNode*>& batch_in)
{
}

void WorldWatcher::unlink_renderable(WorldOctreeNode* n)
{
	if (renderables_head == n)
	{
		renderables_head = n->renderable_next;
	}
	if (renderables_tail == n)
	{
		renderables_tail = n->renderable_prev;
	}
	n->unlink();
	renderables_count--;
}

void WorldWatcher::push_back_renderable(WorldOctreeNode* n)
{
	if (n->renderable_prev || n->renderable_next)
	{
		print() << "ERROR: Attempt to push back linked renderable!" << std::endl;
		return;
	}
	renderables_tail->renderable_next = n;
	n->renderable_prev = renderables_tail;

	renderables_tail = n;
	renderables_count++;
}
