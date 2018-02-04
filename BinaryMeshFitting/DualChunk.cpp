#include "PCH.h"
#include "DualChunk.hpp"
#include "Sampler.hpp"
#include "ImplicitSampler.hpp"
#include "Tables.hpp"

#include <iostream>
#include <iomanip>

using namespace glm;

#define RESOLUTION 128
#define INITIAL_VERT_SIZE 1024

DualChunk::DualChunk()
{
}

DualChunk::DualChunk(glm::vec3 pos, float size, int level, Sampler& sampler)
{
	init(pos, size, level, sampler);
}

DualChunk::~DualChunk()
{
	free(inds);
	free(samples);
}

void DualChunk::init(glm::vec3 pos, float size, int level, Sampler& sampler)
{
	this->pos = pos;
	this->dim = RESOLUTION;
	this->level = level;
	this->size = size;

	this->contains_mesh = false;
	this->mesh_offset = 0;

	this->inds = 0;
	this->samples = 0;
	this->sampler = sampler;
}

void DualChunk::generate_samples(const float (*f0)(const float, const glm::vec3&))
{
	assert(sampler.value != nullptr);
	uint32_t dimp1 = dim + 1;
	bool positive = false, negative = false;
	uint32_t count = dimp1 * dimp1 * dimp1;

	samples = (PrimalVertex*)malloc(sizeof(PrimalVertex) * count);
	float delta = size / (float)dim;

	vec3 dxyz;
	auto f = sampler.value;
	const float res = sampler.world_size;
	for (uint32_t x = 0; x < dimp1; x++)
	{
		dxyz[0] = pos.x + (float)x * delta;
		for (uint32_t y = 0; y < dimp1; y++)
		{
			dxyz[1] = pos.y + (float)y * delta;
			for (uint32_t z = 0; z < dimp1; z++)
			{
				dxyz[2] = pos.z + (float)z * delta;
				uint32_t index = x * dimp1 * dimp1 + y * dimp1 + z;
				float s = f(res, dxyz); //sampler.value(sampler.world_size, dxyz);
				samples[index].s = s;
				samples[index].p = dxyz;
				if (s > 0)
					positive = true;
				else
					negative = true;
			}
		}
	}

	if (positive && negative)
		contains_mesh = true;
}

void DualChunk::generate_dual_vertices()
{
	if (!contains_mesh)
		return;

	uint32_t count = dim * dim * dim;
	vertices.resize(1024);
	inds = (uint32_t*)malloc(sizeof(uint32_t) * count);
	DualVertex temp;

	for (uint32_t x = 0; x < dim; x++)
	{
		for (uint32_t y = 0; y < dim; y++)
		{
			for (uint32_t z = 0; z < dim; z++)
			{
				uint32_t index = x * dim * dim + y * dim + z;
				if (calculate_dual_vertex(uvec3(x,y,z), vertices.count, &temp, false))
				{
					inds[index] = vertices.count;
					vertices.push_back(temp);
				}
				else
					inds[index] = -1;
			}
		}
	}
}

bool DualChunk::calculate_dual_vertex(glm::uvec3 xyz, uint32_t next_index, DualVertex* result, bool force)
{
	//PrimalVertex corners[8];
	int mask = 0;
	float s = 0;
	for (int i = 0; i < 8; i++)
	{
		uvec3 c(xyz.x + Tables::TDX[i], xyz.y + Tables::TDY[i], xyz.z + Tables::TDZ[i]);
		//corners[i] = samples[encode_vertex(c)];
		//if (corners[i].s < 0.0f)
		if(samples[encode_vertex(c)].s < 0.0f)
			mask |= 1 << i;
		//corners[i] = samples[encode_vertex(c)];
		s += samples[encode_vertex(c)].s;
	}

	if (!force && (mask == 0 || mask == 255))
		return false;

	result->valence = 0;
	result->index = next_index;
	result->xyz = xyz;
	result->edge_mask = 0;
	result->mask = (uint8_t)mask;
	result->s = s * 0.125f;
	uint16_t edge_mask = 0;
	vec3 mass_point(0,0,0);
	//int e_count = 0;

	for (int i = 0; i < 12; i++)
	{
		int v0 = Tables::TEdgePairs[i][0];
		int v1 = Tables::TEdgePairs[i][1];

		int c0 = (mask >> v0) & 1;
		int c1 = (mask >> v1) & 1;
		if (c0 == c1)
			continue;

		edge_mask |= 1 << i;
		//mass_point += ImplicitFunctions::get_intersection(corners[v0].p, corners[v1].p, corners[v0].s, corners[v1].s);
		//e_count++;
	}
	result->edge_mask = edge_mask;

	// For now, just place in the middle of the cell
	result->p = pos + vec3((float)xyz.x + 0.5f, (float)xyz.y + 0.5f, (float)xyz.z + 0.5f) * size / (float)dim;
	//result->p = mass_point / (float)e_count;
	result->n = normalize(sampler.gradient(sampler.world_size, result->p, 0.001f));

	return true;
}

