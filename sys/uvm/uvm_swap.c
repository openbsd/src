/*	$NetBSD: uvm_swap.c,v 1.27 1999/03/30 16:07:47 chs Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: NetBSD: vm_swap.c,v 1.52 1997/12/02 13:47:37 pk Exp
 * from: Id: uvm_swap.c,v 1.1.2.42 1998/02/02 20:38:06 chuck Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/extent.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/syscallargs.h>
#include <sys/swap.h>

#include <vm/vm.h>
#include <vm/vm_conf.h>

#include <uvm/uvm.h>
#ifdef UVM_SWAP_ENCRYPT
#include <uvm/uvm_swap_encrypt.h>
#endif

#include <miscfs/specfs/specdev.h>

/*
 * uvm_swap.c: manage configuration and i/o to swap space.
 */

/*
 * swap space is managed in the following way:
 * 
 * each swap partition or file is described by a "swapdev" structure.
 * each "swapdev" structure contains a "swapent" structure which contains
 * information that is passed up to the user (via system calls).
 *
 * each swap partition is assigned a "priority" (int) which controls
 * swap parition usage.
 *
 * the system maintains a global data structure describing all swap
 * partitions/files.   there is a sorted LIST of "swappri" structures
 * which describe "swapdev"'s at that priority.   this LIST is headed
 * by the "swap_priority" global var.    each "swappri" contains a 
 * CIRCLEQ of "swapdev" structures at that priority.
 *
 * the system maintains a fixed pool of "swapbuf" structures for use
 * at swap i/o time.  a swapbuf includes a "buf" structure and an 
 * "aiodone" [we want to avoid malloc()'ing anything at swapout time
 * since memory may be low].
 *
 * locking:
 *  - swap_syscall_lock (sleep lock): this lock serializes the swapctl
 *    system call and prevents the swap priority list from changing
 *    while we are in the middle of a system call (e.g. SWAP_STATS).
 *  - uvm.swap_data_lock (simple_lock): this lock protects all swap data
 *    structures including the priority list, the swapdev structures,
 *    and the swapmap extent.
 *  - swap_buf_lock (simple_lock): this lock protects the free swapbuf
 *    pool.
 *
 * each swap device has the following info:
 *  - swap device in use (could be disabled, preventing future use)
 *  - swap enabled (allows new allocations on swap)
 *  - map info in /dev/drum
 *  - vnode pointer
 * for swap files only:
 *  - block size
 *  - max byte count in buffer
 *  - buffer
 *  - credentials to use when doing i/o to file
 *
 * userland controls and configures swap with the swapctl(2) system call.
 * the sys_swapctl performs the following operations:
 *  [1] SWAP_NSWAP: returns the number of swap devices currently configured
 *  [2] SWAP_STATS: given a pointer to an array of swapent structures 
 *	(passed in via "arg") of a size passed in via "misc" ... we load
 *	the current swap config into the array.
 *  [3] SWAP_ON: given a pathname in arg (could be device or file) and a
 *	priority in "misc", start swapping on it.
 *  [4] SWAP_OFF: as SWAP_ON, but stops swapping to a device
 *  [5] SWAP_CTL: changes the priority of a swap device (new priority in
 *	"misc")
 */

/*
 * SWAP_TO_FILES: allows swapping to plain files.
 */

#define SWAP_TO_FILES

/*
 * swapdev: describes a single swap partition/file
 *
 * note the following should be true:
 * swd_inuse <= swd_nblks  [number of blocks in use is <= total blocks]
 * swd_nblks <= swd_mapsize [because mapsize includes miniroot+disklabel]
 */
struct swapdev {
	struct swapent	swd_se;
#define	swd_dev		swd_se.se_dev		/* device id */
#define	swd_flags	swd_se.se_flags		/* flags:inuse/enable/fake */
#define	swd_priority	swd_se.se_priority	/* our priority */
#define	swd_inuse	swd_se.se_inuse		/* our priority */
#define	swd_nblks	swd_se.se_nblks		/* our priority */
	char			*swd_path;	/* saved pathname of device */
	int			swd_pathlen;	/* length of pathname */
	int			swd_npages;	/* #pages we can use */
	int			swd_npginuse;	/* #pages in use */
	int			swd_drumoffset;	/* page0 offset in drum */
	int			swd_drumsize;	/* #pages in drum */
	struct extent		*swd_ex;	/* extent for this swapdev */
	struct vnode		*swd_vp;	/* backing vnode */
	CIRCLEQ_ENTRY(swapdev)	swd_next;	/* priority circleq */

#ifdef SWAP_TO_FILES
	int			swd_bsize;	/* blocksize (bytes) */
	int			swd_maxactive;	/* max active i/o reqs */
	struct buf		swd_tab;	/* buffer list */
	struct ucred		*swd_cred;	/* cred for file access */
#endif
#ifdef UVM_SWAP_ENCRYPT
#define SWD_DCRYPT_SHIFT	5
#define SWD_DCRYPT_BITS		32
#define SWD_DCRYPT_MASK		(SWD_DCRYPT_BITS - 1)
#define SWD_DCRYPT_OFF(x)	((x) >> SWD_DCRYPT_SHIFT)
#define SWD_DCRYPT_BIT(x)	((x) & SWD_DCRYPT_MASK)
#define SWD_DCRYPT_SIZE(x)	(SWD_DCRYPT_OFF((x) + SWD_DCRYPT_MASK) * sizeof(u_int32_t))
	u_int32_t		*swd_decrypt;	/* bitmap for decryption */
#endif
};

/*
 * swap device priority entry; the list is kept sorted on `spi_priority'.
 */
struct swappri {
	int			spi_priority;     /* priority */
	CIRCLEQ_HEAD(spi_swapdev, swapdev)	spi_swapdev;
	/* circleq of swapdevs at this priority */
	LIST_ENTRY(swappri)	spi_swappri;      /* global list of pri's */
};

/*
 * swapbuf, swapbuffer plus async i/o info
 */
struct swapbuf {
	struct buf sw_buf;		/* a buffer structure */
	struct uvm_aiodesc sw_aio;	/* aiodesc structure, used if ASYNC */
	SIMPLEQ_ENTRY(swapbuf) sw_sq;	/* free list pointer */
};

/*
 * The following two structures are used to keep track of data transfers
 * on swap devices associated with regular files.
 * NOTE: this code is more or less a copy of vnd.c; we use the same
 * structure names here to ease porting..
 */
struct vndxfer {
	struct buf	*vx_bp;		/* Pointer to parent buffer */
	struct swapdev	*vx_sdp;
	int		vx_error;
	int		vx_pending;	/* # of pending aux buffers */
	int		vx_flags;
#define VX_BUSY		1
#define VX_DEAD		2
};

struct vndbuf {
	struct buf	vb_buf;
	struct vndxfer	*vb_xfer;
};


/*
 * We keep a of pool vndbuf's and vndxfer structures.
 */
struct pool *vndxfer_pool;
struct pool *vndbuf_pool;

#define	getvndxfer(vnx)	do {						\
	int s = splbio();						\
	vnx = (struct vndxfer *)					\
		pool_get(vndxfer_pool, PR_MALLOCOK|PR_WAITOK);		\
	splx(s);							\
} while (0)

#define putvndxfer(vnx) {						\
	pool_put(vndxfer_pool, (void *)(vnx));				\
}

#define	getvndbuf(vbp)	do {						\
	int s = splbio();						\
	vbp = (struct vndbuf *)						\
		pool_get(vndbuf_pool, PR_MALLOCOK|PR_WAITOK);		\
	splx(s);							\
} while (0)

#define putvndbuf(vbp) {						\
	pool_put(vndbuf_pool, (void *)(vbp));				\
}


/*
 * local variables
 */
static struct extent *swapmap;		/* controls the mapping of /dev/drum */
SIMPLEQ_HEAD(swapbufhead, swapbuf);
struct pool *swapbuf_pool;

/* list of all active swap devices [by priority] */
LIST_HEAD(swap_priority, swappri);
static struct swap_priority swap_priority;

/* locks */
lock_data_t swap_syscall_lock;

/*
 * prototypes
 */
static void		 swapdrum_add __P((struct swapdev *, int));
static struct swapdev	*swapdrum_getsdp __P((int));

static struct swapdev	*swaplist_find __P((struct vnode *, int));
static void		 swaplist_insert __P((struct swapdev *, 
					     struct swappri *, int));
static void		 swaplist_trim __P((void));

static int swap_on __P((struct proc *, struct swapdev *));
#ifdef SWAP_OFF_WORKS
static int swap_off __P((struct proc *, struct swapdev *));
#endif

#ifdef SWAP_TO_FILES
static void sw_reg_strategy __P((struct swapdev *, struct buf *, int));
static void sw_reg_iodone __P((struct buf *));
static void sw_reg_start __P((struct swapdev *));
#endif

static void uvm_swap_aiodone __P((struct uvm_aiodesc *));
static void uvm_swap_bufdone __P((struct buf *));
static int uvm_swap_io __P((struct vm_page **, int, int, int));

static void swapmount __P((void));

