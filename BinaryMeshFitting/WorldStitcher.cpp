#include "PCH.h"
#include "WorldStitcher.hpp"
#include "WorldOctreeNode.hpp"
#include "DefaultOptions.h"
#include "Tables.hpp"
#include "CubicChunk.hpp"

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
	queue.push_back(StitchOperation(STITCHING_OPERATIONS_CELL, 0, &start));

	while (!queue.empty())
	{
		StitchOperation next = queue.front();
		switch (next.op)
		{
		case STITCHING_OPERATIONS_CELL:
			stitch_cell(next.cells[0], vertices);
			break;
		case STITCHING_OPERATIONS_FACE:
			stitch_faces(next.cells, next.dir, vertices);
			break;
		case STITCHING_OPERATIONS_EDGE:
			stitch_edges(next.cells, next.dir, vertices);
			break;
		case STITCHING_OPERATIONS_INDEXES:
			stitch_indexes(next.cells, next.dir, vertices);
			break;
		}

		queue.pop_front();
	}
}

void WorldStitcher::upload()
{
	gl_chunk.set_data();
}

void WorldStitcher::format()
{
	gl_chunk.format_data(vertices, SMOOTH_NORMALS);
}

void WorldStitcher::stitch_cell(OctreeNode* n, SmartContainer<DualVertex>& v_out)
{
	if (n->is_leaf())
		return;

	if (n->is_world_node())
	{
		WorldOctreeNode* w = (WorldOctreeNode*)n;
		if (!w->force_chunk_octree)
		{
			for (int i = 0; i < 8; i++)
			{
				if (!n->children[i])
				{
					printf("No children\n");
					continue;
				}
				if (n->children[i]->is_world_node() && !n->children[i]->is_leaf())
				{
					//queue.push_back(StitchOperation(STITCHING_OPERATIONS_CELL, Tables::TCellProcFaceMask[i][2], &n->children[i]));
					stitch_cell(n->children[i], v_out);
				}
			}
		}

		for (int i = 0; i < 12; i++)
		{
			OctreeNode* face_nodes[2];
			int c1 = Tables::TCellProcFaceMask[i][0];
			int c2 = Tables::TCellProcFaceMask[i][1];

			face_nodes[0] = (w->force_chunk_octree ? w->chunk->octree.children[c1] : n->children[c1]);
			face_nodes[1] = (w->force_chunk_octree ? w->chunk->octree.children[c1] : n->children[c2]);
			if (!face_nodes[0] || !face_nodes[1])
				continue;
			if (face_nodes[0]->is_world_node() && face_nodes[1]->is_world_node())
			{
				//if (!face_nodes[0]->is_leaf() && !face_nodes[1]->is_leaf())
				{
					//queue.push_back(StitchOperation(STITCHING_OPERATIONS_FACE, Tables::TCellProcFaceMask[i][2], face_nodes));
				stitch_faces(face_nodes, Tables::TCellProcFaceMask[i][2], v_out);
				}
			}
		}

		for (int i = 0; i < 6; i++)
		{
			OctreeNode* edge_nodes[4] =
			{
				(w->force_chunk_octree ? w->chunk->octree.children : n->children)[Tables::TCellProcEdgeMask[i][0]],
				(w->force_chunk_octree ? w->chunk->octree.children : n->children)[Tables::TCellProcEdgeMask[i][1]],
				(w->force_chunk_octree ? w->chunk->octree.children : n->children)[Tables::TCellProcEdgeMask[i][2]],
				(w->force_chunk_octree ? w->chunk->octree.children : n->children)[Tables::TCellProcEdgeMask[i][3]]
			};

			if (!edge_nodes[0] || !edge_nodes[1] || !edge_nodes[2] || !edge_nodes[3])
				continue;
			if (edge_nodes[0]->is_world_node() && edge_nodes[1]->is_world_node() && edge_nodes[2]->is_world_node() && edge_nodes[3]->is_world_node())
			{
				//if (!edge_nodes[0]->is_leaf() && !edge_nodes[1]->is_leaf() && !edge_nodes[2]->is_leaf() && !edge_nodes[3]->is_leaf())
				{
					//queue.push_back(StitchOperation(STITCHING_OPERATIONS_EDGE, Tables::TCellProcEdgeMask[i][4], edge_nodes));
				stitch_edges(edge_nodes, Tables::TCellProcEdgeMask[i][4], v_out);
				}
			}
		}
	}
}

