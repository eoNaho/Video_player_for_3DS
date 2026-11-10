#include "library/lib_index.h"
#include "library/lib_name.h"
#include "library/lib_scan.h"
#include "library/lib_sort.h"
#include "library/lib_view.h"
#include "library/lib_queue.h"
#include "library/lib_migrate.h"
#include "library/lib_thumb.h"
#include "library/lib_ui.h"
#include "library/lib_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(condition) \
	do \
	{ \
		tests_run++; \
		if (!(condition)) \
		{ \
			tests_failed++; \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		} \
	} while (0)

static void Test_hash(void)
{
	uint64_t a = Lib_index_hash_id("episode.mkv", 1234);
	uint64_t b = Lib_index_hash_id("sdmc:/shows/episode.mkv", 1234);
	uint64_t c = Lib_index_hash_id("episode.mkv", 1235);
	uint64_t d = Lib_index_hash_id("other.mkv", 1234);

	CHECK(a == b);
	CHECK(a != c);
	CHECK(a != d);
	CHECK(a != 0);
}

static void Test_name(void)
{
	char title[64] = { 0, };
	int season = 0;
	int episode = 0;
	int year = 0;

	CHECK(Lib_name_parse("sdmc:/shows/My.Show.S01E02.1080p.mkv", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "My Show") == 0);
	CHECK(season == 1 && episode == 2 && year == -1);

	CHECK(Lib_name_parse("Show_s02_e03.mp4", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "Show") == 0);
	CHECK(season == 2 && episode == 3);

	CHECK(Lib_name_parse("1x02 - Pilot.avi", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "Pilot") == 0);
	CHECK(season == 1 && episode == 2);

	CHECK(Lib_name_parse("Movie (2019).mp4", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "Movie") == 0);
	CHECK(year == 2019 && season == -1);

	CHECK(Lib_name_parse("2001 A Space Odyssey.mkv", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "2001 A Space Odyssey") == 0);
	CHECK(year == 2001);

	CHECK(Lib_name_parse("1920x1080 trailer.mp4", title, sizeof(title), &season, &episode, &year));
	CHECK(strcmp(title, "1920x1080 trailer") == 0);
	CHECK(season == -1 && episode == -1);

	CHECK(Lib_name_parse("S01E02.mkv", title, sizeof(title), &season, &episode, &year));
	CHECK(season == 1 && episode == 2);

	CHECK(Lib_name_parse("plain video.webm", title, sizeof(title), NULL, NULL, NULL));
	CHECK(strcmp(title, "plain video") == 0);

	CHECK(!Lib_name_parse(NULL, title, sizeof(title), NULL, NULL, NULL));
	CHECK(!Lib_name_parse("a.mkv", NULL, sizeof(title), NULL, NULL, NULL));
	CHECK(!Lib_name_parse("a.mkv", title, 0, NULL, NULL, NULL));
}

static void Test_old_saved_pos_name(void)
{
	char name[128] = { 0, };
	char small[8] = { 0, };

	CHECK(Lib_path_old_saved_pos_name("sdmc:/videos/a.mp4", name, sizeof(name)));
	CHECK(strcmp(name, "sdmc:_videos_a.mp4") == 0);
	CHECK(!Lib_path_old_saved_pos_name("sdmc:/videos/a.mp4", small, sizeof(small)));
}

