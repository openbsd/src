/*	$OpenBSD: vm_swap.c,v 1.11 1999/07/06 16:53:04 deraadt Exp $	*/
/*	$NetBSD: vm_swap.c,v 1.64 1998/11/08 19:45:17 mycroft Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Matthew R. Green, Tobias Weingartner
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/disklabel.h>
#include <sys/dmap.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/map.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/extent.h>
#include <sys/swap.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <machine/vmparam.h>

#include <vm/vm_conf.h>

#include <miscfs/specfs/specdev.h>

/*
 * The idea here is to provide a single interface for multiple swap devices,
 * of any kind and priority in a simple and fast way.
 *
 * Each swap device has these properties:
 *	* swap in use.
 *	* swap enabled.
 *	* map information in `/dev/drum'.
 *	* vnode pointer.
 * Files have these additional properties:
 *	* block size.
 *	* maximum byte count in buffer.
 *	* buffer.
 *	* credentials.
 *
 * The arguments to swapctl(2) are:
 *	int cmd;
 *	void *arg;
 *	int misc;
 * The cmd can be one of:
 *	SWAP_NSWAP - swapctl(2) returns the number of swap devices currently in
 *		use.
 *	SWAP_STATS - swapctl(2) takes a struct ent * in (void *arg) and writes
 *		misc or fewer (to zero) entries of configured swap devices,
 *		and returns the number of entries written or -1 on error.
 *	SWAP_ON - swapctl(2) takes a (char *) in arg to be the pathname of a
 *		device or file to begin swapping on, with it's priority in
 *		misc, returning 0 on success and -1 on error.
 *	SWAP_OFF - swapctl(2) takes a (char *) n arg to be the pathname of a
 *		device or file to stop swapping on.  returning 0 or -1.
 *		XXX unwritten.
 *	SWAP_CTL - swapctl(2) changes the priority of a swap device, using the
 *		misc value.
 */

#ifdef SWAPDEBUG
#define STATIC
#define VMSDB_SWON	0x0001
#define VMSDB_SWOFF	0x0002
#define VMSDB_SWINIT	0x0004
#define VMSDB_SWALLOC	0x0008
#define VMSDB_SWFLOW	0x0010
#define VMSDB_INFO	0x0020
int vmswapdebug = 0;
int vmswap_domount = 1;

#define DPRINTF(f, m) do {		\
	if (vmswapdebug & (f))		\
		printf m;		\
} while(0)
#else
#define STATIC static
#define DPRINTF(f, m)
#endif

#define SWAP_TO_FILES

struct swapdev {
	struct swapent		swd_se;
#define swd_dev			swd_se.se_dev
#define swd_flags		swd_se.se_flags
#define swd_nblks		swd_se.se_nblks
#define swd_inuse		swd_se.se_inuse
#define swd_priority		swd_se.se_priority
#define swd_path		swd_se.se_path
	daddr_t			swd_mapoffset;
	int			swd_mapsize;
	struct extent		*swd_ex;
	struct vnode		*swd_vp;
	CIRCLEQ_ENTRY(swapdev)	swd_next;

#ifdef SWAP_TO_FILES
	int			swd_bsize;
	int			swd_maxactive;
	struct buf		swd_tab;
	struct ucred		*swd_cred;
#endif
};

/*
 * Swap device priority entry; the list is kept sorted on `spi_priority'.
 */
struct swappri {
	int			spi_priority;
	CIRCLEQ_HEAD(spi_swapdev, swapdev)	spi_swapdev;
	LIST_ENTRY(swappri)	spi_swappri;
};




/*
 * The following two structures are used to keep track of data transfers
 * on swap devices associated with regular files.
 * NOTE: this code is more or less a copy of vnd.c; we use the same
 * structure names here to ease porting..
 */


struct vndxfer {
	struct buf	*vx_bp;			/* Pointer to parent buffer */
	struct swapdev	*vx_sdp;
	int		vx_error;
	int		vx_pending;		/* # of pending aux buffers */
	int		vx_flags;
#define VX_BUSY		1
#define VX_DEAD		2
};


struct vndbuf {
	struct buf	vb_buf;
	struct vndxfer	*vb_xfer;
};

/* To get from a buffer to the encapsulating vndbuf */
#define BUF_TO_VNDBUF(bp) \
	((struct vndbuf *)((long)bp - ((long)&((struct vndbuf *)0)->vb_buf)))

/* vnd macro stuff, rewritten to use malloc()/free() */
#define getvndxfer()							\
	(struct vndxfer *)malloc(sizeof(struct vndxfer), M_VMSWAP, M_WAITOK);

