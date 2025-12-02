#include "tree.h"
#include "arena.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct tree_node *
create_tree_node(void *data, void (*free_fn)(void *))
{
	struct tree_node *node = xmalloc(sizeof(struct tree_node));
	if (!node)
		return NULL;
	node->data = data;
	node->parent = NULL;
	node->children = NULL;
	node->child_count = 0;
	node->child_capacity = 0;
	node->free_fn = free_fn;

	return node;
}

struct tree_node *
rcreate_tree_node(void *data, arena *m)
{
	struct tree_node *node = rmalloc(m, sizeof(struct tree_node));
	if (!node)
		return NULL;
	node->data = data;
	node->parent = NULL;
	node->children = NULL;
	node->child_count = 0;
	node->child_capacity = 0;
	node->free_fn = NULL;

	return node;
}

size_t
add_child(struct tree_node *parent, struct tree_node *child)
{
	if (parent->child_count == parent->child_capacity) {
		size_t new_capacity =
		    parent->child_capacity ? parent->child_capacity * 2 : 4;

		parent->children = xreallocarray(parent->children, new_capacity,
						 sizeof(struct tree_node *));
		parent->child_capacity = new_capacity;
	}
	parent->children[parent->child_count++] = child;
	child->parent = parent;
	return 0;
}
void
update_node_data(struct tree_node *node, void *data)
{
	if (node->data && node->free_fn) {
		node->free_fn(node->data);
	}
	node->data = data;
}

bool
node_has_children(const struct tree_node *node)
{
	return node->child_count > 0;
}

// Free a node and all its children
void
free_tree(struct tree_node *node)
{
	if (!node)
		return;
	for (size_t i = 0; i < node->child_count; ++i) {
		free_tree(node->children[i]);
	}
	if (node->free_fn && node->data) {
		node->free_fn(node->data);
	}
	free(node->children);
	free(node);
}

void
print_tree(struct tree_node *node, int depth, void (*print_data)(void *))
{
	for (int i = 0; i < depth; i++)
		printf("  ");
	if (print_data)
		print_data(node->data);
	else
		printf("(no data)\n");

	for (size_t i = 0; i < node->child_count; ++i) {
		print_tree(node->children[i], depth + 1, print_data);
	}
}

bool
detach_tree_node(struct tree_node *node)
{
	if (!node->parent)
		return false;

	struct tree_node *parent = node->parent;

	// Find index of this node in parent->children
	size_t index = (size_t)-1;
	for (size_t i = 0; i < parent->child_count; ++i) {
		if (parent->children[i] == node) {
			index = i;
			break;
		}
	}

	// Shift remaining children to fill the gap
	for (size_t i = index + 1; i < parent->child_count; ++i) {
		parent->children[i - 1] = parent->children[i];
	}
	parent->child_count--;

	node->parent = NULL;
	return true;
}

int
move_node(struct tree_node *new_parent, struct tree_node *child)
{
	detach_tree_node(child);
	return add_child(new_parent, child);
}
