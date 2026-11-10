#include "library/lib_ui_3ds.h"

#include <stdio.h>

#include "library/lib_library_3ds.h"
#include "library/lib_ui.h"
#include "system/draw/draw.h"
#include "system/util/hid.h"

#define LIB_UI_3DS_ROWS                 (uint8_t)(8)
#define LIB_UI_3DS_ROW_HEIGHT           (uint8_t)(22)
#define LIB_UI_3DS_ROW_X                (float)(4)
#define LIB_UI_3DS_ROW_Y                (float)(30)
#define LIB_UI_3DS_ROW_WIDTH            (float)(312)

static Lib_ui library_ui = { 0, };
static bool library_ui_inited = false;
static bool library_ui_open = false;

void Lib_ui_3ds_init(void)
{
	if (library_ui_inited)
		return;

	Lib_ui_init(&library_ui);
	(void)Lib_ui_set_rows(&library_ui, LIB_UI_3DS_ROWS);
	library_ui_inited = true;
}

void Lib_ui_3ds_exit(void)
{
	if (!library_ui_inited)
		return;

	Lib_ui_free(&library_ui);
	library_ui_inited = false;
	library_ui_open = false;
}

bool Lib_ui_3ds_is_open(void)
{
	return library_ui_open;
}

bool Lib_ui_3ds_open(const Lib_index* index)
{
	if (!library_ui_inited || !Lib_library_is_initialized() || !index || Lib_library_is_busy()
	|| !Lib_ui_rebuild(&library_ui, index))
		return false;

	library_ui_open = true;
	Lib_library_set_paused(true);
	return true;
}

void Lib_ui_3ds_close(void)
{
	if (!library_ui_open)
		return;

	library_ui_open = false;
	Lib_library_set_paused(false);
}

static void Lib_ui_3ds_rebuild(const Lib_index* index)
{
	if (index && library_ui.needs_rebuild)
		(void)Lib_ui_rebuild(&library_ui, index);
}

void Lib_ui_3ds_main(const Hid_info* key, const Lib_index* index,
Lib_ui_3ds_play_callback play_callback)
{
	const Lib_item* item = NULL;

	if (!library_ui_open || !key || !index)
		return;

	Lib_ui_3ds_rebuild(index);
	if (DEF_HID_PR_EM(key->b, 1) || DEF_HID_HD(key->b)
	|| DEF_HID_PR_EM(key->select, 1) || DEF_HID_HD(key->select))
	{
		Lib_ui_3ds_close();
		Util_hid_reset_key_state(HID_KEY_BIT_ALL);
		return;
	}

	if (DEF_HID_PR_EM(key->d_up, 1) || DEF_HID_HD(key->d_up))
		Lib_ui_move(&library_ui, -1);
	else if (DEF_HID_PR_EM(key->d_down, 1) || DEF_HID_HD(key->d_down))
		Lib_ui_move(&library_ui, 1);
	else if (DEF_HID_PR_EM(key->d_left, 1) || DEF_HID_HD(key->d_left))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_CONTINUE, index);
	else if (DEF_HID_PR_EM(key->d_right, 1) || DEF_HID_HD(key->d_right))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_ALL, index);
	else if (DEF_HID_PR_EM(key->l, 1) || DEF_HID_HD(key->l))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_RECENT, index);
	else if (DEF_HID_PR_EM(key->r, 1) || DEF_HID_HD(key->r))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_FAVORITES, index);
	else if (DEF_HID_PR_EM(key->a, 1) || DEF_HID_HD(key->a))
	{
		item = Lib_ui_current(&library_ui, index);
		if (item && play_callback)
		{
			play_callback(item);
			Lib_ui_3ds_close();
		}
	}
}

static const char* Lib_ui_3ds_tab_name(Lib_view_tab tab)
{
	switch (tab)
	{
		case LIB_VIEW_TAB_CONTINUE:
			return "CONTINUE";
		case LIB_VIEW_TAB_RECENT:
			return "RECENT";
		case LIB_VIEW_TAB_FAVORITES:
			return "FAVORITES";
		case LIB_VIEW_TAB_ALL:
		default:
			return "ALL";
	}
}

void Lib_ui_3ds_draw(const Lib_index* index, uint32_t text_color,
uint32_t background_color)
{
	Draw_image_data background = Draw_get_empty_image();
	char header[96] = { 0, };

	if (!library_ui_open || !index)
		return;

	Lib_ui_3ds_rebuild(index);
	Draw_texture(&background, background_color, 0, 0, 320, 240);
	snprintf(header, sizeof(header), "LIBRARY  %s  (%zu)",
	Lib_ui_3ds_tab_name(library_ui.tab), library_ui.count);
	Draw_c(header, 4, 5, 12, text_color);

	for (uint8_t row = 0; row < LIB_UI_3DS_ROWS; row++)
	{
		uint64_t id = 0;
		const Lib_item* item = NULL;
		char line[160] = { 0, };
		uint32_t row_color = text_color;

		if (!Lib_ui_id_at_visible(&library_ui, row, &id))
			continue;

		item = Lib_index_find_id_const(index, id);
		if (!item)
			continue;

		if (library_ui.offset + row == library_ui.cursor)
		{
			Draw_texture(&background, DEF_DRAW_CYAN, LIB_UI_3DS_ROW_X,
			LIB_UI_3DS_ROW_Y + (row * LIB_UI_3DS_ROW_HEIGHT),
			LIB_UI_3DS_ROW_WIDTH, LIB_UI_3DS_ROW_HEIGHT - 2);
			row_color = DEF_DRAW_BLACK;
		}

		snprintf(line, sizeof(line), "%s%s",
		(item->flags & LIB_ITEM_FLAG_WATCHED) ? "[OK] " : "      ",
		Lib_item_get_title(item));
		Draw_with_crop_c(line, LIB_UI_3DS_ROW_X + 3,
		LIB_UI_3DS_ROW_Y + (row * LIB_UI_3DS_ROW_HEIGHT) + 2,
		12, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP,
		LIB_UI_3DS_ROW_WIDTH - 6, LIB_UI_3DS_ROW_HEIGHT - 3);
	}

	Draw_c("A: play  B/SELECT: close  D-PAD: move/tab  L/R: views",
	4, 216, 8, text_color);
}
