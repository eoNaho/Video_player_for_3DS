#include "library/lib_scan.h"
#include "library/lib_name.h"

#include <stdlib.h>
#include <string.h>

static const char* const Lib_scan_video_extensions[] =
{
	"avi", "m4v", "mkv", "mov", "mp4", "webm",
};

static const char* const Lib_scan_audio_extensions[] =
{
	"aac", "ac3", "m4a", "mp1", "mp2", "mp3", "ogg", "wav",
};

static char Lib_scan_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (char)(c - 'A' + 'a');
	return c;
}

static bool Lib_scan_extension_is(const char* extension, const char* const* list, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		const char* a = extension;
		const char* b = list[i];

		while (*a && *b && Lib_scan_lower(*a) == Lib_scan_lower(*b))
		{
			a++;
			b++;
		}

		if (*a == 0x00 && *b == 0x00)
			return true;
	}

	return false;
}

Lib_scan_kind Lib_scan_classify(const char* path)
{
	const char* basename = NULL;
	const char* extension = NULL;

	if (!path)
		return LIB_SCAN_KIND_OTHER;

	basename = Lib_path_basename(path);
	extension = strrchr(basename, '.');

	if (!extension || extension[1] == 0x00)
		return LIB_SCAN_KIND_OTHER;

	extension++;

	if (Lib_scan_extension_is(extension, Lib_scan_video_extensions,
	sizeof(Lib_scan_video_extensions) / sizeof(Lib_scan_video_extensions[0])))
		return LIB_SCAN_KIND_VIDEO;

	if (Lib_scan_extension_is(extension, Lib_scan_audio_extensions,
	sizeof(Lib_scan_audio_extensions) / sizeof(Lib_scan_audio_extensions[0])))
		return LIB_SCAN_KIND_AUDIO;

	return LIB_SCAN_KIND_OTHER;
}

bool Lib_scan_is_supported(const char* path)
{
	return (bool)(Lib_scan_classify(path) != LIB_SCAN_KIND_OTHER);
}

void Lib_scan_files_init(Lib_scan_files* files)
{
	if (files)
		memset(files, 0, sizeof(*files));
}

void Lib_scan_files_free(Lib_scan_files* files)
{
	if (!files)
		return;

	for (size_t i = 0; i < files->count; i++)
		free(files->files[i].path);

	free(files->files);
	memset(files, 0, sizeof(*files));
}

bool Lib_scan_files_add(Lib_scan_files* files, const char* path, uint64_t size, uint64_t mtime, Lib_scan_kind kind)
{
	Lib_scan_file* file = NULL;
	char* copy = NULL;
	size_t length = 0;

	if (!files || !path)
		return false;

	if (files->count == files->capacity)
	{
		size_t new_capacity = (files->capacity == 0) ? 32 : files->capacity * 2;
		Lib_scan_file* new_files = NULL;

		if (new_capacity < files->capacity)
			return false;

		new_files = (Lib_scan_file*)realloc(files->files, new_capacity * sizeof(Lib_scan_file));
		if (!new_files)
			return false;

		files->files = new_files;
		files->capacity = new_capacity;
	}

	length = strlen(path);
	copy = (char*)malloc(length + 1);
	if (!copy)
		return false;
	memcpy(copy, path, length + 1);

	file = &files->files[files->count];
	file->path = copy;
	file->size = size;
	file->mtime = mtime;
	file->kind = kind;
	files->count++;

	return true;
}

static bool Lib_scan_same_directory(const char* a, const char* b)
{
	const char* basename_a = Lib_path_basename(a);
	const char* basename_b = Lib_path_basename(b);
	size_t length_a = (size_t)(basename_a - a);
	size_t length_b = (size_t)(basename_b - b);

	if (length_a != length_b)
		return false;

	return (bool)(strncmp(a, b, length_a) == 0);
}

static Lib_item* Lib_scan_find_renamed(Lib_index* index, const Lib_scan_file* file,
const bool* seen, size_t seen_count)
{
	Lib_item* candidate = NULL;

	for (size_t i = 0; i < seen_count; i++)
	{
		Lib_item* item = &index->items[i];

		if (seen[i])
			continue;
		if (item->size != file->size)
			continue;
		if (!item->path || !Lib_scan_same_directory(item->path, file->path))
			continue;
		if (candidate)
			return NULL;//Ambiguous, do not steal history.

		candidate = item;
	}

	return candidate;
}

