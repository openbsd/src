/*	$OpenBSD: rf_debugMem.c,v 1.5 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_debugMem.c,v 1.7 2000/01/07 03:40:59 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * debugMem.c:  Memory usage debugging stuff.
 *
 * Malloc, Calloc, and Free are #defined everywhere
 * to do_malloc, do_calloc, and do_free.
 *
 * If RF_UTILITY is nonzero, it means we are compiling one of the
 * RAIDframe utility programs, such as rfctrl or smd. In this
 * case, we eliminate all references to the threads package
 * and to the allocation list stuff.
 */

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_options.h"
#include "rf_debugMem.h"
#include "rf_general.h"

static long tot_mem_in_use = 0;

/* Hash table of information about memory allocations. */
#define	RF_MH_TABLESIZE		1000

struct mh_struct {
	void		*address;
	int		 size;
	int		 line;
	char		*filen;
	char		 allocated;
	struct mh_struct *next;
};

static struct mh_struct *mh_table[RF_MH_TABLESIZE];

RF_DECLARE_MUTEX(rf_debug_mem_mutex);
static int mh_table_initialized = 0;

void rf_memory_hash_insert(void *, int, int, char *);
int  rf_memory_hash_remove(void *, int);

void
rf_record_malloc(void *p, int size, int line, char *filen)
{
	RF_ASSERT(size != 0);

	/*RF_LOCK_MUTEX(rf_debug_mem_mutex);*/
	rf_memory_hash_insert(p, size, line, filen);
	tot_mem_in_use += size;
	/*RF_UNLOCK_MUTEX(rf_debug_mem_mutex);*/

	if ((long) p == rf_memDebugAddress) {
		printf("Allocate: debug address allocated from line %d file"
		    " %s\n", line, filen);
	}
}

void
rf_unrecord_malloc(void *p, int sz)
{
	int size;

	/*RF_LOCK_MUTEX(rf_debug_mem_mutex);*/
	size = rf_memory_hash_remove(p, sz);
	tot_mem_in_use -= size;
	/*RF_UNLOCK_MUTEX(rf_debug_mem_mutex);*/
	if ((long) p == rf_memDebugAddress) {
		/* This is really only a flag line for gdb. */
		printf("Free: Found debug address\n");
	}
}

void
rf_print_unfreed(void)
{
	int i, foundone = 0;
	struct mh_struct *p;

	for (i = 0; i < RF_MH_TABLESIZE; i++) {
		for (p = mh_table[i]; p; p = p->next)
			if (p->allocated) {
				if (!foundone)
					printf("\n\nThere are unfreed"
					    " memory locations at"
					    " program shutdown:\n");
				foundone = 1;
				printf("Addr 0x%lx Size %d line %d file %s\n",
				    (long) p->address, p->size, p->line,
				    p->filen);
			}
	}
	if (tot_mem_in_use) {
		printf("%ld total bytes in use\n", tot_mem_in_use);
	}
}

int
rf_ConfigureDebugMem(RF_ShutdownList_t **listp)
{
	int i, rc;

	rc = rf_create_managed_mutex(listp, &rf_debug_mem_mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		return (rc);
	}
	if (rf_memDebug) {
		for (i = 0; i < RF_MH_TABLESIZE; i++)
			mh_table[i] = NULL;
		mh_table_initialized = 1;
	}
	return (0);
}

#define	HASHADDR(_a_)	( (((unsigned long) _a_)>>3) % RF_MH_TABLESIZE )

void
rf_memory_hash_insert(void *addr, int size, int line, char *filen)
{
	unsigned long bucket = HASHADDR(addr);
	struct mh_struct *p;

	RF_ASSERT(mh_table_initialized);

	/* Search for this address in the hash table. */
	for (p = mh_table[bucket]; p && (p->address != addr); p = p->next);
	if (!p) {
		RF_Malloc(p, sizeof(struct mh_struct), (struct mh_struct *));
		RF_ASSERT(p);
		p->next = mh_table[bucket];
		mh_table[bucket] = p;
		p->address = addr;
		p->allocated = 0;
	}
	if (p->allocated) {
		printf("ERROR:  Reallocated address 0x%lx from line %d,"
		    " file %s without intervening free\n", (long) addr,
		    line, filen);
		printf("        Last allocated from line %d file %s\n",
		    p->line, p->filen);
		RF_ASSERT(0);
	}
	p->size = size;
	p->line = line;
	p->filen = filen;
	p->allocated = 1;
}

int
rf_memory_hash_remove(void *addr, int sz)
{
	unsigned long bucket = HASHADDR(addr);
	struct mh_struct *p;

	RF_ASSERT(mh_table_initialized);
	for (p = mh_table[bucket]; p && (p->address != addr); p = p->next);
	if (!p) {
		printf("ERROR:  Freeing never-allocated address 0x%lx\n",
		    (long) addr);
		RF_PANIC();
	}
	if (!p->allocated) {
		printf("ERROR:  Freeing unallocated address 0x%lx."
		    "  Last allocation line %d file %s\n", (long) addr,
		    p->line, p->filen);
		RF_PANIC();
	}
	if (sz > 0 && p->size != sz) {
		/*
		 * This error can be suppressed by using a negative value
		 * as the size to free.
		 */
		printf("ERROR:  Incorrect size at free for address 0x%lx:"
		    " is %d should be %d.  Alloc at line %d of file %s\n",
		    (unsigned long) addr, sz, p->size, p->line, p->filen);
		RF_PANIC();
	}
	p->allocated = 0;
	return (p->size);
}
