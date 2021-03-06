#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <unordered_map>
#include <glm/glm.hpp>
#include "ThreadDebug.hpp"
#include "SmartContainer.hpp"
#include "ChunkGenerator.hpp"
#include "WorldOctreeNode.hpp"
#include "HashMap.hpp"
#include "sparsepp/spp.h"
#include <map>

class WorldWatcher : public ThreadDebug
{
public:
	WorldWatcher();
	~WorldWatcher();

	void init(class WorldOctree* _world, glm::vec3 focus_pos);
	void update();
	void stop();

	glm::vec3 focus_pos;
	glm::vec3 last_focus_pos;

	std::mutex _mutex;
	std::mutex renderables_mutex;

	WorldOctreeNode* renderables_head;
	WorldOctreeNode* renderables_tail;
	SmartContainer<class WorldOctreeNode*> destroy_watchlist;
	std::atomic<int> renderables_count;
	ChunkGenerator generator;
	std::condition_variable upload_cv;

	emilib::HashMap<MortonCode, WorldOctreeNode*> leaf_nodes;
	//emilib::HashMap<MortonCode, DMCNode*> chunk_nodes;
	spp::sparse_hash_map<MortonCode, DMCNode*> chunk_nodes;
	//std::map<MortonCode, DMCNode*> chunk_nodes;

private:
	class WorldOctree* world;
	std::thread _thread;
	std::atomic<bool> _stop;

	void check_leaves(SmartContainer<class WorldOctreeNode*>& batch_out, const int max_gen);
	void handle_split_check(class WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& batch_out);
	void handle_group_check(class WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& batch_out);

	void process_batch(SmartContainer<class WorldOctreeNode*>& batch_in, SmartContainer<class WorldOctreeNode*>& batch_out, SmartContainer<class WorldOctreeNode*>& stitch_batch);
	void post_process_batch(SmartContainer<class WorldOctreeNode*>& batch_in);
	void split_node(class WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& generate_batch_out, SmartContainer<class WorldOctreeNode*>& stitch_batch);
	void group_node_1(class WorldOctreeNode* n, SmartContainer<class WorldOctreeNode*>& generate_batch_out);
	void process_stitching(SmartContainer<class WorldOctreeNode*>& batch_in);

	void unlink_renderable(class WorldOctreeNode* n);
	void push_back_renderable(class WorldOctreeNode* n);

	void add_leaves(class WorldOctreeNode* n);
};
