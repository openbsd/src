/*	$OpenBSD: kern_malloc_debug.c,v 1.22 2003/06/03 01:27:31 art Exp $	*/

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
 * 2. The name of the author may not be used to endorse or promote products
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
 * the second page is left unmapped, and the value returned is aligned
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
#include <sys/pool.h>

#include <uvm/uvm.h>

/*
 * debug_malloc_type and debug_malloc_size define the type and size of
 * memory to be debugged. Use 0 for a wildcard. debug_malloc_size_lo
 * is the lower limit and debug_malloc_size_hi the upper limit of sizes
 * being debugged; 0 will not work as a wildcard for the upper limit.
 * For any debugging to take place, type must be != -1, size must be >= 0,
 * and if the limits are being used, size must be set to 0.
 * See /usr/src/sys/sys/malloc.h and malloc(9) for a list of types.
 *
 * Although those are variables, it's a really bad idea to change the type
 * if any memory chunks of this type are used. It's ok to change the size
 * in runtime.
 */
int debug_malloc_type = -1;
int debug_malloc_size = -1;
int debug_malloc_size_lo = -1;
int debug_malloc_size_hi = -1;

/*
 * MALLOC_DEBUG_CHUNKS is the number of memory chunks we require on the
 * freelist before we reuse them.
 */
#define MALLOC_DEBUG_CHUNKS 16

void debug_malloc_allocate_free(int);

struct debug_malloc_entry {
	TAILQ_ENTRY(debug_malloc_entry) md_list;
	vaddr_t md_va;
	paddr_t md_pa;
	size_t md_size;
	int md_type;
};

TAILQ_HEAD(,debug_malloc_entry) debug_malloc_freelist;
TAILQ_HEAD(,debug_malloc_entry) debug_malloc_usedlist;

int debug_malloc_allocs;
int debug_malloc_frees;
int debug_malloc_pages;
int debug_malloc_chunks_on_freelist;

int debug_malloc_initialized;

struct pool debug_malloc_pool;

int
debug_malloc(unsigned long size, int type, int flags, void **addr)
{
	struct debug_malloc_entry *md = NULL;
	int s, wait = flags & M_NOWAIT;

	/* Careful not to compare unsigned long to int -1 */
	if (((type != debug_malloc_type && debug_malloc_type != 0) ||
	    (size != debug_malloc_size && debug_malloc_size != 0) ||
	    (debug_malloc_size_lo != -1 && size < debug_malloc_size_lo) ||
	    (debug_malloc_size_hi != -1 && size > debug_malloc_size_hi) ||
	    !debug_malloc_initialized) && type != M_DEBUG)
		return (0);

	/* XXX - fix later */
	if (size > PAGE_SIZE)
		return (0);

	s = splvm();
	if (debug_malloc_chunks_on_freelist < MALLOC_DEBUG_CHUNKS)
		debug_malloc_allocate_free(wait);

	md = TAILQ_FIRST(&debug_malloc_freelist);
	if (md == NULL) {
		splx(s);
		return (0);
	}
	TAILQ_REMOVE(&debug_malloc_freelist, md, md_list);
	debug_malloc_chunks_on_freelist--;

	TAILQ_INSERT_HEAD(&debug_malloc_usedlist, md, md_list);
	debug_malloc_allocs++;
	splx(s);

	pmap_kenter_pa(md->md_va, md->md_pa, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());

	md->md_size = size;
	md->md_type = type;

	/*
	 * Align the returned addr so that it ends where the first page
	 * ends. roundup to get decent alignment.
	 */
	*addr = (void *)(md->md_va + PAGE_SIZE - roundup(size, sizeof(long)));
	return (1);
}

