#include "library/lib_probe.h"

void Lib_probe_apply(Lib_item* item, const Lib_probe_info* info, uint32_t watched_guard_ms)
{
	if (!item || !info)
		return;

	if (info->duration_ms > 0)
		item->duration_ms = info->duration_ms;
	if (info->width > 0)
		item->width = info->width;
	if (info->height > 0)
		item->height = info->height;
	if (info->fps_x100 > 0)
		item->fps_x100 = info->fps_x100;
	if (info->video_codec != LIB_VIDEO_CODEC_UNKNOWN)
		item->video_codec = info->video_codec;
	if (info->audio_tracks > 0)
		item->audio_tracks = info->audio_tracks;
	if (info->subtitle_tracks > 0)
		item->subtitle_tracks = info->subtitle_tracks;

	if (!info->has_video && info->has_audio)
		item->flags |= LIB_ITEM_FLAG_AUDIO_ONLY;
	else if (info->has_video)
		item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_AUDIO_ONLY;

	if (info->is_3d)
		item->flags |= LIB_ITEM_FLAG_3D;
	else
		item->flags &= (Lib_item_flags)~LIB_ITEM_FLAG_3D;

	if (item->duration_ms > 0 && item->position_ms > 0
	&& (uint64_t)item->position_ms + watched_guard_ms >= item->duration_ms)
		item->flags |= LIB_ITEM_FLAG_WATCHED;
}