void WorldStitcher::stitch_faces(OctreeNode* nodes[2], int direction, SmartContainer<DualVertex>& v_out)
{
	if (!nodes[0]->is_leaf() || !nodes[1]->is_leaf())
	{
		for (int i = 0; i < 4; i++)
		{
			OctreeNode* face_nodes[2];

			int c[2] = { Tables::TFaceProcFaceMask[direction][i][0], Tables::TFaceProcFaceMask[direction][i][1] };
			for (int j = 0; j < 2; j++)
			{
				if (nodes[j]->is_leaf())
					face_nodes[j] = nodes[j];
				else
				{
					if (!nodes[j]->is_world_node())
						face_nodes[j] = nodes[j]->children[c[j]];
					else
					{
						WorldOctreeNode* w = (WorldOctreeNode*)nodes[j];
						face_nodes[j] = (w->force_chunk_octree ? w->chunk->octree.children[c[j]] : w->children[c[j]]);
					}
				}
			}

			//if (!face_nodes[0] || !face_nodes[1] || (face_nodes[0]->is_leaf() && face_nodes[0]->is_world_node()) || (face_nodes[1]->is_leaf() && face_nodes[1]->is_world_node()))
			//	continue;
			//queue.push_back(StitchOperation(STITCHING_OPERATIONS_FACE, Tables::TFaceProcFaceMask[direction][i][2], face_nodes));
			stitch_faces(face_nodes, Tables::TFaceProcFaceMask[direction][i][2], v_out);
		}

		const int orders[][4] =
		{
			{ 0, 0, 1, 1 },
		{ 0, 1, 0, 1 }
		};

		for (int i = 0; i < 4; i++)
		{
			OctreeNode* edge_nodes[4];
			const int c[4] =
			{
				Tables::TFaceProcEdgeMask[direction][i][1],
				Tables::TFaceProcEdgeMask[direction][i][2],
				Tables::TFaceProcEdgeMask[direction][i][3],
				Tables::TFaceProcEdgeMask[direction][i][4]
			};

			const int* order = orders[Tables::TFaceProcEdgeMask[direction][i][0]];
			for (int j = 0; j < 4; j++)
			{
				if (nodes[order[j]]->is_leaf())
					edge_nodes[j] = nodes[order[j]];
				else
				{
					if (!nodes[order[j]]->is_world_node())
						edge_nodes[j] = nodes[order[j]]->children[c[j]];
					else
					{
						WorldOctreeNode* w = (WorldOctreeNode*)nodes[order[j]];
						edge_nodes[j] = (w->force_chunk_octree ? w->chunk->octree.children[c[j]] : w->children[c[j]]);
					}
				}
			}

			//if (!edge_nodes[0] || !edge_nodes[1] || !edge_nodes[2] || !edge_nodes[3] || (edge_nodes[0]->is_leaf() && edge_nodes[0]->is_world_node()) || (edge_nodes[1]->is_leaf() && edge_nodes[1]->is_world_node()) || (edge_nodes[2]->is_leaf() && edge_nodes[2]->is_world_node()) || (edge_nodes[3]->is_leaf() && edge_nodes[3]->is_world_node()))
			//	continue;
			//queue.push_back(StitchOperation(STITCHING_OPERATIONS_EDGE, Tables::TFaceProcEdgeMask[direction][i][5], edge_nodes));
			stitch_edges(edge_nodes, Tables::TFaceProcEdgeMask[direction][i][5], v_out);
		}
	}
}