int
debug_free(void *addr, int type)
{
	struct debug_malloc_entry *md;
	vaddr_t va;
	int s;

	if (type != debug_malloc_type && debug_malloc_type != 0 &&
	    type != M_DEBUG)
		return (0);

	/*
	 * trunc_page to get the address of the page.
	 */
	va = trunc_page((vaddr_t)addr);

	s = splvm();
	TAILQ_FOREACH(md, &debug_malloc_usedlist, md_list)
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
		TAILQ_FOREACH(md, &debug_malloc_freelist, md_list)
			if (md->md_va == va)
				panic("debug_free: already free");
		splx(s);
		return (0);
	}

	debug_malloc_frees++;
	TAILQ_REMOVE(&debug_malloc_usedlist, md, md_list);

	TAILQ_INSERT_TAIL(&debug_malloc_freelist, md, md_list);
	debug_malloc_chunks_on_freelist++;
	/*
	 * unmap the page.
	 */
	pmap_kremove(md->md_va, PAGE_SIZE);
	pmap_update(pmap_kernel());
	splx(s);

	return (1);
}

void
debug_malloc_init(void)
{

	TAILQ_INIT(&debug_malloc_freelist);
	TAILQ_INIT(&debug_malloc_usedlist);

	debug_malloc_allocs = 0;
	debug_malloc_frees = 0;
	debug_malloc_pages = 0;
	debug_malloc_chunks_on_freelist = 0;

	pool_init(&debug_malloc_pool, sizeof(struct debug_malloc_entry),
	    0, 0, 0, "mdbepl", NULL);

	debug_malloc_initialized = 1;
}

/*
 * Add one chunk to the freelist.
 *
 * called at splvm.
 */
void
debug_malloc_allocate_free(int wait)
{
	vaddr_t va, offset;
	struct vm_page *pg;
	struct debug_malloc_entry *md;

	splassert(IPL_VM);

	md = pool_get(&debug_malloc_pool, wait ? PR_WAITOK : PR_NOWAIT);
	if (md == NULL)
		return;

	va = uvm_km_kmemalloc(kmem_map, uvmexp.kmem_object, PAGE_SIZE * 2,
	    UVM_KMF_VALLOC | (wait ? UVM_KMF_NOWAIT : 0));
	if (va == 0) {
		pool_put(&debug_malloc_pool, md);
		return;
	}

	offset = va - vm_map_min(kernel_map);
	for (;;) {
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
			pool_put(&debug_malloc_pool, md);
			return;
		}
		uvm_wait("debug_malloc");
	}

	md->md_va = va;
	md->md_pa = VM_PAGE_TO_PHYS(pg);

	debug_malloc_pages++;
	TAILQ_INSERT_HEAD(&debug_malloc_freelist, md, md_list);
	debug_malloc_chunks_on_freelist++;
}

void
debug_malloc_print(void)
{

	debug_malloc_printit(printf, NULL);
}

void
debug_malloc_printit(int (*pr)(const char *, ...), vaddr_t addr)
{
	struct debug_malloc_entry *md;

	if (addr) {
		TAILQ_FOREACH(md, &debug_malloc_freelist, md_list) {
			if (addr >= md->md_va &&
			    addr < md->md_va + 2 * PAGE_SIZE) {
				(*pr)("Memory at address 0x%x is in a freed "
				      "area. type %d, size: %d\n ",
				      addr, md->md_type, md->md_size);
				return;
			}
		}
		TAILQ_FOREACH(md, &debug_malloc_usedlist, md_list) {
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

	(*pr)("allocs: %d\n", debug_malloc_allocs);
	(*pr)("frees: %d\n", debug_malloc_frees);
	(*pr)("pages used: %d\n", debug_malloc_pages);
	(*pr)("chunks on freelist: %d\n", debug_malloc_chunks_on_freelist);

	(*pr)("\taddr:\tsize:\n");
	(*pr)("free chunks:\n");
	TAILQ_FOREACH(md, &debug_malloc_freelist, md_list)
		(*pr)("\t0x%x\t0x%x\t%d\n", md->md_va, md->md_size,
		      md->md_type);
	(*pr)("used chunks:\n");
	TAILQ_FOREACH(md, &debug_malloc_usedlist, md_list)
		(*pr)("\t0x%x\t0x%x\t%d\n", md->md_va, md->md_size,
		      md->md_type);
}
