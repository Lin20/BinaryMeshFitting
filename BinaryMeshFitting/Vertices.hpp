#pragma once
#include "PCH.h"
#include <glm/glm.hpp>

struct PrimalVertex
{
	float s;
	glm::vec3 p;
	//glm::vec3 n;

	inline PrimalVertex() {}
	inline PrimalVertex(float sample, glm::vec3 pos) : s(sample), p(pos) {}
	//inline PrimalVertex(float sample, glm::vec3 pos, glm::vec3 norm) : s(sample), p(pos), n(norm) {}
};

struct DualVertex
{
	uint8_t mask;
	uint32_t index;
	uint8_t valence;
	uint8_t init_valence;
	uint8_t adj_next;
	uint32_t adj_offset;
	uint16_t edge_mask;
	float s;
	glm::ivec3 xyz;
	glm::vec3 p;
	glm::vec3 n;
	glm::vec3 avg;
	glm::vec3 color;

	inline DualVertex() {}
	inline DualVertex(glm::vec3 pos, glm::vec3 norm) : p(pos), n(norm) {}
};

struct Cell
{
	int index;
	uint8_t mask;
	uint16_t edge_mask;
	glm::uvec3 xyz;
	uint32_t edge_map;
	uint32_t v_map[4];
	glm::vec3 edge_verts[3];
};

struct DMC_Isovertex
{
	uint32_t index;
	float value;
	glm::vec3 position;
};

struct DMC_Edge
{
	bool snapped;
	uint32_t grid_v0;
	uint32_t grid_v1;
	float length;
	DMC_Isovertex iso_vertex;
};

struct DMC_Cell
{
	uint16_t mask;
	DMC_Edge edges[3];
};

struct DMC_ImmediateCell
{
	uint16_t mask;
	uint32_t iso_verts[20];
};
