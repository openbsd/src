/*	$OpenBSD: swap_pager.c,v 1.15 1999/02/08 01:10:58 art Exp $	*/
/*	$NetBSD: swap_pager.c,v 1.27 1996/03/16 23:15:20 christos Exp $	*/

/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: swap_pager.c 1.4 91/04/30$
 *
 *	@(#)swap_pager.c	8.9 (Berkeley) 3/21/94
 */

/*
 * Quick hack to page to dedicated partition(s).
 * TODO:
 *	Add multiprocessor locks
 *	Deal with async writes in a better fashion
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/swap_pager.h>

#define NSWSIZES	16	/* size of swtab */
#define MAXDADDRS	64	/* max # of disk addrs for fixed allocations */
#ifndef NPENDINGIO
#define NPENDINGIO	64	/* max # of pending cleans */
#endif

#ifdef DEBUG
int	swpagerdebug = 0x100;
#define	SDB_FOLLOW	0x001
#define SDB_INIT	0x002
#define SDB_ALLOC	0x004
#define SDB_IO		0x008
#define SDB_WRITE	0x010
#define SDB_FAIL	0x020
#define SDB_ALLOCBLK	0x040
#define SDB_FULL	0x080
#define SDB_ANOM	0x100
#define SDB_ANOMPANIC	0x200
#define SDB_CLUSTER	0x400
#define SDB_PARANOIA	0x800
#endif

TAILQ_HEAD(swpclean, swpagerclean);

struct swpagerclean {
	TAILQ_ENTRY(swpagerclean)	spc_list;
	int				spc_flags;
	struct buf			*spc_bp;
	sw_pager_t			spc_swp;
	vm_offset_t			spc_kva;
	vm_page_t			spc_m;
	int				spc_npages;
} swcleanlist[NPENDINGIO];
typedef struct swpagerclean *swp_clean_t;

/* spc_flags values */
#define SPC_FREE	0x00
#define SPC_BUSY	0x01
#define SPC_DONE	0x02
#define SPC_ERROR	0x04

struct swtab {
	vm_size_t st_osize;	/* size of object (bytes) */
	int	  st_bsize;	/* vs. size of swap block (DEV_BSIZE units) */
#ifdef DEBUG
	u_long	  st_inuse;	/* number in this range in use */
	u_long	  st_usecnt;	/* total used of this size */
#endif
} swtab[NSWSIZES+1];

#ifdef DEBUG
int		swap_pager_poip;	/* pageouts in progress */
int		swap_pager_piip;	/* pageins in progress */
#endif

int		swap_pager_maxcluster;	/* maximum cluster size */
int		swap_pager_npendingio;	/* number of pager clean structs */

struct swpclean	swap_pager_inuse;	/* list of pending page cleans */
struct swpclean	swap_pager_free;	/* list of free pager clean structs */
struct pagerlst	swap_pager_list;	/* list of "named" anon regions */

extern struct buf bswlist;		/* import from vm_swap.c */

static void 		swap_pager_init __P((void));
static vm_pager_t	swap_pager_alloc
			    __P((caddr_t, vm_size_t, vm_prot_t, vm_offset_t));
static void		swap_pager_clean __P((int));
#ifdef DEBUG
static void		swap_pager_clean_check __P((vm_page_t *, int, int));
#endif
static void		swap_pager_cluster
			    __P((vm_pager_t, vm_offset_t,
				 vm_offset_t *, vm_offset_t *));
static void		swap_pager_dealloc __P((vm_pager_t));
static int		swap_pager_remove
			    __P((vm_pager_t, vm_offset_t, vm_offset_t));
static vm_offset_t	swap_pager_next __P((vm_pager_t, vm_offset_t));
static int		swap_pager_count __P((vm_pager_t));
static int		swap_pager_getpage
			    __P((vm_pager_t, vm_page_t *, int, boolean_t));
static boolean_t	swap_pager_haspage __P((vm_pager_t, vm_offset_t));
static int		swap_pager_io __P((sw_pager_t, vm_page_t *, int, int));
static void		swap_pager_iodone __P((struct buf *));
static int		swap_pager_putpage
			    __P((vm_pager_t, vm_page_t *, int, boolean_t));
static int		count_bits __P((u_int));

struct pagerops swappagerops = {
	swap_pager_init,
	swap_pager_alloc,
	swap_pager_dealloc,
	swap_pager_getpage,
	swap_pager_putpage,
	swap_pager_haspage,
	swap_pager_cluster,
	swap_pager_remove,
	swap_pager_next,
	swap_pager_count
};

static void
swap_pager_init()
{
	register swp_clean_t spc;
	register int i, bsize;
	extern int dmmin, dmmax;
	int maxbsize;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_INIT))
		printf("swpg_init()\n");
