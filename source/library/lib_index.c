#include "library/lib_index.h"
#include "library/lib_name.h"

#include <stdlib.h>
#include <string.h>

#define LIB_INDEX_MAGIC					"VPLIB001"
#define LIB_INDEX_FNV64_OFFSET			(uint64_t)(14695981039346656037ULL)
#define LIB_INDEX_FNV64_PRIME			(uint64_t)(1099511628211ULL)
#define LIB_INDEX_FNV32_OFFSET			(uint32_t)(2166136261U)
#define LIB_INDEX_FNV32_PRIME			(uint32_t)(16777619U)

static uint16_t Lib_index_read_u16(const uint8_t* data)
{
	return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t Lib_index_read_u32(const uint8_t* data)
{
	return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint64_t Lib_index_read_u64(const uint8_t* data)
{
	return ((uint64_t)Lib_index_read_u32(data)) | ((uint64_t)Lib_index_read_u32(data + 4) << 32);
}

static void Lib_index_write_u16(uint8_t* data, uint16_t value)
{
	data[0] = (uint8_t)(value & 0xFF);
	data[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void Lib_index_write_u32(uint8_t* data, uint32_t value)
{
	data[0] = (uint8_t)(value & 0xFF);
	data[1] = (uint8_t)((value >> 8) & 0xFF);
	data[2] = (uint8_t)((value >> 16) & 0xFF);
	data[3] = (uint8_t)((value >> 24) & 0xFF);
}

static void Lib_index_write_u64(uint8_t* data, uint64_t value)
{
	Lib_index_write_u32(data, (uint32_t)(value & 0xFFFFFFFFULL));
	Lib_index_write_u32(data + 4, (uint32_t)(value >> 32));
}

static uint32_t Lib_index_checksum(const uint8_t* data, size_t size)
{
	uint32_t hash = LIB_INDEX_FNV32_OFFSET;

	for (size_t i = 0; i < size; i++)
	{
		hash ^= data[i];
		hash *= LIB_INDEX_FNV32_PRIME;
	}

	return hash;
}

static bool Lib_index_grow(Lib_index* index)
{
	size_t new_capacity = 0;
	Lib_item* items = NULL;

	if (index->capacity == 0)
		new_capacity = 16;
	else
	{
		if (index->capacity > (SIZE_MAX / 2))
			return false;
		new_capacity = index->capacity * 2;
	}

	items = (Lib_item*)realloc(index->items, new_capacity * sizeof(Lib_item));
	if (!items)
		return false;

	memset(&items[index->capacity], 0, (new_capacity - index->capacity) * sizeof(Lib_item));
	index->items = items;
	index->capacity = new_capacity;
	return true;
}

static bool Lib_index_set_string(char** target, const char* value)
{
	char* copy = NULL;

	if (value)
	{
		size_t length = strlen(value);
		copy = (char*)malloc(length + 1);
		if (!copy)
			return false;
		memcpy(copy, value, length + 1);
	}

	free(*target);
	*target = copy;
	return true;
}

uint64_t Lib_index_hash_id(const char* name, uint64_t size)
{
	uint64_t hash = LIB_INDEX_FNV64_OFFSET;
	const char* basename = Lib_path_basename(name);

	for (const char* p = basename; *p; p++)
	{
		hash ^= (uint8_t)*p;
		hash *= LIB_INDEX_FNV64_PRIME;
	}

	for (uint8_t i = 0; i < 8; i++)
	{
		hash ^= (uint8_t)((size >> (i * 8)) & 0xFF);
		hash *= LIB_INDEX_FNV64_PRIME;
	}

	return hash;
}

void Lib_index_init(Lib_index* index)
{
	if (index)
		memset(index, 0, sizeof(*index));
}

void Lib_item_clear(Lib_item* item)
{
	if (!item)
		return;

	free(item->path);
	free(item->title);
	memset(item, 0, sizeof(*item));
}

void Lib_index_free(Lib_index* index)
{
	if (!index)
		return;

	for (size_t i = 0; i < index->count; i++)
		Lib_item_clear(&index->items[i]);

	free(index->items);
	memset(index, 0, sizeof(*index));
}

Lib_item* Lib_index_find_id(Lib_index* index, uint64_t id)
{
	if (!index)
		return NULL;

	for (size_t i = 0; i < index->count; i++)
	{
		if (index->items[i].id == id)
			return &index->items[i];
	}

	return NULL;
}

const Lib_item* Lib_index_find_id_const(const Lib_index* index, uint64_t id)
{
	if (!index)
		return NULL;

	for (size_t i = 0; i < index->count; i++)
	{
		if (index->items[i].id == id)
			return &index->items[i];
	}

	return NULL;
}

Lib_item* Lib_index_find_path(Lib_index* index, const char* path)
{
	if (!index || !path)
		return NULL;

	for (size_t i = 0; i < index->count; i++)
	{
		if (index->items[i].path && strcmp(index->items[i].path, path) == 0)
			return &index->items[i];
	}

	return NULL;
}

const Lib_item* Lib_index_find_path_const(const Lib_index* index, const char* path)
{
	if (!index || !path)
		return NULL;

	for (size_t i = 0; i < index->count; i++)
	{
		if (index->items[i].path && strcmp(index->items[i].path, path) == 0)
			return &index->items[i];
	}

	return NULL;
}

Lib_item* Lib_index_upsert(Lib_index* index, const char* path, uint64_t size, uint64_t mtime, uint64_t id)
{
	Lib_item* item = NULL;
	char title[256] = { 0, };

	if (!index || !path || path[0] == 0x00)
		return NULL;

	item = Lib_index_find_id(index, id);
	if (!item)
		item = Lib_index_find_path(index, path);

	if (!item)
	{
		if (!Lib_index_grow(index))
			return NULL;

		item = &index->items[index->count];
		memset(item, 0, sizeof(*item));
		index->count++;
	}

	item->id = id;
	item->size = size;
	item->mtime = mtime;
	item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_MISSING;

	if (!item->path || strcmp(item->path, path) != 0)
	{
		if (!Lib_index_set_string(&item->path, path))
			return NULL;
	}

	if (!item->title || item->title[0] == 0x00)
	{
		Lib_name_parse(path, title, sizeof(title), NULL, NULL, NULL);
		if (!Lib_index_set_string(&item->title, title))
			return NULL;
	}

	return item;
}

bool Lib_index_remove_at(Lib_index* index, size_t position)
{
	if (!index || position >= index->count)
		return false;

	Lib_item_clear(&index->items[position]);

	if (position + 1 < index->count)
	{
		memmove(&index->items[position], &index->items[position + 1],
		(index->count - position - 1) * sizeof(Lib_item));
	}

	index->count--;
	memset(&index->items[index->count], 0, sizeof(Lib_item));
	return true;
}

bool Lib_index_remove_id(Lib_index* index, uint64_t id)
{
	if (!index)
		return false;

	for (size_t i = 0; i < index->count; i++)
	{
		if (index->items[i].id == id)
			return Lib_index_remove_at(index, i);
	}

	return false;
}

bool Lib_item_set_title(Lib_item* item, const char* title)
{
	if (!item)
		return false;

	return Lib_index_set_string(&item->title, title);
}

bool Lib_item_set_path(Lib_item* item, const char* path)
{
	char title[256] = { 0, };

	if (!item || !path)
		return false;

	if (!Lib_index_set_string(&item->path, path))
		return false;

	if (!item->title || item->title[0] == 0x00)
	{
		Lib_name_parse(path, title, sizeof(title), NULL, NULL, NULL);
		if (!Lib_index_set_string(&item->title, title))
			return false;
	}

	return true;
}

const char* Lib_item_get_title(const Lib_item* item)
{
	if (!item)
		return "";

	if (item->title && item->title[0] != 0x00)
		return item->title;

	if (item->path)
		return Lib_path_basename(item->path);

	return "";
}

bool Lib_item_is_watched(const Lib_item* item)
{
	if (!item || item->duration_ms == 0)
		return false;

	return (bool)(((uint64_t)item->position_ms * 100) >= ((uint64_t)item->duration_ms * 90));
}

void Lib_item_update_progress(Lib_item* item, uint32_t position_ms)
{
	if (!item)
		return;

	if (item->duration_ms > 0 && position_ms > item->duration_ms)
		position_ms = item->duration_ms;

	item->position_ms = position_ms;

	if (Lib_item_is_watched(item))
		item->flags |= LIB_ITEM_FLAG_WATCHED;
	else
		item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_WATCHED;
}

Lib_index_result Lib_index_save(const Lib_index* index, uint8_t** data, size_t* size)
{
	uint8_t* buffer = NULL;
	uint8_t* records = NULL;
	uint8_t* strings = NULL;
	uint64_t string_bytes64 = 0;
	size_t string_bytes = 0;
	size_t total_size = 0;
	size_t string_offset = 0;
	uint32_t checksum = 0;

	if (!index || !data || !size)
		return LIB_INDEX_ERR_INVALID;
	if (index->count > LIB_INDEX_MAX_ITEMS)
		return LIB_INDEX_ERR_INVALID;

	for (size_t i = 0; i < index->count; i++)
	{
		const Lib_item* item = &index->items[i];
		const char* title = item->title ? item->title : "";

		if (!item->path)
			return LIB_INDEX_ERR_INVALID;

		string_bytes64 += strlen(item->path) + 1;
		string_bytes64 += strlen(title) + 1;
	}

	if (string_bytes64 > UINT32_MAX)
		return LIB_INDEX_ERR_INVALID;

	string_bytes = (size_t)string_bytes64;
	if (string_bytes > (SIZE_MAX - LIB_INDEX_HEADER_SIZE))
		return LIB_INDEX_ERR_NOMEM;
	if (index->count > ((SIZE_MAX - LIB_INDEX_HEADER_SIZE - string_bytes) / LIB_INDEX_RECORD_SIZE))
		return LIB_INDEX_ERR_NOMEM;

	total_size = LIB_INDEX_HEADER_SIZE + (index->count * LIB_INDEX_RECORD_SIZE) + string_bytes;

	buffer = (uint8_t*)malloc(total_size);
	if (!buffer)
		return LIB_INDEX_ERR_NOMEM;

	records = buffer + LIB_INDEX_HEADER_SIZE;
	strings = records + (index->count * LIB_INDEX_RECORD_SIZE);

	for (size_t i = 0; i < index->count; i++)
	{
		const Lib_item* item = &index->items[i];
		const char* title = item->title ? item->title : "";
		uint8_t* record = records + (i * LIB_INDEX_RECORD_SIZE);
		size_t path_offset = string_offset;
		size_t title_offset = 0;
		size_t path_length = strlen(item->path);
		size_t title_length = strlen(title);

		memcpy(strings + string_offset, item->path, path_length + 1);
		string_offset += path_length + 1;

		title_offset = string_offset;
		memcpy(strings + string_offset, title, title_length + 1);
		string_offset += title_length + 1;

		memset(record, 0, LIB_INDEX_RECORD_SIZE);
		Lib_index_write_u64(record + 0, item->id);
		Lib_index_write_u64(record + 8, item->size);
		Lib_index_write_u64(record + 16, item->mtime);
		Lib_index_write_u32(record + 24, (uint32_t)path_offset);
		Lib_index_write_u32(record + 28, (uint32_t)title_offset);
		Lib_index_write_u32(record + 32, item->duration_ms);
		Lib_index_write_u32(record + 36, item->position_ms);
		Lib_index_write_u32(record + 40, item->last_played);
		Lib_index_write_u32(record + 44, item->play_count);
		Lib_index_write_u32(record + 48, item->first_seen);
		Lib_index_write_u32(record + 52, item->missing_since);
		Lib_index_write_u16(record + 56, item->width);
		Lib_index_write_u16(record + 58, item->height);
		Lib_index_write_u16(record + 60, item->fps_x100);
		record[62] = item->video_codec;
		record[63] = item->audio_tracks;
		record[64] = item->subtitle_tracks;
		record[65] = item->flags;
	}

	checksum = Lib_index_checksum(records, (index->count * LIB_INDEX_RECORD_SIZE) + string_bytes);

	memcpy(buffer, LIB_INDEX_MAGIC, 8);
	Lib_index_write_u16(buffer + 8, LIB_INDEX_VERSION);
	Lib_index_write_u16(buffer + 10, (uint16_t)LIB_INDEX_HEADER_SIZE);
	Lib_index_write_u32(buffer + 12, (uint32_t)index->count);
	Lib_index_write_u32(buffer + 16, (uint32_t)string_bytes);
	Lib_index_write_u32(buffer + 20, checksum);
	Lib_index_write_u32(buffer + 24, 0);
	Lib_index_write_u32(buffer + 28, 0);

	*data = buffer;
	*size = total_size;
	return LIB_INDEX_OK;
}

Lib_index_result Lib_index_load(Lib_index* index, const uint8_t* data, size_t size)
{
	Lib_index temp = { 0, };
	const uint8_t* records = NULL;
	const uint8_t* strings = NULL;
	uint32_t count = 0;
	uint32_t string_bytes = 0;
	uint32_t stored_checksum = 0;
	uint64_t expected_size = 0;

	if (!index || !data)
		return LIB_INDEX_ERR_INVALID;

	if (size < LIB_INDEX_HEADER_SIZE || memcmp(data, LIB_INDEX_MAGIC, 8) != 0)
		return LIB_INDEX_ERR_CORRUPT;

	if (Lib_index_read_u16(data + 8) != LIB_INDEX_VERSION)
		return LIB_INDEX_ERR_VERSION;

	if (Lib_index_read_u16(data + 10) != LIB_INDEX_HEADER_SIZE)
		return LIB_INDEX_ERR_CORRUPT;

	count = Lib_index_read_u32(data + 12);
	string_bytes = Lib_index_read_u32(data + 16);
	stored_checksum = Lib_index_read_u32(data + 20);

	if (count > LIB_INDEX_MAX_ITEMS)
		return LIB_INDEX_ERR_CORRUPT;

	expected_size = (uint64_t)LIB_INDEX_HEADER_SIZE + ((uint64_t)count * LIB_INDEX_RECORD_SIZE) + string_bytes;
	if (expected_size != size)
		return LIB_INDEX_ERR_CORRUPT;

	records = data + LIB_INDEX_HEADER_SIZE;
	strings = records + ((size_t)count * LIB_INDEX_RECORD_SIZE);

	if (Lib_index_checksum(records, ((size_t)count * LIB_INDEX_RECORD_SIZE) + string_bytes) != stored_checksum)
		return LIB_INDEX_ERR_CORRUPT;

	if (count > 0)
	{
		temp.items = (Lib_item*)calloc(count, sizeof(Lib_item));
		if (!temp.items)
			return LIB_INDEX_ERR_NOMEM;
		temp.count = count;
		temp.capacity = count;
	}

	for (uint32_t i = 0; i < count; i++)
	{
		const uint8_t* record = records + ((size_t)i * LIB_INDEX_RECORD_SIZE);
		Lib_item* item = &temp.items[i];
		uint32_t path_offset = Lib_index_read_u32(record + 24);
		uint32_t title_offset = Lib_index_read_u32(record + 28);
		const char* path = NULL;
		const char* title = NULL;
		const uint8_t* path_end = NULL;
		const uint8_t* title_end = NULL;

		if (path_offset >= string_bytes || title_offset >= string_bytes)
			goto Lib_index_load_failed;

		path = (const char*)(strings + path_offset);
		path_end = (const uint8_t*)memchr(path, 0x00, string_bytes - path_offset);
		if (!path_end || path[0] == 0x00)
			goto Lib_index_load_failed;

		title = (const char*)(strings + title_offset);
		title_end = (const uint8_t*)memchr(title, 0x00, string_bytes - title_offset);
		if (!title_end)
			goto Lib_index_load_failed;

		item->id = Lib_index_read_u64(record + 0);
		item->size = Lib_index_read_u64(record + 8);
		item->mtime = Lib_index_read_u64(record + 16);
		item->duration_ms = Lib_index_read_u32(record + 32);
		item->position_ms = Lib_index_read_u32(record + 36);
		item->last_played = Lib_index_read_u32(record + 40);
		item->play_count = Lib_index_read_u32(record + 44);
		item->first_seen = Lib_index_read_u32(record + 48);
		item->missing_since = Lib_index_read_u32(record + 52);
		item->width = Lib_index_read_u16(record + 56);
		item->height = Lib_index_read_u16(record + 58);
		item->fps_x100 = Lib_index_read_u16(record + 60);
		item->video_codec = record[62];
		item->audio_tracks = record[63];
		item->subtitle_tracks = record[64];
		item->flags = record[65];

		if (!Lib_index_set_string(&item->path, path))
			goto Lib_index_load_failed;
		if (!Lib_index_set_string(&item->title, title))
			goto Lib_index_load_failed;
	}

	Lib_index_free(index);
	*index = temp;
	return LIB_INDEX_OK;

Lib_index_load_failed:
	Lib_index_free(&temp);
	return LIB_INDEX_ERR_CORRUPT;
}
