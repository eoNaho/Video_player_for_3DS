#if !defined(DEF_LIB_NAME_H)
#define DEF_LIB_NAME_H
#include <stdbool.h>
#include <stddef.h>

//Extracts a display title, season/episode and year from a file name/path.
//title_out is always NUL-terminated (truncated when needed).
//season_out, episode_out and year_out may be NULL; they are set to -1 when unknown.
//Returns false only for invalid arguments (NULL path or zero-sized buffer).
bool Lib_name_parse(const char* path, char* title_out, size_t title_size,
int* season_out, int* episode_out, int* year_out);

//Path helpers (no allocation).
const char* Lib_path_basename(const char* path);

//Reproduces the old saved_pos file name: full path with '/' replaced by '_'.
//Returns false when the result does not fit.
bool Lib_path_old_saved_pos_name(const char* path, char* out, size_t out_size);

#endif //!defined(DEF_LIB_NAME_H)