static void Test_index_roundtrip(void)
{
	Lib_index index = { 0, };
	Lib_index loaded = { 0, };
	Lib_item* item = NULL;
	uint8_t* data = NULL;
	size_t size = 0;
	uint64_t id = Lib_index_hash_id("a.mkv", 1000);

	Lib_index_init(&index);
	item = Lib_index_upsert(&index, "sdmc:/videos/a.mkv", 1000, 111, id);
	CHECK(item != NULL);
	CHECK(item && strcmp(item->title, "a") == 0);

	item->duration_ms = 60000;
	item->width = 400;
	item->height = 240;
	item->fps_x100 = 3000;
	item->video_codec = LIB_VIDEO_CODEC_H264;
	item->audio_tracks = 2;
	item->subtitle_tracks = 1;
	item->flags |= LIB_ITEM_FLAG_3D;
	item->play_count = 4;
	item->last_played = 222;
	item->first_seen = 444;
	Lib_item_update_progress(item, 55000);
	CHECK(Lib_item_is_watched(item));

	item = Lib_index_upsert(&index, "sdmc:/other/b.mkv", 2000, 333, Lib_index_hash_id("b.mkv", 2000));
	CHECK(item != NULL);
	CHECK(Lib_item_set_title(item, "Custom Title"));
	Lib_item_update_progress(item, 1000);
	CHECK(!Lib_item_is_watched(item));

	CHECK(Lib_index_save(&index, &data, &size) == LIB_INDEX_OK);
	CHECK(data != NULL && size == 32 + 2 * 72 + strlen("sdmc:/videos/a.mkv") + 1 + strlen("a") + 1
	+ strlen("sdmc:/other/b.mkv") + 1 + strlen("Custom Title") + 1);

	CHECK(Lib_index_load(&loaded, data, size) == LIB_INDEX_OK);
	CHECK(loaded.count == 2);

	item = Lib_index_find_id(&loaded, id);
	CHECK(item != NULL);
	CHECK(item && item->size == 1000);
	CHECK(item && item->mtime == 111);
	CHECK(item && item->duration_ms == 60000);
	CHECK(item && item->position_ms == 55000);
	CHECK(item && item->width == 400 && item->height == 240);
	CHECK(item && item->fps_x100 == 3000);
	CHECK(item && item->video_codec == LIB_VIDEO_CODEC_H264);
	CHECK(item && item->audio_tracks == 2 && item->subtitle_tracks == 1);
	CHECK(item && (item->flags & LIB_ITEM_FLAG_3D) != 0);
	CHECK(item && (item->flags & LIB_ITEM_FLAG_WATCHED) != 0);
	CHECK(item && item->play_count == 4 && item->last_played == 222);
	CHECK(item && item->first_seen == 444);
	CHECK(item && item->missing_since == 0);
	CHECK(item && strcmp(item->path, "sdmc:/videos/a.mkv") == 0);
	CHECK(item && strcmp(Lib_item_get_title(item), "a") == 0);

	item = Lib_index_find_path(&loaded, "sdmc:/other/b.mkv");
	CHECK(item != NULL);
	CHECK(item && strcmp(Lib_item_get_title(item), "Custom Title") == 0);
	CHECK(item && !Lib_item_is_watched(item));

	free(data);
	Lib_index_free(&loaded);
	Lib_index_free(&index);
}

static void Test_index_corruption(void)
{
	Lib_index index = { 0, };
	Lib_index loaded = { 0, };
	uint8_t* data = NULL;
	size_t size = 0;
	uint8_t* copy = NULL;

	Lib_index_init(&index);
	CHECK(Lib_index_upsert(&index, "sdmc:/a.mkv", 1, 1, 123) != NULL);
	CHECK(Lib_index_save(&index, &data, &size) == LIB_INDEX_OK);

	CHECK(Lib_index_load(&loaded, data, size - 1) == LIB_INDEX_ERR_CORRUPT);

	copy = (uint8_t*)malloc(size);
	CHECK(copy != NULL);
	memcpy(copy, data, size);
	copy[0] = 'X';
	CHECK(Lib_index_load(&loaded, copy, size) == LIB_INDEX_ERR_CORRUPT);

	memcpy(copy, data, size);
	copy[8] = 99;
	CHECK(Lib_index_load(&loaded, copy, size) == LIB_INDEX_ERR_VERSION);

	memcpy(copy, data, size);
	copy[40] ^= 0x01;
	CHECK(Lib_index_load(&loaded, copy, size) == LIB_INDEX_ERR_CORRUPT);

	CHECK(loaded.count == 0);
	free(copy);
	free(data);
	Lib_index_free(&index);
}

static void Test_index_upsert_update(void)
{
	Lib_index index = { 0, };
	Lib_item* item = NULL;
	uint64_t old_id = Lib_index_hash_id("a.mkv", 100);
	uint64_t new_id = Lib_index_hash_id("a.mkv", 200);

	Lib_index_init(&index);
	item = Lib_index_upsert(&index, "sdmc:/a.mkv", 100, 1, old_id);
	CHECK(item != NULL);
	CHECK(item && item->id == old_id);
	CHECK(Lib_item_set_title(item, "Keep me"));

	//Same path, size changed => id changes, record is reused by path.
	item = Lib_index_upsert(&index, "sdmc:/a.mkv", 200, 2, new_id);
	CHECK(item != NULL);
	CHECK(index.count == 1);
	CHECK(item && item->id == new_id);
	CHECK(item && item->size == 200);
	CHECK(item && strcmp(Lib_item_get_title(item), "Keep me") == 0);

	//Same id, moved to another folder => path is updated.
	item = Lib_index_upsert(&index, "sdmc:/b/a.mkv", 200, 2, new_id);
	CHECK(item != NULL);
	CHECK(index.count == 1);
	CHECK(item && strcmp(item->path, "sdmc:/b/a.mkv") == 0);

	CHECK(Lib_index_remove_id(&index, new_id));
	CHECK(index.count == 0);
	CHECK(Lib_index_find_id(&index, new_id) == NULL);
	CHECK(!Lib_index_remove_id(&index, new_id));

	Lib_index_free(&index);
}

