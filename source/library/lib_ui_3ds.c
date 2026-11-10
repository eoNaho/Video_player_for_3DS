#include "library/lib_ui_3ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "library/lib_library_3ds.h"
#include "library/lib_name.h"
#include "library/lib_thumb.h"
#include "library/lib_ui.h"
#include "system/draw/draw.h"
#include "system/draw/theme.h"
#include "system/util/decoder.h"
#include "system/util/err_types.h"
#include "system/util/hid.h"
#include "system/util/keyboard.h"
#include "system/util/str.h"
#include "system/util/sync.h"

#define LIB_UI_3DS_VISIBLE_ITEMS		(uint8_t)(8)
#define LIB_UI_3DS_ROWS					(uint8_t)(6)
#define LIB_UI_3DS_GRID_VISIBLE_ITEMS	(size_t)(4)
#define LIB_UI_3DS_GRID_COLUMNS			(size_t)(2)
#define LIB_UI_3DS_LIST_ROW_HEIGHT		(float)(28)
#define LIB_UI_3DS_LIST_ROW_X			(float)(4)
#define LIB_UI_3DS_LIST_ROW_Y			(float)(22)
#define LIB_UI_3DS_LIST_ROW_WIDTH		(float)(312)
#define LIB_UI_3DS_GRID_CELL_WIDTH		(float)(156)
#define LIB_UI_3DS_GRID_CELL_HEIGHT		(float)(82)
#define LIB_UI_3DS_GRID_X				(float)(4)
#define LIB_UI_3DS_GRID_Y				(float)(22)
#define LIB_UI_3DS_BUTTON_ROW_HEIGHT		UI_THEME_TOUCH_MIN
#define LIB_UI_3DS_RAIL_WIDTH			(float)(3)
#define LIB_UI_3DS_BUTTON_ROW_0_Y		(float)(192)
#define LIB_UI_3DS_BUTTON_ROW_1_Y		(float)(216)
#define LIB_UI_3DS_THUMB_WIDTH			(uint32_t)(128)
#define LIB_UI_3DS_THUMB_HEIGHT			(uint32_t)(72)
#define LIB_UI_3DS_THUMB_MAX_FILE_SIZE	(uint32_t)(524288)
#define LIB_UI_3DS_THUMB_MAX_SOURCE_PIXELS	(uint64_t)(4 * 1024 * 1024)
#define LIB_UI_3DS_SEARCH_CHARS			(uint32_t)(48)

typedef struct
{
	Draw_image_data image;
	uint64_t id;
	uint64_t last_used;
	uint16_t width;
	uint16_t height;
	bool loaded;
} Lib_ui_3ds_thumb;

static Lib_ui library_ui = { 0, };
static Lib_ui_3ds_thumb library_thumb_cache[LIB_UI_3DS_VISIBLE_ITEMS] = { 0, };
static uint64_t library_thumb_clock = 0;
static bool library_ui_inited = false;
static bool library_ui_open = false;
static bool library_keep_paused_after_play = false;
static bool library_ui_dark_theme = true;
static bool library_queue_view = false;
static Lib_queue* library_queue = NULL;
static Sync_data* library_queue_lock = NULL;
static size_t library_queue_cursor = 0;
static size_t library_queue_offset = 0;
static bool library_is_portuguese = false;
static Lib_ui_3ds_queue_change_callback library_queue_change_callback = NULL;

#define LIB_UI_3DS_COLOR_BACKGROUND	(library_ui_dark_theme ? UI_THEME_COLOR_BACKGROUND : UI_THEME_COLOR_LIGHT_BACKGROUND)
#define LIB_UI_3DS_COLOR_SURFACE		(library_ui_dark_theme ? UI_THEME_COLOR_SURFACE : UI_THEME_COLOR_LIGHT_SURFACE)
#define LIB_UI_3DS_COLOR_SURFACE_ALT	(library_ui_dark_theme ? UI_THEME_COLOR_SURFACE_ALT : UI_THEME_COLOR_LIGHT_SURFACE_ALT)
#define LIB_UI_3DS_COLOR_ACCENT		(library_ui_dark_theme ? UI_THEME_COLOR_ACCENT : UI_THEME_COLOR_LIGHT_ACCENT)
#define LIB_UI_3DS_COLOR_TEXT			(library_ui_dark_theme ? UI_THEME_COLOR_TEXT : UI_THEME_COLOR_LIGHT_TEXT)
#define LIB_UI_3DS_COLOR_SECONDARY	(library_ui_dark_theme ? UI_THEME_COLOR_SECONDARY_TEXT : UI_THEME_COLOR_LIGHT_SECONDARY_TEXT)
#define LIB_UI_3DS_COLOR_RAIL_OFF		(library_ui_dark_theme ? UI_THEME_COLOR_RAIL_OFF : UI_THEME_COLOR_LIGHT_RAIL_OFF)

static void Lib_ui_3ds_request_redraw(void)
{
	Draw_set_refresh_needed(true);
}

static void Lib_ui_3ds_set_grid(bool enabled)
{
	Lib_ui_set_grid_mode(&library_ui, enabled);
	(void)Lib_ui_set_rows(&library_ui, enabled
	? (uint8_t)LIB_UI_3DS_GRID_VISIBLE_ITEMS : LIB_UI_3DS_ROWS);
	Lib_ui_3ds_request_redraw();
}

static bool Lib_ui_3ds_queue_lock(void)
{
	return (bool)(library_queue_lock && Util_sync_lock(library_queue_lock, UINT64_MAX) == DEF_SUCCESS);
}

static void Lib_ui_3ds_queue_unlock(bool locked)
{
	if (locked)
		Util_sync_unlock(library_queue_lock);
}

static const char* Lib_ui_3ds_text(const char* english, const char* portuguese)
{
	return library_is_portuguese ? portuguese : english;
}

static void Lib_ui_3ds_free_thumbnails(void)
{
	for (uint8_t i = 0; i < LIB_UI_3DS_VISIBLE_ITEMS; i++)
	{
		if (library_thumb_cache[i].loaded)
			Draw_texture_free(&library_thumb_cache[i].image);
		memset(&library_thumb_cache[i], 0, sizeof(library_thumb_cache[i]));
	}
	library_thumb_clock = 0;
}

void Lib_ui_3ds_init(void)
{
	if (library_ui_inited)
		return;

	Lib_ui_init(&library_ui);
	Lib_ui_set_filter(&library_ui, LIB_UI_FILTER_VIDEO);
	(void)Lib_ui_set_rows(&library_ui, LIB_UI_3DS_ROWS);
	library_ui_inited = true;
}