#define putvndxfer(vnx)							\
	free(vnx, M_VMSWAP)

#define getvndbuf()							\
	(struct vndbuf *)malloc(sizeof(struct vndbuf), M_VMSWAP, M_WAITOK);

#define putvndbuf(vbp)							\
	free(vbp, M_VMSWAP)


int nswapdev;
int swflags;
struct extent *swapmap;
LIST_HEAD(swap_priority, swappri) swap_priority;

STATIC int swap_on __P((struct proc *, struct swapdev *));
#ifdef SWAP_OFF_WORKS
STATIC int swap_off __P((struct proc *, struct swapdev *));
#endif
STATIC struct swapdev *swap_getsdpfromaddr __P((daddr_t));
STATIC void swap_addmap __P((struct swapdev *, int));

#ifdef SWAP_TO_FILES
STATIC void sw_reg_strategy __P((struct swapdev *, struct buf *, int));
STATIC void sw_reg_iodone __P((struct buf *));
STATIC void sw_reg_start __P((struct swapdev *));
#endif

STATIC void insert_swapdev __P((struct swapdev *, int));
STATIC struct swapdev *find_swapdev __P((struct vnode *, int));
STATIC void swaplist_trim __P((void));

STATIC void swapmount __P((void));

/*
 * We use two locks to protect the swap device lists.
 * The long-term lock is used only used to prevent races in
 * concurrently executing swapctl(2) system calls.
 */
struct simplelock	swaplist_lock;
struct lock		swaplist_change_lock;

/*
 * Insert a swap device on the priority list.
 */
void
insert_swapdev(sdp, priority)
	struct swapdev *sdp;
	int priority;
{
	struct swappri *spp, *pspp;

again:
	simple_lock(&swaplist_lock);

	/*
	 * Find entry at or after which to insert the new device.
	 */
	for (pspp = NULL, spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		if (priority <= spp->spi_priority)
			break;
		pspp = spp;
	}

	if (spp == NULL || spp->spi_priority != priority) {
		spp = (struct swappri *)
			malloc(sizeof *spp, M_VMSWAP, M_NOWAIT);

		if (spp == NULL) {
			simple_unlock(&swaplist_lock);
			tsleep((caddr_t)&lbolt, PSWP, "memory", 0);
			goto again;
		}
		DPRINTF(VMSDB_SWFLOW,
			("sw: had to create a new swappri = %d\n", priority));

		spp->spi_priority = priority;
		CIRCLEQ_INIT(&spp->spi_swapdev);

		if (pspp)
			LIST_INSERT_AFTER(pspp, spp, spi_swappri);
		else
			LIST_INSERT_HEAD(&swap_priority, spp, spi_swappri);

	}
	/* Onto priority list */
	CIRCLEQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
	sdp->swd_priority = priority;
	simple_unlock(&swaplist_lock);
}

/*
 * Find and optionally remove a swap device from the priority list.
 */
struct swapdev *
find_swapdev(vp, remove)
	struct vnode *vp;
	int remove;
{
	struct swapdev *sdp;
	struct swappri *spp;

	simple_lock(&swaplist_lock);
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next)
			if (sdp->swd_vp == vp) {
				if (remove)
					CIRCLEQ_REMOVE(&spp->spi_swapdev, sdp,
							swd_next);
				simple_unlock(&swaplist_lock);
				return (sdp);
			}
	}
	simple_unlock(&swaplist_lock);
	return (NULL);
}

/*
 * Scan priority list for empty priority entries.
 */
void
swaplist_trim()
{
	struct swappri *spp;

	simple_lock(&swaplist_lock);
restart:
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		if (spp->spi_swapdev.cqh_first != (void *)&spp->spi_swapdev)
			continue;
		LIST_REMOVE(spp, spi_swappri);
		free((caddr_t)spp, M_VMSWAP);
		goto restart;
	}
	simple_unlock(&swaplist_lock);
}