static void Test_progress(void)
{
	Lib_item item = { 0, };

	item.duration_ms = 100000;
	Lib_item_update_progress(&item, 89999);
	CHECK(!Lib_item_is_watched(&item));
	Lib_item_update_progress(&item, 90000);
	CHECK(Lib_item_is_watched(&item));
	Lib_item_update_progress(&item, 200000);
	CHECK(item.position_ms == 100000);

	item.duration_ms = 0;
	CHECK(!Lib_item_is_watched(&item));
}

static void Test_queue_modes(void)
{
	Lib_queue queue;
	uint64_t ids[5] = { 10, 11, 12, 13, 14 };
	uint64_t id = 0;

	Lib_queue_init(&queue);
	CHECK(Lib_queue_set(&queue, ids, 5));
	CHECK(Lib_queue_count(&queue) == 5);
	CHECK(Lib_queue_get_current(&queue, &id) && id == 10);

	CHECK(Lib_queue_next(&queue, &id) && id == 11);
	CHECK(Lib_queue_next(&queue, &id) && id == 12);
	CHECK(Lib_queue_next(&queue, &id) && id == 13);
	CHECK(Lib_queue_next(&queue, &id) && id == 14);
	CHECK(!Lib_queue_next(&queue, &id));

	CHECK(Lib_queue_prev(&queue, &id) && id == 13);

	Lib_queue_set_mode(&queue, LIB_QUEUE_MODE_REPEAT);
	CHECK(Lib_queue_get_current(&queue, &id) && id == 13);
	CHECK(Lib_queue_next(&queue, &id) && id == 13);

	Lib_queue_set_mode(&queue, LIB_QUEUE_MODE_IN_ORDER);
	CHECK(Lib_queue_set_cursor(&queue, 14));
	CHECK(Lib_queue_next(&queue, &id) && id == 10);
	CHECK(Lib_queue_prev(&queue, &id) && id == 14);

	Lib_queue_free(&queue);
}

