#include "library/lib_ui.h"

#include "library/lib_name.h"
#include "library/lib_scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIB_UI_GRID_COLUMNS				(size_t)(2)

static uint32_t Lib_ui_fold_codepoint(uint32_t codepoint)
{
	if (codepoint >= 'A' && codepoint <= 'Z')
		return codepoint + ('a' - 'A');
	if ((codepoint >= 0x00C0 && codepoint <= 0x00D6)
	|| (codepoint >= 0x00D8 && codepoint <= 0x00DE))
		return codepoint + 0x20;
	if (codepoint == 0x0178)
		return 0x00FF;
	return codepoint;
}

static uint32_t Lib_ui_next_codepoint(const char** text)
{
	const unsigned char* bytes = (const unsigned char*)*text;
	uint32_t codepoint = 0;
	uint8_t extra = 0;

	if (bytes[0] < 0x80)
	{
		(*text)++;
		return bytes[0];
	}
	if ((bytes[0] & 0xE0) == 0xC0)
	{
		codepoint = bytes[0] & 0x1F;
		extra = 1;
	}
	else if ((bytes[0] & 0xF0) == 0xE0)
	{
		codepoint = bytes[0] & 0x0F;
		extra = 2;
	}
	else if ((bytes[0] & 0xF8) == 0xF0)
	{
		codepoint = bytes[0] & 0x07;
		extra = 3;
	}
	else
	{
		(*text)++;
		return bytes[0];
	}

	for (uint8_t i = 1; i <= extra; i++)
	{
		if (bytes[i] == 0x00 || (bytes[i] & 0xC0) != 0x80)
		{
			(*text)++;
			return bytes[0];
		}
		codepoint = (codepoint << 6) | (bytes[i] & 0x3F);
	}
	*text += extra + 1;
	return codepoint;
}

static bool Lib_ui_title_matches(const char* title, const char* search)
{
	if (!title || !search)
		return false;
	if (search[0] == 0x00)
		return true;

	for (const char* start = title; *start; )
	{
		const char* text = start;
		const char* query = search;
		bool matches = true;
		while (*text && *query)
		{
			uint32_t text_codepoint = Lib_ui_fold_codepoint(Lib_ui_next_codepoint(&text));
			uint32_t query_codepoint = Lib_ui_fold_codepoint(Lib_ui_next_codepoint(&query));
			if (text_codepoint != query_codepoint)
			{
				matches = false;
				break;
			}
		}
		if (matches && *query == 0x00)
			return true;
		(void)Lib_ui_next_codepoint(&start);
	}
	return false;
}

static bool Lib_ui_title_equal(const char* a, const char* b)
{
	if (!a || !b)
		return false;
	while (*a && *b)
	{
		if (Lib_ui_fold_codepoint(Lib_ui_next_codepoint(&a))
		!= Lib_ui_fold_codepoint(Lib_ui_next_codepoint(&b)))
			return false;
	}
	return (bool)(*a == 0x00 && *b == 0x00);
}

static bool Lib_ui_item_matches_filter(Lib_ui_filter filter, const Lib_item* item)
{
	char title[256] = { 0, };
	Lib_name_info info = { -1, -1, -1, false, false };

	if (filter == LIB_UI_FILTER_ALL)
		return true;
	if (!item || !item->path || (item->flags & LIB_ITEM_FLAG_AUDIO_ONLY)
	|| Lib_scan_classify(item->path) != LIB_SCAN_KIND_VIDEO)
		return false;
	if (filter == LIB_UI_FILTER_VIDEO)
		return true;
	if (filter != LIB_UI_FILTER_MOVIES && filter != LIB_UI_FILTER_SERIES)
		return false;
	if (!Lib_name_parse_info(item->path, title, sizeof(title), NULL, 0, &info))
		return filter == LIB_UI_FILTER_MOVIES;
	return (filter == LIB_UI_FILTER_SERIES) ? info.is_series : !info.is_series;
}

