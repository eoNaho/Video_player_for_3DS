//Includes.
#include "library/lib_scan_3ds.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "3ds.h"
#include "3ds/util/utf.h"

//Defines.
#define LIB_SCAN_3DS_MAX_PATH				(uint32_t)(2048)
#define LIB_SCAN_3DS_SCRATCH_ENTRIES		(uint32_t)(8)

//Typedefs.
typedef struct
{
	char* name;
	uint32_t attributes;
} Lib_scan_3ds_name;

typedef struct
{
	FS_Archive archive;
	FS_DirectoryEntry scratch[LIB_SCAN_3DS_SCRATCH_ENTRIES];
	Lib_scan_files* files;
	Lib_scan_3ds_options options;
	Lib_scan_3ds_stats stats;
	bool cancelled;
} Lib_scan_3ds_ctx;

//Prototypes.
static uint16_t* Lib_scan_3ds_to_utf16(const char* path);
static char* Lib_scan_3ds_join(const char* dir, const char* name);
static bool Lib_scan_3ds_is_dot_entry(const char* name);
static bool Lib_scan_3ds_is_system_dir(const char* name);
static bool Lib_scan_3ds_should_skip_dir(const Lib_scan_3ds_ctx* ctx, const char* dir,
const char* name, uint32_t attributes);
static bool Lib_scan_3ds_read_names(Lib_scan_3ds_ctx* ctx, const char* dir,
Lib_scan_3ds_name** names, size_t* count);
static void Lib_scan_3ds_free_names(Lib_scan_3ds_name* names, size_t count);
static bool Lib_scan_3ds_file_size(FS_Archive archive, const char* path, uint64_t* size);
static bool Lib_scan_3ds_scan_dir(Lib_scan_3ds_ctx* ctx, const char* dir, uint32_t depth);

//Variables.
//N/A.

//Code.
void Lib_scan_3ds_options_init(Lib_scan_3ds_options* options)
{
	if (!options)
		return;

	options->max_depth = LIB_SCAN_3DS_DEFAULT_MAX_DEPTH;
	options->include_hidden = false;
	options->progress = NULL;
	options->userdata = NULL;
}

static uint16_t* Lib_scan_3ds_to_utf16(const char* path)
{
	size_t length = 0;
	ssize_t written = 0;
	uint16_t* output = NULL;

	if (!path)
		return NULL;

	length = strlen(path);
	if (length == 0 || length >= LIB_SCAN_3DS_MAX_PATH)
		return NULL;

	output = (uint16_t*)malloc((length + 1) * sizeof(uint16_t));
	if (!output)
		return NULL;

	written = utf8_to_utf16(output, (const uint8_t*)path, length);
	if (written < 0)
	{
		free(output);
		return NULL;
	}

	output[written] = 0x00;
	return output;
}

static char* Lib_scan_3ds_join(const char* dir, const char* name)
{
	size_t dir_length = 0;
	size_t name_length = 0;
	size_t total = 0;
	size_t offset = 0;
	bool is_root = false;
	char* output = NULL;

	if (!dir || !name)
		return NULL;

	dir_length = strlen(dir);
	name_length = strlen(name);
	is_root = (bool)(dir_length == 1 && dir[0] == '/');
	total = dir_length + (is_root ? 0 : 1) + name_length + 1;
	if (total > LIB_SCAN_3DS_MAX_PATH)
		return NULL;

	output = (char*)malloc(total);
	if (!output)
		return NULL;

	memcpy(output, dir, dir_length);
	offset = dir_length;
	if (!is_root)
		output[offset++] = '/';
	memcpy(output + offset, name, name_length + 1);

	return output;
}

static bool Lib_scan_3ds_is_dot_entry(const char* name)
{
	if (!name)
		return true;

	if (name[0] != '.')
		return false;

	//Skip ".", ".." and AppleDouble metadata ("._video.mkv").
	return (bool)(name[1] == 0x00 || (name[1] == '.' && name[2] == 0x00)
	|| name[1] == '_');
}

static bool Lib_scan_3ds_is_system_dir(const char* name)
{
	static const char* const system_dirs[] =
	{
		"Nintendo 3DS", "DCIM", "private", "3ds",
	};

	if (!name)
		return false;

	for (size_t i = 0; i < (sizeof(system_dirs) / sizeof(system_dirs[0])); i++)
	{
		const char* a = name;
		const char* b = system_dirs[i];

		while (*a && *b)
		{
			char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
			char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;

			if (ca != cb)
				break;
			a++;
			b++;
		}

		if (*a == 0x00 && *b == 0x00)
			return true;
	}

	return false;
}