int
sys_swapctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapctl_args /* {
		syscallarg(int) cmd;
		syscallarg(const void *) arg;
		syscallarg(int) misc;
	} */ *uap = (struct sys_swapctl_args *)v;
	struct vnode *vp;
	struct nameidata nd;
	struct swappri *spp;
	struct swapdev *sdp;
	struct swapent *sep;
	char	userpath[PATH_MAX + 1];
	int	count, error, misc;
	size_t	len;
	int	priority;

	misc = SCARG(uap, misc);

	DPRINTF(VMSDB_SWFLOW, ("entering sys_swapctl\n"));
	
	/* how many swap devices */
	if (SCARG(uap, cmd) == SWAP_NSWAP) {
		DPRINTF(VMSDB_SWFLOW,("did SWAP_NSWAP: leaving sys_swapctl\n"));
		*retval = nswapdev;
		return (0);
	}

	/* stats on the swap devices. */
	if (SCARG(uap, cmd) == SWAP_STATS) {
		sep = (struct swapent *)SCARG(uap, arg);
		count = 0;

		error = lockmgr(&swaplist_change_lock, LK_SHARED, (void *)0, p);
		if (error)
			return (error);
		for (spp = swap_priority.lh_first; spp != NULL;
		    spp = spp->spi_swappri.le_next) {
			for (sdp = spp->spi_swapdev.cqh_first;
			     sdp != (void *)&spp->spi_swapdev && misc-- > 0;
			     sdp = sdp->swd_next.cqe_next, sep++, count++) {
			  	/*
				 * We do not do NetBSD 1.3 compat call.
				 */
				error = copyout((caddr_t)&sdp->swd_se,
				    (caddr_t)sep, sizeof(struct swapent));

				if (error)
					goto out;
			}
		}
out:
		(void)lockmgr(&swaplist_change_lock, LK_RELEASE, (void *)0, p);
		if (error)
			return (error);

		DPRINTF(VMSDB_SWFLOW,("did SWAP_STATS: leaving sys_swapctl\n"));

		*retval = count;
		return (0);
	} 
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	if (SCARG(uap, arg) == NULL) {
		/* XXX - interface - arg==NULL: miniroot */
		vp = rootvp;
		if (vget(vp, LK_EXCLUSIVE, p))
			return (EBUSY);
		if (SCARG(uap, cmd) == SWAP_ON &&
		    copystr("miniroot", userpath, sizeof userpath, &len))
			panic("swapctl: miniroot copy failed");
	} else {
		int	space;
		char	*where;

		if (SCARG(uap, cmd) == SWAP_ON) {
			if ((error = copyinstr(SCARG(uap, arg), userpath,
			    sizeof userpath, &len)))
				return (error);
			space = UIO_SYSSPACE;
			where = userpath;
		} else {
			space = UIO_USERSPACE;
			where = (char *)SCARG(uap, arg);
		}
		NDINIT(&nd, LOOKUP, FOLLOW|LOCKLEAF, space, where, p);
		if ((error = namei(&nd)))
			return (error);

		vp = nd.ni_vp;
	}

	error = lockmgr(&swaplist_change_lock, LK_EXCLUSIVE, (void *)0, p);
	if (error)
		goto bad2;

	switch(SCARG(uap, cmd)) {
	case SWAP_CTL:
		priority = SCARG(uap, misc);
		if ((sdp = find_swapdev(vp, 1)) == NULL) {
			error = ENOENT;
			break;
		}
		insert_swapdev(sdp, priority);
		swaplist_trim();
		break;

	case SWAP_ON:
		priority = SCARG(uap, misc);

		/* Check for duplicates */
		if ((sdp = find_swapdev(vp, 0)) != NULL) {
			if (!bcmp(sdp->swd_path, "swap_device", 12)) {
				copystr(userpath, sdp->swd_path, len, 0);
				error = 0;
			} else
				error = EBUSY;
			goto bad;
		}

		sdp = (struct swapdev *)
			malloc(sizeof *sdp, M_VMSWAP, M_WAITOK);
		bzero(sdp, sizeof(*sdp));

		sdp->swd_vp = vp;
		sdp->swd_dev = (vp->v_type == VBLK) ? vp->v_rdev : NODEV;

		if ((error = swap_on(p, sdp)) != 0) {
			free((caddr_t)sdp, M_VMSWAP);
			break;
		}
#ifdef SWAP_TO_FILES
		/*
		 * XXX Is NFS elaboration necessary?
		 */
		if (vp->v_type == VREG)
			sdp->swd_cred = crdup(p->p_ucred);
#endif
		if (copystr(userpath, sdp->swd_path, len, 0) != 0)
			panic("swapctl: copystr");
		insert_swapdev(sdp, priority);

		/* Keep reference to vnode */
		vref(vp);
		break;

	case SWAP_OFF:
		DPRINTF(VMSDB_SWFLOW, ("doing SWAP_OFF...\n"));
#ifdef SWAP_OFF_WORKS
		if ((sdp = find_swapdev(vp, 0)) == NULL) {
			error = ENXIO;
			break;
		}
		/*
		 * If a device isn't in use or enabled, we
		 * can't stop swapping from it (again).
		 */
		if ((sdp->swd_flags &
		    (SWF_INUSE|SWF_ENABLE)) == 0) {
			error = EBUSY;
			goto bad;
		}
		if ((error = swap_off(p, sdp)) != 0)
			goto bad;

		/* Find again and remove this time */
		if ((sdp = find_swapdev(vp, 1)) == NULL) {
			error = ENXIO;
			break;
		}
		free((caddr_t)sdp, M_VMSWAP);
#else
		error = ENODEV;
#endif
		break;

	default:
		DPRINTF(VMSDB_SWFLOW,
			("unhandled command: %x\n", SCARG(uap, cmd)));
		error = EINVAL;
	}

