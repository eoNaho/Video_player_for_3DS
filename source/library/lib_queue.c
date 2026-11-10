#include "library/lib_queue.h"

#include <stdlib.h>
#include <string.h>

#define LIB_QUEUE_DEFAULT_SEED		(uint64_t)(0x9E3779B97F4A7C15ULL)

static uint64_t Lib_queue_rng(uint64_t* state)
{
	uint64_t value = *state;

	value ^= value >> 12;
	value ^= value << 25;
	value ^= value >> 27;
	*state = value;

	return value * 2685821657736338717ULL;
}

static void Lib_queue_reverse(size_t* values, size_t start, size_t end)
{
	while (start < end)
	{
		size_t swap = values[start];
		values[start] = values[end];
		values[end] = swap;
		start++;
		end--;
	}
}

static bool Lib_queue_rebuild_order(Lib_queue* queue)
{
	size_t* order = NULL;

	if (!queue)
		return false;

	if (queue->count == 0)
	{
		free(queue->order);
		queue->order = NULL;
		queue->order_pos = 0;
		return true;
	}

	order = (size_t*)malloc(queue->count * sizeof(size_t));
	if (!order)
		return false;

	for (size_t i = 0; i < queue->count; i++)
		order[i] = i;

	for (size_t i = queue->count - 1; i > 0; i--)
	{
		size_t j = (size_t)(Lib_queue_rng(&queue->rng_state) % (i + 1));
		size_t swap = order[i];

		order[i] = order[j];
		order[j] = swap;
	}

	free(queue->order);
	queue->order = order;

	//The current item is always the first of the deck; rotate the shuffle.
	queue->order_pos = 0;
	for (size_t i = 0; i < queue->count; i++)
	{
		if (order[i] == queue->cursor)
		{
			if (i > 0)
			{
				Lib_queue_reverse(order, 0, i - 1);
				Lib_queue_reverse(order, i, queue->count - 1);
				Lib_queue_reverse(order, 0, queue->count - 1);
			}
			break;
		}
	}

	return true;
}

static bool Lib_queue_reserve(Lib_queue* queue, size_t needed)
{
	uint64_t* ids = NULL;
	size_t new_capacity = 0;

	if (needed <= queue->capacity)
		return true;

	new_capacity = (queue->capacity == 0) ? 8 : queue->capacity;
	while (new_capacity < needed)
	{
		if (new_capacity > (SIZE_MAX / 2))
			return false;
		new_capacity *= 2;
	}

	ids = (uint64_t*)malloc(new_capacity * sizeof(uint64_t));
	if (!ids)
		return false;

	if (queue->count > 0)
		memcpy(ids, queue->ids, queue->count * sizeof(uint64_t));

	free(queue->ids);
	queue->ids = ids;
	queue->capacity = new_capacity;
	return true;
}

void Lib_queue_init(Lib_queue* queue)
{
	if (!queue)
		return;

	memset(queue, 0, sizeof(*queue));
	queue->mode = LIB_QUEUE_MODE_NO_REPEAT;
	Lib_queue_set_seed(queue, LIB_QUEUE_DEFAULT_SEED);
}

void Lib_queue_free(Lib_queue* queue)
{
	if (!queue)
		return;

	free(queue->ids);
	free(queue->order);
	memset(queue, 0, sizeof(*queue));
}

bool Lib_queue_set(Lib_queue* queue, const uint64_t* ids, size_t count)
{
	uint64_t* copy = NULL;

	if (!queue || (count > 0 && !ids))
		return false;

	if (count > 0)
	{
		copy = (uint64_t*)malloc(count * sizeof(uint64_t));
		if (!copy)
			return false;
		memcpy(copy, ids, count * sizeof(uint64_t));
	}

	free(queue->ids);
	queue->ids = copy;
	queue->count = count;
	queue->capacity = count;
	queue->cursor = 0;

	return Lib_queue_rebuild_order(queue);
}

bool Lib_queue_add(Lib_queue* queue, uint64_t id)
{
	if (!queue)
		return false;
	if (Lib_queue_contains(queue, id))
		return true;
	if (!Lib_queue_reserve(queue, queue->count + 1))
		return false;

	queue->ids[queue->count] = id;
	queue->count++;

	return Lib_queue_rebuild_order(queue);
}

bool Lib_queue_remove(Lib_queue* queue, uint64_t id)
{
	if (!queue)
		return false;

	for (size_t i = 0; i < queue->count; i++)
	{
		if (queue->ids[i] != id)
			continue;

		if (i + 1 < queue->count)
		{
			memmove(&queue->ids[i], &queue->ids[i + 1],
			(queue->count - i - 1) * sizeof(uint64_t));
		}

		queue->count--;
		if (queue->count == 0)
			queue->cursor = 0;
		else if (queue->cursor >= queue->count)
			queue->cursor = queue->count - 1;

		return Lib_queue_rebuild_order(queue);
	}

	return false;
}

