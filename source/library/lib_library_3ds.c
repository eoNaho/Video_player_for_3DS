//Includes.
#include "library/lib_library_3ds.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "3ds.h"

#include "library/lib_migrate_3ds.h"
#include "library/lib_scan.h"
#include "library/lib_store.h"
#include "system/util/err_types.h"
#include "system/util/sync.h"
#include "system/util/thread_types.h"
#include "system/util/util.h"

//Defines.
#define LIB_LIBRARY_ROOT_MAX				(uint32_t)(1024)
#define LIB_LIBRARY_PATH_MAX				(uint32_t)(1024)
#define LIB_LIBRARY_IDLE_SLEEP_US			(uint64_t)(50000)
#define LIB_LIBRARY_PAUSE_SLEEP_US			(uint64_t)(100000)

//Typedefs.
//N/A.

//Prototypes.
static bool Lib_library_copy_string(char* destination, size_t destination_size,
const char* source);
static void Lib_library_thread(void* arg);
static bool Lib_library_scan_progress(const Lib_scan_3ds_stats* stats,
const char* current_dir, void* userdata);
static bool Lib_library_apply_progress(const char* path, uint32_t position_ms,
uint32_t duration_ms);
static bool Lib_library_save_internal(void);

//Variables.
static Lib_index library_index = { 0, };
static Lib_scan_3ds_stats library_scan_stats = { 0, };
static Thread library_thread = 0;
static bool library_inited = false;
static volatile bool library_run = false;
static volatile bool library_paused = false;
static volatile bool library_cancel = false;
static volatile bool library_scan_request = false;
static volatile bool library_save_request = false;
static volatile bool library_scanning = false;
static volatile Lib_library_state library_state = LIB_LIBRARY_STATE_IDLE;
static bool library_migrate_after_scan = false;
static char library_root[LIB_LIBRARY_ROOT_MAX] = { 0, };
static char library_pending_path[LIB_LIBRARY_PATH_MAX] = { 0, };
static char library_pending_probe_path[LIB_LIBRARY_PATH_MAX] = { 0, };
static uint32_t library_pending_position = 0;
static uint32_t library_pending_duration = 0;
static bool library_pending_valid = false;
static Lib_probe_info library_pending_probe = { 0, };
static bool library_pending_probe_valid = false;
static Sync_data library_lock = { 0, };
static bool library_lock_inited = false;

//Code.
static bool Lib_library_copy_string(char* destination, size_t destination_size,
const char* source)
{
	size_t length = 0;

	if (!destination || destination_size == 0 || !source)
		return false;

	length = strlen(source);
	if (length >= destination_size)
		return false;

	memcpy(destination, source, length + 1);
	return true;
}

uint32_t Lib_library_init(void)
{
	bool loaded = false;

	if (library_inited)
		return DEF_ERR_ALREADY_INITIALIZED;

	Lib_index_init(&library_index);
	memset(&library_scan_stats, 0, sizeof(library_scan_stats));
	library_pending_valid = false;
	library_pending_probe_valid = false;
	memset(&library_pending_probe, 0, sizeof(library_pending_probe));
	library_pending_path[0] = 0x00;
	library_pending_probe_path[0] = 0x00;
	library_cancel = false;
	library_paused = false;
	library_scanning = false;
	library_save_request = false;

	if (Util_sync_create(&library_lock, SYNC_TYPE_NON_RECURSIVE_MUTEX) != DEF_SUCCESS)
	{
		Lib_index_free(&library_index);
		return DEF_ERR_OTHER;
	}
	library_lock_inited = true;

	Lib_store_ensure_dirs();

	loaded = (bool)(Lib_store_load(&library_index) == LIB_INDEX_OK);
	if (loaded)
		(void)Lib_migrate_3ds_saved_pos(&library_index, NULL);

	library_migrate_after_scan = (bool)(!loaded || library_index.count == 0);

	(void)Lib_library_copy_string(library_root, sizeof(library_root),
	LIB_LIBRARY_DEFAULT_ROOT);

	library_run = true;
	library_thread = threadCreate(Lib_library_thread, NULL, DEF_THREAD_STACKSIZE,
	DEF_THREAD_PRIORITY_IDLE, 0, false);
	if (!library_thread)
	{
		library_run = false;
		library_lock_inited = false;
		Util_sync_destroy(&library_lock);
		Lib_index_free(&library_index);
		return DEF_ERR_OTHER;
	}

	library_inited = true;
	library_state = LIB_LIBRARY_STATE_IDLE;

	(void)Lib_library_request_scan(NULL);
	return DEF_SUCCESS;
}

