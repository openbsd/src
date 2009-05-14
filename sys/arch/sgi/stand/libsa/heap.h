/*	$OpenBSD: heap.h,v 1.1 2009/05/14 18:57:43 miod Exp $	*/
/* public domain */

/*
 * Declarations for the libsa heap allocator.
 *
 * Relocatable 64 bit bootblocks use memory below the load address and
 * can not use the `end' symbol.
 */

#ifdef __LP64__
#define	NEEDS_HEAP_INIT

#define	HEAP_LIMIT	heap_limit
#define	HEAP_SIZE	(1UL << 20)	/* 1MB */
#define	HEAP_START	heap_start

static unsigned long heap_start;
static unsigned long heap_limit;
static char *top;			/* no longer declared in alloc.c */

static inline void heap_init(void);
static inline void
heap_init()
{
	extern char __start[];

	if (top == NULL) {
		heap_limit = (unsigned long)&__start;
		heap_start = heap_limit - HEAP_SIZE;
		top = (char *)heap_start;
	}
}

#endif
