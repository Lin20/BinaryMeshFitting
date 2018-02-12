#include "PCH.h"
#include "WorldOctreeNode.hpp"
#include "CubicChunk.hpp"
#include "Tables.hpp"
#include "DefaultOptions.h"
#include "ResourceAllocator.hpp"

WorldOctreeNode::WorldOctreeNode() : OctreeNode()
{
	world_node_flag = true;
	chunk = 0;
	leaf_flag = true;
	world_leaf_flag = true;
	flags = 0;
	renderable_prev = 0;
	renderable_next = 0;
	gl_chunk = 0;
}

WorldOctreeNode::WorldOctreeNode(uint32_t _index, WorldOctreeNode* _parent, float _size, glm::vec3 _pos, uint8_t _level) : OctreeNode(_index, _parent, _size, _pos, _level)
{
	world_node_flag = true;
	chunk = 0;
	middle = pos + size * 0.5f;
	leaf_flag = true;
	world_leaf_flag = true;
	flags = 0;
	generation_stage = 0;
	renderable_prev = 0;
	renderable_next = 0;
	gl_chunk = 0;
	force_chunk_octree = false;
}

WorldOctreeNode::WorldOctreeNode(uint32_t _index) : WorldOctreeNode()
{
	world_node_flag = true;
	index = _index;
	gl_chunk = 0;
	force_chunk_octree = false;
}

WorldOctreeNode::~WorldOctreeNode()
{
	unlink();
}

void WorldOctreeNode::init(uint32_t _index, WorldOctreeNode* _parent, float _size, glm::vec3 _pos, uint8_t _level)
{
	chunk = 0;
	middle = pos + size * 0.5f;
	leaf_flag = true;
	world_leaf_flag = true;
	flags = 0;
}

bool WorldOctreeNode::format(ResourceAllocator<GLChunk>* allocator)
{
	assert(allocator);
	
	if (chunk && chunk->contains_mesh)
	{
		assert(chunk->vi);
		if (!gl_chunk)
		{
			gl_chunk = allocator->new_element();
			if (!gl_chunk)
				return false;
		}
		gl_chunk->format_data(chunk->vi->vertices, chunk->vi->mesh_indexes, FLAT_QUADS, SMOOTH_NORMALS);
		
	}

	return true;
}

bool WorldOctreeNode::upload()
{
	if (chunk && chunk->contains_mesh)
	{
		assert(gl_chunk);
		gl_chunk->init(true, true);
		return gl_chunk->set_data(&chunk->vi->mesh_indexes, FLAT_QUADS);
	}

	return true;
}

void WorldOctreeNode::unlink()
{
	if (renderable_prev)
	{
		renderable_prev->renderable_next = renderable_next;
	}
	if (renderable_next)
	{
		renderable_next->renderable_prev = renderable_prev;
	}
	renderable_next = 0;
	renderable_prev = 0;
}

DualNode::DualNode() : OctreeNode()
{
	world_node_flag = false;
	leaf_flag = true;
	cell.mask = 0;
	cell.edge_mask = 0;
	cell.xyz = glm::ivec3(0, 0, 0);
	cell.edge_map = 0;
	cell.v_map[0] = -1;
	cell.v_map[1] = -1;
	cell.v_map[2] = -1;
	cell.v_map[3] = -1;
}

DualNode::DualNode(CubicChunk* _chunk, uint32_t _index, float _size, glm::vec3 _pos, glm::ivec3 _xyz, uint8_t _level, uint32_t _int_size, Cell* _cell) : OctreeNode(_index, 0, _size, _pos, _level)
{
	world_node_flag = false;
	leaf_flag = true;
	root = _chunk;
	uint32_t dim = _chunk->dim;
	xyz = _xyz;
	i_size = _int_size;

	if (_int_size == 1 && _xyz.x < dim && _xyz.y < dim && _xyz.z < dim)
	{
		if (_cell != nullptr)
			cell = *_cell;
		else if (_chunk->inds_block)
		{
			uint32_t ind = _chunk->inds_block->inds[_xyz.x * dim * dim + _xyz.y * dim + _xyz.z];
			if ((int32_t)ind >= 0)
				cell = _chunk->cell_block->cells[ind];
			else
				goto DefaultCell;
		}
		else
			goto DefaultCell;
		return;
	}
	/*else if(dim % 2 != 0 && _int_size == 1)
	{
		if (_xyz.x >= dim)
			_xyz.x--;
		if (_xyz.y >= dim)
			_xyz.y--;
		if (_xyz.z >= dim)
			_xyz.z--;
		uint32_t ind = _chunk->inds[_xyz.x * dim * dim + _xyz.y * dim + _xyz.z];
		if (_int_size == 1 && (int32_t)ind >= 0)
		{
			cell = _chunk->cells[ind];
			cell.xyz = _xyz;
		}
		else
			goto DefaultCell;
		return;
	}*/

DefaultCell:
	/*uint8_t mask = 0;
	uint32_t z_per_y = (dim + 32) / 32;
	uint32_t y_per_x = z_per_y * (dim + 1);
	for (int i = 0; i < 8; i++)
	{
		glm::ivec3 corner_pos = _xyz + glm::ivec3(Tables::TDX[i], Tables::TDY[i], Tables::TDZ[i]) * (int)i_size;
		uint32_t s = root->samples[corner_pos.x * y_per_x + corner_pos.y * z_per_y + corner_pos.z / 32];
		int sign = (s >> (corner_pos.z % 32)) & 1;
		if (sign)
			mask |= 1 << i;
	}

	if (mask != 0 && mask != 255)
	{
		root->calculate_cell(_xyz, root->cells.count, &cell, false, mask);
	}
	else
	{*/
		cell.mask = /*mask*/0;
		cell.edge_mask = 0;
		cell.xyz = _xyz;
		cell.edge_map = 0;
		cell.v_map[0] = -1;
		cell.v_map[1] = -1;
		cell.v_map[2] = -1;
		cell.v_map[3] = -1;
	//}
}

void OctreeNode::generate_outline(SmartContainer<glm::vec3>& v_pos, SmartContainer<uint32_t>& inds)
{
	if (is_leaf())
	{
		using namespace glm;
		uint32_t is[8];
		for (int i = 0; i < 8; i++)
		{
			is[i] = (uint32_t)v_pos.count;
			vec3 p = pos + vec3((float)Tables::TDX[i], (float)Tables::TDY[i], (float)Tables::TDZ[i]) * size;
			v_pos.push_back(p);
		}

		for (int i = 0; i < 12; i++)
		{
			inds.push_back(is[Tables::TEdgePairs[i][0]]);
			inds.push_back(is[Tables::TEdgePairs[i][1]]);
		}
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			if (children[i])
				children[i]->generate_outline(v_pos, inds);
		}
	}
}

OctreeNode* OctreeNode::get_node_at(glm::vec3 p)
{
	if (is_leaf())
	{
		return this;
	}

	float s2 = size * 0.5f;
	for (int i = 0; i < 8; i++)
	{
		if (!children[i])
			continue;
		if (p.x >= children[i]->pos.x && p.y >= children[i]->pos.y && p.z >= children[i]->pos.z && p.x < children[i]->pos.x + children[i]->size && p.y < children[i]->pos.y + children[i]->size && p.z < children[i]->pos.z + children[i]->size)
		{
			return children[i]->get_node_at(p);
		}
	}

	return 0;
}