#ifdef UVM_SWAP_ENCRYPT
/* for swap encrypt */
boolean_t uvm_swap_allocpages __P((struct vm_page **, int));
void uvm_swap_freepages __P((struct vm_page **, int));
void uvm_swap_markdecrypt __P((struct swapdev *, int, int, int));
boolean_t uvm_swap_needdecrypt __P((struct swapdev *, int));
void uvm_swap_initcrypt __P((struct swapdev *, int));
#endif

/*
 * uvm_swap_init: init the swap system data structures and locks
 *
 * => called at boot time from init_main.c after the filesystems 
 *	are brought up (which happens after uvm_init())
 */
void
uvm_swap_init()
{
	UVMHIST_FUNC("uvm_swap_init");

	UVMHIST_CALLED(pdhist);
	/*
	 * first, init the swap list, its counter, and its lock.
	 * then get a handle on the vnode for /dev/drum by using
	 * the its dev_t number ("swapdev", from MD conf.c).
	 */

	LIST_INIT(&swap_priority);
	uvmexp.nswapdev = 0;
	lockinit(&swap_syscall_lock, PVM, "swapsys", 0, 0);
	simple_lock_init(&uvm.swap_data_lock);

	if (bdevvp(swapdev, &swapdev_vp))
		panic("uvm_swap_init: can't get vnode for swap device");

	/*
	 * create swap block resource map to map /dev/drum.   the range
	 * from 1 to INT_MAX allows 2 gigablocks of swap space.  note
	 * that block 0 is reserved (used to indicate an allocation 
	 * failure, or no allocation).
	 */
	swapmap = extent_create("swapmap", 1, INT_MAX,
				M_VMSWAP, 0, 0, EX_NOWAIT);
	if (swapmap == 0)
		panic("uvm_swap_init: extent_create failed");

	/*
	 * allocate our private pool of "swapbuf" structures (includes
	 * a "buf" structure).  ["nswbuf" comes from param.c and can
	 * be adjusted by MD code before we get here].
	 */

	swapbuf_pool =
		pool_create(sizeof(struct swapbuf), 0, 0, 0, "swp buf", 0,
			    NULL, NULL, 0);
	if (swapbuf_pool == NULL)
		panic("swapinit: pool_create failed");
	/* XXX - set a maximum on swapbuf_pool? */

	vndxfer_pool =
		pool_create(sizeof(struct vndxfer), 0, 0, 0, "swp vnx", 0,
			    NULL, NULL, 0);
	if (vndxfer_pool == NULL)
		panic("swapinit: pool_create failed");

	vndbuf_pool =
		pool_create(sizeof(struct vndbuf), 0, 0, 0, "swp vnd", 0,
			    NULL, NULL, 0);
	if (vndbuf_pool == NULL)
		panic("swapinit: pool_create failed");

	/*
	 * Setup the initial swap partition
	 */
	swapmount();

	/*
	 * done!
	 */
	UVMHIST_LOG(pdhist, "<- done", 0, 0, 0, 0);
}

#ifdef UVM_SWAP_ENCRYPT
void
uvm_swap_initcrypt_all(void)
{
	struct swapdev *sdp;
	struct swappri *spp;

	simple_lock(&uvm.swap_data_lock);

	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next)
			if (sdp->swd_decrypt == NULL)
				uvm_swap_initcrypt(sdp, sdp->swd_npages);
	}
	simple_unlock(&uvm.swap_data_lock);
}

void
uvm_swap_initcrypt(struct swapdev *sdp, int npages)
{
	/*
	 * keep information if a page needs to be decrypted when we get it
	 * from the swap device.
	 * We cannot chance a malloc later, if we are doing ASYNC puts,
	 * we may not call malloc with M_WAITOK.  This consumes only
	 * 8KB memory for a 256MB swap partition.
	 */
	sdp->swd_decrypt = malloc(SWD_DCRYPT_SIZE(npages), M_VMSWAP, M_WAITOK);
	bzero(sdp->swd_decrypt, SWD_DCRYPT_SIZE(npages));
}

boolean_t
uvm_swap_allocpages(struct vm_page **pps, int npages)
{
	int i, s;
	int minus, reserve;
	boolean_t fail;

	/* Estimate if we will succeed */
	s = uvm_lock_fpageq();

	minus = uvmexp.free - npages;
	reserve = uvmexp.reserve_kernel;
	fail = uvmexp.free - npages < uvmexp.reserve_kernel;

	uvm_unlock_fpageq(s);

	if (fail)
		return FALSE;

	/* Get new pages */
	for (i = 0; i < npages; i++) {
		pps[i] = uvm_pagealloc(NULL, 0, NULL, 0);
		if (pps[i] == NULL)
			break;
	}

	/* On failure free and return */
	if (i < npages) {
		uvm_swap_freepages(pps, i);
		return FALSE;
	}

	return TRUE;
}

void
uvm_swap_freepages(struct vm_page **pps, int npages)
{
	int i;

	uvm_lock_pageq();
	for (i = 0; i < npages; i++)
		uvm_pagefree(pps[i]);
	uvm_unlock_pageq();
}

/*
 * Mark pages on the swap device for later decryption
 */

void
uvm_swap_markdecrypt(struct swapdev *sdp, int startslot, int npages,
		     int decrypt)
{
	int pagestart, i;
	int off, bit;
	
	if (!sdp)
		return;

	pagestart = startslot - sdp->swd_drumoffset;
	for (i = 0; i < npages; i++, pagestart++) {
		off = SWD_DCRYPT_OFF(pagestart);
		bit = SWD_DCRYPT_BIT(pagestart);
		if (decrypt)
			/* pages read need decryption */
			sdp->swd_decrypt[off] |= 1 << bit;
		else 
			/* pages read do not need decryption */
			sdp->swd_decrypt[off] &= ~(1 << bit);
	}
}

/*
 * Check if the page that we got from disk needs to be decrypted
 */

boolean_t
uvm_swap_needdecrypt(struct swapdev *sdp, int off)
{
	if (!sdp)
		return FALSE;

	off -= sdp->swd_drumoffset;
	return sdp->swd_decrypt[SWD_DCRYPT_OFF(off)] & (1 << SWD_DCRYPT_BIT(off)) ?
		TRUE : FALSE;
}
#endif /* UVM_SWAP_ENCRYPT */
/*
 * swaplist functions: functions that operate on the list of swap
 * devices on the system.
 */

/*
 * swaplist_insert: insert swap device "sdp" into the global list
 *
 * => caller must hold both swap_syscall_lock and uvm.swap_data_lock
 * => caller must provide a newly malloc'd swappri structure (we will
 *	FREE it if we don't need it... this it to prevent malloc blocking
 *	here while adding swap)
 */
static void
swaplist_insert(sdp, newspp, priority)
	struct swapdev *sdp;
	struct swappri *newspp;
	int priority;
{
	struct swappri *spp, *pspp;
	UVMHIST_FUNC("swaplist_insert"); UVMHIST_CALLED(pdhist);

	/*
	 * find entry at or after which to insert the new device.
	 */
	for (pspp = NULL, spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		if (priority <= spp->spi_priority)
			break;
		pspp = spp;
	}

	/*
	 * new priority?
	 */
	if (spp == NULL || spp->spi_priority != priority) {
		spp = newspp;  /* use newspp! */
		UVMHIST_LOG(pdhist, "created new swappri = %d", priority, 0, 0, 0);

		spp->spi_priority = priority;
		CIRCLEQ_INIT(&spp->spi_swapdev);

		if (pspp)
			LIST_INSERT_AFTER(pspp, spp, spi_swappri);
		else
			LIST_INSERT_HEAD(&swap_priority, spp, spi_swappri);
	} else {
	  	/* we don't need a new priority structure, free it */
		FREE(newspp, M_VMSWAP);
	}

	/*
	 * priority found (or created).   now insert on the priority's
	 * circleq list and bump the total number of swapdevs.
	 */
	sdp->swd_priority = priority;
	CIRCLEQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
	uvmexp.nswapdev++;

	/*
	 * done!
	 */
}

/*
 * swaplist_find: find and optionally remove a swap device from the
 *	global list.
 *
 * => caller must hold both swap_syscall_lock and uvm.swap_data_lock
 * => we return the swapdev we found (and removed)
 */
static struct swapdev *
swaplist_find(vp, remove)
	struct vnode *vp;
	boolean_t remove;
{
	struct swapdev *sdp;
	struct swappri *spp;

	/*
	 * search the lists for the requested vp
	 */
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next)
			if (sdp->swd_vp == vp) {
				if (remove) {
					CIRCLEQ_REMOVE(&spp->spi_swapdev,
					    sdp, swd_next);
					uvmexp.nswapdev--;
				}
				return(sdp);
			}
	}
	return (NULL);
}


/*
 * swaplist_trim: scan priority list for empty priority entries and kill
 *	them.
 *
 * => caller must hold both swap_syscall_lock and uvm.swap_data_lock
 */
static void
swaplist_trim()
{
	struct swappri *spp, *nextspp;

	for (spp = swap_priority.lh_first; spp != NULL; spp = nextspp) {
		nextspp = spp->spi_swappri.le_next;
		if (spp->spi_swapdev.cqh_first != (void *)&spp->spi_swapdev)
			continue;
		LIST_REMOVE(spp, spi_swappri);
		free((caddr_t)spp, M_VMSWAP);
	}
}

