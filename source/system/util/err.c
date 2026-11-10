//Includes.
#include "system/util/err.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "system/menu.h"
#include "system/draw/draw.h"
#include "system/draw/theme.h"
#include "system/sem.h"
#include "system/util/file.h"
#include "system/util/hid.h"
#include "system/util/log.h"
#include "system/util/str.h"
#include "system/util/util.h"
#include "system/util/watch.h"

//Defines.
#define BOX_X						(double)(12)	//Box X offset for error screen in px.
#define BOX_Y						(double)(8)	//Box Y offset for error screen in px.
#define BOX_WIDTH					(double)(296)	//Box width for error screen in px.
#define BOX_HEIGHT					(double)(224)	//Box height for error screen in px.

#define BUTTON_X					(double)(72)	//Button X offset for error screen in px.
#define BUTTON_Y					(double)(202)	//Button Y offset for error screen in px.
#define BUTTON_SPACE_X				(double)(12)	//Button spacing for error screen (for X direction) in px.
#define BUTTON_WIDTH				(double)(82)	//Button width for error screen in px.
#define BUTTON_HEIGHT				(double)(24)	//Button height for error screen in px.

#define ITEM_SPACE_X				(double)(10)	//Element spacing for error screen (for X direction) in px.
#define ITEM_SPACE_Y				(double)(0)		//Element spacing for error screen (for Y direction) in px.
#define ITEM_HEIGHT					(double)(20)	//Element height for error screen in px.

//Close.
#define HID_NOT_INITED_CLOSE_CFM(k)	(bool)(DEF_HID_PR_EM((k).a, 1) || DEF_HID_HD((k).a))
//OK (close).
#define HID_OK_SEL(k)				(bool)((DEF_HID_PHY_PR((k).touch) && DEF_HID_INIT_IN(util_err_ok_button, (k))) || DEF_HID_PHY_PR((k).a))
#define HID_OK_CFM(k)				(bool)(((DEF_HID_PR_EM((k).touch, 1) || DEF_HID_HD((k).touch)) && DEF_HID_INIT_LAST_IN(util_err_ok_button, (k))) || (DEF_HID_PR_EM((k).a, 1) || DEF_HID_HD((k).a)))
#define HID_OK_DESEL(k)				(bool)(DEF_HID_PHY_NP((k).touch) && DEF_HID_PHY_NP((k).a))
//Save.
#define HID_SAVE_SEL(k)				(bool)((DEF_HID_PHY_PR((k).touch) && DEF_HID_INIT_IN(util_err_save_button, (k))) || DEF_HID_PHY_PR((k).x))
#define HID_SAVE_CFM(k)				(bool)(((DEF_HID_PR_EM((k).touch, 1) || DEF_HID_HD((k).touch)) && DEF_HID_INIT_LAST_IN(util_err_save_button, (k))) || (DEF_HID_PR_EM((k).x, 1) || DEF_HID_HD((k).x)))
#define HID_SAVE_DESEL(k)			(bool)(DEF_HID_PHY_NP(((k)).touch) && DEF_HID_PHY_NP((k).x))

#define FONT_SIZE_ERROR				(float)(24.00)	//Font size for API error in px.
#define FONT_SIZE_ERROR_CONTENT		(float)(14.50)	//Font size for error content (except description) in px.
#define FONT_SIZE_ERROR_DESCRIPTION	(float)(13.50)	//Font size for error description in px.
#define FONT_SIZE_BUTTON			(float)(13.50)	//Font size for buttons in px.

//Typedefs.
//N/A.

//Prototypes.
static void Util_err_save_callback(void);

//Variables.
static bool util_err_show_flag = false;
static bool util_err_save_request = false;
static bool util_err_init = false;
static Str_data util_err_summary = { 0, };
static Str_data util_err_description = { 0, };
static Str_data util_err_location = { 0, };
static Str_data util_err_code = { 0, };
static Draw_image_data util_err_ok_button = { 0, }, util_err_save_button = { 0, };