static void Test_queue_random(void)
{
	Lib_queue queue;
	uint64_t ids[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	uint64_t first_pass[8] = { 0, };
	uint64_t id = 0;
	bool seen[9] = { false, };

	Lib_queue_init(&queue);
	Lib_queue_set_seed(&queue, 12345);
	CHECK(Lib_queue_set(&queue, ids, 8));
	Lib_queue_set_mode(&queue, LIB_QUEUE_MODE_RANDOM);

	CHECK(Lib_queue_get_current(&queue, &id));
	first_pass[0] = id;
	CHECK(id >= 1 && id <= 8);

	for (size_t i = 1; i < 8; i++)
	{
		CHECK(Lib_queue_next(&queue, &id));
		first_pass[i] = id;
	}

	for (size_t i = 0; i < 8; i++)
	{
		CHECK(first_pass[i] >= 1 && first_pass[i] <= 8);
		CHECK(!seen[first_pass[i]]);
		seen[first_pass[i]] = true;
	}

	//New deck must not start with the item that just played.
	CHECK(Lib_queue_next(&queue, &id));
	CHECK(id >= 1 && id <= 8);

	//Same seed => same first pass.
	Lib_queue_free(&queue);
	Lib_queue_init(&queue);
	Lib_queue_set_seed(&queue, 12345);
	CHECK(Lib_queue_set(&queue, ids, 8));
	Lib_queue_set_mode(&queue, LIB_QUEUE_MODE_RANDOM);
	CHECK(Lib_queue_get_current(&queue, &id));
	CHECK(id == first_pass[0]);
	for (size_t i = 1; i < 8; i++)
	{
		CHECK(Lib_queue_next(&queue, &id));
		CHECK(id == first_pass[i]);
	}

	Lib_queue_free(&queue);
}

static void Test_queue_edit(void)
{
	Lib_queue queue;
	uint64_t ids[3] = { 1, 2, 3 };
	uint64_t id = 0;

	Lib_queue_init(&queue);
	CHECK(Lib_queue_set(&queue, ids, 3));
	CHECK(Lib_queue_contains(&queue, 2));
	CHECK(!Lib_queue_contains(&queue, 9));
	CHECK(Lib_queue_add(&queue, 4));
	CHECK(Lib_queue_count(&queue) == 4);
	CHECK(Lib_queue_add(&queue, 4));
	CHECK(Lib_queue_count(&queue) == 4);
	CHECK(Lib_queue_remove(&queue, 2));
	CHECK(!Lib_queue_contains(&queue, 2));
	CHECK(Lib_queue_count(&queue) == 3);
	CHECK(!Lib_queue_remove(&queue, 2));
	CHECK(Lib_queue_get_at(&queue, 0, &id) && id == 1);
	CHECK(!Lib_queue_get_at(&queue, 3, &id));
	CHECK(!Lib_queue_set_cursor(&queue, 2));
	CHECK(Lib_queue_set_cursor(&queue, 4));
	CHECK(Lib_queue_get_current(&queue, &id) && id == 4);
	Lib_queue_free(&queue);
}

static void Test_scan(void)
{
	Lib_index index = { 0, };
	Lib_scan_files files = { 0, };
	Lib_scan_stats stats = { 0, };
	Lib_item* item = NULL;

	CHECK(Lib_scan_classify("a.MKV") == LIB_SCAN_KIND_VIDEO);
	CHECK(Lib_scan_classify("a.mp3") == LIB_SCAN_KIND_AUDIO);
	CHECK(Lib_scan_classify("a.jpg") == LIB_SCAN_KIND_OTHER);
	CHECK(Lib_scan_classify("no_extension") == LIB_SCAN_KIND_OTHER);
	CHECK(Lib_scan_classify(NULL) == LIB_SCAN_KIND_OTHER);
	CHECK(!Lib_scan_is_supported("a.jpg"));
	CHECK(Lib_scan_is_supported("a.webm"));

	Lib_index_init(&index);
	Lib_scan_files_init(&files);
	CHECK(Lib_scan_files_add(&files, "/videos/a.mkv", 1000, 0, LIB_SCAN_KIND_VIDEO));
	CHECK(Lib_scan_files_add(&files, "/videos/b.mp3", 2000, 0, LIB_SCAN_KIND_AUDIO));
	CHECK(Lib_scan_apply(&index, &files, 1000, 0, &stats));
	CHECK(stats.added == 2 && stats.updated == 0 && stats.unchanged == 0);
	CHECK(index.count == 2);

	item = Lib_index_find_path(&index, "/videos/a.mkv");
	CHECK(item != NULL);
	CHECK(item && item->first_seen == 1000);
	CHECK(item && (item->flags & LIB_ITEM_FLAG_AUDIO_ONLY) == 0);
	CHECK(item && strcmp(Lib_item_get_title(item), "a") == 0);
	if (item)
	{
		item->duration_ms = 60000;
		Lib_item_update_progress(item, 30000);
		item->last_played = 1001;
	}
	item = Lib_index_find_path(&index, "/videos/b.mp3");
	CHECK(item != NULL);
	CHECK(item && (item->flags & LIB_ITEM_FLAG_AUDIO_ONLY) != 0);

	//Second identical scan.
	Lib_scan_files_free(&files);
	Lib_scan_files_init(&files);
	CHECK(Lib_scan_files_add(&files, "/videos/a.mkv", 1000, 0, LIB_SCAN_KIND_VIDEO));
	CHECK(Lib_scan_files_add(&files, "/videos/b.mp3", 2000, 0, LIB_SCAN_KIND_AUDIO));
	CHECK(Lib_scan_apply(&index, &files, 1100, 0, &stats));
	CHECK(stats.added == 0 && stats.unchanged == 2 && stats.updated == 0);

	//a.mkv disappears: mark missing, keep history.
	Lib_scan_files_free(&files);
	Lib_scan_files_init(&files);
	CHECK(Lib_scan_files_add(&files, "/videos/b.mp3", 2000, 0, LIB_SCAN_KIND_AUDIO));
	CHECK(Lib_scan_apply(&index, &files, 2000, 0, &stats));
	CHECK(stats.marked_missing == 1);
	item = Lib_index_find_path(&index, "/videos/a.mkv");
	CHECK(item && (item->flags & LIB_ITEM_FLAG_MISSING) != 0);
	CHECK(item && item->missing_since == 2000);
	CHECK(item && item->position_ms == 30000);

	//Rename: same directory and size keeps history.
	Lib_scan_files_free(&files);
	Lib_scan_files_init(&files);
	CHECK(Lib_scan_files_add(&files, "/videos/a2.mkv", 1000, 0, LIB_SCAN_KIND_VIDEO));
	CHECK(Lib_scan_files_add(&files, "/videos/b.mp3", 2000, 0, LIB_SCAN_KIND_AUDIO));
	CHECK(Lib_scan_apply(&index, &files, 2100, 0, &stats));
	CHECK(stats.renamed == 1 && stats.added == 0);
	CHECK(index.count == 2);
	CHECK(Lib_index_find_path(&index, "/videos/a.mkv") == NULL);
	item = Lib_index_find_path(&index, "/videos/a2.mkv");
	CHECK(item != NULL);
	CHECK(item && item->id == Lib_index_hash_id("a2.mkv", 1000));
	CHECK(item && item->position_ms == 30000);
	CHECK(item && (item->flags & LIB_ITEM_FLAG_MISSING) == 0);

	//Missing beyond the grace period is removed.
	Lib_scan_files_free(&files);
	Lib_scan_files_init(&files);
	CHECK(Lib_scan_files_add(&files, "/videos/b.mp3", 2000, 0, LIB_SCAN_KIND_AUDIO));
	CHECK(Lib_scan_apply(&index, &files, 3000, 0, &stats));
	CHECK(stats.marked_missing == 1);
	CHECK(Lib_scan_apply(&index, &files, 3011, 10, &stats));
	CHECK(stats.removed == 1);
	CHECK(index.count == 1);
	CHECK(Lib_index_find_path(&index, "/videos/a2.mkv") == NULL);

	Lib_scan_files_free(&files);
	Lib_index_free(&index);
}

static void Test_sort(void)
{
	Lib_index index = { 0, };
	Lib_sort_spec spec = { LIB_SORT_KEY_NAME, false };

	CHECK(Lib_sort_natural_compare("S01E2", "S01E10") < 0);
	CHECK(Lib_sort_natural_compare("s01e2", "S01E2") == 0);
	CHECK(Lib_sort_natural_compare("Episode 1", "episode 2") < 0);
	CHECK(Lib_sort_natural_compare("a", "a") == 0);
	CHECK(Lib_sort_natural_compare("a10", "a9") > 0);
	CHECK(Lib_sort_natural_compare("a007", "a7") == 0);

	Lib_index_init(&index);
	CHECK(Lib_index_upsert(&index, "/x/S01E1.mkv", 100, 0, 1) != NULL);
	CHECK(Lib_index_upsert(&index, "/x/S01E2.mkv", 200, 0, 2) != NULL);
	CHECK(Lib_index_upsert(&index, "/x/S01E10.mkv", 300, 0, 3) != NULL);
	CHECK(Lib_item_set_title(&index.items[0], "Episode 1"));
	CHECK(Lib_item_set_title(&index.items[1], "Episode 2"));
	CHECK(Lib_item_set_title(&index.items[2], "Episode 10"));

	CHECK(Lib_sort_items(index.items, index.count, &spec));
	CHECK(strcmp(Lib_item_get_title(&index.items[0]), "Episode 1") == 0);
	CHECK(strcmp(Lib_item_get_title(&index.items[1]), "Episode 2") == 0);
	CHECK(strcmp(Lib_item_get_title(&index.items[2]), "Episode 10") == 0);

	spec.descending = true;
	CHECK(Lib_sort_items(index.items, index.count, &spec));
	CHECK(strcmp(Lib_item_get_title(&index.items[0]), "Episode 10") == 0);
	CHECK(strcmp(Lib_item_get_title(&index.items[2]), "Episode 1") == 0);

	Lib_index_free(&index);
}

static void Test_view(void)
{
	Lib_index index = { 0, };
	Lib_item* a = NULL;
	Lib_item* b = NULL;
	Lib_item* c = NULL;
	Lib_item* d = NULL;
	Lib_item* e = NULL;
	uint64_t ids[8] = { 0, };
	size_t count = 0;

	Lib_index_init(&index);
	CHECK(Lib_index_upsert(&index, "/v/a.mkv", 1, 0, 1) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/b.mkv", 2, 0, 2) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/c.mkv", 3, 0, 3) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/d.mkv", 4, 0, 4) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/e.mkv", 5, 0, 5) != NULL);
	if (index.count != 5)
	{
		Lib_index_free(&index);
		return;
	}
	a = &index.items[0];
	b = &index.items[1];
	c = &index.items[2];
	d = &index.items[3];
	e = &index.items[4];
	CHECK(a && b && c && d && e);

	a->duration_ms = 100000;
	Lib_item_update_progress(a, 5000);
	a->last_played = 100;
	b->duration_ms = 100000;
	Lib_item_update_progress(b, 95000);
	b->last_played = 200;
	c->last_played = 300;
	d->flags |= LIB_ITEM_FLAG_FAVORITE;
	e->duration_ms = 100000;
	Lib_item_update_progress(e, 1000);
	e->last_played = 400;
	e->flags |= LIB_ITEM_FLAG_MISSING;

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_CONTINUE, NULL, ids, 8, &count));
	CHECK(count == 1 && ids[0] == a->id);

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_RECENT, NULL, ids, 8, &count));
	CHECK(count == 3);
	CHECK(count == 3 && ids[0] == c->id && ids[1] == b->id && ids[2] == a->id);

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_FAVORITES, NULL, ids, 8, &count));
	CHECK(count == 1 && ids[0] == d->id);

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_ALL, NULL, ids, 8, &count));
	CHECK(count == 5);

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_CONTINUE, NULL, ids, 1, &count));
	CHECK(count == 1);

	CHECK(Lib_view_build(&index, LIB_VIEW_TAB_CONTINUE, NULL, NULL, 0, &count));
	CHECK(count == 1);

	Lib_index_free(&index);
}

