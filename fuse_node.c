#include "agenda_entry.h"
#include "arena.h"
#include "fuse_node_store.h"
#include "hashmap.h"
#include "ical_extra.h"
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
get_node_by_uuid(arena *ar, const char *target_uuid)
{
	char *filename_vdir = NULL;

	// Filenames are uid.ics, so we levarage that
	size_t wRes = rasprintf(ar, &filename_vdir, "%s.ics", target_uuid);
	assert(wRes != -1);

	LOG("Looking for UID: '%s'", target_uuid);
	LOG("Full filename: '%s'", filename_vdir);

	struct tree_node *node = get_fuse_node_from_vdir_name(filename_vdir);
	LOG("Found it: %b", node != NULL);

	return node;
}

struct tree_node *
get_node_by_path(arena *ar, const char *path)
{
	if (strcmp(path, "/") == 0) {
		LOG("Is ROOT %s", path);
		return fuse_root;
	}

	char *segments[255];
	size_t count = split_path(ar, path, segments);

	struct tree_node *current = fuse_root;

	for (size_t i = 0; i < count && current; ++i) {
		const char *segment = segments[i];
		struct tree_node *next = NULL;
		LOG("Segment: %s", segment);

		for (size_t j = 0; j < current->child_count; ++j) {

			const char *child_name =
			    get_node_filename(current->children[j]);
			if (strcmp(child_name, segment) == 0) {

				next = current->children[j];
				break;
			}
		}
		if (next == NULL) {
			return NULL;
		}
		current = next;
	}
	return current;
}

bool
node_is_directory(arena *ar, const struct tree_node *node,
		  icalcomponent *node_ics)
{
	if (is_root_node(node) || node_has_children(node)) {
		return true;
	}
	else {
		// When user creates a directory, a directory component is
		// stored in the ics file. So even if a node has no children
		// it can still be that it should be shown as a directory.
		return is_directory_component(node_ics);
	}
}

size_t
write_ical_file(arena *ar, const struct tree_node *node, icalcomponent *ic)
{

	// Always set last modified to the time we write
	icalproperty *prop =
	    icalcomponent_get_first_property(ic, ICAL_LASTMODIFIED_PROPERTY);
	if (!prop) {
		prop = icalproperty_new_lastmodified(get_ical_now());
		icalcomponent_add_property(ic, prop);
	}
	else {
		icalproperty_set_lastmodified(prop, get_ical_now());
	}

	icalcomponent *inner = icalcomponent_get_inner(ic);

	const char *descr_prop = icalcomponent_get_description(inner);
	if (descr_prop != NULL && strcmp("", descr_prop) == 0) {
		icalproperty *ical_descr_prop =
		    icalcomponent_get_first_property(inner,
						     ICAL_DESCRIPTION_PROPERTY);
		icalcomponent_remove_property(inner, ical_descr_prop);
	}

	char *ical_str = ricalcomponent_as_ical_string_r(ar, ic);

	int res = write_to_file(get_vdir_filepath(ar, node), ical_str);

	return res;
}

// Owner: ctx
icalcomponent *
get_icalcomponent_from_node(arena *ar, const struct tree_node *n)
{
	if (is_root_node(n)) {
		return NULL;
	}

	const char *orig_path = get_vdir_filepath(ar, n);
	// TODO: Cache intermediate results within context
	icalcomponent *ic = parse_ics_file(ar, orig_path);
	return ic;
}

int
write_parent_child_components(arena *ar, const struct tree_node *parent,
			      icalcomponent *iparent,
			      const struct tree_node *child,
			      icalcomponent *ichild)
{
	LOG("Adding parent and child relation");
	set_parent_child_relationship_to_component(iparent, ichild);
	return write_ical_file(ar, child, ichild);
}

// Writes ics component summary to the filename of the tree_node
int
update_summary(arena *ar, icalcomponent *ics, const struct tree_node *node)
{
	if (is_directory_component(ics)) {
		icalcomponent_set_summary(ics, get_node_filename(node));
	}
	else {
		char *filename_no_ext =
		    without_file_extension(ar, get_node_filename(node));
		icalcomponent_set_summary(ics, filename_no_ext);
	}

	return write_ical_file(ar, node, ics);
}

int
update_node_file_extension(arena *ar, icalcomponent *ics,
			   const struct tree_node *node)
{
	const char *ext = get_file_extension(ar, get_node_filename(node));
	if (!ext) {
		return 0;
	}
	icalcomponent_set_file_extension(ics, ext);
	return write_ical_file(ar, node, ics);
}

