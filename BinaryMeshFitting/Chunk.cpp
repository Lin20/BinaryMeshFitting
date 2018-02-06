#include "PCH.h"
#include "Chunk.hpp"
#include "Sampler.hpp"
#include "ImplicitSampler.hpp"
#include "Tables.hpp"

#include <iostream>
#include <iomanip>

#define RESOLUTION 256

Chunk::Chunk()
{
}

Chunk::Chunk(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads)
{
	init(pos, size, level, sampler, produce_quads);
	vi = 0;
}

Chunk::~Chunk()
{
}

void Chunk::init(glm::vec3 pos, float size, int level, Sampler& sampler, bool produce_quads)
{
	this->pos = pos;
	this->dim = RESOLUTION;
	this->level = level;
	this->size = size;
	this->quads = produce_quads;

	this->contains_mesh = false;
	this->mesh_offset = 0;

	this->inds_block = 0;
	this->sampler = sampler;
	this->dirty = true;
	this->vi = 0;
}

uint32_t Chunk::encode_vertex(glm::uvec3 xyz)
{
	return xyz.x * (dim + 1) * (dim + 1) + xyz.y * (dim + 1) + xyz.z;
}

uint32_t Chunk::encode_cell(glm::uvec3 xyz)
{
	return xyz.x * dim * dim + xyz.y * dim + xyz.z;
}

void Chunk::copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	mesh_offset = v_out.count;
	size_t start = i_out.count;
	v_out.push_back(vi->vertices);
	i_out.push_back(vi->mesh_indexes);

	for (size_t i = start; i < i_out.count; i++)
	{
		i_out[i] += mesh_offset;
	}
}

double Chunk::extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent)
{
	return 0;
}
