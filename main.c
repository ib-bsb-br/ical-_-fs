#define FUSE_USE_VERSION 31

#include "fuse_node.h"
#include "fuse_node_store.h"
#include "ical_extra.h"
#include "arena.h"
#include "tree.h"
#include "util.h"
#include <dirent.h>
#include <errno.h>
#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>
#include <fuse3/fuse_opt.h>
#include <libical/ical.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <wordexp.h>

// Listening to file changes of the original content
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))

// Between the inotify event listener and the fuse filesystem
static pthread_rwlock_t entries_lock = PTHREAD_RWLOCK_INITIALIZER;

#define FUSE_WRITE_BEGIN                                                       \
	LOG("%s", __func__);                                                   \
	pthread_rwlock_wrlock(&entries_lock);                                  \
	arena *ar = create_arena();                                 \
	int status = 0;                                                        \
	if (!ar) {                                                           \
		pthread_rwlock_unlock(&entries_lock);                          \
		return -ENOMEM;                                                \
	}

#define FUSE_READ_BEGIN                                                        \
	LOG("%s", __func__);                                                   \
	pthread_rwlock_rdlock(&entries_lock);                                  \
	arena *ar = create_arena();                                 \
	int status = 0;                                                        \
	if (!ar) {                                                           \
		pthread_rwlock_unlock(&entries_lock);                          \
		return -ENOMEM;                                                \
	}

#define FUSE_CLEANUP                                                           \
	pthread_rwlock_unlock(&entries_lock);                                  \
	free_all(ar);

static int
fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
	FUSE_READ_BEGIN;
	LOG("%s", path);

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 2;

		// TODO: Probably should be set with an option?
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		stbuf->st_ino = 0;
		goto cleanup_return;
	}

	const struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		LOG("Entry not found");
		status = -ENOENT;
		goto cleanup_return;
	}

	icalcomponent *ic = get_icalcomponent_from_node(ar, node);

	struct stat st = get_node_stat(ar, node, ic);

	*stbuf = st;

	stbuf->st_atime = time(NULL);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_readlink(const char *path, char *buf, size_t size)
{
	// TODO: Implement
	LOG("READLINK path: %s", path);
	return -ENOENT;
}

static int
fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	     struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{

	FUSE_READ_BEGIN;
	LOG("%s", path);

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	const struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENOENT;
		goto cleanup_return;
	}

	for (size_t i = 0; i < node->child_count; i++) {
		const struct tree_node *child = node->children[i];
		icalcomponent *ic = get_icalcomponent_from_node(ar, child);
		assert(ic);

		struct stat st = get_node_stat(ar, node, ic);

		if (filler(buf, get_node_filename(child), &st, 0, 0) != 0) {
			status = -ENOMEM;
			goto cleanup_return;
		}
	}

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_open(const char *path, struct fuse_file_info *fi)
{
	FUSE_READ_BEGIN;
	LOG("%s", path);
	const struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENOENT;
	}

	FUSE_CLEANUP;
	return status;
}

