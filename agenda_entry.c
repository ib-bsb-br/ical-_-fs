#include "agenda_entry.h"
#include "arena.h"
#include "util.h"
#include <dirent.h>
#include <libical/ical.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <wordexp.h>

struct agenda_entry *
copy_agenda_entry(const struct agenda_entry *src)
{
        // Root node
        if (!src) {
                return NULL;
        }
        struct agenda_entry *copy = xmalloc(sizeof(struct agenda_entry));
        copy->filename = xstrdup(src->filename);
        copy->filename_vdir = xstrdup(src->filename_vdir);
        copy->type = src->type;
        return copy;
}

void
free_agenda_entry(struct agenda_entry *entry)
{
	if (!entry)
		return;
	if (entry->filename)
		free(entry->filename);
	if (entry->filename_vdir)
		free(entry->filename_vdir);
	free(entry);
	return;
}

struct agenda_entry *
create_agenda_entry(arena *ar, const char *filename,
                    const char *filename_vdir, enum EntryType type)
{
        struct agenda_entry *copy = rmalloc(ar, sizeof(struct agenda_entry));
        copy->filename = rstrdup(ar, filename);
        copy->filename_vdir = rstrdup(ar, filename_vdir);
        copy->type = type;
        return copy;
}
