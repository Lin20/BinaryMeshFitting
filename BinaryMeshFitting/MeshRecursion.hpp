#pragma once

#include <glm/glm.hpp>
#include "Vertices.hpp"

// Average vertex
template<class TOp, int count>
struct recursive_unroll {
	__forceinline static void result(TOp& k)
	{
		k();
		recursive_unroll<TOp, count - 1>::result(k);
	}
};

template<class TOp>
struct recursive_unroll<TOp, 0> {
	__forceinline static void result(TOp& k) {}
};

struct average_vertex
{
	glm::vec3 p;
	uint32_t* inds;
	DualVertex* verts;

	inline average_vertex(uint32_t* v_inds, DualVertex* vertices) : inds(v_inds), verts(vertices), p(0, 0, 0) {}

	__forceinline void operator()()
	{
		p += verts[*(inds++)].p;
	}
};

struct average_normal
{
	glm::vec3 n;
	uint32_t* inds;
	DualVertex* verts;

	inline average_normal(uint32_t* v_inds, DualVertex* vertices) : inds(v_inds), verts(vertices), n(0, 0, 0) {}

	__forceinline void operator()()
	{
		n += verts[*(inds++)].n;
	}
};

struct average_color
{
	glm::vec3 c;
	uint32_t* inds;
	DualVertex* verts;

	inline average_color(uint32_t* v_inds, DualVertex* vertices) : inds(v_inds), verts(vertices), c(0, 0, 0) {}

	__forceinline void operator()()
	{
		c += verts[*(inds++)].color;
	}
};

struct fill_prim
{
	uint32_t* inds;
	uint32_t* src_inds;

	inline fill_prim(uint32_t* dest_inds, uint32_t* source_inds) : inds(dest_inds), src_inds(source_inds) {}

	__forceinline void operator()()
	{
		*inds++ = *src_inds++;
	}
};

struct set_adj
{
	SmartContainer<uint32_t>* adj;
	uint32_t* inds;
	DualVertex* verts;
	uint32_t p_index;
	bool* boundary;

	inline set_adj(SmartContainer<uint32_t>* adj_block, uint32_t* p_inds, DualVertex* vertices, uint32_t prim_index, bool* _boundary) : adj(adj_block), inds(p_inds), verts(vertices), p_index(prim_index), boundary(_boundary) {}

	__forceinline void operator()()
	{
		(*adj)[verts[*inds].adj_offset + verts[*inds].adj_next++] = p_index;
		if (verts[*inds].boundary)
			*boundary = true;
		inds++;
	}
};

struct flush_inds
{
	SmartContainer<uint32_t>* inds;
	uint32_t* src_inds;

	inline flush_inds(SmartContainer<uint32_t>* dest_inds, uint32_t* p_inds) : inds(dest_inds), src_inds(p_inds) {}

	__forceinline void operator()()
	{
		inds->push_back(*src_inds);
		src_inds++;
	}
};

template<int N>
struct build_adjacency
{
	std::unordered_map<Processing::Edge, Processing::Primitive<N>*[2]> map;

};
