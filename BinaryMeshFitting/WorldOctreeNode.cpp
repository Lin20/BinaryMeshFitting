#include "PCH.h"
#include "WorldOctreeNode.hpp"
#include "DMCChunk.hpp"
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
	stitch_flag = false;
	stitch_stored_flag = false;
	stitches = 0;
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
	stitch_flag = false;
	stitch_stored_flag = false;
	stitches = 0;
}

WorldOctreeNode::WorldOctreeNode(uint32_t _index) : WorldOctreeNode()
{
	world_node_flag = true;
	index = _index;
	gl_chunk = 0;
	force_chunk_octree = false;
	stitch_flag = false;
	stitch_stored_flag = false;
	stitches = 0;
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
	stitch_flag = false;
	stitch_stored_flag = false;
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
		gl_chunk->format_data(chunk->vi->vertices, chunk->vi->mesh_indexes, false, false);
		
	}

	return true;
}

bool WorldOctreeNode::upload()
{
	if (chunk && chunk->contains_mesh)
	{
		assert(gl_chunk);
		gl_chunk->init(true, true);
		return gl_chunk->set_data(gl_chunk->p_data, gl_chunk->c_data, &chunk->vi->mesh_indexes);
		//return gl_chunk->set_data(&chunk->vi->mesh_indexes, FLAT_QUADS);
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

DMCNode::DMCNode() : OctreeNode()
{
	world_node_flag = false;
	leaf_flag = true;
	sample = 0.0f;
}

DMCNode::DMCNode(DMCChunk* _chunk, float _size, glm::vec3 _pos, glm::ivec3 _xyz, uint8_t _level, uint32_t _int_size, float _sample) : OctreeNode(0, 0, _size, _pos, _level)
{
	world_node_flag = false;
	leaf_flag = true;
	root = _chunk;
	uint32_t dim = _chunk->dim;
	xyz = _xyz;
	i_size = _int_size;
	sample = _sample;
}