#endif
	dfltpagerops = &swappagerops;
	TAILQ_INIT(&swap_pager_list);

	/*
	 * Allocate async IO structures.
	 *
	 * XXX it would be nice if we could do this dynamically based on
	 * the value of nswbuf (since we are ultimately limited by that)
	 * but neither nswbuf or malloc has been initialized yet.  So the
	 * structs are statically allocated above.
	 */
	swap_pager_npendingio = NPENDINGIO;

	/*
	 * Initialize clean lists
	 */
	TAILQ_INIT(&swap_pager_inuse);
	TAILQ_INIT(&swap_pager_free);
	for (i = 0, spc = swcleanlist; i < swap_pager_npendingio; i++, spc++) {
		TAILQ_INSERT_TAIL(&swap_pager_free, spc, spc_list);
		spc->spc_flags = SPC_FREE;
	}

	/*
	 * Calculate the swap allocation constants.
	 */
	if (dmmin == 0) {
		dmmin = DMMIN;
		if (dmmin < CLBYTES/DEV_BSIZE)
			dmmin = CLBYTES/DEV_BSIZE;
	}
	if (dmmax == 0)
		dmmax = DMMAX;

	/*
	 * Fill in our table of object size vs. allocation size
	 */
	bsize = btodb(PAGE_SIZE);
	if (bsize < dmmin)
		bsize = dmmin;
	maxbsize = btodb(sizeof(sw_bm_t) * NBBY * PAGE_SIZE);
	if (maxbsize > dmmax)
		maxbsize = dmmax;
	for (i = 0; i < NSWSIZES; i++) {
		swtab[i].st_osize = (vm_size_t) (MAXDADDRS * dbtob(bsize));
		swtab[i].st_bsize = bsize;
		if (bsize <= btodb(MAXPHYS))
			swap_pager_maxcluster = dbtob(bsize);
#ifdef DEBUG
		if (swpagerdebug & SDB_INIT)
			printf("swpg_init: ix %d, size %lx, bsize %x\n",
			    i, swtab[i].st_osize, swtab[i].st_bsize);
#endif
		if (bsize >= maxbsize)
			break;
		bsize *= 2;
	}
	swtab[i].st_osize = 0;
	swtab[i].st_bsize = bsize;
}

/*
 * Allocate a pager structure and associated resources.
 * Note that if we are called from the pageout daemon (handle == NULL)
 * we should not wait for memory as it could resulting in deadlock.
 */
static vm_pager_t
swap_pager_alloc(handle, size, prot, foff)
	caddr_t handle;
	register vm_size_t size;
	vm_prot_t prot;
	vm_offset_t foff;
{
	register vm_pager_t pager;
	register sw_pager_t swp;
	struct swtab *swt;
	int waitok;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOC))
		printf("swpg_alloc(%p, %lx, %x)\n", handle, size, prot);
#endif
	/*
	 * If this is a "named" anonymous region, look it up and
	 * return the appropriate pager if it exists.
	 */
	if (handle) {
		pager = vm_pager_lookup(&swap_pager_list, handle);
		if (pager != NULL) {
			/*
			 * Use vm_object_lookup to gain a reference
			 * to the object and also to remove from the
			 * object cache.
			 */
			if (vm_object_lookup(pager) == NULL)
				panic("swap_pager_alloc: bad object");
			return (pager);
		}
	}
	/*
	 * Pager doesn't exist, allocate swap management resources
	 * and initialize.
	 */
	waitok = handle ? M_WAITOK : M_NOWAIT;
	pager = (vm_pager_t)malloc(sizeof *pager, M_VMPAGER, waitok);
	if (pager == NULL)
		return (NULL);
	swp = (sw_pager_t)malloc(sizeof *swp, M_VMPGDATA, waitok);
	if (swp == NULL) {
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("swpg_alloc: swpager malloc failed\n");
#endif
		free((caddr_t)pager, M_VMPAGER);
		return (NULL);
	}
	size = round_page(size);
	for (swt = swtab; swt->st_osize; swt++)
		if (size <= swt->st_osize)
			break;
#ifdef DEBUG
	swt->st_inuse++;
	swt->st_usecnt++;
#endif
	swp->sw_osize = size;
	swp->sw_bsize = swt->st_bsize;
	swp->sw_nblocks = (btodb(size) + swp->sw_bsize - 1) / swp->sw_bsize;
	swp->sw_blocks = (sw_blk_t)malloc(swp->sw_nblocks *
	    sizeof(*swp->sw_blocks), M_VMPGDATA, M_NOWAIT);
	if (swp->sw_blocks == NULL) {
		free((caddr_t)swp, M_VMPGDATA);
		free((caddr_t)pager, M_VMPAGER);
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("swpg_alloc: sw_blocks malloc failed\n");
		swt->st_inuse--;
		swt->st_usecnt--;
#endif
		return (FALSE);
	}
	bzero((caddr_t)swp->sw_blocks,
	    swp->sw_nblocks * sizeof(*swp->sw_blocks));
	swp->sw_poip = swp->sw_cnt = 0;
	if (handle) {
		vm_object_t object;

		swp->sw_flags = SW_NAMED;
		TAILQ_INSERT_TAIL(&swap_pager_list, pager, pg_list);
		/*
		 * Consistant with other pagers: return with object
		 * referenced.  Can't do this with handle == NULL
		 * since it might be the pageout daemon calling.
		 */
		object = vm_object_allocate(size);
		vm_object_enter(object, pager);
		vm_object_setpager(object, pager, 0, FALSE);
	} else {
		swp->sw_flags = 0;
		pager->pg_list.tqe_next = NULL;
		pager->pg_list.tqe_prev = NULL;
	}
	pager->pg_handle = handle;
	pager->pg_ops = &swappagerops;
	pager->pg_type = PG_SWAP;
	pager->pg_flags = PG_CLUSTERPUT;
	pager->pg_data = swp;