void Lib_library_exit(void)
{
	if (!library_inited)
		return;

	library_run = false;
	library_cancel = true;
	library_paused = false;

	if (library_thread)
	{
		threadJoin(library_thread, DEF_THREAD_WAIT_TIME);
		threadFree(library_thread);
		library_thread = 0;
	}

	library_scanning = false;
	library_inited = false;

	(void)Lib_library_save_internal();
	if (library_lock_inited)
	{
		library_lock_inited = false;
		Util_sync_destroy(&library_lock);
	}
	Lib_index_free(&library_index);
	memset(&library_scan_stats, 0, sizeof(library_scan_stats));
}

bool Lib_library_is_initialized(void)
{
	return library_inited;
}

bool Lib_library_request_scan(const char* root)
{
	const char* source = root ? root : LIB_LIBRARY_DEFAULT_ROOT;

	if (!library_inited || library_scanning || library_scan_request)
		return false;

	if (!Lib_library_copy_string(library_root, sizeof(library_root), source))
		return false;

	library_cancel = false;
	library_state = LIB_LIBRARY_STATE_QUEUED;
	library_scan_request = true;
	return true;
}

void Lib_library_set_paused(bool paused)
{
	library_paused = paused;
}

Lib_library_state Lib_library_get_state(void)
{
	return library_state;
}

bool Lib_library_is_busy(void)
{
	return (bool)(library_scanning || library_scan_request);
}

const Lib_scan_3ds_stats* Lib_library_get_scan_stats(void)
{
	return &library_scan_stats;
}

const Lib_index* Lib_library_index(void)
{
	return &library_index;
}

static bool Lib_library_scan_progress(const Lib_scan_3ds_stats* stats,
const char* current_dir, void* userdata)
{
	(void)current_dir;
	(void)userdata;

	if (stats)
		library_scan_stats = *stats;

	while (library_paused && library_run && !library_cancel)
		Util_sleep(LIB_LIBRARY_PAUSE_SLEEP_US);

	return (bool)(library_run && !library_cancel);
}

static bool Lib_library_apply_progress(const char* path, uint32_t position_ms,
uint32_t duration_ms)
{
	Lib_item* item = NULL;
	uint32_t now = (uint32_t)time(NULL);
	bool result = false;

	if (!path || path[0] == 0x00)
		return false;
	if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
		return false;

	item = Lib_index_find_path(&library_index, path);
	if (!item)
	{
		uint64_t id = Lib_index_hash_id(path, 0);

		item = Lib_index_upsert(&library_index, path, 0, 0, id);
		if (!item)
			goto out;
	}

	if (duration_ms > 0)
		item->duration_ms = duration_ms;

	Lib_item_update_progress(item, position_ms);
	item->last_played = now;
	if (item->play_count < UINT32_MAX)
		item->play_count++;

	result = true;

out:
	Util_sync_unlock(&library_lock);
	return result;
}

static bool Lib_library_apply_probe(const char* path, const Lib_probe_info* info)
{
	Lib_item* item = NULL;
	bool result = false;

	if (!path || path[0] == 0x00 || !info)
		return false;
	if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
		return false;

	item = Lib_index_find_path(&library_index, path);
	if (!item)
	{
		uint64_t id = Lib_index_hash_id(path, 0);

		item = Lib_index_upsert(&library_index, path, 0, 0, id);
		if (!item)
			goto out;
	}

	Lib_probe_apply(item, info, LIB_PROBE_WATCHED_GUARD_MS);
	result = true;

out:
	Util_sync_unlock(&library_lock);
	return result;
}

