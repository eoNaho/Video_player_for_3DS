#include "library/lib_thumb.h"

#include <string.h>

static char Lib_thumb_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (char)(c - 'A' + 'a');
	return c;
}

static bool Lib_thumb_ends_with(const char* text, const char* suffix)
{
	size_t text_length = 0;
	size_t suffix_length = 0;

	if (!text || !suffix)
		return false;

	text_length = strlen(text);
	suffix_length = strlen(suffix);
	if (suffix_length > text_length)
		return false;

	text += text_length - suffix_length;
	for (size_t i = 0; i < suffix_length; i++)
	{
		if (Lib_thumb_lower(text[i]) != Lib_thumb_lower(suffix[i]))
			return false;
	}

	return true;
}

size_t Lib_thumb_directory_length(const char* path)
{
	const char* slash = NULL;

	if (!path)
		return 0;

	slash = strrchr(path, '/');
	if (!slash)
		return 0;

	//Root ("/a.mkv") => directory is "/", length 1.
	if (slash == path)
		return 1;

	return (size_t)(slash - path);
}

bool Lib_thumb_sidecar_path(const char* media_path, char* out, size_t out_size)
{
	size_t length = 0;

	if (!media_path || !out || out_size == 0)
		return false;

	length = strlen(media_path);
	if (length + 4 >= out_size)
	{
		out[0] = 0x00;
		return false;
	}

	memcpy(out, media_path, length);
	memcpy(out + length, ".jpg", 5);
	return true;
}

bool Lib_thumb_fallback_path(const char* media_path, const char* file_name,
char* out, size_t out_size)
{
	size_t dir_length = 0;
	size_t file_length = 0;
	size_t offset = 0;
	bool needs_slash = false;

	if (!media_path || !file_name || !out || out_size == 0 || file_name[0] == 0x00)
		return false;

	dir_length = Lib_thumb_directory_length(media_path);
	file_length = strlen(file_name);
	needs_slash = (bool)(dir_length > 0 && media_path[dir_length - 1] != '/');
	if (dir_length + (needs_slash ? 1 : 0) + file_length + 1 > out_size)
	{
		out[0] = 0x00;
		return false;
	}

	memcpy(out, media_path, dir_length);
	offset = dir_length;
	if (needs_slash)
		out[offset++] = '/';
	memcpy(out + offset, file_name, file_length + 1);
	return true;
}

bool Lib_thumb_is_image(const char* name)
{
	if (!name)
		return false;

	return (bool)(Lib_thumb_ends_with(name, ".jpg") || Lib_thumb_ends_with(name, ".jpeg"));
}