#ifdef DEBUG
	if (swpagerdebug & SDB_ALLOC)
		printf("swpg_alloc: pg_data %p, %x of %x at %p\n",
		    swp, swp->sw_nblocks, swp->sw_bsize, swp->sw_blocks);
#endif
	return (pager);
}

static void
swap_pager_dealloc(pager)
	vm_pager_t pager;
{
	register int i;
	register sw_blk_t bp;
	register sw_pager_t swp;
	int s;
#ifdef DEBUG
	struct swtab *swt;
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOC))
		printf("swpg_dealloc(%p)\n", pager);
#endif
	/*
	 * Remove from list right away so lookups will fail if we
	 * block for pageout completion.
	 */
	swp = (sw_pager_t) pager->pg_data;
	if (swp->sw_flags & SW_NAMED) {
		TAILQ_REMOVE(&swap_pager_list, pager, pg_list);
		swp->sw_flags &= ~SW_NAMED;
	}
#ifdef DEBUG
	for (swt = swtab; swt->st_osize; swt++)
		if (swp->sw_osize <= swt->st_osize)
			break;
	swt->st_inuse--;
#endif

	/*
	 * Wait for all pageouts to finish and remove
	 * all entries from cleaning list.
	 */
	s = splbio();
	while (swp->sw_poip) {
		swp->sw_flags |= SW_WANTED;
		(void) tsleep(swp, PVM, "swpgdealloc", 0);
	}
	splx(s);
	swap_pager_clean(B_WRITE);

	/*
	 * Free left over swap blocks
	 */
	for (i = 0, bp = swp->sw_blocks; i < swp->sw_nblocks; i++, bp++)
		if (bp->swb_block) {
#ifdef DEBUG
			if (swpagerdebug & (SDB_ALLOCBLK|SDB_FULL))
				printf("swpg_dealloc: blk %x\n",
				    bp->swb_block);
#endif
			rmfree(swapmap, swp->sw_bsize, bp->swb_block);
		}
	/*
	 * Free swap management resources
	 */
	free((caddr_t)swp->sw_blocks, M_VMPGDATA);
	free((caddr_t)swp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
}

static int
swap_pager_getpage(pager, mlist, npages, sync)
	vm_pager_t pager;
	vm_page_t *mlist;
	int npages;
	boolean_t sync;
{
	register int rv;
#ifdef DIAGNOSTIC
	vm_page_t m;
	int i;
#endif

#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_getpage(%p, %p, %x, %x)\n",
		    pager, mlist, npages, sync);
#endif
#ifdef DIAGNOSTIC
	for (i = 0; i < npages; i++) {
		m = mlist[i];

		if (m->flags & PG_FAULTING)
			panic("swap_pager_getpage: page is already faulting");
		m->flags |= PG_FAULTING;
	}
#endif
	rv = swap_pager_io((sw_pager_t)pager->pg_data, mlist, npages, B_READ);
#ifdef DIAGNOSTIC
	for (i = 0; i < npages; i++) {
		m = mlist[i];

		m->flags &= ~PG_FAULTING;
	}
#endif
	return (rv);
}

static int
swap_pager_putpage(pager, mlist, npages, sync)
	vm_pager_t pager;
	vm_page_t *mlist;
	int npages;
	boolean_t sync;
{
	int flags;

#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_putpage(%p, %p, %x, %x)\n",
		    pager, mlist, npages, sync);
#endif
	if (pager == NULL) {
		swap_pager_clean(B_WRITE);
		return (VM_PAGER_OK);		/* ??? */
	}
	flags = B_WRITE;
	if (!sync)
		flags |= B_ASYNC;
	return (swap_pager_io((sw_pager_t)pager->pg_data, mlist, npages,
	    flags));
}

static boolean_t
swap_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	register sw_pager_t swp;
	register sw_blk_t swb;
	int ix;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOCBLK))
		printf("swpg_haspage(%p, %lx) ", pager, offset);