/*
 * swapdrum_add: add a "swapdev"'s blocks into /dev/drum's area.
 *
 * => caller must hold swap_syscall_lock
 * => uvm.swap_data_lock should be unlocked (we may sleep)
 */
static void
swapdrum_add(sdp, npages)
	struct swapdev *sdp;
	int	npages;
{
	u_long result;

	if (extent_alloc(swapmap, npages, EX_NOALIGN, EX_NOBOUNDARY,
	    EX_WAITOK, &result))
		panic("swapdrum_add");

	sdp->swd_drumoffset = result;
	sdp->swd_drumsize = npages;
}

/*
 * swapdrum_getsdp: given a page offset in /dev/drum, convert it back
 *	to the "swapdev" that maps that section of the drum.
 *
 * => each swapdev takes one big contig chunk of the drum
 * => caller must hold uvm.swap_data_lock
 */
static struct swapdev *
swapdrum_getsdp(pgno)
	int pgno;
{
	struct swapdev *sdp;
	struct swappri *spp;
	
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next)
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next)
			if (pgno >= sdp->swd_drumoffset &&
			    pgno < (sdp->swd_drumoffset + sdp->swd_drumsize)) {
				return sdp;
			}
	return NULL;
}


/*
 * sys_swapctl: main entry point for swapctl(2) system call
 * 	[with two helper functions: swap_on and swap_off]
 */
int
sys_swapctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapctl_args /* {
		syscallarg(int) cmd;
		syscallarg(void *) arg;
		syscallarg(int) misc;
	} */ *uap = (struct sys_swapctl_args *)v;
	struct vnode *vp;
	struct nameidata nd;
	struct swappri *spp;
	struct swapdev *sdp;
	struct swapent *sep;
	char	userpath[PATH_MAX + 1];
	size_t	len;
	int	count, error, misc;
	int	priority;
	UVMHIST_FUNC("sys_swapctl"); UVMHIST_CALLED(pdhist);

	misc = SCARG(uap, misc);

	/*
	 * ensure serialized syscall access by grabbing the swap_syscall_lock
	 */
	lockmgr(&swap_syscall_lock, LK_EXCLUSIVE, (void *)0, p);
	
	/*
	 * we handle the non-priv NSWAP and STATS request first.
	 *
	 * SWAP_NSWAP: return number of config'd swap devices 
	 * [can also be obtained with uvmexp sysctl]
	 */
	if (SCARG(uap, cmd) == SWAP_NSWAP) {
		UVMHIST_LOG(pdhist, "<- done SWAP_NSWAP=%d", uvmexp.nswapdev,
		    0, 0, 0);
		*retval = uvmexp.nswapdev;
		error = 0;
		goto out;
	}

	/*
	 * SWAP_STATS: get stats on current # of configured swap devs
	 *
	 * note that the swap_priority list can't change as long 
	 * as we are holding the swap_syscall_lock.  we don't want
	 * to grab the uvm.swap_data_lock because we may fault&sleep during 
	 * copyout() and we don't want to be holding that lock then!
	 */
	if (SCARG(uap, cmd) == SWAP_STATS
#if defined(COMPAT_13)
	    || SCARG(uap, cmd) == SWAP_OSTATS
#endif
	    ) {
		sep = (struct swapent *)SCARG(uap, arg);
		count = 0;

		for (spp = swap_priority.lh_first; spp != NULL;
		    spp = spp->spi_swappri.le_next) {
			for (sdp = spp->spi_swapdev.cqh_first;
			     sdp != (void *)&spp->spi_swapdev && misc-- > 0;
			     sdp = sdp->swd_next.cqe_next) {
			  	/*
				 * backwards compatibility for system call.
				 * note that we use 'struct oswapent' as an
				 * overlay into both 'struct swapdev' and
				 * the userland 'struct swapent', as we
				 * want to retain backwards compatibility
				 * with NetBSD 1.3.
				 */
				sdp->swd_inuse = 
				    btodb(sdp->swd_npginuse << PAGE_SHIFT);
				error = copyout((caddr_t)&sdp->swd_se,
				    (caddr_t)sep, sizeof(struct swapent));

				/* now copy out the path if necessary */
#if defined(COMPAT_13)
				if (error == 0 && SCARG(uap, cmd) == SWAP_STATS)
#else
				if (error == 0)
#endif
					error = copyout((caddr_t)sdp->swd_path,
					    (caddr_t)&sep->se_path,
					    sdp->swd_pathlen);

				if (error)
					goto out;
				count++;
#if defined(COMPAT_13)
				if (SCARG(uap, cmd) == SWAP_OSTATS)
					((struct oswapent *)sep)++;
				else
#endif
					sep++;
			}
		}

		UVMHIST_LOG(pdhist, "<- done SWAP_STATS", 0, 0, 0, 0);

		*retval = count;
		error = 0;
		goto out;
	} 

	/*
	 * all other requests require superuser privs.   verify.
	 */
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		goto out;

	/*
	 * at this point we expect a path name in arg.   we will
	 * use namei() to gain a vnode reference (vref), and lock
	 * the vnode (VOP_LOCK).
	 *
	 * XXX: a NULL arg means use the root vnode pointer (e.g. for
	 * miniroot)
	 */
	if (SCARG(uap, arg) == NULL) {
		vp = rootvp;		/* miniroot */
		if (vget(vp, LK_EXCLUSIVE, p)) {
			error = EBUSY;
			goto out;
		}
		if (SCARG(uap, cmd) == SWAP_ON &&
		    copystr("miniroot", userpath, sizeof userpath, &len))
			panic("swapctl: miniroot copy failed");
	} else {
		int	space;
		char	*where;

		if (SCARG(uap, cmd) == SWAP_ON) {
			if ((error = copyinstr(SCARG(uap, arg), userpath,
			    sizeof userpath, &len)))
				goto out;
			space = UIO_SYSSPACE;
			where = userpath;
		} else {
			space = UIO_USERSPACE;
			where = (char *)SCARG(uap, arg);
		}
		NDINIT(&nd, LOOKUP, FOLLOW|LOCKLEAF, space, where, p);
		if ((error = namei(&nd)))
			goto out;
		vp = nd.ni_vp;
	}
	/* note: "vp" is referenced and locked */

	error = 0;		/* assume no error */
	switch(SCARG(uap, cmd)) {
	case SWAP_DUMPDEV:
		if (vp->v_type != VBLK) {
			error = ENOTBLK;
			goto out;
		}
		dumpdev = vp->v_rdev;
		
		break;

	case SWAP_CTL:
		/*
		 * get new priority, remove old entry (if any) and then
		 * reinsert it in the correct place.  finally, prune out
		 * any empty priority structures.
		 */
		priority = SCARG(uap, misc);
		spp = (struct swappri *)
			malloc(sizeof *spp, M_VMSWAP, M_WAITOK);
		simple_lock(&uvm.swap_data_lock);
		if ((sdp = swaplist_find(vp, 1)) == NULL) {
			error = ENOENT;
		} else {
			swaplist_insert(sdp, spp, priority);
			swaplist_trim();
		}
		simple_unlock(&uvm.swap_data_lock);
		if (error)
			free(spp, M_VMSWAP);
		break;

	case SWAP_ON:
		/*
		 * check for duplicates.   if none found, then insert a
		 * dummy entry on the list to prevent someone else from
		 * trying to enable this device while we are working on
		 * it.
		 */
		priority = SCARG(uap, misc);
		simple_lock(&uvm.swap_data_lock);
		if ((sdp = swaplist_find(vp, 0)) != NULL) {
			error = EBUSY;
			simple_unlock(&uvm.swap_data_lock);
			break;
		}
		sdp = (struct swapdev *)
			malloc(sizeof *sdp, M_VMSWAP, M_WAITOK);
		spp = (struct swappri *)
			malloc(sizeof *spp, M_VMSWAP, M_WAITOK);
		bzero(sdp, sizeof(*sdp));
		sdp->swd_flags = SWF_FAKE;	/* placeholder only */
		sdp->swd_vp = vp;
		sdp->swd_dev = (vp->v_type == VBLK) ? vp->v_rdev : NODEV;
#ifdef SWAP_TO_FILES
		/*
		 * XXX Is NFS elaboration necessary?
		 */
		if (vp->v_type == VREG)
			sdp->swd_cred = crdup(p->p_ucred);
#endif
		swaplist_insert(sdp, spp, priority);
		simple_unlock(&uvm.swap_data_lock);

		sdp->swd_pathlen = len;
		sdp->swd_path = malloc(sdp->swd_pathlen, M_VMSWAP, M_WAITOK);
		if (copystr(userpath, sdp->swd_path, sdp->swd_pathlen, 0) != 0)
			panic("swapctl: copystr");
		/*
		 * we've now got a FAKE placeholder in the swap list.
		 * now attempt to enable swap on it.  if we fail, undo
		 * what we've done and kill the fake entry we just inserted.
		 * if swap_on is a success, it will clear the SWF_FAKE flag
		 */
		if ((error = swap_on(p, sdp)) != 0) {
			simple_lock(&uvm.swap_data_lock);
			(void) swaplist_find(vp, 1);  /* kill fake entry */
			swaplist_trim();
			simple_unlock(&uvm.swap_data_lock);
#ifdef SWAP_TO_FILES
			if (vp->v_type == VREG)
				crfree(sdp->swd_cred);
#endif
			free(sdp->swd_path, M_VMSWAP);
			free((caddr_t)sdp, M_VMSWAP);
			break;
		}

		/*
		 * got it!   now add a second reference to vp so that
		 * we keep a reference to the vnode after we return.
		 */
		vref(vp);
		break;

	case SWAP_OFF:
		UVMHIST_LOG(pdhist, "someone is using SWAP_OFF...??", 0,0,0,0);
#ifdef SWAP_OFF_WORKS
		/*
		 * find the entry of interest and ensure it is enabled.
		 */
		simple_lock(&uvm.swap_data_lock);
		if ((sdp = swaplist_find(vp, 0)) == NULL) {
			simple_unlock(&uvm.swap_data_lock);
			error = ENXIO;
			break;
		}
		/*
		 * If a device isn't in use or enabled, we
		 * can't stop swapping from it (again).
		 */
		if ((sdp->swd_flags & (SWF_INUSE|SWF_ENABLE)) == 0) {
			simple_unlock(&uvm.swap_data_lock);
			error = EBUSY;
			break;
		}
		/* XXXCDC: should we call with list locked or unlocked? */
		if ((error = swap_off(p, sdp)) != 0)
			break;
		/* XXXCDC: might need relock here */

		/*
		 * now we can kill the entry.
		 */
		if ((sdp = swaplist_find(vp, 1)) == NULL) {
			error = ENXIO;
			break;
		}
		simple_unlock(&uvm.swap_data_lock);
		free((caddr_t)sdp, M_VMSWAP);
#else
		error = EINVAL;
#endif
		break;

	default:
		UVMHIST_LOG(pdhist, "unhandled command: %#x",
		    SCARG(uap, cmd), 0, 0, 0);
		error = EINVAL;
	}

	/*
	 * done!   use vput to drop our reference and unlock
	 */
	vput(vp);
