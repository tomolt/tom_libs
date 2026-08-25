/* tom_slab: SLAB allocator
 * version: 
 *
 * Copyright (C) 2022-2026 Thomas Oltmann
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
 *
 * Macros that may be declared before including this header:
 * SLAB_IMPLEMENTATION
 * SLAB_PAGE_SIZE
 * SLAB_ffs
 * SLAB_malloc
 * SLAB_free
 * SLAB_alloc_page
 * SLAB_free_page
 * SLAB_mutex
 * SLAB_mutex_init
 * SLAB_mutex_lock
 * SLAB_mutex_unlock
 * SLAB_mutex_destroy
 */

// TODO multiple arenas, one per thread?

#ifndef _TOM_SLAB_H_
#define _TOM_SLAB_H_

struct slab;

struct slab *slab_create(int elemsz, void (*ctor)(void *, int), void (*dtor)(void *, int));
void slab_destroy(struct slab *sys);
void slab_reset(struct slab *sys);

void *slab_alloc(struct slab *sys);
void  slab_free (struct slab *sys, void *ptr);

void slab_iterate(struct slab *sys, void (*func)(void *, void *), void *userdata);

void slab_trim(struct slab *slab);

#endif

#ifdef SLAB_IMPLEMENTATION

#include <stddef.h> /* for offsetof */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SLAB_ffs
#  ifdef _MSC_VER
#    include <intrin.h>
static inline int
slab_ffs_ms(int x)
{
	unsigned long index = 0;
	return _BitScanForward(&index, x) ? index + 1 : 0;
}
#    define SLAB_ffs(x) slab_ffs_ms(x)
#  else
#    define SLAB_ffs(x) __builtin_ffs(x)
#  endif
#endif

#ifndef SLAB_malloc
#  include <stdlib.h>
#  define SLAB_malloc(size) malloc(size)
#  define SLAB_free(ptr)    free(ptr)
#endif

#ifndef SLAB_alloc_page
#  ifdef _MSC_VER
#    include <malloc.h>
#    define SLAB_alloc_page(size) _aligned_malloc(size, size)
#    define SLAB_free_page(ptr)   _aligned_free(ptr)
#  else
#    include <stdlib.h>
static inline void *
slab_alloc_page_posix(size_t size)
{
	void *ptr = NULL;
	int s = posix_memalign(&ptr, size, size);
	return s == 0 ? ptr : NULL;
}
#    define SLAB_alloc_page(size) slab_alloc_page_posix(size)
#    define SLAB_free_page(ptr)   free(ptr)
#  endif
#endif

#ifndef SLAB_mutex
#  include <threads.h>
#  define SLAB_mutex              mtx_t
#  define SLAB_mutex_init(mtx)    mtx_init(&mtx, mtx_plain)
#  define SLAB_mutex_lock(mtx)    mtx_lock(&mtx)
#  define SLAB_mutex_unlock(mtx)  mtx_unlock(&mtx)
#  define SLAB_mutex_destroy(mtx) mtx_destroy(&mtx)
#endif

#ifndef SLAB_PAGE_SIZE
#  define SLAB_PAGE_SIZE 4096
#endif

// Bit-field manipulation
#define SLAB_DIVIDE_ROUND_UP(a, b) (((a)+(b)-1)/(b))
#define SLAB_WORD_BITS (8*sizeof(unsigned int))
#define SLAB_SET_BIT(b,i) ((b)[(i)/SLAB_WORD_BITS] |= 1u<<((i)%SLAB_WORD_BITS))
#define SLAB_CLR_BIT(b,i) ((b)[(i)/SLAB_WORD_BITS] &= ~(1u<<((i)%SLAB_WORD_BITS)))
#define SLAB_GET_BIT(b,i) (((b)[(i)/SLAB_WORD_BITS] >> (i%SLAB_WORD_BITS)) & 1u)

// Intrusive doubly-linked list

