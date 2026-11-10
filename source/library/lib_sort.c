#include "library/lib_sort.h"
#include "library/lib_name.h"

#include <stdlib.h>
#include <string.h>

static char Lib_sort_lower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (char)(c - 'A' + 'a');
	return c;
}

static bool Lib_sort_is_digit(char c)
{
	return (bool)(c >= '0' && c <= '9');
}

int Lib_sort_natural_compare(const char* a, const char* b)
{
	if (!a)
		a = "";
	if (!b)
		b = "";

	while (*a && *b)
	{
		if (Lib_sort_is_digit(*a) && Lib_sort_is_digit(*b))
		{
			const char* start_a = a;
			const char* start_b = b;
			size_t length_a = 0;
			size_t length_b = 0;

			while (*a == '0')
				a++;
			while (*b == '0')
				b++;

			start_a = a;
			start_b = b;
			while (Lib_sort_is_digit(*a))
			{
				a++;
				length_a++;
			}
			while (Lib_sort_is_digit(*b))
			{
				b++;
				length_b++;
			}

			if (length_a != length_b)
				return (length_a < length_b) ? -1 : 1;
			if (length_a > 0)
			{
				int result = memcmp(start_a, start_b, length_a);
				if (result != 0)
					return (result < 0) ? -1 : 1;
			}
			continue;
		}

		{
			char ca = Lib_sort_lower(*a);
			char cb = Lib_sort_lower(*b);

			if (ca != cb)
				return (ca < cb) ? -1 : 1;
		}

		a++;
		b++;
	}

	if (*a)
		return 1;
	if (*b)
		return -1;
	return 0;
}

static int Lib_sort_compare_primary(const Lib_item* a, const Lib_item* b, const Lib_sort_spec* spec)
{
	switch (spec->key)
	{
		case LIB_SORT_KEY_DATE_ADDED:
			if (a->first_seen != b->first_seen)
				return (a->first_seen < b->first_seen) ? -1 : 1;
			break;

		case LIB_SORT_KEY_LAST_PLAYED:
			if (a->last_played != b->last_played)
				return (a->last_played < b->last_played) ? -1 : 1;
			break;

		case LIB_SORT_KEY_DURATION:
			if (a->duration_ms != b->duration_ms)
				return (a->duration_ms < b->duration_ms) ? -1 : 1;
			break;

		case LIB_SORT_KEY_SIZE:
			if (a->size != b->size)
				return (a->size < b->size) ? -1 : 1;
			break;

		case LIB_SORT_KEY_NAME:
		default:
		{
			int result = Lib_sort_natural_compare(Lib_item_get_title(a), Lib_item_get_title(b));
			if (result != 0)
				return result;
			break;
		}
	}

	return 0;
}

int Lib_sort_compare_items(const Lib_item* a, const Lib_item* b, const Lib_sort_spec* spec)
{
	Lib_sort_spec default_spec = { LIB_SORT_KEY_NAME, false };
	int primary = 0;
	int tie = 0;

	if (!a || !b)
		return 0;
	if (!spec)
		spec = &default_spec;

	primary = Lib_sort_compare_primary(a, b, spec);
	if (spec->descending)
		primary = -primary;
	if (primary != 0)
		return primary;

	tie = Lib_sort_natural_compare(a->path ? a->path : "", b->path ? b->path : "");
	if (tie != 0)
		return tie;

	if (a->id == b->id)
		return 0;

	return (a->id < b->id) ? -1 : 1;
}

static void Lib_sort_merge_items(Lib_item* items, Lib_item* temp, size_t left, size_t middle,
size_t right, const Lib_sort_spec* spec)
{
	size_t i = left;
	size_t j = middle;
	size_t out = left;

	while (i < middle && j < right)
	{
		if (Lib_sort_compare_items(&items[i], &items[j], spec) <= 0)
			temp[out++] = items[i++];
		else
			temp[out++] = items[j++];
	}

	while (i < middle)
		temp[out++] = items[i++];
	while (j < right)
		temp[out++] = items[j++];

	memcpy(&items[left], &temp[left], (right - left) * sizeof(Lib_item));
}

static void Lib_sort_merge_const_ptrs(const Lib_item** items, const Lib_item** temp,
size_t left, size_t middle, size_t right, const Lib_sort_spec* spec)
{
	size_t i = left;
	size_t j = middle;
	size_t out = left;

	while (i < middle && j < right)
	{
		if (Lib_sort_compare_items(items[i], items[j], spec) <= 0)
			temp[out++] = items[i++];
		else
			temp[out++] = items[j++];
	}

	while (i < middle)
		temp[out++] = items[i++];
	while (j < right)
		temp[out++] = items[j++];

	for (size_t k = left; k < right; k++)
		items[k] = temp[k];
}

static void Lib_sort_insertion_items(Lib_item* items, size_t count, const Lib_sort_spec* spec)
{
	for (size_t i = 1; i < count; i++)
	{
		Lib_item value = items[i];
		size_t j = i;

		while (j > 0 && Lib_sort_compare_items(&items[j - 1], &value, spec) > 0)
		{
			items[j] = items[j - 1];
			j--;
		}
		items[j] = value;
	}
}

static void Lib_sort_insertion_const_ptrs(const Lib_item** items, size_t count, const Lib_sort_spec* spec)
{
	for (size_t i = 1; i < count; i++)
	{
		const Lib_item* value = items[i];
		size_t j = i;

		while (j > 0 && Lib_sort_compare_items(items[j - 1], value, spec) > 0)
		{
			items[j] = items[j - 1];
			j--;
		}
		items[j] = value;
	}
}

bool Lib_sort_items(Lib_item* items, size_t count, const Lib_sort_spec* spec)
{
	Lib_item* temp = NULL;

	if (!items || count < 2)
		return true;

	if (count > (SIZE_MAX / sizeof(Lib_item)))
		return false;

	temp = (Lib_item*)malloc(count * sizeof(Lib_item));
	if (!temp)
	{
		Lib_sort_insertion_items(items, count, spec);
		return false;
	}

	for (size_t width = 1; width < count;)
	{
		for (size_t left = 0; left < count; left += width * 2)
		{
			size_t middle = (left + width < count) ? left + width : count;
			size_t right = (left + (width * 2) < count) ? left + (width * 2) : count;

			Lib_sort_merge_items(items, temp, left, middle, right, spec);
		}

		if (width > (SIZE_MAX / 2))
			break;
		width *= 2;
	}

	free(temp);
	return true;
}

bool Lib_sort_const_ptrs(const Lib_item** items, size_t count, const Lib_sort_spec* spec)
{
	const Lib_item** temp = NULL;

	if (!items || count < 2)
		return true;

	if (count > (SIZE_MAX / sizeof(const Lib_item*)))
		return false;

	temp = (const Lib_item**)malloc(count * sizeof(const Lib_item*));
	if (!temp)
	{
		Lib_sort_insertion_const_ptrs(items, count, spec);
		return false;
	}

	for (size_t width = 1; width < count;)
	{
		for (size_t left = 0; left < count; left += width * 2)
		{
			size_t middle = (left + width < count) ? left + width : count;
			size_t right = (left + (width * 2) < count) ? left + (width * 2) : count;

			Lib_sort_merge_const_ptrs(items, temp, left, middle, right, spec);
		}

		if (width > (SIZE_MAX / 2))
			break;
		width *= 2;
	}

	free(temp);
	return true;
}
