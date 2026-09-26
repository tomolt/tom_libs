/* R* Tree
 */

#ifndef _TOM_RTREE_H_
#define _TOM_RTREE_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define RTREE_DIMENSION 3

struct rtree_box {
	float min[RTREE_DIMENSION];
	float max[RTREE_DIMENSION];
};

typedef struct rtree RTREE;
typedef uintptr_t RTREE_ID;

#define RTREE_INVALID_ID SIZE_MAX

RTREE *rtree_create(void);
void   rtree_destroy(RTREE *tree);

RTREE_ID rtree_insert(RTREE *tree, struct rtree_box box);
bool     rtree_delete(RTREE *tree, RTREE_ID oid);
size_t   rtree_search(RTREE *tree, RTREE_ID *results, size_t maxResults);

// TODO bulk build, K nearest neighbor, ray cast

#endif

#ifdef RTREE_IMPLEMENTATION

#include <string.h>

#ifndef RTREE_malloc
#  include <stdlib.h>
#  define RTREE_malloc(size) malloc(size)
#  define RTREE_free(ptr)    free(ptr)
#endif

#ifndef RTREE_rwlock_t

// Pthreads

#  include <pthread.h>

#  define RTREE_rwlock_t              pthread_rwlock_t
#  define RTREE_rwlock_init(lockp)    pthread_rwlock_init(lockp, NULL)
#  define RTREE_rwlock_destroy(lockp) pthread_rwlock_destroy(lockp)
#  define RTREE_lock_read(lockp)      pthread_rwlock_rdlock(lockp)
#  define RTREE_lock_write(lockp)     pthread_rwlock_wrlock(lockp)
#  define RTREE_unlock_read(lockp)    pthread_rwlock_unlock(lockp)
#  define RTREE_unlock_write(lockp)   pthread_rwlock_unlock(lockp)

#endif

#ifndef RTREE_assert
#  include <assert.h>
#  define RTREE_assert(cond) assert(cond) 
#endif

#define RTREE_MIN(a,b) ((a) < (b) ? (a) : (b))
#define RTREE_MAX(a,b) ((a) > (b) ? (a) : (b))

#define RTREE_SORT_INLINE(elemtype, compare, array, count)\
	do {\
		for (size_t _i = 1; _i < (count); _i++) {\
			for (size_t _j = _i; _j > 0 && compare((array)[_j - 1], (array)[_j]) > 0; _j--) {\
				elemtype _tmp = (array)[_j];\
				(array)[_j] = (array)[_j - 1];\
				(array)[_j - 1] = _tmp;\
			}\
		}\
	} while (0)

#define RTREE_GET_BIT(val, idx) (((val) >> (idx)) & 1u)
#define RTREE_SET_BIT(val, idx) ((val) |= (uint64_t)1u << (idx))

// TODO determine actual values
#define RTREE_MIN_CHILDREN 26
#define RTREE_MAX_CHILDREN 64
#define RTREE_MAX_HEIGHT   64

struct rtree_node {
	struct rtree_box box[RTREE_MAX_CHILDREN];
	union rtree_child {
    		RTREE_ID           oid; // If node is a leaf
    		struct rtree_node *ptr; // if node is an internal node
	} child[RTREE_MAX_CHILDREN];
	unsigned count;
};

struct rtree {
	RTREE_rwlock_t     lock;
	struct rtree_node *root;
	struct rtree_box   rootBox;
	unsigned           height;
};

static bool
rtree_boxes_overlap(struct rtree_box boxA, struct rtree_box boxB)
{
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {
		if (boxA.max[axis] <= boxB.min[axis] || boxB.max[axis] <= boxA.min[axis]) {
			return false;
		}
	}
	return true;
}

static struct rtree_box
rtree_box_union(struct rtree_box boxA, struct rtree_box boxB)
{
	struct rtree_box boxU;
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {
		boxU.min[axis] = RTREE_MIN(boxA.min[axis], boxB.min[axis]);
		boxU.max[axis] = RTREE_MAX(boxA.max[axis], boxB.max[axis]);
	}
	return boxU;
}

static struct rtree_box
rtree_box_intersection(struct rtree_box boxA, struct rtree_box boxB)
{
	struct rtree_box boxI;
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {
		boxI.min[axis] = RTREE_MAX(boxA.min[axis], boxB.min[axis]);
		boxI.max[axis] = RTREE_MIN(boxA.max[axis], boxB.max[axis]);
		if (boxI.min[axis] > boxI.max[axis]) {
			boxI.max[axis] = boxI.min[axis];
		}
	}
	return boxI;
}

