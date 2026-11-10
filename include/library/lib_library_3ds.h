#if !defined(DEF_LIB_LIBRARY_3DS_H)
#define DEF_LIB_LIBRARY_3DS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "library/lib_index.h"
#include "library/lib_probe.h"
#include "library/lib_scan_3ds.h"

#define LIB_LIBRARY_DEFAULT_ROOT			(const char*)"/"
#define LIB_LIBRARY_SCAN_DEPTH				(uint16_t)(8)
#define LIB_LIBRARY_MISSING_GRACE_S		(uint32_t)(7 * 24 * 60 * 60)

typedef uint8_t Lib_library_state;
#define LIB_LIBRARY_STATE_IDLE				(Lib_library_state)(0)
#define LIB_LIBRARY_STATE_QUEUED			(Lib_library_state)(1)
#define LIB_LIBRARY_STATE_SCANNING			(Lib_library_state)(2)
#define LIB_LIBRARY_STATE_READY				(Lib_library_state)(3)
#define LIB_LIBRARY_STATE_ERROR				(Lib_library_state)(4)

//Owns the index, the background scan thread and persisted storage.
//- Loads library/index.bin at init (an empty/new index is fine).
//- Queues a full SD scan right after init (paused while the player runs).
//- Only the worker thread mutates the index while STATE_SCANNING is set;
//  do not read Lib_library_index() during that state.
//All bool functions return false on invalid state/allocation failure.
uint32_t Lib_library_init(void);

void Lib_library_exit(void);

bool Lib_library_is_initialized(void);

//Queues a recursive scan. root may be NULL to use LIB_LIBRARY_DEFAULT_ROOT.
//Returns false when a scan is already queued/running.
bool Lib_library_request_scan(const char* root);

//Lets the caller pause/resume the worker between files (used during playback).
void Lib_library_set_paused(bool paused);

Lib_library_state Lib_library_get_state(void);

bool Lib_library_is_busy(void);

const Lib_scan_3ds_stats* Lib_library_get_scan_stats(void);

const Lib_index* Lib_library_index(void);

//Persists the index. When the worker is scanning, the save is deferred.
bool Lib_library_save(void);

//Updates an item when playback stops. Creates the record when needed.
//This is the bridge used by video_player.c (replaces saved_pos writes).
bool Lib_library_update_progress(const char* path, uint32_t position_ms,
uint32_t duration_ms);

//Updates metadata discovered by the player/probe path. If a scan is active,
//the update is applied immediately after the scan completes.
bool Lib_library_update_probe(const char* path, const Lib_probe_info* info);

//Returns the persisted resume position for path, or false when not indexed.
bool Lib_library_get_progress(const char* path, uint32_t* position_ms,
uint32_t* duration_ms);

bool Lib_library_set_favorite(uint64_t id, bool favorite);

#endif //!defined(DEF_LIB_LIBRARY_3DS_H)
