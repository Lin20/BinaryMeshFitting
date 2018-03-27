#include "PCH.h"
#include "DynamicGLChunk.hpp"

#define DEFAULT_V_COUNT 786432

using namespace glm;

DynamicGLChunk::DynamicGLChunk()
{
	initialized = false;
	normals = true;

	vao = 0;
	v_vbo = 0;
	n_vbo = 0;

	v_count = 0;
	p_count = 0;
	vbo_size = 0;
}

DynamicGLChunk::~DynamicGLChunk()
{
	destroy();
}

void DynamicGLChunk::init(bool _normals, bool _colors)
{
	if (initialized)
		return;

	normals = _normals;
	colors = _colors;

	p_data.prepare(DEFAULT_V_COUNT);
	c_data.prepare(DEFAULT_V_COUNT);

	glGenBuffers(1, &v_vbo);
	if (normals)
		glGenBuffers(1, &n_vbo);
	if (colors)
		glGenBuffers(1, &c_vbo);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	initialized = true;

	VertexRegion* base = region_pool.newElement(0, DEFAULT_V_COUNT);
	free_regions.push_back(base);
	reset(base);
}

void DynamicGLChunk::destroy()
{
	if (initialized)
	{
		glDeleteVertexArrays(1, &vao);
		int v_count = 1;
		if (normals)
			v_count++;
		if (colors)
			v_count++;
		glDeleteBuffers(v_count, &v_vbo);
	}
	initialized = 0;
}

VertexRegion* DynamicGLChunk::allocate(uint32_t count)
{
	if (!count)
		return 0;

	LinkedNode<VertexRegion>* next = free_regions.head;
	LinkedNode<VertexRegion>* smallest_node = 0;

	while (next)
	{
		VertexRegion* region = (VertexRegion*)next;
		if (region->count >= count)
		{
			if (!smallest_node)
				smallest_node = next;
			else if (((VertexRegion*)smallest_node)->count > region->count)
				smallest_node = region;
		}
		next = next->next;
	}

	if (!smallest_node)
		return 0;

	smallest_node->unlink();
	used_regions.push_back(smallest_node);
	VertexRegion* dest = (VertexRegion*)smallest_node;
	uint32_t leftover = dest->count - count;

	if (leftover > 0)
	{
		VertexRegion* remaining = region_pool.newElement(dest->start + dest->count, leftover);
		free_regions.push_back(remaining);
		dest->count = count;
	}

	return (VertexRegion*)smallest_node;
}

void DynamicGLChunk::free(VertexRegion* r)
{
	r->unlink();
	free_regions.push_back(r);
}

void DynamicGLChunk::reset(VertexRegion* r, uint32_t offset)
{
	if (!r || !r->count)
		return;

	uint32_t end = r->start + r->count;

	for (uint32_t i = r->start + offset; i < end; i++)
		p_data[i].x = p_data[i].y = p_data[i].z = 0;

	if (normals)
	{
		for (uint32_t i = r->start + offset; i < end; i++)
			n_data[i].x = p_data[i].y = p_data[i].z = 0;
	}

	if (colors)
	{
		for (uint32_t i = r->start + offset; i < end; i++)
			c_data[i].x = p_data[i].y = p_data[i].z = 0;
	}
}

void DynamicGLChunk::upload(VertexRegion* r)
{
	assert(initialized);
	if (!r)
		return;

	glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * p_data.count, p_data.elements);

	if (normals)
	{
		glBindBuffer(GL_ARRAY_BUFFER, n_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * n_data.count, n_data.elements);
	}

	if (colors)
	{
		glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * n_data.count, n_data.elements);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	r->marked_dirty = false;
}

void DynamicGLChunk::upload_dirty_regions()
{
	if (!dirty_regions.count)
		return;

	int count = (int)dirty_regions.count;

	glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
	for (int i = 0; i < count; i++)
	{
		VertexRegion* r = dirty_regions[i];
		assert(r);

		glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * p_data.count, p_data.elements);
	}

	if (normals)
	{
		glBindBuffer(GL_ARRAY_BUFFER, n_vbo);
		for (int i = 0; i < count; i++)
		{
			VertexRegion* r = dirty_regions[i];
			assert(r);

			glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * n_data.count, n_data.elements);
		}
	}

	if (colors)
	{
		glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
		for (int i = 0; i < count; i++)
		{
			VertexRegion* r = dirty_regions[i];
			assert(r);

			glBufferSubData(GL_ARRAY_BUFFER, r->start * sizeof(vec3), sizeof(vec3) * c_data.count, c_data.elements);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	dirty_regions.count = 0;
}
