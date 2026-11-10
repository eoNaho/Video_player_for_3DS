#include "library/lib_migrate.h"
#include "library/lib_name.h"

#include <stdlib.h>
#include <string.h>

bool Lib_migrate_saved_pos(Lib_index* index, Lib_migrate_read_cb read_cb, void* userdata,
Lib_migrate_stats* stats)
{
	Lib_migrate_stats local_stats = { 0, };

	if (stats)
		*stats = local_stats;
	if (!index || !read_cb)
		return false;
	if (index->count == 0)
		return true;

	for (size_t i = 0; i < index->count; i++)
	{
		Lib_item* item = &index->items[i];
		char* old_name = NULL;
		uint64_t position_ms = 0;
		size_t length = 0;

		local_stats.items_seen++;

		if (!item->path)
		{
			local_stats.not_found++;
			continue;
		}

		if (item->position_ms > 0)
		{
			local_stats.already_known++;
			continue;
		}

		length = strlen(item->path);
		old_name = (char*)malloc(length + 1);
		if (!old_name)
		{
			if (stats)
				*stats = local_stats;
			return false;
		}

		if (!Lib_path_old_saved_pos_name(item->path, old_name, length + 1))
		{
			free(old_name);
			local_stats.not_found++;
			continue;
		}

		if (read_cb(old_name, &position_ms, userdata) && position_ms > 0)
		{
			item->position_ms = (position_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)position_ms;
			local_stats.migrated++;
		}
		else
			local_stats.not_found++;

		free(old_name);
	}

	if (stats)
		*stats = local_stats;
	return true;
}
