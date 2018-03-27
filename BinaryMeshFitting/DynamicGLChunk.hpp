#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "SmartContainer.hpp"
#include "Vertices.hpp"
#include "LinkedList.hpp"
#include "MemoryPool.h"

struct VertexRegion : public LinkedNode<VertexRegion>
{
	uint32_t start;
	uint32_t count;
	bool marked_dirty;

	inline VertexRegion()
	{
		start = 0;
		count = 0;
		marked_dirty = false;
	}

	inline VertexRegion(uint32_t _start, uint32_t _count) : start(_start), count(_count), marked_dirty(false)
	{
	}
};

class DynamicGLChunk
{
public:
	bool initialized;
	bool normals;
	bool colors;

	// Important -- Do not change structure!
	GLuint vao;
	GLuint v_vbo;
	GLuint n_vbo;
	GLuint c_vbo;

	uint32_t v_count;
	uint32_t p_count;
	uint32_t vbo_size;

	SmartContainer<glm::vec3> p_data;
	SmartContainer<glm::vec3> n_data;
	SmartContainer<glm::vec3> c_data;

	DynamicGLChunk();
	~DynamicGLChunk();
	void init(bool _normals, bool _colors);
	void destroy();

	VertexRegion* allocate(uint32_t count);
	void free(VertexRegion* r);
	void reset(VertexRegion* r, uint32_t offset = 0);

	void upload(VertexRegion* r);
	void upload_dirty_regions();

	MemoryPool<VertexRegion> region_pool;
	LinkedList<VertexRegion> used_regions;
	LinkedList<VertexRegion> free_regions;

	SmartContainer<VertexRegion*> dirty_regions;
};
