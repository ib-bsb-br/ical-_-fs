#ifndef arena_h_INCLUDED
#define arena_h_INCLUDED
#include "util.h"
#include <assert.h>
#include <libical/ical.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*free_func_t)(void *);

typedef struct memory_block {
	void *ptr;
	free_func_t free_func;
	struct memory_block *next;
} memory_block;

/*
 * Dynamic arena allocator
 */
typedef struct {
	memory_block *head;
} arena;

arena *
create_arena(void);

void
arena_register(arena *region, void *ptr, free_func_t free_func);

void *
rmalloc(arena *region, size_t size);

void
free_all(arena *region);

char *
rstrndup(arena *ar, const char *src, size_t len);

char *
rstrins(arena *region, const char *str, size_t offset, const char *buf,
	size_t size);
// String functions
char *
rstrdup(arena *region, const char *str);

int
rasprintf(arena *region, char **strp, const char *fmt, ...);

// iCal functions

icalparser *
ricalparser_new(arena *region);

void
ricalcomponent_free(icalcomponent *ic);

icalcomponent *
ricalcomponent_empty(arena *region);

icalcomponent *
ricalcomponent_new_clone(arena *region, icalcomponent *ic);

icalcomponent *
ricalcomponent_new_from_string(arena *region, const char *ic);

char *
ricalcomponent_as_ical_string(arena *region, icalcomponent *ic);

char *
ricalcomponent_as_ical_string_r(arena *region, icalcomponent *ic);

icalcomponent *
ricalcomponent_new_vcalendar(arena *region);
#endif // mrgeion_h_INCLUDED
