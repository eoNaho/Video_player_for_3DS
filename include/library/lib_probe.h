#if !defined(DEF_LIB_PROBE_H)
#define DEF_LIB_PROBE_H
#include <stdbool.h>
#include <stdint.h>
#include "library/lib_index.h"

#define LIB_PROBE_WATCHED_GUARD_MS		(uint32_t)(5000)

//Metadata discovered by the decoder/probe thread.
//Zero/false values mean "unknown" and never erase data already in the index.
typedef struct
{
	uint32_t duration_ms;
	uint16_t width;
	uint16_t height;
	uint16_t fps_x100;
	Lib_video_codec video_codec;
	uint8_t audio_tracks;
	uint8_t subtitle_tracks;
	bool has_video;
	bool has_audio;
	bool is_3d;
} Lib_probe_info;

//Copies the probe result into item.
//- Unknown fields are left untouched.
//- AUDIO_ONLY/3D are refreshed from has_video/is_3d.
//- WATCHED is set when the saved position is within watched_guard_ms of the end.
void Lib_probe_apply(Lib_item* item, const Lib_probe_info* info, uint32_t watched_guard_ms);

#endif //!defined(DEF_LIB_PROBE_H)
