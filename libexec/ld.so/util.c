/*	$OpenBSD: util.c,v 1.1 2002/02/21 23:17:53 drahn Exp $	*/

#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include "archdep.h"

/*
 * Static vars usable after bootsrapping.
 */
static void *_dl_malloc_base;
static void *_dl_malloc_pool = 0;
static long *_dl_malloc_free = 0;

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	newstr = _dl_malloc(_dl_strlen(orig)+1);
	_dl_strcpy(newstr, orig);
	return (newstr);
}

/*
 *  The following malloc/free code is a very simplified implementation
 *  of a malloc function. However, we do not need to be very complex here
 *  because we only free memory when 'dlclose()' is called and we can
 *  reuse at least the memory allocated for the object descriptor. We have
 *  one dynamic string allocated, the library name and it is likely that
 *  we can reuse that one to without a lot of complex colapsing code.
 */

void *
_dl_malloc(int size)
{
	long *p;
	long *t, *n;

	size = (size + 8 + DL_MALLOC_ALIGN - 1) & ~(DL_MALLOC_ALIGN - 1);

	if ((t = _dl_malloc_free) != 0) {	/* Try free list first */
		n = (long *)&_dl_malloc_free;
		while (t && t[-1] < size) {
			n = t;
			t = (long *)*t;
		}
		if (t) {
			*n = *t;
			_dl_memset(t, 0, t[-1] - 4);
			return((void *)t);
		}
	}
	if ((_dl_malloc_pool == 0) ||
	    (_dl_malloc_pool + size > _dl_malloc_base + 4096)) {
		_dl_malloc_pool = (void *)_dl_mmap((void *)0, 4096,
						PROT_READ|PROT_WRITE,
						MAP_ANON|MAP_PRIVATE, -1, 0);
		if (_dl_malloc_pool == 0 || _dl_malloc_pool == MAP_FAILED ) {
			_dl_printf("Dynamic loader failure: malloc.\n");
			_dl_exit(7);
		}
		_dl_malloc_base = _dl_malloc_pool;
	}
	p = _dl_malloc_pool;
	_dl_malloc_pool += size;
	_dl_memset(p, 0, size);
	*p = size;
	return((void *)(p + 1));
}

void
_dl_free(void *p)
{
	long *t = (long *)p;

	*t = (long)_dl_malloc_free;
	_dl_malloc_free = p;
}
