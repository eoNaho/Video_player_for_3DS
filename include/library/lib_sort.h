#if !defined(DEF_LIB_SORT_H)
#define DEF_LIB_SORT_H
#include <stdbool.h>
#include <stddef.h>
#include "library/lib_index.h"

typedef uint8_t Lib_sort_key;
#define LIB_SORT_KEY_NAME				(Lib_sort_key)(0)	//Natural, case-insensitive title order (S01E2 before S01E10).
#define LIB_SORT_KEY_DATE_ADDED		(Lib_sort_key)(1)
#define LIB_SORT_KEY_LAST_PLAYED		(Lib_sort_key)(2)
#define LIB_SORT_KEY_DURATION			(Lib_sort_key)(3)
#define LIB_SORT_KEY_SIZE				(Lib_sort_key)(4)

typedef struct
{
	Lib_sort_key key;
	bool descending;
} Lib_sort_spec;

//Natural, case-insensitive comparison used by LIB_SORT_KEY_NAME.
int Lib_sort_natural_compare(const char* a, const char* b);

int Lib_sort_compare_items(const Lib_item* a, const Lib_item* b, const Lib_sort_spec* spec);

//Stable sort. Returns false on allocation failure (items are left untouched).
bool Lib_sort_items(Lib_item* items, size_t count, const Lib_sort_spec* spec);

//Stable sort of pointers (used by the library views).
bool Lib_sort_const_ptrs(const Lib_item** items, size_t count, const Lib_sort_spec* spec);

#endif //!defined(DEF_LIB_SORT_H)
