#pragma once

template <class T>
class LinkedNode
{
public:
	LinkedNode<T>* prev;
	LinkedNode<T>* next;

	inline LinkedNode()
	{
		prev = 0;
		next = 0;
	}

	inline ~LinkedNode()
	{
		unlink();
	}

	inline void unlink()
	{
		if (prev)
		{
			prev->next = next;
		}
		if (next)
		{
			next->prev = prev;
		}
		next = 0;
		prev = 0;
	}
};