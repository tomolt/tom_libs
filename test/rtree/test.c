#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <math.h> // for INFINITY

#define DH_IMPLEMENT_HERE
#include "dh_cuts.h"

#define RTREE_IMPLEMENTATION
#include "../../tom_rtree.h"

static void
test_box_operations(void)
{
	dh_push("Box Operations");

	struct rtree_box boxA = {
		.min = { -1.0f, -1.0f, -1.0f },
		.max = {  1.0f,  1.0f,  1.0f },
	};
	struct rtree_box boxB = {
		.min = {  0.0f,  0.0f,  0.0f },
		.max = {  2.0f,  2.0f,  2.0f },
	};
	struct rtree_box boxC = {
		.min = {  1.0f,  1.0f,  1.0f },
		.max = {  2.0f,  2.0f,  2.0f },
	};

	dh_push("overlap");
	dh_assert(rtree_boxes_overlap(boxA, boxB) == true);
	dh_assert(rtree_boxes_overlap(boxB, boxA) == true);
	dh_assert(rtree_boxes_overlap(boxA, boxC) == false);
	dh_assert(rtree_boxes_overlap(boxC, boxA) == false);
	dh_pop();

	dh_push("union");
	struct rtree_box boxU = rtree_box_union(boxA, boxB);
	for (unsigned axis = 0; axis < 3; axis++) {
		dh_assert(boxU.min[axis] == -1.0f);
		dh_assert(boxU.max[axis] ==  2.0f);
	}
	boxU = rtree_box_union(boxB, boxA);
	for (unsigned axis = 0; axis < 3; axis++) {
		dh_assert(boxU.min[axis] == -1.0f);
		dh_assert(boxU.max[axis] ==  2.0f);
	}
	dh_pop();

	dh_push("intersection");
	struct rtree_box boxI = rtree_box_intersection(boxA, boxB);
	for (unsigned axis = 0; axis < 3; axis++) {
		dh_assert(boxI.min[axis] == 0.0f);
		dh_assert(boxI.max[axis] == 1.0f);
	}
	boxI = rtree_box_intersection(boxB, boxA);
	for (unsigned axis = 0; axis < 3; axis++) {
		dh_assert(boxI.min[axis] == 0.0f);
		dh_assert(boxI.max[axis] == 1.0f);
	}
	dh_pop();

	dh_push("margin");
	dh_assert(rtree_box_margin(boxA) == 6.0f);
	dh_assert(rtree_box_margin(boxB) == 6.0f);
	dh_assert(rtree_box_margin(boxC) == 3.0f);
	dh_pop();

	dh_push("area");
	dh_assert(rtree_box_area(boxA) == 8.0f);
	dh_assert(rtree_box_area(boxB) == 8.0f);
	dh_assert(rtree_box_area(boxC) == 1.0f);
	dh_pop();
	
	dh_pop();
}

static void
test_choose_subtree(void)
{
	dh_push("rtree_choose_subtree()");

	//rtree_choose_subtree(tree, newBox, path, lengthOut);
	
	dh_pop();
}

static void
test_create_destroy(void)
{
	dh_push("Create & Destroy");

	RTREE *tree = rtree_create();
	// This test will fail erroneously if the system is out of memory
	dh_assert(tree != NULL);

	rtree_destroy(tree);

	dh_pop();
}

#if 0
static void
check_integrity_rec(struct rtree_node *node, int height, const struct rtree_box *bound)
{
	if (!node) dh_throw("Node at height %d is NULL", height);
	if (height == 0) return;
	for (unsigned i = 0; i < node->count; i++) {
		const struct rtree_box *box = &node->box[i];
		if (!(box->min[0] >= bound->min[0] && box->min[1] >= bound->min[1] && box->min[2] >= bound->min[2] &&
			box->max[0] <= bound->max[0] && box->max[1] <= bound->max[1] && box->max[2] <= bound->max[2])) {
			dh_throw("AABB at height %d does not cover all children", height);
		}
		check_integrity_rec(node->child[i].ptr, height-1, box);
	}
}

