#if !defined(DEF_LIB_MIGRATE_3DS_H)
#define DEF_LIB_MIGRATE_3DS_H
#include <stdbool.h>
#include <stdint.h>
#include "library/lib_index.h"

//Lib_migrate_read_cb implementation for the 3DS:
//reads "<main dir>/saved_pos/<old_name>" (little-endian double, seconds).
bool Lib_migrate_3ds_read_position(const char* old_name, uint64_t* position_ms, void* userdata);

//Convenience wrapper: imports every legacy position into index.
bool Lib_migrate_3ds_saved_pos(Lib_index* index, size_t* migrated);

#endif //!defined(DEF_LIB_MIGRATE_3DS_H)
