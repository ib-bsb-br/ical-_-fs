#include "agenda_entry.h"
#include "hashmap.h"
#include "path.h"
#include "tree.h"
#include "util.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
char VDIR[256];

char FILE_EXTENSION[256] = "";

// A structure of the current root
struct tree_node *fuse_root = NULL;
// ics entries, keys are the filename as stored in ICS_DIR
// I.E. bcb4c14b-8f3f-4a53-ad33-1f4499071a9m-caldavfs.ics
struct hashmap *entries_vdir = NULL;

// Copies vdir
void
set_vdir(const char *expanded_path)
{
	strlcpy(VDIR, expanded_path, sizeof(VDIR));
}

const char *
get_default_file_extension()
{
	return FILE_EXTENSION;
}

int
set_file_extension(const char *extension)
{
	if (extension[0] == '.') {
		return -1;
	}

	if (strlen(extension) > 256) {
		return -2;
	}

	strlcpy(FILE_EXTENSION, extension, sizeof(FILE_EXTENSION));
	return 0;
}

int
set_node_filename(struct tree_node *node, const char *filename)
{
	struct agenda_entry *entry = node->data;
	free(entry->filename);
	entry->filename = xstrdup(filename);
	return 0;
}

struct tree_node *
get_fuse_node_from_vdir_name(const char *vdir_name)
{
	return hashmap_get(entries_vdir, vdir_name);
}

const struct agenda_entry *
get_entry(const struct tree_node *node)
{
	return node->data;
}

bool
is_root_node(const struct tree_node *node)
{
	// Root node is the only item without an agenda_entry
	return !node->data;
}

const char *
get_node_filename(const struct tree_node *node)
{
	struct agenda_entry *entry = node->data;
	if (entry == NULL)
		return "";
	return entry->filename;
}

const char *
get_vdir_filepath(arena *ar, const struct tree_node *node)
{
	const struct agenda_entry *entry = get_entry(node);
	return append_path(ar, VDIR, entry->filename_vdir);
}

void
delete_fuse_node(struct tree_node *node)
{
	const struct agenda_entry *e = get_entry(node);
	hashmap_remove(entries_vdir, e->filename_vdir);
	detach_tree_node(node);
	free_tree(node);
}

// Copies agenda_entry
struct tree_node *
create_fuse_node(const struct agenda_entry *entry)
{
	struct agenda_entry *cpy = copy_agenda_entry(entry);

	struct tree_node *new_node =
	    create_tree_node(cpy, (void *)free_agenda_entry);

	hashmap_insert(entries_vdir, cpy->filename_vdir, new_node);

	return new_node;
}

struct tree_node *
upsert_fuse_node(const struct agenda_entry *entry)
{
	struct tree_node *node =
	    hashmap_get(entries_vdir, entry->filename_vdir);
	if (node) {
		set_node_filename(node, entry->filename);
	}
	else {
		node = create_fuse_node(entry);
	}

	return node;
}

// Add fuse child to node.
// OBS: Modifies filename if not unique!
size_t
add_fuse_child(struct tree_node *parent, struct tree_node *child)
{
	const char *filename = get_node_filename(child);

	char *new_filename = xstrdup(filename);

	size_t conflict_count = 1;

	while (true) {
		bool is_unique = true;
		for (size_t i = 0; i < parent->child_count; i++) {
			const char *sibling_filename =
			    get_node_filename(parent->children[i]);

			if (strcmp(new_filename, sibling_filename) == 0) {
				is_unique = false;
				break;
			}
		}

		if (is_unique) {
			break;
		}

		free(new_filename);
		new_filename = filename_numbered(filename, conflict_count++);
	}

	set_node_filename(child, new_filename);

	return add_child(parent, child);
}

size_t
move_fuse_node(struct tree_node *new_parent, struct tree_node *child)
{
	detach_tree_node(child);
	return add_fuse_child(new_parent, child);
}
