/* tom_spatial.h: Spatial index data structure for AABBs (R*-Tree)
 * version: TBA
 *
 * Copyright (C) 2026 Thomas Oltmann
 * 
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, HETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef _TOM_SPATIAL_H_
#define _TOM_SPATIAL_H_

spatial_create();
spatial_destroy();

spatial_search();
spatial_insert();
spatial_delete();

#endif

#ifdef SPATIAL_IMPLEMENTATION

#include <stdbool.h>
#include <stdlib.h>

#define SPATIAL_DIMENSION 3

// TODO tune this value
#define SPATIAL_FANOUT 16

#define SPATIAL_LEAF_LEVEL 0

struct spatial_rect {
	float min[SPATIAL_DIMENSION];
	float max[SPATIAL_DIMENSION];
};

struct spatial_node {
	struct spatial_rect rect[SPATIAL_FANOUT];
	void *pointers[SPATIAL_FANOUT];
	unsigned count;
};

struct spatial {
	struct spatial_node *root;
	int height;
};

static bool
spatial_rect_overlaps(struct spatial_rect rect1, struct spatial_rect rect2)
{
	for (unsigned a = 0; a < SPATIAL_DIMENSION; a++) {
		if (rect1.max < rect2.min) return false;
		if (rect2.max < rect1.min) return false;
	}
	return true;
}

static struct spatial_rect
spatial_rect_intersection(struct spatial_rect rect1, struct spatial_rect rect2)
{
	struct spatial_rect rect_out;
	for (unsigned a = 0; a < SPATIAL_DIMENSION; a++) {
		rect_out.min[a] = MAX(rect1.min[a], rect2.min[a]);
		rect_out.max[a] = MIN(rect1.max[a], rect2.max[a]);
	}
	return rect_out;
}

static struct spatial_node *
spatial_alloc_node(void)
{
	return calloc(1, sizeof (struct spatial_node));
}

static struct spatial_node *
spatial_choose_subtree(struct spatial *tree)
{
	struct spatial_node *node = tree->root;
	int level = tree->height - 1;
	
	while (level != SPATIAL_LEAF_LEVEL) {
		if (level == SPATIAL_LEAF_LEVEL + 1) {
			// Determine the minimum overlap cost
		} else {
			// Determine the minimum area cost
		}

		node = child;
	}

	return node;
}

static int
spatial_choose_split_axis()
{
	float margin_value_sum[SPATIAL_DIMENSION] = { 0 };
	for (int axis = 0; axis < SPATIAL_DIMENSION; axis++) {
		float area_value;
		float margin_value;

		margin_value_sum[axis] += margin_value;
	}
	
	// Choose the axis with the minimum margin-value sum as split axis
	int chosen = 0;
	for (int axis = 1; axis < SPATIAL_DIMENSION; axis++) {
		if (margin_value_sum[axis] < margin_value_sum[chosen]) {
			chosen = axis;
		}
	}
	return chosen;
}

static unsigned
spatial_choose_split_index(int axis)
{
	unsigned min_index = 0;
	float min_overlap_value = 0.0f;
	for (unsigned i = 0; i < count; i++) {
		float overlap_value = 0.0f;

		// TODO break ties with minimum area value

		if (overlap_value < min_overlap_value) {
			min_index = i;
			min_overlap_value = overlap_value;
		}
	}
}

static
spatial_split()
{
	int axis = spatial_choose_split_axis();
	spatial_choose_split_index(axis);
}

static struct spatial_node *
spatial_insert_rec(struct spatial_node *node, int level)
{
	struct spatial_node *subtree = spatial_choose_subtree();

	if (level == SPATIAL_LEAF_LEVEL) {
	} else {
		if (node->count < SPATIAL_FANOUT) {
		}
	}
}

static
spatial_insert_data()
{
}

static
spatial_insert(int level)
{
	struct spatial_node *node = spatial_choose_subtree(level);

	if (node->count < SPATIAL_FANOUT) {
		// TODO
		return;
	}

	if (node->count == SPATIAL_FANOUT) {
		spatial_treat_overflow(level);
	}

	// TODO Propagate overflow treatment upward
	// TODO Propagate minimum bounding boxes changes
}

static
spatial_treat_overflow(int level, uint64_t *overflowed)
{
	if (level != root_level &&
		((*overflowed >> level) & 1u) == 0) {
		spatial_reinsert();
	} else {
		spatial_split();
	}
	*overflowed |= 1u << level;
}

static
spatial_reinsert()
{
	// Compute distances between the centers of the rectangles of
	// entries and the center of the bounding rectangle of the node
	
	// Sort the entries in decreasing order of their distances
	
	// Remove the first p entries from the node and adjust the bounding rectangle
	
	// In the sort: Start with the minimum distance;
	// Invoke insert to reinsert the entrie
}

#if 0
static
spatial_search_rec(const struct spatial_node *node, struct spatial_rect window)
{
	if (spatial_is_leaf(node)) {
		for (unsigned i = 0; i < node->count; i++) {
			if (spatial_rect_overlaps(node->rects[i], window)) {
				report(node->pointers[i]);
			}
		}
	} else {
		for (unsigned i = 0; i < node->count; i++) {
			if (spatial_rect_overlaps(node->rects[i], window)) {
				struct spatial_rect sub_window = spatial_rect_intersection(node->rects[i], window);
				spatial_search_rec(node->pointers[i], sub_window);
			}
		}
	}
}

spatial_search(const struct spatial *tree, struct spatial_rect window)
{
	spatial_search_rec(tree->root, window);
}

static
spatial_sweep(const struct spatial_rect *rects, unsigned num_rects, unsigned axis, float min, unsigned fill_factor)
{
	struct spatial_sweep_event {
		unsigned index;
		float    pos;
		bool     is_min;
	};

	struct spatial_sweep_event events[2 * (SPATIAL_FANOUT + 1)];
	for (unsigned i = 0; i < num_rects; i++) {
		events[2*i+0].index  = i;
		events[2*i+0].pos    = rects[i].min[axis];
		events[2*i+0].is_min = true;

		events[2*i+1].index  = i;
		events[2*i+1].pos    = rects[i].max[axis];
		events[2*i+1].is_min = false;
	}

	// TODO sort events
	
	unsigned active[SPATIAL_FANOUT + 1];
	unsigned num_active = 0;
	for (unsigned e = 0; e < 2 * num_rects; e++) {
		if (events[e].is_min) {
			active[num_active++] = events[e].index;
		} else {
			for (unsigned a = 0; a < num_active; a++) {
				if (active[a] == events[e].index) {
					active[a] = active[--num_active];
				}
			}
		}
	}


}

static void
spatial_partition(const struct spatial_rect *rects, unsigned count, unsigned fill_factor)
{
	// No partition required?
	if (count < fill_factor) {
		// TODO
		return;
	}

	// Compute minimum
	float min[SPATIAL_DIMENSION];
	for (unsigned a = 0; a < SPATIAL_DIMENSION; a++) {
		min[a] = INFINITY;
	}
	for (unsigned i = 0; i < count; i++) {
		for (unsigned a = 0; a < SPATIAL_DIMENSION; a++) {
			min[a] = SPATIAL_MIN(min[a], rects[i].min[a]);
		}
	}

	// Sweep along each axis
	for (unsigned a = 0; a < SPATIAL_DIMENSION; a++) {
		spatial_sweep();
	}

	// Choose a partition point

}

static struct spatial_node *
spatial_split_node(struct spatial_node *node)
{
	// Find a partition
	unsigned axis;
	float threshold;
	spatial_partition(node);

	// Create sibling node
	struct spatial_node *sibling = spatial_alloc_node();

	// Re-distribute child nodes
	unsigned r = 0, w = 0;
	while (r < node->count) {
		r += 1;
	}
	node->count = w;
	
	return sibling;
}

/**
 * Recursively insert a new object into a node.
 *
 * If the node is split as a result, returns the newly-created sibling node.
 * Returns NULL otherwise.
 */
