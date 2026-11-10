#include "library/lib_name.h"

#include <stdio.h>
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

static char Lib_name_lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool Lib_name_starts_ci(const char* text, size_t size, const char* word)
{
	size_t length = strlen(word);
	if (!text || length > size)
		return false;
	for (size_t i = 0; i < length; i++)
	{
		if (Lib_name_lower(text[i]) != word[i])
			return false;
	}
	return true;
}

static bool Lib_name_contains_ci(const char* text, size_t size, const char* word)
{
	size_t length = strlen(word);
	if (!text || length == 0 || length > size)
		return false;
	for (size_t i = 0; i + length <= size; i++)
	{
		if (Lib_name_starts_ci(text + i, size - i, word))
			return true;
	}
	return false;
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
	while (*start < *end && (text[*start] == ' ' || text[*start] == '-'))
		(*start)++;
	while (*end > *start && (text[*end - 1] == ' ' || text[*end - 1] == '-'))
		(*end)--;
}

static void Lib_name_copy(const char* text, size_t start, size_t end, char* out, size_t out_size)
{
	size_t length = 0;

	if (!out || out_size == 0)
		return;
	if (end > start)
		length = end - start;
	if (length >= out_size)
		length = out_size - 1;
	if (length > 0)
		memcpy(out, &text[start], length);
	out[length] = 0x00;
}

static size_t Lib_name_normalize(const char* text, size_t size, char* out, size_t out_size)
{
	size_t length = 0;
	if (!text || !out || out_size == 0)
		return 0;
	for (size_t i = 0; i < size && length + 1 < out_size; i++)
	{
		char c = text[i];
		if (c == '.' || c == '_' || c == '(' || c == ')' || c == '[' || c == ']')
			c = ' ';
		if (c == ' ' && length > 0 && out[length - 1] == ' ')
			continue;
		out[length++] = c;
	}
	while (length > 0 && out[length - 1] == ' ')
		length--;
	out[length] = 0x00;
	return length;
}