#define SLAB_container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define SLAB_FOR_IN_LIST(elem,list,type,memb) \
	type *elem; \
	struct slab_list_node *_node = (list).head.next; \
	while (_node != &(list).head && \
	       (elem  = SLAB_container_of(_node, type, memb), \
	        _node = _node->next, \
		1))

struct slab_list_node {
	struct slab_list_node *next;
	struct slab_list_node *prev;
};

struct list {
	struct slab_list_node head;
};

static inline void
slab_link_nodes(struct slab_list_node *n1, struct slab_list_node *n2)
{
	n1->next = n2;
	n2->prev = n1;
}

static inline void
slab_list_create(struct list *list)
{
	list->head.next = &list->head;
	list->head.prev = &list->head;
}

static inline void
slab_list_push_back(struct list *list, struct slab_list_node *node)
{
	slab_link_nodes(list->head.prev, node);
	slab_link_nodes(node, &list->head);
}

static inline void
slab_list_remove(struct slab_list_node *node)
{
	slab_link_nodes(node->prev, node->next);
	node->next = NULL;
	node->prev = NULL;
}

static inline struct slab_list_node *
slab_list_pop_front(struct list *list)
{
	struct slab_list_node *node;
	node = list->head.next;
	if (node == &list->head)
		return NULL;
	slab_list_remove(node);
	return node;
}

static inline bool
slab_list_is_empty(struct list *list)
{
	return list->head.next == &list->head;
}

#define SLAB_MIN_ALLOC 16
#define SLAB_AVAIL_WORDS ((int) SLAB_DIVIDE_ROUND_UP(SLAB_PAGE_SIZE / SLAB_MIN_ALLOC, SLAB_WORD_BITS))

#define SLAB_GET_BASE(p) ((uintptr_t) (p) & ~(uintptr_t) (SLAB_PAGE_SIZE - 1))
#define SLAB_GET_FOOTER(b) ((struct slab_footer *) ((b) + SLAB_PAGE_SIZE) - 1)

struct slab_footer {
	struct slab_list_node node;
	unsigned int avail[SLAB_AVAIL_WORDS];
	int numelems;
};

struct slab {
	struct list empty;
	struct list partial;
	struct list full;
	void (*ctor)(void *, int);
	void (*dtor)(void *, int);
	int elemsz;
	int maxelems;
};

static void slab_noop_ctor_or_dtor(void *ptr, int elemsz) { (void)ptr; (void)elemsz; }

static void
slab_grow(struct slab *sys)
{
	uintptr_t base = (uintptr_t) SLAB_alloc_page(SLAB_PAGE_SIZE);
	struct slab_footer *footer = SLAB_GET_FOOTER(base);
	memset(footer, 0, sizeof *footer);
	for (int i = 0; i < sys->maxelems; i++) {
		void *ptr = (void *) (base + i * sys->elemsz);
		sys->ctor(ptr, sys->elemsz);
		SLAB_SET_BIT(footer->avail, i);
	}
	slab_list_push_back(&sys->empty, &footer->node);
}

static void
slab_release(struct slab *sys, struct slab_list_node *node)
{
	slab_list_remove(node);
	uintptr_t base = SLAB_GET_BASE(node);
	for (int i = 0; i < sys->maxelems; i++) {
		void *ptr = (void *) (base + i * sys->elemsz);
		sys->dtor(ptr, sys->elemsz);
	}
	SLAB_free_page((void *) base);
}

static void
slab_release_list(struct slab *sys, struct list *list)
{
	struct slab_list_node *node = list->head.next;
	while (node != &list->head) {
		struct slab_list_node *next = node->next;
		slab_release(sys, node);
		node = next;
	}
}