#endif
	swp = (sw_pager_t) pager->pg_data;
	ix = offset / dbtob(swp->sw_bsize);
	if (swp->sw_blocks == NULL || ix >= swp->sw_nblocks) {
#ifdef DEBUG
		if (swpagerdebug & (SDB_FAIL|SDB_FOLLOW|SDB_ALLOCBLK))
			printf("swpg_haspage: %p bad offset %lx, ix %x\n",
			    swp->sw_blocks, offset, ix);
#endif
		return (FALSE);
	}
	swb = &swp->sw_blocks[ix];
	if (swb->swb_block)
		ix = atop(offset % dbtob(swp->sw_bsize));
#ifdef DEBUG
	if (swpagerdebug & SDB_ALLOCBLK)
		printf("%p blk %x+%x ", swp->sw_blocks, swb->swb_block, ix);
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOCBLK))
		printf("-> %c\n",
		    "FT"[swb->swb_block && (swb->swb_mask & (1 << ix))]);
#endif
	if (swb->swb_block && (swb->swb_mask & (1 << ix)))
		return (TRUE);
	return (FALSE);
}

static void
swap_pager_cluster(pager, offset, loffset, hoffset)
	vm_pager_t	pager;
	vm_offset_t	offset;
	vm_offset_t	*loffset;
	vm_offset_t	*hoffset;
{
	sw_pager_t swp;
	register int bsize;
	vm_offset_t loff, hoff;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_CLUSTER))
		printf("swpg_cluster(%p, %lx) ", pager, offset);
#endif
	swp = (sw_pager_t) pager->pg_data;
	bsize = dbtob(swp->sw_bsize);
	if (bsize > swap_pager_maxcluster)
		bsize = swap_pager_maxcluster;

	loff = offset - (offset % bsize);
#ifdef DIAGNOSTIC
	if (loff >= swp->sw_osize)
		panic("swap_pager_cluster: bad offset");
#endif

	hoff = loff + bsize;
	if (hoff > swp->sw_osize)
		hoff = swp->sw_osize;

	*loffset = loff;
	*hoffset = hoff;
#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_CLUSTER))
		printf("returns [%lx-%lx]\n", loff, hoff);
#endif
}

/*
 * Scaled down version of swap().
 * Assumes that PAGE_SIZE < MAXPHYS; i.e. only one operation needed.
 * BOGUS:  lower level IO routines expect a KVA so we have to map our
 * provided physical page into the KVA to keep them happy.
 */
