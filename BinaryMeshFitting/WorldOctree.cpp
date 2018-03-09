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
	NoiseSamplers::create_sampler_terrain_pert_3d(&sampler);
	sampler.world_size = 256;
	focus_point = glm::vec3(-5.88f, 357.70f, -465.41f);
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

void destroy_world_nodes(MemoryPool<WorldOctreeNode, 65536>* pool, MemoryPool<CubicChunk, 65536>* chunk_pool, WorldOctreeNode* n)
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
	new (&chunk_pool) MemoryPool<CubicChunk, 65536>();
	node_pool.~MemoryPool();
	new (&node_pool) MemoryPool<WorldOctreeNode, 65536>();
}

void WorldOctree::init(uint32_t size)
{
	size *= 2;
	glm::vec3 pos = glm::vec3(1, 1, 1) * (float)size * -0.5f;

	// Use placement new to initialize octree because atomic has no copy constructor
	octree.~WorldOctreeNode();
	new(&octree) WorldOctreeNode(0, 0, (float)size, pos, 0);
	octree.flags = NODE_FLAGS_DIRTY | NODE_FLAGS_DRAW;
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
		glm::vec3 c_pos(n->pos.x + (float)Tables::TDX[i] * c_size, n->pos.y + (float)Tables::TDY[i] * c_size, n->pos.z + (float)Tables::TDZ[i] * c_size);
		WorldOctreeNode* c = node_pool.newElement(0, n, n->size * 0.5f, c_pos, n->level + 1);
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
	n->chunk->init(n->pos, n->size, n->level, sampler, QUADS);
	n->chunk->dim = properties.chunk_resolution;
	n->chunk->id = next_chunk_id++;
}

double WorldOctree::extract_all()
{
	using namespace std;

	if (properties.num_threads == 0)
		properties.num_threads = std::thread::hardware_concurrency();
	omp_set_num_threads(properties.num_threads);

	size_t cell_count = 0;
	uint32_t res = 0;
	SmartContainer<WorldOctreeNode*> batch;
	batch.resize(leaves.size());
	for (auto& n : leaves)
	{
		//if (n->chunk->id != 198/* && n->chunk->id != 201*/)
		//	continue;
		batch.push_back(n);
		cell_count += n->chunk->dim * n->chunk->dim * n->chunk->dim;
		res = (n->chunk->dim > res ? n->chunk->dim : res);
	}

	cout << "Extracting batch:" << endl;
	cout << "\t-Nodes: " << batch.count << endl;
	cout << "\t-Res: " << res << endl;
	cout << "\t-Cells: " << cell_count << " cells)..." << endl;
	cout << "\t-Perc: " << ((float)cell_count / (float)(255 * 255 * 255)) << "x 255^3" << endl;
	clock_t start_clock = clock();

	extract_samples(batch);
	extract_dual_vertices(batch);
	extract_octrees(batch);
	extract_base_meshes(batch);
	extract_copy_vis(batch);
	//extract_stitches(batch);

	double ms = clock() - start_clock;
	cout << endl << "Complete in " << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << " ms. " << v_out.count << " verts, " << (i_out.count / (QUADS ? 4 : 3)) << " prims." << endl << endl;

	//generate_outline(batch);

	return ms;
}

double WorldOctree::color_all()
{
	using namespace std;

	cout << "Coloring...";
	clock_t start_clock = clock();

	color_mapper.generate_colors(v_out);

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
	return elapsed;
}

double WorldOctree::process_all()
{
	using namespace std;

	cout << "Processing...";
	clock_t start_clock = clock();

	int iters = properties.process_iters;
	if (iters > 0)
	{
		Processing::MeshProcessor<(QUADS ? 4 : 3)> mp(true, true);
		mp.init(v_out, i_out, sampler);
		mp.collapse_bad_quads();
		if (!QUADS && false)
		{
			v_out.count = 0;
			i_out.count = 0;
			mp.flush_to_tris(v_out, i_out);
			Processing::MeshProcessor<3> nmp = Processing::MeshProcessor<3>(true, true);
			if (iters > 0)
			{
				nmp.init(v_out, i_out, sampler);
				nmp.optimize_dual_grid(iters);
				nmp.optimize_primal_grid(false, iters == 1);
				v_out.count = 0;
				i_out.count = 0;
				nmp.flush(v_out, i_out);
			}
		}
		else
		{
			mp.optimize_dual_grid(iters);
			mp.optimize_primal_grid(false, iters == 1);
			v_out.count = 0;
			i_out.count = 0;
			mp.flush(v_out, i_out);
		}
	}

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
	return elapsed;
}

