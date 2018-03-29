#include "PCH.h"
#include "WorldStitcher.hpp"
#include "WorldOctreeNode.hpp"
#include "DefaultOptions.h"
#include "Tables.hpp"
#include "MCTable.h"
#include "DMCChunk.hpp"
#include <omp.h>

#define RESTITCH_ALL true

WorldStitcher::WorldStitcher()
{
	stage = STITCHING_STAGES_READY;
}

WorldStitcher::~WorldStitcher()
{
}

void WorldStitcher::init()
{
	gl_chunk.init(true, true);
}

void WorldStitcher::stitch_all(WorldOctreeNode* root)
{
	if (root->leaf_flag)
		return;

	vertices.count = 0;
	for (int i = 0; i < 8; i++)
		v_containers[i].count = 0;

	SmartContainer<WorldOctreeNode*> cells;
	gather_all_cells(root, cells);
	int count = (int)cells.count;

#pragma omp parallel for
	for (int i = 0; i < count; i++)
	{
		int thread_id = omp_get_thread_num();
		stitch_cell(cells[i], v_containers[thread_id]);
	}
	for (int i = 0; i < 8; i++)
	{
		vertices.push_back(v_containers[i]);
	}
}

void WorldStitcher::stitch_all(emilib::HashMap<MortonCode, class WorldOctreeNode*>& sub_leaves, spp::sparse_hash_map<MortonCode, DMCNode*>& chunk_nodes)
{
	vertices.count = 0;
	SmartContainer<WorldOctreeNode*> chunks;

	clock_t start_clock = clock();
	for (auto it = sub_leaves.begin(); it != sub_leaves.end(); ++it)
	{
		WorldOctreeNode* n = it->second;
		if (!n)
			continue;
		if (!n->leaf_flag)
			continue;
		mark_chunks(it->second, sub_leaves, chunks);
	}
	double chunks_delta = clock() - start_clock;

	using namespace std;
	cout << "Generated " << (int)chunks.count << " dual chunks in " << (int)(chunks_delta / (double)CLOCKS_PER_SEC * 1000.0) << "ms." << endl;
	cout << "Stitching dual chunks...";

	SmartContainer<DualVertex> v_containers[8];
	start_clock = clock();
	int count = (int)chunks.count;
	std::atomic<int> skipped_count = 0;
	int ten = count / 10;
	//#pragma omp parallel for
	for (int i = 0; i < count; i++)
	{
		int thread_id = omp_get_thread_num();
		if (!stitch_dual_chunk(chunks[i], v_containers[thread_id], sub_leaves))
			skipped_count++;
		if (i % ten == 0)
			std::cout << i / ten * 10 << "%...";
	}

	for (int i = 0; i < 8; i++)
	{
		vertices.push_back(v_containers[i]);
	}

	double delta = clock() - start_clock;

	cout << "done. Generated " << (int)vertices.count / 3 << " tris in " << (int)(delta / (double)CLOCKS_PER_SEC * 1000.0) << "ms (" << skipped_count << " skipped)" << endl;
}

void WorldStitcher::stitch_all_linear(emilib::HashMap<MortonCode, class WorldOctreeNode*>& chunks)
{
}

void WorldStitcher::upload()
{
	gl_chunk.set_data(gl_chunk.p_data, gl_chunk.c_data, 0);
}

void WorldStitcher::format()
{
	gl_chunk.format_data_tris(vertices);
}

void WorldStitcher::gather_all_cells(WorldOctreeNode* n, SmartContainer<WorldOctreeNode*>& out)
{
	if (n->world_leaf_flag)
	{
		out.push_back(n);
		return;
	}
	bool contains_mesh = false;
	bool stitch_marked = RESTITCH_ALL;
	for (int i = 0; i < 8; i++)
	{
		assert(n->children[i]);
		WorldOctreeNode* w = ((WorldOctreeNode*)n->children[i]);
		if (!w->world_leaf_flag || w->chunk->contains_mesh)
		{
			contains_mesh = true;
		}
		if (!w->world_leaf_flag || w->stitch_flag)
		{
			stitch_marked = true;
		}
	}
	if (!contains_mesh || !stitch_marked)
		return;

	out.push_back(n);
	if (n->world_leaf_flag)
		return;

	for (int i = 0; i < 8; i++)
	{
		gather_all_cells((WorldOctreeNode*)n->children[i], out);
	}
}

