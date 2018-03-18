#include "PCH.h"
#include "WorldStitcher.hpp"
#include "WorldOctreeNode.hpp"
#include "DefaultOptions.h"
#include "Tables.hpp"
#include "MCTable.h"
#include "DMCChunk.hpp"

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
	vertices.count = 0;
	OctreeNode* start = root;
	stitch_cell(start, vertices);
}

void WorldStitcher::stitch_leaves(SmartContainer<WorldOctreeNode*> chunk_leaves, emilib::HashMap<MortonCode, class DMCNode*>& sub_leaves)
{
	vertices.count = 0;
	int count = (int)chunk_leaves.count;
	for (int i = 0; i < count; i++)
	{
		int sub_count = (int)chunk_leaves[i]->chunk->leaves.count;
		for (int k = 0; k < sub_count; k++)
		{
			stitch_leaf(&chunk_leaves[i]->chunk->leaves[k], sub_leaves, vertices);
		}
	}

	printf("Done stitching (%i verts)\n", (int)vertices.count);
}

void WorldStitcher::upload()
{
	gl_chunk.set_data(gl_chunk.p_data, gl_chunk.c_data, 0);
}

void WorldStitcher::format()
{
	gl_chunk.format_data_tris(vertices);
}

void WorldStitcher::stitch_cell(OctreeNode* n, SmartContainer<DualVertex>& v_out)
{
	if (n->leaf_flag || !n->world_node_flag)
		return;

	if (n->world_node_flag)
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
	}

	OctreeNode* children[8];

	WorldOctreeNode* w = (WorldOctreeNode*)n;
	if (!w->force_chunk_octree)
		memcpy(children, n->children, sizeof(OctreeNode*) * 8);
	else
		memcpy(children, w->chunk->octree.children, sizeof(OctreeNode*) * 8);

	stitch_face_xy(children[0], children[3], v_out);
	stitch_face_xy(children[1], children[2], v_out);
	stitch_face_xy(children[4], children[7], v_out);
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