static int
swap_pager_io(swp, mlist, npages, flags)
	register sw_pager_t swp;
	vm_page_t *mlist;
	int npages;
	int flags;
{
	register struct buf *bp;
	register sw_blk_t swb;
	register int s;
	int ix;
	u_int mask;
	boolean_t rv;
	vm_offset_t kva, off;
	swp_clean_t spc;
	vm_page_t m;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return (VM_PAGER_FAIL);		/* XXX: correct return? */
	if (swpagerdebug & (SDB_FOLLOW|SDB_IO))
		printf("swpg_io(%p, %p, %x, %x)\n", swp, mlist, npages, flags);
	if (flags & B_READ) {
		if (flags & B_ASYNC)
			panic("swap_pager_io: cannot do ASYNC reads");
		if (npages != 1)
			panic("swap_pager_io: cannot do clustered reads");
	}
#endif

	/*
	 * First determine if the page exists in the pager if this is
	 * a sync read.  This quickly handles cases where we are
	 * following shadow chains looking for the top level object
	 * with the page.
	 */
	m = *mlist;
	off = m->offset + m->object->paging_offset;
	ix = off / dbtob(swp->sw_bsize);
	if (swp->sw_blocks == NULL || ix >= swp->sw_nblocks) {
#ifdef DEBUG
		if ((flags & B_READ) == 0 && (swpagerdebug & SDB_ANOM)) {
			printf("swap_pager_io: no swap block on write\n");
			return (VM_PAGER_BAD);
		}
#endif
		return (VM_PAGER_FAIL);
	}
	swb = &swp->sw_blocks[ix];
	off = off % dbtob(swp->sw_bsize);
	if ((flags & B_READ) &&
	    (swb->swb_block == 0 || (swb->swb_mask & (1 << atop(off))) == 0))
		return (VM_PAGER_FAIL);

	/*
	 * For reads (pageins) and synchronous writes, we clean up
	 * all completed async pageouts.
	 */
	if ((flags & B_ASYNC) == 0) {
		s = splbio();
		swap_pager_clean(flags&B_READ);
#ifdef DEBUG
		if (swpagerdebug & SDB_PARANOIA)
			swap_pager_clean_check(mlist, npages, flags&B_READ);
#endif
		splx(s);
	}
	/*
	 * For async writes (pageouts), we cleanup completed pageouts so
	 * that all available resources are freed.  Also tells us if this
	 * page is already being cleaned.  If it is, or no resources
	 * are available, we try again later.
	 */
	else {
		swap_pager_clean(B_WRITE);
#ifdef DEBUG
		if (swpagerdebug & SDB_PARANOIA)
			swap_pager_clean_check(mlist, npages, B_WRITE);
#endif
		if (swap_pager_free.tqh_first == NULL) {
#ifdef DEBUG
			if (swpagerdebug & SDB_FAIL)
				printf("%s: no available io headers\n",
				    "swap_pager_io");
#endif
			return (VM_PAGER_AGAIN);
		}
	}

	/*
	 * Allocate a swap block if necessary.
	 */
	if (swb->swb_block == 0) {
		swb->swb_block = rmalloc(swapmap, swp->sw_bsize);
		if (swb->swb_block == 0) {
#ifdef DEBUG
			if (swpagerdebug & SDB_FAIL)
				printf("swpg_io: rmalloc of %x failed\n",
				    swp->sw_bsize);
#endif
			/*
			 * XXX this is technically a resource shortage that
			 * should return AGAIN, but the situation isn't likely
			 * to be remedied just by delaying a little while and
			 * trying again (the pageout daemon's current response
			 * to AGAIN) so we just return FAIL.
			 */
			return (VM_PAGER_FAIL);
		}
#ifdef DEBUG
		if (swpagerdebug & (SDB_FULL|SDB_ALLOCBLK))
			printf("swpg_io: %p alloc blk %x at ix %x\n",
			    swp->sw_blocks, swb->swb_block, ix);
#endif
	}

	/*
	 * Allocate a kernel virtual address and initialize so that PTE
	 * is available for lower level IO drivers.
	 */
	kva = vm_pager_map_pages(mlist, npages, !(flags & B_ASYNC));
	if (kva == NULL) {
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("%s: no KVA space to map pages\n",
			    "swap_pager_io");
#endif
		return (VM_PAGER_AGAIN);
	}

	/*
	 * Get a swap buffer header and initialize it.
	 */
	s = splbio();
	while (bswlist.b_actf == NULL) {
#ifdef DEBUG
		if (swpagerdebug & SDB_IO)	/* XXX what should this be? */
			printf("swap_pager_io: wait on swbuf for %p (%d)\n",
			    m, flags);
#endif
		bswlist.b_flags |= B_WANTED;
		tsleep((caddr_t)&bswlist, PSWP+1, "swpgiobuf", 0);
	}
	bp = bswlist.b_actf;
	bswlist.b_actf = bp->b_actf;
	splx(s);
	bp->b_flags = B_BUSY | (flags & B_READ);
	bp->b_proc = &proc0;	/* XXX (but without B_PHYS set this is ok) */
	bp->b_data = (caddr_t)kva;
	bp->b_blkno = swb->swb_block + btodb(off);
	VHOLD(swapdev_vp);
	bp->b_vp = swapdev_vp;
	if (swapdev_vp->v_type == VBLK)
		bp->b_dev = swapdev_vp->v_rdev;
	bp->b_bcount = npages * PAGE_SIZE;

	/*
	 * For writes we set up additional buffer fields, record a pageout
	 * in progress and mark that these swap blocks are now allocated.
	 */
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_dirtyoff = 0;
		bp->b_dirtyend = npages * PAGE_SIZE;
		s = splbio();
		swp->sw_poip++;
		swapdev_vp->v_numoutput++;
		splx(s);
		mask = (~(~0 << npages)) << atop(off);
#ifdef DEBUG
		swap_pager_poip++;
		if (swpagerdebug & SDB_WRITE)
			printf("swpg_io: write: bp=%p swp=%p poip=%d\n",
			    bp, swp, swp->sw_poip);
		if ((swpagerdebug & SDB_ALLOCBLK) &&
		    (swb->swb_mask & mask) != mask)
			printf("swpg_io: %p write %d pages at %x+%lx\n",
			    swp->sw_blocks, npages, swb->swb_block, atop(off));
		if (swpagerdebug & SDB_CLUSTER)
			printf("swpg_io: off=%lx, npg=%x, mask=%x, bmask=%x\n",
			    off, npages, mask, swb->swb_mask);
#endif
		swp->sw_cnt += count_bits(mask & ~swb->swb_mask);
		swb->swb_mask |= mask;
	}
	/*
	 * If this is an async write we set up still more buffer fields
	 * and place a "cleaning" entry on the inuse queue.
	 */
	if ((flags & (B_READ|B_ASYNC)) == B_ASYNC) {
#ifdef DIAGNOSTIC
		if (swap_pager_free.tqh_first == NULL)
			panic("swpg_io: lost spc");
#endif
		spc = swap_pager_free.tqh_first;
		TAILQ_REMOVE(&swap_pager_free, spc, spc_list);
#ifdef DIAGNOSTIC
		if (spc->spc_flags != SPC_FREE)
			panic("swpg_io: bad free spc");
#endif
		spc->spc_flags = SPC_BUSY;
		spc->spc_bp = bp;
		spc->spc_swp = swp;
		spc->spc_kva = kva;
		/*
		 * Record the first page.  This allows swap_pager_clean
		 * to efficiently handle the common case of a single page.
		 * For clusters, it allows us to locate the object easily
		 * and we then reconstruct the rest of the mlist from spc_kva.
		 */
		spc->spc_m = m;
		spc->spc_npages = npages;
		bp->b_flags |= B_CALL;
		bp->b_iodone = swap_pager_iodone;
		s = splbio();
		TAILQ_INSERT_TAIL(&swap_pager_inuse, spc, spc_list);
		splx(s);
	}

	/*
	 * Finally, start the IO operation.
	 * If it is async we are all done, otherwise we must wait for
	 * completion and cleanup afterwards.
	 */
