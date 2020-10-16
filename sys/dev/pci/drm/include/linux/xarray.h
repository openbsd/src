/* Public domain. */

#ifndef _LINUX_XARRAY_H
#define _LINUX_XARRAY_H

#include <linux/gfp.h>

#include <sys/tree.h>

#define XA_FLAGS_ALLOC		1
#define XA_FLAGS_ALLOC1		2

struct xarray_entry {
	SPLAY_ENTRY(xarray_entry) entry;
	int id;
	void *ptr;
};

struct xarray {
	gfp_t		xa_flags;
	SPLAY_HEAD(xarray_tree, xarray_entry) xa_tree;
};

void xa_init_flags(struct xarray *, gfp_t);
void xa_destroy(struct xarray *);
int xa_alloc(struct xarray *, u32 *, void *, int, gfp_t);
void *xa_load(struct xarray *, unsigned long);
void *xa_erase(struct xarray *, unsigned long);
void *xa_get_next(struct xarray *, unsigned long *);

#define xa_for_each(xa, index, entry) \
	for (index = 0; ((entry) = xa_get_next(xa, &(index))) != NULL; index++)

#define xa_limit_32b	0

static inline void *
xa_mk_value(unsigned long v)
{
	unsigned long r = (v << 1) | 1;
	return (void *)r;
}

static inline bool
xa_is_value(const void *e)
{
	unsigned long v = (unsigned long)e;
	return v & 1;
}

static inline unsigned long
xa_to_value(const void *e)
{
	unsigned long v = (unsigned long)e;
	return v >> 1;
}

#endif