bad:
	(void)lockmgr(&swaplist_change_lock, LK_RELEASE, (void *)0, p);
bad2:
	vput(vp);

	DPRINTF(VMSDB_SWFLOW, ("leaving sys_swapctl: error %d\n", error));
	return (error);
}

/*
 * swap_on() attempts to begin swapping on a swapdev.  we check that this
 * device is OK to swap from, miss the start of any disk (to avoid any
 * disk labels that may exist).
 */
STATIC int
swap_on(p, sdp)
	struct proc *p;
	struct swapdev *sdp;
{
	static int count = 0;
	struct vnode *vp = sdp->swd_vp;
	int error, nblks, size;
	long addr;
	char *storage;
	int storagesize;
#ifdef SWAP_TO_FILES
	struct vattr va;
#endif
#if defined(NFSSERVER) || defined(NFSCLIENT)
	extern int (**nfsv2_vnodeop_p) __P((void *));
#endif /* defined(NFSSERVER) || defined(NFSCLIENT) */
	dev_t dev = sdp->swd_dev;
	char *name;


	/* If root on swap, then the skip open/close operations. */
	if (vp != rootvp) {
		if ((error = VOP_OPEN(vp, FREAD|FWRITE, p->p_ucred, p)))
			return (error);
		vp->v_writecount++;
	}

	DPRINTF(VMSDB_INFO,
		("swap_on: dev = %d, major(dev) = %d\n", dev, major(dev)));

	switch (vp->v_type) {
	case VBLK:
		if (bdevsw[major(dev)].d_psize == 0 ||
		    (nblks = (*bdevsw[major(dev)].d_psize)(dev)) == -1) {
			error = ENXIO;
			goto bad;
		}
		break;

#ifdef SWAP_TO_FILES
	case VREG:
		if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)))
			goto bad;
		nblks = (int)btodb(va.va_size);
		if ((error =
		     VFS_STATFS(vp->v_mount, &vp->v_mount->mnt_stat, p)) != 0)
			goto bad;

		sdp->swd_bsize = vp->v_mount->mnt_stat.f_iosize;
#if defined(NFSSERVER) || defined(NFSCLIENT)
		if (vp->v_op == nfsv2_vnodeop_p)
			sdp->swd_maxactive = 2; /* XXX */
		else
#endif /* defined(NFSSERVER) || defined(NFSCLIENT) */
			sdp->swd_maxactive = 8; /* XXX */
		break;
#endif

	default:
		error = ENXIO;
		goto bad;
	}
	if (nblks == 0) {
		DPRINTF(VMSDB_SWFLOW, ("swap_on: nblks == 0\n"));
		error = EINVAL;
		goto bad;
	}

	sdp->swd_flags |= SWF_INUSE;
	sdp->swd_nblks = nblks;

	/*
	 * skip over first cluster of a device in case of labels or
	 * boot blocks.
	 */
	if (vp->v_type == VBLK) {
		size = (int)(nblks - ctod(CLSIZE));
		addr = (long)ctod(CLSIZE);
	} else {
		size = (int)nblks;
		addr = (long)0;
	}

	DPRINTF(VMSDB_SWON,
		("swap_on: dev %x: size %d, addr %ld\n", dev, size, addr));

	name = malloc(12, M_VMSWAP, M_WAITOK);
	sprintf(name, "swap0x%04x", count++);
	/* XXX make this based on ram as well. */
	storagesize = EXTENT_FIXED_STORAGE_SIZE(maxproc * 2);
	storage = malloc(storagesize, M_VMSWAP, M_WAITOK);
	sdp->swd_ex = extent_create(name, 0, nblks, M_VMSWAP,
				    storage, storagesize, EX_WAITOK);
	if (addr) {
		if (extent_alloc_region(sdp->swd_ex, 0, addr, EX_WAITOK))
			panic("disklabel region");
		sdp->swd_inuse += addr;
	}


	if (vp == rootvp) {
		struct mount *mp;
		struct statfs *sp;
		int rootblks;

		/* Get size from root FS (mountroot did statfs) */
		mp = rootvnode->v_mount;
		sp = &mp->mnt_stat;
		rootblks = sp->f_blocks * (sp->f_bsize / DEV_BSIZE);
		if (rootblks > nblks)
			panic("miniroot size");

		if (extent_alloc_region(sdp->swd_ex, addr, rootblks, EX_WAITOK))
			panic("miniroot region");

		printf("Preserved %d blocks, leaving %d pages of swap\n",
		    rootblks, dtoc(size - rootblks));
	}

	swap_addmap(sdp, size);
	nswapdev++;
	sdp->swd_flags |= SWF_ENABLE;
	return (0);

