#include "library/lib_index.h"
#include "library/lib_name.h"
#include "library/lib_queue.h"

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
	Lib_item_update_progress(item, 55000);
	CHECK(Lib_item_is_watched(item));

	item = Lib_index_upsert(&index, "sdmc:/other/b.mkv", 2000, 333, Lib_index_hash_id("b.mkv", 2000));
	CHECK(item != NULL);
	CHECK(Lib_item_set_title(item, "Custom Title"));
	Lib_item_update_progress(item, 1000);
	CHECK(!Lib_item_is_watched(item));

	CHECK(Lib_index_save(&index, &data, &size) == LIB_INDEX_OK);
	CHECK(data != NULL && size == 32 + 2 * 64 + strlen("sdmc:/videos/a.mkv") + 1 + strlen("a") + 1
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

	if (tests_failed != 0)
	{
		fprintf(stderr, "%d/%d checks failed\n", tests_failed, tests_run);
		return 1;
	}

	printf("All %d checks passed\n", tests_run);
	return 0;
}