#define CHECK_WORLD(n) (n->world_node_flag && ((WorldOctreeNode*)n)->force_chunk_octree ? ((WorldOctreeNode*)n)->chunk->octree.children : n->children)
void WorldStitcher::stitch_face_xy(OctreeNode* n0, OctreeNode* n1, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0) || (n0->leaf_flag && n1->leaf_flag))
		return;

	OctreeNode* c0 = !n0->leaf_flag ? CHECK_WORLD(n0)[3] : n0;
	OctreeNode* c1 = !n0->leaf_flag ? CHECK_WORLD(n0)[2] : n0;
	OctreeNode* c2 = !n1->leaf_flag ? CHECK_WORLD(n1)[1] : n1;
	OctreeNode* c3 = !n1->leaf_flag ? CHECK_WORLD(n1)[0] : n1;

	OctreeNode* c4 = !n0->leaf_flag ? CHECK_WORLD(n0)[7] : n0;
	OctreeNode* c5 = !n0->leaf_flag ? CHECK_WORLD(n0)[6] : n0;
	OctreeNode* c6 = !n1->leaf_flag ? CHECK_WORLD(n1)[5] : n1;
	OctreeNode* c7 = !n1->leaf_flag ? CHECK_WORLD(n1)[4] : n1;

	stitch_face_xy(c0, c3, v_out);
	stitch_face_xy(c1, c2, v_out);
	stitch_face_xy(c4, c7, v_out);
	stitch_face_xy(c5, c6, v_out);

	stitch_edge_x(c0, c3, c7, c4, v_out);
	stitch_edge_x(c1, c2, c6, c5, v_out);
	stitch_edge_y(c0, c1, c2, c3, v_out);
	stitch_edge_y(c4, c5, c6, c7, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_face_zy(OctreeNode* n0, OctreeNode* n1, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0) || (n0->leaf_flag && n1->leaf_flag))
		return;

	OctreeNode* c0 = !n0->leaf_flag ? CHECK_WORLD(n0)[1] : n0;
	OctreeNode* c1 = !n1->leaf_flag ? CHECK_WORLD(n1)[0] : n1;
	OctreeNode* c2 = !n1->leaf_flag ? CHECK_WORLD(n1)[3] : n1;
	OctreeNode* c3 = !n0->leaf_flag ? CHECK_WORLD(n0)[2] : n0;

	OctreeNode* c4 = !n0->leaf_flag ? CHECK_WORLD(n0)[5] : n0;
	OctreeNode* c5 = !n1->leaf_flag ? CHECK_WORLD(n1)[4] : n1;
	OctreeNode* c6 = !n1->leaf_flag ? CHECK_WORLD(n1)[7] : n1;
	OctreeNode* c7 = !n0->leaf_flag ? CHECK_WORLD(n0)[6] : n0;

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

	OctreeNode* c0 = !n1->leaf_flag ? CHECK_WORLD(n1)[4] : n1;
	OctreeNode* c1 = !n1->leaf_flag ? CHECK_WORLD(n1)[5] : n1;
	OctreeNode* c2 = !n1->leaf_flag ? CHECK_WORLD(n1)[6] : n1;
	OctreeNode* c3 = !n1->leaf_flag ? CHECK_WORLD(n1)[7] : n1;

	OctreeNode* c4 = !n0->leaf_flag ? CHECK_WORLD(n0)[0] : n0;
	OctreeNode* c5 = !n0->leaf_flag ? CHECK_WORLD(n0)[1] : n0;
	OctreeNode* c6 = !n0->leaf_flag ? CHECK_WORLD(n0)[2] : n0;
	OctreeNode* c7 = !n0->leaf_flag ? CHECK_WORLD(n0)[3] : n0;

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

	OctreeNode* c0 = !n0->leaf_flag ? CHECK_WORLD(n0)[7] : n0;
	OctreeNode* c1 = !n0->leaf_flag ? CHECK_WORLD(n0)[6] : n0;
	OctreeNode* c2 = !n1->leaf_flag ? CHECK_WORLD(n1)[5] : n1;
	OctreeNode* c3 = !n1->leaf_flag ? CHECK_WORLD(n1)[4] : n1;
	OctreeNode* c4 = !n3->leaf_flag ? CHECK_WORLD(n3)[3] : n3;
	OctreeNode* c5 = !n3->leaf_flag ? CHECK_WORLD(n3)[2] : n3;
	OctreeNode* c6 = !n2->leaf_flag ? CHECK_WORLD(n2)[1] : n2;
	OctreeNode* c7 = !n2->leaf_flag ? CHECK_WORLD(n2)[0] : n2;

	stitch_edge_x(c0, c3, c7, c4, v_out);
	stitch_edge_x(c1, c2, c6, c5, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_edge_y(OctreeNode* n0, OctreeNode* n1, OctreeNode* n2, OctreeNode* n3, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0 || n2 == 0 || n3 == 0) || (n0->leaf_flag && n1->leaf_flag && n2->leaf_flag && n3->leaf_flag))
		return;

	OctreeNode* c0 = !n0->leaf_flag ? CHECK_WORLD(n0)[2] : n0;
	OctreeNode* c1 = !n1->leaf_flag ? CHECK_WORLD(n1)[3] : n1;
	OctreeNode* c2 = !n2->leaf_flag ? CHECK_WORLD(n2)[0] : n2;
	OctreeNode* c3 = !n3->leaf_flag ? CHECK_WORLD(n3)[1] : n3;
	OctreeNode* c4 = !n0->leaf_flag ? CHECK_WORLD(n0)[6] : n0;
	OctreeNode* c5 = !n1->leaf_flag ? CHECK_WORLD(n1)[7] : n1;
	OctreeNode* c6 = !n2->leaf_flag ? CHECK_WORLD(n2)[4] : n2;
	OctreeNode* c7 = !n3->leaf_flag ? CHECK_WORLD(n3)[5] : n3;

	stitch_edge_y(c0, c1, c2, c3, v_out);
	stitch_edge_y(c4, c5, c6, c7, v_out);

	OctreeNode* ns[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };
	stitch_indexes(ns, v_out);
}

