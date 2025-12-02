#ifndef fuse_node_h_INCLUDED
#define fuse_node_h_INCLUDED

#include "agenda_entry.h"
#include "fuse_node_store.h"
#include "hashmap.h"
#include "ical_extra.h"
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

struct tree_node *
get_node_by_uuid(arena *ar, const char *target_uuid);

struct tree_node *
get_node_by_path(arena *ar, const char *path);

bool
node_is_directory(arena *ar, const struct tree_node *node,
		  icalcomponent *node_ics);

size_t
write_ical_file(arena *ar, const struct tree_node *node,
		icalcomponent *ic);

// Owner: ctx
icalcomponent *
get_icalcomponent_from_node(arena *ar, const struct tree_node *n);

int
write_parent_child_components(arena *ar,
			      const struct tree_node *parent,
			      icalcomponent *iparent,
			      const struct tree_node *child,
			      icalcomponent *ichild);

// Writes ics component summary to the filename of the tree_node
int
update_summary(arena *ar, icalcomponent *ics,
	       const struct tree_node *node);

struct agenda_entry *
parse_ics_to_agenda_entry(arena *ar, const char *vdir_filepath);

// agenda_entry is automatically cleaned up by ar, use copy_agenda_entry
// to take ownership!
struct agenda_entry *
load_agenda_entry_from_ics_file(arena *ar, const char *filename);

void
load_root_node_tree();

// Returns comma separated list of categories for a file
char *
get_node_categories(arena *ar, const struct tree_node *node);

int
delete_node_categories(arena *ar, const struct tree_node *node);

int
set_node_categories(arena *ar, const struct tree_node *node,
		    const char *new_categories, size_t s);

const char *
get_node_class(arena *ar, const struct tree_node *node);

const char *
get_node_status(arena *ar, const struct tree_node *node);

int
delete_node_class(arena *ar, const struct tree_node *node);

int
set_node_class(arena *ar, const struct tree_node *node,
	       const icalproperty_class new_class);

int
set_node_status(arena *ar, const struct tree_node *node,
		const icalproperty_status new_status);

struct stat
get_node_stat(arena *ar, const struct tree_node *node,
	      icalcomponent *node_component);

int
insert_fuse_node_to_path(arena *ar, const char *fuse_path,
			 struct tree_node *child_node,
			 icalcomponent *child_ics);

enum ENTRY_TYPE { ENTRY_DIRECTORY, ENTRY_FILE };

int
create_entry_from_fuse(arena *ar, const char *fuse_path,
		       enum ENTRY_TYPE etype);

const char *
get_dtstart(arena *ar, struct tree_node *n);

int
set_dtstart(arena *ar, const char *dtstart_c,
	    const struct tree_node *node);

int
clear_dtstart(arena *ar, struct tree_node *node);
const char *
get_node_due(arena *ar, const struct tree_node *node);

int
set_node_due(arena *ar, const char *due_c, const struct tree_node *node);

int
clear_node_due(arena *ar, const struct tree_node *node);

int
set_node_priority(arena *ar, const struct tree_node *node, int priority);

int
get_node_priority(arena *ar, const struct tree_node *node);

int
get_node_percentcomplete(arena *ar, const struct tree_node *node);

int
set_node_percentcomplete(arena *ar, const struct tree_node *node,
                        int percent);

const char *
get_node_assignee(arena *ar, const struct tree_node *node);

int
set_node_assignee(arena *ar, const struct tree_node *node, const char *value);

char *
get_node_links(arena *ar, const struct tree_node *node);
int
create_directory_from_fuse_path(arena *ar, const char *fuse_path);

void
update_or_create_fuse_entry_from_vdir(arena *ar,
				      const char *filepath_vdir);

int
delete_dir_from_fuse_path(arena *ar, const char *filepath);

int
do_agenda_rename(arena *ar, const char *, const char *);

int
delete_vdir_entry(arena *ar, struct tree_node *node);

int
delete_from_vdir_path(arena *ar, const char *filepath);

#endif // fuse_node_h_INCLUDED
