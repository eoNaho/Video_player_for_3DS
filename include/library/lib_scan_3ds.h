#if !defined(DEF_LIB_SCAN_3DS_H)
#define DEF_LIB_SCAN_3DS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_scan.h"

//Recommended when the caller has no specific preference.
#define LIB_SCAN_3DS_DEFAULT_MAX_DEPTH		(uint16_t)(8)

typedef struct
{
	size_t dirs_visited;
	size_t files_found;					//Supported files added to Lib_scan_files.
	size_t skipped;						//Hidden, system or unsupported entries.
	size_t errors;						//Paths that could not be read.
} Lib_scan_3ds_stats;

//Called after each supported file is found (and between directories).
//Return false to stop the scan; already collected files stay valid.
typedef bool (*Lib_scan_3ds_progress_cb)(const Lib_scan_3ds_stats* stats,
const char* current_dir, void* userdata);

typedef struct
{
	uint16_t max_depth;					//0 = LIB_SCAN_3DS_DEFAULT_MAX_DEPTH.
	bool include_hidden;				//false = skip hidden attributes and dot entries.
	Lib_scan_3ds_progress_cb progress;
	void* userdata;
} Lib_scan_3ds_options;

void Lib_scan_3ds_options_init(Lib_scan_3ds_options* options);

//Recursively collects supported media under root (e.g. "/Videos") into files.
//Paths are stored without the "sdmc:" prefix, like the built-in explorer.
//- Root itself is always scanned, even when it looks hidden or is a system dir.
//- Directories under the SD root that hold system data are skipped.
//- Symbolic links are not followed (the 3DS FAT driver does not expose them).
//- mtime is always 0: the 3DS file API does not expose it.
//Call it from a background thread; it is not thread safe.
//Returns false on invalid arguments or when the root cannot be opened.
bool Lib_scan_3ds_collect(const char* root, const Lib_scan_3ds_options* options,
Lib_scan_files* files, Lib_scan_3ds_stats* stats);

#endif //!defined(DEF_LIB_SCAN_3DS_H)
