#ifndef tree_h
#define tree_h

#include "arena.h"
#include <stdbool.h>
#include <stdlib.h>

struct tree_node {
	void *data;

	struct tree_node *parent;
	struct tree_node **children;
	size_t child_count;
	size_t child_capacity;

	void (*free_fn)(void *);
};

struct tree_node *
create_tree_node(void *data, void (*free_fn)(void *));

struct tree_node *
rcreate_tree_node(void *data, arena *m);

bool
node_has_children(const struct tree_node *node);

int
move_node(struct tree_node *new_parent, struct tree_node *child);

void
update_node_data(struct tree_node *node, void *data);

// detach from parent
bool
detach_tree_node(struct tree_node *node);

size_t
add_child(struct tree_node *parent, struct tree_node *child);

void
free_tree(struct tree_node *node);

void
print_tree(struct tree_node *node, int depth, void (*print_data)(void *));

#endif