static bool Lib_name_find_episode(const char* text, size_t size,
size_t* marker_start, size_t* marker_end, int* season, int* episode)
{
	for (size_t i = 0; i < size; i++)
	{
		size_t position = 0;
		size_t after = 0;
		int parsed_season = -1;
		int parsed_episode = -1;

		if (i > 0 && Lib_name_is_alpha_num(text[i - 1]))
			continue;

		//S01E02 / S01 E02 / S01 - E02.
		if ((text[i] == 's' || text[i] == 'S') && i + 1 < size && Lib_name_is_digit(text[i + 1]))
		{
			position = i + 1;
			parsed_season = Lib_name_parse_number(text, size, &position, 3);
			while (position < size && (text[position] == ' ' || text[position] == '-'))
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

		if (parsed_season > 0 && parsed_season <= 99 && parsed_episode > 0
		&& parsed_episode <= LIB_NAME_MAX_EPISODE)
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

static bool Lib_name_find_episode_ordinal(const char* text, size_t size,
size_t* marker_start, size_t* marker_end, int* episode)
{
	for (size_t i = 0; i < size; i++)
	{
		size_t number_start = 0;
		size_t after = 0;
		bool marker = false;
		int parsed_episode = -1;

		if (i > 0 && Lib_name_is_alpha_num(text[i - 1]))
			continue;
		if (Lib_name_starts_ci(text + i, size - i, "episode"))
		{
			number_start = i + 7;
			while (number_start < size && (text[number_start] == ' ' || text[number_start] == '-'))
				number_start++;
			marker = true;
		}
		else if (Lib_name_starts_ci(text + i, size - i, "epis"))
		{
			//Also accept Portuguese "Episódio" (the accented bytes are skipped).
			number_start = i + 4;
			while (number_start < size && !Lib_name_is_digit(text[number_start])
			&& number_start - i < 20)
				number_start++;
			marker = true;
		}
		else if (Lib_name_starts_ci(text + i, size - i, "ep"))
		{
			number_start = i + 2;
			while (number_start < size && (text[number_start] == ' ' || text[number_start] == '-'))
				number_start++;
			marker = true;
		}
		else if ((text[i] == 'e' || text[i] == 'E') && i + 1 < size && Lib_name_is_digit(text[i + 1]))
		{
			number_start = i + 1;
			marker = true;
		}

		if (!marker || number_start >= size || !Lib_name_is_digit(text[number_start]))
			continue;
		after = number_start;
		parsed_episode = Lib_name_parse_number(text, size, &after, 4);
		if (parsed_episode <= 0 || parsed_episode > LIB_NAME_MAX_EPISODE)
			continue;
		*marker_start = i;
		*marker_end = after;
		*episode = parsed_episode;
		return true;
	}
	return false;
}

static bool Lib_name_find_leading_episode(const char* text, size_t size,
size_t* marker_start, size_t* marker_end, int* episode)
{
	size_t position = 0;
	int parsed_episode = -1;
	if (!text || size == 0 || !Lib_name_is_digit(text[0]))
		return false;
	parsed_episode = Lib_name_parse_number(text, size, &position, 4);
	if (parsed_episode <= 0 || parsed_episode > LIB_NAME_MAX_EPISODE
	|| (position < size && text[position] != ' ' && text[position] != '-'))
		return false;
	*marker_start = 0;
	*marker_end = position;
	*episode = parsed_episode;
	return true;
}

static bool Lib_name_find_year(const char* text, size_t size,
size_t* year_start, size_t* year_end, int* year)
{
	for (size_t i = 0; i + 4 <= size; i++)
	{
		size_t position = i;
		int value = 0;
		if ((i > 0 && Lib_name_is_digit(text[i - 1]))
		|| (i + 4 < size && (Lib_name_is_digit(text[i + 4])
		|| ((text[i + 4] == 'x' || text[i + 4] == 'X') && i + 5 < size && Lib_name_is_digit(text[i + 5])))))
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

static bool Lib_name_parse_season_component(const char* text, size_t size, int* season)
{
	char lower[LIB_NAME_BUFFER_SIZE] = { 0, };
	size_t length = Lib_name_normalize(text, size, lower, sizeof(lower));
	size_t position = 0;
	int value = -1;
	bool has_temp_word = (bool)(Lib_name_starts_ci(lower, length, "temp ")
	|| (length >= 4 && Lib_name_starts_ci(lower, length, "temp")
	&& (length == 4 || lower[4] == ' '))
	|| Lib_name_contains_ci(lower, length, " temp ")
	|| (length >= 5 && Lib_name_starts_ci(lower + length - 5, 5, " temp")));
	bool has_season_word = (bool)(Lib_name_contains_ci(lower, length, "season")
	|| Lib_name_contains_ci(lower, length, "temporada")
	|| has_temp_word);

	if (!season || length == 0)
		return false;
	if ((lower[0] == 's' || lower[0] == 'S' || lower[0] == 't' || lower[0] == 'T') && length > 1)
	{
		position = 1;
		while (position < length && lower[position] == ' ')
			position++;
		value = Lib_name_parse_number(lower, length, &position, 2);
		if (value > 0 && value <= 99 && position == length)
		{
			*season = value;
			return true;
		}
	}
	if (!has_season_word)
		return false;
	for (position = 0; position < length && !Lib_name_is_digit(lower[position]); position++)
		;
	if (position >= length)
		return false;
	value = Lib_name_parse_number(lower, length, &position, 2);
	if (value <= 0 || value > 99)
		return false;
	*season = value;
	return true;
}

static bool Lib_name_is_generic_folder(const char* text)
{
	static const char* const generic[] =
	{
		"sdmc:", "shows", "show", "series", "tv", "tv shows", "anime", "animes",
		"movies", "movie", "films", "videos", "video", "media", "temporadas",
	};
	char lower[LIB_NAME_BUFFER_SIZE] = { 0, };
	size_t length = text ? strlen(text) : 0;
	if (!text || length == 0)
		return true;
	Lib_name_normalize(text, length, lower, sizeof(lower));
	for (size_t i = 0; lower[i]; i++)
		lower[i] = Lib_name_lower(lower[i]);
	for (size_t i = 0; i < sizeof(generic) / sizeof(generic[0]); i++)
	{
		if (strcmp(lower, generic[i]) == 0)
			return true;
	}
	return false;
}

static bool Lib_name_is_generic_media_title(const char* text)
{
	static const char* const generic[] =
	{
		"movie", "film", "video", "feature", "main", "full movie", "full film",
	};
	char lower[LIB_NAME_BUFFER_SIZE] = { 0, };
	size_t length = text ? strlen(text) : 0;
	if (!text || length == 0)
		return false;
	Lib_name_normalize(text, length, lower, sizeof(lower));
	for (size_t i = 0; lower[i]; i++)
		lower[i] = Lib_name_lower(lower[i]);
	for (size_t i = 0; i < sizeof(generic) / sizeof(generic[0]); i++)
	{
		if (strcmp(lower, generic[i]) == 0)
			return true;
	}
	return false;
}

static bool Lib_name_is_generic_folder_segment(const char* text, size_t size)
{
	char component[LIB_NAME_BUFFER_SIZE] = { 0, };
	if (!text || size == 0 || size >= sizeof(component))
		return true;
	Lib_name_normalize(text, size, component, sizeof(component));
	return Lib_name_is_generic_folder(component);
}

static bool Lib_name_find_season_folder(const char* path, int* season,
char* series_title, size_t series_title_size)
{
	const char* segment = path;
	const char* previous = NULL;
	size_t previous_length = 0;
	int parsed_season = -1;
	if (series_title && series_title_size > 0)
		series_title[0] = 0x00;
	if (!path || !season)
		return false;

	while (*segment)
	{
		const char* end = segment;
		while (*end && *end != '/' && *end != '\\')
			end++;
		if (*end == 0x00)
			break; //Last component is the filename.
		if (Lib_name_parse_season_component(segment, (size_t)(end - segment), &parsed_season))
		{
			if (previous && series_title && series_title_size > 0
			&& !Lib_name_is_generic_folder_segment(previous, previous_length))
			{
				Lib_name_normalize(previous, previous_length, series_title, series_title_size);
				if (Lib_name_is_generic_folder(series_title))
					series_title[0] = 0x00;
			}
			*season = parsed_season;
			return true;
		}
		previous = segment;
		previous_length = (size_t)(end - segment);
		segment = end + 1;
	}
	return false;
}

static bool Lib_name_parent_folder(const char* path, char* title, size_t title_size)
{
	const char* basename = NULL;
	const char* end = NULL;
	const char* start = NULL;
	if (!path || !title || title_size == 0)
		return false;
	title[0] = 0x00;
	basename = Lib_path_basename(path);
	if (basename == path)
		return false;
	end = basename - 1;
	while (end > path && (*end == '/' || *end == '\\'))
		end--;
	start = end;
	while (start > path && start[-1] != '/' && start[-1] != '\\')
		start--;
	if (end < start || (size_t)(end - start + 1) >= title_size)
		return false;
	Lib_name_normalize(start, (size_t)(end - start + 1), title, title_size);
	if (Lib_name_is_generic_folder(title))
	{
		title[0] = 0x00;
		return false;
	}
	return (bool)(title[0] != 0x00);
}

bool Lib_name_parse_info(const char* path, char* title_out, size_t title_size,
char* episode_title_out, size_t episode_title_size, Lib_name_info* info)
{
	char raw[LIB_NAME_BUFFER_SIZE] = { 0, };
	char base[LIB_NAME_BUFFER_SIZE] = { 0, };
	char folder_title[LIB_NAME_BUFFER_SIZE] = { 0, };
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
	bool has_episode_name = false;
	bool has_year = false;
	bool has_season_folder = false;
	bool has_parent_folder = false;
	bool is_series = false;

	if (!path || !title_out || title_size == 0)
		return false;
	title_out[0] = 0x00;
	if (episode_title_out && episode_title_size > 0)
		episode_title_out[0] = 0x00;
	if (info)
	{
		info->season = -1;
		info->episode = -1;
		info->year = -1;
		info->is_series = false;
		info->episode_known = false;
	}

	basename = Lib_path_basename(path);
	raw_length = strlen(basename);
	if (raw_length >= sizeof(raw))
		raw_length = sizeof(raw) - 1;
	memcpy(raw, basename, raw_length);
	raw[raw_length] = 0x00;

	//Drop the extension.
	for (size_t i = raw_length; i > 1; i--)
	{
		if (raw[i - 1] == '.')
		{
			raw[i - 1] = 0x00;
			raw_length = i - 1;
			break;
		}
	}
	base_length = Lib_name_normalize(raw, raw_length, base, sizeof(base));
	has_season_folder = Lib_name_find_season_folder(path, &season, folder_title, sizeof(folder_title));
	has_parent_folder = Lib_name_parent_folder(path, folder_title, sizeof(folder_title));

	has_episode = Lib_name_find_episode(base, base_length, &marker_start, &marker_end, &season, &episode);
	if (!has_episode)
	{
		has_episode_name = Lib_name_find_episode_ordinal(base, base_length,
		&marker_start, &marker_end, &episode);
		if (!has_episode_name && has_season_folder)
			has_episode_name = Lib_name_find_leading_episode(base, base_length,
			&marker_start, &marker_end, &episode);
	}

	if (has_episode || has_episode_name || has_season_folder)
	{
		char parsed_folder_title[LIB_NAME_BUFFER_SIZE] = { 0, };
		is_series = true;
		if (has_season_folder)
		{
			(void)Lib_name_find_season_folder(path, &season, parsed_folder_title, sizeof(parsed_folder_title));
			snprintf(folder_title, sizeof(folder_title), "%s", parsed_folder_title);
		}
		else if (has_parent_folder && folder_title[0] != 0x00)
			season = 1;

		//With a season directory, the folder immediately above it is the series name.
		//Otherwise use the title prefix before SxxExx, then fall back to the parent folder.
		title_start = 0;
		title_end = has_episode || has_episode_name ? marker_start : 0;
		Lib_name_trim(base, &title_start, &title_end);
		if (has_season_folder && folder_title[0] != 0x00)
		{
			Lib_name_copy(folder_title, 0, strlen(folder_title), title_out, title_size);
		}
		else if (title_end > title_start)
		{
			Lib_name_copy(base, title_start, title_end, title_out, title_size);
		}
		else if (has_parent_folder && folder_title[0] != 0x00)
		{
			Lib_name_copy(folder_title, 0, strlen(folder_title), title_out, title_size);
		}
		else
		{
			//For a marker without a show folder/name, retain the episode's own title.
			title_start = has_episode || has_episode_name ? marker_end : 0;
			while (title_start < base_length && (base[title_start] == ' ' || base[title_start] == '-'))
				title_start++;
			title_end = base_length;
			Lib_name_trim(base, &title_start, &title_end);
			Lib_name_copy(base, title_start, title_end, title_out, title_size);
		}

		if ((has_episode || has_episode_name) && episode_title_out && episode_title_size > 0)
		{
			title_start = marker_end;
			while (title_start < base_length && (base[title_start] == ' ' || base[title_start] == '-'))
				title_start++;
			title_end = base_length;
			Lib_name_trim(base, &title_start, &title_end);
			if (title_end > title_start)
				Lib_name_copy(base, title_start, title_end, episode_title_out, episode_title_size);
			else
				snprintf(episode_title_out, episode_title_size, "Episode %02d", episode > 0 ? episode : 0);
		}
		else if (is_series && episode_title_out && episode_title_size > 0)
			Lib_name_copy(base, 0, base_length, episode_title_out, episode_title_size);
		if (season <= 0 && is_series)
			season = 1;
	}
	else
	{
		has_year = Lib_name_find_year(base, base_length, &year_start, &year_end, &year);
		if (has_year)
		{
			title_start = 0;
			title_end = year_start;
			Lib_name_trim(base, &title_start, &title_end);
			//Keep the whole title for "2001 A Space Odyssey".
			if (title_end <= title_start)
			{
				title_start = 0;
				title_end = base_length;
				Lib_name_trim(base, &title_start, &title_end);
			}
		}
		else
		{
			if (has_parent_folder && folder_title[0] != 0x00
			&& (Lib_name_find_year(folder_title, strlen(folder_title),
			&year_start, &year_end, &year) || Lib_name_is_generic_media_title(base)))
			{
				has_year = (bool)(year > 0);
				title_start = 0;
				title_end = has_year ? year_start : strlen(folder_title);
				Lib_name_trim(folder_title, &title_start, &title_end);
				if (title_end <= title_start)
				{
				title_start = 0;
				title_end = strlen(folder_title);
				}
				Lib_name_copy(folder_title, title_start, title_end, title_out, title_size);
			}
			else
			{
				title_start = 0;
				title_end = base_length;
				Lib_name_trim(base, &title_start, &title_end);
			}
		}
		if (title_out[0] == 0x00 && title_end <= title_start && base_length > 0)
		{
			title_start = 0;
			title_end = base_length;
		}
		if (title_out[0] == 0x00)
			Lib_name_copy(base, title_start, title_end, title_out, title_size);
	}

	if (title_out[0] == 0x00)
		Lib_name_copy(raw, 0, raw_length, title_out, title_size);
	if (info)
	{
		info->season = is_series ? season : -1;
		info->episode = (has_episode || has_episode_name) ? episode : -1;
		info->year = has_year ? year : -1;
		info->is_series = is_series;
		info->episode_known = (bool)(has_episode || has_episode_name);
	}
	return true;
}

bool Lib_name_parse(const char* path, char* title_out, size_t title_size,
int* season_out, int* episode_out, int* year_out)
{
	Lib_name_info info = { -1, -1, -1, false, false };
	if (!Lib_name_parse_info(path, title_out, title_size, NULL, 0, &info))
		return false;
	if (season_out)
		*season_out = info.season;
	if (episode_out)
		*episode_out = info.episode;
	if (year_out)
		*year_out = info.year;
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