static bool Lib_scan_3ds_should_skip_dir(const Lib_scan_3ds_ctx* ctx, const char* dir,
const char* name, uint32_t attributes)
{
	if (!ctx || !dir || !name)
		return true;

	if (!ctx->options.include_hidden && (attributes & FS_ATTRIBUTE_HIDDEN))
		return true;
	if (name[0] == '.')
		return true;

	//Only protect the SD root; an explicitly requested /3ds is still scanned.
	if (strcmp(dir, "/") == 0 && Lib_scan_3ds_is_system_dir(name))
		return true;

	return false;
}

static bool Lib_scan_3ds_push_name(Lib_scan_3ds_name** names, size_t* count,
size_t* capacity, const char* name, uint32_t attributes)
{
	Lib_scan_3ds_name* new_names = NULL;
	char* copy = NULL;
	size_t length = 0;

	if (!names || !count || !capacity || !name)
		return false;

	if (*count == *capacity)
	{
		size_t new_capacity = (*capacity == 0) ? 32 : (*capacity * 2);

		if (new_capacity < *capacity)
			return false;

		new_names = (Lib_scan_3ds_name*)realloc(*names, new_capacity * sizeof(Lib_scan_3ds_name));
		if (!new_names)
			return false;

		*names = new_names;
		*capacity = new_capacity;
	}

	length = strlen(name);
	copy = (char*)malloc(length + 1);
	if (!copy)
		return false;
	memcpy(copy, name, length + 1);

	(*names)[*count].name = copy;
	(*names)[*count].attributes = attributes;
	(*count)++;
	return true;
}

static bool Lib_scan_3ds_read_names(Lib_scan_3ds_ctx* ctx, const char* dir,
Lib_scan_3ds_name** names, size_t* count)
{
	uint16_t* utf16_dir = NULL;
	Handle handle = 0;
	uint32_t result = 0;
	size_t capacity = 0;
	bool success = true;
	char utf8_name[256 + 1] = { 0, };

	if (names)
		*names = NULL;
	if (count)
		*count = 0;
	if (!ctx || !dir || !names || !count)
		return false;

	utf16_dir = Lib_scan_3ds_to_utf16(dir);
	if (!utf16_dir)
		return false;

	result = FSUSER_OpenDirectory(&handle, ctx->archive, fsMakePath(PATH_UTF16, utf16_dir));
	free(utf16_dir);
	if (R_FAILED(result))
		return false;

	while (true)
	{
		uint32_t read_count = 0;

		result = FSDIR_Read(handle, &read_count, LIB_SCAN_3DS_SCRATCH_ENTRIES, ctx->scratch);
		if (R_FAILED(result))
		{
			success = false;
			break;
		}

		if (read_count == 0)
			break;

		for (uint32_t i = 0; i < read_count; i++)
		{
			ssize_t written = utf16_to_utf8((uint8_t*)utf8_name, ctx->scratch[i].name,
			sizeof(utf8_name) - 1);
			if (written < 0)
			{
				success = false;
				break;
			}

			utf8_name[written] = 0x00;
			if (!Lib_scan_3ds_push_name(names, count, &capacity, utf8_name,
			ctx->scratch[i].attributes))
			{
				success = false;
				break;
			}
		}

		if (!success)
			break;
	}

	FSDIR_Close(handle);

	if (!success)
		Lib_scan_3ds_free_names(*names, *count);

	return success;
}

static void Lib_scan_3ds_free_names(Lib_scan_3ds_name* names, size_t count)
{
	if (!names)
		return;

	for (size_t i = 0; i < count; i++)
		free(names[i].name);

	free(names);
}

static bool Lib_scan_3ds_file_size(FS_Archive archive, const char* path, uint64_t* size)
{
	uint16_t* utf16_path = NULL;
	Handle handle = 0;
	uint64_t value = 0;
	uint32_t result = 0;

	if (!path || !size)
		return false;

	utf16_path = Lib_scan_3ds_to_utf16(path);
	if (!utf16_path)
		return false;

	result = FSUSER_OpenFile(&handle, archive, fsMakePath(PATH_UTF16, utf16_path),
	FS_OPEN_READ, FS_ATTRIBUTE_ARCHIVE);
	free(utf16_path);
	if (R_FAILED(result))
		return false;

	result = FSFILE_GetSize(handle, &value);
	FSFILE_Close(handle);
	if (R_FAILED(result))
		return false;

	*size = (uint64_t)value;
	return true;
}