static bool Lib_library_save_internal(void)
{
	bool result = false;

	if (!library_inited && library_index.items == NULL)
		return false;

	if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
		return false;

	result = (bool)(Lib_store_save(&library_index) == LIB_INDEX_OK);
	Util_sync_unlock(&library_lock);
	return result;
}

bool Lib_library_save(void)
{
	if (!library_inited)
		return false;

	if (library_scanning)
	{
		library_save_request = true;
		return false;
	}

	if (Lib_library_save_internal())
	{
		library_save_request = false;
		return true;
	}

	return false;
}

bool Lib_library_update_progress(const char* path, uint32_t position_ms,
uint32_t duration_ms)
{
	if (!library_inited || !path || path[0] == 0x00)
		return false;

	if (library_scanning)
	{
		size_t length = strlen(path);

		if (length >= sizeof(library_pending_path))
			return false;

		if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
			return false;
		if (!library_scanning)
		{
			Util_sync_unlock(&library_lock);
			goto apply_now;
		}
		memcpy(library_pending_path, path, length + 1);
		library_pending_position = position_ms;
		library_pending_duration = duration_ms;
		library_pending_valid = true;
		Util_sync_unlock(&library_lock);
		return true;
	}

apply_now:
	if (!Lib_library_apply_progress(path, position_ms, duration_ms))
		return false;

	library_save_request = true;
	return true;
}

bool Lib_library_update_probe(const char* path, const Lib_probe_info* info)
{
	if (!library_inited || !path || path[0] == 0x00 || !info)
		return false;

	if (library_scanning)
	{
		if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
			return false;
		if (!library_scanning)
		{
			Util_sync_unlock(&library_lock);
			goto apply_now;
		}
		if (!Lib_library_copy_string(library_pending_probe_path,
		sizeof(library_pending_probe_path), path))
		{
			Util_sync_unlock(&library_lock);
			return false;
		}

		library_pending_probe = *info;
		library_pending_probe_valid = true;
		Util_sync_unlock(&library_lock);
		return true;
	}

apply_now:
	if (!Lib_library_apply_probe(path, info))
		return false;

	library_save_request = true;
	return true;
}

bool Lib_library_get_progress(const char* path, uint32_t* position_ms,
uint32_t* duration_ms)
{
	const Lib_item* item = NULL;
	bool result = false;

	if (position_ms)
		*position_ms = 0;
	if (duration_ms)
		*duration_ms = 0;
	if (!library_inited || !path)
		return false;
	if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
		return false;

	item = Lib_index_find_path_const(&library_index, path);
	if (!item)
		goto out;

	if (position_ms)
		*position_ms = item->position_ms;
	if (duration_ms)
		*duration_ms = item->duration_ms;
	result = true;

out:
	Util_sync_unlock(&library_lock);
	return result;
}

bool Lib_library_set_favorite(uint64_t id, bool favorite)
{
	Lib_item* item = NULL;

	if (!library_inited || library_scanning)
		return false;
	if (!library_lock_inited || Util_sync_lock(&library_lock, UINT64_MAX) != DEF_SUCCESS)
		return false;

	item = Lib_index_find_id(&library_index, id);
	if (!item)
	{
		Util_sync_unlock(&library_lock);
		return false;
	}

	if (favorite)
		item->flags |= LIB_ITEM_FLAG_FAVORITE;
	else
		item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_FAVORITE;

	library_save_request = true;
	Util_sync_unlock(&library_lock);
	return true;
}

