#if !defined(DEF_LIB_VIEW_H)
#define DEF_LIB_VIEW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_index.h"
#include "library/lib_sort.h"

typedef uint8_t Lib_view_tab;
#define LIB_VIEW_TAB_CONTINUE			(Lib_view_tab)(0)	//Started but not finished.
#define LIB_VIEW_TAB_RECENT				(Lib_view_tab)(1)	//Played at least once.
#define LIB_VIEW_TAB_ALL				(Lib_view_tab)(2)
#define LIB_VIEW_TAB_FAVORITES			(Lib_view_tab)(3)

//Builds the id list for a tab.
//ids may be NULL when capacity is 0 (useful to count matches).
//out_count receives the number of matching items (may exceed capacity).
//When spec is NULL, each tab uses its natural order.
bool Lib_view_build(const Lib_index* index, Lib_view_tab tab, const Lib_sort_spec* spec,
uint64_t* ids, size_t capacity, size_t* out_count);

#endif //!defined(DEF_LIB_VIEW_H)