//Code.
uint32_t Util_err_init(void)
{
	uint32_t result = DEF_ERR_OTHER;
	Str_data* str_list[] = { &util_err_summary, &util_err_description, &util_err_location, &util_err_code, };

	if(util_err_init)
		goto already_inited;

	util_err_show_flag = false;
	util_err_save_request = false;
	util_err_ok_button = Draw_get_empty_image();
	util_err_save_button = Draw_get_empty_image();

	for(uint8_t i = 0; i < DEF_UTIL_ARRAY_NUM_OF_ELEMENTS(str_list); i++)
	{
		result = Util_str_init(str_list[i]);
		if(result != DEF_SUCCESS)
		{
			DEF_LOG_RESULT(Util_str_init, false, result);
			goto error_other;
		}
		Util_str_set(str_list[i], "N/A");
	}

	if(!Menu_add_worker_thread_callback(Util_err_save_callback))
	{
		result = DEF_ERR_OTHER;
		DEF_LOG_RESULT(Menu_add_worker_thread_callback, false, result);
		goto error_other;
	}

	Util_watch_add(WATCH_HANDLE_GLOBAL, &util_err_show_flag, sizeof(util_err_show_flag));
	Util_watch_add(WATCH_HANDLE_ERR, &util_err_ok_button.selected, sizeof(util_err_ok_button.selected));
	Util_watch_add(WATCH_HANDLE_ERR, &util_err_save_button.selected, sizeof(util_err_save_button.selected));

	util_err_init = true;
	return DEF_SUCCESS;

	already_inited:
	return DEF_ERR_ALREADY_INITIALIZED;

	error_other:
	for(uint8_t i = 0; i < DEF_UTIL_ARRAY_NUM_OF_ELEMENTS(str_list); i++)
		Util_str_free(str_list[i]);

	Menu_remove_worker_thread_callback(Util_err_save_callback);
	return result;
}

void Util_err_exit(void)
{
	Str_data* str_list[] = { &util_err_summary, &util_err_description, &util_err_location, &util_err_code, };

	if(!util_err_init)
		return;

	util_err_init = false;
	for(uint8_t i = 0; i < DEF_UTIL_ARRAY_NUM_OF_ELEMENTS(str_list); i++)
		Util_str_free(str_list[i]);

	Menu_remove_worker_thread_callback(Util_err_save_callback);

	Util_watch_remove(WATCH_HANDLE_GLOBAL, &util_err_show_flag);
	Util_watch_remove(WATCH_HANDLE_ERR, &util_err_ok_button.selected);
	Util_watch_remove(WATCH_HANDLE_ERR, &util_err_save_button.selected);
}

bool Util_err_query_show_flag(void)
{
	if(!util_err_init)
		return false;

	return util_err_show_flag;
}

void Util_err_set_error_message(const char* summary, const char* description, const char* location, uint32_t error_code)
{
	if(!util_err_init)
		return;

	if(!summary || !description || !location)
		return;

	Util_err_clear_error_message();
	Util_str_set(&util_err_summary, summary);
	Util_str_set(&util_err_description, description);
	Util_str_set(&util_err_location, location);

	if(error_code == DEF_ERR_NO_RESULT_CODE)
		Util_str_set(&util_err_code, "N/A");
	else
		Util_str_format(&util_err_code, "0x%" PRIX32, error_code);
}

void Util_err_set_show_flag(bool flag)
{
	if(!util_err_init)
		return;

	util_err_show_flag = flag;
}

void Util_err_clear_error_message(void)
{
	Str_data* str_list[] = { &util_err_summary, &util_err_description, &util_err_location, &util_err_code, };

	if(!util_err_init)
		return;

	for(uint8_t i = 0; i < DEF_UTIL_ARRAY_NUM_OF_ELEMENTS(str_list); i++)
		Util_str_set(str_list[i], "N/A");
}

void Util_err_save_error(void)
{
	if(!util_err_init)
		return;

	util_err_save_request = true;
}