#ifdef DEBUG
	if (swpagerdebug & SDB_IO)
		printf("swpg_io: IO start: bp %p, db %lx, va %lx, pa %lx\n",
		    bp, swb->swb_block+btodb(off), kva, VM_PAGE_TO_PHYS(m));
#endif
	VOP_STRATEGY(bp);
	if ((flags & (B_READ|B_ASYNC)) == B_ASYNC) {
#ifdef DEBUG
		if (swpagerdebug & SDB_IO)
			printf("swpg_io:  IO started: bp %p\n", bp);
#endif
		return (VM_PAGER_PEND);
	}
	s = splbio();
#ifdef DEBUG
	if (flags & B_READ)
		swap_pager_piip++;
	else
		swap_pager_poip++;
#endif
	while ((bp->b_flags & B_DONE) == 0)
		(void) tsleep(bp, PVM, "swpgio", 0);
	if ((flags & B_READ) == 0)
		--swp->sw_poip;
#ifdef DEBUG
	if (flags & B_READ)
		--swap_pager_piip;
	else
		--swap_pager_poip;
#endif
	rv = (bp->b_flags & B_ERROR) ? VM_PAGER_ERROR : VM_PAGER_OK;
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_PAGET|B_UAREA|B_DIRTY);
	bp->b_actf = bswlist.b_actf;
	bswlist.b_actf = bp;
	if (bp->b_vp)
		brelvp(bp);
	if (bswlist.b_flags & B_WANTED) {
		bswlist.b_flags &= ~B_WANTED;
		wakeup(&bswlist);
	}
	if ((flags & B_READ) == 0 && rv == VM_PAGER_OK) {
		m->flags |= PG_CLEAN;
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	}
	splx(s);
#ifdef DEBUG
	if (swpagerdebug & SDB_IO)
		printf("swpg_io:  IO done: bp %p, rv %d\n", bp, rv);
	if ((swpagerdebug & SDB_FAIL) && rv == VM_PAGER_ERROR)
		printf("swpg_io: IO error\n");
#endif
	vm_pager_unmap_pages(kva, npages);
	return (rv);
}

static void
swap_pager_clean(rw)
	int rw;
{
	register swp_clean_t spc;
	register int s, i;
	vm_object_t object;
	vm_page_t m;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_clean(%x)\n", rw);
#endif

	for (;;) {
		/*
		 * Look up and removal from inuse list must be done
		 * at splbio() to avoid conflicts with swap_pager_iodone.
		 */
		s = splbio();
		for (spc = swap_pager_inuse.tqh_first;
		     spc != NULL;
		     spc = spc->spc_list.tqe_next) {
			/*
			 * If the operation is done, remove it from the
			 * list and process it.
			 *
			 * XXX if we can't get the object lock we also
			 * leave it on the list and try again later.
			 * Is there something better we could do?
			 */
			if ((spc->spc_flags & SPC_DONE) &&
			    vm_object_lock_try(spc->spc_m->object)) {
				TAILQ_REMOVE(&swap_pager_inuse, spc, spc_list);
				break;
			}
		}
		splx(s);

		/*
		 * No operations done, thats all we can do for now.
		 */
		if (spc == NULL)
			break;

		/*
		 * Found a completed operation so finish it off.
		 * Note: no longer at splbio since entry is off the list.
		 */
		m = spc->spc_m;
		object = m->object;

		/*
		 * Process each page in the cluster.
		 * The first page is explicitly kept in the cleaning
		 * entry, others must be reconstructed from the KVA.
		 */
		for (i = 0; i < spc->spc_npages; i++) {
			if (i)
				m = vm_pager_atop(spc->spc_kva + ptoa(i));
			/*
			 * If no error mark as clean and inform the pmap
			 * system.  If there was an error, mark as dirty
			 * so we will try again.
			 *
			 * XXX could get stuck doing this, should give up
			 * after awhile.
			 */
			if (spc->spc_flags & SPC_ERROR) {
				printf("%s: clean of page %lx failed\n",
				    "swap_pager_clean", VM_PAGE_TO_PHYS(m));
				m->flags |= PG_LAUNDRY;
			} else {
				m->flags |= PG_CLEAN;
				pmap_clear_modify(VM_PAGE_TO_PHYS(m));
			}
			m->flags &= ~PG_BUSY;
			PAGE_WAKEUP(m);
		}

		/*
		 * Done with the object, decrement the paging count
		 * and unlock it.
		 */
		vm_object_paging_end(object);
		vm_object_unlock(object);

		/*
		 * Free up KVM used and put the entry back on the list.
		 */
		vm_pager_unmap_pages(spc->spc_kva, spc->spc_npages);
		spc->spc_flags = SPC_FREE;
		TAILQ_INSERT_TAIL(&swap_pager_free, spc, spc_list);
#ifdef DEBUG
		if (swpagerdebug & SDB_WRITE)
			printf("swpg_clean: free spc %p\n", spc);
#endif
	}
}