double WorldOctree::upload_all()
{
	using namespace std;

	cout << "Uploading...";
	clock_t start_clock = clock();

	SmartContainer<WorldOctreeNode*> batch;
	batch.resize(leaves.size());

	for (auto& n : leaves)
	{
		batch.push_back(n);
	}
	upload_batch(batch);

	double elapsed = clock() - start_clock;
	cout << "done (" << (int)(elapsed / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
	return elapsed;
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

CubicChunk* WorldOctree::get_chunk_id_at(glm::vec3 p)
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

void WorldOctree::extract_samples(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Generating samples...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		//batch[i]->chunk->generate_samples(0, 0);
	}

	/*for (auto& n : leaves)
	{
		n->chunk->generate_samples();
	}*/
	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_filter(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Filtering data...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < count; i++)
	{
		//batch[i]->chunk->filter();
	}

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_dual_vertices(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Calculating dual vertices...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			//batch[i]->chunk->generate_dual_vertices();
		}
	}

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_octrees(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Generating octrees...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			//batch[i]->chunk->generate_octree();
			if (!batch[i]->chunk->octree.is_leaf())
			{
				memcpy(batch[i]->children, batch[i]->chunk->octree.children, sizeof(OctreeNode*) * 8);
				batch[i]->leaf_flag = false;
			}
			else
				cout << "WARNING: Octree is leaf." << endl;
		}
	}

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_base_meshes(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Generating base meshes...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
#pragma omp parallel
	{
#pragma omp for
		for (int i = 0; i < count; i++)
		{
			//batch[i]->chunk->generate_base_mesh();
		}
	}

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_copy_vis(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Copying verts and inds...";
	clock_t start_clock = clock();

	int count = (int)batch.count;
	for (int i = 0; i < count; i++)
	{
		//batch[i]->chunk->copy_verts_and_inds(v_out, i_out);
		//batch[i]->chunk->dirty = false;
	}

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::extract_stitches(SmartContainer<WorldOctreeNode*>& batch)
{
	using namespace std;
	cout << "-Stitching...";
	clock_t start_clock = clock();

	stitch_cell(&octree, v_out, i_out);

	double ms = clock() - start_clock;
	cout << "done (" << (int)(ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;
}

void WorldOctree::stitch_cell(OctreeNode* n, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	if (n->is_leaf())
		return;

	if (n->is_world_node())
	{
		for (int i = 0; i < 8; i++)
		{
			if (!n->children[i])
			{
				printf("No children\n");
				continue;
			}
			if (n->children[i]->is_world_node())
			{
				stitch_cell(n->children[i], v_out, i_out);
			}
		}
		for (int i = 0; i < 12; i++)
		{
			OctreeNode* face_nodes[2];
			int c1 = Tables::TCellProcFaceMask[i][0];
			int c2 = Tables::TCellProcFaceMask[i][1];

			face_nodes[0] = n->children[c1];
			face_nodes[1] = n->children[c2];
			if (face_nodes[0]->is_world_node() && face_nodes[1]->is_world_node())

				stitch_faces(face_nodes, Tables::TCellProcFaceMask[i][2], v_out, i_out);
		}

		for (int i = 0; i < 6; i++)
		{
			OctreeNode* edge_nodes[4] =
			{
				n->children[Tables::TCellProcEdgeMask[i][0]],
				n->children[Tables::TCellProcEdgeMask[i][1]],
				n->children[Tables::TCellProcEdgeMask[i][2]],
				n->children[Tables::TCellProcEdgeMask[i][3]]
			};

			if (edge_nodes[0]->is_world_node() && edge_nodes[1]->is_world_node() && edge_nodes[2]->is_world_node() && edge_nodes[3]->is_world_node())
				stitch_edges(edge_nodes, Tables::TCellProcEdgeMask[i][4], v_out, i_out);
		}
	}
}

void WorldOctree::stitch_faces(OctreeNode* nodes[2], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	if (!nodes[0] || !nodes[1] || (nodes[0]->is_leaf() && nodes[0]->is_world_node()) || (nodes[1]->is_leaf() && nodes[1]->is_world_node()))
	{
		//printf("Returned!");
		return;
	}

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
					face_nodes[j] = nodes[j]->children[c[j]];
			}

			stitch_faces(face_nodes, Tables::TFaceProcFaceMask[direction][i][2], v_out, i_out);
		}

		const int orders[][4] =
		{
			{ 0, 0, 1, 1},
			{ 0, 1, 0, 1}
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
					edge_nodes[j] = nodes[order[j]]->children[c[j]];
			}

			stitch_edges(edge_nodes, Tables::TFaceProcEdgeMask[direction][i][5], v_out, i_out);
		}
	}
}