const char* Util_err_get_error_msg(uint32_t result)
{
	if(!util_err_init)
		return "";

	switch (result)
	{
		case DEF_SUCCESS:
			return "";
		case DEF_ERR_OTHER:
			return DEF_ERR_OTHER_STR;
		case DEF_ERR_OUT_OF_MEMORY:
			return DEF_ERR_OUT_OF_MEMORY_STR;
		case DEF_ERR_OUT_OF_LINEAR_MEMORY:
			return DEF_ERR_OUT_OF_LINEAR_MEMORY_STR;
		case DEF_ERR_GAS_RETURNED_NOT_SUCCESS:
			return DEF_ERR_GAS_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_STB_IMG_RETURNED_NOT_SUCCESS:
			return DEF_ERR_STB_IMG_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_FFMPEG_RETURNED_NOT_SUCCESS:
			return DEF_ERR_FFMPEG_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_INVALID_ARG:
			return DEF_ERR_INVALID_ARG_STR;
		case DEF_ERR_JSMN_RETURNED_NOT_SUCCESS:
			return DEF_ERR_JSMN_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_TRY_AGAIN:
			return DEF_ERR_TRY_AGAIN_STR;
		case DEF_ERR_ALREADY_INITIALIZED:
			return DEF_ERR_ALREADY_INITIALIZED_STR;
		case DEF_ERR_NOT_INITIALIZED:
			return DEF_ERR_NOT_INITIALIZED_STR;
		case DEF_ERR_CURL_RETURNED_NOT_SUCCESS:
			return DEF_ERR_CURL_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_NEED_MORE_INPUT:
			return DEF_ERR_NEED_MORE_INPUT_STR;
		case DEF_ERR_DECODER_TRY_AGAIN_NO_OUTPUT:
			return DEF_ERR_DECODER_TRY_AGAIN_NO_OUTPUT_STR;
		case DEF_ERR_DECODER_TRY_AGAIN:
			return DEF_ERR_DECODER_TRY_AGAIN_STR;
		case DEF_ERR_MBEDTLS_RETURNED_NOT_SUCCESS:
			return DEF_ERR_MBEDTLS_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_SOCKET_RETURNED_NOT_SUCCESS:
			return DEF_ERR_SOCKET_RETURNED_NOT_SUCCESS_STR;
		case DEF_ERR_DISABLED:
			return DEF_ERR_DISABLED_STR;
		default:
			return "Something went wrong (likely Nintendo API error).";
	}
}

void Util_err_main(const Hid_info* key)
{
	if(!key)
		return;

	if(!util_err_init)
	{
		//Execute functions if conditions are satisfied.
		if (HID_NOT_INITED_CLOSE_CFM(*key))
		{
			util_err_show_flag = false;
			//Reset key state on scene change.
			Util_hid_reset_key_state(HID_KEY_BIT_ALL);
		}
		return;
	}

	//Notify user that button is being pressed.
	if(HID_OK_SEL(*key) && !util_err_save_request)
		util_err_ok_button.selected = true;
	if(HID_SAVE_SEL(*key) && !util_err_save_request)
		util_err_save_button.selected = true;

	//Execute functions if conditions are satisfied.
	if (HID_OK_CFM(*key) && !util_err_save_request)
	{
		util_err_show_flag = false;
		//Reset key state on scene change.
		Util_hid_reset_key_state(HID_KEY_BIT_ALL);
	}
	else if (HID_SAVE_CFM(*key) && !util_err_save_request)
		Util_err_save_error();

	//Notify user that button is NOT being pressed anymore.
	if(HID_OK_DESEL(*key))
		util_err_ok_button.selected = false;
	if(HID_SAVE_DESEL(*key))
		util_err_save_button.selected = false;
}