void Lib_ui_3ds_set_language(const char* language)
{
	bool is_portuguese = (bool)(language && strncmp(language, "pt", 2) == 0);
	if (library_is_portuguese != is_portuguese)
	{
		library_is_portuguese = is_portuguese;
		if (library_ui_open)
			Lib_ui_3ds_request_redraw();
	}
}

void Lib_ui_3ds_set_queue_change_callback(Lib_ui_3ds_queue_change_callback callback)
{
	library_queue_change_callback = callback;
}

void Lib_ui_3ds_exit(void)
{
	if (!library_ui_inited)
		return;

	Lib_ui_3ds_free_thumbnails();
	Lib_ui_free(&library_ui);
	library_queue = NULL;
	library_queue_lock = NULL;
	library_queue_change_callback = NULL;
	library_ui_inited = false;
	library_ui_open = false;
}

bool Lib_ui_3ds_is_open(void)
{
	return library_ui_open;
}

bool Lib_ui_3ds_open(const Lib_index* index, Lib_queue* queue, Sync_data* queue_lock,
bool keep_library_paused)
{
	if (!library_ui_inited || !Lib_library_is_initialized() || !index || Lib_library_is_busy()
	|| !Lib_ui_rebuild(&library_ui, index))
		return false;

	Lib_ui_3ds_free_thumbnails();
	library_queue = queue;
	library_queue_lock = queue_lock;
	library_keep_paused_after_play = keep_library_paused;
	library_queue_view = false;
	library_queue_cursor = 0;
	library_queue_offset = 0;
	library_ui_open = true;
	Lib_library_set_paused(true);
	Lib_ui_3ds_request_redraw();
	return true;
}

void Lib_ui_3ds_close(void)
{
	if (!library_ui_open)
		return;

	library_ui_open = false;
	Lib_ui_3ds_free_thumbnails();
	if (!library_keep_paused_after_play)
		Lib_library_set_paused(false);
	library_keep_paused_after_play = false;
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_rebuild(const Lib_index* index)
{
	if (index && library_ui.needs_rebuild)
		(void)Lib_ui_rebuild(&library_ui, index);
}

static void Lib_ui_3ds_open_search(const Lib_index* index)
{
	Str_data hint = { 0, };
	Str_data initial_text = { 0, };
	Str_data result_text = { 0, };
	bool hint_inited = false;
	bool initial_inited = false;
	bool result_inited = false;
	bool keyboard_inited = false;

	if (Util_str_init(&hint) != DEF_SUCCESS)
		goto cleanup;
	hint_inited = true;
	if (Util_str_init(&initial_text) != DEF_SUCCESS)
		goto cleanup;
	initial_inited = true;
	if (Util_str_init(&result_text) != DEF_SUCCESS)
		goto cleanup;
	result_inited = true;
	if (Util_str_set(&hint, Lib_ui_3ds_text("Search titles", "Buscar títulos")) != DEF_SUCCESS
	|| Util_str_set(&initial_text, library_ui.search) != DEF_SUCCESS)
		goto cleanup;

	if (Util_keyboard_init(KEYBOARD_TYPE_QWERTY, KEYBOARD_ACCEPTABLE_INPUT_NO_EMPTY,
	KEYBOARD_DISPLAY_BUTTON_LEFT_MIDDLE, LIB_UI_3DS_SEARCH_CHARS, &hint, &initial_text,
	KEYBOARD_PASSWORD_MODE_OFF, KEYBOARD_FEATURES_BIT_NONE) != DEF_SUCCESS)
		goto cleanup;
	keyboard_inited = true;
	if (Util_keyboard_launch(&result_text, NULL) == DEF_SUCCESS && result_text.buffer)
		(void)Lib_ui_set_search(&library_ui, result_text.buffer, index);

	cleanup:
	if (keyboard_inited)
		Util_keyboard_exit();
	if (result_inited)
		Util_str_free(&result_text);
	if (initial_inited)
		Util_str_free(&initial_text);
	if (hint_inited)
		Util_str_free(&hint);
	Lib_ui_3ds_request_redraw();
}

static size_t Lib_ui_3ds_queue_count(void)
{
	size_t count = 0;
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue)
		count = Lib_queue_count(library_queue);
	Lib_ui_3ds_queue_unlock(locked);
	return count;
}

static bool Lib_ui_3ds_queue_get(size_t position, uint64_t* id)
{
	bool found = false;
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue)
		found = Lib_queue_get_at(library_queue, position, id);
	Lib_ui_3ds_queue_unlock(locked);
	return found;
}

static void Lib_ui_3ds_queue_add_current(const Lib_index* index)
{
	const Lib_item* item = Lib_ui_current(&library_ui, index);
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue && item)
	{
		bool was_present = Lib_queue_contains(library_queue, item->id);
		if (!was_present && Lib_queue_add(library_queue, item->id) && library_queue_change_callback)
			library_queue_change_callback();
	}
	Lib_ui_3ds_queue_unlock(locked);
	Lib_ui_3ds_request_redraw();
}

static const Lib_item* Lib_ui_3ds_queue_current(const Lib_index* index)
{
	uint64_t id = 0;
	if (!index || !Lib_ui_3ds_queue_get(library_queue_cursor, &id))
		return NULL;
	return Lib_index_find_id_const(index, id);
}

static void Lib_ui_3ds_play_item(const Lib_item* item, Lib_ui_3ds_play_callback play_callback)
{
	if (!item || !play_callback)
		return;
	play_callback(item);
	library_keep_paused_after_play = true;
	Lib_ui_3ds_close();
}

static void Lib_ui_3ds_queue_play_current(const Lib_index* index,
Lib_ui_3ds_play_callback play_callback)
{
	const Lib_item* item = Lib_ui_3ds_queue_current(index);
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue && item)
		(void)Lib_queue_set_cursor(library_queue, item->id);
	Lib_ui_3ds_queue_unlock(locked);
	Lib_ui_3ds_play_item(item, play_callback);
}

static void Lib_ui_3ds_queue_remove_current(void)
{
	uint64_t id = 0;
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue && Lib_queue_get_at(library_queue, library_queue_cursor, &id))
	{
		if (Lib_queue_remove(library_queue, id) && library_queue_change_callback)
			library_queue_change_callback();
		if (library_queue_cursor > 0)
			library_queue_cursor--;
		if (library_queue_offset > library_queue_cursor)
			library_queue_offset = library_queue_cursor;
	}
	Lib_ui_3ds_queue_unlock(locked);
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_queue_clear(void)
{
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue)
	{
		if (Lib_queue_set(library_queue, NULL, 0) && library_queue_change_callback)
			library_queue_change_callback();
	}
	Lib_ui_3ds_queue_unlock(locked);
	library_queue_cursor = 0;
	library_queue_offset = 0;
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_queue_set_mode(Lib_queue_mode mode)
{
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue)
		Lib_queue_set_mode(library_queue, mode);
	Lib_ui_3ds_queue_unlock(locked);
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_play_browser_current(const Lib_index* index,
Lib_ui_3ds_play_callback play_callback)
{
	Lib_ui_3ds_play_item(Lib_ui_current(&library_ui, index), play_callback);
}

