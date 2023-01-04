/* Public domain. */

#ifndef _LINUX_STRING_H
#define _LINUX_STRING_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/stdint.h>
#include <sys/errno.h>

#include <linux/compiler.h>

void *memchr_inv(const void *, int, size_t);

static inline void *
memset32(uint32_t *b, uint32_t c, size_t len)
{
	uint32_t *dst = b;
	while (len--)
		*dst++ = c;
	return b;
}

static inline void *
memset64(uint64_t *b, uint64_t c, size_t len)
{
	uint64_t *dst = b;
	while (len--)
		*dst++ = c;
	return b;
}

static inline void *
memset_p(void **p, void *v, size_t n)
{
#ifdef __LP64__
	return memset64((uint64_t *)p, (uintptr_t)v, n);
#else
	return memset32((uint32_t *)p, (uintptr_t)v, n);
#endif
}

static inline void *
kmemdup(const void *src, size_t len, int flags)
{
	void *p = malloc(len, M_DRM, flags);
	if (p)
		memcpy(p, src, len);
	return (p);
}

static inline void *
kstrdup(const char *str, int flags)
{
	size_t len;
	char *p;

	if (str == NULL)
		return NULL;

	len = strlen(str) + 1;
	p = malloc(len, M_DRM, flags);
	if (p)
		memcpy(p, str, len);
	return (p);
}

static inline int
match_string(const char * const *array,  size_t n, const char *str)
{
	int i;

	for (i = 0; i < n; i++) {
		if (array[i] == NULL)
			break;
		if (!strcmp(array[i], str))	
			return i;
	}

	return -EINVAL;
}

static inline ssize_t
strscpy(char *dst, const char *src, size_t dstsize)
{
	ssize_t r;
	r = strlcpy(dst, src, dstsize);
	if (dstsize == 0 || r >= dstsize)
		return -E2BIG;
	return r;
}

static inline ssize_t
strscpy_pad(char *dst, const char *src, size_t dstsize)
{
	memset(dst, 0, dstsize);
	return strscpy(dst, src, dstsize);
}

#endif