bad:
	if (vp != rootvp) {
		vp->v_writecount--;
		(void)VOP_CLOSE(vp, FREAD|FWRITE, p->p_ucred, p);
	}
	return (error);
}

#ifdef SWAP_OFF_WORKS
STATIC int
swap_off(p, sdp)
	struct proc *p;
	struct swapdev *sdp;
{
	char	*name;

	/* turn off the enable flag */
	sdp->swd_flags &= ~SWF_ENABLE;

	DPRINTF(VMSDB_SWOFF, ("swap_off: %x\n", sdp->swd_dev));

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
	 * There might be an easier way to do a "soft" swapoff.  First
	 * we mark the particular swap partition as not desirable anymore.
	 * Then we use the pager to page a couple of pages in, each time
	 * it has the memory, and the chance to do so.  Thereby moving pages
	 * back into memory.  Once they are in memory, when they get paged
	 * out again, they do not go back onto the "undesirable" device
	 * anymore, but to good devices.  This might take longer, but it
	 * can certainly work.  If need be, the user process can sleep on
	 * the particular sdp entry, and the swapper can then wake him up
	 * when everything is done.
	 */

	/* until the above code is written, we must ENODEV */
	return ENODEV;

	extent_free(swapmap, sdp->swd_mapoffset, sdp->swd_mapsize, EX_WAITOK);
	nswapdev--;
	name = sdp->swd_ex->ex_name;
	extent_destroy(sdp->swd_ex);
	free(name, M_VMSWAP);
	free((caddr_t)sdp->swd_ex, M_VMSWAP);
	if (sdp->swp_vp != rootvp) {
		vp->v_writecount--;
		(void) VOP_CLOSE(sdp->swd_vp, FREAD|FWRITE, p->p_ucred, p);
	}
	if (sdp->swd_vp)
		vrele(sdp->swd_vp);
	free((caddr_t)sdp, M_VMSWAP);
	return (0);
}
#endif

/*
 * To decide where to allocate what part of swap, we must "round robin"
 * the swap devices in swap_priority of the same priority until they are
 * full.  we do this with a list of swap priorities that have circle
 * queues of swapdevs.
 *
 * The following functions control allocation and freeing of part of the
 * swap area.  you call swap_alloc() with a size and it returns an address.
 * later you call swap_free() and it frees the use of that swap area.
 *
 *	daddr_t swap_alloc(int size);
 *	void swap_free(int size, daddr_t addr);
 */

daddr_t
swap_alloc(size)
	int size;
{
	struct swapdev *sdp;
	struct swappri *spp;
	u_long	result;

	if (nswapdev < 1)
		return 0;
	
	simple_lock(&swaplist_lock);
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next) {
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next) {
			/* if it's not enabled, then we can't swap from it */
			if ((sdp->swd_flags & SWF_ENABLE) == 0 ||
			    /* XXX IS THIS CORRECT ? */
#if 1
			    (sdp->swd_inuse + size > sdp->swd_nblks) ||
#endif
			    extent_alloc(sdp->swd_ex, size, EX_NOALIGN,
					 EX_NOBOUNDARY, EX_MALLOCOK|EX_NOWAIT,
					 &result) != 0) {
				continue;
			}
			CIRCLEQ_REMOVE(&spp->spi_swapdev, sdp, swd_next);
			CIRCLEQ_INSERT_TAIL(&spp->spi_swapdev, sdp, swd_next);
			sdp->swd_inuse += size;
			simple_unlock(&swaplist_lock);
			return (daddr_t)(result + sdp->swd_mapoffset);
		}
	}
	simple_unlock(&swaplist_lock);
	return 0;
}