static void Lib_ui_3ds_show_tab(Lib_view_tab tab, const Lib_index* index)
{
	Lib_ui_set_filter(&library_ui, LIB_UI_FILTER_VIDEO);
	(void)Lib_ui_set_tab(&library_ui, tab, index);
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_show_filter(Lib_ui_filter filter, const Lib_index* index)
{
	Lib_ui_set_filter(&library_ui, filter);
	(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_ALL, index);
	Lib_ui_3ds_request_redraw();
}

static void Lib_ui_3ds_toggle_favorite(const Lib_index* index)
{
	const Lib_item* item = Lib_ui_current(&library_ui, index);
	if (!item)
		return;
	if (Lib_library_set_favorite(item->id,
	(bool)((item->flags & LIB_ITEM_FLAG_FAVORITE) == 0)))
	{
		Lib_ui_invalidate(&library_ui);
		Lib_ui_3ds_rebuild(index);
		Lib_ui_3ds_request_redraw();
	}
}

static Lib_sort_key Lib_ui_3ds_next_sort(Lib_sort_key key, bool forward)
{
	if (forward)
		return (key >= LIB_SORT_KEY_SIZE) ? LIB_SORT_KEY_NAME : (Lib_sort_key)(key + 1);
	return (key == LIB_SORT_KEY_NAME) ? LIB_SORT_KEY_SIZE : (Lib_sort_key)(key - 1);
}

void Lib_ui_3ds_main(const Hid_info* key, const Lib_index* index,
Lib_ui_3ds_play_callback play_callback)
{
	bool touch_pressed = false;
	int16_t touch_x = 0;
	int16_t touch_y = 0;

	if (!library_ui_open || !key || !index)
		return;

	Lib_ui_3ds_rebuild(index);
	touch_pressed = DEF_HID_PR_EM(key->touch, 1);
	touch_x = key->touch_x_last;
	touch_y = key->touch_y_last;
	if (DEF_HID_PR_EM(key->b, 1) || DEF_HID_HD(key->b)
	|| DEF_HID_PR_EM(key->select, 1) || DEF_HID_HD(key->select))
	{
		Lib_ui_3ds_close();
		Util_hid_reset_key_state(HID_KEY_BIT_ALL);
		return;
	}

	if (touch_pressed)
	{
		if (library_queue_view)
		{
			if (touch_y >= LIB_UI_3DS_BUTTON_ROW_1_Y)
			{
				if (touch_x < 80)
					Lib_ui_3ds_queue_set_mode(LIB_QUEUE_MODE_NO_REPEAT);
				else if (touch_x < 160)
					Lib_ui_3ds_queue_set_mode(LIB_QUEUE_MODE_REPEAT);
				else if (touch_x < 240)
					Lib_ui_3ds_queue_set_mode(LIB_QUEUE_MODE_IN_ORDER);
				else
					Lib_ui_3ds_queue_set_mode(LIB_QUEUE_MODE_RANDOM);
			}
			else if (touch_y >= LIB_UI_3DS_BUTTON_ROW_0_Y)
			{
				if (touch_x < 80)
					library_queue_view = false;
				else if (touch_x < 160)
					Lib_ui_3ds_queue_remove_current();
				else if (touch_x < 240)
					Lib_ui_3ds_queue_clear();
				else
					Lib_ui_3ds_queue_play_current(index, play_callback);
			}
			else if (touch_y >= LIB_UI_3DS_LIST_ROW_Y)
			{
				size_t row = (size_t)((touch_y - LIB_UI_3DS_LIST_ROW_Y) / LIB_UI_3DS_LIST_ROW_HEIGHT);
				size_t target = library_queue_offset + row;
				if (row < LIB_UI_3DS_ROWS && target < Lib_ui_3ds_queue_count())
				{
					library_queue_cursor = target;
					Lib_ui_3ds_request_redraw();
				}
			}
		}
		else if (touch_y >= LIB_UI_3DS_BUTTON_ROW_1_Y)
		{
			if (touch_x < 54)
				library_queue_view = true;
			else if (touch_x < 107)
				Lib_ui_3ds_queue_add_current(index);
			else if (touch_x < 160)
				Lib_ui_3ds_play_browser_current(index, play_callback);
			else if (touch_x < 214)
				Lib_ui_3ds_open_search(index);
			else if (touch_x < 267)
				Lib_ui_3ds_set_grid(!library_ui.grid_mode);
			else
				Lib_ui_3ds_close();
		}
		else if (touch_y >= LIB_UI_3DS_BUTTON_ROW_0_Y)
		{
			if (touch_x < 64)
				Lib_ui_3ds_show_tab(LIB_VIEW_TAB_CONTINUE, index);
			else if (touch_x < 128)
				Lib_ui_3ds_show_tab(LIB_VIEW_TAB_RECENT, index);
			else if (touch_x < 192)
				Lib_ui_3ds_show_filter(LIB_UI_FILTER_MOVIES, index);
			else if (touch_x < 256)
				Lib_ui_3ds_show_filter(LIB_UI_FILTER_SERIES, index);
			else
				Lib_ui_3ds_show_tab(LIB_VIEW_TAB_FAVORITES, index);
			Lib_ui_3ds_request_redraw();
		}
		else if (touch_y >= LIB_UI_3DS_LIST_ROW_Y)
		{
			size_t row_height = library_ui.grid_mode
				? (size_t)LIB_UI_3DS_GRID_CELL_HEIGHT : (size_t)LIB_UI_3DS_LIST_ROW_HEIGHT;
			size_t row = (size_t)((touch_y - LIB_UI_3DS_LIST_ROW_Y) / row_height);
			size_t visible = 0;
			if (library_ui.grid_mode)
				visible = row * LIB_UI_3DS_GRID_COLUMNS + (size_t)(touch_x >= 160);
			else
				visible = row;
			if ((!library_ui.grid_mode || row < (LIB_UI_3DS_GRID_VISIBLE_ITEMS / LIB_UI_3DS_GRID_COLUMNS))
			&& Lib_ui_select_visible(&library_ui, visible))
				Lib_ui_3ds_request_redraw();
		}
		Lib_ui_3ds_request_redraw();
		return;
	}

	if (DEF_HID_PR_EM(key->d_up, 1) || DEF_HID_HD(key->d_up))
	{
		if (library_queue_view)
		{
			if (library_queue_cursor > 0)
				library_queue_cursor--;
			if (library_queue_cursor < library_queue_offset)
				library_queue_offset = library_queue_cursor;
		}
		else
			Lib_ui_move_direction(&library_ui, 0, -1);
		Lib_ui_3ds_request_redraw();
	}
	else if (DEF_HID_PR_EM(key->d_down, 1) || DEF_HID_HD(key->d_down))
	{
		if (library_queue_view)
		{
			size_t count = Lib_ui_3ds_queue_count();
			if (library_queue_cursor + 1 < count)
				library_queue_cursor++;
			if (library_queue_cursor >= library_queue_offset + LIB_UI_3DS_ROWS)
				library_queue_offset = library_queue_cursor - LIB_UI_3DS_ROWS + 1;
		}
		else
			Lib_ui_move_direction(&library_ui, 0, 1);
		Lib_ui_3ds_request_redraw();
	}
	else if (DEF_HID_PR_EM(key->d_left, 1) || DEF_HID_HD(key->d_left))
	{
		if (library_queue_view)
		{
			if (library_queue_cursor > 0)
				library_queue_cursor--;
			if (library_queue_cursor < library_queue_offset)
				library_queue_offset = library_queue_cursor;
		}
		else if (library_ui.grid_mode)
			Lib_ui_move_direction(&library_ui, -1, 0);
		else
			Lib_ui_3ds_show_filter(LIB_UI_FILTER_MOVIES, index);
	}
	else if (DEF_HID_PR_EM(key->d_right, 1) || DEF_HID_HD(key->d_right))
	{
		if (library_queue_view)
		{
			size_t count = Lib_ui_3ds_queue_count();
			if (library_queue_cursor + 1 < count)
				library_queue_cursor++;
			if (library_queue_cursor >= library_queue_offset + LIB_UI_3DS_ROWS)
				library_queue_offset = library_queue_cursor - LIB_UI_3DS_ROWS + 1;
		}
		else if (library_ui.grid_mode)
			Lib_ui_move_direction(&library_ui, 1, 0);
		else
			Lib_ui_3ds_show_filter(LIB_UI_FILTER_SERIES, index);
	}
	else if (DEF_HID_PR_EM(key->l, 1) || DEF_HID_HD(key->l))
		Lib_ui_3ds_show_tab(LIB_VIEW_TAB_RECENT, index);
	else if (DEF_HID_PR_EM(key->r, 1) || DEF_HID_HD(key->r))
		Lib_ui_3ds_toggle_favorite(index);
	else if (library_queue_view && (DEF_HID_PR_EM(key->x, 1) || DEF_HID_HD(key->x)))
		Lib_ui_3ds_queue_remove_current();
	else if (library_queue_view && (DEF_HID_PR_EM(key->y, 1) || DEF_HID_HD(key->y)))
		Lib_ui_3ds_queue_clear();
	else if (!library_queue_view && (DEF_HID_PR_EM(key->y, 1) || DEF_HID_HD(key->y)))
		Lib_ui_3ds_set_grid(!library_ui.grid_mode);
	else if (!library_queue_view && (DEF_HID_PR_EM(key->x, 1) || DEF_HID_HD(key->x)))
		Lib_ui_3ds_open_search(index);
	else if (DEF_HID_PR_EM(key->start, 1) || DEF_HID_HD(key->start))
		(void)Lib_ui_set_search(&library_ui, "", index);
	else if (DEF_HID_PR_EM(key->zl, 1) || DEF_HID_HD(key->zl))
		(void)Lib_ui_set_sort(&library_ui, Lib_ui_3ds_next_sort(library_ui.sort.key, false),
		library_ui.sort.descending, index);
	else if (DEF_HID_PR_EM(key->zr, 1) || DEF_HID_HD(key->zr))
		(void)Lib_ui_set_sort(&library_ui, Lib_ui_3ds_next_sort(library_ui.sort.key, true),
		library_ui.sort.descending, index);
	else if (DEF_HID_PR_EM(key->a, 1) || DEF_HID_HD(key->a))
	{
		if (library_queue_view)
			Lib_ui_3ds_queue_play_current(index, play_callback);
		else
			Lib_ui_3ds_play_browser_current(index, play_callback);
	}
	if (DEF_HID_PR_EM(key->d_up, 1) || DEF_HID_HD(key->d_up)
	|| DEF_HID_PR_EM(key->d_down, 1) || DEF_HID_HD(key->d_down)
	|| DEF_HID_PR_EM(key->d_left, 1) || DEF_HID_HD(key->d_left)
	|| DEF_HID_PR_EM(key->d_right, 1) || DEF_HID_HD(key->d_right)
	|| DEF_HID_PR_EM(key->l, 1) || DEF_HID_HD(key->l)
	|| DEF_HID_PR_EM(key->r, 1) || DEF_HID_HD(key->r)
	|| DEF_HID_PR_EM(key->y, 1) || DEF_HID_HD(key->y)
	|| DEF_HID_PR_EM(key->x, 1) || DEF_HID_HD(key->x)
	|| DEF_HID_PR_EM(key->start, 1) || DEF_HID_HD(key->start)
	|| DEF_HID_PR_EM(key->zl, 1) || DEF_HID_HD(key->zl)
	|| DEF_HID_PR_EM(key->zr, 1) || DEF_HID_HD(key->zr)
	|| DEF_HID_PR_EM(key->a, 1) || DEF_HID_HD(key->a))
		Lib_ui_3ds_request_redraw();
}

static const char* Lib_ui_3ds_screen_title(void)
{
	if (library_ui.filter == LIB_UI_FILTER_MOVIES)
		return Lib_ui_3ds_text("MOVIES", "FILMES");
	if (library_ui.filter == LIB_UI_FILTER_SERIES)
		return Lib_ui_3ds_text("SERIES", "SÉRIES");
	switch (library_ui.tab)
	{
		case LIB_VIEW_TAB_CONTINUE:
			return Lib_ui_3ds_text("CONTINUE", "CONTINUAR");
		case LIB_VIEW_TAB_RECENT:
			return Lib_ui_3ds_text("RECENT", "RECENTES");
		case LIB_VIEW_TAB_FAVORITES:
			return Lib_ui_3ds_text("FAVORITES", "FAVORITOS");
		case LIB_VIEW_TAB_ALL:
		default:
			return Lib_ui_3ds_text("MOVIES + SERIES", "FILMES + SÉRIES");
	}
}

static const char* Lib_ui_3ds_sort_name(Lib_sort_key key)
{
	if (library_is_portuguese)
	{
		switch (key)
		{
			case LIB_SORT_KEY_DATE_ADDED:
				return "DATA";
			case LIB_SORT_KEY_LAST_PLAYED:
				return "REPRODUZIDO";
			case LIB_SORT_KEY_DURATION:
				return "DURAÇÃO";
			case LIB_SORT_KEY_SIZE:
				return "TAMANHO";
			case LIB_SORT_KEY_NAME:
			default:
				return "NOME";
		}
	}
	switch (key)
	{
		case LIB_SORT_KEY_DATE_ADDED:
			return "DATE";
		case LIB_SORT_KEY_LAST_PLAYED:
			return "PLAYED";
		case LIB_SORT_KEY_DURATION:
			return "DURATION";
		case LIB_SORT_KEY_SIZE:
			return "SIZE";
		case LIB_SORT_KEY_NAME:
		default:
			return "NAME";
	}
}

static bool Lib_ui_3ds_episode_info(const Lib_item* item, int* season, int* episode)
{
	char title[256] = { 0, };
	int parsed_season = -1;
	int parsed_episode = -1;
	if (!item || !item->path || !Lib_name_parse(item->path, title, sizeof(title),
	&parsed_season, &parsed_episode, NULL) || parsed_episode < 0)
		return false;
	if (season)
		*season = parsed_season;
	if (episode)
		*episode = parsed_episode;
	return true;
}

static const char* Lib_ui_3ds_poster_label(const Lib_item* item)
{
	bool is_episode = Lib_ui_3ds_episode_info(item, NULL, NULL);
	return Lib_ui_3ds_text(is_episode ? "SHOW" : "FILM",
	is_episode ? "SÉRIE" : "FILME");
}

static void Lib_ui_3ds_format_meta(const Lib_item* item, char* out, size_t out_size)
{
	int season = -1;
	int episode = -1;
	int year = -1;
	bool is_series = false;
	uint32_t progress = 0;
	char status[32] = { 0, };

	if (!out || out_size == 0 || !item)
		return;
	is_series = Lib_ui_3ds_episode_info(item, &season, &episode);
	if (item->flags & LIB_ITEM_FLAG_WATCHED)
		snprintf(status, sizeof(status), "%s", Lib_ui_3ds_text("WATCHED", "ASSISTIDO"));
	else if (item->duration_ms > 0 && item->position_ms > 0)
	{
		progress = (uint32_t)(((uint64_t)item->position_ms * 100) / item->duration_ms);
		if (progress > 100)
			progress = 100;
		snprintf(status, sizeof(status), "%s %u%%",
		Lib_ui_3ds_text("RESUME", "RETOMAR"), (unsigned)progress);
	}
	if (is_series)
		snprintf(out, out_size, "%s %02d  %s %02d%s%s",
		library_is_portuguese ? "TEMP." : "S",
		season > 0 ? season : 0,
		library_is_portuguese ? "EP." : "E",
		episode,
		status[0] ? "  |  " : "", status);
	else
	{
		(void)Lib_name_parse(item->path, out, out_size, NULL, NULL, &year);
		if (year > 0)
			snprintf(out, out_size, "%d", year);
		else if (item->duration_ms > 0)
		{
			uint32_t minutes = item->duration_ms / 60000;
			if (minutes >= 60)
				snprintf(out, out_size, "%uh %02umin",
				(unsigned)(minutes / 60), (unsigned)(minutes % 60));
			else
				snprintf(out, out_size, "%umin", (unsigned)minutes);
		}
		else
			snprintf(out, out_size, "%s", Lib_ui_3ds_text("FEATURE FILM", "LONGA-METRAGEM"));
		if (status[0] && strlen(out) < out_size)
			snprintf(out + strlen(out), out_size - strlen(out), "  |  %s", status);
	}
}

static bool Lib_ui_3ds_get_file_size(const char* path, uint32_t* size)
{
	FILE* file = NULL;
	long file_size = 0;

	if (!path || !size)
		return false;
	file = fopen(path, "rb");
	if (!file)
		return false;
	if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) <= 0
	|| fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return false;
	}
	fclose(file);
	if ((unsigned long)file_size > LIB_UI_3DS_THUMB_MAX_FILE_SIZE)
		return false;
	*size = (uint32_t)file_size;
	return true;
}

