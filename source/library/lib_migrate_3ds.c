#include "library/lib_migrate_3ds.h"
#include "library/lib_migrate.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "system/menu.h"
#include "system/util/err_types.h"
#include "system/util/file.h"

bool Lib_migrate_3ds_read_position(const char* old_name, uint64_t* position_ms, void* userdata)
{
	uint8_t* data = NULL;
	uint32_t read_size = 0;
	uint32_t result = DEF_ERR_OTHER;
	double seconds = 0;

	(void)userdata;

	if (!old_name || !position_ms)
		return false;

	result = Util_file_load_from_file(old_name, DEF_MENU_MAIN_DIR "saved_pos/", &data,
	sizeof(double), 0, &read_size);
	if (result != DEF_SUCCESS || !data || read_size < sizeof(double))
	{
		free(data);
		return false;
	}

	memcpy(&seconds, data, sizeof(double));
	free(data);

	if (!(seconds > 0.0))
		return false;
	if (seconds > ((double)UINT64_MAX / 1000.0))
		return false;

	*position_ms = (uint64_t)(seconds * 1000.0);
	return true;
}

bool Lib_migrate_3ds_saved_pos(Lib_index* index, size_t* migrated)
{
	Lib_migrate_stats stats = { 0, };
	bool result = false;

	if (migrated)
		*migrated = 0;
	if (!index)
		return false;

	result = Lib_migrate_saved_pos(index, Lib_migrate_3ds_read_position, NULL, &stats);
	if (migrated)
		*migrated = stats.migrated;

	return result;
}
