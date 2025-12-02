#ifndef agenda_entry_h_INCLUDED
#define agenda_entry_h_INCLUDED

#include "hashmap.h"
#include "arena.h"
#include "path.h"
#include "tree.h"
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

struct agenda_entry {
	// filename is full path relative to fuse directory
	// I.E. hello world
	char *filename;
	// filename_original is the relative path to ICS_DIR
	// I.E. 910319208nrao19p.ics
	char *filename_vdir;
};

struct agenda_entry *
copy_agenda_entry(const struct agenda_entry *src);

void
free_agenda_entry(struct agenda_entry *entry);

struct agenda_entry *
create_agenda_entry(arena *ar, const char *filename,
		    const char *filename_vdir);
#endif // agenda_entry_h_INCLUDED