static void
check_integrity(struct rtree *tree)
{
	struct rtree_box bound;
	bound.min[0] = bound.min[1] = bound.min[2] = -INFINITY;
	bound.max[0] = bound.max[1] = bound.max[2] =  INFINITY;
	check_integrity_rec(tree->root, tree->height, &bound);
}

static struct rtree_box
synthesize_box(int idx)
{
	struct rtree_box box;
	box.min[0] = box.min[1] = box.min[2] = idx;
	box.max[0] = box.min[0] + 5;
	box.max[1] = box.min[1] + 5;
	box.max[2] = box.min[2] + 5;
	return box;
}

static void
check_contents_rec(struct rtree_node *node, int height, bool *found, int count)
{
	if (height > 0) {
		for (unsigned i = 0; i < node->count; i++) {
			check_contents_rec(node->child[i].ptr, height-1, found, count);
		}
	} else {
		for (unsigned i = 0; i < node->count; i++) {
			int idx = (intptr_t)(void *)node->child[i].ptr;
			if (idx < 0 || idx >= count) dh_throw("Invalid element #%d emerged", idx);
			if (found[idx]) dh_throw("Element #%d was duplicated", idx);
			found[idx] = true;
		}
	}
}

static void
check_contents(struct rtree *tree, int count)
{
	bool *found = calloc(1, count);
	check_contents_rec(tree->root, tree->height, found, count);
	for (int i = 0; i < count; i++) {
		if (!found[i]) dh_throw("Element #%d was lost", i);
	}
	free(found);
}

void
test_rtree(void)
{
	dh_push("R*-Tree");

	dh_push("tree creation");
	RTREE *tree = rtree_create();
	check_integrity(tree);
	dh_pop();
	
	dh_push("filling tree without splitting");
	for (int i = 0; i < RTREE_MAX_CHILDREN; i++) {
		dh_push("inserting element #%d", i);
		struct rtree_box box = synthesize_box(i);
		rtree_insert(tree, box);
		dh_pop();
	}
	check_integrity(tree);
	dh_assert(tree->height == 0);
	dh_assert(tree->root->count == RTREE_MAX_CHILDREN);
	check_contents(tree, RTREE_MAX_CHILDREN);
	dh_pop();

	dh_push("forcing tree to split");
	struct rtree_box box = synthesize_box(RTREE_MAX_CHILDREN);
	rtree_insert(tree, box);
	check_integrity(tree);
	dh_assert(tree->height == 1);
	dh_assert(tree->root->count == 2);
	check_contents(tree, RTREE_MAX_CHILDREN+1);
	dh_pop();

#if 0
	dh_push("deleting a lot of elements");
	for (int i = RTREE_MIN_CHILDREN; i < RTREE_MAX_CHILDREN+1; i++) {
		dh_push("deleting element #%d", i);
		struct rtree_box box = synthesize_box(i);
		rtree_delete(tree, (void *)(intptr_t)i, &box);
		dh_pop();
	}
	check_integrity(tree);
	check_contents(tree, RTREE_MIN_CHILDREN);
	dh_pop();

	dh_push("deleting the last remaining elements");
	for (int i = 0; i < RTREE_MIN_CHILDREN; i++) {
		dh_push("deleting element #%d", i);
		struct rtree_box box = synthesize_box(i);
		rtree_delete(tree, (void *)(intptr_t)i, &box);
		dh_pop();
	}
	dh_assert(tree.height == 0);
	dh_assert(tree.root->count == 0);
	dh_pop();
#endif

	rtree_destroy(tree);
	dh_pop();
}
#endif

int
main()
{
	dh_init(stderr);
	dh_branch( test_box_operations(); );
	dh_branch( test_create_destroy(); );
	return dh_summarize();
}