bool Lib_queue_contains(const Lib_queue* queue, uint64_t id)
{
	if (!queue)
		return false;

	for (size_t i = 0; i < queue->count; i++)
	{
		if (queue->ids[i] == id)
			return true;
	}

	return false;
}

size_t Lib_queue_count(const Lib_queue* queue)
{
	return queue ? queue->count : 0;
}

bool Lib_queue_set_cursor(Lib_queue* queue, uint64_t id)
{
	if (!queue)
		return false;

	for (size_t i = 0; i < queue->count; i++)
	{
		if (queue->ids[i] == id)
		{
			queue->cursor = i;
			if (queue->mode == LIB_QUEUE_MODE_RANDOM)
				return Lib_queue_rebuild_order(queue);
			return true;
		}
	}

	return false;
}

bool Lib_queue_get_current(const Lib_queue* queue, uint64_t* id_out)
{
	if (!queue || !id_out || queue->count == 0 || queue->cursor >= queue->count)
		return false;

	*id_out = queue->ids[queue->cursor];
	return true;
}

bool Lib_queue_get_at(const Lib_queue* queue, size_t position, uint64_t* id_out)
{
	if (!queue || !id_out || position >= queue->count)
		return false;

	*id_out = queue->ids[position];
	return true;
}

void Lib_queue_set_mode(Lib_queue* queue, Lib_queue_mode mode)
{
	if (!queue)
		return;

	queue->mode = mode;
	if (mode == LIB_QUEUE_MODE_RANDOM)
		Lib_queue_rebuild_order(queue);
}

Lib_queue_mode Lib_queue_get_mode(const Lib_queue* queue)
{
	return queue ? queue->mode : LIB_QUEUE_MODE_NO_REPEAT;
}

void Lib_queue_set_seed(Lib_queue* queue, uint64_t seed)
{
	if (!queue)
		return;

	queue->rng_state = (seed != 0) ? seed : LIB_QUEUE_DEFAULT_SEED;
	if (queue->order && queue->count > 0)
		Lib_queue_rebuild_order(queue);
}

bool Lib_queue_next(Lib_queue* queue, uint64_t* id_out)
{
	if (!queue || !id_out || queue->count == 0)
		return false;

	if (queue->cursor >= queue->count)
		queue->cursor = 0;

	switch (queue->mode)
	{
		case LIB_QUEUE_MODE_REPEAT:
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_IN_ORDER:
			queue->cursor = (queue->cursor + 1) % queue->count;
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_RANDOM:
			if (queue->count == 1)
			{
				*id_out = queue->ids[0];
				return true;
			}

			if (!queue->order && !Lib_queue_rebuild_order(queue))
			{
				queue->cursor = (queue->cursor + 1) % queue->count;
				*id_out = queue->ids[queue->cursor];
				return true;
			}

			if (queue->order_pos + 1 < queue->count)
				queue->order_pos++;
			else
			{
				//New deck: avoid replaying the current item first.
				if (!Lib_queue_rebuild_order(queue))
					return false;
				if (queue->order[0] == queue->cursor)
				{
					size_t swap = queue->order[0];
					queue->order[0] = queue->order[1];
					queue->order[1] = swap;
				}
				queue->order_pos = 0;
			}

			queue->cursor = queue->order[queue->order_pos];
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_NO_REPEAT:
		default:
			if (queue->cursor + 1 >= queue->count)
				return false;
			queue->cursor++;
			*id_out = queue->ids[queue->cursor];
			return true;
	}
}

bool Lib_queue_prev(Lib_queue* queue, uint64_t* id_out)
{
	if (!queue || !id_out || queue->count == 0)
		return false;

	if (queue->cursor >= queue->count)
		queue->cursor = 0;

	switch (queue->mode)
	{
		case LIB_QUEUE_MODE_REPEAT:
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_RANDOM:
			if (!queue->order || queue->order_pos == 0)
				return false;
			queue->order_pos--;
			queue->cursor = queue->order[queue->order_pos];
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_IN_ORDER:
			queue->cursor = (queue->cursor + queue->count - 1) % queue->count;
			*id_out = queue->ids[queue->cursor];
			return true;

		case LIB_QUEUE_MODE_NO_REPEAT:
		default:
			if (queue->cursor == 0)
				return false;
			queue->cursor--;
			*id_out = queue->ids[queue->cursor];
			return true;
	}
}
