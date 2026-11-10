#if !defined(DEF_LIB_UI_H)
#define DEF_LIB_UI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_index.h"
#include "library/lib_sort.h"
#include "library/lib_view.h"

#define LIB_UI_MAX_ROWS					(uint8_t)(8)

//Pure navigation state for the library screen.
//Rendering and input translation live in the 3DS adapter; this part is
//host-testable and only depends on the index/view modules.
typedef struct
{
	Lib_view_tab tab;
	Lib_sort_spec sort;
	uint64_t* ids;					//Owned, sorted ids of the current tab.
	size_t count;
	size_t capacity;
	size_t cursor;					//Index into ids.
	size_t offset;					//First visible index.
	uint8_t rows;					//Visible rows (1..LIB_UI_MAX_ROWS).
	bool needs_rebuild;
} Lib_ui;

void Lib_ui_init(Lib_ui* ui);

void Lib_ui_free(Lib_ui* ui);

//Sets rows per screen. Returns false for 0 or > LIB_UI_MAX_ROWS.
bool Lib_ui_set_rows(Lib_ui* ui, uint8_t rows);

//Marks the id list as stale; the next update rebuilds it.
void Lib_ui_invalidate(Lib_ui* ui);

//Rebuilds the id list for the current tab/sort, keeping the selected id when
//it still exists. Returns false only on allocation failure.
bool Lib_ui_rebuild(Lib_ui* ui, const Lib_index* index);

//Convenience: set tab/sort and rebuild.
bool Lib_ui_set_tab(Lib_ui* ui, Lib_view_tab tab, const Lib_index* index);

bool Lib_ui_set_sort(Lib_ui* ui, Lib_sort_key key, bool descending, const Lib_index* index);

//Moves the cursor by delta (clamped). Keeps the cursor visible.
void Lib_ui_move(Lib_ui* ui, int32_t delta);

//Moves by one screen (rows - 1, like a page down).
void Lib_ui_page(Lib_ui* ui, bool forward);

//Selects a visible row (0 = first drawn row). Returns false when out of range.
bool Lib_ui_select_visible(Lib_ui* ui, size_t row);

//Selected item, or NULL when the list is empty/the id disappeared.
const Lib_item* Lib_ui_current(const Lib_ui* ui, const Lib_index* index);

//Id of a visible row. Returns false when out of range.
bool Lib_ui_id_at_visible(const Lib_ui* ui, size_t row, uint64_t* id);

#endif //!defined(DEF_LIB_UI_H)
