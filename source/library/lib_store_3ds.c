#include "library/lib_store.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "system/menu.h"
#include "system/util/err_types.h"
#include "system/util/file.h"

#define LIB_STORE_DIR					(DEF_MENU_MAIN_DIR "library/")
#define LIB_STORE_THUMB_DIR				(DEF_MENU_MAIN_DIR "library/thumbs/")
#define LIB_STORE_INDEX_NAME			"index.bin"
#define LIB_STORE_BACKUP_NAME			"index.bak"
#define LIB_STORE_TEMP_NAME				"index.tmp"
#define LIB_STORE_MAX_INDEX_SIZE		(uint32_t)(8 * 1024 * 1024)

static Lib_index_result Lib_store_load_file(Lib_index* index, const char* file_name)
{
	uint8_t* data = NULL;
	uint32_t read_size = 0;
	uint32_t result = DEF_ERR_OTHER;
	Lib_index_result index_result = LIB_INDEX_ERR_IO;

	result = Util_file_load_from_file(file_name, LIB_STORE_DIR, &data, LIB_STORE_MAX_INDEX_SIZE, 0, &read_size);
	if (result != DEF_SUCCESS || !data || read_size == 0)
	{
		free(data);
		return LIB_INDEX_ERR_IO;
	}

	index_result = Lib_index_load(index, data, read_size);
	free(data);
	return index_result;
}

bool Lib_store_ensure_dirs(void)
{
	uint32_t result = DEF_ERR_OTHER;

	result = Util_file_create_directory(LIB_STORE_DIR);
	if (result != DEF_SUCCESS)
		return false;

	result = Util_file_create_directory(LIB_STORE_THUMB_DIR);
	return (bool)(result == DEF_SUCCESS);
}

Lib_index_result Lib_store_load(Lib_index* index)
{
	Lib_index_result result = LIB_INDEX_ERR_INVALID;

	if (!index)
		return LIB_INDEX_ERR_INVALID;

	result = Lib_store_load_file(index, LIB_STORE_INDEX_NAME);
	if (result == LIB_INDEX_OK)
		return result;

	return Lib_store_load_file(index, LIB_STORE_BACKUP_NAME);
}

Lib_index_result Lib_store_save(const Lib_index* index)
{
	uint8_t* data = NULL;
	size_t size = 0;
	uint32_t result = DEF_ERR_OTHER;
	Lib_index_result index_result = LIB_INDEX_ERR_INVALID;
	bool had_index = false;

	if (!index)
		return LIB_INDEX_ERR_INVALID;

	index_result = Lib_index_save(index, &data, &size);
	if (index_result != LIB_INDEX_OK)
		return index_result;

	if (size > UINT32_MAX)
	{
		free(data);
		return LIB_INDEX_ERR_INVALID;
	}

	if (!Lib_store_ensure_dirs())
	{
		free(data);
		return LIB_INDEX_ERR_IO;
	}

	result = Util_file_save_to_file(LIB_STORE_TEMP_NAME, LIB_STORE_DIR, data, (uint32_t)size, true);
	free(data);
	if (result != DEF_SUCCESS)
		return LIB_INDEX_ERR_IO;

	//Rotate: index.bin -> index.bak, index.tmp -> index.bin.
	Util_file_delete_file(LIB_STORE_BACKUP_NAME, LIB_STORE_DIR);
	had_index = (Util_file_rename_file(LIB_STORE_INDEX_NAME, LIB_STORE_DIR, LIB_STORE_BACKUP_NAME) == DEF_SUCCESS);

	result = Util_file_rename_file(LIB_STORE_TEMP_NAME, LIB_STORE_DIR, LIB_STORE_INDEX_NAME);
	if (result != DEF_SUCCESS)
	{
		if (had_index)
			Util_file_rename_file(LIB_STORE_BACKUP_NAME, LIB_STORE_DIR, LIB_STORE_INDEX_NAME);
		Util_file_delete_file(LIB_STORE_TEMP_NAME, LIB_STORE_DIR);
		return LIB_INDEX_ERR_IO;
	}

	return LIB_INDEX_OK;
}
