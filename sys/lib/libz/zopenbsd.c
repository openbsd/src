#include <sys/types.h>
#include <sys/malloc.h>

/*
 * Space allocation and freeing routines for use by zlib routines.
 */
void *
zcalloc(void *notused, u_int items, u_int size)
{
    return mallocarray(items, size, M_DEVBUF, M_NOWAIT);
}

void
zcfree(void *notused, void *ptr)
{
    free(ptr, M_DEVBUF, 0);
}
