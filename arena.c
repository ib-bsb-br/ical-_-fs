#include "arena.h"
#include "util.h"
#include <assert.h>
#include <libical/ical.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*free_func_t)(void *);

arena *
create_arena(void)
{
	arena *region = xmalloc(sizeof(arena));
	region->head = NULL;
	return region;
}

void
arena_register(arena *region, void *ptr, free_func_t free_func)
{
	assert(free_func);
	memory_block *block = xmalloc(sizeof(memory_block));
	block->ptr = ptr;
	block->free_func = free_func;
	block->next = region->head;
	region->head = block;
}

void *
rmalloc(arena *region, size_t size)
{
	void *ptr = xmalloc(size);
	arena_register(region, ptr, free);
	return ptr;
}

// Copies and null terminates a string
char *
rstrndup(arena *ar, const char *src, size_t len)
{
	char *buf = rmalloc(ar, len + 1);
	memcpy(buf, src, len);
	buf[len] = '\0';
	return buf;
}

void
free_all(arena *region)
{
	if (!region)
		return;
	memory_block *block = region->head;
	while (block) {
		memory_block *next = block->next;
		if (block->free_func)
			block->free_func(block->ptr);
		free(block);
		block = next;
	}
	region->head = NULL;
	free(region);
}

// String functions
char *
rstrdup(arena *region, const char *str)
{
	char *copy = xstrdup(str);
	arena_register(region, copy, free);
	return copy;
}

char *
rstrins(arena *region, const char *str, size_t offset, const char *buf,
	size_t size)
{
	size_t str_len = strlen(str);

	size_t previous_new_len = (offset <= str_len) ? offset : str_len;

	char *result = NULL;
	size_t result_len = 0;

	FILE *stream = open_memstream(&result, &result_len);
	assert(stream);

	fwrite(str, 1, previous_new_len, stream);
	fwrite(buf, 1, size, stream);

	fclose(stream);

	arena_register(region, result, free);

	return result;
}

int
rasprintf(arena *region, char **strp, const char *fmt, ...)
{
	// Calculate length
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (len < 0) {
		*strp = NULL;
		return -1;
	}

	char *buffer = rmalloc(region, len + 1);
	va_start(args, fmt);
	int ret = vsnprintf(buffer, len + 1, fmt, args);
	va_end(args);
	if (ret < 0) {
		*strp = NULL;
		return -1;
	}
	*strp = buffer;
	return ret;
}

// iCal functions

icalparser *
ricalparser_new(arena *region)
{
	icalparser *p = icalparser_new();
	arena_register(region, p, (void *)icalparser_free);
	return p;
}

void
ricalcomponent_free(icalcomponent *ic)
{
	if (ic)
		icalcomponent_free(ic);
}

icalcomponent *
ricalcomponent_empty(arena *region)
{
	icalcomponent *ic = NULL;
	arena_register(region, ic, (void *)ricalcomponent_free);
	return ic;
}

icalcomponent *
ricalcomponent_new_clone(arena *region, icalcomponent *ic)
{
	icalcomponent *nic = icalcomponent_new_clone(ic);
	arena_register(region, nic, (void *)ricalcomponent_free);
	return nic;
}

icalcomponent *
ricalcomponent_new_from_string(arena *region, const char *ic)
{
	icalcomponent *nic = icalcomponent_new_from_string(ic);
	arena_register(region, nic, (void *)ricalcomponent_free);
	return nic;
}

char *
ricalcomponent_as_ical_string(arena *region, icalcomponent *ic)
{
	char *c = icalcomponent_as_ical_string(ic);
	arena_register(region, c, free);
	return c;
}

char *
ricalcomponent_as_ical_string_r(arena *region, icalcomponent *ic)
{
	char *c = icalcomponent_as_ical_string_r(ic);
	arena_register(region, c, free);
	return c;
}

icalcomponent *
ricalcomponent_new_vcalendar(arena *region)
{
	icalcomponent *new_comp = icalcomponent_new_vcalendar();
	arena_register(region, new_comp, (void *)ricalcomponent_free);
	return new_comp;
}