void
swap_free(size, addr)
	int size;
	daddr_t addr;
{
	struct swapdev *sdp = swap_getsdpfromaddr(addr);

#ifdef DIAGNOSTIC
	if (sdp == NULL)
		panic("swap_free: unmapped address\n");
	if (nswapdev < 1)
		panic("swap_free: nswapdev < 1\n");
#endif
	extent_free(sdp->swd_ex, addr - sdp->swd_mapoffset, size,
		    EX_MALLOCOK|EX_NOWAIT);
	sdp->swd_inuse -= size;
#ifdef DIAGNOSTIC
	if (sdp->swd_inuse < 0)
		panic("swap_free: inuse < 0");
#endif
}

/*
 * We have a physical -> virtual mapping to address here.  There are several
 * different physical address spaces (one for each swap partition) that are
 * to be mapped onto a single virtual address space.
 */
#define ADDR_IN_MAP(addr, sdp) \
	(((addr) >= (sdp)->swd_mapoffset) && \
 	 ((addr) < ((sdp)->swd_mapoffset + (sdp)->swd_mapsize)))

struct swapdev *
swap_getsdpfromaddr(addr)
	daddr_t addr;
{
	struct swapdev *sdp;
	struct swappri *spp;
	
	simple_lock(&swaplist_lock);
	for (spp = swap_priority.lh_first; spp != NULL;
	     spp = spp->spi_swappri.le_next)
		for (sdp = spp->spi_swapdev.cqh_first;
		     sdp != (void *)&spp->spi_swapdev;
		     sdp = sdp->swd_next.cqe_next)
			if (ADDR_IN_MAP(addr, sdp)) {
				simple_unlock(&swaplist_lock);
				return sdp;
			}
	simple_unlock(&swaplist_lock);
	return NULL;
}

void
swap_addmap(sdp, size)
	struct swapdev *sdp;
	int	size;
{
	u_long result;

	if (extent_alloc(swapmap, size, EX_NOALIGN, EX_NOBOUNDARY,
			 EX_WAITOK, &result))
		panic("swap_addmap");

	sdp->swd_mapoffset = result;
	sdp->swd_mapsize = size;
}

/*ARGSUSED*/
int
swread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(swstrategy, NULL, dev, B_READ, minphys, uio));
}

/*ARGSUSED*/
int
swwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(swstrategy, NULL, dev, B_WRITE, minphys, uio));
}

void
swstrategy(bp)
	struct buf *bp;
{
	struct swapdev *sdp;
	struct vnode *vp;
	daddr_t	bn;

	bn = bp->b_blkno;
	sdp = swap_getsdpfromaddr(bn);
	if (sdp == NULL) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	bn -= sdp->swd_mapoffset;

	DPRINTF(VMSDB_SWFLOW,
		("swstrategy(%s): mapoff %x, bn %x, bcount %ld\n",
			((bp->b_flags & B_READ) == 0) ? "write" : "read",
			sdp->swd_mapoffset, bn, bp->b_bcount));

	switch (sdp->swd_vp->v_type) {
	default:
		panic("swstrategy: vnode type %x", sdp->swd_vp->v_type);
	case VBLK:
		bp->b_blkno = bn + ctod(CLSIZE);
		vp = sdp->swd_vp;
		bp->b_dev = sdp->swd_dev;
		VHOLD(vp);
		if ((bp->b_flags & B_READ) == 0) {
			int s = splbio();
			vwakeup(bp);
			vp->v_numoutput++;
			splx(s);
		}

		if (bp->b_vp != NULL)
			brelvp(bp);

		bp->b_vp = vp;
		VOP_STRATEGY(bp);
		return;
#ifdef SWAP_TO_FILES
	case VREG:
		sw_reg_strategy(sdp, bp, bn);
		return;
#endif
	}
	/* NOTREACHED */
}

#ifdef SWAP_TO_FILES

