#include "PCH.h"
#include "MeshProcessor.hpp"
#include "MeshRecursion.hpp"

#include <iostream>
#include <Vc/Vc>

using namespace glm;

template<int N>
Processing::MeshProcessor<N>::MeshProcessor(bool simple_quality, bool smooth_normals)
{
	this->simple_quality = simple_quality;
	this->smooth_normals = smooth_normals;
	prims = 0;
}

template<int N>
Processing::MeshProcessor<N>::~MeshProcessor()
{
	free(prims);
	prims = 0;
}

template<int N>
bool Processing::MeshProcessor<N>::init(SmartContainer<DualVertex>& vertices, SmartContainer<uint32_t>& inds, Sampler& sampler)
{
	if (vertices.count == 0 || inds.count < N)
		return true;
	this->sampler = sampler;

	uint32_t a_count = 0;
	uint32_t count = vertices.count;
	for (uint32_t i = 0; i < count; i++)
	{
		vertices[i].adj_offset = a_count;
		vertices[i].adj_next = 0;
		vertices[i].valence = vertices[i].init_valence;
		a_count += vertices[i].init_valence;
	}
	if (!a_count)
		return false;

	//adj_block = (uint32_t*)malloc(a_count * sizeof(uint32_t));
	adj_block.resize(a_count);
	if (!adj_block.elements)
		return false;

	this->vertices.push_back(vertices);
	prims = (Primitive<N>*)malloc(inds.count / N * sizeof(Primitive<N>));
	prim_count = inds.count / N;
	init_primitives(inds);

	return true;
}

template <int N>
void Processing::MeshProcessor<N>::flush(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& inds)
{
	v_out.push_back(vertices);

	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>& t = prims[i];
		if (t.destroyed)
			continue;

		flush_inds flush_op(&inds, t.v);
		recursive_unroll<flush_inds, N>::result(flush_op);
	}
}

template<int N>
void Processing::MeshProcessor<N>::flush_to_tris(SmartContainer<DualVertex>& v_out, SmartContainer<uint32_t>& inds)
{
	v_out.push_back(vertices);

	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>& t = prims[i];
		if (t.destroyed)
			continue;

		inds.push_back(t.v[0]);
		inds.push_back(t.v[1]);
		inds.push_back(t.v[2]);
		inds.push_back(t.v[2]);
		inds.push_back(t.v[3]);
		inds.push_back(t.v[0]);
	}
}

template <int N>
void Processing::MeshProcessor<N>::flush(SmartContainer<glm::vec3>& v_pos, SmartContainer<glm::vec3>& v_norm, SmartContainer<uint32_t>& inds)
{
}

template <int N>
void Processing::MeshProcessor<N>::init_primitives(SmartContainer<uint32_t>& inds)
{
	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>& t = prims[i];
		t.destroyed = 0;
		t.s = 0;
		fill_prim fill_op(t.v, &inds.elements[i * N]);
		recursive_unroll<fill_prim, N>::result(fill_op);
		//t.v[0] = inds[i * 3];
		//t.v[1] = inds[i * 3 + 1];
		//t.v[2] = inds[i * 3 + 2];
		t.weight = 0;
	}

	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>& t = prims[i];
		if (t.destroyed)
			continue;

		//DualVertex* vs[3] = { &vertices[t.v[0]], &vertices[t.v[1]], &vertices[t.v[2]] };
		//adj_block[vs[0]->adj_offset + vs[0]->adj_next++] = i;
		//adj_block[vs[1]->adj_offset + vs[1]->adj_next++] = i;
		//adj_block[vs[2]->adj_offset + vs[2]->adj_next++] = i;
		set_adj adj_op(&adj_block, t.v, vertices.elements, i);
		recursive_unroll<set_adj, N>::result(adj_op);
		adj_block.count += N;
	}
}

