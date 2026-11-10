#if !defined(DEF_LIB_UI_3DS_H)
#define DEF_LIB_UI_3DS_H
#include <stdbool.h>
#include <stdint.h>
#include "library/lib_index.h"
#include "library/lib_queue.h"
#include "system/util/sync_types.h"
#include "system/util/hid_types.h"

typedef void (*Lib_ui_3ds_play_callback)(const Lib_item* item);
typedef void (*Lib_ui_3ds_queue_change_callback)(void);

void Lib_ui_3ds_init(void);

void Lib_ui_3ds_exit(void);

bool Lib_ui_3ds_is_open(void);

bool Lib_ui_3ds_open(const Lib_index* index, Lib_queue* queue, Sync_data* queue_lock,
bool keep_library_paused);

void Lib_ui_3ds_set_language(const char* language);

void Lib_ui_3ds_set_queue_change_callback(Lib_ui_3ds_queue_change_callback callback);

void Lib_ui_3ds_close(void);

void Lib_ui_3ds_main(const Hid_info* key, const Lib_index* index,
Lib_ui_3ds_play_callback play_callback);

void Lib_ui_3ds_draw(const Lib_index* index, uint32_t text_color,
uint32_t background_color);

#endif //!defined(DEF_LIB_UI_3DS_H)
