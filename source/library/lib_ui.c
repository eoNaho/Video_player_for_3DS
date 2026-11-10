#include "library/lib_ui.h"

#include <stdlib.h>
#include <string.h>

void Lib_ui_init(Lib_ui* ui)
{
	if (!ui)
		return;

	memset(ui, 0, sizeof(*ui));
	ui->tab = LIB_VIEW_TAB_ALL;
	ui->sort.key = LIB_SORT_KEY_NAME;
	ui->sort.descending = false;
	ui->rows = 6;
	ui->needs_rebuild = true;
}

void Lib_ui_free(Lib_ui* ui)
{
	if (!ui)
		return;

	free(ui->ids);
	memset(ui, 0, sizeof(*ui));
}

bool Lib_ui_set_rows(Lib_ui* ui, uint8_t rows)
{
	if (!ui || rows == 0 || rows > LIB_UI_MAX_ROWS)
		return false;

	ui->rows = rows;
	Lib_ui_move(ui, 0);
	return true;
}

void Lib_ui_invalidate(Lib_ui* ui)
{
	if (ui)
		ui->needs_rebuild = true;
}

static bool Lib_ui_reserve(Lib_ui* ui, size_t needed)
{
	uint64_t* ids = NULL;
	size_t new_capacity = 0;

	if (needed <= ui->capacity)
		return true;

	new_capacity = (ui->capacity == 0) ? 32 : ui->capacity;
	while (new_capacity < needed)
	{
		if (new_capacity > (SIZE_MAX / 2))
			return false;
		new_capacity *= 2;
	}

	if (new_capacity > (SIZE_MAX / sizeof(uint64_t)))
		return false;

	ids = (uint64_t*)realloc(ui->ids, new_capacity * sizeof(uint64_t));
	if (!ids)
		return false;

	ui->ids = ids;
	ui->capacity = new_capacity;
	return true;
}

bool Lib_ui_rebuild(Lib_ui* ui, const Lib_index* index)
{
	uint64_t selected_id = 0;
	uint64_t* ids = NULL;
	size_t count = 0;
	bool had_selection = false;

	if (!ui || !index)
		return false;

	if (index->count > (SIZE_MAX / sizeof(uint64_t)))
		return false;

	if (ui->ids && ui->cursor < ui->count)
	{
		selected_id = ui->ids[ui->cursor];
		had_selection = true;
	}

	ids = (uint64_t*)malloc(((index->count > 0) ? index->count : 1) * sizeof(uint64_t));
	if (!ids)
		return false;

	if (!Lib_view_build(index, ui->tab, &ui->sort, ids, index->count, &count))
	{
		free(ids);
		return false;
	}

	if (!Lib_ui_reserve(ui, count))
	{
		free(ids);
		return false;
	}

	if (count > 0)
		memcpy(ui->ids, ids, count * sizeof(uint64_t));
	free(ids);

	ui->count = count;
	ui->needs_rebuild = false;
	if (count == 0)
	{
		ui->cursor = 0;
		ui->offset = 0;
		return true;
	}

	if (had_selection)
	{
		for (size_t i = 0; i < count; i++)
		{
			if (ui->ids[i] == selected_id)
			{
				ui->cursor = i;
				break;
			}
		}
	}

	if (ui->cursor >= count)
		ui->cursor = count - 1;

	if (ui->offset >= count)
		ui->offset = (count > ui->rows) ? (count - ui->rows) : 0;

	Lib_ui_move(ui, 0);
	return true;
}

bool Lib_ui_set_tab(Lib_ui* ui, Lib_view_tab tab, const Lib_index* index)
{
	if (!ui)
		return false;

	if (ui->tab != tab)
	{
		ui->tab = tab;
		ui->cursor = 0;
		ui->offset = 0;
	}

	return Lib_ui_rebuild(ui, index);
}

bool Lib_ui_set_sort(Lib_ui* ui, Lib_sort_key key, bool descending, const Lib_index* index)
{
	if (!ui)
		return false;

	if (ui->sort.key != key || ui->sort.descending != descending)
	{
		ui->sort.key = key;
		ui->sort.descending = descending;
		ui->cursor = 0;
		ui->offset = 0;
	}

	return Lib_ui_rebuild(ui, index);
}

void Lib_ui_move(Lib_ui* ui, int32_t delta)
{
	size_t position = 0;

	if (!ui)
		return;

	if (ui->count == 0)
	{
		ui->cursor = 0;
		ui->offset = 0;
		return;
	}

	position = ui->cursor;
	if (position >= ui->count)
		position = ui->count - 1;

	if (delta < 0)
	{
		size_t amount = (size_t)(-(delta + 1)) + 1;
		position = (position > amount) ? (position - amount) : 0;
	}
	else if (delta > 0)
	{
		size_t amount = (size_t)delta;
		size_t last = ui->count - 1;

		position = (amount > (last - position)) ? last : (position + amount);
	}

	ui->cursor = position;

	if (ui->cursor < ui->offset)
		ui->offset = ui->cursor;
	else if (ui->cursor >= (ui->offset + ui->rows))
		ui->offset = (ui->cursor - ui->rows) + 1;

	if (ui->offset > ui->count - 1)
		ui->offset = ui->count - 1;
}

void Lib_ui_page(Lib_ui* ui, bool forward)
{
	size_t amount = 0;
	size_t last = 0;

	if (!ui)
		return;

	if (ui->count == 0)
	{
		ui->cursor = 0;
		ui->offset = 0;
		return;
	}

	amount = (ui->rows > 1) ? (size_t)(ui->rows - 1) : 1;
	last = ui->count - 1;

	if (forward)
	{
		ui->cursor = (amount > (last - ui->cursor)) ? last : (ui->cursor + amount);
		ui->offset = (ui->cursor + 1 > ui->rows) ? ((ui->cursor + 1) - ui->rows) : 0;
		if (ui->offset > last)
			ui->offset = last;
	}
	else
	{
		ui->cursor = (ui->cursor > amount) ? (ui->cursor - amount) : 0;
		ui->offset = (ui->offset > amount) ? (ui->offset - amount) : 0;
	}
}

bool Lib_ui_select_visible(Lib_ui* ui, size_t row)
{
	size_t index = 0;

	if (!ui || row >= ui->rows)
		return false;

	index = ui->offset + row;
	if (index >= ui->count)
		return false;

	ui->cursor = index;
	return true;
}

const Lib_item* Lib_ui_current(const Lib_ui* ui, const Lib_index* index)
{
	uint64_t id = 0;

	if (!ui || !index || ui->count == 0 || ui->cursor >= ui->count)
		return NULL;

	id = ui->ids[ui->cursor];
	return Lib_index_find_id_const(index, id);
}

bool Lib_ui_id_at_visible(const Lib_ui* ui, size_t row, uint64_t* id)
{
	size_t index = 0;

	if (!ui || !id || row >= ui->rows)
		return false;

	index = ui->offset + row;
	if (index >= ui->count)
		return false;

	*id = ui->ids[index];
	return true;
}
