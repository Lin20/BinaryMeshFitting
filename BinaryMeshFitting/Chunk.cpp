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
}

Chunk::~Chunk()
{
	free(inds);
	inds = 0;
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

	this->inds = 0;
	this->sampler = sampler;
	this->dirty = true;
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
	v_out.push_back(vertices);
	i_out.push_back(mesh_indexes);

	for (size_t i = start; i < i_out.count; i++)
	{
		i_out[i] += mesh_offset;
	}
}

double Chunk::extract(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& i_out, bool silent)
{
	using namespace std;
	double temp_ms = 0, total_ms = 0;
	if (!silent)
		cout << "Extracting base mesh." << endl << "--dim: " << dim << endl << "--level: " << level << endl << "--size: " << setiosflags(ios::fixed) << setprecision(2) << size << endl;

	// Samples
	if (!silent)
		cout << "-Generating samples...";
	clock_t start_clock = clock();
	generate_samples(0, 0);
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

	// Valences
	/*if (!silent)
		cout << "-Calculating valences...";
	start_clock = clock();
	calculate_valences();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;

	// Collapse
	if (!silent)
		cout << "-Collapsing bad cells...";
	start_clock = clock();
	uint32_t bad_count = 0;// collapse_bad_cells();
	temp_ms = clock() - start_clock;
	total_ms += temp_ms;
	if (!silent)
		cout << "done, " << bad_count << " bad cells found (" << (int)(temp_ms / (double)CLOCKS_PER_SEC * 1000.0) << "ms)" << endl;*/

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
		cout << "Complete in " << (int)(total_ms / (double)CLOCKS_PER_SEC * 1000.0) << " ms. " << vertices.count << " verts, " << (mesh_indexes.count / (quads ? 4 : 3)) << " prims." << endl << endl;

	return total_ms;
}
