#pragma once

#include <gl/glew.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#define GLM_FORCE_NO_CTOR_INIT
#define GLM_FORCE_INLINE
#include <glm/glm.hpp>
#include <vector>

#include "Vertices.hpp"
#include "SmartContainer.hpp"
#include "Sampler.hpp"

class DualChunk
{
public:
	glm::vec3 pos;
	uint32_t dim;
	int level;
	float size;

	bool contains_mesh;
	uint32_t mesh_offset;

	uint32_t* inds;
	PrimalVertex* samples;
	Sampler sampler;

	SmartContainer<DualVertex> vertices;
	SmartContainer<uint32_t> mesh_indexes;

	DualChunk();
	DualChunk(glm::vec3 pos, float size, int level, Sampler& sampler);
	~DualChunk();
	void init(glm::vec3 pos, float size, int level, Sampler& sampler);
	void generate_samples(const float(*f)(const float, const glm::vec3&));
	void generate_dual_vertices();
	bool calculate_dual_vertex(glm::uvec3 xyz, uint32_t next_index, DualVertex* result, bool force);

	uint32_t encode_vertex(glm::uvec3 xyz);
	uint32_t encode_cell(glm::uvec3 xyz);

	void generate_base_mesh();
	void copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out);

	void extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent);
};