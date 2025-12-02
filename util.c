// Copyright © 2019-2024 Michael Forney
// util.c from cproc
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *
xmalloc(size_t len)
{
	void *buf;

	buf = malloc(len);
	if (!buf && len) {
		LOG("FAILED TO MALLOC FOR %zu", len);
		exit(1);
	}

	return buf;
}

void *
xcalloc(size_t memb, size_t s)
{
	void *buf;

	buf = calloc(memb, s);
	if (!buf)
		exit(1);

	return buf;
}

char *
xstrdup(const char *s)
{
	char *cpy = strdup(s);
	if (!cpy)
		exit(1);
	return cpy;
}

void *
reallocarray(void *buf, size_t n, size_t m)
{
	if (n > 0 && SIZE_MAX / n < m) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(buf, n * m);
}

void *
xreallocarray(void *buf, size_t n, size_t m)
{
	buf = reallocarray(buf, n, m);
	if (!buf && n && m)
		exit(1);

	return buf;
}

bool
starts_with_str(const char *str, const char *prefix)
{
	size_t len = strlen(prefix);
	return strncmp(str, prefix, len) == 0;
}

