/*	$OpenBSD: kern_malloc_debug.c,v 1.10 2001/07/26 13:33:52 art Exp $	*/

/*
 * Copyright (c) 1999, 2000 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * This really belongs in kern/kern_malloc.c, but it was too much pollution.
 */

/*
 * It's only possible to debug one type/size at a time. The question is
 * if this is a limitation or a feature. We never want to run this as the
 * default malloc because we'll run out of memory really fast. Adding
 * more types will also add to the complexity of the code.
 *
 * This is really simple. Every malloc() allocates two virtual pages,
 * the second page is left unmapped, and the the value returned is aligned
 * so that it ends at (or very close to) the page boundary to catch overflows.
 * Every free() changes the protection of the first page to VM_PROT_NONE so
 * that we can catch any dangling writes to it.
 * To minimize the risk of writes to recycled chunks we keep an LRU of latest
 * freed chunks. The length of it is controlled by MALLOC_DEBUG_CHUNKS.
 *
 * Don't expect any performance.
 *
 * TODO:
 *  - support for size >= PAGE_SIZE
 *  - add support to the fault handler to give better diagnostics if we fail.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

/*
 * malloc_deb_type and malloc_deb_size define the type and size of
 * memory to be debugged. Use 0 for a wildcard. malloc_deb_size_lo
 * is the lower limit and malloc_deb_size_hi the upper limit of sizes
 * being debugged; 0 will not work as a wildcard for the upper limit.
 * For any debugging to take place, type must be != -1, size must be >= 0,
 * and if the limits are being used, size must be set to 0.
 * See /usr/src/sys/sys/malloc.h and malloc(9) for a list of types.
 *
 * Although those are variables, it's a really bad idea to change the type
 * if any memory chunks of this type are used. It's ok to change the size
 * in runtime.
 */
int malloc_deb_type = -1;
int malloc_deb_size = -1;
int malloc_deb_size_lo = -1;
int malloc_deb_size_hi = -1;

/*
 * MALLOC_DEBUG_CHUNKS is the number of memory chunks we require on the
 * freelist before we reuse them.
 */
#define MALLOC_DEBUG_CHUNKS 16

/* returns 0 if normal malloc/free should be used */
int debug_malloc __P((unsigned long, int, int, void **));
int debug_free __P((void *, int));
void debug_malloc_init __P((void));

void malloc_deb_allocate_free __P((int));
void debug_malloc_print __P((void));
void debug_malloc_printit __P((int (*) __P((const char *, ...)), vaddr_t));

struct malloc_deb_entry {
	TAILQ_ENTRY(malloc_deb_entry) md_list;
	vaddr_t md_va;
	paddr_t md_pa;
	size_t md_size;
	int md_type;
};

TAILQ_HEAD(,malloc_deb_entry) malloc_deb_free;
TAILQ_HEAD(,malloc_deb_entry) malloc_deb_used;

int malloc_deb_allocs;
int malloc_deb_frees;
int malloc_deb_pages;
int malloc_deb_chunks_on_freelist;

#ifndef M_DEBUG
#define M_DEBUG M_TEMP
#endif

int
debug_malloc(size, type, flags, addr)
	unsigned long size;
	int type, flags;
	void **addr;
{
	struct malloc_deb_entry *md = NULL;
	int s;
	int wait = flags & M_NOWAIT;

	/* Careful not to compare unsigned long to int -1 */
	if ((type != malloc_deb_type && malloc_deb_type != 0) ||
	    (size != malloc_deb_size && malloc_deb_size != 0) ||
	    (malloc_deb_size_lo != -1 && size < malloc_deb_size_lo) ||
	    (malloc_deb_size_hi != -1 && size > malloc_deb_size_hi) ||
	    type == M_DEBUG)
		return 0;

	/* XXX - fix later */
	if (size > PAGE_SIZE)
		return 0;

	s = splimp();
	if (malloc_deb_chunks_on_freelist < MALLOC_DEBUG_CHUNKS)
		malloc_deb_allocate_free(wait);

	md = TAILQ_FIRST(&malloc_deb_free);
	if (md == NULL) {
		splx(s);
		return 0;
	}
	TAILQ_REMOVE(&malloc_deb_free, md, md_list);
	malloc_deb_chunks_on_freelist--;

	TAILQ_INSERT_HEAD(&malloc_deb_used, md, md_list);
	malloc_deb_allocs++;
	splx(s);


	pmap_kenter_pa(md->md_va, md->md_pa, VM_PROT_ALL);

	md->md_size = size;
	md->md_type = type;

	/*
	 * Align the returned addr so that it ends where the first page
	 * ends. roundup to get decent alignment.
	 */
	*addr = (void *)(md->md_va + PAGE_SIZE - roundup(size, sizeof(long)));
	return 1;
}