out:
	lockmgr(&swap_syscall_lock, LK_RELEASE, (void *)0, p);

	UVMHIST_LOG(pdhist, "<- done!  error=%d", error, 0, 0, 0);
	return (error);
}

/*
 * swap_on: attempt to enable a swapdev for swapping.   note that the
 *	swapdev is already on the global list, but disabled (marked
 *	SWF_FAKE).
 *
 * => we avoid the start of the disk (to protect disk labels)
 * => we also avoid the miniroot, if we are swapping to root.
 * => caller should leave uvm.swap_data_lock unlocked, we may lock it
 *	if needed.
 */
static int
swap_on(p, sdp)
	struct proc *p;
	struct swapdev *sdp;
{
	static int count = 0;	/* static */
	struct vnode *vp;
	int error, npages, nblocks, size;
	long addr;
#ifdef SWAP_TO_FILES
	struct vattr va;
#endif
#if defined(NFSCLIENT)
	extern int (**nfsv2_vnodeop_p) __P((void *));
#endif /* defined(NFSCLIENT) */
	dev_t dev;
	char *name;
	UVMHIST_FUNC("swap_on"); UVMHIST_CALLED(pdhist);

	/*
	 * we want to enable swapping on sdp.   the swd_vp contains
	 * the vnode we want (locked and ref'd), and the swd_dev
	 * contains the dev_t of the file, if it a block device.
	 */

	vp = sdp->swd_vp;
	dev = sdp->swd_dev;

	/*
	 * open the swap file (mostly useful for block device files to
	 * let device driver know what is up).
	 *
	 * we skip the open/close for root on swap because the root
	 * has already been opened when root was mounted (mountroot).
	 */
	if (vp != rootvp) {
		if ((error = VOP_OPEN(vp, FREAD|FWRITE, p->p_ucred, p)))
			return (error);
	}

	/* XXX this only works for block devices */
	UVMHIST_LOG(pdhist, "  dev=%d, major(dev)=%d", dev, major(dev), 0,0);

	/*
	 * we now need to determine the size of the swap area.   for
	 * block specials we can call the d_psize function.
	 * for normal files, we must stat [get attrs].
	 *
	 * we put the result in nblks.
	 * for normal files, we also want the filesystem block size
	 * (which we get with statfs).
	 */
	switch (vp->v_type) {
	case VBLK:
		if (bdevsw[major(dev)].d_psize == 0 ||
		    (nblocks = (*bdevsw[major(dev)].d_psize)(dev)) == -1) {
			error = ENXIO;
			goto bad;
		}
		break;

#ifdef SWAP_TO_FILES
	case VREG:
		if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)))
			goto bad;
		nblocks = (int)btodb(va.va_size);
		if ((error =
		     VFS_STATFS(vp->v_mount, &vp->v_mount->mnt_stat, p)) != 0)
			goto bad;

		sdp->swd_bsize = vp->v_mount->mnt_stat.f_iosize;
		/*
		 * limit the max # of outstanding I/O requests we issue
		 * at any one time.   take it easy on NFS servers.
		 */
#if defined(NFSCLIENT)
		if (vp->v_op == nfsv2_vnodeop_p)
			sdp->swd_maxactive = 2; /* XXX */
		else
#endif /* defined(NFSCLIENT) */
			sdp->swd_maxactive = 8; /* XXX */
		break;
#endif

	default:
		error = ENXIO;
		goto bad;
	}

	/*
	 * save nblocks in a safe place and convert to pages.
	 */

	sdp->swd_nblks = nblocks;
	npages = dbtob((u_int64_t)nblocks) >> PAGE_SHIFT;

	/*
	 * for block special files, we want to make sure that leave
	 * the disklabel and bootblocks alone, so we arrange to skip
	 * over them (randomly choosing to skip PAGE_SIZE bytes).
	 * note that because of this the "size" can be less than the
	 * actual number of blocks on the device.
	 */
	if (vp->v_type == VBLK) {
		/* we use pages 1 to (size - 1) [inclusive] */
		size = npages - 1;
		addr = 1;
	} else {
		/* we use pages 0 to (size - 1) [inclusive] */
		size = npages;
		addr = 0;
	}

	/*
	 * make sure we have enough blocks for a reasonable sized swap
	 * area.   we want at least one page.
	 */

	if (size < 1) {
		UVMHIST_LOG(pdhist, "  size <= 1!!", 0, 0, 0, 0);
		error = EINVAL;
		goto bad;
	}

	UVMHIST_LOG(pdhist, "  dev=%x: size=%d addr=%ld\n", dev, size, addr, 0);

	/*
	 * now we need to allocate an extent to manage this swap device
	 */
	name = malloc(12, M_VMSWAP, M_WAITOK);
	sprintf(name, "swap0x%04x", count++);

	/* note that extent_create's 3rd arg is inclusive, thus "- 1" */
	sdp->swd_ex = extent_create(name, 0, npages - 1, M_VMSWAP,
				    0, 0, EX_WAITOK);
	/* allocate the `saved' region from the extent so it won't be used */
	if (addr) {
		if (extent_alloc_region(sdp->swd_ex, 0, addr, EX_WAITOK))
			panic("disklabel region");
		sdp->swd_npginuse += addr;
		simple_lock(&uvm.swap_data_lock);
		uvmexp.swpginuse += addr;
		uvmexp.swpgonly += addr;
		simple_unlock(&uvm.swap_data_lock);
	}

	/*
	 * if the vnode we are swapping to is the root vnode 
	 * (i.e. we are swapping to the miniroot) then we want
	 * to make sure we don't overwrite it.   do a statfs to 
	 * find its size and skip over it.
	 */
	if (vp == rootvp) {
		struct mount *mp;
		struct statfs *sp;
		int rootblocks, rootpages;

		mp = rootvnode->v_mount;
		sp = &mp->mnt_stat;
		rootblocks = sp->f_blocks * btodb(sp->f_bsize);
		rootpages = round_page(dbtob(rootblocks)) >> PAGE_SHIFT;
		if (rootpages > npages)
			panic("swap_on: miniroot larger than swap?");

		if (extent_alloc_region(sdp->swd_ex, addr, 
					rootpages, EX_WAITOK))
			panic("swap_on: unable to preserve miniroot");

		simple_lock(&uvm.swap_data_lock);
		sdp->swd_npginuse += (rootpages - addr);
		uvmexp.swpginuse += (rootpages - addr);
		uvmexp.swpgonly += (rootpages - addr);
		simple_unlock(&uvm.swap_data_lock);

		printf("Preserved %d pages of miniroot ", rootpages);
		printf("leaving %d pages of swap\n", size - rootpages);
	}

#ifdef UVM_SWAP_ENCRYPT
	if (uvm_doswapencrypt)
		uvm_swap_initcrypt(sdp, npages);
#endif
	/*
	 * now add the new swapdev to the drum and enable.
	 */
	simple_lock(&uvm.swap_data_lock);
	swapdrum_add(sdp, npages);
	sdp->swd_npages = npages;
	sdp->swd_flags &= ~SWF_FAKE;	/* going live */
	sdp->swd_flags |= (SWF_INUSE|SWF_ENABLE);
	simple_unlock(&uvm.swap_data_lock);
	uvmexp.swpages += npages;

	/*
	 * add anon's to reflect the swap space we added
	 */
	uvm_anon_add(size);