static int
fuse_mkdir(const char *filepath, mode_t mode)
{
	FUSE_WRITE_BEGIN;

	// Reserved
	if (pathIsHidden(filepath)) {
		status = -EPERM;
		goto cleanup_return;
	}

	struct tree_node *node = get_node_by_path(ar, filepath);
	if (node) {
		status = -EEXIST;
		goto cleanup_return;
	}

	status = create_entry_from_fuse(ar, filepath, ENTRY_DIRECTORY);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_read(const char *path, char *buf, size_t size, off_t offset,
	  struct fuse_file_info *fi)
{
	FUSE_READ_BEGIN;

	LOG("READ: offset=%ld, size=%zu", offset, size);

	const struct tree_node *n = get_node_by_path(ar, path);
	if (!n) {
		status = -ENOENT;
		goto cleanup_return;
	}
	icalcomponent *ic = get_icalcomponent_from_node(ar, n);
	if (!ic) {
		status = -EIO;
		goto cleanup_return;
	}

	const char *description = icalcomponent_get_description(ic);
	if (!description) {
		goto cleanup_return;
	}

	size_t content_len = strlen(description);

	if (offset >= content_len) {
		goto cleanup_return;
	}

	size_t bytes_to_read = content_len - offset;
	if (bytes_to_read > size) {
		bytes_to_read = size;
	}
	if (bytes_to_read > 0) {
		memcpy(buf, description + offset, bytes_to_read);
	}

	status = bytes_to_read;

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_write(const char *path, const char *buf, size_t size, off_t offset,
	   struct fuse_file_info *fi)
{

	FUSE_WRITE_BEGIN;
	LOG("%s, %zu, %zu", buf, size, offset);

	if (pathIsHidden(path)) {
		status = -EPERM;
		goto cleanup_return;
	}

	const struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENONET;
		goto cleanup_return;
	}

	icalcomponent *ic = get_icalcomponent_from_node(ar, node);

	// It possible to write to "/". The rest have an icalcomponent
	assert(ic);

	icalcomponent_insert_description(ar, ic, buf, size, offset);

	status = write_ical_file(ar, node, ic) >= 0 ? size : status;

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

// Aka remove or delete
static int
fuse_unlink(const char *file)
{
	FUSE_WRITE_BEGIN;
	LOG("%s", file);

	struct tree_node *node = get_node_by_path(ar, file);
	if (!node) {
		status = -ENOENT;
		goto cleanup_return;
	}

	icalcomponent *node_ics = get_icalcomponent_from_node(ar, node);
	if (node_is_directory(ar, node, node_ics)) {
		status = -EISDIR;
		goto cleanup_return;
	}

	status = delete_vdir_entry(ar, node);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_rmdir(const char *filepath)
{
	FUSE_WRITE_BEGIN;
	LOG("%s", filepath);

	struct tree_node *node = get_node_by_path(ar, filepath);
	if (!node) {
		status = -ENOENT;
		goto cleanup_return;
	}

	icalcomponent *node_ics = get_icalcomponent_from_node(ar, node);
	if (!node_is_directory(ar, node, node_ics)) {
		status = -ENOTDIR;
		goto cleanup_return;
	}

	if (node->child_count > 0) {
		status = -ENOTEMPTY;
		goto cleanup_return;
	}

	status = delete_vdir_entry(ar, node);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_rename(const char *old, const char *new, unsigned int flags)
{

	FUSE_WRITE_BEGIN;
	LOG("%s -> %s", old, new);

	struct tree_node *existing_node = get_node_by_path(ar, new);
	if (existing_node) {
		if (node_has_children(existing_node)) {
			status = -ENOTEMPTY;
			goto cleanup_return;
		}
		if (delete_vdir_entry(ar, existing_node) != 0) {
			status = -EIO;
			goto cleanup_return;
		}
	}

	status = do_agenda_rename(ar, old, new);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_create(const char *filepath, mode_t mode, struct fuse_file_info *info)
{

	FUSE_WRITE_BEGIN;
	LOG("%s", filepath);

	// Reserved
	if (pathIsHidden(filepath)) {
		status = -EPERM;
		goto cleanup_return;
	}

	struct tree_node *node = get_node_by_path(ar, filepath);
	if (node) {
		status = -EEXIST;
		goto cleanup_return;
	}

	// TODO: Handle mode and fuse_file_info
	status = create_entry_from_fuse(ar, filepath, ENTRY_FILE);

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_removexattr(const char *path, const char *attribute)
{
	FUSE_WRITE_BEGIN;
	LOG("'%s' '%s'", path, attribute);

	struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENONET;
		goto cleanup_return;
	}

	if (strcmp(attribute, "user.categories") == 0) {
		status = set_node_categories(ar, node, "", 0);
		goto cleanup_return;
	}
	else if (strcmp(attribute, "user.class") == 0) {
		status = delete_node_class(ar, node);
		goto cleanup_return;
	}

	// It should probably always be set
	else if (strcmp(attribute, "user.status") == 0) {
		status = -EPERM;
		goto cleanup_return;
	}
	if (strcmp(attribute, "user.dtstart") == 0) {
		status = clear_dtstart(ar, node);
		goto cleanup_return;
	}
	else if (starts_with_str(attribute, "user.")) {
		icalcomponent *ic = get_icalcomponent_from_node(ar, node);

		const char *key = attribute + 5;
		icalcomponent_remove_custom_prop(ar, ic, key);
		status = write_ical_file(ar, node, ic);
		goto cleanup_return;
	}
	else {
		status = -EINVAL;
		goto cleanup_return;
	}

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_setxattr(const char *path, const char *attribute, const char *value,
	      size_t s, int flags)
{

	FUSE_WRITE_BEGIN;
	LOG("'%s' '%s' '%zu' '%s' '%d'", path, attribute, s, value, flags);

	// Limit documented in xattr(7)
	if (strlen(attribute) >= 256) {
		status = -EMSGSIZE;
		goto cleanup_return;
	}

	// Limit documented in xattr(7)
	if (strlen(value) >= 64 * 1024) {
		status = -EMSGSIZE;
		goto cleanup_return;
	}

	struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENONET;
		goto cleanup_return;
	}

	// uid is set by the ics file and is immutable
	if (strcmp(attribute, "user.uid") == 0) {
		status = -EPERM;
		goto cleanup_return;
	}

	// user.sibling is reserved but not implemented
	else if (strcmp(attribute, "user.sibling") == 0) {
		status = -EPERM;
		goto cleanup_return;
	}

	else if (strcmp(attribute, "user.dtstart") == 0) {
		if (strcmp(value, "") == 0) {
			status = clear_dtstart(ar, node);
		}
		else {
			status = set_dtstart(ar, value, node);
		}
		goto cleanup_return;
	}

	else if (strcmp(attribute, "user.categories") == 0) {
		LOG("Updating categories");
		status = set_node_categories(ar, node, value, s);
		goto cleanup_return;
	}

	else if (strcmp(attribute, "user.class") == 0) {
		LOG("Updating class");
		enum icalproperty_class iclass = parse_ical_class(value);

		if (iclass == ICAL_CLASS_NONE) {
			LOG("INVALID CLASS %s", value);
			status = -EINVAL;
			goto cleanup_return;
		}

		status = set_node_class(ar, node, iclass);
		goto cleanup_return;
	}
	else if (strcmp(attribute, "user.status") == 0) {
		LOG("Updating status");
		enum icalproperty_status istatus = parse_ical_status(value);

		if (istatus == ICAL_STATUS_NONE) {
			LOG("INVALID STATUS");
			status = -EINVAL;
			goto cleanup_return;
		}

		status = set_node_status(ar, node, istatus);
		goto cleanup_return;
	}
	else if (starts_with_str(attribute, "user.")) {
		icalcomponent *ic = get_icalcomponent_from_node(ar, node);

		const char *rkey = attribute + 5;
		LOG("Updating %s", rkey);

		icalcomponent_set_custom_x_value(ar, ic, rkey, value);
		status = write_ical_file(ar, node, ic);
		goto cleanup_return;
	}
	else {
		status = -EINVAL;
		goto cleanup_return;
	}

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_listxattr(const char *path, char *list, size_t size)
{
	FUSE_READ_BEGIN;
	LOG("%s %zu", list, size);

	FILE *stream;
	char *xattribute_list = NULL;
	size_t xattr_list_len = 0;

	struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENONET;
		goto cleanup_return_2;
	}

	if (is_root_node(node)) {
		status = -EPERM;
		goto cleanup_return_2;
	}

	icalcomponent *comp = get_icalcomponent_from_node(ar, node);
	assert(comp);

	stream = open_memstream(&xattribute_list, &xattr_list_len);
	if (stream == NULL) {
		status = -EIO;
		goto cleanup_return_2;
	}

	char *cats = get_node_categories(ar, node);
	if (cats != NULL && strcmp("", cats) != 0) {
		fprintf(stream, "user.categories");
		fputc('\0', stream);
	}

	const char *dtstart = get_dtstart(ar, node);
	if (dtstart != NULL) {
		fprintf(stream, "user.dtstart");
		fputc('\0', stream);
	}

	// All notes have a uid
	fprintf(stream, "user.uid");
	fputc('\0', stream);

	LOG("Checking node class");
	const char *iclass = get_node_class(ar, node);
	if (iclass != NULL && strcmp("", iclass) != 0) {
		fprintf(stream, "user.class");
		fputc('\0', stream);
	}

	LOG("Checking node status");
	const char *istatus = get_node_status(ar, node);
	if (istatus != NULL && strcmp("", istatus) != 0) {
		fprintf(stream, "user.status");
		fputc('\0', stream);
	}

	icalcomponent_print_x_props(stream, comp);

	if (fclose(stream) != 0) {
		status = -EIO;
		goto cleanup_return;
	}

	if (size > 0) {
		if (xattr_list_len > size) {
			memcpy(list, xattribute_list, xattr_list_len);
		}
		else {
			memcpy(list, xattribute_list, size);
		}
	}
	status = xattr_list_len;

cleanup_return:
	free(xattribute_list);
cleanup_return_2:
	FUSE_CLEANUP;
	return status;
}

static int
fuse_getxattr(const char *path, const char *attribute, char *buf, size_t s)
{
	FUSE_READ_BEGIN;
	LOG("'%s' '%s' '%zu'\n", path, attribute, s);

	struct tree_node *node = get_node_by_path(ar, path);
	if (!node) {
		status = -ENONET;
		goto cleanup_return;
	}

	if (strcmp(attribute, "user.categories") == 0) {

		const char *cats = get_node_categories(ar, node);

		if (!cats || *cats == '\0') {
			status = -ENODATA;
			goto cleanup_return;
		}

		int string_len = strlen(cats);

		// If s == 0 we don't need to write, just return size to
		// allocate
		if (s > 0) {
			snprintf(buf, s, "%s", cats);
		}
		status = string_len;
		goto cleanup_return;
	}
	if (strcmp(attribute, "user.uid") == 0) {
		icalcomponent *ic = get_icalcomponent_from_node(ar, node);
		const char *uid = icalcomponent_get_uid(ic);
		assert(uid);

		int string_len = strlen(uid);

		// If s == 0 we don't need to write, just return size to
		// allocate
		if (s > 0) {
			snprintf(buf, s, "%s", uid);
		}

		status = string_len;
		goto cleanup_return;
	}
	else if (strcmp(attribute, "user.class") == 0) {
		const char *c = get_node_class(ar, node);
		if (!c || *c == '\0') {
			status = -ENODATA;
			goto cleanup_return;
		}

		int string_len = strlen(c);

		if (s > 0) {
			snprintf(buf, s, "%s", c);
		}

		status = string_len;
		goto cleanup_return;
	}
	else if (strcmp(attribute, "user.status") == 0) {
		const char *c = get_node_status(ar, node);
		if (!c || *c == '\0') {
			status = -ENODATA;
			goto cleanup_return;
		}

		int string_len = strlen(c);

		if (s > 0) {
			snprintf(buf, s, "%s", c);
		}

		status = string_len;
		goto cleanup_return;
	}
	else if (strcmp(attribute, "user.dtstart") == 0) {
		const char *res = get_dtstart(ar, node);
		if (!res) {
			status = -ENODATA;
			goto cleanup_return;
		}

		int string_len = strlen(res);

		// If s == 0 we don't need to write, just return size to
		// allocate
		if (s > 0) {
			snprintf(buf, s, "%s", res);
		}

		status = string_len;
		goto cleanup_return;
	}
	else if (starts_with_str(attribute, "user.")) {
		icalcomponent *ic = get_icalcomponent_from_node(ar, node);

		const char *key = attribute + 5;
		const char *value =
		    icalcomponent_get_custom_x_value(ar, ic, key);
		if (!value) {
			status = -ENODATA;
			goto cleanup_return;
		}

		int string_len = strlen(value);

		// If s == 0 we don't need to write, just return size to
		// allocate
		if (s > 0) {
			snprintf(buf, s, "%s", value);
		}

		status = string_len;
		goto cleanup_return;
	}
	else {
		status = -ENODATA;
		goto cleanup_return;
	}

cleanup_return:
	FUSE_CLEANUP;
	return status;
}

static void
handle_vdir_event(struct inotify_event *event)
{
	if (!event->len || !strstr(event->name, ".ics"))
		return;

	arena *ar = create_arena();
	pthread_rwlock_wrlock(&entries_lock);

	LOG("Detected change in ICS file: %s\n", event->name);

	char *full_path = NULL;
	rasprintf(ar, &full_path, "%s/%s", VDIR, event->name);

	if (event->mask & IN_DELETE) {
		LOG("IN_DELETE HOOK");
		delete_from_vdir_path(ar, full_path);
	}
	else if (event->mask & IN_MODIFY) {
		LOG("IN_MODIFY HOOK");
		update_or_create_fuse_entry_from_vdir(ar, full_path);
	}
	else if (event->mask & IN_CREATE) {
		LOG("IN_CREATE HOOK");
		update_or_create_fuse_entry_from_vdir(ar, full_path);
	}

	pthread_rwlock_unlock(&entries_lock);
	free_all(ar);
}

static void *
watch_vdir_changes(void *arg)
{
	int fd = inotify_init1(IN_NONBLOCK);
	assert(fd >= 0);

	int wd = inotify_add_watch(fd, VDIR, IN_CREATE | IN_MODIFY | IN_DELETE);
	assert(wd >= 0);

	char buffer[EVENT_BUF_LEN];

	while (1) {
		ssize_t length = read(fd, buffer, EVENT_BUF_LEN);

		if (length < 0) {
			// No event, sleep and check again
			assert(errno == EAGAIN || errno == EINTR);
			usleep(100000);
			continue;
		}

		size_t i = 0;
		while (i < length) {
			struct inotify_event *event =
			    (struct inotify_event *)&buffer[i];
			handle_vdir_event(event);
			size_t event_size =
			    sizeof(struct inotify_event) + event->len;

			i += event_size;
		}
	}

	inotify_rm_watch(fd, wd);
	close(fd);
	return NULL;
}

// Ignore for now
static int
fuse_utimens(const char *path, const struct timespec tv[2],
	     struct fuse_file_info *info)
{
	return 0;
}

struct agendafs_config {
	char *ics_directory;
	char *default_file_extension;
};
enum {
	KEY_HELP,
	KEY_VERSION,
};

#define CUSTOMFS_OPT(t, p, v) {t, offsetof(struct agendafs_config, p), v}

static struct fuse_opt agendafs_opts[] = {
    CUSTOMFS_OPT("ext=%s", default_file_extension, 0),
    CUSTOMFS_OPT("vdir=%s", ics_directory, 0),
    FUSE_OPT_KEY("-V", KEY_VERSION),
    FUSE_OPT_KEY("--version", KEY_VERSION),
    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_END};

size_t
load_agendafs_environment(char *vdir_env)
{
	wordexp_t expanded;
	if (wordexp(vdir_env, &expanded, 0) != 0 || expanded.we_wordc == 0) {
		fprintf(stderr, "Failed to expand CALDAVFS_ICS_DIR\n");
		wordfree(&expanded);
		return -1;
	}

	char *expanded_path = expanded.we_wordv[0];

	struct stat s;
	if (stat(expanded_path, &s) == 0 && S_ISDIR(s.st_mode)) {
		assert(strlen(expanded_path) < 256);
		set_vdir(expanded_path);
		wordfree(&expanded);
		return 0;
	}

	fprintf(stderr, "CALDAVFS_ICS_DIR is not a directory: %s\n",
		expanded_path);
	return -1;
}

struct fuse_operations fuse_oper = {.getattr = fuse_getattr,
				    .readdir = fuse_readdir,
				    .open = fuse_open,
				    .read = fuse_read,
				    .utimens = fuse_utimens,
				    .write = fuse_write,
				    .create = fuse_create,
				    .mkdir = fuse_mkdir,
				    .unlink = fuse_unlink,
				    .getxattr = fuse_getxattr,
				    .setxattr = fuse_setxattr,
				    .listxattr = fuse_listxattr,
				    .removexattr = fuse_removexattr,
				    .rmdir = fuse_rmdir,
				    .readlink = fuse_readlink,
				    .rename = fuse_rename};

static int
agendafs_opt_proc(void *data, const char *arg, int key,
		  struct fuse_args *outargs)
{
	switch (key) {
	case KEY_HELP:
		fprintf(stderr,
			"usage: %s mountpoint [options]\n"
			"\n"
			"agendafs options:\n"
			"    -o vdir=STRING\n",
			outargs->argv[0]);
		fuse_opt_add_arg(outargs, "-h");
		fuse_main(outargs->argc, outargs->argv, &fuse_oper, NULL);
		exit(1);

	case KEY_VERSION:
		fprintf(stderr, "agendafs version %s\n", "v0.01 ALPHA");
		fuse_opt_add_arg(outargs, "--version");
		fuse_main(outargs->argc, outargs->argv, &fuse_oper, NULL);
		exit(0);
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	pthread_t watcher_thread;

	tzset();

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct agendafs_config conf = {0};

	fuse_opt_parse(&args, &conf, agendafs_opts, agendafs_opt_proc);

	if (!conf.ics_directory) {
		fprintf(stderr,
			"usage: %s [options] mountpoint\n"
			"\n"
			"agendafs options:\n"
			"    -o vdir=STRING\n\n",
			argv[0]);
		fuse_opt_add_arg(&args, "-h");
		fuse_main(args.argc, args.argv, &fuse_oper, NULL);
		exit(1);
	}

	if (conf.default_file_extension) {
		LOG("DEFAULT FILE EXTENSION SET");
		int res = set_file_extension(conf.default_file_extension);
		if (res == -1) {
			fprintf(stderr,
				"File extension should be without dot\n");
			exit(1);
		}

		if (res == -2) {
			fprintf(stderr, "File extension can not be more than "
					"256 characters\n");
			exit(1);
		}
	}
	else {
		LOG("DEFAULT FILE EXTENSION NOT SET. DEFAULTING TO .txt");
	}

#ifdef DEBUG
	fuse_opt_add_arg(&args, "-f");
#endif

	if (load_agendafs_environment(conf.ics_directory) != 0) {
		fprintf(stderr, "Failed to load ICS directory\n");
		exit(1);
	}

	LOG("LOADED ICS DIR");

	load_root_node_tree();

	if (pthread_create(&watcher_thread, NULL, watch_vdir_changes, NULL) !=
	    0) {
		perror("Failed to create inotify watcher thread");
		return 1;
	}

	int ret = fuse_main(args.argc, args.argv, &fuse_oper, NULL);
	LOG("Cleaning up");

	pthread_cancel(watcher_thread);
	pthread_join(watcher_thread, NULL);
	LOG("Pthread freed");

	hashmap_free(entries_vdir);
	free_tree(fuse_root);
	LOG("Hashmap and tree freed");

	exit(ret);
}