static bool Lib_ui_3ds_convert_thumbnail(const uint8_t* source, uint32_t source_width,
uint32_t source_height, Raw_pixel source_format, uint8_t** out, uint16_t* out_width,
uint16_t* out_height)
{
	uint32_t width = source_width;
	uint32_t height = source_height;
	uint32_t channels = 0;
	uint64_t source_pixels = 0;
	uint8_t* pixels = NULL;

	if (!source || !out || !out_width || !out_height || source_width == 0 || source_height == 0)
		return false;
	source_pixels = (uint64_t)source_width * source_height;
	if (source_pixels > LIB_UI_3DS_THUMB_MAX_SOURCE_PIXELS)
		return false;
	switch (source_format)
	{
		case RAW_PIXEL_RGBA8888:
			channels = 4;
			break;
		case RAW_PIXEL_RGB888:
			channels = 3;
			break;
		case RAW_PIXEL_GRAYALPHA88:
			channels = 2;
			break;
		case RAW_PIXEL_GRAY8:
			channels = 1;
			break;
		default:
			return false;
	}

	if (width > LIB_UI_3DS_THUMB_WIDTH || height > LIB_UI_3DS_THUMB_HEIGHT)
	{
		if ((uint64_t)width * LIB_UI_3DS_THUMB_HEIGHT
		> (uint64_t)height * LIB_UI_3DS_THUMB_WIDTH)
		{
			height = (uint32_t)(((uint64_t)height * LIB_UI_3DS_THUMB_WIDTH) / width);
			width = LIB_UI_3DS_THUMB_WIDTH;
		}
		else
		{
			width = (uint32_t)(((uint64_t)width * LIB_UI_3DS_THUMB_HEIGHT) / height);
			height = LIB_UI_3DS_THUMB_HEIGHT;
		}
	}
	if (width < 2)
		width = 2;
	if ((width & 1) != 0)
		width--;
	if (height == 0)
		height = 1;

	pixels = (uint8_t*)malloc((size_t)width * height * 4);
	if (!pixels)
		return false;
	for (uint32_t y = 0; y < height; y++)
	{
		uint32_t source_y = (uint32_t)(((uint64_t)y * source_height) / height);
		if (source_y >= source_height)
			source_y = source_height - 1;
		for (uint32_t x = 0; x < width; x++)
		{
			uint32_t source_x = (uint32_t)(((uint64_t)x * source_width) / width);
			const uint8_t* source_pixel = source + (((size_t)source_y * source_width + source_x) * channels);
			uint8_t* target_pixel = pixels + (((size_t)y * width + x) * 4);
			if (channels == 4)
			{
				//Draw's ABGR texture input is stored as A, B, G, R bytes.
				target_pixel[0] = source_pixel[3];
				target_pixel[1] = source_pixel[2];
				target_pixel[2] = source_pixel[1];
				target_pixel[3] = source_pixel[0];
			}
			else if (channels == 3)
			{
				target_pixel[0] = 0xFF;
				target_pixel[1] = source_pixel[2];
				target_pixel[2] = source_pixel[1];
				target_pixel[3] = source_pixel[0];
			}
			else if (channels == 2)
			{
				target_pixel[0] = source_pixel[1];
				target_pixel[1] = source_pixel[0];
				target_pixel[2] = source_pixel[0];
				target_pixel[3] = source_pixel[0];
			}
			else
			{
				target_pixel[0] = 0xFF;
				target_pixel[1] = source_pixel[0];
				target_pixel[2] = source_pixel[0];
				target_pixel[3] = source_pixel[0];
			}
		}
	}
	*out = pixels;
	*out_width = (uint16_t)width;
	*out_height = (uint16_t)height;
	return true;
}