#if 0
	/*
	 * At this point we could arrange to reserve memory for the
	 * swap buffer pools.
	 *
	 * I don't think this is necessary, since swapping starts well
	 * ahead of serious memory deprivation and the memory resource
	 * pools hold on to actively used memory. This should ensure
	 * we always have some resources to continue operation.
	 */

	int s = splbio();
	int n = 8 * sdp->swd_maxactive;

	(void)pool_prime(swapbuf_pool, n, 0);

	if (vp->v_type == VREG) {
		/* Allocate additional vnx and vnd buffers */
		/*
		 * Allocation Policy:
		 *	(8  * swd_maxactive) vnx headers per swap dev
		 *	(16 * swd_maxactive) vnd buffers per swap dev
		 */

		n = 8 * sdp->swd_maxactive;
		(void)pool_prime(vndxfer_pool, n, 0);

		n = 16 * sdp->swd_maxactive;
		(void)pool_prime(vndbuf_pool, n, 0);
	}
	splx(s);
#endif

	return (0);

bad:
	/*
	 * failure: close device if necessary and return error.
	 */
	if (vp != rootvp)
		(void)VOP_CLOSE(vp, FREAD|FWRITE, p->p_ucred, p);
	return (error);
}

#ifdef SWAP_OFF_WORKS
/*
 * swap_off: stop swapping on swapdev
 *
 * XXXCDC: what conditions go here?
 */
static int
swap_off(p, sdp)
	struct proc *p;
	struct swapdev *sdp;
{
	char	*name;
	UVMHIST_FUNC("swap_off"); UVMHIST_CALLED(pdhist);

	/* turn off the enable flag */
	sdp->swd_flags &= ~SWF_ENABLE;

	UVMHIST_LOG(pdhist, "  dev=%x", sdp->swd_dev);

	/*
	 * XXX write me
	 *
	 * the idea is to find out which processes are using this swap
	 * device, and page them all in.
	 *
	 * eventually, we should try to move them out to other swap areas
	 * if available.
	 *
	 * The alternative is to create a redirection map for this swap
	 * device.  This should work by moving all the pages of data from
	 * the ex-swap device to another one, and making an entry in the
	 * redirection map for it.  locking is going to be important for
	 * this!
	 *
	 * XXXCDC: also need to shrink anon pool
	 */

	/* until the above code is written, we must ENODEV */
	return ENODEV;

#ifdef UVM_SWAP_ENCRYPT
	if (sdp->swd_decrypt)
		free(sdp->swd_decrypt);
#endif
	extent_free(swapmap, sdp->swd_mapoffset, sdp->swd_mapsize, EX_WAITOK);
	name = sdp->swd_ex->ex_name;
	extent_destroy(sdp->swd_ex);
	free(name, M_VMSWAP);
	free((caddr_t)sdp->swd_ex, M_VMSWAP);
	if (sdp->swp_vp != rootvp)
		(void) VOP_CLOSE(sdp->swd_vp, FREAD|FWRITE, p->p_ucred, p);
	if (sdp->swd_vp)
		vrele(sdp->swd_vp);
	free((caddr_t)sdp, M_VMSWAP);
	return (0);
}
#endif

/*
 * /dev/drum interface and i/o functions
 */

/*
 * swread: the read function for the drum (just a call to physio)
 */
