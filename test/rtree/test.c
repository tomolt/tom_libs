#define _POSIX_C_SOURCE 200809L
#include <stdint.h>

#define DH_IMPLEMENT_HERE
#include "dh_cuts.h"

#define RTREE_IMPLEMENTATION
#include "../../tom_rtree.h"

static void
check_integrity_rec(struct rnode *node, int height, const struct raabb *bound)
{
	if (!node) dh_throw("Node at height %d is NULL", height);
	if (height == 0) return;
	for (int i = 0; i < node->degree; i++) {
		struct raabb *aabb = &node->aabbs[i];
		if (!(aabb->min[0] >= bound->min[0] && aabb->min[1] >= bound->min[1] && aabb->min[2] >= bound->min[2] &&
			aabb->max[0] <= bound->max[0] && aabb->max[1] <= bound->max[1] && aabb->max[2] <= bound->max[2])) {
			dh_throw("AABB at height %d does not cover all children", height);
		}
		struct raabb aabbCopy = node->aabbs[i];
		dh_assert(node->aabbs[i].volume == aabb_update_volume(&aabbCopy));
		check_integrity_rec(node->child[i], height-1, aabb);
	}

}

static void
check_integrity(struct rtree *tree)
{
	struct raabb bound;
	bound.min[0] = bound.min[1] = bound.min[2] = -INFINITY;
	bound.max[0] = bound.max[1] = bound.max[2] =  INFINITY;
	check_integrity_rec(tree->root, tree->height, &bound);
}

static struct raabb
synthesize_aabb(int idx)
{
	struct raabb aabb;
	aabb.min[0] = aabb.min[1] = aabb.min[2] = idx;
	aabb.max[0] = aabb.min[0] + 5;
	aabb.max[1] = aabb.min[1] + 5;
	aabb.max[2] = aabb.min[2] + 5;
	return aabb;
}

static void
check_contents_rec(struct rnode *node, int height, bool *found, int count)
{
	if (height > 0) {
		for (int i = 0; i < node->degree; i++) {
			check_contents_rec(node->child[i], height-1, found, count);
		}
	} else {
		for (int i = 0; i < node->degree; i++) {
			int idx = (intptr_t)(void *)node->child[i];
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
	dh_push("R-Tree");
	struct rtree tree;

	dh_push("tree creation");
	rtree_create(&tree);
	check_integrity(&tree);
	dh_pop();
	
	dh_push("filling tree without splitting");
	for (int i = 0; i < RTREE_MAX_DEGREE; i++) {
		dh_push("inserting element #%d", i);
		struct raabb aabb = synthesize_aabb(i);
		rtree_insert(&tree, (void *)(intptr_t)i, &aabb);
		dh_pop();
	}
	check_integrity(&tree);
	dh_assert(tree.height == 0);
	dh_assert(tree.root->degree == RTREE_MAX_DEGREE);
	check_contents(&tree, RTREE_MAX_DEGREE);
	dh_pop();

	dh_push("forcing tree to split");
	struct raabb aabb = synthesize_aabb(RTREE_MAX_DEGREE);
	rtree_insert(&tree, (void *)(intptr_t)RTREE_MAX_DEGREE, &aabb);
	check_integrity(&tree);
	dh_assert(tree.height == 1);
	dh_assert(tree.root->degree == 2);
	check_contents(&tree, RTREE_MAX_DEGREE+1);
	dh_pop();

	dh_push("deleting a lot of elements");
	for (int i = RTREE_MIN_DEGREE; i < RTREE_MAX_DEGREE+1; i++) {
		dh_push("deleting element #%d", i);
		struct raabb aabb = synthesize_aabb(i);
		rtree_delete(&tree, (void *)(intptr_t)i, &aabb);
		dh_pop();
	}
	check_integrity(&tree);
	check_contents(&tree, RTREE_MIN_DEGREE);
	dh_pop();

	dh_push("deleting the last remaining elements");
	for (int i = 0; i < RTREE_MIN_DEGREE; i++) {
		dh_push("deleting element #%d", i);
		struct raabb aabb = synthesize_aabb(i);
		rtree_delete(&tree, (void *)(intptr_t)i, &aabb);
		dh_pop();
	}
	dh_assert(tree.height == 0);
	dh_assert(tree.root->degree == 0);
	dh_pop();

	rtree_destroy(&tree);
	dh_pop();
}

int
main()
{
	dh_branch( test_rtree(); );
	return 0;
}