void WorldStitcher::gather_marked_cells(SmartContainer<WorldOctreeNode*>& in_out)
{
	for (int i = 0; i < (int)in_out.count; i++)
	{
		WorldOctreeNode* w = in_out[i];
		assert(w);
		WorldOctreeNode* parent = (WorldOctreeNode*)w->parent;
		if (parent && !parent->stitch_flag)
		{
			parent->stitch_flag = true;
			parent->stitch_stored_flag = true;
			in_out.push_back(parent);
		}
	}
}

void WorldStitcher::stitch_batch(SmartContainer<WorldOctreeNode*>& batch)
{
	vertices.count = 0;
	for (int i = 0; i < 8; i++)
		v_containers[i].count = 0;

	clock_t start_clock = clock();

	int count = (int)batch.count;

#pragma omp parallel for
	for (int i = 0; i < count; i++)
	{
		int thread_id = omp_get_thread_num();
		stitch_cell(batch[i], v_containers[thread_id]);
	}
	for (int i = 0; i < 8; i++)
	{
		vertices.push_back(v_containers[i]);
	}
}

void WorldStitcher::stitch_cell(OctreeNode* n, SmartContainer<DualVertex>& v_out, bool allow_children)
{
	if (n->leaf_flag || !n->world_node_flag || ((WorldOctreeNode*)n)->world_leaf_flag)
		return;

	/*if (allow_children && n->world_node_flag)
	{
	for (int i = 0; i < 8; i++)
	{
	if (!n->children[i])
	continue;
	if (n->children[i]->world_node_flag)
	{
	stitch_cell(n->children[i], v_out);
	}
	}
	}*/

	OctreeNode* children[8];

	WorldOctreeNode* w = (WorldOctreeNode*)n;
	if (!w->force_chunk_octree)
		memcpy(children, n->children, sizeof(OctreeNode*) * 8);
	else
		memcpy(children, w->chunk->octree.children, sizeof(OctreeNode*) * 8);

	if(children[0] && children[3])
	stitch_face_xy(children[0], children[3], v_out);
	if (children[1] && children[2])
	stitch_face_xy(children[1], children[2], v_out);
	if (children[4] && children[7])
	stitch_face_xy(children[4], children[7], v_out);
	if (children[5] && children[6])
	stitch_face_xy(children[5], children[6], v_out);

	stitch_face_zy(children[0], children[1], v_out);
	stitch_face_zy(children[3], children[2], v_out);
	stitch_face_zy(children[4], children[5], v_out);
	stitch_face_zy(children[7], children[6], v_out);

	stitch_face_xz(children[4], children[0], v_out);
	stitch_face_xz(children[5], children[1], v_out);
	stitch_face_xz(children[7], children[3], v_out);
	stitch_face_xz(children[6], children[2], v_out);

	stitch_edge_x(children[0], children[3], children[7], children[4], v_out);
	stitch_edge_x(children[1], children[2], children[6], children[5], v_out);

	stitch_edge_y(children[0], children[1], children[2], children[3], v_out);
	stitch_edge_y(children[4], children[5], children[6], children[7], v_out);

	stitch_edge_z(children[7], children[6], children[2], children[3], v_out);
	stitch_edge_z(children[4], children[5], children[1], children[0], v_out);

	stitch_indexes(children, v_out);
}

#define CHECK_WORLD(n, ind) (worlds[ind] && ((WorldOctreeNode*)n)->force_chunk_octree ? ((WorldOctreeNode*)n)->chunk->octree.children : n->children)

