#if !defined(DEF_LIB_MIGRATE_H)
#define DEF_LIB_MIGRATE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_index.h"

//Reads the legacy saved_pos file identified by old_name.
//Return true and write the position in milliseconds when a usable value exists.
typedef bool (*Lib_migrate_read_cb)(const char* old_name, uint64_t* position_ms, void* userdata);

typedef struct
{
	size_t items_seen;
	size_t migrated;
	size_t already_known;				//Item already had a position.
	size_t not_found;					//No legacy position for this item.
} Lib_migrate_stats;

//Imports playback positions from the legacy saved_pos/ files.
//- Only items with position_ms == 0 are touched (newer progress wins).
//- last_played is left untouched: the legacy format has no timestamp.
//Returns false only on allocation failure.
bool Lib_migrate_saved_pos(Lib_index* index, Lib_migrate_read_cb read_cb, void* userdata,
Lib_migrate_stats* stats);

#endif //!defined(DEF_LIB_MIGRATE_H)