static bool Lib_ui_3ds_decode_thumbnail_path(const char* path, Lib_ui_3ds_thumb* cache)
{
	uint8_t* decoded = NULL;
	uint8_t* converted = NULL;
	uint32_t source_width = 0;
	uint32_t source_height = 0;
	uint32_t file_size = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	Raw_pixel source_format = RAW_PIXEL_INVALID;
	bool loaded = false;

	if (!path || !cache || !Lib_ui_3ds_get_file_size(path, &file_size))
		return false;
	(void)file_size;
	if (Util_decoder_image_decode(path, &decoded, &source_width, &source_height, &source_format) != DEF_SUCCESS
	|| !decoded)
		goto cleanup;
	if (!Lib_ui_3ds_convert_thumbnail(decoded, source_width, source_height, source_format,
	&converted, &width, &height))
		goto cleanup;
	if (Draw_texture_init(&cache->image, width, height, RAW_PIXEL_ABGR8888) != DEF_SUCCESS)
		goto cleanup;
	if (Draw_texture_set_data(&cache->image, converted, width, height, 0, 0) != DEF_SUCCESS)
	{
		Draw_texture_free(&cache->image);
		goto cleanup;
	}
	cache->width = width;
	cache->height = height;
	cache->loaded = true;
	loaded = true;

	cleanup:
	free(converted);
	free(decoded);
	return loaded;
}