typedef struct
{
	const char* name;
	uint64_t position_ms;
} Test_saved_pos;

static bool Test_migrate_read(const char* old_name, uint64_t* position_ms, void* userdata)
{
	const Test_saved_pos* table = (const Test_saved_pos*)userdata;

	if (!old_name || !position_ms || !table)
		return false;

	for (size_t i = 0; table[i].name; i++)
	{
		if (strcmp(table[i].name, old_name) == 0)
		{
			*position_ms = table[i].position_ms;
			return true;
		}
	}

	return false;
}

static void Test_migrate(void)
{
	static Test_saved_pos positions[] =
	{
		{ "_videos_a.mkv", 12345 },
		{ "_videos_b.mkv", 777 },
		{ "_videos_c.mkv", 5000000000ULL },
		{ NULL, 0 },
	};
	Lib_index index = { 0, };
	Lib_migrate_stats stats = { 0, };
	Lib_item* a = NULL;
	Lib_item* b = NULL;
	Lib_item* c = NULL;

	Lib_index_init(&index);
	CHECK(Lib_index_upsert(&index, "/videos/a.mkv", 10, 0, 1) != NULL);
	CHECK(Lib_index_upsert(&index, "/videos/b.mkv", 20, 0, 2) != NULL);
	CHECK(Lib_index_upsert(&index, "/videos/c.mkv", 30, 0, 3) != NULL);
	if (index.count != 3)
	{
		Lib_index_free(&index);
		return;
	}

	a = &index.items[0];
	b = &index.items[1];
	c = &index.items[2];
	b->position_ms = 5000;
	b->duration_ms = 60000;

	CHECK(Lib_migrate_saved_pos(&index, Test_migrate_read, (void*)positions, &stats));
	CHECK(stats.items_seen == 3);
	CHECK(stats.migrated == 2);
	CHECK(stats.already_known == 1);
	CHECK(stats.not_found == 0);
	CHECK(a->position_ms == 12345);
	CHECK(b->position_ms == 5000);
	CHECK(c->position_ms == UINT32_MAX);
	CHECK(b->duration_ms == 60000);
	CHECK(a->last_played == 0);

	CHECK(!Lib_migrate_saved_pos(NULL, Test_migrate_read, (void*)positions, &stats));
	CHECK(!Lib_migrate_saved_pos(&index, NULL, NULL, &stats));

	Lib_index_free(&index);
}