bool Lib_scan_apply(Lib_index* index, const Lib_scan_files* files, uint64_t now,
uint32_t missing_grace_seconds, Lib_scan_stats* stats)
{
	Lib_scan_stats local_stats = { 0, };
	bool* seen = NULL;
	size_t initial_count = 0;
	uint64_t* remove_ids = NULL;
	size_t remove_count = 0;

	if (stats)
		*stats = local_stats;
	if (!index || !files)
		return false;

	initial_count = index->count;
	seen = (bool*)calloc((initial_count > 0) ? initial_count : 1, sizeof(bool));
	if (!seen)
		return false;

	if (initial_count > 0)
	{
		remove_ids = (uint64_t*)malloc(initial_count * sizeof(uint64_t));
		if (!remove_ids)
		{
			free(seen);
			return false;
		}
	}

	for (size_t i = 0; i < files->count; i++)
	{
		const Lib_scan_file* file = &files->files[i];
		uint64_t id = Lib_index_hash_id(file->path, file->size);
		Lib_item* item = Lib_index_find_id(index, id);
		uint64_t old_size = 0;
		bool renamed = false;

		if (!item)
			item = Lib_index_find_path(index, file->path);

		if (item)
			old_size = item->size;
		else
		{
			item = Lib_scan_find_renamed(index, file, seen, initial_count);
			renamed = (item != NULL);
			if (item)
				old_size = item->size;
		}

		if (!item)
		{
			item = Lib_index_upsert(index, file->path, file->size, file->mtime, id);
			if (!item)
			{
				free(remove_ids);
				free(seen);
				return false;
			}
			local_stats.added++;
		}
		else if (renamed)
		{
			//Keep the record (position, play count, favorite) under the new name.
			item->id = id;
			item->size = file->size;
			item->mtime = file->mtime;
			if (!Lib_item_set_path(item, file->path))
			{
				free(remove_ids);
				free(seen);
				return false;
			}
			local_stats.renamed++;
		}
		else
		{
			bool size_changed = (old_size != file->size);

			item = Lib_index_upsert(index, file->path, file->size, file->mtime, id);
			if (!item)
			{
				free(remove_ids);
				free(seen);
				return false;
			}

			if (size_changed || item->missing_since != 0)
				local_stats.updated++;
			else
				local_stats.unchanged++;
		}

		item->mtime = file->mtime;
		item->missing_since = 0;
		item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_MISSING;
		if (item->first_seen == 0 && now > 0 && now <= UINT32_MAX)
			item->first_seen = (uint32_t)now;

		if (file->kind == LIB_SCAN_KIND_AUDIO)
			item->flags |= LIB_ITEM_FLAG_AUDIO_ONLY;
		else
			item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_AUDIO_ONLY;

		if (initial_count > 0)
		{
			size_t position = (size_t)(item - index->items);
			if (position < initial_count)
				seen[position] = true;
		}
	}

	for (size_t i = 0; i < initial_count; i++)
	{
		Lib_item* item = &index->items[i];

		if (seen[i])
			continue;

		if (!(item->flags & LIB_ITEM_FLAG_MISSING))
		{
			item->flags |= LIB_ITEM_FLAG_MISSING;
			if (now > 0 && now <= UINT32_MAX)
				item->missing_since = (uint32_t)now;
			local_stats.marked_missing++;
		}
		else if (missing_grace_seconds > 0 && item->missing_since > 0
		&& now > ((uint64_t)item->missing_since + missing_grace_seconds))
		{
			remove_ids[remove_count] = item->id;
			remove_count++;
		}
	}

	for (size_t i = 0; i < remove_count; i++)
	{
		if (Lib_index_remove_id(index, remove_ids[i]))
			local_stats.removed++;
	}

	free(remove_ids);
	free(seen);

	if (stats)
		*stats = local_stats;
	return true;
}