#ifdef DEBUG
static void
swap_pager_clean_check(mlist, npages, rw)
	vm_page_t *mlist;
	int npages;
	int rw;
{
	register swp_clean_t spc;
	boolean_t bad;
	int i, j, s;
	vm_page_t m;

	if (panicstr)
		return;

	bad = FALSE;
	s = splbio();
	for (spc = swap_pager_inuse.tqh_first;
	     spc != NULL;
	     spc = spc->spc_list.tqe_next) {
		for (j = 0; j < spc->spc_npages; j++) {
			m = vm_pager_atop(spc->spc_kva + ptoa(j));
			for (i = 0; i < npages; i++)
				if (m == mlist[i]) {
					if (swpagerdebug & SDB_ANOM)
						printf(
		"swpg_clean_check: %s: page %p on list, flags %x\n",
		rw == B_WRITE ? "write" : "read", mlist[i], spc->spc_flags);
					bad = TRUE;
				}
		}
	}
	splx(s);
	if (bad)
		panic("swpg_clean_check");
}
#endif

static void
swap_pager_iodone(bp)
	register struct buf *bp;
{
	register swp_clean_t spc;
	daddr_t blk;
	int s;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_iodone(%p)\n", bp);
#endif
	s = splbio();
	for (spc = swap_pager_inuse.tqh_first;
	     spc != NULL;
	     spc = spc->spc_list.tqe_next)
		if (spc->spc_bp == bp)
			break;
#ifdef DIAGNOSTIC
	if (spc == NULL)
		panic("swap_pager_iodone: bp not found");
#endif

	spc->spc_flags &= ~SPC_BUSY;
	spc->spc_flags |= SPC_DONE;
	if (bp->b_flags & B_ERROR)
		spc->spc_flags |= SPC_ERROR;
	spc->spc_bp = NULL;
	blk = bp->b_blkno;

#ifdef DEBUG
	--swap_pager_poip;
	if (swpagerdebug & SDB_WRITE)
		printf("swpg_iodone: bp=%p swp=%p flags=%x spc=%p poip=%x\n",
		    bp, spc->spc_swp, spc->spc_swp->sw_flags,
		    spc, spc->spc_swp->sw_poip);
#endif

	spc->spc_swp->sw_poip--;
	if (spc->spc_swp->sw_flags & SW_WANTED) {
		spc->spc_swp->sw_flags &= ~SW_WANTED;
		wakeup(spc->spc_swp);
	}
		
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_PAGET|B_UAREA|B_DIRTY);
	bp->b_actf = bswlist.b_actf;
	bswlist.b_actf = bp;
	if (bp->b_vp)
		brelvp(bp);
	if (bswlist.b_flags & B_WANTED) {
		bswlist.b_flags &= ~B_WANTED;
		wakeup(&bswlist);
	}
	wakeup(&vm_pages_needed);
	splx(s);
}

/*
 *	swap_pager_remove:
 *
 *	This is called via the vm_pager_remove path and
 *	will remove any pages inside the range [from, to)
 *	backed by us.  It is assumed that both addresses
 *	are multiples of PAGE_SIZE.  The special case
 *	where TO is zero means: remove to end of object.
 */