static void Test_thumb(void)
{
	char name[64] = { 0, };

	CHECK(Lib_thumb_directory_length("/videos/a.mkv") == 7);
	CHECK(Lib_thumb_directory_length("/a.mkv") == 1);
	CHECK(Lib_thumb_directory_length("a.mkv") == 0);
	CHECK(Lib_thumb_directory_length(NULL) == 0);

	CHECK(Lib_thumb_sidecar_path("/videos/a.mkv", name, sizeof(name)));
	CHECK(strcmp(name, "/videos/a.mkv.jpg") == 0);
	CHECK(Lib_thumb_sidecar_path("a.mkv", name, sizeof(name)));
	CHECK(strcmp(name, "a.mkv.jpg") == 0);
	CHECK(!Lib_thumb_sidecar_path("/videos/a.mkv", name, 12));
	CHECK(name[0] == 0x00);

	CHECK(Lib_thumb_fallback_path("/videos/a.mkv", "poster.jpg", name, sizeof(name)));
	CHECK(strcmp(name, "/videos/poster.jpg") == 0);
	CHECK(Lib_thumb_fallback_path("/a.mkv", "poster.jpg", name, sizeof(name)));
	CHECK(strcmp(name, "/poster.jpg") == 0);
	CHECK(Lib_thumb_fallback_path("a.mkv", "folder.jpg", name, sizeof(name)));
	CHECK(strcmp(name, "folder.jpg") == 0);
	CHECK(!Lib_thumb_fallback_path("/videos/a.mkv", "poster.jpg", name, 18));
	CHECK(name[0] == 0x00);

	CHECK(Lib_thumb_is_image("poster.jpg"));
	CHECK(Lib_thumb_is_image("POSTER.JPG"));
	CHECK(Lib_thumb_is_image("folder.jpeg"));
	CHECK(!Lib_thumb_is_image("poster.jpg.txt"));
	CHECK(!Lib_thumb_is_image("poster.png"));
	CHECK(!Lib_thumb_is_image(NULL));
}

