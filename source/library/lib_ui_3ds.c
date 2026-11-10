#include "library/lib_ui_3ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "library/lib_library_3ds.h"
#include "library/lib_thumb.h"
#include "library/lib_ui.h"
#include "system/draw/draw.h"
#include "system/util/decoder.h"
#include "system/util/err_types.h"
#include "system/util/hid.h"
#include "system/util/keyboard.h"
#include "system/util/str.h"

#define LIB_UI_3DS_VISIBLE_ITEMS		(uint8_t)(8)
#define LIB_UI_3DS_GRID_COLUMNS			(size_t)(2)
#define LIB_UI_3DS_LIST_ROW_HEIGHT		(float)(22)
#define LIB_UI_3DS_LIST_ROW_X			(float)(4)
#define LIB_UI_3DS_LIST_ROW_Y			(float)(30)
#define LIB_UI_3DS_LIST_ROW_WIDTH		(float)(312)
#define LIB_UI_3DS_GRID_CELL_WIDTH		(float)(156)
#define LIB_UI_3DS_GRID_CELL_HEIGHT		(float)(45)
#define LIB_UI_3DS_GRID_X				(float)(4)
#define LIB_UI_3DS_GRID_Y				(float)(29)
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
	(void)Lib_ui_set_rows(&library_ui, LIB_UI_3DS_VISIBLE_ITEMS);
	library_ui_inited = true;
}

void Lib_ui_3ds_exit(void)
{
	if (!library_ui_inited)
		return;

	Lib_ui_3ds_free_thumbnails();
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

	Lib_ui_3ds_free_thumbnails();
	library_ui_open = true;
	Lib_library_set_paused(true);
	return true;
}

void Lib_ui_3ds_close(void)
{
	if (!library_ui_open)
		return;

	library_ui_open = false;
	Lib_ui_3ds_free_thumbnails();
	Lib_library_set_paused(false);
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
	if (Util_str_set(&hint, "Search titles") != DEF_SUCCESS
	|| Util_str_set(&initial_text, library_ui.search) != DEF_SUCCESS)
		goto cleanup;

	keyboard_inited = true;
	if (Util_keyboard_init(KEYBOARD_TYPE_QWERTY, KEYBOARD_ACCEPTABLE_INPUT_NO_EMPTY,
	KEYBOARD_DISPLAY_BUTTON_LEFT_MIDDLE, LIB_UI_3DS_SEARCH_CHARS, &hint, &initial_text,
	KEYBOARD_PASSWORD_MODE_OFF, KEYBOARD_FEATURES_BIT_NONE) != DEF_SUCCESS)
		goto cleanup;
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
		Lib_ui_move_direction(&library_ui, 0, -1);
	else if (DEF_HID_PR_EM(key->d_down, 1) || DEF_HID_HD(key->d_down))
		Lib_ui_move_direction(&library_ui, 0, 1);
	else if (DEF_HID_PR_EM(key->d_left, 1) || DEF_HID_HD(key->d_left))
	{
		if (library_ui.grid_mode)
			Lib_ui_move_direction(&library_ui, -1, 0);
		else
			(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_CONTINUE, index);
	}
	else if (DEF_HID_PR_EM(key->d_right, 1) || DEF_HID_HD(key->d_right))
	{
		if (library_ui.grid_mode)
			Lib_ui_move_direction(&library_ui, 1, 0);
		else
			(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_ALL, index);
	}
	else if (DEF_HID_PR_EM(key->l, 1) || DEF_HID_HD(key->l))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_RECENT, index);
	else if (DEF_HID_PR_EM(key->r, 1) || DEF_HID_HD(key->r))
		(void)Lib_ui_set_tab(&library_ui, LIB_VIEW_TAB_FAVORITES, index);
	else if (DEF_HID_PR_EM(key->y, 1) || DEF_HID_HD(key->y))
		Lib_ui_set_grid_mode(&library_ui, !library_ui.grid_mode);
	else if (DEF_HID_PR_EM(key->x, 1) || DEF_HID_HD(key->x))
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