struct agenda_entry *
parse_ics_to_agenda_entry(arena *ar, const char *vdir_filepath)
{
	icalcomponent *component = parse_ics_file(ar, vdir_filepath);
	if (component == NULL) {
		LOG("No component");
		return NULL;
	}
	icalcomponent *journal =
	    icalcomponent_get_first_real_component(component);

	if (journal == NULL ||
	    icalcomponent_isa(journal) != ICAL_VJOURNAL_COMPONENT) {
		LOG("Not journal component");
		return NULL;
	}
	const char *summary = icalcomponent_get_summary(component);
	if (summary == NULL) {
		LOG("No summary found");
		return NULL;
	}

	char *filename = NULL;
	if (is_directory_component(component)) {
		filename = rstrdup(ar, summary);
	}
	else {
		LOG("GETTING FILE EXT");
		const char *extension =
		    icalcomponent_get_file_extension(component);
		if (!extension) {
			LOG("Has none, setting to default");
			extension = get_default_file_extension();
		}

		if (strcmp(extension, "") == 0) {
			filename = rstrdup(ar, summary);
		}
		else {
			rasprintf(ar, &filename, "%s.%s", summary, extension);
		}
	}

	struct agenda_entry *e =
	    create_agenda_entry(ar, filename, get_filename(vdir_filepath));

	return e;
}

// Owner: arena
struct agenda_entry *
load_agenda_entry_from_ics_file(arena *ar, const char *filename)
{
	path *filepath = append_path(ar, VDIR, filename);
	LOG("Filepath is %s", filepath);

	struct stat fileStat;
	if (stat(filepath, &fileStat) == -1) {
		LOG("Can not stat %s.", filepath);
		return NULL;
	}

	struct agenda_entry *new_entry =
	    parse_ics_to_agenda_entry(ar, filepath);
	if (!new_entry) {
		LOG("Could not parse entry");
		return NULL;
	}

	LOG("Parsed %s to file %s", new_entry->filename_vdir,
	    new_entry->filename);
	return new_entry;
}

void
load_root_node_tree()
{
	entries_vdir = hashmap_new(NULL);
	fuse_root = create_tree_node(NULL, NULL);
	arena *ar = create_arena();
	LOG("Loading journal entries from: %s\n", VDIR);

	DIR *dir = opendir(VDIR);
	if (!dir) {
		perror("opendir");
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir))) {
		if (entry->d_type == DT_UNKNOWN) {
			printf("Unsupported file type for '%s'. Skipping\n",
			       entry->d_name);
		}
		if (entry->d_type == DT_REG && strstr(entry->d_name, ".ics")) {
			LOG("Loading %s", entry->d_name);
			struct agenda_entry *new_entry =
			    load_agenda_entry_from_ics_file(ar, entry->d_name);
			if (new_entry) {
				create_fuse_node(new_entry);
				LOG("Inserted filename_vdir: %s, %s",
				    new_entry->filename,
				    new_entry->filename_vdir);
			}
			else {
				LOG("Skipping %s", entry->d_name);
			}
		}
	}
	closedir(dir);
	LOG("Set up directories according to parent-child");

	size_t n_keys = 0;
	char **keys = hashmap_get_keys(entries_vdir, &n_keys);
	for (size_t i = 0; i < n_keys; i++) {
		const char *vdirname = keys[i];
		struct tree_node *child =
		    get_fuse_node_from_vdir_name(vdirname);

		LOG("Parsing %s", vdirname);
		icalcomponent *ic = get_icalcomponent_from_node(ar, child);
		LOG("Success");

		const char *parent_uid = get_parent_uid(ic);
		if (parent_uid) {
			LOG("Has parent");

			struct tree_node *parent =
			    get_node_by_uuid(ar, parent_uid);
			if (parent) {
				icalcomponent *pic =
				    get_icalcomponent_from_node(ar, parent);
				if (!is_directory_component(pic)) {
					icalcomponent_mark_as_directory(pic);
					assert(write_ical_file(ar, parent,
							       pic) == 0);
				}

				move_fuse_node(parent, child);
			}
			else {
				LOG("COULD NOT FIND PARENT NODE: %s",
				    parent_uid);
			}
		}
		else {
			add_fuse_child(fuse_root, child);
		}
	}
	LOG("Inserted %zu entries", n_keys);

	hashmap_free_keys(keys, n_keys);
	free_all(ar);

	LOG("Done");
}