static void Test_ui(void)
{
	Lib_index index = { 0, };
	Lib_ui ui = { 0, };
	const Lib_item* item = NULL;
	uint64_t id = 0;

	Lib_index_init(&index);
	Lib_ui_init(&ui);
	CHECK(Lib_ui_set_rows(&ui, 0) == false);
	CHECK(Lib_ui_set_rows(&ui, LIB_UI_MAX_ROWS + 1) == false);
	CHECK(Lib_ui_set_rows(&ui, 3));

	CHECK(Lib_index_upsert(&index, "/v/E1.mkv", 1, 0, 1) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/E2.mkv", 2, 0, 2) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/E3.mkv", 3, 0, 3) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/E4.mkv", 4, 0, 4) != NULL);
	CHECK(Lib_index_upsert(&index, "/v/E5.mkv", 5, 0, 5) != NULL);
	if (index.count != 5)
	{
		Lib_ui_free(&ui);
		Lib_index_free(&index);
		return;
	}

	CHECK(Lib_ui_rebuild(&ui, &index));
	CHECK(ui.count == 5 && ui.cursor == 0 && ui.offset == 0);
	item = Lib_ui_current(&ui, &index);
	CHECK(item != NULL);
	CHECK(item && strcmp(Lib_item_get_title(item), "E1") == 0);

	Lib_ui_move(&ui, 10);
	CHECK(ui.cursor == 4 && ui.offset == 2);
	Lib_ui_move(&ui, -10);
	CHECK(ui.cursor == 0 && ui.offset == 0);

	Lib_ui_page(&ui, true);
	CHECK(ui.cursor == 2 && ui.offset == 0);
	Lib_ui_page(&ui, true);
	CHECK(ui.cursor == 4);
	Lib_ui_page(&ui, false);
	CHECK(ui.cursor == 2);

	CHECK(Lib_ui_select_visible(&ui, 2));
	CHECK(ui.cursor == 2);
	CHECK(!Lib_ui_select_visible(&ui, 3));
	CHECK(Lib_ui_id_at_visible(&ui, 0, &id));
	CHECK(id == ui.ids[ui.offset]);
	CHECK(Lib_ui_id_at_visible(&ui, 2, &id));
	CHECK(id == ui.ids[2]);
	CHECK(!Lib_ui_id_at_visible(&ui, 3, &id));

	//Rebuild keeps the selected id.
	id = ui.ids[ui.cursor];
	CHECK(Lib_ui_rebuild(&ui, &index));
	CHECK(ui.cursor < ui.count && ui.ids[ui.cursor] == id);

	//Tab change resets the cursor and filters.
	index.items[0].position_ms = 1000;
	CHECK(Lib_ui_set_tab(&ui, LIB_VIEW_TAB_CONTINUE, &index));
	CHECK(ui.count == 1 && ui.cursor == 0 && ui.offset == 0);
	CHECK(Lib_ui_set_tab(&ui, LIB_VIEW_TAB_ALL, &index));
	CHECK(ui.count == 5);

	//Title search filters in memory, ignores ASCII case, and can be cleared.
	CHECK(Lib_item_set_title(Lib_index_find_id(&index, 5), "Ética"));
	CHECK(Lib_ui_set_search(&ui, "ética", &index));
	CHECK(ui.count == 1 && strcmp(Lib_item_get_title(Lib_ui_current(&ui, &index)), "Ética") == 0);
	CHECK(Lib_ui_set_search(&ui, "e2", &index));
	CHECK(ui.count == 1 && strcmp(Lib_item_get_title(Lib_ui_current(&ui, &index)), "E2") == 0);
	CHECK(Lib_ui_set_search(&ui, "", &index));
	CHECK(ui.count == 5);

	//Grid movement uses two columns and clamps on the partial last row.
	CHECK(Lib_ui_set_rows(&ui, LIB_UI_MAX_ROWS));
	Lib_ui_move(&ui, -10);
	Lib_ui_set_grid_mode(&ui, true);
	Lib_ui_move_direction(&ui, 1, 0);
	CHECK(ui.cursor == 1);
	Lib_ui_move_direction(&ui, 0, 1);
	CHECK(ui.cursor == 3);
	Lib_ui_move_direction(&ui, 0, 1);
	CHECK(ui.cursor == 4);
	Lib_ui_move_direction(&ui, -1, 0);
	CHECK(ui.cursor == 4);
	Lib_ui_move_direction(&ui, 0, -1);
	CHECK(ui.cursor == 2);
	Lib_ui_set_grid_mode(&ui, false);

	//Selecting the last item and shrinking the list clamps.
	Lib_ui_move(&ui, 10);
	CHECK(Lib_index_remove_id(&index, index.items[4].id));
	CHECK(Lib_ui_rebuild(&ui, &index));
	CHECK(ui.count == 4 && ui.cursor == 3);
	CHECK(ui.cursor >= ui.offset && ui.cursor < (ui.offset + ui.rows));

	Lib_ui_free(&ui);
	Lib_index_free(&index);
}

