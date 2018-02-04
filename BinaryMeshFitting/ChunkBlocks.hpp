#pragma once

#include <cstdint>
#include <string>
#include <FastNoiseSIMD.h>
#include "LinkedNode.hpp"

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
	FastNoiseVectorSet vectorset;
	bool initialized;

	inline FloatBlock()
	{
		initialized = false;
		size = 0;
		data = 0;
	}

	inline ~FloatBlock()
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
		data = (float*)_aligned_malloc(sizeof(float) * _raw_size, 16);
		vectorset.SetSize(_raw_size);
		initialized = true;
	}
};