static int
swap_pager_remove(pager, from, to)
	vm_pager_t pager;
	vm_offset_t from, to;
{
	sw_pager_t swp;
	sw_blk_t swb;
	int bsize, blk, bit, to_blk, to_bit, mask, cnt = 0;

#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_remove()\n");
#endif

	/*	Special case stupid ranges.	*/
	if (to > 0 && from >= to)
		return (0);

	swp = (sw_pager_t)pager->pg_data;

	/*
	 *	If we back no pages, just return.  XXX Can this
	 *	ever be the case?  At least all remove calls should
	 *	be through vm_object_remove_from_pager which also
	 *	deallocates the pager when it no longer backs any
	 *	pages.  Left is the initial case: can a swap-pager
	 *	be created without any pages put into it?
	 */
	if (swp->sw_cnt == 0)
		return (0);

	bsize = dbtob(swp->sw_bsize);
	blk = from / bsize;

	/*	Another fast one.. no blocks in range.	*/
	if (blk >= swp->sw_nblocks)
		return (0);
	bit = atop(from % bsize);

	/*
	 *	Deal with the special case with TO == 0.
	 *	XXX Perhaps the code might be improved if we
	 *	made to_blk & to_bit signify the inclusive end
	 *	of range instead (i.e. to - 1).
	 */
	if (to) {
		to_blk = to / bsize;
		if (to_blk >= swp->sw_nblocks) {
			to_blk = swp->sw_nblocks;
			to_bit = 0;
		} else
			to_bit = atop(to % bsize);
	} else {
		to_blk = swp->sw_nblocks;
		to_bit = 0;
	}

	/*
	 *	Loop over the range, remove pages as we find them.
	 *	If all pages in a block get freed, deallocate the
	 *	swap block as well.
	 */
	for (swb = &swp->sw_blocks[blk], mask = (1 << bit) - 1;
	    blk < to_blk || (blk == to_blk && to_bit);
	    blk++, swb++, mask = 0) {

		/*	Don't bother if the block is already cleared.  */
		if (swb->swb_block == 0)
			continue;

		/*
		 *	When coming to the end-block we need to
		 *	adjust the mask in the other end, as well as
		 *	ensuring this will be the last iteration.
		 */
		if (blk == to_blk) {
			mask |= ~((1 << to_bit) - 1);
			to_bit = 0;
		}

		/*	Count pages that will be removed.	*/
		cnt += count_bits(swb->swb_mask & ~mask);

		/*
		 *	Remove pages by applying our mask, and if this
		 *	means no pages are left in the block, free it.
		 */
		if ((swb->swb_mask &= mask) == 0) {
			rmfree(swapmap, swp->sw_bsize, swb->swb_block);
			swb->swb_block = 0;
		}
 	}

	/*	Adjust the page count and return the removed count.	*/
	swp->sw_cnt -= cnt;
#ifdef DIAGNOSTIC
	if (swp->sw_cnt < 0)
		panic("swap_pager_remove: sw_cnt < 0");
#endif
	return (cnt);
}

/*
 * swap_pager_next:
 *
 * This is called via the vm_pager_next path and
 * will return the offset of the next page (addresswise)
 * which this pager is backing.  If there are no more
 * pages we will return the size of the pager's managed
 * space (which by definition is larger than any page's
 * offset).
 */
static vm_offset_t
swap_pager_next(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	sw_pager_t swp;
	sw_blk_t swb;
	int bsize, blk, bit, to_blk, to_bit, mask;

#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_next()\n");
#endif

	swp = (sw_pager_t)pager->pg_data;

	/*
	 * If we back no pages, just return our size.  XXX Can
	 * this ever be the case?  At least all remove calls
	 * should be through vm_object_remove_from_pager which
	 * also deallocates the pager when it no longer backs any
	 * pages.  Left is the initial case: can a swap-pager
	 * be created without any pages put into it?
	 */
	if (swp->sw_cnt == 0)
		return (swp->sw_osize);

	bsize = dbtob(swp->sw_bsize);
	blk = offset / bsize;

	/* Another fast one.. no blocks in range.	*/
	if (blk >= swp->sw_nblocks)
		return (swp->sw_osize);
	bit = atop(offset % bsize);
	to_blk = swp->sw_osize / bsize;
	to_bit = atop(swp->sw_osize % bsize);

	/*
	 *	Loop over the remaining blocks, returning as soon
	 *	as we find a page.
	 */
	swb = &swp->sw_blocks[blk];
	mask = ~((1 << bit) - 1);
	for (;;) {
		if (blk == to_blk) {
			/*	Nothing to be done in this end-block?	*/
			if (to_bit == 0)
				break;
			mask &= (1 << to_bit) - 1; 
		}

		/*
		 *	Check this block for a backed page and return
		 *	its offset if there.
		 */
		mask &= swb->swb_mask;
		if (mask)
			return (blk * bsize + (ffs (mask) - 1) * PAGE_SIZE);

		/*
		 *	If we handled the end of range now, this
		 *	means we are ready.
		 */
		if (blk == to_blk)
			break;

		/*	Get on with the next block.	*/
		blk++;
		swb++;
		mask = ~0;
 	}
	return (swp->sw_osize);
}

/*
 *	swap_pager_count:
 *
 *	Just returns the count of pages backed by this pager.
 */
int
swap_pager_count(pager)
	vm_pager_t	pager;
{
#ifndef notyet
	return ((sw_pager_t)pager->pg_data)->sw_cnt;
#else
	sw_pager_t swp;
	sw_blk_t swb;
	int i, cnt = 0;

	swp = (sw_pager_t)pager->pg_data;
	if (swp->sw_blocks == NULL)
		return (0);
	for (i = 0; i < swp->sw_nblocks; i++)
		cnt += count_bits(swp->sw_blocks[i].swb_mask); 
	return (cnt);
#endif
}

/*
 *	count_bits:
 *
 *	Counts the number of set bits in a word.
 */
static int
count_bits(x)
	u_int	x;
{
	int	cnt = 0;

	while (x) {
		cnt += x & 1;
		x >>= 1;
	}
	return (cnt);
}