void WorldStitcher::stitch_edges(OctreeNode* nodes[4], int direction, SmartContainer<DualVertex>& v_out)
{
	if (nodes[0]->is_leaf() && nodes[1]->is_leaf() && nodes[2]->is_leaf() && nodes[3]->is_leaf())
	{
		//queue.push_back(StitchOperation(STITCHING_OPERATIONS_INDEXES, direction, nodes));
		stitch_indexes(nodes, direction, v_out);
	}
	else
	{
		for (int i = 0; i < 2; i++)
		{
			OctreeNode* edge_nodes[4];
			for (int j = 0; j < 4; j++)
			{
				if (nodes[j]->is_leaf())
					edge_nodes[j] = nodes[j];
				else
				{
					if (!nodes[j]->is_world_node())
						edge_nodes[j] = nodes[j]->children[Tables::TEdgeProcEdgeMask[direction][i][j]];
					else
					{
						WorldOctreeNode* w = (WorldOctreeNode*)nodes[j];
						edge_nodes[j] = (w->force_chunk_octree ? w->chunk->octree.children[Tables::TEdgeProcEdgeMask[direction][i][j]] : w->children[Tables::TEdgeProcEdgeMask[direction][i][j]]);
					}
				}
			}

			if (edge_nodes[0]->is_leaf() && edge_nodes[1]->is_leaf() && edge_nodes[2]->is_leaf() && edge_nodes[3]->is_leaf())
			{
				DualNode* d_nodes[4] = { (DualNode*)edge_nodes[0], (DualNode*)edge_nodes[1], (DualNode*)edge_nodes[2], (DualNode*)edge_nodes[3] };
				if (d_nodes[0]->root == d_nodes[1]->root && d_nodes[1]->root == d_nodes[2]->root && d_nodes[2]->root == d_nodes[3]->root)
					continue;
				//queue.push_back(StitchOperation(STITCHING_OPERATIONS_INDEXES, Tables::TEdgeProcEdgeMask[direction][i][4], edge_nodes));
				stitch_indexes(edge_nodes, direction, v_out);
			}
			else
			//	queue.push_back(StitchOperation(STITCHING_OPERATIONS_EDGE, Tables::TEdgeProcEdgeMask[direction][i][4], edge_nodes));
			stitch_edges(edge_nodes, Tables::TEdgeProcEdgeMask[direction][i][4], v_out);
		}
	}
}

