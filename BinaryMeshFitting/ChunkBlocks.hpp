#pragma once

#include <cstdint>
#include <string>
#include <FastNoiseSIMD.h>
#include "LinkedNode.hpp"
#include "Vertices.hpp"

struct BinaryBlock : public LinkedNode<BinaryBlock>
{
	uint32_t size;
	uint32_t* data;
	bool initialized;

	inline BinaryBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
	}

	inline ~BinaryBlock()
	{
		_aligned_free(data);
		size = 0;
		initialized = false;
	}

	void init(uint32_t _raw_size, uint32_t _binary_size)
	{
		if (initialized)
			return;
		_aligned_free(data);
		data = (uint32_t*)_aligned_malloc(sizeof(uint32_t) * _binary_size, 16);
		initialized = true;
	}
};

struct FloatBlock : public LinkedNode<FloatBlock>
{
	uint32_t size;
	float* data;
	float* dest_noise;
	FastNoiseVectorSet vectorset;
	bool initialized;

	inline FloatBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
		dest_noise = 0;
	}

	inline ~FloatBlock()
	{
		_aligned_free(data);
		_aligned_free(dest_noise);
		size = 0;
		initialized = false;
	}

	void init(uint32_t _raw_size, uint32_t noise_size)
	{
		if (initialized)
			return;
		_aligned_free(data);
		_aligned_free(dest_noise);
		data = (float*)_aligned_malloc(sizeof(float) * _raw_size, 16);
		dest_noise = (float*)_aligned_malloc(sizeof(float) * noise_size, 16);
		vectorset.SetSize(noise_size);
		initialized = true;
	}
};

struct VerticesIndicesBlock : public LinkedNode<VerticesIndicesBlock>
{
	SmartContainer<DualVertex> vertices;
	SmartContainer<uint32_t> mesh_indexes;

	inline VerticesIndicesBlock()
	{
	}

	inline ~VerticesIndicesBlock()
	{
	}

	void init()
	{
		vertices.count = 0;
		mesh_indexes.count = 0;
	}
};

struct CellsBlock : public LinkedNode<CellsBlock>
{
	SmartContainer<Cell> cells;

	inline CellsBlock()
	{
	}

	inline ~CellsBlock()
	{
	}

	void init()
	{
		cells.count = 0;
	}
};

struct IndexesBlock : public LinkedNode<IndexesBlock>
{
	uint32_t size;
	uint32_t* inds;
	bool initialized;

	inline IndexesBlock()
	{
		initialized = false;
		size = 0;
		inds = 0;
	}

	inline ~IndexesBlock()
	{
		_aligned_free(inds);
		size = 0;
		initialized = false;
	}

	void init(uint32_t size)
	{
		if (initialized)
			return;
		_aligned_free(inds);
		inds = (uint32_t*)_aligned_malloc(sizeof(uint32_t) * size, 16);
		initialized = true;
	}
};
