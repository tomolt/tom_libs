#define SLAB_IMPLEMENTATION
#include <tom_slab.h>

int
main()
{
	struct slab *slab;
	slab = slab_create(9, NULL, NULL);

	void *elem = slab_alloc(slab);
	slab_free(slab, elem);

	slab_destroy(slab);

	return 0;
}
