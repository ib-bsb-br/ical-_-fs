#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef DEBUG
#define LOG(fmt, ...)                                                          \
	fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

void *
reallocarray(void *, size_t, size_t);
void *
xreallocarray(void *, size_t, size_t);
void *
xmalloc(size_t);
void *
xcalloc(size_t, size_t);
char *
xstrdup(const char *s);
bool
starts_with_str(const char *str, const char *prefix);
#endif