/*ARGSUSED*/
int
swread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	UVMHIST_FUNC("swread"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  dev=%x offset=%qx", dev, uio->uio_offset, 0, 0);
	return (physio(swstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 * swwrite: the write function for the drum (just a call to physio)
 */
/*ARGSUSED*/
int
swwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	UVMHIST_FUNC("swwrite"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  dev=%x offset=%qx", dev, uio->uio_offset, 0, 0);
	return (physio(swstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * swstrategy: perform I/O on the drum
 *
 * => we must map the i/o request from the drum to the correct swapdev.
 */
void
swstrategy(bp)
	struct buf *bp;
{
	struct swapdev *sdp;
	struct vnode *vp;
	int s, pageno, bn;
	UVMHIST_FUNC("swstrategy"); UVMHIST_CALLED(pdhist);

	/*
	 * convert block number to swapdev.   note that swapdev can't
	 * be yanked out from under us because we are holding resources
	 * in it (i.e. the blocks we are doing I/O on).
	 */
	pageno = dbtob(bp->b_blkno) >> PAGE_SHIFT;
	simple_lock(&uvm.swap_data_lock);
	sdp = swapdrum_getsdp(pageno);
	simple_unlock(&uvm.swap_data_lock);
	if (sdp == NULL) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		UVMHIST_LOG(pdhist, "  failed to get swap device", 0, 0, 0, 0);
		return;
	}

	/*
	 * convert drum page number to block number on this swapdev.
	 */

	pageno = pageno - sdp->swd_drumoffset;	/* page # on swapdev */
	bn = btodb(pageno << PAGE_SHIFT);	/* convert to diskblock */

	UVMHIST_LOG(pdhist, "  %s: mapoff=%x bn=%x bcount=%ld\n",
		((bp->b_flags & B_READ) == 0) ? "write" : "read",
		sdp->swd_drumoffset, bn, bp->b_bcount);


	/*
	 * for block devices we finish up here.
	 * for regular files we have to do more work which we deligate
	 * to sw_reg_strategy().
	 */

	switch (sdp->swd_vp->v_type) {
	default:
		panic("swstrategy: vnode type 0x%x", sdp->swd_vp->v_type);
	case VBLK:

		/*
		 * must convert "bp" from an I/O on /dev/drum to an I/O
		 * on the swapdev (sdp).
		 */
		s = splbio();
		bp->b_blkno = bn;		/* swapdev block number */
		vp = sdp->swd_vp;		/* swapdev vnode pointer */
		bp->b_dev = sdp->swd_dev;	/* swapdev dev_t */
		VHOLD(vp);			/* "hold" swapdev vp for i/o */

		/*
		 * if we are doing a write, we have to redirect the i/o on
		 * drum's v_numoutput counter to the swapdevs.
		 */
		if ((bp->b_flags & B_READ) == 0) {
			vwakeup(bp);	/* kills one 'v_numoutput' on drum */
			vp->v_numoutput++;	/* put it on swapdev */
		}

		/* 
		 * dissassocate buffer with /dev/drum vnode 
		 * [could be null if buf was from physio]
		 */
		if (bp->b_vp != NULLVP)
			brelvp(bp);

		/* 
		 * finally plug in swapdev vnode and start I/O
		 */
		bp->b_vp = vp;
		splx(s);
		VOP_STRATEGY(bp);
		return;
#ifdef SWAP_TO_FILES
	case VREG:
		/*
		 * deligate to sw_reg_strategy function.
		 */
		sw_reg_strategy(sdp, bp, bn);
		return;
#endif
	}
	/* NOTREACHED */
}

#ifdef SWAP_TO_FILES
/*
 * sw_reg_strategy: handle swap i/o to regular files
 */
static void
sw_reg_strategy(sdp, bp, bn)
	struct swapdev	*sdp;
	struct buf	*bp;
	int		bn;
{
	struct vnode	*vp;
	struct vndxfer	*vnx;
	daddr_t		nbn, byteoff;
	caddr_t		addr;
	int		s, off, nra, error, sz, resid;
	UVMHIST_FUNC("sw_reg_strategy"); UVMHIST_CALLED(pdhist);

	/*
	 * allocate a vndxfer head for this transfer and point it to
	 * our buffer.
	 */
	getvndxfer(vnx);
	vnx->vx_flags = VX_BUSY;
	vnx->vx_error = 0;
	vnx->vx_pending = 0;
	vnx->vx_bp = bp;
	vnx->vx_sdp = sdp;

	/*
	 * setup for main loop where we read filesystem blocks into
	 * our buffer.
	 */
	error = 0;
	bp->b_resid = bp->b_bcount;	/* nothing transfered yet! */
	addr = bp->b_data;		/* current position in buffer */
	byteoff = dbtob(bn);

	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vndbuf	*nbp;

		/*
		 * translate byteoffset into block number.  return values:
		 *   vp = vnode of underlying device
		 *  nbn = new block number (on underlying vnode dev)
		 *  nra = num blocks we can read-ahead (excludes requested
		 *	block)
		 */
		nra = 0;
		error = VOP_BMAP(sdp->swd_vp, byteoff / sdp->swd_bsize,
				 	&vp, &nbn, &nra);

		if (error == 0 && (long)nbn == -1) {
			/* 
			 * this used to just set error, but that doesn't
			 * do the right thing.  Instead, it causes random
			 * memory errors.  The panic() should remain until
			 * this condition doesn't destabilize the system.
			 */
#if 1
			panic("sw_reg_strategy: swap to sparse file");
#else
			error = EIO;	/* failure */
#endif
		}

		/*
		 * punt if there was an error or a hole in the file.
		 * we must wait for any i/o ops we have already started
		 * to finish before returning.
		 *
		 * XXX we could deal with holes here but it would be
		 * a hassle (in the write case).
		 */
		if (error) {
			s = splbio();
			vnx->vx_error = error;	/* pass error up */
			goto out;
		}

		/*
		 * compute the size ("sz") of this transfer (in bytes).
		 * XXXCDC: ignores read-ahead for non-zero offset
		 */
		if ((off = (byteoff % sdp->swd_bsize)) != 0)
			sz = sdp->swd_bsize - off;
		else
			sz = (1 + nra) * sdp->swd_bsize;

		if (resid < sz)
			sz = resid;

		UVMHIST_LOG(pdhist, "sw_reg_strategy: vp %p/%p offset 0x%x/0x%x",
				sdp->swd_vp, vp, byteoff, nbn);

		/*
		 * now get a buf structure.   note that the vb_buf is
		 * at the front of the nbp structure so that you can
		 * cast pointers between the two structure easily.
		 */
		getvndbuf(nbp);
		nbp->vb_buf.b_flags    = bp->b_flags | B_CALL;
		nbp->vb_buf.b_bcount   = sz;
#if 0
		nbp->vb_buf.b_bufsize  = bp->b_bufsize; /* XXXCDC: really? */
#endif
		nbp->vb_buf.b_bufsize  = sz;
		nbp->vb_buf.b_error    = 0;
		nbp->vb_buf.b_data     = addr;
		nbp->vb_buf.b_blkno    = nbn + btodb(off);
		nbp->vb_buf.b_proc     = bp->b_proc;
		nbp->vb_buf.b_iodone   = sw_reg_iodone;
		nbp->vb_buf.b_vp       = NULLVP;
		nbp->vb_buf.b_vnbufs.le_next = NOLIST;
		nbp->vb_buf.b_rcred    = sdp->swd_cred;
		nbp->vb_buf.b_wcred    = sdp->swd_cred;
		LIST_INIT(&nbp->vb_buf.b_dep);

		/* 
		 * set b_dirtyoff/end and b_validoff/end.   this is
		 * required by the NFS client code (otherwise it will
		 * just discard our I/O request).
		 */
		if (bp->b_dirtyend == 0) {
			nbp->vb_buf.b_dirtyoff = 0;
			nbp->vb_buf.b_dirtyend = sz;
		} else {
			nbp->vb_buf.b_dirtyoff =
			    max(0, bp->b_dirtyoff - (bp->b_bcount-resid));
			nbp->vb_buf.b_dirtyend =
			    min(sz,
				max(0, bp->b_dirtyend - (bp->b_bcount-resid)));
		}
		if (bp->b_validend == 0) {
			nbp->vb_buf.b_validoff = 0;
			nbp->vb_buf.b_validend = sz;
		} else {
			nbp->vb_buf.b_validoff =
			    max(0, bp->b_validoff - (bp->b_bcount-resid));
			nbp->vb_buf.b_validend =
			    min(sz,
				max(0, bp->b_validend - (bp->b_bcount-resid)));
		}

		nbp->vb_xfer = vnx;	/* patch it back in to vnx */

		/*
		 * Just sort by block number
		 */
		nbp->vb_buf.b_cylinder = nbp->vb_buf.b_blkno;
		s = splbio();
		if (vnx->vx_error != 0) {
			putvndbuf(nbp);
			goto out;
		}
		vnx->vx_pending++;

		/* assoc new buffer with underlying vnode */
		bgetvp(vp, &nbp->vb_buf);	

		/* sort it in and start I/O if we are not over our limit */
		disksort(&sdp->swd_tab, &nbp->vb_buf);
		sw_reg_start(sdp);
		splx(s);

		/*
		 * advance to the next I/O
		 */
		byteoff += sz;
		addr += sz;
	}

	s = splbio();

out: /* Arrive here at splbio */
	vnx->vx_flags &= ~VX_BUSY;
	if (vnx->vx_pending == 0) {
		if (vnx->vx_error != 0) {
			bp->b_error = vnx->vx_error;
			bp->b_flags |= B_ERROR;
		}
		putvndxfer(vnx);
		biodone(bp);
	}
	splx(s);
}

/*
 * sw_reg_start: start an I/O request on the requested swapdev
 *
 * => reqs are sorted by disksort (above)
 */
static void
sw_reg_start(sdp)
	struct swapdev	*sdp;
{
	struct buf	*bp;
	UVMHIST_FUNC("sw_reg_start"); UVMHIST_CALLED(pdhist);

	/* recursion control */
	if ((sdp->swd_flags & SWF_BUSY) != 0)
		return;

	sdp->swd_flags |= SWF_BUSY;

	while (sdp->swd_tab.b_active < sdp->swd_maxactive) {
		bp = sdp->swd_tab.b_actf;
		if (bp == NULL)
			break;
		sdp->swd_tab.b_actf = bp->b_actf;
		sdp->swd_tab.b_active++;

		UVMHIST_LOG(pdhist,
		    "sw_reg_start:  bp %p vp %p blkno %p cnt %lx",
		    bp, bp->b_vp, bp->b_blkno, bp->b_bcount);
		if ((bp->b_flags & B_READ) == 0)
			bp->b_vp->v_numoutput++;
		VOP_STRATEGY(bp);
	}
	sdp->swd_flags &= ~SWF_BUSY;
}

/*
 * sw_reg_iodone: one of our i/o's has completed and needs post-i/o cleanup
 *
 * => note that we can recover the vndbuf struct by casting the buf ptr
 */
static void
sw_reg_iodone(bp)
	struct buf *bp;
{
	struct vndbuf *vbp = (struct vndbuf *) bp;
	struct vndxfer *vnx = vbp->vb_xfer;
	struct buf *pbp = vnx->vx_bp;		/* parent buffer */
	struct swapdev	*sdp = vnx->vx_sdp;
	int		s, resid;
	UVMHIST_FUNC("sw_reg_iodone"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "  vbp=%p vp=%p blkno=%x addr=%p",
	    vbp, vbp->vb_buf.b_vp, vbp->vb_buf.b_blkno, vbp->vb_buf.b_data);
	UVMHIST_LOG(pdhist, "  cnt=%lx resid=%lx",
	    vbp->vb_buf.b_bcount, vbp->vb_buf.b_resid, 0, 0);

	/*
	 * protect vbp at splbio and update.
	 */

	s = splbio();
	resid = vbp->vb_buf.b_bcount - vbp->vb_buf.b_resid;
	pbp->b_resid -= resid;
	vnx->vx_pending--;

	if (vbp->vb_buf.b_error) {
		UVMHIST_LOG(pdhist, "  got error=%d !",
		    vbp->vb_buf.b_error, 0, 0, 0);

		/* pass error upward */
		vnx->vx_error = vbp->vb_buf.b_error;
	}

	/*
	 * drop "hold" reference to vnode (if one)
	 * XXXCDC: always set to NULLVP, this is useless, right?
	 */
	if (vbp->vb_buf.b_vp != NULLVP)
		brelvp(&vbp->vb_buf);

	/*
	 * kill vbp structure
	 */
	putvndbuf(vbp);

	/*
	 * wrap up this transaction if it has run to completion or, in
	 * case of an error, when all auxiliary buffers have returned.
	 */
	if (vnx->vx_error != 0) {
		/* pass error upward */
		pbp->b_flags |= B_ERROR;
		pbp->b_error = vnx->vx_error;
		if ((vnx->vx_flags & VX_BUSY) == 0 && vnx->vx_pending == 0) {
			putvndxfer(vnx);
			biodone(pbp);
		}
	} else if (pbp->b_resid == 0) {
#ifdef DIAGNOSTIC
		if (vnx->vx_pending != 0)
			panic("sw_reg_iodone: vnx pending: %d",vnx->vx_pending);
#endif

		if ((vnx->vx_flags & VX_BUSY) == 0) {
			UVMHIST_LOG(pdhist, "  iodone error=%d !",
			    pbp, vnx->vx_error, 0, 0);
			putvndxfer(vnx);
			biodone(pbp);
		}
	}

	/*
	 * done!   start next swapdev I/O if one is pending
	 */
	sdp->swd_tab.b_active--;
	sw_reg_start(sdp);

	splx(s);
}
#endif /* SWAP_TO_FILES */


/*
 * uvm_swap_alloc: allocate space on swap
 *
 * => allocation is done "round robin" down the priority list, as we
 *	allocate in a priority we "rotate" the circle queue.
 * => space can be freed with uvm_swap_free
 * => we return the page slot number in /dev/drum (0 == invalid slot)
 * => we lock uvm.swap_data_lock
 * => XXXMRG: "LESSOK" INTERFACE NEEDED TO EXTENT SYSTEM
 */
int
uvm_swap_alloc(nslots, lessok)
	int *nslots;	/* IN/OUT */
	boolean_t lessok;
{
	struct swapdev *sdp;
	struct swappri *spp;
	u_long	result;
	UVMHIST_FUNC("uvm_swap_alloc"); UVMHIST_CALLED(pdhist);

	/*
	 * no swap devices configured yet?   definite failure.
	 */
	if (uvmexp.nswapdev < 1)
		return 0;
	
	/*
	 * lock data lock, convert slots into blocks, and enter loop
	 */
	simple_lock(&uvm.swap_data_lock);

ReTry:	/* XXXMRG */
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next) {
			/* if it's not enabled, then we can't swap from it */
			if ((sdp->swd_flags & SWF_ENABLE) == 0)
				continue;
			if (sdp->swd_npginuse + *nslots > sdp->swd_npages)
				continue;
			if (extent_alloc(sdp->swd_ex, *nslots, EX_NOALIGN,
					 EX_NOBOUNDARY, EX_MALLOCOK|EX_NOWAIT,
					 &result) != 0) {
				continue;
			}

			/*
			 * successful allocation!  now rotate the circleq.
			 */
			CIRCLEQ_REMOVE(&spp->spi_swapdev, sdp, swd_next);
			CIRCLEQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
			sdp->swd_npginuse += *nslots;
			uvmexp.swpginuse += *nslots;
			simple_unlock(&uvm.swap_data_lock);
			/* done!  return drum slot number */
			UVMHIST_LOG(pdhist,
			    "success!  returning %d slots starting at %d",
			    *nslots, result + sdp->swd_drumoffset, 0, 0);
			return(result + sdp->swd_drumoffset);
		}
	}

	/* XXXMRG: BEGIN HACK */
	if (*nslots > 1 && lessok) {
		*nslots = 1;
		goto ReTry;	/* XXXMRG: ugh!  extent should support this for us */
	}
	/* XXXMRG: END HACK */

	simple_unlock(&uvm.swap_data_lock);
	return 0;		/* failed */
}

/*
 * uvm_swap_free: free swap slots
 *
 * => this can be all or part of an allocation made by uvm_swap_alloc
 * => we lock uvm.swap_data_lock
 */
void
uvm_swap_free(startslot, nslots)
	int startslot;
	int nslots;
{
	struct swapdev *sdp;
	UVMHIST_FUNC("uvm_swap_free"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "freeing %d slots starting at %d", nslots,
	    startslot, 0, 0);
	/*
	 * convert drum slot offset back to sdp, free the blocks 
	 * in the extent, and return.   must hold pri lock to do 
	 * lookup and access the extent.
	 */
	simple_lock(&uvm.swap_data_lock);
	sdp = swapdrum_getsdp(startslot);

#ifdef DIAGNOSTIC
	if (uvmexp.nswapdev < 1)
		panic("uvm_swap_free: uvmexp.nswapdev < 1\n");
	if (sdp == NULL) {
		printf("uvm_swap_free: startslot %d, nslots %d\n", startslot,
		    nslots);
		panic("uvm_swap_free: unmapped address\n");
	}
#endif
	if (extent_free(sdp->swd_ex, startslot - sdp->swd_drumoffset, nslots,
			EX_MALLOCOK|EX_NOWAIT) != 0)
		printf("warning: resource shortage: %d slots of swap lost\n",
			nslots);

	sdp->swd_npginuse -= nslots;
	uvmexp.swpginuse -= nslots;
#ifdef DIAGNOSTIC
	if (sdp->swd_npginuse < 0)
		panic("uvm_swap_free: inuse < 0");
#endif
	simple_unlock(&uvm.swap_data_lock);
}

/*
 * uvm_swap_put: put any number of pages into a contig place on swap
 *
 * => can be sync or async
 * => XXXMRG: consider making it an inline or macro
 */
int
uvm_swap_put(swslot, ppsp, npages, flags)
	int swslot;
	struct vm_page **ppsp;
	int	npages;
	int	flags;
{
	int	result;

#if 0
	flags |= PGO_SYNCIO; /* XXXMRG: tmp, force sync */
#endif

	result = uvm_swap_io(ppsp, swslot, npages, B_WRITE |
	    ((flags & PGO_SYNCIO) ? 0 : B_ASYNC));

	return (result);
}

/*
 * uvm_swap_get: get a single page from swap
 *
 * => usually a sync op (from fault)
 * => XXXMRG: consider making it an inline or macro
 */
int
uvm_swap_get(page, swslot, flags)
	struct vm_page *page;
	int swslot, flags;
{
	int	result;

	uvmexp.nswget++;
#ifdef DIAGNOSTIC
	if ((flags & PGO_SYNCIO) == 0)
		printf("uvm_swap_get: ASYNC get requested?\n");
#endif

	/*
	 * this page is (about to be) no longer only in swap.
	 */
	simple_lock(&uvm.swap_data_lock);
	uvmexp.swpgonly--;
	simple_unlock(&uvm.swap_data_lock);

	result = uvm_swap_io(&page, swslot, 1, B_READ | 
	    ((flags & PGO_SYNCIO) ? 0 : B_ASYNC));

	if (result != VM_PAGER_OK && result != VM_PAGER_PEND) {
		/*
		 * oops, the read failed so it really is still only in swap.
		 */
		simple_lock(&uvm.swap_data_lock);
		uvmexp.swpgonly++;
		simple_unlock(&uvm.swap_data_lock);
	}

	return (result);
}

/*
 * uvm_swap_io: do an i/o operation to swap
 */

static int
uvm_swap_io(pps, startslot, npages, flags)
	struct vm_page **pps;
	int startslot, npages, flags;
{
	daddr_t startblk;
	struct swapbuf *sbp;
	struct	buf *bp;
	vaddr_t kva;
	int	result, s, waitf, pflag;
#ifdef UVM_SWAP_ENCRYPT
	vaddr_t dstkva;
	struct vm_page *tpps[MAXBSIZE >> PAGE_SHIFT];
	struct swapdev *sdp;
	int	encrypt = 0;
#endif
	UVMHIST_FUNC("uvm_swap_io"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "<- called, startslot=%d, npages=%d, flags=%d",
	    startslot, npages, flags, 0);

	/*
	 * convert starting drum slot to block number
	 */
	startblk = btodb(startslot << PAGE_SHIFT);

	/*
	 * first, map the pages into the kernel (XXX: currently required
	 * by buffer system).   note that we don't let pagermapin alloc
	 * an aiodesc structure because we don't want to chance a malloc.
	 * we've got our own pool of aiodesc structures (in swapbuf).
	 */
	waitf = (flags & B_ASYNC) ? M_NOWAIT : M_WAITOK;
	kva = uvm_pagermapin(pps, npages, NULL, waitf);
	if (kva == NULL)
		return (VM_PAGER_AGAIN);

#ifdef UVM_SWAP_ENCRYPT
	/* 
	 * encrypt to swap
	 */
	if ((flags & B_READ) == 0) {
		int i, opages;
		caddr_t src, dst;

		/*
		 * Check if we need to do swap encryption on old pages.
		 * Later we need a different scheme, that swap encrypts
		 * all pages of a process that had at least one page swap
		 * encrypted.  Then we might not need to copy all pages
		 * in the cluster, and avoid the memory overheard in 
		 * swapping.
		 */
		if (!uvm_doswapencrypt)
				goto noswapencrypt;

		if (!uvm_swap_allocpages(tpps, npages)) {
			uvm_pagermapout(kva, npages);
			return (VM_PAGER_AGAIN);
		}
		
		dstkva = uvm_pagermapin(tpps, npages, NULL, waitf);
		if (dstkva == NULL) {
			uvm_pagermapout(kva, npages);
			uvm_swap_freepages(tpps, npages);
			return (VM_PAGER_AGAIN);
		}

		src = (caddr_t) kva;
		dst = (caddr_t) dstkva;
		for (i = 0; i < npages; i++) {
			/* mark for async writes */
			tpps[i]->pqflags |= PQ_ENCRYPT;
			swap_encrypt(src, dst, 1 << PAGE_SHIFT);
			src += 1 << PAGE_SHIFT;
			dst += 1 << PAGE_SHIFT;
		}

		uvm_pagermapout(kva, npages);

		/* dispose of pages we dont use anymore */
		opages = npages;
		uvm_pager_dropcluster(NULL, NULL, pps, &opages, 
				      PGO_PDFREECLUST, 0);

		kva = dstkva;

		encrypt = 1;
	noswapencrypt:
	}
#endif /* UVM_SWAP_ENCRYPT */

	/* 
	 * now allocate a swap buffer off of freesbufs
	 * [make sure we don't put the pagedaemon to sleep...]
	 */
	s = splbio();
	pflag = ((flags & B_ASYNC) != 0 || curproc == uvm.pagedaemon_proc)
		? 0
		: PR_WAITOK;
	sbp = pool_get(swapbuf_pool, pflag);
	splx(s);		/* drop splbio */

	/*
	 * if we failed to get a swapbuf, return "try again"
	 */
	if (sbp == NULL) {
#ifdef UVM_SWAP_ENCRYPT
		if ((flags & B_READ) == 0 && encrypt) {
			/* swap encrypt needs cleanup */
			uvm_pagermapout(kva, npages);
			uvm_swap_freepages(tpps, npages);
		}
#endif
		return (VM_PAGER_AGAIN);
	}
	
#ifdef UVM_SWAP_ENCRYPT
	/* 
	 * prevent ASYNC reads.
	 * uvm_swap_io is only called from uvm_swap_get, uvm_swap_get
	 * assumes that all gets are SYNCIO.  Just make sure here.
	 */
	if (flags & B_READ)
	  flags &= ~B_ASYNC;
#endif
	/*
	 * fill in the bp/sbp.   we currently route our i/o through
	 * /dev/drum's vnode [swapdev_vp].
	 */
	bp = &sbp->sw_buf;
	bp->b_flags = B_BUSY | B_NOCACHE | (flags & (B_READ|B_ASYNC));
	bp->b_proc = &proc0;	/* XXX */
	bp->b_rcred = bp->b_wcred = proc0.p_ucred;
	bp->b_vnbufs.le_next = NOLIST;
	bp->b_data = (caddr_t)kva;
	bp->b_blkno = startblk;
	LIST_INIT(&bp->b_dep);
	s = splbio();
	VHOLD(swapdev_vp);
	bp->b_vp = swapdev_vp;
	splx(s);
	/* XXXCDC: isn't swapdev_vp always a VCHR? */
	/* XXXMRG: probably -- this is obviously something inherited... */
	if (swapdev_vp->v_type == VBLK)
		bp->b_dev = swapdev_vp->v_rdev;
	bp->b_bcount = npages << PAGE_SHIFT;

#ifdef UVM_SWAP_ENCRYPT
	if (swap_encrypt_initalized) { 
		/*
		 * we need to know the swap device that we are swapping to/from
		 * to see if the pages need to be marked for decryption or
		 * actually need to be decrypted.
		 * XXX - does this information stay the same over the whole 
		 * execution of this function?
		 */
		simple_lock(&uvm.swap_data_lock);
		sdp = swapdrum_getsdp(startslot);
		simple_unlock(&uvm.swap_data_lock);
	}
#endif

	/* 
	 * for pageouts we must set "dirtyoff" [NFS client code needs it].
	 * and we bump v_numoutput (counter of number of active outputs).
	 */
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_dirtyoff = 0;
		bp->b_dirtyend = npages << PAGE_SHIFT;
		s = splbio();
		swapdev_vp->v_numoutput++;
		splx(s);
#ifdef UVM_SWAP_ENCRYPT
		/* mark the pages in the drum for decryption */
		if (swap_encrypt_initalized)
			uvm_swap_markdecrypt(sdp, startslot, npages, encrypt);
#endif
	}

	/*
	 * for async ops we must set up the aiodesc and setup the callback
	 * XXX: we expect no async-reads, but we don't prevent it here.
	 */
	if (flags & B_ASYNC) {
		sbp->sw_aio.aiodone = uvm_swap_aiodone;
		sbp->sw_aio.kva = kva;
		sbp->sw_aio.npages = npages;
		sbp->sw_aio.pd_ptr = sbp;	/* backpointer */
		bp->b_flags |= B_CALL;		/* set callback */
		bp->b_iodone = uvm_swap_bufdone;/* "buf" iodone function */
		UVMHIST_LOG(pdhist, "doing async!", 0, 0, 0, 0);
	}
	UVMHIST_LOG(pdhist,
	    "about to start io: data = 0x%p blkno = 0x%x, bcount = %ld",
	    bp->b_data, bp->b_blkno, bp->b_bcount, 0);

	/*
	 * now we start the I/O, and if async, return.
	 */
	VOP_STRATEGY(bp);
	if (flags & B_ASYNC)
		return (VM_PAGER_PEND);

	/*
	 * must be sync i/o.   wait for it to finish
	 */
	bp->b_error = biowait(bp);
	result = (bp->b_flags & B_ERROR) ? VM_PAGER_ERROR : VM_PAGER_OK;

#ifdef UVM_SWAP_ENCRYPT
	/* 
	 * decrypt swap
	 */
	if (swap_encrypt_initalized &&
	    (bp->b_flags & B_READ) && !(bp->b_flags & B_ERROR)) {
		int i;
		caddr_t data = bp->b_data;
		for (i = 0; i < npages; i++) {
			/* Check if we need to decrypt */
			if (uvm_swap_needdecrypt(sdp, startslot + i))
				swap_decrypt(data, data, 1 << PAGE_SHIFT);
			data += 1 << PAGE_SHIFT;
		}
	}
#endif
	/*
	 * kill the pager mapping
	 */
	uvm_pagermapout(kva, npages);

#ifdef UVM_SWAP_ENCRYPT
	/*
	 *  Not anymore needed, free after encryption
	 */
	if ((bp->b_flags & B_READ) == 0 && encrypt)
		uvm_swap_freepages(tpps, npages);
#endif
	/*
	 * now dispose of the swap buffer
	 */
	s = splbio();
	if (bp->b_vp)
		brelvp(bp);

	pool_put(swapbuf_pool, sbp);
	splx(s);

	/*
	 * finally return.
	 */
	UVMHIST_LOG(pdhist, "<- done (sync)  result=%d", result, 0, 0, 0);
	return (result);
}

/*
 * uvm_swap_bufdone: called from the buffer system when the i/o is done
 */
static void
uvm_swap_bufdone(bp)
	struct buf *bp;
{
	struct swapbuf *sbp = (struct swapbuf *) bp;
	int	s = splbio();
	UVMHIST_FUNC("uvm_swap_bufdone"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "cleaning buf %p", buf, 0, 0, 0);
#ifdef DIAGNOSTIC
	/*
	 * sanity check: swapbufs are private, so they shouldn't be wanted
	 */
	if (bp->b_flags & B_WANTED)
		panic("uvm_swap_bufdone: private buf wanted");
#endif

	/*
	 * drop the buffer's reference to the vnode.
	 */
	if (bp->b_vp)
		brelvp(bp);

	/*
	 * now put the aio on the uvm.aio_done list and wake the
	 * pagedaemon (which will finish up our job in its context).
	 */
	simple_lock(&uvm.pagedaemon_lock);	/* locks uvm.aio_done */
	TAILQ_INSERT_TAIL(&uvm.aio_done, &sbp->sw_aio, aioq);
	simple_unlock(&uvm.pagedaemon_lock);

	thread_wakeup(&uvm.pagedaemon);
	splx(s);
}

/*
 * uvm_swap_aiodone: aiodone function for anonymous memory
 *
 * => this is called in the context of the pagedaemon (but with the
 *	page queues unlocked!)
 * => our "aio" structure must be part of a "swapbuf"
 */
static void
uvm_swap_aiodone(aio)
	struct uvm_aiodesc *aio;
{
	struct swapbuf *sbp = aio->pd_ptr;
	struct vm_page *pps[MAXBSIZE >> PAGE_SHIFT];
	int lcv, s;
	vaddr_t addr;
	UVMHIST_FUNC("uvm_swap_aiodone"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "done with aio %p", aio, 0, 0, 0);
#ifdef DIAGNOSTIC
	/*
	 * sanity check
	 */
	if (aio->npages > (MAXBSIZE >> PAGE_SHIFT))
		panic("uvm_swap_aiodone: aio too big!");
#endif

	/*
	 * first, we have to recover the page pointers (pps) by poking in the
	 * kernel pmap (XXX: should be saved in the buf structure).
	 */
	for (addr = aio->kva, lcv = 0 ; lcv < aio->npages ; 
		addr += PAGE_SIZE, lcv++) {
		pps[lcv] = uvm_pageratop(addr);
	}

	/*
	 * now we can dispose of the kernel mappings of the buffer
	 */
	uvm_pagermapout(aio->kva, aio->npages);

	/*
	 * now we can dispose of the pages by using the dropcluster function
	 * [note that we have no "page of interest" so we pass in null]
	 */

#ifdef UVM_SWAP_ENCRYPT
	/*
	 * XXX - assumes that we only get ASYNC writes. used to be above.
	 */
	if (pps[0]->pqflags & PQ_ENCRYPT)
		uvm_swap_freepages(pps, aio->npages);
	else
#endif /* UVM_SWAP_ENCRYPT */
	uvm_pager_dropcluster(NULL, NULL, pps, &aio->npages, 
			      PGO_PDFREECLUST, 0);

	/*
	 * finally, we can dispose of the swapbuf
	 */
	s = splbio();
	pool_put(swapbuf_pool, sbp);
	splx(s);

	/*
	 * done!
	 */
}

static void
swapmount()
{
	struct swapdev *sdp;
	struct swappri *spp;
	struct vnode *vp;
	dev_t swap_dev = swdevt[0].sw_dev;

	/*
	 * No locking here since we happen to know that we will just be called
	 * once before any other process has forked.
	 */

	if (swap_dev == NODEV) {
		printf("swapmount: no device\n");
		return;
	}

	if (bdevvp(swap_dev, &vp)) {
		printf("swapmount: no device 2\n");
		return;
	}

	sdp = malloc(sizeof(*sdp), M_VMSWAP, M_WAITOK);
	spp = malloc(sizeof(*spp), M_VMSWAP, M_WAITOK);
	bzero(sdp, sizeof(*sdp));

	sdp->swd_flags = SWF_FAKE;
	sdp->swd_dev = swap_dev;
	sdp->swd_vp = vp;
	swaplist_insert(sdp, spp, 0);
	sdp->swd_pathlen = strlen("swap_device") + 1;
	sdp->swd_path = malloc(sdp->swd_pathlen, M_VMSWAP, M_WAITOK);
	if (copystr("swap_device", sdp->swd_path, sdp->swd_pathlen, 0))
		panic("swapmount: copystr");

	if (swap_on(curproc, sdp)) {
		swaplist_find(vp, 1);
		swaplist_trim();
		vput(sdp->swd_vp);
		free(sdp->swd_path, M_VMSWAP);
		free(sdp, M_VMSWAP);
		return;
	}

	VOP_UNLOCK(vp, 0, curproc);
}
