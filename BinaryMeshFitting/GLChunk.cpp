#include "PCH.h"
#include "GLChunk.hpp"
#include "SmartContainer.hpp"

GLChunk::GLChunk()
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

GLChunk::~GLChunk()
{
	destroy();
}

void GLChunk::init(bool _normals, bool _colors)
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
	glGenBuffers(1, &ibo);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(0);

	initialized = true;
}

void GLChunk::destroy()
{
	if (initialized)
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(4, &v_vbo);
	}
	initialized = 0;
}

bool GLChunk::set_data(SmartContainer<uint32_t>& index_data, bool unwind_verts)
{
	return set_data(p_data, n_data, c_data, index_data, unwind_verts);
}

bool GLChunk::set_data(SmartContainer<glm::vec3>& pos_data, SmartContainer<uint32_t>& index_data)
{
	if (!pos_data.count || !index_data.count)
		return false;

	using namespace glm;

	// Vertex buffers
	if (pos_data.count < vbo_size && false)
	{
		glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * pos_data.count, pos_data.elements);
	}
	else
	{
		vbo_size = pos_data.count;
		glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * pos_data.count, pos_data.elements, GL_STATIC_DRAW);
	}

	// Index buffer
	if (index_data.count < ibo_size && false)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(uint32_t) * index_data.count, index_data.elements);
	}
	else
	{
		ibo_size = index_data.count;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_data.count, index_data.elements, GL_STATIC_DRAW);
	}

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	v_count = pos_data.count;
	p_count = index_data.count;

	return true;
}

bool GLChunk::set_data(SmartContainer<glm::vec3>& pos_data, SmartContainer<glm::vec3>& norm_data, SmartContainer<glm::vec3>& color_data, SmartContainer<uint32_t>& index_data, bool unwind_verts)
{
	if (!pos_data.count || !norm_data.count || !index_data.count)
	{
		v_count = 0;
		p_count = 0;
		return false;
	}

	using namespace glm;

	// Vertex buffers
	if (pos_data.count < vbo_size && false)
	{
		glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * pos_data.count, pos_data.elements);

		glBindBuffer(GL_ARRAY_BUFFER, n_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * norm_data.count, norm_data.elements);
	}
	else
	{
		vbo_size = pos_data.count;
		glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * pos_data.count, pos_data.elements, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, n_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * norm_data.count, norm_data.elements, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * color_data.count, color_data.elements, GL_STATIC_DRAW);
	}

	// Index buffer
	if (!unwind_verts)
	{
		if (index_data.count < ibo_size && false)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(uint32_t) * index_data.count, index_data.elements);
		}
		else
		{
			ibo_size = index_data.count;
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_data.count, index_data.elements, GL_STATIC_DRAW);
		}
	}

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, v_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ARRAY_BUFFER, n_vbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glBindBuffer(GL_ARRAY_BUFFER, c_vbo);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	if (!unwind_verts)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	v_count = pos_data.count;
	if (!unwind_verts)
		p_count = index_data.count;
	else
		p_count = pos_data.count;

	return true;
}

bool GLChunk::format_data(SmartContainer<DualVertex>& vert_data, SmartContainer<uint32_t>& index_data, bool unwind_verts, bool smooth_normals)
{
	p_data.count = 0;
	n_data.count = 0;
	c_data.count = 0;
	using namespace glm;
	if (!unwind_verts)
	{
		p_data.prepare_exact(vert_data.count);
		n_data.prepare_exact(vert_data.count);
		c_data.prepare_exact(vert_data.count);

		size_t count = vert_data.count;
		for (size_t i = 0; i < count; i++)
		{
			p_data.push_back(vert_data[i].p);
			n_data.push_back(vert_data[i].n);
			c_data.push_back(vert_data[i].color);
		}
	}
	else
	{
		p_data.prepare_exact(vert_data.count * 4);
		n_data.prepare_exact(vert_data.count * 4);
		c_data.prepare_exact(vert_data.count * 4);

		size_t count = index_data.count;
		for (size_t i = 0; i < count; i += 4)
		{
			vec3 p[4] = { vert_data[index_data[i]].p, vert_data[index_data[i + 1]].p, vert_data[index_data[i + 2]].p, vert_data[index_data[i + 3]].p };
			vec3 c[4] = { vert_data[index_data[i]].color, vert_data[index_data[i + 1]].color, vert_data[index_data[i + 2]].color, vert_data[index_data[i + 3]].color };
			p_data.push_back(p, 4);
			c_data.push_back(c, 4);
			
			vec3 n;
			if (smooth_normals)
			{
				n = (vert_data[index_data[i]].n + vert_data[index_data[i + 1]].n + vert_data[index_data[i + 2]].n + vert_data[index_data[i + 3]].n) * 0.25f;
			}
			else
			{
				vec3 n0 = cross(normalize(vert_data[index_data[i]].p - vert_data[index_data[i + 1]].p), normalize(vert_data[index_data[i]].p - vert_data[index_data[i + 2]].p));
				vec3 n1 = cross(normalize(vert_data[index_data[i + 2]].p - vert_data[index_data[i + 3]].p), normalize(vert_data[index_data[i + 2]].p - vert_data[index_data[i]].p));
				if (isnan(n0.x))
					n0 = n1;
				if (isnan(n1.x))
					n1 = n0;
				n = -normalize((n0 + n1) * 0.5f);
			}
			n_data.push_back(n);
			n_data.push_back(n);
			n_data.push_back(n);
			n_data.push_back(n);
		}
	}

	return true;
}
