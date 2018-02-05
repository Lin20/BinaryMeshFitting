#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "SmartContainer.hpp"
#include "Vertices.hpp"
#include "LinkedNode.hpp"

class GLChunk : public LinkedNode<GLChunk>
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
	GLuint ibo;

	uint32_t v_count;
	uint32_t p_count;
	uint32_t vbo_size;
	uint32_t ibo_size;

	SmartContainer<glm::vec3> p_data;
	SmartContainer<glm::vec3> n_data;
	SmartContainer<glm::vec3> c_data;

	GLChunk();
	~GLChunk();
	void init(bool _normals, bool _colors);
	void destroy();
	bool set_data(SmartContainer<uint32_t>& index_data, bool unwind_verts);
	bool set_data(SmartContainer<glm::vec3>& pos_data, SmartContainer<uint32_t>& index_data);
	bool set_data(SmartContainer<glm::vec3>& pos_data, SmartContainer<glm::vec3>& norm_data, SmartContainer<glm::vec3>& color_data, SmartContainer<uint32_t>& index_data, bool unwind_verts);
	bool format_data(SmartContainer<DualVertex>& vert_data, SmartContainer<uint32_t>& index_data, bool unwind_verts, bool smooth_normals);
};