void Util_err_draw(void)
{
	Sem_config config = { 0, };
	uint32_t text_color = 0;
	uint32_t secondary_color = 0;
	uint32_t background_color = 0;
	uint32_t surface_color = 0;
	uint32_t accent_color = 0;
	uint32_t selected_text_color = 0;
	uint32_t button_text_color = 0;
	uint32_t ok_button_color = 0;
	uint32_t save_button_color = 0;
	double draw_x = 0;
	double draw_y = 0;
	bool portuguese = false;
	Draw_image_data background = Draw_get_empty_image();
	Sem_get_config(&config);
	text_color = UI_THEME_COLOR_MODE(config.is_night, TEXT);
	secondary_color = UI_THEME_COLOR_MODE(config.is_night, SECONDARY_TEXT);
	background_color = UI_THEME_COLOR_MODE(config.is_night, BACKGROUND);
	surface_color = UI_THEME_COLOR_MODE(config.is_night, SURFACE);
	accent_color = UI_THEME_COLOR_MODE(config.is_night, ACCENT);
	selected_text_color = UI_THEME_COLOR_MODE(config.is_night, SELECTED_TEXT);
	portuguese = (bool)(strncmp(config.lang, "pt", 2) == 0);

	if(!util_err_init)
	{
		Draw_with_background_c(portuguese ? "O painel de erro não foi iniciado.\nPressione A para fechar."
		: "Error screen is not initialized.\nPress A to close.", BOX_X, BOX_Y, FONT_SIZE_ERROR, text_color,
		DRAW_X_ALIGN_CENTER, DRAW_Y_ALIGN_CENTER, BOX_WIDTH, BOX_HEIGHT, DRAW_BACKGROUND_ENTIRE_BOX, &background, surface_color);
		return;
	}

	button_text_color = (util_err_save_request ? secondary_color : text_color);
	ok_button_color = (util_err_ok_button.selected ? accent_color : surface_color);
	save_button_color = (util_err_save_button.selected ? accent_color : surface_color);

	Draw_texture(&background, background_color, BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT);
	Draw_texture(&background, UI_THEME_COLOR_MODE(config.is_night, DANGER), BOX_X, BOX_Y, 3, BOX_HEIGHT);

	draw_x = (BOX_X + ITEM_SPACE_X);
	draw_y = (BOX_Y + 8 + ITEM_SPACE_Y);
	Draw_c(portuguese ? "Resumo:" : "Summary:", draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, UI_THEME_COLOR_MODE(config.is_night, DANGER));

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw(&util_err_summary, draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, text_color);

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw_c(portuguese ? "Local:" : "Location:", draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, UI_THEME_COLOR_MODE(config.is_night, DANGER));

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw(&util_err_location, draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, text_color);

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw_c(portuguese ? "Código:" : "Error code:", draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, UI_THEME_COLOR_MODE(config.is_night, DANGER));

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw(&util_err_code, draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, text_color);

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw_c(portuguese ? "Descrição:" : "Description:", draw_x, draw_y, FONT_SIZE_ERROR_CONTENT, UI_THEME_COLOR_MODE(config.is_night, DANGER));

	draw_y += (ITEM_HEIGHT + ITEM_SPACE_Y);
	Draw(&util_err_description, draw_x, draw_y, FONT_SIZE_ERROR_DESCRIPTION, text_color);

	draw_x = BUTTON_X;
	draw_y = BUTTON_Y;
	Draw_with_background_c("OK (A)", draw_x, draw_y, FONT_SIZE_BUTTON,
	util_err_ok_button.selected ? selected_text_color : button_text_color, DRAW_X_ALIGN_CENTER, DRAW_Y_ALIGN_CENTER,
	BUTTON_WIDTH, BUTTON_HEIGHT, DRAW_BACKGROUND_ENTIRE_BOX, &util_err_ok_button, ok_button_color);

	draw_x += (BUTTON_WIDTH + BUTTON_SPACE_X);
	Draw_with_background_c(portuguese ? "SALVAR (X)" : "SAVE (X)", draw_x, draw_y, FONT_SIZE_BUTTON,
	util_err_save_button.selected ? selected_text_color : button_text_color, DRAW_X_ALIGN_CENTER, DRAW_Y_ALIGN_CENTER,
	BUTTON_WIDTH, BUTTON_HEIGHT, DRAW_BACKGROUND_ENTIRE_BOX, &util_err_save_button, save_button_color);
}

static void Util_err_save_callback(void)
{
	if (util_err_init && util_err_save_request)
	{
		uint32_t result = DEF_ERR_OTHER;
		Str_data file_name = { 0, };
		Str_data save_data = { 0, };
		time_t unix_time = time(NULL);
		struct tm* time = gmtime((const time_t*)&unix_time);

		time->tm_year += 1900;
		time->tm_mon += 1;

		result = Util_str_init(&file_name);
		if(result != DEF_SUCCESS)
			DEF_LOG_RESULT(Util_str_init, false, result);

		result = Util_str_init(&save_data);
		if(result != DEF_SUCCESS)
			DEF_LOG_RESULT(Util_str_init, false, result);

		if(Util_str_is_valid(&file_name) && Util_str_is_valid(&save_data))
		{
			Util_str_format(&file_name, "%04" PRIu16 "_%02" PRIu8 "_%02" PRIu8 "_%02" PRIu8 "_%02" PRIu8 "_%02" PRIu8 ".txt",
			time->tm_year, time->tm_mon, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);

			Util_str_format(&save_data, "\n\n##ERROR MESSAGE##\n%s\n%s\n%s\n%s\n",
			DEF_STR_NEVER_NULL(&util_err_summary), DEF_STR_NEVER_NULL(&util_err_description),
			DEF_STR_NEVER_NULL(&util_err_location), DEF_STR_NEVER_NULL(&util_err_code));

			result = Util_log_dump(file_name.buffer, (DEF_MENU_MAIN_DIR "error/"));
			if(result != DEF_SUCCESS)
				DEF_LOG_RESULT(Util_log_dump, false, result);

			result = Util_file_save_to_file(file_name.buffer, (DEF_MENU_MAIN_DIR "error/"), (uint8_t*)save_data.buffer , save_data.length, false);
			if(result != DEF_SUCCESS)
				DEF_LOG_RESULT(Util_file_save_to_file, false, result);

			Util_err_set_show_flag(false);
		}
		util_err_save_request = false;

		Util_str_free(&file_name);
		Util_str_free(&save_data);
	}
}