static bool Lib_ui_3ds_try_thumbnail(const char* path, Lib_ui_3ds_thumb* cache)
{
	return Lib_ui_3ds_decode_thumbnail_path(path, cache);
}

static bool Lib_ui_3ds_load_thumbnail(const Lib_item* item, Lib_ui_3ds_thumb* cache)
{
	char path[2048] = { 0, };
	static const char* fallback_names[] = { "poster.jpg", "folder.jpg" };

	if (!item || !item->path || !cache)
		return false;
	if (Lib_thumb_sidecar_path(item->path, path, sizeof(path))
	&& Lib_ui_3ds_try_thumbnail(path, cache))
		return true;
	for (size_t i = 0; i < (sizeof(fallback_names) / sizeof(fallback_names[0])); i++)
	{
		if (Lib_thumb_fallback_path(item->path, fallback_names[i], path, sizeof(path))
		&& Lib_ui_3ds_try_thumbnail(path, cache))
			return true;
	}
	return false;
}

static Lib_ui_3ds_thumb* Lib_ui_3ds_get_thumbnail(const Lib_item* item, bool* may_decode)
{
	Lib_ui_3ds_thumb* cache = NULL;
	Lib_ui_3ds_thumb* oldest = &library_thumb_cache[0];

	if (!item)
		return NULL;
	for (uint8_t i = 0; i < LIB_UI_3DS_VISIBLE_ITEMS; i++)
	{
		if (library_thumb_cache[i].id == item->id)
		{
			cache = &library_thumb_cache[i];
			break;
		}
		if (library_thumb_cache[i].id == 0)
			oldest = &library_thumb_cache[i];
		else if (oldest->id != 0 && library_thumb_cache[i].last_used < oldest->last_used)
			oldest = &library_thumb_cache[i];
	}
	if (!cache)
	{
		if (!may_decode || !*may_decode)
			return NULL;
		*may_decode = false;
		cache = oldest;
		if (cache->loaded)
			Draw_texture_free(&cache->image);
		memset(cache, 0, sizeof(*cache));
		cache->id = item->id;
		(void)Lib_ui_3ds_load_thumbnail(item, cache);
		Lib_ui_3ds_request_redraw();
	}
	if (library_thumb_clock < UINT64_MAX)
		library_thumb_clock++;
	else
		library_thumb_clock = 1;
	cache->last_used = library_thumb_clock;
	return cache;
}

static void Lib_ui_3ds_draw_thumbnail(Lib_ui_3ds_thumb* thumb, float x, float y,
float box_width, float box_height)
{
	float width = 0;
	float height = 0;

	if (!thumb || !thumb->loaded || thumb->width == 0 || thumb->height == 0)
		return;
	width = box_width;
	height = width * ((float)thumb->height / thumb->width);
	if (height > box_height)
	{
		height = box_height;
		width = height * ((float)thumb->width / thumb->height);
	}
	Draw_texture(&thumb->image, DEF_DRAW_NO_COLOR, x + ((box_width - width) / 2),
	y + ((box_height - height) / 2), width, height);
}

static void Lib_ui_3ds_draw_poster(const Lib_item* item, Lib_ui_3ds_thumb* thumb,
float x, float y, float width, float height)
{
	Draw_image_data background = Draw_get_empty_image();
	Draw_texture(&background, LIB_UI_3DS_COLOR_BACKGROUND, x, y, width, height);
	if (thumb && thumb->loaded)
		Lib_ui_3ds_draw_thumbnail(thumb, x + 1, y + 1, width - 2, height - 2);
	else
		Draw_align_c(Lib_ui_3ds_poster_label(item), x + 1, y + 1, UI_THEME_FONT_CAPTION,
		LIB_UI_3DS_COLOR_ACCENT, DRAW_X_ALIGN_CENTER, DRAW_Y_ALIGN_CENTER, width - 2, height - 2);
}

