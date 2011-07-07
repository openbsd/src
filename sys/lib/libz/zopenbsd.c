#include <sys/types.h>
#include <sys/malloc.h>
#include <lib/libz/zutil.h>

/*
 * Space allocation and freeing routines for use by zlib routines.
 */
void *
zcalloc(notused, items, size)
    void *notused;
    u_int items, size;
{
    void *ptr;

    ptr = malloc(items * size, M_DEVBUF, M_NOWAIT);
    return ptr;
}

void
zcfree(notused, ptr)
    void *notused;
    void *ptr;
{
    free(ptr, M_DEVBUF);
}