void WorldOctree::stitch_edges(OctreeNode* nodes[4], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3] || (nodes[0]->is_leaf() && nodes[0]->is_world_node()) || (nodes[1]->is_leaf() && nodes[1]->is_world_node()) || (nodes[2]->is_leaf() && nodes[2]->is_world_node()) || (nodes[3]->is_leaf() && nodes[3]->is_world_node()))
	{
		//printf("Returned!");
		return;
	}

	if (nodes[0]->is_leaf() && nodes[1]->is_leaf() && nodes[2]->is_leaf() && nodes[3]->is_leaf())
	{
		stitch_indexes(nodes, direction, v_out, i_out);
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
					edge_nodes[j] = nodes[j]->children[Tables::TEdgeProcEdgeMask[direction][i][j]];
			}

			stitch_edges(edge_nodes, Tables::TEdgeProcEdgeMask[direction][i][4], v_out, i_out);
		}
	}
}

void WorldOctree::stitch_indexes(OctreeNode* nodes[4], int direction, SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	float min_size = 1e20f;
	int min_index = 0;
	bool flip = false;
	bool sign_changed = false;

	DualNode* d_nodes[4] = { (DualNode*)nodes[0], (DualNode*)nodes[1], (DualNode*)nodes[2], (DualNode*)nodes[3] };

	if (d_nodes[0]->root == d_nodes[1]->root && d_nodes[1]->root == d_nodes[2]->root && d_nodes[2]->root == d_nodes[3]->root)
		return;

	uint32_t inds[4];

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

		if (d_nodes[i]->size < min_size)
		{
			min_size = d_nodes[i]->size;
			min_index = i;
			flip = m1 == 1;
			sign_changed = (n->cell.edge_mask >> edge) & 1; //((m1 == 0 && m2 != 0) || (m1 != 0 && m2 == 0));
		}
	}

	if (!sign_changed)
		return;


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
				inds[i] = ind + n->root->mesh_offset;
			else
				inds[i] = ind & 0x3FFFFFFF;
		}
		else
		{
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
			n->root->calculate_dual_vertex(n->cell.xyz, v_out.count | 0x40000000, &temp, n->i_size > 2, 0, new_v_pos);
			n->cell.v_map[new_index] = temp.index;
			inds[i] = v_out.count;
			v_out.push_back(temp);

			if (((n->cell.edge_mask >> edge) & 1) == 0)
				n->cell.edge_map |= new_index << (2 * edge);
			n->cell.edge_mask |= 1 << edge;
		}
	}

	uint32_t fill_index = (inds[0] != -1 ? inds[0] : inds[1] != -1 ? inds[1] : inds[3]);
	if (inds[0] == -1)
		inds[0] = fill_index;
	if (inds[1] == -1)
		inds[1] = fill_index;
	if (inds[3] == -1)
		inds[3] = fill_index;

	if (inds[2] == -1)
		inds[2] = (inds[1] != -1 ? inds[1] : inds[3] != -1 ? inds[3] : inds[2]);

	if (inds[0] == -1 || inds[1] == -1 || inds[2] == -1 || inds[3] == -1)
	{
		printf("Detected -1\n");
	}

	if (QUADS)
	{
		v_out[inds[0]].init_valence += 1;
		v_out[inds[1]].init_valence += 1;
		v_out[inds[2]].init_valence += 1;
		v_out[inds[3]].init_valence += 1;
		if (!flip)
		{
			i_out.push_back(inds[0]);
			i_out.push_back(inds[1]);
			i_out.push_back(inds[3]);
			i_out.push_back(inds[2]);
		}
		else
		{
			i_out.push_back(inds[3]);
			i_out.push_back(inds[1]);
			i_out.push_back(inds[0]);
			i_out.push_back(inds[2]);
		}
	}
	else
	{
		if (!flip)
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
		}
	}
}

void WorldOctree::init_updates(glm::vec3 focus_pos)
{
	watcher.init(this, focus_pos);
}

void WorldOctree::update(glm::vec3 pos)
{
}

void WorldOctree::process_from_render_thread()
{
	// Renderables mutex is already locked from the main render loop

	const int MAX_DELETES = 1500;
	const int MAX_UPLOADS = 400;

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
			n->generation_stage = GENERATION_STAGES_DONE;
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

void WorldOctree::update_leaves()
{
}