// Sets a validated dtstart
int
set_dtstart(arena *ar, const char *dtstart_c, const struct tree_node *node)
{
	struct icaltimetype dtstart = icaltime_from_string(dtstart_c);
	if (icaltime_compare(dtstart, icaltime_null_time()) == 0) {
		return -EINVAL;
	}

	icalcomponent *comp = get_icalcomponent_from_node(ar, node);

	icalcomponent_set_dtstart(comp, dtstart);

	return write_ical_file(ar, node, comp);
}

const char *
get_dtstart(arena *ar, struct tree_node *n)
{

	icalcomponent *comp = get_icalcomponent_from_node(ar, n);

	struct icaltimetype dtstart = icalcomponent_get_dtstart(comp);
	if (icaltime_compare(dtstart, icaltime_null_time()) == 0) {
		return NULL;
	}

	const char *time = icaltime_as_ical_string(dtstart);
	return time;
}

int
clear_dtstart(arena *ar, struct tree_node *node)
{
	icalcomponent *comp = get_icalcomponent_from_node(ar, node);

	icalproperty *p =
	    icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY);

	if (p) {
		icalcomponent_remove_property(comp, p);
	}

	return write_ical_file(ar, node, comp);
}

static int
append_category_to_memstream(FILE *memstream, const char *category,
			     bool is_first_category)
{
	int res = 0;
	if (!is_first_category) {
		res = fputs(",", memstream);
		assert(res != EOF);
	}

	res = fputs(category, memstream);
	assert(res != EOF);
	return 0;
}

// Returns comma separated list of categories for a file
char *
get_node_categories(arena *ar, const struct tree_node *node)
{
	// TODO: Take this as parameter instead...
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return NULL;
	}

	FILE *memstream = NULL;
	char *result_buffer = NULL;
	arena_register(ar, result_buffer, free);
	size_t buffer_size = 0;

	memstream = open_memstream(&result_buffer, &buffer_size);

	icalcomponent *inner = icalcomponent_get_inner(component);

	icalproperty *catp =
	    icalcomponent_get_first_property(inner, ICAL_CATEGORIES_PROPERTY);

	bool first_category = true;
	while (catp) {
		const char *category = icalproperty_get_categories(catp);
		append_category_to_memstream(memstream, category,
					     first_category);
		catp = icalcomponent_get_next_property(
		    inner, ICAL_CATEGORIES_PROPERTY);

		first_category = false;
	}

	size_t res = fclose(memstream);
	assert(res != EOF);

	return result_buffer;
}

int
delete_node_categories(arena *ar, const struct tree_node *node)
{
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent *inner = icalcomponent_get_inner(component);

	icalproperty *catp =
	    icalcomponent_get_first_property(inner, ICAL_CATEGORIES_PROPERTY);

	// Delete old categories
	while (catp) {
		icalcomponent_remove_property(inner, catp);

		catp = icalcomponent_get_next_property(
		    inner, ICAL_CATEGORIES_PROPERTY);
	}

	return write_ical_file(ar, node, component);
}

int
set_node_categories(arena *ar, const struct tree_node *node,
		    const char *new_categories, size_t s)
{
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent *inner = icalcomponent_get_inner(component);

	icalproperty *catp =
	    icalcomponent_get_first_property(inner, ICAL_CATEGORIES_PROPERTY);

	// Delete old categories
	while ((catp = icalcomponent_get_first_property(
		    inner, ICAL_CATEGORIES_PROPERTY)) != NULL) {
		icalcomponent_remove_property(inner, catp);
	}

	if (s == 0) {
		return write_ical_file(ar, node, component);
	}

	char *null_terminated_categories = rstrndup(ar, new_categories, s);

	icalproperty *new_cat_prop =
	    icalproperty_new_categories(null_terminated_categories);
	icalcomponent_add_property(inner, new_cat_prop);

	return write_ical_file(ar, node, component);
}

const char *
get_node_class(arena *ar, const struct tree_node *node)
{

	// TODO: Take this as parameter instead...
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent *inner = icalcomponent_get_inner(component);

	const icalproperty *classp =
	    icalcomponent_get_first_property(inner, ICAL_CLASS_PROPERTY);

	if (!classp) {
		return NULL;
	}

	const enum icalproperty_class classv = icalproperty_get_class(classp);
	return format_ical_class(classv);
}

