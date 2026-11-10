#if !defined(DEF_LIB_UI_3DS_H)
#define DEF_LIB_UI_3DS_H
#include <stdbool.h>
#include <stdint.h>
#include "library/lib_index.h"
#include "system/util/hid_types.h"

typedef void (*Lib_ui_3ds_play_callback)(const Lib_item* item);

void Lib_ui_3ds_init(void);

void Lib_ui_3ds_exit(void);

bool Lib_ui_3ds_is_open(void);

bool Lib_ui_3ds_open(const Lib_index* index);

void Lib_ui_3ds_close(void);

void Lib_ui_3ds_main(const Hid_info* key, const Lib_index* index,
Lib_ui_3ds_play_callback play_callback);

void Lib_ui_3ds_draw(const Lib_index* index, uint32_t text_color,
uint32_t background_color);

#endif //!defined(DEF_LIB_UI_3DS_H)
