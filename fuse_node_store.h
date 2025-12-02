#ifndef fuse_node_store_h_INCLUDED
#define fuse_node_store_h_INCLUDED
#include "agenda_entry.h"
#include "hashmap.h"
#include "util.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
extern char VDIR[256];

// A structure of the current root
extern struct tree_node *fuse_root;
// ics entries, keys are the filename as stored in ICS_DIR
// I.E. bcb4c14b-8f3f-4a53-ad33-1f4499071a9m-caldavfs.ics
extern struct hashmap *entries_vdir;

void
set_vdir(const char *expanded_path);

int
set_node_filename(struct tree_node *node, const char *filename);

struct tree_node *
get_fuse_node_from_vdir_name(const char *vdir_name);

const struct agenda_entry *
get_entry(const struct tree_node *node);

const char *
get_vdir_filepath(arena *ar, const struct tree_node *node);

bool
is_root_node(const struct tree_node *node);

const char *
get_node_filename(const struct tree_node *node);

int
set_node_filename(struct tree_node *node, const char *filename);

struct tree_node *
create_fuse_node(const struct agenda_entry *entry);

void
delete_fuse_node(struct tree_node *node);

struct tree_node *
upsert_fuse_node(const struct agenda_entry *entry);

size_t
add_fuse_child(struct tree_node *parent, struct tree_node *child);

size_t
move_fuse_node(struct tree_node *new_parent, struct tree_node *child);

const char *
get_default_file_extension();

int
set_file_extension(const char *);

#endif // fuse_node_store_h_INCLUDED