const char *
get_node_status(arena *ar, const struct tree_node *node)
{

	// TODO: Take this as parameter instead...
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent *inner = icalcomponent_get_inner(component);

	const icalproperty *statusp =
	    icalcomponent_get_first_property(inner, ICAL_STATUS_PROPERTY);

	if (!statusp) {
		LOG("No status found");
		return NULL;
	}

	const enum icalproperty_status statusv =
	    icalproperty_get_status(statusp);
	return format_ical_status(statusv);
}

int
delete_node_class(arena *ar, const struct tree_node *node)
{
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}
	icalcomponent *inner = icalcomponent_get_inner(component);
	icalproperty *prop =
	    icalcomponent_get_first_property(inner, ICAL_CLASS_PROPERTY);

	icalcomponent_remove_property(inner, prop);

	return write_ical_file(ar, node, component);
}

int
set_node_class(arena *ar, const struct tree_node *node,
	       const icalproperty_class new_class)
{
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent *inner = icalcomponent_get_inner(component);

	icalproperty *classp =
	    icalcomponent_get_first_property(inner, ICAL_CLASS_PROPERTY);

	icalproperty_set_class(classp, new_class);
	return write_ical_file(ar, node, component);
}

int
set_node_status(arena *ar, const struct tree_node *node,
		const icalproperty_status new_status)
{
	icalcomponent *component = get_icalcomponent_from_node(ar, node);
	if (!component) {
		return 0;
	}

	icalcomponent_set_status(component, new_status);

	return write_ical_file(ar, node, component);
}

size_t
calculate_node_size(arena *ar, const struct tree_node *node,
		    icalcomponent *node_component)
{
	if (!node_is_directory(ar, node, node_component)) {
		return icalcomponent_get_description_size(node_component);
	}

	size_t total_size = 0;
	for (int i = 0; i < node->child_count; i++) {
		icalcomponent *cic =
		    get_icalcomponent_from_node(ar, node->children[i]);
		total_size += calculate_node_size(ar, node->children[i], cic);
	}
	return total_size;
}

struct stat
get_node_stat(arena *ar, const struct tree_node *node,
	      icalcomponent *node_component)
{
	if (is_root_node(node)) {
		struct stat st = {0};
		st.st_gid = getgid();
		st.st_uid = getuid();
		st.st_ino = 0;
		return st;
	}
	else {
		struct stat vdir_stat = {0};
		const char *vdir_path = get_vdir_filepath(ar, node);
		int res = stat(vdir_path, &vdir_stat);
		assert(res == 0);
		if (node_is_directory(ar, node, node_component)) {
			vdir_stat.st_size = (off_t)calculate_node_size(
			    ar, node, node_component);
			vdir_stat.st_mode = S_IFDIR | 0444;
			vdir_stat.st_nlink = 2;
		}
		else {
			vdir_stat.st_size =
			    (off_t)icalcomponent_get_description_size(
				node_component);

			vdir_stat.st_mode = S_IFREG | 0774;
			vdir_stat.st_nlink = 1;
		}

		return vdir_stat;
	}
}

int
insert_fuse_node_to_path(arena *ar, const char *fuse_path,
			 struct tree_node *child_node, icalcomponent *child_ics)
{
	char *parent_path = get_parent_path(ar, fuse_path);
	int status = 0;
	LOG("Parent path is %s", parent_path);

	struct tree_node *parent_node = get_node_by_path(ar, parent_path);
	if (!parent_node) {
		LOG("Parent does not exist");
		return -ENOENT;
	}

	icalcomponent *parent_ics =
	    get_icalcomponent_from_node(ar, parent_node);

	if (!node_is_directory(ar, parent_node, parent_ics)) {
		LOG("Parent is not a directory");
		return -ENOTDIR;
	}

	if (parent_ics) {
		status = write_parent_child_components(
		    ar, parent_node, parent_ics, child_node, child_ics);
	}
	else {
		write_ical_file(ar, child_node, child_ics);
	}

	add_child(parent_node, child_node);
	return status;
}

enum ENTRY_TYPE { ENTRY_DIRECTORY, ENTRY_FILE };

