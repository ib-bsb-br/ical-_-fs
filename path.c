#include "path.h"
#include "arena.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

path *
without_file_extension(arena *m, const path *p)
{
	char *cpy = rstrdup(m, p);
	char *last_dot = strrchr(cpy, '.');
	char *last_slash = strrchr(cpy, '/');

	// Only remove the extension if dot is after
	if (last_dot && (!last_slash || last_dot > last_slash)) {
		*last_dot = '\0';
	}
	return cpy;
}

path *
get_file_extension(arena *m, const path *p)
{
	char *cpy = rstrdup(m, p);
	char *last_dot = strrchr(cpy, '.');
	if (last_dot) {
		last_dot++;
	}

	return last_dot;
}

bool
pathIsHidden(const path *p)
{
	return p[0] == '.';
}

path *
append_path(arena *m, const path *parentp, const char *childp)
{
	char *filepath = NULL;
	size_t res = asprintf(&filepath, "%s/%s", parentp, childp);
	assert(res != -1);
	arena_register(m, filepath, free);
	return filepath;
}

const path *
get_filename(const char *full_path)
{
	char *base_name = strrchr(full_path, '/');
	if (!base_name)
		return (char *)full_path;
	return base_name + 1;
}

char *
get_parent_path(arena *ar, const char *path)
{
	char *path_copy = rstrdup(ar, path);

	char *last_slash = strrchr(path_copy, '/');
	if (last_slash == NULL) {
		return NULL;
	}

	// If they are the same, it means the parent is root
	if (strcmp(last_slash, path_copy) == 0) {
		return "/";
	}

	*last_slash = '\0';
	return path_copy;
}

size_t
split_path(arena *m, const path *p, char **segments)
{
	char *path_copy = rstrdup(m, p);

	size_t count = 0;
	char *segment;

	char *saveptr;
	segment = strtok_r(path_copy, "/", &saveptr);
	while (segment) {
		segments[count++] = rstrdup(m, segment);
		segment = strtok_r(NULL, "/", &saveptr);
	}
	return count;
}

size_t
write_to_file(const path *filepath, const char *content)
{
	FILE *f = fopen(filepath, "w");
	if (!f) {
		perror("Failed to open file");
		return -EIO;
	}
	fputs(content, f);
	fclose(f);
	return 0;
}

// Creates a new path
// my-file.txt -> my-file.1.txt
// Caller is responsible for freeing memory
path *
filename_numbered(const char *filename, size_t n)
{
	const char *ext = strrchr(filename, '.');
	char *new_filename;
	size_t len = 0;

	FILE *stream = open_memstream(&new_filename, &len);
	assert(stream);

	if (ext) {
		size_t base_len = ext - filename;
		fwrite(filename, 1, base_len, stream);
		fprintf(stream, ".%zu%s", n, ext);
	}
	else {
		fprintf(stream, "%s.%zu", filename, n);
	}

	fclose(stream);

	return new_filename;
}