static void Lib_ui_3ds_draw_list_item(const Lib_index* index, size_t slot,
uint32_t text_color, bool* may_decode)
{
	uint64_t id = 0;
	const Lib_item* item = NULL;
	Lib_ui_3ds_thumb* thumb = NULL;
	char meta[96] = { 0, };
	float y = LIB_UI_3DS_LIST_ROW_Y + (slot * LIB_UI_3DS_LIST_ROW_HEIGHT);
	uint32_t row_color = text_color;

	if (!Lib_ui_id_at_visible(&library_ui, slot, &id))
		return;
	item = Lib_index_find_id_const(index, id);
	if (!item)
		return;
	{
		Draw_image_data background = Draw_get_empty_image();
		bool selected = (bool)(library_ui.offset + slot == library_ui.cursor);
		Draw_texture(&background, selected ? LIB_UI_3DS_COLOR_SURFACE_ALT
		: ((slot & 1) ? LIB_UI_3DS_COLOR_SURFACE : LIB_UI_3DS_COLOR_SURFACE_ALT),
		LIB_UI_3DS_LIST_ROW_X, y, LIB_UI_3DS_LIST_ROW_WIDTH, LIB_UI_3DS_LIST_ROW_HEIGHT - 1);
		if (selected)
			Draw_texture(&background, LIB_UI_3DS_COLOR_ACCENT, LIB_UI_3DS_LIST_ROW_X,
			y, LIB_UI_3DS_RAIL_WIDTH, LIB_UI_3DS_LIST_ROW_HEIGHT - 1);
	}
	thumb = Lib_ui_3ds_get_thumbnail(item, may_decode);
	Lib_ui_3ds_draw_poster(item, thumb, LIB_UI_3DS_LIST_ROW_X + 5, y + 1, 40, 26);
	Draw_with_crop_c(Lib_item_get_title(item), LIB_UI_3DS_LIST_ROW_X + 52, y + 1,
	UI_THEME_FONT_BODY, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_LIST_ROW_WIDTH - 56, 15);
	Lib_ui_3ds_format_meta(item, meta, sizeof(meta));
	Draw_with_crop_c(meta, LIB_UI_3DS_LIST_ROW_X + 52, y + 16,
	UI_THEME_FONT_CAPTION, LIB_UI_3DS_COLOR_SECONDARY,
	DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_LIST_ROW_WIDTH - 56, 11);
}

static void Lib_ui_3ds_draw_grid_item(const Lib_index* index, size_t slot,
uint32_t text_color, bool* may_decode)
{
	uint64_t id = 0;
	const Lib_item* item = NULL;
	Lib_ui_3ds_thumb* thumb = NULL;
	char meta[96] = { 0, };
	size_t column = slot % LIB_UI_3DS_GRID_COLUMNS;
	size_t row = slot / LIB_UI_3DS_GRID_COLUMNS;
	float x = LIB_UI_3DS_GRID_X + (column * LIB_UI_3DS_GRID_CELL_WIDTH);
	float y = LIB_UI_3DS_GRID_Y + (row * LIB_UI_3DS_GRID_CELL_HEIGHT);
	uint32_t row_color = text_color;

	if (!Lib_ui_id_at_visible(&library_ui, slot, &id))
		return;
	item = Lib_index_find_id_const(index, id);
	if (!item)
		return;
	{
		Draw_image_data background = Draw_get_empty_image();
		bool selected = (bool)(library_ui.offset + slot == library_ui.cursor);
		Draw_texture(&background, selected ? LIB_UI_3DS_COLOR_SURFACE_ALT : LIB_UI_3DS_COLOR_SURFACE,
		x, y, LIB_UI_3DS_GRID_CELL_WIDTH - 3, LIB_UI_3DS_GRID_CELL_HEIGHT - 2);
		if (selected)
			Draw_texture(&background, LIB_UI_3DS_COLOR_ACCENT, x, y,
			LIB_UI_3DS_RAIL_WIDTH, LIB_UI_3DS_GRID_CELL_HEIGHT - 2);
	}
	thumb = Lib_ui_3ds_get_thumbnail(item, may_decode);
	Lib_ui_3ds_draw_poster(item, thumb, x + 6, y + 5, 54, 70);
	Draw_with_crop_c(Lib_item_get_title(item), x + 66, y + 7,
	UI_THEME_FONT_BODY, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP,
	LIB_UI_3DS_GRID_CELL_WIDTH - 71, 30);
	Lib_ui_3ds_format_meta(item, meta, sizeof(meta));
	Draw_with_crop_c(meta, x + 66, y + 42,
	UI_THEME_FONT_CAPTION, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP,
	LIB_UI_3DS_GRID_CELL_WIDTH - 71, 24);
	if (item->flags & LIB_ITEM_FLAG_FAVORITE)
		Draw_with_crop_c(Lib_ui_3ds_text("FAVORITE", "FAVORITO"), x + 66, y + 67,
		UI_THEME_FONT_CAPTION, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP,
		LIB_UI_3DS_GRID_CELL_WIDTH - 71, 12);
}

static void Lib_ui_3ds_draw_button(const char* label, size_t slot, size_t count,
float y, uint32_t text_color, bool selected)
{
	Draw_image_data background = Draw_get_empty_image();
	float width = 320.0f / (float)count;
	float x = width * (float)slot;
		Draw_texture(&background, selected ? LIB_UI_3DS_COLOR_SURFACE_ALT : LIB_UI_3DS_COLOR_SURFACE,
	x + 1, y, width - 2, LIB_UI_3DS_BUTTON_ROW_HEIGHT);
	Draw_align_c(label, x + 2, y, UI_THEME_FONT_BUTTON,
	text_color, DRAW_X_ALIGN_CENTER,
	DRAW_Y_ALIGN_CENTER, width - 4, LIB_UI_3DS_BUTTON_ROW_HEIGHT);
	if (selected)
		Draw_texture(&background, LIB_UI_3DS_COLOR_ACCENT, x + 1,
		y + LIB_UI_3DS_BUTTON_ROW_HEIGHT - LIB_UI_3DS_RAIL_WIDTH,
		width - 2, LIB_UI_3DS_RAIL_WIDTH);
}

