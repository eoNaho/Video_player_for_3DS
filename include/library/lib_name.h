#if !defined(DEF_LIB_NAME_H)
#define DEF_LIB_NAME_H
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
	int season;						//Season number, -1 when the path does not identify one.
	int episode;						//Episode number, -1 when unknown.
	int year;							//Release year, -1 when unknown.
	bool is_series;					//Path or filename identifies a series episode.
	bool episode_known;				//Episode has an explicit number.
} Lib_name_info;

//Extracts a display title, season/episode and year from a file name/path.
//title_out is always NUL-terminated (truncated when needed).
//season_out, episode_out and year_out may be NULL; they are set to -1 when unknown.
//Returns false only for invalid arguments (NULL path or zero-sized buffer).
bool Lib_name_parse(const char* path, char* title_out, size_t title_size,
int* season_out, int* episode_out, int* year_out);

//Like Lib_name_parse, but also uses season/show folders and exposes episode
//classification separately from the optional episode number.
bool Lib_name_parse_info(const char* path, char* title_out, size_t title_size,
char* episode_title_out, size_t episode_title_size, Lib_name_info* info);

//Path helpers (no allocation).
const char* Lib_path_basename(const char* path);

//Reproduces the old saved_pos file name: full path with '/' replaced by '_'.
//Returns false when the result does not fit.
bool Lib_path_old_saved_pos_name(const char* path, char* out, size_t out_size);

#endif //!defined(DEF_LIB_NAME_H)