STATIC void
sw_reg_strategy(sdp, bp, bn)
	struct swapdev	*sdp;
	struct buf	*bp;
	int		bn;
{
	struct vnode	*vp;
	struct vndxfer	*vnx;
	daddr_t		nbn;
	caddr_t		addr;
	int		s, off, nra, error, sz, resid;

	/*
	 * Translate the device logical block numbers into physical
	 * block numbers of the underlying filesystem device.
	 */
	bp->b_resid = bp->b_bcount;
	addr = bp->b_data;
	bn   = dbtob(bn);

	/* Allocate a header for this transfer and link it to the buffer */
	vnx = getvndxfer();
	vnx->vx_flags = VX_BUSY;
	vnx->vx_error = 0;
	vnx->vx_pending = 0;
	vnx->vx_bp = bp;
	vnx->vx_sdp = sdp;

	error = 0;
	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vndbuf	*nbp;

		nra = 0;
		error = VOP_BMAP(sdp->swd_vp, bn / sdp->swd_bsize,
				 	&vp, &nbn, &nra);

		if (error == 0 && (long)nbn == -1)
			error = EIO;

		/*
		 * If there was an error or a hole in the file...punt.
		 * Note that we may have to wait for any operations
		 * that we have already fired off before releasing
		 * the buffer.
		 *
		 * XXX we could deal with holes here but it would be
		 * a hassle (in the write case).
		 */
		if (error) {
			s = splbio();
			vnx->vx_error = error;
			goto out;
		}

		if ((off = bn % sdp->swd_bsize) != 0)
			sz = sdp->swd_bsize - off;
		else
			sz = (1 + nra) * sdp->swd_bsize;

		if (resid < sz)
			sz = resid;

		DPRINTF(VMSDB_SWFLOW,
			("sw_reg_strategy: vp %p/%p bn 0x%x/0x%x"
				" sz 0x%x\n", sdp->swd_vp, vp, bn, nbn, sz));

		nbp = getvndbuf();
		nbp->vb_buf.b_flags    = bp->b_flags | B_NOCACHE | B_CALL;
		nbp->vb_buf.b_bcount   = sz;
		nbp->vb_buf.b_bufsize  = bp->b_bufsize;
		nbp->vb_buf.b_error    = 0;
		nbp->vb_buf.b_data     = addr;
		nbp->vb_buf.b_blkno    = nbn + btodb(off);
		nbp->vb_buf.b_proc     = bp->b_proc;
		nbp->vb_buf.b_iodone   = sw_reg_iodone;
		nbp->vb_buf.b_vp       = NULLVP;
		nbp->vb_buf.b_rcred    = sdp->swd_cred;
		nbp->vb_buf.b_wcred    = sdp->swd_cred;
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

		nbp->vb_xfer = vnx;

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
		bgetvp(vp, &nbp->vb_buf);
		disksort(&sdp->swd_tab, &nbp->vb_buf);
		sw_reg_start(sdp);
		splx(s);

		bn   += sz;
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
 * Feed requests sequentially.
 * We do it this way to keep from flooding NFS servers if we are connected
 * to an NFS file.  This places the burden on the client rather than the
 * server.
 */
STATIC void
sw_reg_start(sdp)
	struct swapdev	*sdp;
{
	struct buf	*bp;

	if ((sdp->swd_flags & SWF_BUSY) != 0)
		/* Recursion control */
		return;

	sdp->swd_flags |= SWF_BUSY;

	while (sdp->swd_tab.b_active < sdp->swd_maxactive) {
		bp = sdp->swd_tab.b_actf;
		if (bp == NULL)
			break;
		sdp->swd_tab.b_actf = bp->b_actf;
		sdp->swd_tab.b_active++;

		DPRINTF(VMSDB_SWFLOW,
			("sw_reg_start: bp %p vp %p blkno %x addr %p cnt %lx\n",
			bp, bp->b_vp, bp->b_blkno,bp->b_data, bp->b_bcount));

		if ((bp->b_flags & B_READ) == 0)
			bp->b_vp->v_numoutput++;
		VOP_STRATEGY(bp);
	}
	sdp->swd_flags &= ~SWF_BUSY;
}

STATIC void
sw_reg_iodone(bp)
	struct buf *bp;
{
	register struct vndbuf *vbp = BUF_TO_VNDBUF(bp);
	register struct vndxfer *vnx = (struct vndxfer *)vbp->vb_xfer;
	register struct buf *pbp = vnx->vx_bp;
	struct swapdev	*sdp = vnx->vx_sdp;
	int		s, resid;

	DPRINTF(VMSDB_SWFLOW,
		("sw_reg_iodone: vbp %p vp %p blkno %x addr %p "
			"cnt %lx(%lx)\n",
			vbp, vbp->vb_buf.b_vp, vbp->vb_buf.b_blkno,
			vbp->vb_buf.b_data, vbp->vb_buf.b_bcount,
			vbp->vb_buf.b_resid));

	s = splbio();
	resid = vbp->vb_buf.b_bcount - vbp->vb_buf.b_resid;
	pbp->b_resid -= resid;
	vnx->vx_pending--;

	if (vbp->vb_buf.b_error) {
		DPRINTF(VMSDB_INFO, ("sw_reg_iodone: vbp %p error %d\n", vbp,
					vbp->vb_buf.b_error));

		vnx->vx_error = vbp->vb_buf.b_error;
	}

	if (vbp->vb_buf.b_vp != NULLVP)
		brelvp(&vbp->vb_buf);

	putvndbuf(vbp);

	/*
	 * Wrap up this transaction if it has run to completion or, in
	 * case of an error, when all auxiliary buffers have returned.
	 */
	if (vnx->vx_error != 0) {
		pbp->b_flags |= B_ERROR;
		pbp->b_error = vnx->vx_error;
		if ((vnx->vx_flags & VX_BUSY) == 0 && vnx->vx_pending == 0) {

			DPRINTF(VMSDB_SWFLOW,
				("swiodone: pbp %p iodone: error %d\n",
				pbp, vnx->vx_error));
			putvndxfer(vnx);
			biodone(pbp);
		}
	} else if (pbp->b_resid == 0) {

#ifdef DIAGNOSTIC
		if (vnx->vx_pending != 0)
			panic("swiodone: vnx pending: %d", vnx->vx_pending);
#endif

		if ((vnx->vx_flags & VX_BUSY) == 0) {
			DPRINTF(VMSDB_SWFLOW,
				("swiodone: pbp %p iodone\n", pbp));
			putvndxfer(vnx);
			biodone(pbp);
		}
	}

	sdp->swd_tab.b_active--;
	sw_reg_start(sdp);

	splx(s);
}
#endif /* SWAP_TO_FILES */

void
swapinit()
{
	struct buf *sp = swbuf;
	struct proc *p = &proc0;       /* XXX */
	int i;

	DPRINTF(VMSDB_SWINIT, ("swapinit\n"));

	nswapdev = 0;
	if (bdevvp(swapdev, &swapdev_vp))
		panic("swapinit: can not setup swapdev_vp");

	simple_lock_init(&swaplist_lock);
	lockinit(&swaplist_change_lock, PSWP, "swap change", 0, 0);
	LIST_INIT(&swap_priority);

	/*
	 * Create swap block resource map. The range [1..INT_MAX] allows
	 * for a grand total of 2 gigablocks of swap resource.
	 * (start at 1 because "block #0" will be interpreted as
	 *  an allocation failure).
	 */
	swapmap = extent_create("swapmap", 1, INT_MAX,
				M_VMSWAP, 0, 0, EX_WAITOK);
	if (swapmap == 0)
		panic("swapinit: extent_create failed");

	/*
	 * Now set up swap buffer headers.
	 */
	bswlist.b_actf = sp;
	for (i = 0; i < nswbuf - 1; i++, sp++) {
		sp->b_actf = sp + 1;
		sp->b_rcred = sp->b_wcred = p->p_ucred;
		sp->b_vnbufs.le_next = NOLIST;
	}
	sp->b_rcred = sp->b_wcred = p->p_ucred;
	sp->b_vnbufs.le_next = NOLIST;
	sp->b_actf = NULL;

	/* Mount primary swap if available */
#ifdef SWAPDEBUG
	if(vmswap_domount)
#endif
	swapmount();

	DPRINTF(VMSDB_SWINIT, ("leaving swapinit\n"));
}

/*
 * Mount the primary swap device pointed to by 'swdevt[0]'.
 */
STATIC void
swapmount()
{
	extern int getdevvp(dev_t, struct vnode **, enum vtype);
	struct swapdev *sdp;
	struct vnode *vp = NULL;
	struct proc *p = curproc;
	dev_t swap_dev = swdevt[0].sw_dev;

	/* Make sure we have a device */
	if (swap_dev == NODEV) {
		printf("swapmount: No swap device!\n");
		return;
	}

	/* Malloc needed things */
	sdp = (struct swapdev *)malloc(sizeof *sdp, M_VMSWAP, M_WAITOK);
	bzero(sdp, sizeof(*sdp));

	/* Do swap_on() stuff */
	if(bdevvp(swap_dev, &vp)){
		printf("swapmount: bdevvp() failed\n");
		return;
	}

#ifdef SWAPDEBUG
	vprint("swapmount", vp);
#endif

	sdp->swd_vp = vp;
	sdp->swd_dev = (vp->v_type == VBLK) ? vp->v_rdev : NODEV;
	if(copystr("swap_device", sdp->swd_path, sizeof sdp->swd_path, 0) != 0){
		printf("swapmount: copystr() failed\n");
		return;
	}
		
	/* Look for a swap device */
	if (swap_on(p, sdp) != 0) {
		free((caddr_t)sdp, M_VMSWAP);
		return;
	}

#ifdef SWAP_TO_FILES
	/*
	 * XXX Is NFS elaboration necessary?
	 */
	if (vp->v_type == VREG)
		sdp->swd_cred = crdup(p->p_ucred);
#endif
	insert_swapdev(sdp, 0);
}