template <int N>
void Processing::MeshProcessor<N>::optimize_dual_grid(int iterations)
{
	float total_weight = 1.0f;
	const int set_colors = 3;
	const int hard_norm_max = 10;
	int max_norms = (iterations / 2 - 3 < hard_norm_max ? iterations / 2 - 3 : hard_norm_max);
	int s_prim_count = (int)prim_count;
	for (int m = 0; m < iterations; m++)
	{
		int i;
#pragma omp parallel for
		for (i = 0; i < s_prim_count; i++)
		{
			Primitive<N>& t = prims[i];
			if (t.destroyed)
				continue;

			//float weight = (vertices[t.v[0]].s + vertices[t.v[1]].s + vertices[t.v[2]].s) / 3.0f;
			average_vertex avg_op(t.v, vertices.elements);
			recursive_unroll<average_vertex, N>::result(avg_op);
			t.dual_p = avg_op.p / (float)N;

			average_color avg_opc(t.v, vertices.elements);
			recursive_unroll<average_color, N>::result(avg_opc);
			t.dual_c = avg_opc.c / (float)N;
			//t.dual_p = (vertices[t.v[0]].p + vertices[t.v[1]].p + vertices[t.v[2]].p) / 3.0f;
			//t.dual_n = (vertices[t.v[0]].n + vertices[t.v[1]].n + vertices[t.v[2]].n) / 3.0f;

			if (smooth_normals)
			{
				if (m == 0 || m < max_norms || m < 3)
				{
					if (N == 3)
					{
						vec3 a, b;
						if (t.v[0] != t.v[1] && t.v[1] != t.v[2] && t.v[0] != t.v[2])
						{
							a = vertices[t.v[0]].p - vertices[t.v[1]].p;
							b = vertices[t.v[0]].p - vertices[t.v[2]].p;
						}
						else if (N == 4)
						{
							if (t.v[0] != t.v[1] && t.v[1] != t.v[3] && t.v[0] != t.v[3])
							{
								a = vertices[t.v[0]].p - vertices[t.v[1]].p;
								b = vertices[t.v[0]].p - vertices[t.v[3]].p;
							}
							else if (t.v[0] != t.v[2] && t.v[2] != t.v[3] && t.v[0] != t.v[3])
							{
								a = vertices[t.v[0]].p - vertices[t.v[2]].p;
								b = vertices[t.v[0]].p - vertices[t.v[3]].p;
							}
						}
						t.dual_n = -cross(normalize(a), normalize(b));
					}
					else if (N == 4)
					{
						vec3 n1, n2;
						vec3 a, b;
						a = vertices[t.v[0]].p - vertices[t.v[1]].p;
						b = vertices[t.v[0]].p - vertices[t.v[2]].p;
						n1 = cross(normalize(a), normalize(b));
						a = vertices[t.v[2]].p - vertices[t.v[3]].p;
						b = vertices[t.v[2]].p - vertices[t.v[0]].p;
						n2 = cross(normalize(a), normalize(b));

						if (isnan(n1.x))
						{
							n1 = n2;
							if (isnan(n1.x))
								n1 = vec3(0, 1, 0);
						}
						if (isnan(n2.x))
							n2 = n1;

						t.dual_n = -normalize((n1 + n2) * 0.5f);
					}
				}
				else
				{
					average_normal avg_opn(t.v, vertices.elements);
					recursive_unroll<average_normal, N>::result(avg_opn);
					t.dual_n = avg_opn.n;
					//t.s = weight;
				}
			}
			if (!simple_quality)
			{
				// Project onto surface
				//t.dual_p -= normalize(t.dual_n) * weight * total_weight;
			}
			if (m == iterations - 1)
			{
				//t.dual_n = normalize(sampler.gradient(sampler.world_size, t.dual_p, 0.001f));
			}
			t.weight = 1;
		}

		if (m < iterations - 1)
		{
			optimize_primal_grid(false, (m == set_colors) || (m == 0 && iterations <= set_colors));
		}

		total_weight *= 0.0f;
	}
}

template <int N>
void Processing::MeshProcessor<N>::optimize_primal_grid(bool qef, bool set_colors)
{
	int v_count = (int)vertices.count;
	int i;
#pragma omp parallel for
	for (i = 0; i < v_count; i++)
	{
		DualVertex& v = vertices[i];
		if (v.adj_next == 0)
			continue;
		vec3 p(0, 0, 0);
		vec3 n(0, 0, 0);
		vec3 c(0, 0, 0);
		float s = 0;
		uint32_t* adj = adj_block.elements + v.adj_offset;
		int count = 0;
		for (int k = 0; k < v.adj_next; k++)
		{
			if (*adj == (uint32_t)(-1))
			{
				adj++;
				continue;
			}
			if (prims[*adj].destroyed)
			{
				adj++;
				continue;
			}
			count++;
			s += prims[*adj].s;
			p += prims[*adj].dual_p;
			c += prims[*adj].dual_c;
			if (smooth_normals)
				n += prims[*adj].dual_n;
			adj++;
		}
		s /= (float)count;
		p /= (float)count;
		c /= (float)count;
		if (smooth_normals)
			n /= (float)count;

		if (set_colors)
			n = normalize(n);
		v.s = s;
		v.p = p;
		v.color = c;
		if (n.y != 0)
			v.n = n;

		/*if (set_colors)
		{
		if (v.p.y >= 200.0f)
		v.color = vec3(1.0f, 1.0f, 1.0f);
		else
		{
		float d = glm::dot(v.n, vec3(0, 1, 0));
		if (d >= 0.5f)
		v.color = vec3(0.45f, 0.92f, 0.0f);
		else if (d >= 0.0f)
		v.color = vec3(0.625f, 0.45f, 0.125f);
		else
		v.color = vec3(0.15f, 0.15f, 0.35f);
		}
		}*/
		//v.adj_next = 0;
	}
}