void WorldStitcher::stitch_indexes(OctreeNode* nodes[4], int direction, SmartContainer<DualVertex>& v_out)
{
	float min_size = 1e20f;
	int min_index = 0;
	bool flip = false;
	bool sign_changed = false;

	DualNode* d_nodes[4] = { (DualNode*)nodes[0], (DualNode*)nodes[1], (DualNode*)nodes[2], (DualNode*)nodes[3] };

	//if (d_nodes[0]->root == d_nodes[1]->root && d_nodes[1]->root == d_nodes[2]->root && d_nodes[2]->root == d_nodes[3]->root)
	//	return;

	DualVertex inds[4];

	for (int i = 0; i < 4; i++)
	{
		DualNode* n = d_nodes[i];
		if (n->index == 329)
		{
			//return;
		}

		int edge = Tables::TProcessEdgeMask[direction][i];
		int c1 = Tables::TEdgePairs[edge][0];
		int c2 = Tables::TEdgePairs[edge][1];

		int m1 = (n->cell.mask >> c1) & 1;
		int m2 = (n->cell.mask >> c2) & 1;

		if (n->size < min_size)
		{
			min_size = n->size;
			min_index = i;
			flip = m1 == 1;
			sign_changed = (n->cell.edge_mask >> edge) & 1; //((m1 == 0 && m2 != 0) || (m1 != 0 && m2 == 0));
		}
		else if (n->size == min_size && n != d_nodes[i-1])
		{
			/*assert(((n->cell.edge_mask >> edge) & 1) == sign_changed);
			if (((n->cell.edge_mask >> edge) & 1) != sign_changed)
			{
				return;
			}*/
		}
	}

	if (!sign_changed)
		return;

	int indx = 0;
	bool connected[4] = { false, false, false, false };

	for (int i = 0; i < 4; i++)
	{
		DualNode* n = d_nodes[i];
		int edge = Tables::TProcessEdgeMask[direction][i];
		bool crossing = (n->cell.edge_mask >> edge) & 1;
		/*if (!crossing)
		{
		inds[i] = -1;
		continue;
		}*/

		uint32_t ind = n->cell.v_map[(n->cell.edge_map >> (2 * edge)) & 3];
		//if ((int)ind == -1)
		//	ind = n->cell.v_map[0];
		if ((int)ind >= 0)
		{
			if (ind < 0x40000000)
				inds[i] = n->root->vi->vertices[ind + n->root->mesh_offset];
			else
				inds[i] = n->root->vi->vertices[ind & 0x3FFFFFFF];
			connected[i] = true;
		}
		else
		{
			connected[i] = false;
			int new_index = -1;
			for (int k = 0; k < 4; k++)
			{
				if ((int)n->cell.v_map[k] < 0)
				{
					new_index = k;
					break;
				}
			}

			if (new_index != 0)
				printf("Index not zero.\n");

			glm::vec3 new_v_pos(0, 0, 0);
			if (n->i_size > 2)
			{
				new_v_pos = d_nodes[min_index]->pos;
			}

			DualVertex temp;
			n->root->calculate_dual_vertex(n->cell.xyz, n->root->vi->vertices.count | 0x40000000, &temp, n->i_size > 2, 0, new_v_pos);
			n->cell.v_map[new_index] = temp.index;
			inds[i] = temp;
			n->root->vi->vertices.push_back(temp);

			if (((n->cell.edge_mask >> edge) & 1) == 0)
				n->cell.edge_map |= new_index << (2 * edge);
			n->cell.edge_mask |= 1 << edge;
		}

	}

	if (QUADS)
	{
		/*v_out[inds[0]].init_valence += 1;
		v_out[inds[1]].init_valence += 1;
		v_out[inds[2]].init_valence += 1;
		v_out[inds[3]].init_valence += 1;*/
		if (!flip)
		{
			v_out.push_back(inds[0]);
			v_out.push_back(inds[1]);
			v_out.push_back(inds[3]);
			v_out.push_back(inds[2]);
		}
		else
		{
			v_out.push_back(inds[3]);
			v_out.push_back(inds[1]);
			v_out.push_back(inds[0]);
			v_out.push_back(inds[2]);
		}
	}
	else
	{
		/*if (!flip)
		{
			if (inds[0] != inds[1] && inds[1] != inds[3] && inds[3] != inds[0] && inds[0] != -1 && inds[1] != -1 && inds[3] != -1)
			{
				i_out.push_back(inds[0]);
				i_out.push_back(inds[1]);
				i_out.push_back(inds[3]);
				v_out[inds[0]].init_valence++;
				v_out[inds[1]].init_valence++;
				v_out[inds[3]].init_valence++;
			}

			if (inds[3] != inds[2] && inds[2] != inds[0] && inds[0] != inds[3] && inds[3] != -1 && inds[2] != -1 && inds[0] != -1)
			{
				i_out.push_back(inds[3]);
				i_out.push_back(inds[2]);
				i_out.push_back(inds[0]);
				v_out[inds[3]].init_valence++;
				v_out[inds[2]].init_valence++;
				v_out[inds[0]].init_valence++;
			}
		}
		else
		{
			if (inds[0] != inds[1] && inds[1] != inds[3] && inds[3] != inds[0] && inds[0] != -1 && inds[1] != -1 && inds[3] != -1)
			{
				i_out.push_back(inds[3]);
				i_out.push_back(inds[1]);
				i_out.push_back(inds[0]);
				v_out[inds[3]].init_valence++;
				v_out[inds[1]].init_valence++;
				v_out[inds[0]].init_valence++;
			}

			if (inds[3] != inds[2] && inds[2] != inds[0] && inds[0] != inds[3] && inds[3] != -1 && inds[2] != -1 && inds[0] != -1)
			{
				i_out.push_back(inds[0]);
				i_out.push_back(inds[2]);
				i_out.push_back(inds[3]);
				v_out[inds[0]].init_valence++;
				v_out[inds[2]].init_valence++;
				v_out[inds[3]].init_valence++;
			}
		}*/
	}
}