int
debug_free(addr, type)
	void *addr;
	int type;
{
	struct malloc_deb_entry *md;
	int s;
	vaddr_t va;

	if ((type != malloc_deb_type && malloc_deb_type != 0) ||
	    type == M_DEBUG)
		return 0;

	/*
	 * trunc_page to get the address of the page.
	 */
	va = trunc_page((vaddr_t)addr);

	s = splimp();
	TAILQ_FOREACH(md, &malloc_deb_used, md_list)
		if (md->md_va == va)
			break;

	/*
	 * If we are not responsible for this entry, let the normal free
	 * handle it
	 */
	if (md == NULL) {
		/*
		 * sanity check. Check for multiple frees.
		 */
		TAILQ_FOREACH(md, &malloc_deb_free, md_list)
			if (md->md_va == va)
				panic("debug_free: already free");
		splx(s);
		return 0;
	}

	malloc_deb_frees++;
	TAILQ_REMOVE(&malloc_deb_used, md, md_list);

	TAILQ_INSERT_TAIL(&malloc_deb_free, md, md_list);
	malloc_deb_chunks_on_freelist++;
	/*
	 * unmap the page.
	 */
	pmap_kremove(md->md_va, PAGE_SIZE);
	splx(s);

	return 1;
}

void
debug_malloc_init()
{
	TAILQ_INIT(&malloc_deb_free);
	TAILQ_INIT(&malloc_deb_used);

	malloc_deb_allocs = 0;
	malloc_deb_frees = 0;
	malloc_deb_pages = 0;
	malloc_deb_chunks_on_freelist = 0;
}

/*
 * Add one chunk to the freelist.
 *
 * called at splimp.
 */
void
malloc_deb_allocate_free(wait)
	int wait;
{
	vaddr_t va, offset;
	struct vm_page *pg;
	struct malloc_deb_entry *md;

	md = malloc(sizeof(struct malloc_deb_entry), M_DEBUG,
		    wait ? M_WAITOK : M_NOWAIT);
	if (md == NULL)
		return;

	va = uvm_km_kmemalloc(kmem_map, uvmexp.kmem_object,
			      PAGE_SIZE * 2,
			      UVM_KMF_VALLOC | (wait ? UVM_KMF_NOWAIT : 0));
	if (va == 0) {
		free(md, M_DEBUG);
		return;
	}

	offset = va - vm_map_min(kernel_map);
	do {
		simple_lock(&uvmexp.kmem_object->vmobjlock);
		pg = uvm_pagealloc(uvmexp.kmem_object, offset, NULL, 0);
		if (pg) {
			pg->flags &= ~PG_BUSY;  /* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&uvmexp.kmem_object->vmobjlock);

		if (pg)
			break;

		if (wait == 0) {
			uvm_unmap(kmem_map, va, va + PAGE_SIZE * 2);
			free(md, M_DEBUG);
			return;
		}
		uvm_wait("debug_malloc");
	} while (1);

	md->md_va = va;
	md->md_pa = VM_PAGE_TO_PHYS(pg);

	malloc_deb_pages++;
	TAILQ_INSERT_HEAD(&malloc_deb_free, md, md_list);
	malloc_deb_chunks_on_freelist++;
}

void
debug_malloc_print()
{
	debug_malloc_printit(printf, NULL);
}

void
debug_malloc_printit(pr, addr)
        int (*pr) __P((const char *, ...));
	vaddr_t addr;
{
	struct malloc_deb_entry *md;

	if (addr) {
		TAILQ_FOREACH(md, &malloc_deb_free, md_list) {
			if (addr >= md->md_va &&
			    addr < md->md_va + 2 * PAGE_SIZE) {
				(*pr)("Memory at address 0x%x is in a freed "
				      "area. type %d, size: %d\n ",
				      addr, md->md_type, md->md_size);
				return;
			}
		}
		TAILQ_FOREACH(md, &malloc_deb_used, md_list) {
			if (addr >= md->md_va + PAGE_SIZE &&
			    addr < md->md_va + 2 * PAGE_SIZE) {
				(*pr)("Memory at address 0x%x is just outside "
				      "an allocated area. type %d, size: %d\n",
				      addr, md->md_type, md->md_size);
				return;
			}
		}
		(*pr)("Memory at address 0x%x is outside debugged malloc.\n");
		return;
	}

	(*pr)("allocs: %d\n", malloc_deb_allocs);
	(*pr)("frees: %d\n", malloc_deb_frees);
	(*pr)("pages used: %d\n", malloc_deb_pages);
	(*pr)("chunks on freelist: %d\n", malloc_deb_chunks_on_freelist);

	(*pr)("\taddr:\tsize:\n");
	(*pr)("free chunks:\n");
	TAILQ_FOREACH(md, &malloc_deb_free, md_list)
		(*pr)("\t0x%x\t0x%x\t%d\n", md->md_va, md->md_size,
		      md->md_type);
	(*pr)("used chunks:\n");
	TAILQ_FOREACH(md, &malloc_deb_used, md_list)
		(*pr)("\t0x%x\t0x%x\t%d\n", md->md_va, md->md_size,
		      md->md_type);
}