static bool Lib_scan_3ds_scan_dir(Lib_scan_3ds_ctx* ctx, const char* dir, uint32_t depth)
{
	Lib_scan_3ds_name* names = NULL;
	size_t count = 0;

	if (!ctx || !dir)
		return false;
	if (ctx->cancelled)
		return true;

	ctx->stats.dirs_visited++;

	if (!Lib_scan_3ds_read_names(ctx, dir, &names, &count))
	{
		ctx->stats.errors++;
		return false;
	}

	if (ctx->options.progress && !ctx->options.progress(&ctx->stats, dir, ctx->options.userdata))
	{
		ctx->cancelled = true;
		Lib_scan_3ds_free_names(names, count);
		return true;
	}

	for (size_t i = 0; i < count; i++)
	{
		Lib_scan_3ds_name* entry = &names[i];
		char* path = NULL;

		if (ctx->cancelled)
			break;

		if (Lib_scan_3ds_is_dot_entry(entry->name))
		{
			ctx->stats.skipped++;
			continue;
		}

		path = Lib_scan_3ds_join(dir, entry->name);
		if (!path)
		{
			ctx->stats.errors++;
			continue;
		}

		if (entry->attributes & FS_ATTRIBUTE_DIRECTORY)
		{
			if (Lib_scan_3ds_should_skip_dir(ctx, dir, entry->name, entry->attributes)
			|| depth >= ctx->options.max_depth)
			{
				ctx->stats.skipped++;
			}
			else
			{
				(void)Lib_scan_3ds_scan_dir(ctx, path, depth + 1);
			}
		}
		else
		{
			Lib_scan_kind kind = LIB_SCAN_KIND_OTHER;
			uint64_t size = 0;

			if (!ctx->options.include_hidden && (entry->attributes & FS_ATTRIBUTE_HIDDEN))
			{
				ctx->stats.skipped++;
				free(path);
				continue;
			}

			kind = Lib_scan_classify(path);
			if (kind != LIB_SCAN_KIND_VIDEO)
			{
				ctx->stats.skipped++;
				free(path);
				continue;
			}

			if (!Lib_scan_3ds_file_size(ctx->archive, path, &size))
			{
				ctx->stats.errors++;
				free(path);
				continue;
			}

			if (!Lib_scan_files_add(ctx->files, path, size, 0, kind))
			{
				ctx->stats.errors++;
				free(path);
				continue;
			}

			ctx->stats.files_found++;

			if (ctx->options.progress
			&& !ctx->options.progress(&ctx->stats, dir, ctx->options.userdata))
			{
				ctx->cancelled = true;
			}
		}

		free(path);
	}

	Lib_scan_3ds_free_names(names, count);
	return true;
}

bool Lib_scan_3ds_collect(const char* root, const Lib_scan_3ds_options* options,
Lib_scan_files* files, Lib_scan_3ds_stats* stats)
{
	Lib_scan_3ds_ctx* ctx = NULL;
	char* normalized = NULL;
	size_t length = 0;
	uint32_t result = 0;
	bool success = false;

	if (stats)
		memset(stats, 0, sizeof(*stats));
	if (!root || !files)
		return false;

	length = strlen(root);
	if (length == 0 || length >= LIB_SCAN_3DS_MAX_PATH || root[0] != '/')
		return false;

	normalized = (char*)malloc(length + 1);
	if (!normalized)
		return false;
	memcpy(normalized, root, length + 1);
	while (length > 1 && normalized[length - 1] == '/')
		normalized[--length] = 0x00;

	ctx = (Lib_scan_3ds_ctx*)calloc(1, sizeof(Lib_scan_3ds_ctx));
	if (!ctx)
	{
		free(normalized);
		return false;
	}

	if (options)
		ctx->options = *options;
	else
		Lib_scan_3ds_options_init(&ctx->options);

	if (ctx->options.max_depth == 0)
		ctx->options.max_depth = LIB_SCAN_3DS_DEFAULT_MAX_DEPTH;
	ctx->files = files;

	result = FSUSER_OpenArchive(&ctx->archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
	if (R_FAILED(result))
	{
		free(ctx);
		free(normalized);
		return false;
	}

	success = Lib_scan_3ds_scan_dir(ctx, normalized, 0);

	FSUSER_CloseArchive(ctx->archive);

	if (stats)
		*stats = ctx->stats;

	free(ctx);
	free(normalized);
	return success;
}
