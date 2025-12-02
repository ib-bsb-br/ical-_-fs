#ifndef path_h_INCLUDED
#define path_h_INCLUDED
#include "arena.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char path;

path *
without_file_extension(arena *m, const path *p);

path *
append_path(arena *m, const path *parentp, const char *childp);

bool
pathIsHidden(const path *p);

const path *
get_filename(const char *full_path);

path *
get_file_extension(arena *m, const path *p);

char *
get_parent_path(arena *ar, const char *path);

size_t
split_path(arena *m, const path *p, char **segments);

size_t
write_to_file(const path *filepath, const char *content);

path *
filename_numbered(const char *filename, size_t n);

#endif // path_h_INCLUDED