static float
rtree_box_margin(struct rtree_box box)
{
	float margin = 0.0f;
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {
		margin += box.max[axis] - box.min[axis];
	}
	return margin;
}

static float
rtree_box_area(struct rtree_box box)
{
	float area = 1.0f;
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {
    		area *= box.max[axis] - box.min[axis];
	}
	return area;
}

/* Path does not include the (returned) leaf node.
 */
static struct rtree_node *
rtree_choose_subtree(RTREE *tree, struct rtree_box newBox, struct rtree_node *path[], unsigned *lengthOut)
{
	if (!tree || !tree->root) return NULL;

	struct rtree_node *node = tree->root;
	unsigned level = tree->height - 1;
	unsigned length = 0;
	while (level > 0) {
		path[length++] = node;

		RTREE_assert(node->count > 0);

		float costs[RTREE_MAX_CHILDREN];
#if 0
		if (level == 1) {
			// Minimum overlap costs
			for (unsigned i = 0; i < node->count; i++) {
				struct rtree_box boxU = rtree_box_union(node->box[i], newBox);
			}
		} else {
#endif
			// Minimum area costs
			for (unsigned i = 0; i < node->count; i++) {
				struct rtree_box boxU = rtree_box_union(node->box[i], newBox);
				costs[i] = rtree_box_area(boxU);
			}
#if 0
		}
#endif

		unsigned bestIdx = 0;
		for (unsigned i = 1; i < node->count; i++) {
			if (costs[i] < costs[bestIdx]) {
				bestIdx = i;
			}
		}

		node = node->child[bestIdx].ptr;
		level--;
	}

	*lengthOut = length;
	return node;
}

static unsigned
rtree_choose_split_axis(const struct rtree_box box[], unsigned count)
{
	float scores[RTREE_DIMENSION];
	for (unsigned axis = 0; axis < RTREE_DIMENSION; axis++) {

		unsigned order[RTREE_MAX_CHILDREN];
		for (unsigned i = 0; i < count; i++) {
			order[i] = i;
		}

#define RTREE_COMPARE_ALONG_AXIS(a, b)\
	box[a].min[axis] < box[b].min[axis] ? -1 :\
	box[a].min[axis] > box[b].min[axis] ?  1 :\
	box[a].max[axis] < box[b].max[axis] ? -1 :\
	box[a].max[axis] > box[b].max[axis] ?  1 :\
	0

		RTREE_SORT_INLINE(unsigned, RTREE_COMPARE_ALONG_AXIS, order, count);

#undef RTREE_COMPARE_ALONG_AXIS

		RTREE_assert(count >= 2 * RTREE_MIN_CHILDREN);

		for (unsigned d = RTREE_MIN_CHILDREN; d < count - RTREE_MIN_CHILDREN; d++) {
		}
		
		scores[axis] = 0.0f;
	}

	unsigned bestAxis = 0;
	for (unsigned axis = 1; axis < RTREE_DIMENSION; axis++) {
    		if (scores[axis] < scores[bestAxis]) {
        		bestAxis = axis;
    		}
	}
	return bestAxis;
}

static unsigned
rtree_choose_split_index(struct rtree_node *node, unsigned axis)
{
	// TODO this whole implementation is extremely naive and should be optimized and streamlined

	unsigned order[RTREE_MAX_CHILDREN];
	for (unsigned i = 0; i < node->count; i++) {
		order[i] = i;
	}

#define RTREE_COMPARE_ALONG_AXIS(a, b)\
	node->box[a].min[axis] < node->box[b].min[axis] ? -1 :\
	node->box[a].min[axis] > node->box[b].min[axis] ?  1 :\
	node->box[a].max[axis] < node->box[b].max[axis] ? -1 :\
	node->box[a].max[axis] > node->box[b].max[axis] ?  1 :\
	0

	RTREE_SORT_INLINE(unsigned, RTREE_COMPARE_ALONG_AXIS, order, node->count);

#undef RTREE_COMPARE_ALONG_AXIS

	float costs[RTREE_MAX_CHILDREN];
	memset(costs, 0, sizeof costs);
	struct rtree_box belowUnion = node->box[0];
	struct rtree_box aboveUnion = node->box[node->count - 1];
	for (unsigned i = 1; i < node->count; i++) {
		costs[i] += rtree_box_margin(belowUnion);
		costs[node->count - 1 - i] += rtree_box_margin(aboveUnion);
		
		belowUnion = rtree_box_union(belowUnion, node->box[i]);
		aboveUnion = rtree_box_union(aboveUnion, node->box[node->count - 1 - i]);
	}

	unsigned splitIndex = RTREE_MIN_CHILDREN;
	for (unsigned d = RTREE_MIN_CHILDREN + 1; d < node->count - RTREE_MIN_CHILDREN; d++) {
		if (costs[d] < costs[splitIndex]) {
			splitIndex = d;
		}
	}

	return splitIndex;
}

