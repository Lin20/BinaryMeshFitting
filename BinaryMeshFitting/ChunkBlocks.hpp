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

struct DensityBlock : public LinkedNode<DensityBlock>
{
	uint32_t size;
	float* data;
	bool initialized;

	inline DensityBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
	}

	inline ~DensityBlock()
	{
		_aligned_free(data);
		size = 0;
		initialized = false;
	}

	void init(uint32_t _size)
	{
		if (initialized)
			return;
		_aligned_free(data);
		data = (float*)_aligned_malloc(sizeof(float) * _size, 16);
		initialized = true;
	}
};

struct IsoVertexBlock : public LinkedNode<IsoVertexBlock>
{
	uint32_t size;
	DMC_Isovertex* data;
	bool initialized;

	inline IsoVertexBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
	}

	inline ~IsoVertexBlock()
	{
		_aligned_free(data);
		size = 0;
		initialized = false;
	}

	void init(uint32_t _size)
	{
		if (initialized)
			return;
		_aligned_free(data);
		data = (DMC_Isovertex*)_aligned_malloc(sizeof(DMC_Isovertex) * _size, 16);
		initialized = true;
	}
};

struct NoiseBlock : public LinkedNode<NoiseBlock>
{
	uint32_t size;
	float* dest_noise;
	FastNoiseVectorSet vectorset;
	bool initialized;

	inline NoiseBlock()
	{
		initialized = false;
		size = 0;
		dest_noise = 0;
	}

	inline ~NoiseBlock()
	{
		_aligned_free(dest_noise);
		size = 0;
		initialized = false;
	}

	void init(uint32_t noise_size)
	{
		if (initialized)
			return;
		_aligned_free(dest_noise);
		dest_noise = (float*)_aligned_malloc(sizeof(float) * noise_size, 16);
		vectorset.SetSize(noise_size);
		initialized = true;
	}
};

struct MasksBlock : public LinkedNode<MasksBlock>
{
	uint32_t size;
	uint64_t* data;
	bool initialized;

	inline MasksBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
	}

	inline ~MasksBlock()
	{
		_aligned_free(data);
		size = 0;
		initialized = false;
	}

	void init(uint32_t _size)
	{
		if (initialized)
			return;
		_aligned_free(data);
		data = (uint64_t*)_aligned_malloc(sizeof(uint64_t) * _size, 16);
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

struct DMC_CellsBlock : public LinkedNode<DMC_CellsBlock>
{
	SmartContainer<DMC_Cell> cells;

	inline DMC_CellsBlock()
	{
	}

	inline ~DMC_CellsBlock()
	{
	}

	void init()
	{
		cells.count = 0;
	}
};