uint32_t DualChunk::encode_vertex(glm::uvec3 xyz)
{
	return xyz.x * (dim + 1) * (dim + 1) + xyz.y * (dim + 1) + xyz.z;
}

uint32_t DualChunk::encode_cell(glm::uvec3 xyz)
{
	return xyz.x * dim * dim + xyz.y * dim + xyz.z;
}

void DualChunk::generate_base_mesh()
{
	if (!contains_mesh)
		return;

	assert(vertices.count > 0);
	const int masks[] = { 3, 7, 11 };
	const int de[][3][3] = {
		{ { 0, 0, 1 },{ 0, 1, 0 },{ 0, 1, 1 } },
		{ { 0, 0, 1 },{ 1, 0, 0 },{ 1, 0, 1 } },
		{ { 0, 1, 0 },{ 1, 0, 0 },{ 1, 1, 0 } }
	};

	mesh_indexes.prepare(vertices.count * 6);
	size_t v_count = vertices.count;
	uint32_t v_inds[4];
	for (size_t i = 0; i < v_count; i++)
	{
		DualVertex dv = vertices[i];
		uvec3 xyz = dv.xyz;
		v_inds[0] = dv.index;
		for (int i = 0; i < 3; i++)
		{
			if (((dv.edge_mask >> masks[i]) & 1) != 0)
			{
				if (xyz.x + de[i][0][0] >= dim || xyz.x + de[i][1][0] >= dim || xyz.x + de[i][2][0] >= dim)
					continue;
				if (xyz.y + de[i][0][1] >= dim || xyz.y + de[i][1][1] >= dim || xyz.y  + de[i][2][1] >= dim)
					continue;
				if (xyz.z + de[i][0][2] >= dim || xyz.z + de[i][1][2] >= dim || xyz.z + de[i][2][2] >= dim)
					continue;

				v_inds[1] = inds[(xyz.x + de[i][0][0]) * dim * dim + (xyz.y + de[i][0][1]) * dim + (xyz.z + de[i][0][2])];
				v_inds[2] = inds[(xyz.x + de[i][1][0]) * dim * dim + (xyz.y + de[i][1][1]) * dim + (xyz.z + de[i][1][2])];
				v_inds[3] = inds[(xyz.x + de[i][2][0]) * dim * dim + (xyz.y + de[i][2][1]) * dim + (xyz.z + de[i][2][2])];

				uint32_t v_out[6] = { v_inds[0], v_inds[1], v_inds[3], v_inds[3], v_inds[2], v_inds[0] };
				vertices[v_inds[0]].valence += 3;
				vertices[v_inds[1]].valence += 2;
				vertices[v_inds[2]].valence += 2;
				vertices[v_inds[3]].valence += 3;
				//if (v_out[0] != v_out[1] && v_out[1] != v_out[2])
				{
					mesh_indexes.push_back(v_out[0]);
					mesh_indexes.push_back(v_out[1]);
					mesh_indexes.push_back(v_out[2]);
				}
				//if (v_out[3] != v_out[4] && v_out[4] != v_out[5])
				{
					mesh_indexes.push_back(v_out[3]);
					mesh_indexes.push_back(v_out[4]);
					mesh_indexes.push_back(v_out[5]);
				}
			}
		}
	}
}

void DualChunk::copy_verts_and_inds(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out)
{
	mesh_offset = i_out.count;
	v_out.push_back(vertices);
	i_out.push_back(mesh_indexes);
}

void DualChunk::extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent)
{
	using namespace std;
	double temp_ms = 0, total_ms = 0;
	if (!silent)
		cout << "Extracting base mesh." << endl << "--dim: " << dim << endl << "--level: " << level << endl << "--size: " << setiosflags(ios::fixed) << setprecision(2) << size << endl;

	// Samples
	if (!silent)
		cout << "-Generating samples...";
	clock_t start_clock = clock();
	generate_samples(&ImplicitFunctions::torus_z);
	total_ms += clock() - start_clock;
	if (!silent)
		cout << "done (" << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Dual vertices
	if (!silent)
		cout << "-Generating dual vertices...";
	start_clock = clock();
	generate_dual_vertices();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Base mesh
	if (!silent)
		cout << "-Generating base mesh...";
	start_clock = clock();
	generate_base_mesh();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Vertex/index output
	if (!silent)
		cout << "-Copying vertices...";
	start_clock = clock();
	copy_verts_and_inds(v_out, i_out);
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Final
	if (!silent)
		cout << "Complete in " << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << " ms. " << vertices.count << " verts, " << (mesh_indexes.count / 3) << " prims." << endl << endl;
}