void Lib_ui_init(Lib_ui* ui)
{
	if (!ui)
		return;

	memset(ui, 0, sizeof(*ui));
	ui->tab = LIB_VIEW_TAB_ALL;
	ui->sort.key = LIB_SORT_KEY_NAME;
	ui->sort.descending = false;
	ui->rows = 6;
	ui->level = LIB_UI_LEVEL_ROOT;
	ui->group_season = -1;
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
	Lib_sort_spec effective_sort = { LIB_SORT_KEY_NAME, false };
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

	effective_sort = ui->sort;
	if (ui->filter == LIB_UI_FILTER_SERIES && ui->level != LIB_UI_LEVEL_ROOT)
	{
		effective_sort.key = LIB_SORT_KEY_NAME;
		effective_sort.descending = false;
	}
	if (!Lib_view_build(index, ui->tab, &effective_sort, ids, index->count, &count))
	{
		free(ids);
		return false;
	}

	{
		size_t filtered_count = 0;
		for (size_t i = 0; i < count; i++)
		{
			const Lib_item* item = Lib_index_find_id_const(index, ids[i]);
			char parsed_title[256] = { 0, };
			Lib_name_info info = { -1, -1, -1, false, false };
			bool duplicate = false;
			if (!item || !Lib_ui_item_matches_filter(ui->filter, item))
				continue;

			if (ui->filter == LIB_UI_FILTER_SERIES)
			{
				if (!Lib_name_parse_info(item->path, parsed_title, sizeof(parsed_title), NULL, 0, &info)
				|| !info.is_series)
					continue;
				if (ui->level != LIB_UI_LEVEL_ROOT
				&& !Lib_ui_title_equal(parsed_title, ui->group_title))
					continue;
				if (ui->level == LIB_UI_LEVEL_EPISODES
				&& (info.season > 0 ? info.season : 1) != ui->group_season)
					continue;
				if (!Lib_ui_title_matches(parsed_title, ui->search))
					continue;

				if (ui->level == LIB_UI_LEVEL_ROOT)
				{
					for (size_t j = 0; j < filtered_count; j++)
					{
						const Lib_item* previous = Lib_index_find_id_const(index, ids[j]);
						char previous_title[256] = { 0, };
						if (previous && Lib_name_parse_info(previous->path, previous_title,
						sizeof(previous_title), NULL, 0, NULL)
						&& Lib_ui_title_equal(parsed_title, previous_title))
						{
							duplicate = true;
						break;
						}
					}
				}
				else if (ui->level == LIB_UI_LEVEL_SEASONS)
				{
					int season = (info.season > 0) ? info.season : 1;
					for (size_t j = 0; j < filtered_count; j++)
					{
						const Lib_item* previous = Lib_index_find_id_const(index, ids[j]);
						Lib_name_info previous_info = { -1, -1, -1, false, false };
						if (previous && Lib_name_parse_info(previous->path, parsed_title,
						sizeof(parsed_title), NULL, 0, &previous_info)
						&& (previous_info.season > 0 ? previous_info.season : 1) == season)
						{
							duplicate = true;
						break;
						}
					}
				}
			}
			else if (!Lib_ui_title_matches(Lib_item_get_title(item), ui->search))
				continue;
			if (!duplicate)
				ids[filtered_count++] = ids[i];
		}
		count = filtered_count;
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
		ui->level = LIB_UI_LEVEL_ROOT;
		ui->group_title[0] = 0x00;
		ui->group_season = -1;
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

bool Lib_ui_set_search(Lib_ui* ui, const char* search, const Lib_index* index)
{
	size_t length = 0;

	if (!ui || !index)
		return false;
	if (!search)
		search = "";

	while (length < (LIB_UI_SEARCH_MAX - 1) && search[length] != 0x00)
		length++;
	if (length > 0)
		memcpy(ui->search, search, length);
	ui->search[length] = 0x00;
	ui->cursor = 0;
	ui->offset = 0;
	return Lib_ui_rebuild(ui, index);
}

void Lib_ui_set_filter(Lib_ui* ui, Lib_ui_filter filter)
{
	if (!ui || filter > LIB_UI_FILTER_SERIES)
		return;
	if (ui->filter == filter)
	{
		if (ui->level == LIB_UI_LEVEL_ROOT)
			return;
		ui->level = LIB_UI_LEVEL_ROOT;
		ui->group_title[0] = 0x00;
		ui->group_season = -1;
		ui->cursor = 0;
		ui->offset = 0;
		Lib_ui_invalidate(ui);
		return;
	}
	ui->filter = filter;
	ui->level = LIB_UI_LEVEL_ROOT;
	ui->group_title[0] = 0x00;
	ui->group_season = -1;
	ui->cursor = 0;
	ui->offset = 0;
	Lib_ui_invalidate(ui);
}

bool Lib_ui_enter(Lib_ui* ui, const Lib_index* index)
{
	const Lib_item* item = NULL;
	char title[sizeof(ui->group_title)] = { 0, };
	Lib_name_info info = { -1, -1, -1, false, false };
	if (!ui || !index || ui->filter != LIB_UI_FILTER_SERIES)
		return false;
	item = Lib_ui_current(ui, index);
	if (!item || !Lib_name_parse_info(item->path, title, sizeof(title), NULL, 0, &info)
	|| !info.is_series)
		return false;

	if (ui->level == LIB_UI_LEVEL_ROOT)
	{
		snprintf(ui->group_title, sizeof(ui->group_title), "%s", title);
		ui->group_season = -1;
		ui->level = LIB_UI_LEVEL_SEASONS;
	}
	else if (ui->level == LIB_UI_LEVEL_SEASONS)
	{
		ui->group_season = (info.season > 0) ? info.season : 1;
		ui->level = LIB_UI_LEVEL_EPISODES;
	}
	else
		return false;

	ui->cursor = 0;
	ui->offset = 0;
	ui->count = 0;
	ui->needs_rebuild = true;
	return Lib_ui_rebuild(ui, index);
}

bool Lib_ui_back(Lib_ui* ui, const Lib_index* index)
{
	Lib_ui_level previous_level = LIB_UI_LEVEL_ROOT;
	char previous_group[256] = { 0, };
	if (!ui || !index || ui->filter != LIB_UI_FILTER_SERIES || ui->level == LIB_UI_LEVEL_ROOT)
		return false;
	previous_level = ui->level;
	snprintf(previous_group, sizeof(previous_group), "%s", ui->group_title);
	if (ui->level == LIB_UI_LEVEL_EPISODES)
		ui->level = LIB_UI_LEVEL_SEASONS;
	else
	{
		ui->level = LIB_UI_LEVEL_ROOT;
		ui->group_title[0] = 0x00;
		ui->group_season = -1;
	}
	ui->cursor = 0;
	ui->offset = 0;
	ui->count = 0;
	ui->needs_rebuild = true;
	if (!Lib_ui_rebuild(ui, index))
		return false;

	if (previous_level == LIB_UI_LEVEL_EPISODES)
	{
		for (size_t i = 0; i < ui->count; i++)
		{
			const Lib_item* item = Lib_index_find_id_const(index, ui->ids[i]);
			Lib_name_info info = { -1, -1, -1, false, false };
			char title[sizeof(ui->group_title)] = { 0, };
			if (item && Lib_name_parse_info(item->path, title, sizeof(title), NULL, 0, &info)
			&& (info.season > 0 ? info.season : 1) == ui->group_season)
			{
				ui->cursor = i;
				break;
			}
		}
	}
	else
	{
		for (size_t i = 0; i < ui->count; i++)
		{
			const Lib_item* item = Lib_index_find_id_const(index, ui->ids[i]);
			char title[sizeof(ui->group_title)] = { 0, };
			if (item && Lib_name_parse_info(item->path, title, sizeof(title), NULL, 0, NULL)
			&& Lib_ui_title_equal(title, previous_group))
			{
				ui->cursor = i;
				break;
			}
		}
	}
	Lib_ui_move(ui, 0);
	return true;
}

void Lib_ui_set_grid_mode(Lib_ui* ui, bool enabled)
{
	if (!ui || ui->grid_mode == enabled)
		return;

	ui->grid_mode = enabled;
	Lib_ui_move(ui, 0);
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
	if (ui->grid_mode)
	{
		ui->offset = ((ui->offset + 1) / LIB_UI_GRID_COLUMNS) * LIB_UI_GRID_COLUMNS;
		if (ui->cursor < ui->offset)
			ui->offset = (ui->cursor / LIB_UI_GRID_COLUMNS) * LIB_UI_GRID_COLUMNS;
	}
}

void Lib_ui_move_direction(Lib_ui* ui, int32_t dx, int32_t dy)
{
	size_t row = 0;
	size_t column = 0;
	size_t target = 0;
	size_t last = 0;

	if (!ui || ui->count == 0)
		return;
	if (!ui->grid_mode)
	{
		if (dy != 0)
			Lib_ui_move(ui, dy);
		return;
	}

	if (ui->cursor >= ui->count)
		ui->cursor = ui->count - 1;
	row = ui->cursor / LIB_UI_GRID_COLUMNS;
	column = ui->cursor % LIB_UI_GRID_COLUMNS;
	last = ui->count - 1;
	if (dx != 0)
	{
		if ((dx < 0 && column == 0) || (dx > 0 && column + 1 >= LIB_UI_GRID_COLUMNS))
			return;
		target = (size_t)((int32_t)ui->cursor + ((dx < 0) ? -1 : 1));
		if (target <= last)
			Lib_ui_move(ui, (dx < 0) ? -1 : 1);
		return;
	}
	if (dy == 0)
		return;
	if (dy < 0 && row == 0)
		return;
	row = (size_t)((int32_t)row + ((dy < 0) ? -1 : 1));
	target = row * LIB_UI_GRID_COLUMNS + column;
	if (target > last)
		target = last;
	Lib_ui_move(ui, (int32_t)target - (int32_t)ui->cursor);
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
