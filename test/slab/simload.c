// Simulated workload for the SLAB allocator.

#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define SLAB_IMPLEMENTATION
#define SLAB_assert(cond) assert(cond)
#include <tom_slab.h>

#define CANARY_SIZE 16
#define MAX_ITEMS (1 * 1000 * 1000)

#define LENGTH(arr) (sizeof(arr)/sizeof*(arr))

struct item {
	char canary[CANARY_SIZE];
};

SLAB *slab;
struct item *items[MAX_ITEMS];
size_t num_items;

static void
op_reset()
{
	num_items = 0;

	slab_reset(slab);
}

static void
op_alloc()
{
	if (num_items >= MAX_ITEMS) return;

	struct item *item = slab_alloc(slab);
	SLAB_assert(item != NULL);

	getentropy(item->canary, CANARY_SIZE);

	items[num_items] = item;
	num_items += 1;
}

static void
free_item(size_t index)
{
	struct item *item = items[index];

	num_items -= 1;
	memmove(&items[index], &items[index + 1],
		(num_items - index) * sizeof(*items));
	
	slab_free(slab, item);
}

static void
op_free_random()
{
	if (num_items == 0) return;

	// This isn't perfectly fair selection,
	// but it is good enough for our purposes.
	size_t index;
	getentropy(&index, sizeof index);
	index %= num_items;

	free_item(index);
}

static void
op_free_latest()
{
	if (num_items == 0) return;

	free_item(num_items - 1);
}

struct op_option {
	float weight;
	void (*op)();
};

const struct op_option op_table[] = {
	{     1.0, op_reset },
	{ 10000.0, op_alloc },
	{  1000.0, op_free_random },
	{  2000.0, op_free_latest },
};

int
main()
{
	slab = slab_create(sizeof(struct item), NULL, NULL);

	float total_weight = 0;
	for (unsigned i = 0; i < LENGTH(op_table); i++) {
		total_weight += op_table[i].weight;
	}

	for (;;) {
		printf("%zu / %zu\n", num_items, (size_t)MAX_ITEMS);

		size_t number = 0;
		getentropy(&number, sizeof number);
		float unif = (float)number / SIZE_MAX;

		for (unsigned i = 0; i < LENGTH(op_table); i++) {
			float norm_weight = op_table[i].weight / total_weight;
			if (unif < norm_weight) {
				op_table[i].op();
				break;
			}
			unif -= norm_weight;
		}
	}

	slab_destroy(slab);

	return 0;
}