int
create_entry_from_fuse(arena *ar, const char *fuse_path, enum ENTRY_TYPE etype)
{
	const char *entry_prefix = "/";
	if (strncmp(fuse_path, entry_prefix, strlen(entry_prefix)) != 0) {
		return -EINVAL;
	}

	const char *new_filename = get_filename(fuse_path);
	icalcomponent *new_component = NULL;

	switch (etype) {
	case (ENTRY_DIRECTORY):
		new_component = create_vjournal_directory(ar, new_filename);
		break;
	case (ENTRY_FILE):
		new_component = create_vjournal_entry(ar, new_filename);
		break;
	}

	char *new_filname_vdir = NULL;
	rasprintf(ar, &new_filname_vdir, "%s.ics",
		  icalcomponent_get_uid(new_component));

	struct agenda_entry *new_entry =
	    create_agenda_entry(ar, new_filename, new_filname_vdir);

	LOG("Inserting vjournal directory");

	struct tree_node *new_node = create_fuse_node(new_entry);

	return insert_fuse_node_to_path(ar, fuse_path, new_node, new_component);
}

int
update_or_create_fuse_entry_from_vdir(arena *ar, const char *filepath_vdir)
{
	const char *filename_vdir = get_filename(filepath_vdir);
	LOG("Filename original is %s", filename_vdir);

	struct agenda_entry *updated_entry =
	    load_agenda_entry_from_ics_file(ar, filename_vdir);

	if (!updated_entry) {
		return -EIO;
	}

	struct tree_node *node = upsert_fuse_node(updated_entry);

	icalcomponent *entry_ics = get_icalcomponent_from_node(ar, node);

	const char *new_parent_uid = get_parent_uid(entry_ics);

	if (new_parent_uid) {
		LOG("Has parent");
		struct tree_node *new_parent =
		    get_node_by_uuid(ar, new_parent_uid);
		move_fuse_node(new_parent, node);

		icalcomponent *pic =
		    get_icalcomponent_from_node(ar, new_parent);
		if (!is_directory_component(pic)) {
			icalcomponent_mark_as_directory(pic);
			assert(write_ical_file(ar, new_parent, pic) == 0);
		}
	}
	else {
		LOG("Does not have parent, adding to root");
		move_fuse_node(fuse_root, node);
	}

	LOG("Tree updated");
	return 0;
}

int
do_agenda_rename(arena *ar, const char *old, const char *new)
{
	char *new_copy = rstrdup(ar, new);
	const char *new_filename = get_filename(new_copy);

	const char *old_filename = get_filename(old);
	if (!old_filename)
		return -EINVAL;

	struct tree_node *new_parent_node =
	    get_node_by_path(ar, get_parent_path(ar, new));

	if (new_parent_node == NULL) {
		return -EINVAL;
	}

	struct tree_node *child_node = get_node_by_path(ar, old);
	assert(child_node);

	icalcomponent *child_ics = get_icalcomponent_from_node(ar, child_node);
	assert(child_ics);

	icalcomponent *new_parent_ics =
	    get_icalcomponent_from_node(ar, new_parent_node);

	struct tree_node *old_parent_node = child_node->parent;
	icalcomponent *old_parent_ics =
	    get_icalcomponent_from_node(ar, old_parent_node);

	set_node_filename(child_node, new_filename);
	update_summary(ar, child_ics, child_node);
	update_node_file_extension(ar, child_ics, child_node);

	if (old_parent_node && old_parent_ics) {
		remove_parent_child_relationship_from_component(old_parent_ics,
								child_ics);
	}

	if (new_parent_ics) {
		move_node(new_parent_node, child_node);
		write_parent_child_components(
		    ar, new_parent_node, new_parent_ics, child_node, child_ics);
	}
	else {
		move_node(fuse_root, child_node);
	}
	return 0;
}

int
delete_vdir_entry(arena *ar, struct tree_node *node)
{
	int res = 0;

	const char *filepath_original = get_vdir_filepath(ar, node);

	LOG("Deleting file %s", filepath_original);

	res = remove(filepath_original);
	if (res == -1) {
		return -EIO;
	}

	delete_fuse_node(node);
	return res;
}

// Caller needs to ensure the node has no children.
int
delete_from_vdir_path(arena *ar, const char *filepath)
{

	struct tree_node *node = get_node_by_uuid(ar, filepath);
	if (!node) {
		return -ENOENT;
	}

	assert(node->child_count > 0);
	delete_fuse_node(node);
	return 0;
}
