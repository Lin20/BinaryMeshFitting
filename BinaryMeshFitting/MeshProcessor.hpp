#pragma once

#include "Sampler.hpp"
#include "SmartContainer.hpp"
#include "Vertices.hpp"
#include <unordered_map>

namespace Processing
{
	template<int N>
	struct Primitive
	{
		float s;
		uint32_t v[N];
		//uint32_t neighbor_duals[3];

		// Dual vertex
		glm::vec3 dual_p;
		glm::vec3 dual_n;
		glm::vec3 dual_c;
		float weight;
		int destroyed : 1;
	};

	struct Edge
	{
		uint32_t v[2];

		Edge() {}
		Edge(uint32_t v0, uint32_t v1)
		{
			v[0] = v0;
			v[1] = v1;
		}

		bool operator==(const Edge& other) const
		{
			return (v[0] == other.v[0] && v[1] == other.v[1]) || (v[0] == other.v[1] && v[1] == other.v[0]);
		}
	};

	template <int N>
	struct PrimPair
	{
		Primitive<N>* p[2];

		PrimPair() {}
		PrimPair(Primitive<N>* p0, Primitive<N>* p1) {
			p[0] = p0;
			p[1] = p1;
		}
	};

	template <int N>
	class MeshProcessor
	{
		uint32_t prim_count;
		Sampler sampler;
		SmartContainer<DualVertex> vertices;
		SmartContainer<uint32_t> adj_block;
		Primitive<N>* prims;
		bool smooth_normals;

	public:
		MeshProcessor(bool simple_quality, bool smooth_normals);
		~MeshProcessor();
		bool init(SmartContainer<DualVertex>& vertices, SmartContainer<uint32_t>& inds, Sampler& sampler);
		void flush(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& inds);
		void flush_to_tris(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& inds);
		void flush(SmartContainer<glm::vec3>& v_pos, SmartContainer <glm::vec3>& v_norm, SmartContainer<uint32_t>& inds);
		void init_primitives(SmartContainer<uint32_t>& inds);
		void optimize_dual_grid(int iterations);
		void optimize_primal_grid(bool qef, bool set_colors);

		void optimize_dual_prims(int start, bool face_norm);

		void collapse_bad_quads();
		void collapse_edges();

		bool simple_quality;

	private:
	};
}

namespace std
{
	template <>
	struct hash<Processing::Edge>
	{
		size_t operator()(const Processing::Edge& e) const
		{
			return e.v[0] ^ e.v[1] + e.v[0] + e.v[1];
		}
	};
}
