#pragma once

#include <cstdint>
#include <stdlib.h>
#include <string.h>

template <class T>
class SmartContainer
{
	const size_t DEFAULT_SIZE = 32;
	const int DEFAULT_SCALE = 2;

public:
	size_t size;
	size_t count;
	T* elements;
	int scale;

	inline SmartContainer() : size(0), count(0), elements(0), scale(DEFAULT_SCALE) {}
	inline SmartContainer(size_t num_elements) : count(0)
	{
		scale = DEFAULT_SCALE;
		elements = static_cast<T*>(malloc(sizeof(T) * num_elements));
		if (!elements)
			size = 0;
		else
			size = num_elements;
	}

	inline ~SmartContainer()
	{
		reset();
	}

	inline T& operator[] (int index) { return elements[index]; }

	inline bool prepare(size_t amount)
	{
		if (count + amount <= size)
			return true;
		size_t new_size = (size_t)(pow(2.0, ceil(log2((double)(size + amount)))));
		return resize(new_size);
	}

	inline bool prepare_exact(size_t amount)
	{
		if (count + amount <= size)
			return true;
		return resize(size + amount);
	}

	inline bool resize(size_t new_size)
	{
		if (!elements)
		{
			elements = static_cast<T*>(malloc(sizeof(T) * new_size));
			if (!elements)
				return false;
		}
		else
		{
			T* new_p = static_cast<T*>(realloc((void*)elements, sizeof(T) * new_size));
			if (!new_p)
				return false;
			elements = new_p;
		}
		size = new_size;
		return true;
	}

	inline bool shrink()
	{
		if (!elements)
			return true;

		assert(count < size);
		if (!count)
		{
			free(elements);
			elements = 0;
			size = 0;
			return true;
		}

		realloc((void*)elements, sizeof(T) * count);
		size = count;
		return true;
	}

	inline bool push_back(const T& other)
	{
		if (count >= size)
		{
			if (!resize((!size ? DEFAULT_SIZE : size * scale)))
				return false;
		}

		elements[count++] = other;
		return true;
	}

	inline bool push_back(const SmartContainer<T>& other)
	{
		if (!other.count)
			return true;
		if (!prepare(other.count))
			return false;
		memcpy(elements + count, other.elements, sizeof(T) * other.count);
		count += other.count;
		return true;
	}

	inline bool push_back(const T* other, size_t _count)
	{
		if (!_count)
			return true;
		if (!prepare(_count))
			return false;
		memcpy(elements + count, other, sizeof(T) * _count);
		count += _count;
		return true;
	}

	inline void zero()
	{
		if (!elements || !size)
			return;
		
		memset(elements, 0, sizeof(T) * size);
	}

	inline void reset()
	{
		size = 0;
		count = 0;
		free(elements);
		elements = 0;
	}
};