static const char* Lib_ui_3ds_sort_name(Lib_sort_key key)
{
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

static const char* Lib_ui_3ds_codec_name(Lib_video_codec codec)
{
	switch (codec)
	{
		case LIB_VIDEO_CODEC_H264:
			return "H264";
		case LIB_VIDEO_CODEC_MPEG4:
			return "MPEG4";
		case LIB_VIDEO_CODEC_MPEG2:
			return "MPEG2";
		case LIB_VIDEO_CODEC_MPEG1:
			return "MPEG1";
		case LIB_VIDEO_CODEC_VP8:
			return "VP8";
		case LIB_VIDEO_CODEC_VP9:
			return "VP9";
		case LIB_VIDEO_CODEC_AV1:
			return "AV1";
		case LIB_VIDEO_CODEC_H266:
			return "H266";
		case LIB_VIDEO_CODEC_UNKNOWN:
		default:
			return NULL;
	}
}

static const char* Lib_ui_3ds_resolution_name(const Lib_item* item)
{
	if (!item || item->height == 0)
		return NULL;
	if (item->height >= 2000)
		return "2160P";
	if (item->height >= 1000)
		return "1080P";
	if (item->height >= 700)
		return "720P";
	if (item->height >= 400)
		return "480P";
	return "LOW";
}

static void Lib_ui_3ds_format_tags(const Lib_item* item, char* out, size_t out_size)
{
	const char* codec = NULL;
	const char* resolution = NULL;
	bool known = false;

	if (!out || out_size == 0 || !item)
		return;
	codec = Lib_ui_3ds_codec_name(item->video_codec);
	resolution = Lib_ui_3ds_resolution_name(item);
	known = (bool)(item->duration_ms > 0 || item->width > 0 || item->height > 0
	|| item->video_codec != LIB_VIDEO_CODEC_UNKNOWN || item->audio_tracks > 0 || item->subtitle_tracks > 0);
	out[0] = 0x00;
	if (codec)
		snprintf(out + strlen(out), out_size - strlen(out), "[%s] ", codec);
	if (resolution)
		snprintf(out + strlen(out), out_size - strlen(out), "[%s] ", resolution);
	if (item->flags & LIB_ITEM_FLAG_3D)
		snprintf(out + strlen(out), out_size - strlen(out), "[3D] ");
	if (item->audio_tracks > 1)
		snprintf(out + strlen(out), out_size - strlen(out), "[A:%u] ", (unsigned)item->audio_tracks);
	if (item->subtitle_tracks > 0)
		snprintf(out + strlen(out), out_size - strlen(out), "[S:%u] ", (unsigned)item->subtitle_tracks);
	if (item->flags & LIB_ITEM_FLAG_MISSING)
		snprintf(out + strlen(out), out_size - strlen(out), "[MISSING]");
	else if (item->flags & LIB_ITEM_FLAG_UNSUPPORTED)
		snprintf(out + strlen(out), out_size - strlen(out), "[UNSUPPORTED]");
	else if (known)
		snprintf(out + strlen(out), out_size - strlen(out), "[OK]");
	else
		snprintf(out + strlen(out), out_size - strlen(out), "[?]");
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

static void Lib_ui_3ds_draw_list_item(const Lib_index* index, size_t slot,
uint32_t text_color, bool* may_decode)
{
	uint64_t id = 0;
	const Lib_item* item = NULL;
	Lib_ui_3ds_thumb* thumb = NULL;
	char title[192] = { 0, };
	char tags[128] = { 0, };
	float y = LIB_UI_3DS_LIST_ROW_Y + (slot * LIB_UI_3DS_LIST_ROW_HEIGHT);
	uint32_t row_color = text_color;

	if (!Lib_ui_id_at_visible(&library_ui, slot, &id))
		return;
	item = Lib_index_find_id_const(index, id);
	if (!item)
		return;
	if (library_ui.offset + slot == library_ui.cursor)
	{
		Draw_image_data background = Draw_get_empty_image();
		Draw_texture(&background, DEF_DRAW_CYAN, LIB_UI_3DS_LIST_ROW_X, y,
		LIB_UI_3DS_LIST_ROW_WIDTH, LIB_UI_3DS_LIST_ROW_HEIGHT - 2);
		row_color = DEF_DRAW_BLACK;
	}
	thumb = Lib_ui_3ds_get_thumbnail(item, may_decode);
	Lib_ui_3ds_draw_thumbnail(thumb, LIB_UI_3DS_LIST_ROW_X + 2, y + 1, 28, 18);
	snprintf(title, sizeof(title), "%s%s", (item->flags & LIB_ITEM_FLAG_FAVORITE) ? "* " : "",
	Lib_item_get_title(item));
	Lib_ui_3ds_format_tags(item, tags, sizeof(tags));
	Draw_with_crop_c(title, LIB_UI_3DS_LIST_ROW_X + 34, y + 1,
	10, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_LIST_ROW_WIDTH - 38, 10);
	Draw_with_crop_c(tags, LIB_UI_3DS_LIST_ROW_X + 34, y + 12,
	7, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_LIST_ROW_WIDTH - 38, 8);
}

static void Lib_ui_3ds_draw_grid_item(const Lib_index* index, size_t slot,
uint32_t text_color, bool* may_decode)
{
	uint64_t id = 0;
	const Lib_item* item = NULL;
	Lib_ui_3ds_thumb* thumb = NULL;
	char tags[128] = { 0, };
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
	if (library_ui.offset + slot == library_ui.cursor)
	{
		Draw_image_data background = Draw_get_empty_image();
		Draw_texture(&background, DEF_DRAW_CYAN, x, y,
		LIB_UI_3DS_GRID_CELL_WIDTH - 3, LIB_UI_3DS_GRID_CELL_HEIGHT - 2);
		row_color = DEF_DRAW_BLACK;
	}
	thumb = Lib_ui_3ds_get_thumbnail(item, may_decode);
	Lib_ui_3ds_draw_thumbnail(thumb, x + 2, y + 3, 56, 36);
	Draw_with_crop_c(Lib_item_get_title(item), x + 61, y + 3,
	9, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_GRID_CELL_WIDTH - 65, 20);
	Lib_ui_3ds_format_tags(item, tags, sizeof(tags));
	Draw_with_crop_c(tags, x + 61, y + 25,
	7, row_color, DRAW_X_ALIGN_LEFT, DRAW_Y_ALIGN_TOP, LIB_UI_3DS_GRID_CELL_WIDTH - 65, 16);
}

void Lib_ui_3ds_draw(const Lib_index* index, uint32_t text_color,
uint32_t background_color)
{
	Draw_image_data background = Draw_get_empty_image();
	char header[192] = { 0, };
	char footer[128] = { 0, };
	bool may_decode_thumbnail = true;

	if (!library_ui_open || !index)
		return;

	Lib_ui_3ds_rebuild(index);
	Draw_texture(&background, background_color, 0, 0, 320, 240);
	snprintf(header, sizeof(header), "LIBRARY %s (%zu) %s SORT:%s%s%s",
	Lib_ui_3ds_tab_name(library_ui.tab), library_ui.count,
	library_ui.grid_mode ? "GRID" : "LIST", Lib_ui_3ds_sort_name(library_ui.sort.key),
	library_ui.search[0] ? " Q:" : "", library_ui.search);
	Draw_with_crop_c(header, 4, 4, 10, text_color, DRAW_X_ALIGN_LEFT,
	DRAW_Y_ALIGN_TOP, 312, 16);

	for (size_t slot = 0; slot < LIB_UI_3DS_VISIBLE_ITEMS; slot++)
	{
		if (library_ui.grid_mode)
			Lib_ui_3ds_draw_grid_item(index, slot, text_color, &may_decode_thumbnail);
		else
			Lib_ui_3ds_draw_list_item(index, slot, text_color, &may_decode_thumbnail);
	}
	if (library_ui.count == 0)
		Draw_c(library_ui.search[0] ? "No matching titles" : "No videos in this view",
		8, 112, 10, text_color);

	snprintf(footer, sizeof(footer), "A play  Y grid  X search  START clear  B close  L/R views  ZL/ZR sort");
	Draw_with_crop_c(footer, 4, 220, 8, text_color, DRAW_X_ALIGN_LEFT,
	DRAW_Y_ALIGN_TOP, 312, 12);
}