void WorldStitcher::stitch_face_xy(OctreeNode* n0, OctreeNode* n1, SmartContainer<DualVertex>& v_out)
{
	if (n0->leaf_flag && n1->leaf_flag)
	{
		//stitch_face(n0, n1);
		return;
	}
	if (n0->world_node_flag && n1->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		if (!w0->stitch_flag && !w1->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh)
			return;
	}

	bool leaves[2] = { n0->leaf_flag, n1->leaf_flag };
	bool worlds[2] = { n0->world_node_flag, n1->world_node_flag };
	OctreeNode* c0 = !leaves[0] ? CHECK_WORLD(n0, 0)[3] : n0;
	OctreeNode* c1 = !leaves[0] ? CHECK_WORLD(n0, 0)[2] : n0;
	OctreeNode* c2 = !leaves[1] ? CHECK_WORLD(n1, 1)[1] : n1;
	OctreeNode* c3 = !leaves[1] ? CHECK_WORLD(n1, 1)[0] : n1;

	OctreeNode* c4 = !leaves[0] ? CHECK_WORLD(n0, 0)[7] : n0;
	OctreeNode* c5 = !leaves[0] ? CHECK_WORLD(n0, 0)[6] : n0;
	OctreeNode* c6 = !leaves[1] ? CHECK_WORLD(n1, 1)[5] : n1;
	OctreeNode* c7 = !leaves[1] ? CHECK_WORLD(n1, 1)[4] : n1;

	if(c0 && c3)
	stitch_face_xy(c0, c3, v_out);
	if (c1 && c2)
	stitch_face_xy(c1, c2, v_out);
	if (c4 && c7)
	stitch_face_xy(c4, c7, v_out);
	if (c5 && c6)
	stitch_face_xy(c5, c6, v_out);

	if (c0 && c3 && c7 && c4)
	stitch_edge_x(c0, c3, c7, c4, v_out);
	if (c1 && c2 && c6 && c5)
	stitch_edge_x(c1, c2, c6, c5, v_out);
	if (c0 && c1 && c2 && c3)
	stitch_edge_y(c0, c1, c2, c3, v_out);
	if (c4 && c5 && c6 && c7)
	stitch_edge_y(c4, c5, c6, c7, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_face_zy(OctreeNode* n0, OctreeNode* n1, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0) || (n0->leaf_flag && n1->leaf_flag))
		return;
	if (n0->world_node_flag && n1->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		if (!w0->stitch_flag && !w1->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh)
			return;
	}

	bool leaves[2] = { n0->leaf_flag, n1->leaf_flag };
	bool worlds[2] = { n0->world_node_flag, n1->world_node_flag };
	OctreeNode* c0 = !leaves[0] ? CHECK_WORLD(n0, 0)[1] : n0;
	OctreeNode* c1 = !leaves[1] ? CHECK_WORLD(n1, 1)[0] : n1;
	OctreeNode* c2 = !leaves[1] ? CHECK_WORLD(n1, 1)[3] : n1;
	OctreeNode* c3 = !leaves[0] ? CHECK_WORLD(n0, 0)[2] : n0;

	OctreeNode* c4 = !leaves[0] ? CHECK_WORLD(n0, 0)[5] : n0;
	OctreeNode* c5 = !leaves[1] ? CHECK_WORLD(n1, 1)[4] : n1;
	OctreeNode* c6 = !leaves[1] ? CHECK_WORLD(n1, 1)[7] : n1;
	OctreeNode* c7 = !leaves[0] ? CHECK_WORLD(n0, 0)[6] : n0;

	stitch_face_zy(c0, c1, v_out);
	stitch_face_zy(c3, c2, v_out);
	stitch_face_zy(c4, c5, v_out);
	stitch_face_zy(c7, c6, v_out);

	stitch_edge_y(c0, c1, c2, c3, v_out);
	stitch_edge_y(c4, c5, c6, c7, v_out);
	stitch_edge_z(c7, c6, c2, c3, v_out);
	stitch_edge_z(c4, c5, c1, c0, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_face_xz(OctreeNode* n0, OctreeNode* n1, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0) || (n0->leaf_flag && n1->leaf_flag))
		return;
	if (n0->world_node_flag && n1->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		if (!w0->stitch_flag && !w1->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh)
			return;
	}

	bool leaves[2] = { n0->leaf_flag, n1->leaf_flag };
	bool worlds[2] = { n0->world_node_flag, n1->world_node_flag };
	OctreeNode* c0 = !leaves[1] ? CHECK_WORLD(n1, 1)[4] : n1;
	OctreeNode* c1 = !leaves[1] ? CHECK_WORLD(n1, 1)[5] : n1;
	OctreeNode* c2 = !leaves[1] ? CHECK_WORLD(n1, 1)[6] : n1;
	OctreeNode* c3 = !leaves[1] ? CHECK_WORLD(n1, 1)[7] : n1;

	OctreeNode* c4 = !leaves[0] ? CHECK_WORLD(n0, 0)[0] : n0;
	OctreeNode* c5 = !leaves[0] ? CHECK_WORLD(n0, 0)[1] : n0;
	OctreeNode* c6 = !leaves[0] ? CHECK_WORLD(n0, 0)[2] : n0;
	OctreeNode* c7 = !leaves[0] ? CHECK_WORLD(n0, 0)[3] : n0;

	stitch_face_xz(c4, c0, v_out);
	stitch_face_xz(c5, c1, v_out);
	stitch_face_xz(c7, c3, v_out);
	stitch_face_xz(c6, c2, v_out);

	stitch_edge_x(c0, c3, c7, c4, v_out);
	stitch_edge_x(c1, c2, c6, c5, v_out);
	stitch_edge_z(c7, c6, c2, c3, v_out);
	stitch_edge_z(c4, c5, c1, c0, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_edge_x(OctreeNode* n0, OctreeNode* n1, OctreeNode* n2, OctreeNode* n3, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0 || n2 == 0 || n3 == 0) || (n0->leaf_flag && n1->leaf_flag && n2->leaf_flag && n3->leaf_flag))
		return;
	if (n0->world_node_flag && n1->world_node_flag && n2->world_node_flag && n3->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		WorldOctreeNode* w2 = (WorldOctreeNode*)n2;
		WorldOctreeNode* w3 = (WorldOctreeNode*)n3;
		if (!w0->stitch_flag && !w1->stitch_flag && !w2->stitch_flag && !w3->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && w2->world_leaf_flag && w3->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh && !w2->chunk->contains_mesh && !w3->chunk->contains_mesh)
			return;
	}

	bool leaves[4] = { n0->leaf_flag, n1->leaf_flag, n2->leaf_flag, n3->leaf_flag };
	bool worlds[4] = { n0->world_node_flag, n1->world_node_flag, n2->world_node_flag, n3->world_node_flag };
	OctreeNode* c0 = !leaves[0] ? CHECK_WORLD(n0, 0)[7] : n0;
	OctreeNode* c1 = !leaves[0] ? CHECK_WORLD(n0, 0)[6] : n0;
	OctreeNode* c2 = !leaves[1] ? CHECK_WORLD(n1, 1)[5] : n1;
	OctreeNode* c3 = !leaves[1] ? CHECK_WORLD(n1, 1)[4] : n1;
	OctreeNode* c4 = !leaves[3] ? CHECK_WORLD(n3, 3)[3] : n3;
	OctreeNode* c5 = !leaves[3] ? CHECK_WORLD(n3, 3)[2] : n3;
	OctreeNode* c6 = !leaves[2] ? CHECK_WORLD(n2, 2)[1] : n2;
	OctreeNode* c7 = !leaves[2] ? CHECK_WORLD(n2, 2)[0] : n2;

	stitch_edge_x(c0, c3, c7, c4, v_out);
	stitch_edge_x(c1, c2, c6, c5, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_edge_y(OctreeNode* n0, OctreeNode* n1, OctreeNode* n2, OctreeNode* n3, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0 || n2 == 0 || n3 == 0) || (n0->leaf_flag && n1->leaf_flag && n2->leaf_flag && n3->leaf_flag))
		return;
	if (n0->world_node_flag && n1->world_node_flag && n2->world_node_flag && n3->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		WorldOctreeNode* w2 = (WorldOctreeNode*)n2;
		WorldOctreeNode* w3 = (WorldOctreeNode*)n3;
		if (!w0->stitch_flag && !w1->stitch_flag && !w2->stitch_flag && !w3->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && w2->world_leaf_flag && w3->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh && !w2->chunk->contains_mesh && !w3->chunk->contains_mesh)
			return;
	}

	bool leaves[4] = { n0->leaf_flag, n1->leaf_flag, n2->leaf_flag, n3->leaf_flag };
	bool worlds[4] = { n0->world_node_flag, n1->world_node_flag, n2->world_node_flag, n3->world_node_flag };
	OctreeNode* c0 = !leaves[0] ? CHECK_WORLD(n0, 0)[2] : n0;
	OctreeNode* c1 = !leaves[1] ? CHECK_WORLD(n1, 1)[3] : n1;
	OctreeNode* c2 = !leaves[2] ? CHECK_WORLD(n2, 2)[0] : n2;
	OctreeNode* c3 = !leaves[3] ? CHECK_WORLD(n3, 3)[1] : n3;
	OctreeNode* c4 = !leaves[0] ? CHECK_WORLD(n0, 0)[6] : n0;
	OctreeNode* c5 = !leaves[1] ? CHECK_WORLD(n1, 1)[7] : n1;
	OctreeNode* c6 = !leaves[2] ? CHECK_WORLD(n2, 2)[4] : n2;
	OctreeNode* c7 = !leaves[3] ? CHECK_WORLD(n3, 3)[5] : n3;

	stitch_edge_y(c0, c1, c2, c3, v_out);
	stitch_edge_y(c4, c5, c6, c7, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_edge_z(OctreeNode* n0, OctreeNode* n1, OctreeNode* n2, OctreeNode* n3, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0 || n2 == 0 || n3 == 0) || (n0->leaf_flag && n1->leaf_flag && n2->leaf_flag && n3->leaf_flag))
		return;
	if (n0->world_node_flag && n1->world_node_flag && n2->world_node_flag && n3->world_node_flag)
	{
		WorldOctreeNode* w0 = (WorldOctreeNode*)n0;
		WorldOctreeNode* w1 = (WorldOctreeNode*)n1;
		WorldOctreeNode* w2 = (WorldOctreeNode*)n2;
		WorldOctreeNode* w3 = (WorldOctreeNode*)n3;
		if (!w0->stitch_flag && !w1->stitch_flag && !w2->stitch_flag && !w3->stitch_flag && !RESTITCH_ALL)
			return;
		if (w0->world_leaf_flag && w1->world_leaf_flag && w2->world_leaf_flag && w3->world_leaf_flag && !w0->chunk->contains_mesh && !w1->chunk->contains_mesh && !w2->chunk->contains_mesh && !w3->chunk->contains_mesh)
			return;
	}

	bool leaves[4] = { n0->leaf_flag, n1->leaf_flag, n2->leaf_flag, n3->leaf_flag };
	bool worlds[4] = { n0->world_node_flag, n1->world_node_flag, n2->world_node_flag, n3->world_node_flag };
	OctreeNode* c0 = !leaves[3] ? CHECK_WORLD(n3, 3)[5] : n3;
	OctreeNode* c1 = !leaves[2] ? CHECK_WORLD(n2, 2)[4] : n2;
	OctreeNode* c2 = !leaves[2] ? CHECK_WORLD(n2, 2)[7] : n2;
	OctreeNode* c3 = !leaves[3] ? CHECK_WORLD(n3, 3)[6] : n3;
	OctreeNode* c4 = !leaves[0] ? CHECK_WORLD(n0, 0)[1] : n0;
	OctreeNode* c5 = !leaves[1] ? CHECK_WORLD(n1, 1)[0] : n1;
	OctreeNode* c6 = !leaves[1] ? CHECK_WORLD(n1, 1)[3] : n1;
	OctreeNode* c7 = !leaves[0] ? CHECK_WORLD(n0, 0)[2] : n0;

	stitch_edge_z(c7, c6, c2, c3, v_out);
	stitch_edge_z(c4, c5, c1, c0, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

__forceinline glm::vec3 _get_intersection(glm::vec3& v0, glm::vec3& v1, float s0, float s1, float isolevel)
{
	float mu = (isolevel - s0) / (s1 - s0);
	glm::vec3 delta_v = (v1 - v0) * mu;
	return delta_v + v0;
}

#define DUAL_VERTEX(i, v0, v1) \
if (edge_mask & (1 << i)) { \
	crossings[i].p = _get_intersection(pos[v0], pos[v1], samples[v0], samples[v1], 0.0f); \
	crossings[i].color = glm::vec3(0.85f, 1.0f, 0.85f); }

#define INDS_LEAF(n, ind) \
(n->world_node_flag && ((WorldOctreeNode*)n)->force_chunk_octree ? ((WorldOctreeNode*)n)->chunk->octree.children : n->children)

void WorldStitcher::stitch_indexes(OctreeNode* nodes[8], SmartContainer<DualVertex>& v_out)
{
	for (int i = 0; i < 8; i++)
	{
		if (!nodes[i])
			return;
	}

	while(nodes[0] && !nodes[0]->leaf_flag)
		nodes[0] = !nodes[0]->leaf_flag ? INDS_LEAF(nodes[0], 0)[6] : nodes[0];
	while (nodes[1] && !nodes[1]->leaf_flag)
		nodes[1] = !nodes[1]->leaf_flag ? INDS_LEAF(nodes[1], 1)[7] : nodes[1];
	while (nodes[2] && !nodes[2]->leaf_flag)
		nodes[2] = !nodes[2]->leaf_flag ? INDS_LEAF(nodes[2], 2)[4] : nodes[2];
	while (nodes[3] && !nodes[3]->leaf_flag)
		nodes[3] = !nodes[3]->leaf_flag ? INDS_LEAF(nodes[3], 3)[5] : nodes[3];
	while (nodes[4] && !nodes[4]->leaf_flag)
		nodes[4] = !nodes[4]->leaf_flag ? INDS_LEAF(nodes[4], 4)[2] : nodes[4];
	while (nodes[5] && !nodes[5]->leaf_flag)
		nodes[5] = !nodes[5]->leaf_flag ? INDS_LEAF(nodes[5], 5)[3] : nodes[5];
	while (nodes[6] && !nodes[6]->leaf_flag)
		nodes[6] = !nodes[6]->leaf_flag ? INDS_LEAF(nodes[6], 6)[0] : nodes[6];
	while (nodes[7] && !nodes[7]->leaf_flag)
		nodes[7] = !nodes[7]->leaf_flag ? INDS_LEAF(nodes[7], 7)[1] : nodes[7];

	for (int i = 0; i < 8; i++)
	{
		if (!nodes[i])
			return;
	}

	OctreeNode* new_nodes[8] = { nodes[0], nodes[3], nodes[4], nodes[7], nodes[1], nodes[2], nodes[5], nodes[6] };

	using namespace glm;
	vec3 pos[8];
	float samples[8];
	int mask = 0;
	for (int i = 0; i < 8; i++)
	{
		float size = new_nodes[i]->size * 0.5f;
		//float delta = new_nodes[i]->size / (float)(31);
		pos[i] = new_nodes[i]->pos;// +vec3(size);
		samples[i] = ((DMCNode*)new_nodes[i])->sample;
		if (samples[i] < 0.0f)
			mask |= 1 << i;
	}
	if (mask == 0 || mask == 255)
		return;

	DualVertex crossings[12];
	int edge_mask = MarchingCubes::edge_map[mask];

	DUAL_VERTEX(0, 0, 4);
	DUAL_VERTEX(1, 1, 5);
	DUAL_VERTEX(2, 2, 6);
	DUAL_VERTEX(3, 3, 7);

	DUAL_VERTEX(4, 0, 2);
	DUAL_VERTEX(5, 1, 3);
	DUAL_VERTEX(6, 4, 6);
	DUAL_VERTEX(7, 5, 7);

	DUAL_VERTEX(8, 0, 1);
	DUAL_VERTEX(9, 2, 3);
	DUAL_VERTEX(10, 4, 5);
	DUAL_VERTEX(11, 6, 7);

	for (int i = 0; i < 16; i += 3)
	{
		int e0 = MarchingCubes::tri_table[mask][i + 0];
		if (e0 == -1)
			break;
		int e1 = MarchingCubes::tri_table[mask][i + 1];
		int e2 = MarchingCubes::tri_table[mask][i + 2];

		//TODO: don't push degenerate triangles

		v_out.push_back(crossings[e0]);
		v_out.push_back(crossings[e1]);
		v_out.push_back(crossings[e2]);
	}
}

void WorldStitcher::mark_chunks(WorldOctreeNode* n, emilib::HashMap<MortonCode, class WorldOctreeNode*>& leaves, SmartContainer<WorldOctreeNode*>& dest)
{
	if (!n)
		return;
	assert(n);

	uint64_t v_codes[8];
	int lv;
	leaf2vert(n->morton_code.code, v_codes, &lv);
	for (int i = 0; i < 8; i++)
	{
		if (v_codes[i] == 0)
			continue;

		bool mesh = false;
		uint64_t keys[8];
		WorldOctreeNode* final_nodes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		vert2leaf(v_codes[i], lv, keys);
		for (int j = 0; j < 8; j++)
		{
			uint64_t key_j = keys[j];
			WorldOctreeNode* node_at = leaves[key_j];
			if (!node_at)
				continue;
			final_nodes[j] = node_at;
			if (!node_at->leaf_flag)
				goto next_vertex;
			if (j < i)
				goto next_vertex;
		}

		for (int j = 0; j < 8; j++)
		{
			while (!final_nodes[j] && keys[j] != 0)
			{
				final_nodes[j] = leaves[keys[j]];
				keys[j] >>= 3;
			}
			if (!final_nodes[j])
				goto next_vertex;
		}

		/*for (int i = 0; i < 8; i++)
		{
		if (final_nodes[i]->chunk->contains_mesh)
		{
		mesh = true;
		break;
		}
		}*/
		//if (mesh)
		//{
		for (int i = 0; i < 8; i++)
		{
			if (!final_nodes[i]->stitch_flag && final_nodes[i]->chunk->contains_mesh)
			{
				final_nodes[i]->stitch_flag = true;
				dest.push_back(final_nodes[i]);
			}
		}
		//}
		//dest.push_back(PseudoDualChunk(final_nodes));
		//stitch_mc(final_nodes, v_out);

	next_vertex:
		continue;
	}
}

bool WorldStitcher::stitch_dual_chunk(WorldOctreeNode* n, SmartContainer<DualVertex>& v_out, emilib::HashMap<MortonCode, WorldOctreeNode*>& chunk_nodes)
{
	auto& nodes = n->chunk->nodes;
	int count = nodes.count;
	for (int i = 0; i < count; i++)
	{
		MortonCode& morton_code = nodes[i]->morton_code;
		stitch_primal(morton_code, v_out, chunk_nodes);
	}

	return true;
}

void WorldStitcher::stitch_primal(MortonCode& morton_code, SmartContainer<DualVertex>& v_out, emilib::HashMap<MortonCode, WorldOctreeNode*>& chunk_nodes)
{
	uint64_t v_codes[8];
	int lv;
	leaf2vert(morton_code.code, v_codes, &lv);
	for (int i = 0; i < 8; i++)
	{
		if (v_codes[i] == 0)
			continue;
		uint64_t v = v_codes[i];
		int x = (v & 0b100) >> 2 | (v & 0b100000) >> 4 | (v & 0b100000000) >> 6 | (v & 0b100000000000) >> 8 | (v & 0b100000000000000) >> 10;
		int y = (v & 0b010) >> 1 | (v & 0b010000) >> 3 | (v & 0b010000000) >> 5 | (v & 0b010000000000) >> 7 | (v & 0b010000000000000) >> 9;
		int z = (v & 0b001) >> 0 | (v & 0b001000) >> 2 | (v & 0b001000000) >> 4 | (v & 0b001000000000) >> 6 | (v & 0b001000000000000) >> 8;
		if (x != 0 && x != 31 && y != 0 && y != 31 && z != 0 && z != 31)
			continue;

		uint64_t keys[8];
		DMCNode* final_nodes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		WorldOctreeNode* chunks[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		DMCNode pseudo_nodes[8];
		vert2leaf(v_codes[i], lv, keys);

		// Get the base chunks
		for (int j = 0; j < 8; j++)
		{
			uint64_t chunk_key = keys[j] >> 15;
			WorldOctreeNode* at = 0;
			auto f = chunk_nodes.try_get(MortonCode(chunk_key));
			if (f)
			{
				at = *f;
				if (!at)
					continue;
			}
			else
				continue;
			if (!at->leaf_flag)
				goto next_vertex;
			if (j > i)
				goto next_vertex;
			chunks[j] = at;
		}

		// Traverse up the tree to find the existing chunks
		for (int j = 0; j < 8; j++)
		{
			uint64_t chunk_key = keys[j] >> 18;
			while (!chunks[j] && chunk_key != 0)
			{
				auto f = chunk_nodes.try_get(MortonCode(chunk_key));
				if (f)
					chunks[j] = *f;
				chunk_key >>= 3;
			}
			if (!chunks[j])
				goto next_vertex;
		}

		for (int j = 0; j < 8; j++)
		{
			DMCChunk* c = chunks[j]->chunk;
			assert(c);
			uint64_t code = keys[j] & 0x7FFF;
			if (!c->contains_mesh)
				code &= 7;
			int x = (code & 0b100) >> 2 | (code & 0b100000) >> 4 | (code & 0b100000000) >> 6 | (code & 0b100000000000) >> 8 | (code & 0b100000000000000) >> 10;
			int y = (code & 0b010) >> 1 | (code & 0b010000) >> 3 | (code & 0b010000000) >> 5 | (code & 0b010000000000) >> 7 | (code & 0b010000000000000) >> 9;
			int z = (code & 0b001) >> 0 | (code & 0b001000) >> 2 | (code & 0b001000000) >> 4 | (code & 0b001000000000) >> 6 | (code & 0b001000000000000) >> 8;
			assert(x < 32 && y < 32 && z < 32);

			if (!c->contains_mesh)
				final_nodes[j] = (DMCNode*)c->octree.children[code];
			else
			{
				float delta = 1.0f / 32.0f * c->size;
				pseudo_nodes[j].pos = c->pos + glm::vec3((float)x * delta, (float)y * delta, (float)z * delta);
				pseudo_nodes[j].size = delta;
				pseudo_nodes[j].sample = c->density_block->data[x * 32 * 32 + y * 32 + z];
				final_nodes[j] = &pseudo_nodes[j];
			}
		}

		stitch_mc(final_nodes, v_out);

	next_vertex:
		continue;
	}
}

int WorldStitcher::key2level(uint64_t key)
{
	int level = 0;
	while (key > 1)
	{
		level++;
		key >>= 3;
	}

	return level;
}

void WorldStitcher::leaf2vert(uint64_t k, uint64_t* v_out, int* lv)
{
	const uint64_t dil_z = 0b001001001001001001001001001001001001001001001001001001001;
	const uint64_t dil_y = 0b010010010010010010010010010010010010010010010010010010010;
	const uint64_t dil_x = 0b100100100100100100100100100100100100100100100100100100100;

	*lv = key2level(k);
	uint64_t lv_k = 1ll << 3ll * (*lv);
	for (int i = 0; i < 8; i++)
	{
		uint64_t vk =
			(((k | ~dil_x) + (i & dil_x)) & dil_x) |
			(((k | ~dil_y) + (i & dil_y)) & dil_y) |
			(((k | ~dil_z) + (i & dil_z)) & dil_z);
		if ((vk >= (lv_k << 1)) || !((vk - lv_k) & dil_x) || !((vk - lv_k) & dil_y) || !((vk - lv_k) & dil_z))
			v_out[i] = 0;
		else
			v_out[i] = vk;
	}
}

void WorldStitcher::vert2leaf(uint64_t c, int lv, uint64_t* n_out)
{
	const int MAX_LEVEL = 21;
	const uint64_t dil_z = 0b001001001001001001001001001001001001001001001001001001001;
	const uint64_t dil_y = 0b010010010010010010010010010010010010010010010010010010010;
	const uint64_t dil_x = 0b100100100100100100100100100100100100100100100100100100100;

	// The paper performs the bit shifting to truncate the vertex codes because leaf2vert is supposed to produce codes at the max level, but it doesn't, so no truncation needed
	uint64_t dc = c;// >> 3 * (MAX_LEVEL - lv);
	for (int i = 0; i < 8; i++)
	{
		uint64_t o =
			(((dc & dil_x) - (i & dil_x)) & dil_x) |
			(((dc & dil_y) - (i & dil_y)) & dil_y) |
			(((dc & dil_z) - (i & dil_z)) & dil_z);
		n_out[i] = o;
	}
}

void WorldStitcher::stitch_mc(DMCNode* nodes[8], SmartContainer<DualVertex>& v_out)
{
	using namespace glm;
	vec3 pos[8];
	float samples[8];
	int mask = 0;
	for (int i = 0; i < 8; i++)
	{
		if (!nodes[i])
			return;
		float size = nodes[i]->size * 0.5f;
		pos[i] = nodes[i]->pos + size;
		samples[i] = nodes[i]->sample;
		if (samples[i] < 0.0f)
			mask |= 1 << i;
	}
	if (mask == 0 || mask == 255)
		return;

	DualVertex crossings[12];
	int edge_mask = MarchingCubes::edge_map[mask];

	DUAL_VERTEX(0, 0, 4);
	DUAL_VERTEX(1, 1, 5);
	DUAL_VERTEX(2, 2, 6);
	DUAL_VERTEX(3, 3, 7);

	DUAL_VERTEX(4, 0, 2);
	DUAL_VERTEX(5, 1, 3);
	DUAL_VERTEX(6, 4, 6);
	DUAL_VERTEX(7, 5, 7);

	DUAL_VERTEX(8, 0, 1);
	DUAL_VERTEX(9, 2, 3);
	DUAL_VERTEX(10, 4, 5);
	DUAL_VERTEX(11, 6, 7);

	for (int i = 0; i < 16; i += 3)
	{
		int e0 = MarchingCubes::tri_table[mask][i + 0];
		if (e0 == -1)
			break;
		int e1 = MarchingCubes::tri_table[mask][i + 1];
		int e2 = MarchingCubes::tri_table[mask][i + 2];

		//TODO: don't push degenerate triangles

		v_out.push_back(crossings[e0]);
		v_out.push_back(crossings[e1]);
		v_out.push_back(crossings[e2]);
	}
}

