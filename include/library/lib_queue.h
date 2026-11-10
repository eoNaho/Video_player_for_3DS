#if !defined(DEF_LIB_QUEUE_H)
#define DEF_LIB_QUEUE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t Lib_queue_mode;
#define LIB_QUEUE_MODE_NO_REPEAT			(Lib_queue_mode)(0)	//Stop at the end.
#define LIB_QUEUE_MODE_REPEAT				(Lib_queue_mode)(1)	//Replay the same item.
#define LIB_QUEUE_MODE_IN_ORDER			(Lib_queue_mode)(2)	//Advance and wrap.
#define LIB_QUEUE_MODE_RANDOM				(Lib_queue_mode)(3)	//Shuffled deck, no repeats per pass.

typedef struct
{
	uint64_t* ids;
	size_t count;
	size_t capacity;
	size_t cursor;
	size_t* order;						//Permutation of [0, count) for random mode.
	size_t order_pos;
	uint64_t rng_state;
	Lib_queue_mode mode;
} Lib_queue;

void Lib_queue_init(Lib_queue* queue);

void Lib_queue_free(Lib_queue* queue);

//Replaces the queue contents and moves the cursor to the first item.
bool Lib_queue_set(Lib_queue* queue, const uint64_t* ids, size_t count);

bool Lib_queue_add(Lib_queue* queue, uint64_t id);

bool Lib_queue_remove(Lib_queue* queue, uint64_t id);

bool Lib_queue_contains(const Lib_queue* queue, uint64_t id);

size_t Lib_queue_count(const Lib_queue* queue);

//Moves the cursor to id. Returns false when the id is not in the queue.
bool Lib_queue_set_cursor(Lib_queue* queue, uint64_t id);

bool Lib_queue_get_current(const Lib_queue* queue, uint64_t* id_out);

bool Lib_queue_get_at(const Lib_queue* queue, size_t position, uint64_t* id_out);

void Lib_queue_set_mode(Lib_queue* queue, Lib_queue_mode mode);

Lib_queue_mode Lib_queue_get_mode(const Lib_queue* queue);

//Seeds the deterministic shuffle (0 is replaced by a fixed non-zero seed).
void Lib_queue_set_seed(Lib_queue* queue, uint64_t seed);

//Returns false when playback should stop (end of a NO_REPEAT queue/empty queue).
bool Lib_queue_next(Lib_queue* queue, uint64_t* id_out);

bool Lib_queue_prev(Lib_queue* queue, uint64_t* id_out);

#endif //!defined(DEF_LIB_QUEUE_H)