static void Lib_ui_3ds_draw_queue(const Lib_index* index, uint32_t text_color,
bool* may_decode_thumbnail)
{
	char header[96] = { 0, };
	uint64_t id = 0;
	size_t count = Lib_ui_3ds_queue_count();
	Lib_queue_mode mode = LIB_QUEUE_MODE_NO_REPEAT;
	bool locked = Lib_ui_3ds_queue_lock();
	if (locked && library_queue)
		mode = Lib_queue_get_mode(library_queue);
	Lib_ui_3ds_queue_unlock(locked);
	snprintf(header, sizeof(header), "%s (%zu)",
	Lib_ui_3ds_text("QUEUE", "FILA"), count);
	Draw_with_crop_c(header, 4, 4, UI_THEME_FONT_BODY, text_color,
	DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, 312, 16);

	for (size_t row = 0; row < LIB_UI_3DS_ROWS; row++)
	{
		const Lib_item* item = NULL;
		Lib_ui_3ds_thumb* thumb = NULL;
		char meta[96] = { 0, };
		float y = LIB_UI_3DS_LIST_ROW_Y + (row * LIB_UI_3DS_LIST_ROW_HEIGHT);
		size_t position = library_queue_offset + row;
		uint32_t row_color = text_color;
		if (position >= count || !Lib_ui_3ds_queue_get(position, &id))
			continue;
		item = Lib_index_find_id_const(index, id);
		if (!item)
			continue;
		{
			Draw_image_data background = Draw_get_empty_image();
			bool selected = (position == library_queue_cursor);
			Draw_texture(&background, selected ? LIB_UI_3DS_COLOR_SURFACE_ALT
			: ((row & 1) ? LIB_UI_3DS_COLOR_SURFACE : LIB_UI_3DS_COLOR_SURFACE_ALT),
			LIB_UI_3DS_LIST_ROW_X, y, LIB_UI_3DS_LIST_ROW_WIDTH, LIB_UI_3DS_LIST_ROW_HEIGHT - 1);
			if (selected)
				Draw_texture(&background, LIB_UI_3DS_COLOR_ACCENT, LIB_UI_3DS_LIST_ROW_X,
				y, LIB_UI_3DS_RAIL_WIDTH, LIB_UI_3DS_LIST_ROW_HEIGHT - 1);
		}
		thumb = Lib_ui_3ds_get_thumbnail(item, may_decode_thumbnail);
		Lib_ui_3ds_draw_poster(item, thumb, LIB_UI_3DS_LIST_ROW_X + 5, y + 1, 40, 26);
		Draw_with_crop_c(Lib_item_get_title(item), LIB_UI_3DS_LIST_ROW_X + 52, y + 1,
		UI_THEME_FONT_BODY, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP,
		LIB_UI_3DS_LIST_ROW_WIDTH - 56, 15);
		Lib_ui_3ds_format_meta(item, meta, sizeof(meta));
		Draw_with_crop_c(meta, LIB_UI_3DS_LIST_ROW_X + 52, y + 16,
		UI_THEME_FONT_CAPTION, LIB_UI_3DS_COLOR_SECONDARY,
		DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_LIST_ROW_WIDTH - 56, 11);
	}
	if (count == 0)
		Draw_align_c(Lib_ui_3ds_text("Queue is empty", "Fila vazia"), 4, 100,
		UI_THEME_FONT_BODY, text_color, DRAW_X_ALIGN_CENTER, DRAW_Y_ALIGN_CENTER, 312, 24);

	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("BACK", "VOLTAR"), 0, 4,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("REMOVE", "REMOVER"), 1, 4,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("CLEAR", "LIMPAR"), 2, 4,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("PLAY", "TOCAR"), 3, 4,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, false);

	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("ONCE", "UMA VEZ"), 0, 4,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, mode == LIB_QUEUE_MODE_NO_REPEAT);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("REPEAT", "REPETIR"), 1, 4,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, mode == LIB_QUEUE_MODE_REPEAT);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("ORDER", "ORDEM"), 2, 4,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, mode == LIB_QUEUE_MODE_IN_ORDER);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("RANDOM", "ALEATÓRIO"), 3, 4,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, mode == LIB_QUEUE_MODE_RANDOM);
}

void Lib_ui_3ds_draw(const Lib_index* index, uint32_t text_color,
uint32_t background_color)
{
	Draw_image_data background = Draw_get_empty_image();
	char header[160] = { 0, };
	char status[48] = { 0, };
	library_ui_dark_theme = (bool)(background_color == DEF_DRAW_BLACK
	|| background_color == UI_THEME_COLOR_BACKGROUND);
	text_color = LIB_UI_3DS_COLOR_TEXT;
	bool may_decode_thumbnail = true;

	if (!library_ui_open || !index)
		return;

	Lib_ui_3ds_rebuild(index);
	Draw_texture(&background, LIB_UI_3DS_COLOR_BACKGROUND, 0, 0, 320, 240);
	if (library_queue_view)
	{
		Lib_ui_3ds_draw_queue(index, text_color, &may_decode_thumbnail);
		return;
	}
	snprintf(header, sizeof(header), "%s%s%s", Lib_ui_3ds_screen_title(),
	library_ui.search[0] ? "  ·  " : "", library_ui.search);
	snprintf(status, sizeof(status), "%zu  |  %s", library_ui.count,
	Lib_ui_3ds_sort_name(library_ui.sort.key));
	Draw_with_crop_c(header, 4, 3, UI_THEME_FONT_HEADING, text_color, DRAW_X_ALIGN_LEFT,
	DRAW_Y_ALIGN_TOP, 220, 18);
	Draw_with_crop_c(status, 224, 5, UI_THEME_FONT_CAPTION, LIB_UI_3DS_COLOR_SECONDARY, DRAW_X_ALIGN_RIGHT,
	DRAW_Y_ALIGN_TOP, 92, 14);
	Draw_texture(&background, LIB_UI_3DS_COLOR_RAIL_OFF, 4, 20, 312, 1);

	for (size_t slot = 0; slot < (library_ui.grid_mode
	? LIB_UI_3DS_GRID_VISIBLE_ITEMS : LIB_UI_3DS_ROWS); slot++)
	{
		if (library_ui.grid_mode)
			Lib_ui_3ds_draw_grid_item(index, slot, text_color, &may_decode_thumbnail);
		else
			Lib_ui_3ds_draw_list_item(index, slot, text_color, &may_decode_thumbnail);
	}
	if (library_ui.count == 0)
		Draw_align_c(library_ui.search[0] ? Lib_ui_3ds_text("No matching titles", "Nenhum resultado")
		: Lib_ui_3ds_text("No videos in this view", "Nenhum vídeo nesta aba"),
		4, 104, UI_THEME_FONT_BODY, text_color, DRAW_X_ALIGN_CENTER,
		DRAW_Y_ALIGN_CENTER, 312, 24);

	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("CONT.", "CONT."), 0, 5,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, library_ui.tab == LIB_VIEW_TAB_CONTINUE);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("REC.", "REC."), 1, 5,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, library_ui.tab == LIB_VIEW_TAB_RECENT);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("MOVIES", "FILMES"), 2, 5,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, library_ui.filter == LIB_UI_FILTER_MOVIES);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("SERIES", "SÉRIES"), 3, 5,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, library_ui.filter == LIB_UI_FILTER_SERIES);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("FAV", "FAV."), 4, 5,
	LIB_UI_3DS_BUTTON_ROW_0_Y, text_color, library_ui.tab == LIB_VIEW_TAB_FAVORITES);

	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("QUEUE", "FILA"), 0, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("ADD", "ADIC."), 1, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("PLAY", "VER"), 2, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("SEARCH", "BUSCA"), 3, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, false);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("GRID", "GRADE"), 4, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, library_ui.grid_mode);
	Lib_ui_3ds_draw_button(Lib_ui_3ds_text("CLOSE", "FECHAR"), 5, 6,
	LIB_UI_3DS_BUTTON_ROW_1_Y, text_color, false);
}