struct spatial_node *
spatial_insert_rec(struct spatial_node *node, struct spatial_rect ins_rect, void *ins_pointer)
{
	if (spatial_is_leaf(node)) {
		unsigned idx = node->count;
		node->rects[idx] = ins_rect;
		node->pointers[idx] = ins_pointer;
		node->count += 1;
		if (node->count == SPATIAL_FANOUT) {
			struct spatial_node *sibling = spatial_split_node(node);
			return sibling;
		}
		return NULL;
	} else {
		for (unsigned i = 0; i < node->count; i++) {
			if (spatial_rect_overlaps(node->rects[i], window)) {
				struct spatial_node *sibling = spatial_insert_rec(node->pointers[i], ins_rect, ins_pointer);
			}
		}
	}
}

void
spatial_insert(struct spatial *tree, struct spatial_rect ins_rect, void *ins_pointer)
{
	spatial_insert_rec(tree->root, ins_rect, ins_pointer);
}

static
spatial_delete_rec(struct spatial_node *node, struct spatial_rect del_rect, void *del_pointer)
{
	if (spatial_is_leaf(node)) {
		bool changed = false;
		for (unsigned i = 0; i < node->count; i++) {
			if (node->pointers[i] == del_pointer) {
				node->count -= 1;
				node->rects[i] = node->rects[node->count];
				node->pointers[i] = node->pointers[node->count];
				changed = true;
			}
		}
		// TODO percolate up the information wether we have changed, adjust AABBs accordingly
	} else {
		for (unsigned i = 0; i < node->count; i++) {
			if (spatial_rect_overlaps(node->rects[i], del_rect)) {
				spatial_delete_rec(node->pointers[i], del_rect, del_pointer);
			}
		}
		// Unclear wether we should percolate here or not ...
	}
}

spatial_delete(struct spatial *tree, struct spatial_rect del_rect, void *del_pointer)
{
	spatial_delete_rec(node, del_rect, del_pointer);
}
#endif

#endif
