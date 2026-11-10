#include "library/lib_view.h"

#include <stdlib.h>

static bool Lib_view_matches(const Lib_item* item, Lib_view_tab tab)
{
	if (!item)
		return false;

	switch (tab)
	{
		case LIB_VIEW_TAB_CONTINUE:
			return (bool)(item->position_ms > 0
			&& !(item->flags & LIB_ITEM_FLAG_WATCHED)
			&& !(item->flags & LIB_ITEM_FLAG_MISSING)
			&& !(item->flags & LIB_ITEM_FLAG_UNSUPPORTED));

		case LIB_VIEW_TAB_RECENT:
			return (bool)(item->last_played > 0 && !(item->flags & LIB_ITEM_FLAG_MISSING));

		case LIB_VIEW_TAB_FAVORITES:
			return (bool)((item->flags & LIB_ITEM_FLAG_FAVORITE) != 0);

		case LIB_VIEW_TAB_ALL:
		default:
			return true;
	}
}

bool Lib_view_build(const Lib_index* index, Lib_view_tab tab, const Lib_sort_spec* spec,
uint64_t* ids, size_t capacity, size_t* out_count)
{
	const Lib_item** items = NULL;
	Lib_sort_spec default_spec = { LIB_SORT_KEY_NAME, false };
	size_t count = 0;
	size_t written = 0;

	if (out_count)
		*out_count = 0;
	if (!index || (!ids && capacity > 0))
		return false;

	if (index->count == 0)
		return true;

	if (index->count > (SIZE_MAX / sizeof(const Lib_item*)))
		return false;

	items = (const Lib_item**)malloc(index->count * sizeof(const Lib_item*));
	if (!items)
		return false;

	for (size_t i = 0; i < index->count; i++)
	{
		if (Lib_view_matches(&index->items[i], tab))
			items[count++] = &index->items[i];
	}

	if (!spec)
	{
		if (tab == LIB_VIEW_TAB_CONTINUE || tab == LIB_VIEW_TAB_RECENT)
		{
			default_spec.key = LIB_SORT_KEY_LAST_PLAYED;
			default_spec.descending = true;
		}
		else
		{
			default_spec.key = LIB_SORT_KEY_NAME;
			default_spec.descending = false;
		}
		spec = &default_spec;
	}

	if (!Lib_sort_const_ptrs(items, count, spec))
	{
		free(items);
		return false;
	}

	written = (count < capacity) ? count : capacity;
	for (size_t i = 0; i < written; i++)
		ids[i] = items[i]->id;

	free(items);

	if (out_count)
		*out_count = count;
	return true;
}
