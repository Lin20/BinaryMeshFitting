#include "PCH.h"
#include "DynamicGLChunk.hpp"

#define DEFAULT_V_COUNT 524288

DynamicGLChunk::DynamicGLChunk()
{
	initialized = false;
	normals = true;

	vao = 0;
	v_vbo = 0;
	n_vbo = 0;
	ibo = 0;

	v_count = 0;
	p_count = 0;
	vbo_size = 0;
	ibo_size = 0;
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

	glGenBuffers(1, &v_vbo);
	if (normals)
		glGenBuffers(1, &n_vbo);
	if (colors)
		glGenBuffers(1, &c_vbo);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(0);

	initialized = true;

	VertexRegion* base = region_pool.newElement(0, DEFAULT_V_COUNT);
	free_regions.push_back(base);
}

void DynamicGLChunk::destroy()
{
	if (initialized)
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(4, &v_vbo);
	}
	initialized = 0;
}
