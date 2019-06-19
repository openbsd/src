/* Public domain. */

#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <sys/types.h>
#include <sys/malloc.h>

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/gfp.h>

static inline void *
kmalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags);
}

static inline void *
kmalloc_array(size_t n, size_t size, int flags)
{
	if (n != 0 && SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags);
}

static inline void *
kcalloc(size_t n, size_t size, int flags)
{
	if (n != 0 && SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags | M_ZERO);
}

static inline void *
kzalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags | M_ZERO);
}

static inline void
kfree(const void *objp)
{
	free((void *)objp, M_DRM, 0);
}

#endif
