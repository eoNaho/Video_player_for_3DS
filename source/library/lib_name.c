#include "library/lib_name.h"

#include <ctype.h>
#include <string.h>

#define LIB_NAME_BUFFER_SIZE		(size_t)(256)
#define LIB_NAME_MAX_EPISODE		(int)(9999)

static bool Lib_name_is_digit(char c)
{
	return (bool)(c >= '0' && c <= '9');
}

static bool Lib_name_is_alpha_num(char c)
{
	return (bool)(Lib_name_is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static int Lib_name_parse_number(const char* text, size_t size, size_t* position, int max_digits)
{
	int value = 0;
	int digits = 0;

	while (*position < size && digits < max_digits && Lib_name_is_digit(text[*position]))
	{
		value = (value * 10) + (text[*position] - '0');
		(*position)++;
		digits++;
	}

	return (digits > 0) ? value : -1;
}

static void Lib_name_trim(const char* text, size_t* start, size_t* end)
{
	while (*start < *end && text[*start] == ' ')
		(*start)++;
	while (*end > *start && text[*end - 1] == ' ')
		(*end)--;
}

static void Lib_name_copy(const char* text, size_t start, size_t end, char* out, size_t out_size)
{
	size_t length = 0;

	if (end > start)
		length = end - start;
	if (length >= out_size)
		length = out_size - 1;

	if (length > 0)
		memcpy(out, &text[start], length);
	out[length] = 0x00;
}

static bool Lib_name_find_episode(const char* text, size_t size, size_t* marker_start, size_t* marker_end,
int* season, int* episode)
{
	for (size_t i = 0; i + 3 < size; i++)
	{
		size_t position = 0;
		size_t after = 0;
		int parsed_season = -1;
		int parsed_episode = -1;

		if (i > 0 && Lib_name_is_alpha_num(text[i - 1]))
			continue;

		//S01E02 / S1E2 / S01 E02.
		if ((text[i] == 's' || text[i] == 'S') && Lib_name_is_digit(text[i + 1]))
		{
			position = i + 1;
			parsed_season = Lib_name_parse_number(text, size, &position, 3);
			while (position < size && text[position] == ' ')
				position++;

			if (parsed_season > 0 && position < size && (text[position] == 'e' || text[position] == 'E')
			&& position + 1 < size && Lib_name_is_digit(text[position + 1]))
			{
				after = position + 1;
				parsed_episode = Lib_name_parse_number(text, size, &after, 4);
			}
		}
		//1x02.
		else if (Lib_name_is_digit(text[i]))
		{
			position = i;
			parsed_season = Lib_name_parse_number(text, size, &position, 3);
			if (parsed_season > 0 && position < size && (text[position] == 'x' || text[position] == 'X')
			&& position + 1 < size && Lib_name_is_digit(text[position + 1]))
			{
				after = position + 1;
				parsed_episode = Lib_name_parse_number(text, size, &after, 4);
			}
		}

		if (parsed_season > 0 && parsed_season <= 99 && parsed_episode > 0 && parsed_episode <= LIB_NAME_MAX_EPISODE)
		{
			*marker_start = i;
			*marker_end = after;
			*season = parsed_season;
			*episode = parsed_episode;
			return true;
		}
	}

	return false;
}

static bool Lib_name_find_year(const char* text, size_t size, size_t* year_start, size_t* year_end, int* year)
{
	for (size_t i = 0; i + 4 <= size; i++)
	{
		size_t position = i;
		int value = 0;

		if (i > 0 && Lib_name_is_digit(text[i - 1]))
			continue;
		if (i + 4 < size && Lib_name_is_digit(text[i + 4]))
			continue;
		if (!Lib_name_is_digit(text[i]) || !Lib_name_is_digit(text[i + 1])
		|| !Lib_name_is_digit(text[i + 2]) || !Lib_name_is_digit(text[i + 3]))
			continue;

		value = Lib_name_parse_number(text, size, &position, 4);
		if (value >= 1900 && value <= 2099)
		{
			*year_start = i;
			*year_end = i + 4;
			*year = value;
			return true;
		}
	}

	return false;
}

bool Lib_name_parse(const char* path, char* title_out, size_t title_size,
int* season_out, int* episode_out, int* year_out)
{
	char raw[LIB_NAME_BUFFER_SIZE] = { 0, };
	char base[LIB_NAME_BUFFER_SIZE] = { 0, };
	const char* basename = NULL;
	size_t raw_length = 0;
	size_t base_length = 0;
	size_t title_start = 0;
	size_t title_end = 0;
	size_t marker_start = 0;
	size_t marker_end = 0;
	size_t year_start = 0;
	size_t year_end = 0;
	int season = -1;
	int episode = -1;
	int year = -1;
	bool has_episode = false;
	bool has_year = false;

	if (!path || !title_out || title_size == 0)
		return false;

	if (season_out)
		*season_out = -1;
	if (episode_out)
		*episode_out = -1;
	if (year_out)
		*year_out = -1;

	basename = Lib_path_basename(path);
	raw_length = strlen(basename);
	if (raw_length >= sizeof(raw))
		raw_length = sizeof(raw) - 1;
	memcpy(raw, basename, raw_length);
	raw[raw_length] = 0x00;

	//Drop the extension (but keep dot files).
	for (size_t i = raw_length; i > 1; i--)
	{
		if (raw[i - 1] == '.')
		{
			raw[i - 1] = 0x00;
			raw_length = i - 1;
			break;
		}
	}

	//Normalize separators and collapse spaces.
	for (size_t i = 0; i < raw_length; i++)
	{
		char c = raw[i];

		if (c == '.' || c == '_' || c == '(' || c == ')' || c == '[' || c == ']')
			c = ' ';
		if (c == ' ' && base_length > 0 && base[base_length - 1] == ' ')
			continue;

		base[base_length] = c;
		base_length++;
	}
	while (base_length > 0 && base[base_length - 1] == ' ')
		base_length--;
	base[base_length] = 0x00;

	has_episode = Lib_name_find_episode(base, base_length, &marker_start, &marker_end, &season, &episode);
	if (has_episode)
	{
		title_start = 0;
		title_end = marker_start;
		Lib_name_trim(base, &title_start, &title_end);

		if (title_end <= title_start)
		{
			//"1x02 - Pilot" -> "Pilot".
			title_start = marker_end;
			while (title_start < base_length && (base[title_start] == ' ' || base[title_start] == '-'))
				title_start++;
			title_end = base_length;
			Lib_name_trim(base, &title_start, &title_end);
		}
	}
	else
	{
		has_year = Lib_name_find_year(base, base_length, &year_start, &year_end, &year);
		if (has_year)
		{
			title_start = 0;
			title_end = year_start;
			Lib_name_trim(base, &title_start, &title_end);

			//Keep the whole name for things like "2001 A Space Odyssey".
			if (title_end <= title_start)
			{
				title_start = 0;
				title_end = base_length;
				Lib_name_trim(base, &title_start, &title_end);
			}
		}
		else
		{
			title_start = 0;
			title_end = base_length;
			Lib_name_trim(base, &title_start, &title_end);
		}
	}

	//Fall back to the cleaned file name when nothing useful was found.
	if (title_end <= title_start && base_length > 0)
	{
		title_start = 0;
		title_end = base_length;
		Lib_name_trim(base, &title_start, &title_end);
	}

	if (base_length > 0)
		Lib_name_copy(base, title_start, title_end, title_out, title_size);
	else
		Lib_name_copy(raw, 0, raw_length, title_out, title_size);

	if (season_out && has_episode)
		*season_out = season;
	if (episode_out && has_episode)
		*episode_out = episode;
	if (year_out && has_year)
		*year_out = year;

	return true;
}

const char* Lib_path_basename(const char* path)
{
	const char* basename = path;

	if (!path)
		return "";

	for (const char* p = path; *p; p++)
	{
		if (*p == '/' || *p == '\\')
			basename = p + 1;
	}

	return basename;
}

bool Lib_path_old_saved_pos_name(const char* path, char* out, size_t out_size)
{
	size_t length = 0;

	if (!path || !out || out_size == 0)
		return false;

	while (path[length])
	{
		if (length + 1 >= out_size)
		{
			out[0] = 0x00;
			return false;
		}

		out[length] = (path[length] == '/') ? '_' : path[length];
		length++;
	}

	out[length] = 0x00;
	return true;
}
