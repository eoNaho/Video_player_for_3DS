#if !defined(DEF_LIB_INDEX_H)
#define DEF_LIB_INDEX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//On-disk format version. Bump only with a compatible loader path.
#define LIB_INDEX_VERSION					(uint16_t)(1)
#define LIB_INDEX_HEADER_SIZE				(uint32_t)(32)
#define LIB_INDEX_RECORD_SIZE				(uint32_t)(64)
#define LIB_INDEX_MAX_ITEMS					(uint32_t)(1000000)

typedef uint8_t Lib_item_flags;
#define LIB_ITEM_FLAG_3D					(Lib_item_flags)(1 << 0)
#define LIB_ITEM_FLAG_FAVORITE				(Lib_item_flags)(1 << 1)
#define LIB_ITEM_FLAG_WATCHED				(Lib_item_flags)(1 << 2)
#define LIB_ITEM_FLAG_MISSING				(Lib_item_flags)(1 << 3)
#define LIB_ITEM_FLAG_UNSUPPORTED			(Lib_item_flags)(1 << 4)
#define LIB_ITEM_FLAG_HAS_THUMB				(Lib_item_flags)(1 << 5)
#define LIB_ITEM_FLAG_AUDIO_ONLY			(Lib_item_flags)(1 << 6)

typedef uint8_t Lib_video_codec;
#define LIB_VIDEO_CODEC_UNKNOWN				(Lib_video_codec)(0)
#define LIB_VIDEO_CODEC_H264				(Lib_video_codec)(1)
#define LIB_VIDEO_CODEC_MPEG4				(Lib_video_codec)(2)
#define LIB_VIDEO_CODEC_MPEG2				(Lib_video_codec)(3)
#define LIB_VIDEO_CODEC_MPEG1				(Lib_video_codec)(4)
#define LIB_VIDEO_CODEC_VP8					(Lib_video_codec)(5)
#define LIB_VIDEO_CODEC_VP9					(Lib_video_codec)(6)
#define LIB_VIDEO_CODEC_AV1					(Lib_video_codec)(7)
#define LIB_VIDEO_CODEC_H266				(Lib_video_codec)(8)

//In-memory item. Strings are owned by the index.
typedef struct
{
	uint64_t id;						//FNV-1a of basename + size.
	uint64_t size;						//File size in bytes.
	uint64_t mtime;						//Last modification time (Unix seconds).
	char* path;							//Full path ("sdmc:/...").
	char* title;						//Display title (never NULL after upsert).
	uint32_t duration_ms;
	uint32_t position_ms;
	uint32_t last_played;				//Unix seconds, 0 = never.
	uint32_t play_count;
	uint16_t width;
	uint16_t height;
	uint16_t fps_x100;
	Lib_video_codec video_codec;
	uint8_t audio_tracks;
	uint8_t subtitle_tracks;
	Lib_item_flags flags;
} Lib_item;

typedef struct
{
	Lib_item* items;
	size_t count;
	size_t capacity;
} Lib_index;

typedef enum
{
	LIB_INDEX_OK = 0,
	LIB_INDEX_ERR_INVALID,				//Bad arguments.
	LIB_INDEX_ERR_NOMEM,				//Allocation failure.
	LIB_INDEX_ERR_CORRUPT,				//Bad magic/size/checksum/string.
	LIB_INDEX_ERR_VERSION,				//Unsupported format version.
} Lib_index_result;

//FNV-1a 64-bit over the basename and the little-endian file size.
uint64_t Lib_index_hash_id(const char* name, uint64_t size);

void Lib_index_init(Lib_index* index);

void Lib_index_free(Lib_index* index);

//Find helpers. Return NULL when not found.
Lib_item* Lib_index_find_id(Lib_index* index, uint64_t id);

const Lib_item* Lib_index_find_id_const(const Lib_index* index, uint64_t id);

Lib_item* Lib_index_find_path(Lib_index* index, const char* path);

//Insert or update by id (then by path). Returns the item or NULL on error.
Lib_item* Lib_index_upsert(Lib_index* index, const char* path, uint64_t size, uint64_t mtime, uint64_t id);

bool Lib_index_remove_at(Lib_index* index, size_t position);

bool Lib_index_remove_id(Lib_index* index, uint64_t id);

//Serialization to a malloc'd buffer. Caller owns *data.
Lib_index_result Lib_index_load(Lib_index* index, const uint8_t* data, size_t size);

Lib_index_result Lib_index_save(const Lib_index* index, uint8_t** data, size_t* size);

//Item helpers.
const char* Lib_item_get_title(const Lib_item* item);

bool Lib_item_set_title(Lib_item* item, const char* title);

bool Lib_item_is_watched(const Lib_item* item);

//Marks watched when position is at least 90% of duration.
void Lib_item_update_progress(Lib_item* item, uint32_t position_ms);

void Lib_item_clear(Lib_item* item);

#endif //!defined(DEF_LIB_INDEX_H)