void WorldStitcher::stitch_edge_z(OctreeNode* n0, OctreeNode* n1, OctreeNode* n2, OctreeNode* n3, SmartContainer<DualVertex>& v_out)
{
	if ((n0 == 0 || n1 == 0 || n2 == 0 || n3 == 0) || (n0->leaf_flag && n1->leaf_flag && n2->leaf_flag && n3->leaf_flag))
		return;

	OctreeNode* c0 = !n3->leaf_flag ? CHECK_WORLD(n3)[5] : n3;
	OctreeNode* c1 = !n2->leaf_flag ? CHECK_WORLD(n2)[4] : n2;
	OctreeNode* c2 = !n2->leaf_flag ? CHECK_WORLD(n2)[7] : n2;
	OctreeNode* c3 = !n3->leaf_flag ? CHECK_WORLD(n3)[6] : n3;
	OctreeNode* c4 = !n0->leaf_flag ? CHECK_WORLD(n0)[1] : n0;
	OctreeNode* c5 = !n1->leaf_flag ? CHECK_WORLD(n1)[0] : n1;
	OctreeNode* c6 = !n1->leaf_flag ? CHECK_WORLD(n1)[3] : n1;
	OctreeNode* c7 = !n0->leaf_flag ? CHECK_WORLD(n0)[2] : n0;

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

void WorldStitcher::stitch_indexes(OctreeNode* nodes[8], SmartContainer<DualVertex>& v_out)
{
Start:
	for (int i = 0; i < 8; i++)
	{
		if (nodes[i] == 0 || (nodes[i]->leaf_flag && nodes[i]->world_node_flag))
			return;
	}

	for (int i = 0; i < 8; i++)
	{
		if (!nodes[i]->leaf_flag)
		{
			nodes[0] = !nodes[0]->leaf_flag ? CHECK_WORLD(nodes[0])[6] : nodes[0];
			nodes[1] = !nodes[1]->leaf_flag ? CHECK_WORLD(nodes[1])[7] : nodes[1];
			nodes[2] = !nodes[2]->leaf_flag ? CHECK_WORLD(nodes[2])[4] : nodes[2];
			nodes[3] = !nodes[3]->leaf_flag ? CHECK_WORLD(nodes[3])[5] : nodes[3];
			nodes[4] = !nodes[4]->leaf_flag ? CHECK_WORLD(nodes[4])[2] : nodes[4];
			nodes[5] = !nodes[5]->leaf_flag ? CHECK_WORLD(nodes[5])[3] : nodes[5];
			nodes[6] = !nodes[6]->leaf_flag ? CHECK_WORLD(nodes[6])[0] : nodes[6];
			nodes[7] = !nodes[7]->leaf_flag ? CHECK_WORLD(nodes[7])[1] : nodes[7];

			goto Start;
		}
	}

	OctreeNode* new_nodes[8] = { nodes[0], nodes[3], nodes[4], nodes[7], nodes[1], nodes[2], nodes[5], nodes[6] };

	using namespace glm;
	vec3 pos[8];
	float samples[8];
	int mask = 0;
	for (int i = 0; i < 8; i++)
	{
		float size = new_nodes[i]->size * 0.5f;
		pos[i] = new_nodes[i]->pos + vec3(size);
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

void WorldStitcher::stitch_leaf(DMCNode* n, emilib::HashMap<MortonCode, class DMCNode*>& leaves, SmartContainer<DualVertex>& v_out)
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

		uint64_t keys[8];
		DMCNode* final_nodes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		vert2leaf(v_codes[i], lv, keys);
		for (int j = 0; j < 8; j++)
		{
			uint64_t key_j = keys[j];
			DMCNode* node_at = leaves[key_j];
			if (!node_at)
				continue;
			if (!node_at->leaf_flag)
				goto next_vertex;
			//if (j < i)
			//	goto next_vertex;
		}

		for (int j = 0; j < 8; j++)
		{
			while (!final_nodes[j] && keys[j] != 0)
			{
				final_nodes[j] = leaves[keys[j]];
				keys[j] >>= 3;
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
	const uint64_t dil_x = 0b001001001001001001001001001001001001001001001001001001001;
	const uint64_t dil_y = 0b010010010010010010010010010010010010010010010010010010010;
	const uint64_t dil_z = 0b100100100100100100100100100100100100100100100100100100100;

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
	const uint64_t dil_x = 0b001001001001001001001001001001001001001001001001001001001;
	const uint64_t dil_y = 0b010010010010010010010010010010010010010010010010010010010;
	const uint64_t dil_z = 0b100100100100100100100100100100100100100100100100100100100;

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
	OctreeNode* new_nodes[8] = { nodes[7], nodes[3], nodes[5], nodes[1], nodes[6], nodes[2], nodes[4], nodes[0] };

	using namespace glm;
	vec3 pos[8];
	float samples[8];
	int mask = 0;
	for (int i = 0; i < 8; i++)
	{
		if (!new_nodes[i])
			return;
		float size = new_nodes[i]->size * 0.5f;
		pos[i] = new_nodes[i]->pos + vec3(size);
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

