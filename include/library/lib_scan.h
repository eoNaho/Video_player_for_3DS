#if !defined(DEF_LIB_SCAN_H)
#define DEF_LIB_SCAN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_index.h"

typedef uint8_t Lib_scan_kind;
#define LIB_SCAN_KIND_OTHER				(Lib_scan_kind)(0)
#define LIB_SCAN_KIND_VIDEO				(Lib_scan_kind)(1)
#define LIB_SCAN_KIND_AUDIO				(Lib_scan_kind)(2)

//Classifies by extension (case-insensitive) according to the README lists.
Lib_scan_kind Lib_scan_classify(const char* path);

bool Lib_scan_is_supported(const char* path);

//A file found on disk, already filtered by Lib_scan_classify.
typedef struct
{
	char* path;							//Owned by Lib_scan_files.
	uint64_t size;
	uint64_t mtime;						//0 when unavailable (the 3DS FS API has no mtime).
	Lib_scan_kind kind;
} Lib_scan_file;

typedef struct
{
	Lib_scan_file* files;
	size_t count;
	size_t capacity;
} Lib_scan_files;

void Lib_scan_files_init(Lib_scan_files* files);

void Lib_scan_files_free(Lib_scan_files* files);

bool Lib_scan_files_add(Lib_scan_files* files, const char* path, uint64_t size, uint64_t mtime, Lib_scan_kind kind);

typedef struct
{
	size_t added;						//New ids.
	size_t updated;						//Same path/id, size changed or record touched.
	size_t unchanged;
	size_t renamed;						//Missing item reused by same dir + size.
	size_t marked_missing;				//Missing this scan.
	size_t removed;						//Missing beyond the grace period.
} Lib_scan_stats;

//Applies a scan result to the index.
//- New files are added, changed files updated, missing files flagged.
//- A file in the same directory and with the same size as an unseen item is
//  treated as a rename and keeps its history.
//- Items missing for longer than missing_grace_seconds are removed
//  (0 or now == 0 keeps them forever).
//Returns false only on allocation failure.
bool Lib_scan_apply(Lib_index* index, const Lib_scan_files* files, uint64_t now,
uint32_t missing_grace_seconds, Lib_scan_stats* stats);

#endif //!defined(DEF_LIB_SCAN_H)