template<int N>
void Processing::MeshProcessor<N>::collapse_bad_quads()
{
	if (N != 4)
		return;

	uint32_t bad_count = 0;
	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>& p = prims[i];
		uint32_t pair[4];
		uint32_t p_out[12] = { 0,0,0,0 };
		int next_p = 0;
		int next = 0;
		for (int k = 0; k < 4; k++)
		{
			DualVertex& dv = vertices[p.v[k]];
			if (dv.adj_next == 3)
			{
				pair[next++] = k;
				if (adj_block[dv.adj_offset + 0] != -1 && adj_block[dv.adj_offset + 0] != i)
					p_out[next_p++] = adj_block[dv.adj_offset + 0];
				if (adj_block[dv.adj_offset + 1] != -1 && adj_block[dv.adj_offset + 1] != i)
					p_out[next_p++] = adj_block[dv.adj_offset + 1];
				if (adj_block[dv.adj_offset + 2] != -1 && adj_block[dv.adj_offset + 2] != i)
					p_out[next_p++] = adj_block[dv.adj_offset + 2];
				//if (next == 2)
				//	break;
			}
		}

		if (next == 4 && next_p == 8)
		{
			continue;
			uint32_t p_inds[4] = { p_out[0], p_out[1], p_out[2], p_out[5] };
			assert(p_inds[0] != p_inds[1] && p_inds[1] != p_inds[2] && p_inds[2] != p_inds[3]);

			prims[p_inds[0]].destroyed = true;
			prims[p_inds[1]].destroyed = true;
			prims[p_inds[2]].destroyed = true;
			prims[p_inds[3]].destroyed = true;

			bad_count++;
			continue;
		}

		if (next != 2 || next_p != 4 || pair[1] - pair[0] != 2)
			continue;

		vec3 new_p(0, 0, 0);
		uint32_t new_index = p.v[pair[0]];
		int old_adj = vertices[new_index].adj_next;
		for (int k = 0; k < 4; k++)
		{
			new_p += vertices[p.v[k]].p;
		}
		new_p *= 0.25f;
		vertices[new_index].p = new_p;
		vertices[new_index].adj_next = 4;

		uint32_t p_other = p.v[pair[1]];

		for (int k = 0; k < 4; k++)
		{
			Primitive<N>& np = prims[p_out[k]];
			if (np.v[0] == new_index || np.v[1] == new_index || np.v[2] == new_index || np.v[3] == new_index)
				continue;
			for (int j = 0; j < 4; j++)
			{
				if (np.v[j] == p_other)
				{
					np.v[j] = new_index;
					break;
				}
			}
		}

		vertices[new_index].adj_offset = adj_block.count;
		for (int k = 0; k < 4; k++)
		{
			adj_block.push_back(p_out[k]);
		}
		p.destroyed = true;

		bad_count++;
	}

	std::cout << "detected " << bad_count << " bad quads...";
}

template<int N>
void Processing::MeshProcessor<N>::collapse_edges()
{
	if (N != 4)
		return;

	using namespace std;

	unordered_map<Edge, PrimPair<N>> edge_map;
	edge_map.reserve(prim_count * 4);

	for (uint32_t i = 0; i < prim_count; i++)
	{
		Primitive<N>* p = &prims[i];

		int edge_maps[8] = { 0, 1, 1, 2, 2, 3, 3, 0 };
		for (int i = 0; i < 4; i++)
		{
			Edge e(p->v[edge_maps[i * 2 + 0]], p->v[edge_maps[i * 2 + 1]]);
			auto search = edge_map.find(e);
			if (search != edge_map.end())
			{
				assert(search->second.p[1] == 0);
				search->second.p[1] = p;
			}
			else
			{
				PrimPair<N> ins(p, 0);
				edge_map.insert({ e,ins });
			}
		}
	}


}

template class Processing::MeshProcessor<3>;
template class Processing::MeshProcessor<4>;