struct slab *
slab_create(int elemsz, void (*ctor)(void *, int), void (*dtor)(void *, int))
{
	if (elemsz < SLAB_MIN_ALLOC) {
		elemsz = SLAB_MIN_ALLOC;
	}
	if (SLAB_PAGE_SIZE - sizeof (struct slab_footer) < (unsigned) elemsz) {
		return NULL;
	}

	struct slab *sys = SLAB_malloc(sizeof *sys);
	if (!sys) {
		return NULL;
	}
	memset(sys, 0, sizeof *sys);

	slab_list_create(&sys->empty);
	slab_list_create(&sys->partial);
	slab_list_create(&sys->full);

	sys->ctor = ctor ? ctor : slab_noop_ctor_or_dtor;
	sys->dtor = dtor ? dtor : slab_noop_ctor_or_dtor;
	sys->elemsz = elemsz;
	sys->maxelems = (SLAB_PAGE_SIZE - sizeof (struct slab_footer)) / sys->elemsz;

	return sys;
}

void
slab_destroy(struct slab *sys)
{
	slab_release_list(sys, &sys->empty);
	slab_release_list(sys, &sys->partial);
	slab_release_list(sys, &sys->full);
	SLAB_free(sys);
}

static void
slab_reset_list(struct slab *sys, struct list *list)
{
	SLAB_FOR_IN_LIST(footer, *list, struct slab_footer, node) {
		for (int idx = 0; idx < sys->maxelems; idx++) {
			SLAB_SET_BIT(footer->avail, idx);
		}
		slab_list_remove(&footer->node);
		slab_list_push_back(&sys->empty, &footer->node);
	}
}

void
slab_reset(struct slab *sys)
{
	slab_reset_list(sys, &sys->partial);
	slab_reset_list(sys, &sys->full);
}

void *
slab_alloc(struct slab *sys)
{
	if (slab_list_is_empty(&sys->partial)) {
		if (slab_list_is_empty(&sys->empty)) {
			slab_grow(sys);
		}
		slab_list_push_back(&sys->partial, slab_list_pop_front(&sys->empty));
	}

	struct slab_list_node *node = sys->partial.head.next;
	struct slab_footer *footer = SLAB_container_of(node, struct slab_footer, node);

	int idx = -1;
	for (int w = 0; w < SLAB_AVAIL_WORDS; w++) {
		if (footer->avail[w]) {
			int b = SLAB_ffs(footer->avail[w]) - 1;
			idx = w * SLAB_WORD_BITS + b;
			break;
		}
	}
	SLAB_CLR_BIT(footer->avail, idx);

	if (++footer->numelems == sys->maxelems) {
		slab_list_remove(&footer->node);
		slab_list_push_back(&sys->full, &footer->node);
	}

	uintptr_t base = SLAB_GET_BASE(node);
	return (void *) (base + idx * sys->elemsz);
}

void
slab_free(struct slab *sys, void *ptr)
{
	uintptr_t base = SLAB_GET_BASE(ptr);
	struct slab_footer *footer = SLAB_GET_FOOTER(base);
	int idx = ((uintptr_t) ptr - base) / sys->elemsz;

	SLAB_SET_BIT(footer->avail, idx);
	
	if (!--footer->numelems) {
		slab_list_remove(&footer->node);
		slab_list_push_back(&sys->empty, &footer->node);
	}
	
	// TODO trigger garbage collection if ratio too extreme
}

static void
slab_iterate_list(struct list *list, int elemsz, int maxelems,
	void (*func)(void *, void *), void *userdata)
{
	SLAB_FOR_IN_LIST(footer, *list, struct slab_footer, node) {
		uintptr_t base = SLAB_GET_BASE(footer);
		for (int idx = 0; idx < maxelems; idx++) {
			if (!SLAB_GET_BIT(footer->avail, idx)) {
				void *ptr = (void *) (base + idx * elemsz);
				func(userdata, ptr);
			}
		}
	}
}

void
slab_iterate(struct slab *sys,
	void (*func)(void *, void *), void *userdata)
{
	slab_iterate_list(&sys->full, sys->elemsz, sys->maxelems, func, userdata);
	slab_iterate_list(&sys->partial, sys->elemsz, sys->maxelems, func, userdata);
}

void
slab_trim(struct slab *slab)
{
	// TODO garbage-collect
}

#endif