static struct rtree_node *
rtree_distribute(struct rtree_node *node, unsigned axis, unsigned splitIndex)
{
	unsigned order[RTREE_MAX_CHILDREN];
	for (unsigned i = 0; i < node->count; i++) {
		order[i] = i;
	}

#define RTREE_COMPARE_ALONG_AXIS(a, b)\
	node->box[a].min[axis] < node->box[b].min[axis] ? -1 :\
	node->box[a].min[axis] > node->box[b].min[axis] ?  1 :\
	node->box[a].max[axis] < node->box[b].max[axis] ? -1 :\
	node->box[a].max[axis] > node->box[b].max[axis] ?  1 :\
	0

	RTREE_SORT_INLINE(unsigned, RTREE_COMPARE_ALONG_AXIS, order, node->count);

#undef RTREE_COMPARE_ALONG_AXIS

	struct rtree_box sortedBoxes[RTREE_MAX_CHILDREN];
	union rtree_child sortedChildren[RTREE_MAX_CHILDREN];
	for (unsigned i = 0; i < node->count; i++) {
		sortedBoxes[i] = node->box[order[i]];
		sortedChildren[i] = node->child[order[i]];
	}

	struct rtree_node *other = RTREE_malloc(sizeof *other);
	if (!other) {
		// TODO
		return NULL;
	}

	unsigned count = node->count;
	memcpy(node->box,    sortedBoxes,    splitIndex * sizeof(struct rtree_box));
	memcpy(node->child,  sortedChildren, splitIndex * sizeof(union rtree_child));
	memcpy(other->box,   sortedBoxes    + splitIndex, (node->count - splitIndex) * sizeof(struct rtree_box));
	memcpy(other->child, sortedChildren + splitIndex, (node->count - splitIndex) * sizeof(union rtree_child));

	node->count  = splitIndex;
	other->count = count - splitIndex;

	return other;
}

static struct rtree_node *
rtree_split(struct rtree_node *node)
{
	unsigned axis = rtree_choose_split_axis(node->box, node->count);
	unsigned index = rtree_choose_split_index(node, axis);
	return rtree_distribute(node, axis, index);
}

#if 0
static void
rtree_reinsert()
{
	for (children and candidate) {
    		
	}
}
#endif

#if 0
static void
rtree_treat_overflow(RTREE *tree, struct rtree_node *node, unsigned level, uint64_t *overflowed)
{
	if (node != tree->root && !RTREE_GET_BIT(*overflowed, level)) {
    		rtree_reinsert();
	} else {
    		rtree_split();
	}
}
#endif

#if 0
static void
rtree_insert_at_level(RTREE *tree, struct rtree_box newBox)
{
	struct rtree_node *path[RTREE_MAX_CHILDREN];
	unsigned length;
	struct rtree_node *leaf = rtree_choose_subtree(tree, path, &length);

	if (leaf->count < RTREE_MAX_CHILDREN) {
		// store
		return;
	}

	if (subtree->count == RTREE_MAX_CHILDREN) {
		rtree_treat_overflow();
	}

	// Propagate split upward, updating the boxes
	for () {
	}
}
#endif

RTREE *
rtree_create(void)
{
	RTREE *tree = RTREE_malloc(sizeof *tree);
	if (!tree) return NULL;

	RTREE_rwlock_init(&tree->lock);
	tree->root = NULL;
	tree->height = 0;
	
	return tree;
}

void
rtree_destroy(RTREE *tree)
{
	if (!tree) return;

	RTREE_rwlock_destroy(&tree->lock);

	free(tree);
}

RTREE_ID
rtree_insert(RTREE *tree, struct rtree_box box)
{
	if (!tree) return false;

	RTREE_lock_write(&tree->lock);

	//rtree_insert_at_level();

	RTREE_unlock_write(&tree->lock);

	return true;
}

#endif
