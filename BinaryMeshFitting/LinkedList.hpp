#pragma once

#include "LinkedNode.hpp"

template <class T>
class LinkedList
{
public:
	LinkedNode<T>* head;
	LinkedNode<T>* tail;
	size_t count;

	inline LinkedList()
	{
		head = 0;
		tail = 0;
		count = 0;
	}

	inline ~LinkedList()
	{
		head = 0;
		tail = 0;
		count = 0;
	}

	inline void set_first(LinkedNode<T>* node)
	{
		head = node;
		tail = node;
		count = 1;
	}

	inline void push_back(LinkedNode<T>* node)
	{
		if (!node || node->prev || node->next)
			return;
		if (!tail)
		{
			set_first(node);
			return;
		}

		tail->next = node;
		node->prev = tail;
		tail = node;
		count++;
	}

	inline LinkedNode<T>* unlink(LinkedNode<T>* node)
	{
		if (head == node)
		{
			head = node->next;
		}
		if (tail == node)
		{
			tail = node->prev;
		}
		node->unlink();
		count--;
		return node;
	}
};