static void Test_probe(void)
{
	Lib_index index = { 0, };
	Lib_item* item = NULL;
	Lib_probe_info info = { 0, };
	Lib_probe_info empty = { 0, };

	Lib_index_init(&index);
	item = Lib_index_upsert(&index, "/v/movie.mkv", 10, 0, 1);
	CHECK(item != NULL);
	if (!item)
	{
		Lib_index_free(&index);
		return;
	}

	item->duration_ms = 111;
	item->width = 222;
	item->flags |= LIB_ITEM_FLAG_FAVORITE;
	Lib_probe_apply(item, &empty, LIB_PROBE_WATCHED_GUARD_MS);
	CHECK(item->duration_ms == 111);
	CHECK(item->width == 222);
	CHECK(item->flags == LIB_ITEM_FLAG_FAVORITE);
	Lib_probe_apply(NULL, &info, LIB_PROBE_WATCHED_GUARD_MS);
	Lib_probe_apply(item, NULL, LIB_PROBE_WATCHED_GUARD_MS);

	info.duration_ms = 120000;
	info.width = 400;
	info.height = 240;
	info.fps_x100 = 2997;
	info.video_codec = LIB_VIDEO_CODEC_MPEG4;
	info.audio_tracks = 2;
	info.subtitle_tracks = 1;
	info.has_video = true;
	info.has_audio = true;
	info.is_3d = true;
	Lib_probe_apply(item, &info, LIB_PROBE_WATCHED_GUARD_MS);
	CHECK(item->duration_ms == 120000);
	CHECK(item->width == 400 && item->height == 240 && item->fps_x100 == 2997);
	CHECK(item->video_codec == LIB_VIDEO_CODEC_MPEG4);
	CHECK(item->audio_tracks == 2 && item->subtitle_tracks == 1);
	CHECK((item->flags & LIB_ITEM_FLAG_3D) != 0);
	CHECK((item->flags & LIB_ITEM_FLAG_AUDIO_ONLY) == 0);
	CHECK((item->flags & LIB_ITEM_FLAG_FAVORITE) != 0);

	info.is_3d = false;
	info.has_video = false;
	info.has_audio = true;
	Lib_probe_apply(item, &info, LIB_PROBE_WATCHED_GUARD_MS);
	CHECK((item->flags & LIB_ITEM_FLAG_3D) == 0);
	CHECK((item->flags & LIB_ITEM_FLAG_AUDIO_ONLY) != 0);

	item->position_ms = 116000;
	Lib_probe_apply(item, &info, LIB_PROBE_WATCHED_GUARD_MS);
	CHECK((item->flags & LIB_ITEM_FLAG_WATCHED) != 0);

	item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_WATCHED;
	item->position_ms = 10000;
	Lib_probe_apply(item, &info, LIB_PROBE_WATCHED_GUARD_MS);
	CHECK((item->flags & LIB_ITEM_FLAG_WATCHED) == 0);

	Lib_index_free(&index);
}

int main(void)
{
	Test_hash();
	Test_name();
	Test_old_saved_pos_name();
	Test_index_roundtrip();
	Test_index_corruption();
	Test_index_upsert_update();
	Test_progress();
	Test_queue_modes();
	Test_queue_random();
	Test_queue_edit();
	Test_scan();
	Test_sort();
	Test_view();
	Test_migrate();
	Test_thumb();
	Test_ui();
	Test_probe();

	if (tests_failed != 0)
	{
		fprintf(stderr, "%d/%d checks failed\n", tests_failed, tests_run);
		return 1;
	}

	printf("All %d checks passed\n", tests_run);
	return 0;
}