static void Lib_library_thread(void* arg)
{
	(void)arg;

	while (library_run)
	{
		if (library_scan_request && !library_scanning)
		{
			Lib_scan_files files = { 0, };
			Lib_scan_3ds_options options = { 0, };
			Lib_scan_stats diff = { 0, };
			uint64_t now = 0;
			char root[LIB_LIBRARY_ROOT_MAX] = { 0, };
			bool ok = false;

			library_scanning = true;
			library_state = LIB_LIBRARY_STATE_SCANNING;
			library_scan_request = false;
			memset(&library_scan_stats, 0, sizeof(library_scan_stats));

			(void)Lib_library_copy_string(root, sizeof(root), library_root);

			Lib_scan_files_init(&files);
			Lib_scan_3ds_options_init(&options);
			options.max_depth = LIB_LIBRARY_SCAN_DEPTH;
			options.include_hidden = false;
			options.progress = Lib_library_scan_progress;
			options.userdata = NULL;

			ok = Lib_scan_3ds_collect(root, &options, &files, &library_scan_stats);
			if (ok && library_run && !library_cancel)
			{
				now = (uint64_t)time(NULL);
				if (library_lock_inited && Util_sync_lock(&library_lock, UINT64_MAX) == DEF_SUCCESS)
				{
					ok = Lib_scan_apply(&library_index, &files, now,
					LIB_LIBRARY_MISSING_GRACE_S, &diff);
					Util_sync_unlock(&library_lock);
				}
				else
					ok = false;
			}
			else
				ok = false;

			Lib_scan_files_free(&files);

			if (!library_run)
				library_state = LIB_LIBRARY_STATE_IDLE;
			else if (ok)
			{
				library_state = LIB_LIBRARY_STATE_READY;

				if (library_migrate_after_scan)
				{
					if (library_lock_inited && Util_sync_lock(&library_lock, UINT64_MAX) == DEF_SUCCESS)
					{
						(void)Lib_migrate_3ds_saved_pos(&library_index, NULL);
						Util_sync_unlock(&library_lock);
					}
					library_migrate_after_scan = false;
				}
			}
			else
				library_state = LIB_LIBRARY_STATE_ERROR;

			{
				char pending_path[LIB_LIBRARY_PATH_MAX] = { 0, };
				uint32_t pending_position = 0;
				uint32_t pending_duration = 0;
				bool pending_progress = false;
				char pending_probe_path[LIB_LIBRARY_PATH_MAX] = { 0, };
				Lib_probe_info pending_probe = { 0, };
				bool pending_probe_update = false;

				//Transfer deferred updates atomically with respect to callers that
				//may observe the scan finishing at the same time.
				if (library_lock_inited && Util_sync_lock(&library_lock, UINT64_MAX) == DEF_SUCCESS)
				{
					pending_progress = library_pending_valid;
					if (pending_progress)
					{
						(void)Lib_library_copy_string(pending_path, sizeof(pending_path),
						library_pending_path);
						pending_position = library_pending_position;
						pending_duration = library_pending_duration;
					}
					library_pending_valid = false;
					library_pending_path[0] = 0x00;

					pending_probe_update = library_pending_probe_valid;
					if (pending_probe_update)
					{
						(void)Lib_library_copy_string(pending_probe_path,
						sizeof(pending_probe_path), library_pending_probe_path);
						pending_probe = library_pending_probe;
					}
					library_pending_probe_valid = false;
					library_pending_probe_path[0] = 0x00;
					memset(&library_pending_probe, 0, sizeof(library_pending_probe));
					//Close the hand-off while still holding the lock. A caller that
					//arrives after this point applies directly instead of enqueueing
					//behind an already-transferred update.
					library_scanning = false;
					Util_sync_unlock(&library_lock);
				}

				if (pending_progress)
				{
					(void)Lib_library_apply_progress(pending_path, pending_position,
					pending_duration);
					library_save_request = true;
				}

				if (pending_probe_update)
				{
					(void)Lib_library_apply_probe(pending_probe_path, &pending_probe);
					library_save_request = true;
				}
			}

			library_cancel = false;

			if (library_run && (library_state == LIB_LIBRARY_STATE_READY))
				(void)Lib_library_save_internal();

			library_save_request = false;
		}
		else if (library_save_request && !library_scanning)
		{
			if (!Lib_library_save())
				Util_sleep(LIB_LIBRARY_IDLE_SLEEP_US);
		}
		else
			Util_sleep(LIB_LIBRARY_IDLE_SLEEP_US);
	}
}
