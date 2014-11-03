/*	$OpenBSD: vfs_subr.c,v 1.220 2014/11/03 03:08:00 deraadt Exp $	*/
/*	$NetBSD: vfs_subr.c,v 1.53 1996/04/22 01:39:13 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)vfs_subr.c	8.13 (Berkeley) 4/18/94
 */

/*
 * External virtual filesystem routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/acct.h>
#include <sys/namei.h>
#include <sys/ucred.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/syscallargs.h>
#include <sys/pool.h>
#include <sys/tree.h>
#include <sys/specdev.h>

#include <netinet/in.h>

#include "softraid.h"

void sr_shutdown(void);

enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};

int	vttoif_tab[9] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK,
	S_IFSOCK, S_IFIFO, S_IFMT,
};

int doforce = 1;		/* 1 => permit forcible unmounting */
int prtactive = 0;		/* 1 => print out reclaim of active vnodes */
int suid_clear = 1;		/* 1 => clear SUID / SGID on owner change */

/*
 * Insq/Remq for the vnode usage lists.
 */
#define	bufinsvn(bp, dp)	LIST_INSERT_HEAD(dp, bp, b_vnbufs)
#define	bufremvn(bp) {							\
	LIST_REMOVE(bp, b_vnbufs);					\
	LIST_NEXT(bp, b_vnbufs) = NOLIST;				\
}

struct freelst vnode_hold_list;	/* list of vnodes referencing buffers */
struct freelst vnode_free_list;	/* vnode free list */

struct mntlist mountlist;	/* mounted filesystem list */

void	vclean(struct vnode *, int, struct proc *);

void insmntque(struct vnode *, struct mount *);
int getdevvp(dev_t, struct vnode **, enum vtype);

int vfs_hang_addrlist(struct mount *, struct netexport *,
				  struct export_args *);
int vfs_free_netcred(struct radix_node *, void *, u_int);
void vfs_free_addrlist(struct netexport *);
void vputonfreelist(struct vnode *);

int vflush_vnode(struct vnode *, void *);
int maxvnodes;

#ifdef DEBUG
void printlockedvnodes(void);
#endif

struct pool vnode_pool;

static int rb_buf_compare(struct buf *b1, struct buf *b2);
RB_GENERATE(buf_rb_bufs, buf, b_rbbufs, rb_buf_compare);

static int
rb_buf_compare(struct buf *b1, struct buf *b2)
{
	if (b1->b_lblkno < b2->b_lblkno)
		return(-1);
	if (b1->b_lblkno > b2->b_lblkno)
		return(1);
	return(0);
}

/*
 * Initialize the vnode management data structures.
 */
void
vntblinit(void)
{
	/* buffer cache may need a vnode for each buffer */
	maxvnodes = 2 * desiredvnodes;
	pool_init(&vnode_pool, sizeof(struct vnode), 0, 0, 0, "vnodes",
	    &pool_allocator_nointr);
	TAILQ_INIT(&vnode_hold_list);
	TAILQ_INIT(&vnode_free_list);
	TAILQ_INIT(&mountlist);
	/*
	 * Initialize the filesystem syncer.
	 */
	vn_initialize_syncerd();
}

/*
 * Mark a mount point as busy. Used to synchronize access and to delay
 * unmounting.
 *
 * Default behaviour is to attempt getting a READ lock and in case of an
 * ongoing unmount, to wait for it to finish and then return failure.
 */
int
vfs_busy(struct mount *mp, int flags)
{
	int rwflags = 0;

	/* new mountpoints need their lock initialised */
	if (mp->mnt_lock.rwl_name == NULL)
		rw_init(&mp->mnt_lock, "vfslock");

	if (flags & VB_WRITE)
		rwflags |= RW_WRITE;
	else
		rwflags |= RW_READ;

	if (flags & VB_WAIT)
		rwflags |= RW_SLEEPFAIL;
	else
		rwflags |= RW_NOSLEEP;

	if (rw_enter(&mp->mnt_lock, rwflags))
		return (EBUSY);

	return (0);
}

/*
 * Free a busy file system
 */
void
vfs_unbusy(struct mount *mp)
{
	rw_exit(&mp->mnt_lock);
}

int
vfs_isbusy(struct mount *mp) 
{
	if (RWLOCK_OWNER(&mp->mnt_lock) > 0)
		return (1);
	else
		return (0);
}

/*
 * Lookup a filesystem type, and if found allocate and initialize
 * a mount structure for it.
 *
 * Devname is usually updated by mount(8) after booting.
 */
int
vfs_rootmountalloc(char *fstypename, char *devname, struct mount **mpp)
{
	struct vfsconf *vfsp;
	struct mount *mp;

	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (!strcmp(vfsp->vfc_name, fstypename))
			break;
	if (vfsp == NULL)
		return (ENODEV);
	mp = malloc(sizeof(struct mount), M_MOUNT, M_WAITOK|M_ZERO);
	(void)vfs_busy(mp, VB_READ|VB_NOWAIT);
	LIST_INIT(&mp->mnt_vnodelist);
	mp->mnt_vfc = vfsp;
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	mp->mnt_vnodecovered = NULLVP;
	vfsp->vfc_refcount++;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strncpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_stat.f_mntonname[0] = '/';
	copystr(devname, mp->mnt_stat.f_mntfromname, MNAMELEN, 0);
	copystr(devname, mp->mnt_stat.f_mntfromspec, MNAMELEN, 0);
	*mpp = mp;
 	return (0);
 }

/*
 * Lookup a mount point by filesystem identifier.
 */
struct mount *
vfs_getvfs(fsid_t *fsid)
{
	struct mount *mp;

	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (mp->mnt_stat.f_fsid.val[0] == fsid->val[0] &&
		    mp->mnt_stat.f_fsid.val[1] == fsid->val[1]) {
			return (mp);
		}
	}

	return (NULL);
}


/*
 * Get a new unique fsid
 */
void
vfs_getnewfsid(struct mount *mp)
{
	static u_short xxxfs_mntid;

	fsid_t tfsid;
	int mtype;

	mtype = mp->mnt_vfc->vfc_typenum;
	mp->mnt_stat.f_fsid.val[0] = makedev(nblkdev + mtype, 0);
	mp->mnt_stat.f_fsid.val[1] = mtype;
	if (xxxfs_mntid == 0)
		++xxxfs_mntid;
	tfsid.val[0] = makedev(nblkdev + mtype, xxxfs_mntid);
	tfsid.val[1] = mtype;
	if (!TAILQ_EMPTY(&mountlist)) {
		while (vfs_getvfs(&tfsid)) {
			tfsid.val[0]++;
			xxxfs_mntid++;
		}
	}
	mp->mnt_stat.f_fsid.val[0] = tfsid.val[0];
}

/*
 * Set vnode attributes to VNOVAL
 */
void
vattr_null(struct vattr *vap)
{

	vap->va_type = VNON;
	/* XXX These next two used to be one line, but for a GCC bug. */
	vap->va_size = VNOVAL;
	vap->va_bytes = VNOVAL;
	vap->va_mode = vap->va_nlink = vap->va_uid = vap->va_gid =
		vap->va_fsid = vap->va_fileid =
		vap->va_blocksize = vap->va_rdev =
		vap->va_atime.tv_sec = vap->va_atime.tv_nsec =
		vap->va_mtime.tv_sec = vap->va_mtime.tv_nsec =
		vap->va_ctime.tv_sec = vap->va_ctime.tv_nsec =
		vap->va_flags = vap->va_gen = VNOVAL;
	vap->va_vaflags = 0;
}

/*
 * Routines having to do with the management of the vnode table.
 */
long numvnodes;

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(enum vtagtype tag, struct mount *mp, struct vops *vops,
    struct vnode **vpp)
{
	struct proc *p = curproc;
	struct freelst *listhd;
	static int toggle;
	struct vnode *vp;
	int s;

	/*
	 * allow maxvnodes to increase if the buffer cache itself
	 * is big enough to justify it. (we don't shrink it ever)
	 */
	maxvnodes = maxvnodes < bcstats.numbufs ? bcstats.numbufs
	    : maxvnodes;

	/*
	 * We must choose whether to allocate a new vnode or recycle an
	 * existing one. The criterion for allocating a new one is that
	 * the total number of vnodes is less than the number desired or
	 * there are no vnodes on either free list. Generally we only
	 * want to recycle vnodes that have no buffers associated with
	 * them, so we look first on the vnode_free_list. If it is empty,
	 * we next consider vnodes with referencing buffers on the
	 * vnode_hold_list. The toggle ensures that half the time we
	 * will use a buffer from the vnode_hold_list, and half the time
	 * we will allocate a new one unless the list has grown to twice
	 * the desired size. We are reticent to recycle vnodes from the
	 * vnode_hold_list because we will lose the identity of all its
	 * referencing buffers.
	 */
	toggle ^= 1;
	if (numvnodes / 2 > maxvnodes)
		toggle = 0;

	s = splbio();
	if ((numvnodes < maxvnodes) ||
	    ((TAILQ_FIRST(listhd = &vnode_free_list) == NULL) &&
	    ((TAILQ_FIRST(listhd = &vnode_hold_list) == NULL) || toggle))) {
		splx(s);
		vp = pool_get(&vnode_pool, PR_WAITOK | PR_ZERO);
		RB_INIT(&vp->v_bufs_tree);
		RB_INIT(&vp->v_nc_tree);
		TAILQ_INIT(&vp->v_cache_dst);
		numvnodes++;
	} else {
		for (vp = TAILQ_FIRST(listhd); vp != NULLVP;
		    vp = TAILQ_NEXT(vp, v_freelist)) {
			if (VOP_ISLOCKED(vp) == 0)
				break;
		}
		/*
		 * Unless this is a bad time of the month, at most
		 * the first NCPUS items on the free list are
		 * locked, so this is close enough to being empty.
		 */
		if (vp == NULL) {
			splx(s);
			tablefull("vnode");
			*vpp = 0;
			return (ENFILE);
		}

#ifdef DIAGNOSTIC
		if (vp->v_usecount) {
			vprint("free vnode", vp);
			panic("free vnode isn't");
		}
#endif

		TAILQ_REMOVE(listhd, vp, v_freelist);
		vp->v_bioflag &= ~VBIOONFREELIST;
		splx(s);

		if (vp->v_type != VBAD)
			vgonel(vp, p);
#ifdef DIAGNOSTIC
		if (vp->v_data) {
			vprint("cleaned vnode", vp);
			panic("cleaned vnode isn't");
		}
		s = splbio();
		if (vp->v_numoutput)
			panic("Clean vnode has pending I/O's");
		splx(s);
#endif
		vp->v_flag = 0;
		vp->v_socket = 0;
	}
	cache_purge(vp);
	vp->v_type = VNON;
	vp->v_tag = tag;
	vp->v_op = vops;
	insmntque(vp, mp);
	*vpp = vp;
	vp->v_usecount = 1;
	vp->v_data = 0;
	simple_lock_init(&vp->v_uvm.u_obj.vmobjlock);
	return (0);
}

/*
 * Move a vnode from one mount queue to another.
 */
void
insmntque(struct vnode *vp, struct mount *mp)
{
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mount != NULL)
		LIST_REMOVE(vp, v_mntvnodes);
	/*
	 * Insert into list of vnodes for the new mount point, if available.
	 */
	if ((vp->v_mount = mp) != NULL)
		LIST_INSERT_HEAD(&mp->mnt_vnodelist, vp, v_mntvnodes);
}

/*
 * Create a vnode for a block device.
 * Used for root filesystem, argdev, and swap areas.
 * Also used for memory file system special devices.
 */
int
bdevvp(dev_t dev, struct vnode **vpp)
{
	return (getdevvp(dev, vpp, VBLK));
}

/*
 * Create a vnode for a character device.
 * Used for console handling.
 */
int
cdevvp(dev_t dev, struct vnode **vpp)
{
	return (getdevvp(dev, vpp, VCHR));
}

/*
 * Create a vnode for a device.
 * Used by bdevvp (block device) for root file system etc.,
 * and by cdevvp (character device) for console.
 */
int
getdevvp(dev_t dev, struct vnode **vpp, enum vtype type)
{
	struct vnode *vp;
	struct vnode *nvp;
	int error;

	if (dev == NODEV) {
		*vpp = NULLVP;
		return (0);
	}
	error = getnewvnode(VT_NON, NULL, &spec_vops, &nvp);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	vp = nvp;
	vp->v_type = type;
	if ((nvp = checkalias(vp, dev, NULL)) != 0) {
		vput(vp);
		vp = nvp;
	}
	*vpp = vp;
	return (0);
}

/*
 * Check to see if the new vnode represents a special device
 * for which we already have a vnode (either because of
 * bdevvp() or because of a different vnode representing
 * the same block device). If such an alias exists, deallocate
 * the existing contents and return the aliased vnode. The
 * caller is responsible for filling it with its new contents.
 */
struct vnode *
checkalias(struct vnode *nvp, dev_t nvp_rdev, struct mount *mp)
{
	struct proc *p = curproc;
	struct vnode *vp;
	struct vnode **vpp;

	if (nvp->v_type != VBLK && nvp->v_type != VCHR)
		return (NULLVP);

	vpp = &speclisth[SPECHASH(nvp_rdev)];
loop:
	for (vp = *vpp; vp; vp = vp->v_specnext) {
		if (nvp_rdev != vp->v_rdev || nvp->v_type != vp->v_type) {
			continue;
		}
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vp->v_usecount == 0) {
			vgonel(vp, p);
			goto loop;
		}
		if (vget(vp, LK_EXCLUSIVE, p)) {
			goto loop;
		}
		break;
	}

	/*
	 * Common case is actually in the if statement
	 */
	if (vp == NULL || !(vp->v_tag == VT_NON && vp->v_type == VBLK)) {
		nvp->v_specinfo = malloc(sizeof(struct specinfo), M_VNODE,
			M_WAITOK);
		nvp->v_rdev = nvp_rdev;
		nvp->v_hashchain = vpp;
		nvp->v_specnext = *vpp;
		nvp->v_specmountpoint = NULL;
		nvp->v_speclockf = NULL;
		memset(nvp->v_specbitmap, 0, sizeof(nvp->v_specbitmap));
		*vpp = nvp;
		if (vp != NULLVP) {
			nvp->v_flag |= VALIASED;
			vp->v_flag |= VALIASED;
			vput(vp);
		}
		return (NULLVP);
	}

	/*
	 * This code is the uncommon case. It is called in case
	 * we found an alias that was VT_NON && vtype of VBLK
	 * This means we found a block device that was created
	 * using bdevvp.
	 * An example of such a vnode is the root partition device vnode
	 * created in ffs_mountroot.
	 *
	 * The vnodes created by bdevvp should not be aliased (why?).
	 */

	VOP_UNLOCK(vp, 0, p);
	vclean(vp, 0, p);
	vp->v_op = nvp->v_op;
	vp->v_tag = nvp->v_tag;
	nvp->v_type = VNON;
	insmntque(vp, mp);
	return (vp);
}

/*
 * Grab a particular vnode from the free list, increment its
 * reference count and lock it. If the vnode lock bit is set,
 * the vnode is being eliminated in vgone. In that case, we
 * cannot grab it, so the process is awakened when the
 * transition is completed, and an error code is returned to
 * indicate that the vnode is no longer usable, possibly
 * having been changed to a new file system type.
 */
int
vget(struct vnode *vp, int flags, struct proc *p)
{
	int error, s, onfreelist;

	/*
	 * If the vnode is in the process of being cleaned out for
	 * another use, we wait for the cleaning to finish and then
	 * return failure. Cleaning is determined by checking that
	 * the VXLOCK flag is set.
	 */

	if (vp->v_flag & VXLOCK) {
		if (flags & LK_NOWAIT) {
			return (EBUSY);
		}

		vp->v_flag |= VXWANT;
		tsleep(vp, PINOD, "vget", 0);
		return (ENOENT);
	}

	onfreelist = vp->v_bioflag & VBIOONFREELIST;
	if (vp->v_usecount == 0 && onfreelist) {
		s = splbio();
		if (vp->v_holdcnt > 0)
			TAILQ_REMOVE(&vnode_hold_list, vp, v_freelist);
		else
			TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		vp->v_bioflag &= ~VBIOONFREELIST;
		splx(s);
	}

 	vp->v_usecount++;
	if (flags & LK_TYPE_MASK) {
		if ((error = vn_lock(vp, flags, p)) != 0) {
			vp->v_usecount--;
			if (vp->v_usecount == 0 && onfreelist)
				vputonfreelist(vp);
		}
		return (error);
	}

	return (0);
}


/* Vnode reference. */
void
vref(struct vnode *vp)
{
#ifdef DIAGNOSTIC
	if (vp->v_usecount == 0)
		panic("vref used where vget required");
	if (vp->v_type == VNON)
		panic("vref on a VNON vnode");
#endif
	vp->v_usecount++;
}

void
vputonfreelist(struct vnode *vp)
{
	int s;
	struct freelst *lst;

	s = splbio();
#ifdef DIAGNOSTIC
	if (vp->v_usecount != 0)
		panic("Use count is not zero!");

	if (vp->v_bioflag & VBIOONFREELIST) {
		vprint("vnode already on free list: ", vp);
		panic("vnode already on free list");
	}
#endif

	vp->v_bioflag |= VBIOONFREELIST;

	if (vp->v_holdcnt > 0)
		lst = &vnode_hold_list;
	else
		lst = &vnode_free_list;

	if (vp->v_type == VBAD)
		TAILQ_INSERT_HEAD(lst, vp, v_freelist);
	else
		TAILQ_INSERT_TAIL(lst, vp, v_freelist);

	splx(s);
}

/*
 * vput(), just unlock and vrele()
 */
void
vput(struct vnode *vp)
{
	struct proc *p = curproc;

#ifdef DIAGNOSTIC
	if (vp == NULL)
		panic("vput: null vp");
#endif

#ifdef DIAGNOSTIC
	if (vp->v_usecount == 0) {
		vprint("vput: bad ref count", vp);
		panic("vput: ref cnt");
	}
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0) {
		VOP_UNLOCK(vp, 0, p);
		return;
	}

#ifdef DIAGNOSTIC
	if (vp->v_writecount != 0) {
		vprint("vput: bad writecount", vp);
		panic("vput: v_writecount != 0");
	}
#endif

	VOP_INACTIVE(vp, p);

	if (vp->v_usecount == 0 && !(vp->v_bioflag & VBIOONFREELIST))
		vputonfreelist(vp);
}

/*
 * Vnode release - use for active VNODES.
 * If count drops to zero, call inactive routine and return to freelist.
 * Returns 0 if it did not sleep.
 */
int
vrele(struct vnode *vp)
{
	struct proc *p = curproc;

#ifdef DIAGNOSTIC
	if (vp == NULL)
		panic("vrele: null vp");
#endif
#ifdef DIAGNOSTIC
	if (vp->v_usecount == 0) {
		vprint("vrele: bad ref count", vp);
		panic("vrele: ref cnt");
	}
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0) {
		return (0);
	}

#ifdef DIAGNOSTIC
	if (vp->v_writecount != 0) {
		vprint("vrele: bad writecount", vp);
		panic("vrele: v_writecount != 0");
	}
#endif

	if (vn_lock(vp, LK_EXCLUSIVE, p)) {
#ifdef DIAGNOSTIC
		vprint("vrele: cannot lock", vp);
#endif
		return (1);
	}

	VOP_INACTIVE(vp, p);

	if (vp->v_usecount == 0 && !(vp->v_bioflag & VBIOONFREELIST))
		vputonfreelist(vp);
	return (1);
}

/* Page or buffer structure gets a reference. */
void
vhold(struct vnode *vp)
{
	/*
	 * If it is on the freelist and the hold count is currently
	 * zero, move it to the hold list.
	 */
	if ((vp->v_bioflag & VBIOONFREELIST) &&
	    vp->v_holdcnt == 0 && vp->v_usecount == 0) {
		TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
		TAILQ_INSERT_TAIL(&vnode_hold_list, vp, v_freelist);
	}
	vp->v_holdcnt++;
}

/* Lose interest in a vnode. */
void
vdrop(struct vnode *vp)
{
#ifdef DIAGNOSTIC
	if (vp->v_holdcnt == 0)
		panic("vdrop: zero holdcnt");
#endif

	vp->v_holdcnt--;

	/*
	 * If it is on the holdlist and the hold count drops to
	 * zero, move it to the free list.
	 */
	if ((vp->v_bioflag & VBIOONFREELIST) &&
	    vp->v_holdcnt == 0 && vp->v_usecount == 0) {
		TAILQ_REMOVE(&vnode_hold_list, vp, v_freelist);
		TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
	}
}

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If MNT_NOFORCE is specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If MNT_FORCE is specified, detach any active vnodes
 * that are found.
 */
#ifdef DEBUG
int busyprt = 0;	/* print out busy vnodes */
struct ctldebug debug1 = { "busyprt", &busyprt };
#endif

int
vfs_mount_foreach_vnode(struct mount *mp, 
    int (*func)(struct vnode *, void *), void *arg) {
	struct vnode *vp, *nvp;
	int error = 0;

loop:
	for (vp = LIST_FIRST(&mp->mnt_vnodelist); vp != NULL; vp = nvp) {
		if (vp->v_mount != mp)
			goto loop;
		nvp = LIST_NEXT(vp, v_mntvnodes);

		error = func(vp, arg);

		if (error != 0)
			break;
	}

	return (error);
}

struct vflush_args {
	struct vnode *skipvp;
	int busy;
	int flags;
};

int
vflush_vnode(struct vnode *vp, void *arg) {
	struct vflush_args *va = arg;
	struct proc *p = curproc;

	if (vp == va->skipvp) {
		return (0);
	}

	if ((va->flags & SKIPSYSTEM) && (vp->v_flag & VSYSTEM)) {
		return (0);
	}

	/*
	 * If WRITECLOSE is set, only flush out regular file
	 * vnodes open for writing.
	 */
	if ((va->flags & WRITECLOSE) &&
	    (vp->v_writecount == 0 || vp->v_type != VREG)) {
		return (0);
	}

	/*
	 * With v_usecount == 0, all we need to do is clear
	 * out the vnode data structures and we are done.
	 */
	if (vp->v_usecount == 0) {
		vgonel(vp, p);
		return (0);
	}

	/*
	 * If FORCECLOSE is set, forcibly close the vnode.
	 * For block or character devices, revert to an
	 * anonymous device. For all other files, just kill them.
	 */
	if (va->flags & FORCECLOSE) {
		if (vp->v_type != VBLK && vp->v_type != VCHR) {
			vgonel(vp, p);
		} else {
			vclean(vp, 0, p);
			vp->v_op = &spec_vops;
			insmntque(vp, (struct mount *)0);
		}
		return (0);
	}

#ifdef DEBUG
	if (busyprt)
		vprint("vflush: busy vnode", vp);
#endif
	va->busy++;
	return (0);
}

int
vflush(struct mount *mp, struct vnode *skipvp, int flags)
{
	struct vflush_args va;
	va.skipvp = skipvp;
	va.busy = 0;
	va.flags = flags;

	vfs_mount_foreach_vnode(mp, vflush_vnode, &va);

	if (va.busy)
		return (EBUSY);
	return (0);
}

/*
 * Disassociate the underlying file system from a vnode.
 */
void
vclean(struct vnode *vp, int flags, struct proc *p)
{
	int active;

	/*
	 * Check to see if the vnode is in use.
	 * If so we have to reference it before we clean it out
	 * so that its count cannot fall to zero and generate a
	 * race against ourselves to recycle it.
	 */
	if ((active = vp->v_usecount) != 0)
		vp->v_usecount++;

	/*
	 * Prevent the vnode from being recycled or
	 * brought into use while we clean it out.
	 */
	if (vp->v_flag & VXLOCK)
		panic("vclean: deadlock");
	vp->v_flag |= VXLOCK;
	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still
	 * have the object locked while it cleans it out. The VOP_LOCK
	 * ensures that the VOP_INACTIVE routine is done with its work.
	 * For active vnodes, it ensures that no other activity can
	 * occur while the underlying object is being cleaned out.
	 */
	VOP_LOCK(vp, LK_DRAIN, p);

	/*
	 * Clean out any VM data associated with the vnode.
	 */
	uvm_vnp_terminate(vp);
	/*
	 * Clean out any buffers associated with the vnode.
	 */
	if (flags & DOCLOSE)
		vinvalbuf(vp, V_SAVE, NOCRED, p, 0, 0);
	/*
	 * If purging an active vnode, it must be closed and
	 * deactivated before being reclaimed. Note that the
	 * VOP_INACTIVE will unlock the vnode
	 */
	if (active) {
		if (flags & DOCLOSE)
			VOP_CLOSE(vp, FNONBLOCK, NOCRED, p);
		VOP_INACTIVE(vp, p);
	} else {
		/*
		 * Any other processes trying to obtain this lock must first
		 * wait for VXLOCK to clear, then call the new lock operation.
		 */
		VOP_UNLOCK(vp, 0, p);
	}

	/*
	 * Reclaim the vnode.
	 */
	if (VOP_RECLAIM(vp, p))
		panic("vclean: cannot reclaim");
	if (active) {
		vp->v_usecount--;
		if (vp->v_usecount == 0) {
			if (vp->v_holdcnt > 0)
				panic("vclean: not clean");
			vputonfreelist(vp);
		}
	}
	cache_purge(vp);

	/*
	 * Done with purge, notify sleepers of the grim news.
	 */
	vp->v_op = &dead_vops;
	VN_KNOTE(vp, NOTE_REVOKE);
	vp->v_tag = VT_NON;
	vp->v_flag &= ~VXLOCK;
#ifdef VFSLCKDEBUG
	vp->v_flag &= ~VLOCKSWORK;
#endif
	if (vp->v_flag & VXWANT) {
		vp->v_flag &= ~VXWANT;
		wakeup(vp);
	}
}

/*
 * Recycle an unused vnode to the front of the free list.
 */
int
vrecycle(struct vnode *vp, struct proc *p)
{
	if (vp->v_usecount == 0) {
		vgonel(vp, p);
		return (1);
	}
	return (0);
}

/*
 * Eliminate all activity associated with a vnode
 * in preparation for reuse.
 */
void
vgone(struct vnode *vp)
{
	struct proc *p = curproc;
	vgonel(vp, p);
}

/*
 * vgone, with struct proc.
 */
void
vgonel(struct vnode *vp, struct proc *p)
{
	struct vnode *vq;
	struct vnode *vx;

	/*
	 * If a vgone (or vclean) is already in progress,
	 * wait until it is done and return.
	 */
	if (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		tsleep(vp, PINOD, "vgone", 0);
		return;
	}

	/*
	 * Clean out the filesystem specific data.
	 */
	vclean(vp, DOCLOSE, p);
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mount != NULL)
		insmntque(vp, (struct mount *)0);
	/*
	 * If special device, remove it from special device alias list
	 * if it is on one.
	 */
	if ((vp->v_type == VBLK || vp->v_type == VCHR) && vp->v_specinfo != 0) {
		if (*vp->v_hashchain == vp) {
			*vp->v_hashchain = vp->v_specnext;
		} else {
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_specnext != vp)
					continue;
				vq->v_specnext = vp->v_specnext;
				break;
			}
			if (vq == NULL)
				panic("missing bdev");
		}
		if (vp->v_flag & VALIASED) {
			vx = NULL;
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type)
					continue;
				if (vx)
					break;
				vx = vq;
			}
			if (vx == NULL)
				panic("missing alias");
			if (vq == NULL)
				vx->v_flag &= ~VALIASED;
			vp->v_flag &= ~VALIASED;
		}
		free(vp->v_specinfo, M_VNODE, sizeof(struct specinfo));
		vp->v_specinfo = NULL;
	}
	/*
	 * If it is on the freelist and not already at the head,
	 * move it to the head of the list.
	 */
	vp->v_type = VBAD;

	/*
	 * Move onto the free list, unless we were called from
	 * getnewvnode and we're not on any free list
	 */
	if (vp->v_usecount == 0 &&
	    (vp->v_bioflag & VBIOONFREELIST)) {
		int s;

		s = splbio();

		if (vp->v_holdcnt > 0)
			panic("vgonel: not clean");

		if (TAILQ_FIRST(&vnode_free_list) != vp) {
			TAILQ_REMOVE(&vnode_free_list, vp, v_freelist);
			TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
		}
		splx(s);
	}
}

/*
 * Lookup a vnode by device number.
 */
int
vfinddev(dev_t dev, enum vtype type, struct vnode **vpp)
{
	struct vnode *vp;
	int rc =0;

	for (vp = speclisth[SPECHASH(dev)]; vp; vp = vp->v_specnext) {
		if (dev != vp->v_rdev || type != vp->v_type)
			continue;
		*vpp = vp;
		rc = 1;
		break;
	}
	return (rc);
}

/*
 * Revoke all the vnodes corresponding to the specified minor number
 * range (endpoints inclusive) of the specified major.
 */
void
vdevgone(int maj, int minl, int minh, enum vtype type)
{
	struct vnode *vp;
	int mn;

	for (mn = minl; mn <= minh; mn++)
		if (vfinddev(makedev(maj, mn), type, &vp))
			VOP_REVOKE(vp, REVOKEALL);
}

/*
 * Calculate the total number of references to a special device.
 */
int
vcount(struct vnode *vp)
{
	struct vnode *vq, *vnext;
	int count;

loop:
	if ((vp->v_flag & VALIASED) == 0)
		return (vp->v_usecount);
	for (count = 0, vq = *vp->v_hashchain; vq; vq = vnext) {
		vnext = vq->v_specnext;
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		if (vq->v_usecount == 0 && vq != vp) {
			vgone(vq);
			goto loop;
		}
		count += vq->v_usecount;
	}
	return (count);
}

#if defined(DEBUG) || defined(DIAGNOSTIC)
/*
 * Print out a description of a vnode.
 */
static char *typename[] =
   { "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD" };

void
vprint(char *label, struct vnode *vp)
{
	char buf[64];

	if (label != NULL)
		printf("%s: ", label);
	printf("%p, type %s, use %u, write %u, hold %u,",
		vp, typename[vp->v_type], vp->v_usecount, vp->v_writecount,
		vp->v_holdcnt);
	buf[0] = '\0';
	if (vp->v_flag & VROOT)
		strlcat(buf, "|VROOT", sizeof buf);
	if (vp->v_flag & VTEXT)
		strlcat(buf, "|VTEXT", sizeof buf);
	if (vp->v_flag & VSYSTEM)
		strlcat(buf, "|VSYSTEM", sizeof buf);
	if (vp->v_flag & VXLOCK)
		strlcat(buf, "|VXLOCK", sizeof buf);
	if (vp->v_flag & VXWANT)
		strlcat(buf, "|VXWANT", sizeof buf);
	if (vp->v_bioflag & VBIOWAIT)
		strlcat(buf, "|VBIOWAIT", sizeof buf);
	if (vp->v_bioflag & VBIOONFREELIST)
		strlcat(buf, "|VBIOONFREELIST", sizeof buf);
	if (vp->v_bioflag & VBIOONSYNCLIST)
		strlcat(buf, "|VBIOONSYNCLIST", sizeof buf);
	if (vp->v_flag & VALIASED)
		strlcat(buf, "|VALIASED", sizeof buf);
	if (buf[0] != '\0')
		printf(" flags (%s)", &buf[1]);
	if (vp->v_data == NULL) {
		printf("\n");
	} else {
		printf("\n\t");
		VOP_PRINT(vp);
	}
}
#endif /* DEBUG || DIAGNOSTIC */

#ifdef DEBUG
/*
 * List all of the locked vnodes in the system.
 * Called when debugging the kernel.
 */
void
printlockedvnodes(void)
{
	struct mount *mp, *nmp;
	struct vnode *vp;

	printf("Locked vnodes\n");

	TAILQ_FOREACH_SAFE(mp, &mountlist, mnt_list, nmp) {
		if (vfs_busy(mp, VB_READ|VB_NOWAIT))
			continue;
		LIST_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes) {
			if (VOP_ISLOCKED(vp))
				vprint((char *)0, vp);
		}
		vfs_unbusy(mp);
 	}

}
#endif

/*
 * Top level filesystem related information gathering.
 */
int
vfs_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	struct vfsconf *vfsp, *tmpvfsp;
	int ret;

	/* all sysctl names at this level are at least name and field */
	if (namelen < 2)
		return (ENOTDIR);		/* overloaded */

	if (name[0] != VFS_GENERIC) {
		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (vfsp->vfc_typenum == name[0])
				break;

		if (vfsp == NULL)
			return (EOPNOTSUPP);

		return ((*vfsp->vfc_vfsops->vfs_sysctl)(&name[1], namelen - 1,
		    oldp, oldlenp, newp, newlen, p));
	}

	switch (name[1]) {
	case VFS_MAXTYPENUM:
		return (sysctl_rdint(oldp, oldlenp, newp, maxvfsconf));

	case VFS_CONF:
		if (namelen < 3)
			return (ENOTDIR);	/* overloaded */

		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (vfsp->vfc_typenum == name[2])
				break;

		if (vfsp == NULL)
			return (EOPNOTSUPP);

		/* Make a copy, clear out kernel pointers */
		tmpvfsp = malloc(sizeof(*tmpvfsp), M_TEMP, M_WAITOK);
		bcopy(vfsp, tmpvfsp, sizeof(*tmpvfsp));
		tmpvfsp->vfc_vfsops = NULL;
		tmpvfsp->vfc_next = NULL;

		ret = sysctl_rdstruct(oldp, oldlenp, newp, tmpvfsp,
		    sizeof(struct vfsconf));

		free(tmpvfsp, M_TEMP, sizeof(*tmpvfsp));
		return (ret);
	case VFS_BCACHESTAT:	/* buffer cache statistics */
		ret = sysctl_rdstruct(oldp, oldlenp, newp, &bcstats,
		    sizeof(struct bcachestats));
		return(ret);
	}
	return (EOPNOTSUPP);
}

int kinfo_vdebug = 1;
#define KINFO_VNODESLOP	10
/*
 * Dump vnode list (via sysctl).
 * Copyout address of vnode followed by vnode.
 */
/* ARGSUSED */
int
sysctl_vnode(char *where, size_t *sizep, struct proc *p)
{
	struct mount *mp, *nmp;
	struct vnode *vp, *nvp;
	char *bp = where, *savebp;
	char *ewhere;
	int error;

	if (where == NULL) {
		*sizep = (numvnodes + KINFO_VNODESLOP) * sizeof(struct e_vnode);
		return (0);
	}
	ewhere = where + *sizep;

	TAILQ_FOREACH_SAFE(mp, &mountlist, mnt_list, nmp) {
		if (vfs_busy(mp, VB_READ|VB_NOWAIT))
			continue;
		savebp = bp;
again:
		LIST_FOREACH_SAFE(vp, &mp->mnt_vnodelist, v_mntvnodes, nvp) {
			/*
			 * Check that the vp is still associated with
			 * this filesystem.  RACE: could have been
			 * recycled onto the same filesystem.
			 */
			if (vp->v_mount != mp) {
				if (kinfo_vdebug)
					printf("kinfo: vp changed\n");
				bp = savebp;
				goto again;
			}
			if (bp + sizeof(struct e_vnode) > ewhere) {
				*sizep = bp - where;
				vfs_unbusy(mp);
				return (ENOMEM);
			}
			if ((error = copyout(&vp,
			    &((struct e_vnode *)bp)->vptr,
			    sizeof(struct vnode *))) ||
			   (error = copyout(vp,
			    &((struct e_vnode *)bp)->vnode,
			    sizeof(struct vnode)))) {
				vfs_unbusy(mp);
				return (error);
			}
			bp += sizeof(struct e_vnode);
		}

		vfs_unbusy(mp);
	}

	*sizep = bp - where;

	return (0);
}

/*
 * Check to see if a filesystem is mounted on a block device.
 */
int
vfs_mountedon(struct vnode *vp)
{
	struct vnode *vq;
	int error = 0;

 	if (vp->v_specmountpoint != NULL)
		return (EBUSY);
	if (vp->v_flag & VALIASED) {
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type)
				continue;
			if (vq->v_specmountpoint != NULL) {
				error = EBUSY;
				break;
			}
 		}
	}
	return (error);
}

/*
 * Build hash lists of net addresses and hang them off the mount point.
 * Called by ufs_mount() to set up the lists of export addresses.
 */
int
vfs_hang_addrlist(struct mount *mp, struct netexport *nep,
    struct export_args *argp)
{
	struct netcred *np;
	struct radix_node_head *rnh;
	int nplen, i;
	struct radix_node *rn;
	struct sockaddr *saddr, *smask = 0;
	int error;

	if (argp->ex_addrlen == 0) {
		if (mp->mnt_flag & MNT_DEFEXPORTED)
			return (EPERM);
		np = &nep->ne_defexported;
		mp->mnt_flag |= MNT_DEFEXPORTED;
		goto finish;
	}
	if (argp->ex_addrlen > MLEN || argp->ex_masklen > MLEN ||
	    argp->ex_addrlen < 0 || argp->ex_masklen < 0)
		return (EINVAL);
	nplen = sizeof(struct netcred) + argp->ex_addrlen + argp->ex_masklen;
	np = (struct netcred *)malloc(nplen, M_NETADDR, M_WAITOK|M_ZERO);
	saddr = (struct sockaddr *)(np + 1);
	error = copyin(argp->ex_addr, saddr, argp->ex_addrlen);
	if (error)
		goto out;
	if (saddr->sa_len > argp->ex_addrlen)
		saddr->sa_len = argp->ex_addrlen;
	if (argp->ex_masklen) {
		smask = (struct sockaddr *)((caddr_t)saddr + argp->ex_addrlen);
		error = copyin(argp->ex_mask, smask, argp->ex_masklen);
		if (error)
			goto out;
		if (smask->sa_len > argp->ex_masklen)
			smask->sa_len = argp->ex_masklen;
	}
	i = saddr->sa_family;
	switch (i) {
	case AF_INET:
		if ((rnh = nep->ne_rtable_inet) == NULL) {
			if (!rn_inithead((void **)&nep->ne_rtable_inet,
			    offsetof(struct sockaddr_in, sin_addr) * 8)) {
				error = ENOBUFS;
				goto out;
			}
			rnh = nep->ne_rtable_inet;
		}
		break;
	default:
		error = EINVAL;
		goto out;
	}
	rn = (*rnh->rnh_addaddr)((caddr_t)saddr, (caddr_t)smask, rnh,
		np->netc_rnodes, 0);
	if (rn == 0 || np != (struct netcred *)rn) { /* already exists */
		error = EPERM;
		goto out;
	}
finish:
	np->netc_exflags = argp->ex_flags;
	/* fill in the kernel's ucred from userspace's xucred */
	crfromxucred(&np->netc_anon, &argp->ex_anon);
	return (0);
out:
	free(np, M_NETADDR, nplen);
	return (error);
}

/* ARGSUSED */
int
vfs_free_netcred(struct radix_node *rn, void *w, u_int id)
{
	struct radix_node_head *rnh = (struct radix_node_head *)w;

	(*rnh->rnh_deladdr)(rn->rn_key, rn->rn_mask, rnh, NULL);
	free(rn, M_NETADDR, 0);
	return (0);
}

/*
 * Free the net address hash lists that are hanging off the mount points.
 */
void
vfs_free_addrlist(struct netexport *nep)
{
	struct radix_node_head *rnh;

	if ((rnh = nep->ne_rtable_inet) != NULL) {
		(*rnh->rnh_walktree)(rnh, vfs_free_netcred, rnh);
		free(rnh, M_RTABLE, 0);
		nep->ne_rtable_inet = NULL;
	}
}

int
vfs_export(struct mount *mp, struct netexport *nep, struct export_args *argp)
{
	int error;

	if (argp->ex_flags & MNT_DELEXPORT) {
		vfs_free_addrlist(nep);
		mp->mnt_flag &= ~(MNT_EXPORTED | MNT_DEFEXPORTED);
	}
	if (argp->ex_flags & MNT_EXPORTED) {
		if ((error = vfs_hang_addrlist(mp, nep, argp)) != 0)
			return (error);
		mp->mnt_flag |= MNT_EXPORTED;
	}
	return (0);
}

struct netcred *
vfs_export_lookup(struct mount *mp, struct netexport *nep, struct mbuf *nam)
{
	struct netcred *np;
	struct radix_node_head *rnh;
	struct sockaddr *saddr;

	np = NULL;
	if (mp->mnt_flag & MNT_EXPORTED) {
		/*
		 * Lookup in the export list first.
		 */
		if (nam != NULL) {
			saddr = mtod(nam, struct sockaddr *);
			switch(saddr->sa_family) {
			case AF_INET:
				rnh = nep->ne_rtable_inet;
				break;
			default:
				rnh = NULL;
				break;
			}
			if (rnh != NULL) {
				np = (struct netcred *)
					(*rnh->rnh_matchaddr)((caddr_t)saddr,
					    rnh);
				if (np && np->netc_rnodes->rn_flags & RNF_ROOT)
					np = NULL;
			}
		}
		/*
		 * If no address match, use the default if it exists.
		 */
		if (np == NULL && mp->mnt_flag & MNT_DEFEXPORTED)
			np = &nep->ne_defexported;
	}
	return (np);
}

/*
 * Do the usual access checking.
 * file_mode, uid and gid are from the vnode in question,
 * while acc_mode and cred are from the VOP_ACCESS parameter list
 */
int
vaccess(enum vtype type, mode_t file_mode, uid_t uid, gid_t gid,
    mode_t acc_mode, struct ucred *cred)
{
	mode_t mask;

	/* User id 0 always gets read/write access. */
	if (cred->cr_uid == 0) {
		/* For VEXEC, at least one of the execute bits must be set. */
		if ((acc_mode & VEXEC) && type != VDIR &&
		    (file_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0)
			return EACCES;
		return 0;
	}

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == uid) {
		if (acc_mode & VEXEC)
			mask |= S_IXUSR;
		if (acc_mode & VREAD)
			mask |= S_IRUSR;
		if (acc_mode & VWRITE)
			mask |= S_IWUSR;
		return (file_mode & mask) == mask ? 0 : EACCES;
	}

	/* Otherwise, check the groups. */
	if (groupmember(gid, cred)) {
		if (acc_mode & VEXEC)
			mask |= S_IXGRP;
		if (acc_mode & VREAD)
			mask |= S_IRGRP;
		if (acc_mode & VWRITE)
			mask |= S_IWGRP;
		return (file_mode & mask) == mask ? 0 : EACCES;
	}

	/* Otherwise, check everyone else. */
	if (acc_mode & VEXEC)
		mask |= S_IXOTH;
	if (acc_mode & VREAD)
		mask |= S_IROTH;
	if (acc_mode & VWRITE)
		mask |= S_IWOTH;
	return (file_mode & mask) == mask ? 0 : EACCES;
}

/*
 * Unmount all file systems.
 * We traverse the list in reverse order under the assumption that doing so
 * will avoid needing to worry about dependencies.
 */
void
vfs_unmountall(void)
{
	struct mount *mp, *nmp;
	int allerror, error, again = 1;

 retry:
	allerror = 0;
	TAILQ_FOREACH_REVERSE_SAFE(mp, &mountlist, mntlist, mnt_list, nmp) {
		if ((vfs_busy(mp, VB_WRITE|VB_NOWAIT)) != 0)
			continue;
		if ((error = dounmount(mp, MNT_FORCE, curproc, NULL)) != 0) {
			printf("unmount of %s failed with error %d\n",
			    mp->mnt_stat.f_mntonname, error);
			allerror = 1;
		}
	}

	if (allerror) {
		printf("WARNING: some file systems would not unmount\n");
		if (again) {
			printf("retrying\n");
			again = 0;
			goto retry;
		}
	}
}

/*
 * Sync and unmount file systems before shutting down.
 */
void
vfs_shutdown(void)
{
#ifdef ACCOUNTING
	acct_shutdown();
#endif

	/* XXX Should suspend scheduling. */
	(void) spl0();

	printf("syncing disks... ");

	if (panicstr == 0) {
		/* Sync before unmount, in case we hang on something. */
		sys_sync(&proc0, (void *)0, (register_t *)0);

		/* Unmount file systems. */
		vfs_unmountall();
	}

	if (vfs_syncwait(1))
		printf("giving up\n");
	else
		printf("done\n");

#if NSOFTRAID > 0
	sr_shutdown();
#endif
}

/*
 * perform sync() operation and wait for buffers to flush.
 * assumptions: called w/ scheduler disabled and physical io enabled
 * for now called at spl0() XXX
 */
int
vfs_syncwait(int verbose)
{
	struct buf *bp;
	int iter, nbusy, dcount, s;
	struct proc *p;

	p = curproc? curproc : &proc0;
	sys_sync(p, (void *)0, (register_t *)0);

	/* Wait for sync to finish. */
	dcount = 10000;
	for (iter = 0; iter < 20; iter++) {
		nbusy = 0;
		LIST_FOREACH(bp, &bufhead, b_list) {
			if ((bp->b_flags & (B_BUSY|B_INVAL|B_READ)) == B_BUSY)
				nbusy++;
			/*
			 * With soft updates, some buffers that are
			 * written will be remarked as dirty until other
			 * buffers are written.
			 */
			if (bp->b_flags & B_DELWRI) {
				s = splbio();
				bremfree(bp);
				buf_acquire(bp);
				splx(s);
				nbusy++;
				bawrite(bp);
				if (dcount-- <= 0) {
					if (verbose)
						printf("softdep ");
					return 1;
				}
			}
		}
		if (nbusy == 0)
			break;
		if (verbose)
			printf("%d ", nbusy);
		DELAY(40000 * iter);
	}

	return nbusy;
}

/*
 * posix file system related system variables.
 */
int
fs_posix_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen, struct proc *p)
{
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case FS_POSIX_SETUID:
		if (newp && securelevel > 0)
			return (EPERM);
		return(sysctl_int(oldp, oldlenp, newp, newlen, &suid_clear));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * file system related system variables.
 */
int
fs_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	sysctlfn *fn;

	switch (name[0]) {
	case FS_POSIX:
		fn = fs_posix_sysctl;
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (*fn)(name + 1, namelen - 1, oldp, oldlenp, newp, newlen, p);
}


/*
 * Routines dealing with vnodes and buffers
 */

/*
 * Wait for all outstanding I/Os to complete
 *
 * Manipulates v_numoutput. Must be called at splbio()
 */
int
vwaitforio(struct vnode *vp, int slpflag, char *wmesg, int timeo)
{
	int error = 0;

	splassert(IPL_BIO);

	while (vp->v_numoutput) {
		vp->v_bioflag |= VBIOWAIT;
		error = tsleep(&vp->v_numoutput,
		    slpflag | (PRIBIO + 1), wmesg, timeo);
		if (error)
			break;
	}

	return (error);
}

/*
 * Update outstanding I/O count and do wakeup if requested.
 *
 * Manipulates v_numoutput. Must be called at splbio()
 */
void
vwakeup(struct vnode *vp)
{
	splassert(IPL_BIO);

	if (vp != NULL) {
		if (vp->v_numoutput-- == 0)
			panic("vwakeup: neg numoutput");
		if ((vp->v_bioflag & VBIOWAIT) && vp->v_numoutput == 0) {
			vp->v_bioflag &= ~VBIOWAIT;
			wakeup(&vp->v_numoutput);
		}
	}
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying object locked.
 */
int
vinvalbuf(struct vnode *vp, int flags, struct ucred *cred, struct proc *p,
    int slpflag, int slptimeo)
{
	struct buf *bp;
	struct buf *nbp, *blist;
	int s, error;

#ifdef VFSLCKDEBUG
	if ((vp->v_flag & VLOCKSWORK) && !VOP_ISLOCKED(vp))
		panic("vinvalbuf(): vp isn't locked");
#endif

	if (flags & V_SAVE) {
		s = splbio();
		vwaitforio(vp, 0, "vinvalbuf", 0);
		if (!LIST_EMPTY(&vp->v_dirtyblkhd)) {
			splx(s);
			if ((error = VOP_FSYNC(vp, cred, MNT_WAIT, p)) != 0)
				return (error);
			s = splbio();
			if (vp->v_numoutput > 0 ||
			    !LIST_EMPTY(&vp->v_dirtyblkhd))
				panic("vinvalbuf: dirty bufs");
		}
		splx(s);
	}
loop:
	s = splbio();
	for (;;) {
		if ((blist = LIST_FIRST(&vp->v_cleanblkhd)) &&
		    (flags & V_SAVEMETA))
			while (blist && blist->b_lblkno < 0)
				blist = LIST_NEXT(blist, b_vnbufs);
		if (blist == NULL &&
		    (blist = LIST_FIRST(&vp->v_dirtyblkhd)) &&
		    (flags & V_SAVEMETA))
			while (blist && blist->b_lblkno < 0)
				blist = LIST_NEXT(blist, b_vnbufs);
		if (!blist)
			break;

		for (bp = blist; bp; bp = nbp) {
			nbp = LIST_NEXT(bp, b_vnbufs);
			if (flags & V_SAVEMETA && bp->b_lblkno < 0)
				continue;
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				error = tsleep(bp, slpflag | (PRIBIO + 1),
				    "vinvalbuf", slptimeo);
				if (error) {
					splx(s);
					return (error);
				}
				break;
			}
			bremfree(bp);
			/*
			 * XXX Since there are no node locks for NFS, I believe
			 * there is a slight chance that a delayed write will
			 * occur while sleeping just above, so check for it.
			 */
			if ((bp->b_flags & B_DELWRI) && (flags & V_SAVE)) {
				buf_acquire(bp);
				splx(s);
				(void) VOP_BWRITE(bp);
				goto loop;
			}
			buf_acquire_nomap(bp);
			bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}
	if (!(flags & V_SAVEMETA) &&
	    (!LIST_EMPTY(&vp->v_dirtyblkhd) || !LIST_EMPTY(&vp->v_cleanblkhd)))
		panic("vinvalbuf: flush failed");
	splx(s);
	return (0);
}

void
vflushbuf(struct vnode *vp, int sync)
{
	struct buf *bp, *nbp;
	int s;

loop:
	s = splbio();
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp != NULL; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("vflushbuf: not dirty");
		bremfree(bp);
		buf_acquire(bp);
		splx(s);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 */
		if (bp->b_vp == vp || sync == 0)
			(void) bawrite(bp);
		else
			(void) bwrite(bp);
		goto loop;
	}
	if (sync == 0) {
		splx(s);
		return;
	}
	vwaitforio(vp, 0, "vflushbuf", 0);
	if (!LIST_EMPTY(&vp->v_dirtyblkhd)) {
		splx(s);
#ifdef DIAGNOSTIC
		vprint("vflushbuf: dirty", vp);
#endif
		goto loop;
	}
	splx(s);
}

/*
 * Associate a buffer with a vnode.
 *
 * Manipulates buffer vnode queues. Must be called at splbio().
 */
void
bgetvp(struct vnode *vp, struct buf *bp)
{
	splassert(IPL_BIO);


	if (bp->b_vp)
		panic("bgetvp: not free");
	vhold(vp);
	bp->b_vp = vp;
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		bp->b_dev = vp->v_rdev;
	else
		bp->b_dev = NODEV;
	/*
	 * Insert onto list for new vnode.
	 */
	bufinsvn(bp, &vp->v_cleanblkhd);
}

/*
 * Disassociate a buffer from a vnode.
 *
 * Manipulates vnode buffer queues. Must be called at splbio().
 */
void
brelvp(struct buf *bp)
{
	struct vnode *vp;

	splassert(IPL_BIO);

	if ((vp = bp->b_vp) == (struct vnode *) 0)
		panic("brelvp: NULL");
	/*
	 * Delete from old vnode list, if on one.
	 */
	if (LIST_NEXT(bp, b_vnbufs) != NOLIST)
		bufremvn(bp);
	if ((vp->v_bioflag & VBIOONSYNCLIST) &&
	    LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
		vp->v_bioflag &= ~VBIOONSYNCLIST;
		LIST_REMOVE(vp, v_synclist);
	}
	bp->b_vp = NULL;

	vdrop(vp);
}

/*
 * Replaces the current vnode associated with the buffer, if any,
 * with a new vnode.
 *
 * If an output I/O is pending on the buffer, the old vnode
 * I/O count is adjusted.
 *
 * Ignores vnode buffer queues. Must be called at splbio().
 */
void
buf_replacevnode(struct buf *bp, struct vnode *newvp)
{
	struct vnode *oldvp = bp->b_vp;

	splassert(IPL_BIO);

	if (oldvp)
		brelvp(bp);

	if ((bp->b_flags & (B_READ | B_DONE)) == 0) {
		newvp->v_numoutput++;	/* put it on swapdev */
		vwakeup(oldvp);
	}

	bgetvp(newvp, bp);
	bufremvn(bp);
}

/*
 * Used to assign buffers to the appropriate clean or dirty list on
 * the vnode and to add newly dirty vnodes to the appropriate
 * filesystem syncer list.
 *
 * Manipulates vnode buffer queues. Must be called at splbio().
 */
void
reassignbuf(struct buf *bp)
{
	struct buflists *listheadp;
	int delay;
	struct vnode *vp = bp->b_vp;

	splassert(IPL_BIO);

	/*
	 * Delete from old vnode list, if on one.
	 */
	if (LIST_NEXT(bp, b_vnbufs) != NOLIST)
		bufremvn(bp);

	/*
	 * If dirty, put on list of dirty buffers;
	 * otherwise insert onto list of clean buffers.
	 */
	if ((bp->b_flags & B_DELWRI) == 0) {
		listheadp = &vp->v_cleanblkhd;
		if ((vp->v_bioflag & VBIOONSYNCLIST) &&
		    LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
			vp->v_bioflag &= ~VBIOONSYNCLIST;
			LIST_REMOVE(vp, v_synclist);
		}
	} else {
		listheadp = &vp->v_dirtyblkhd;
		if ((vp->v_bioflag & VBIOONSYNCLIST) == 0) {
			switch (vp->v_type) {
			case VDIR:
				delay = syncdelay / 2;
				break;
			case VBLK:
				if (vp->v_specmountpoint != NULL) {
					delay = syncdelay / 3;
					break;
				}
				/* FALLTHROUGH */
			default:
				delay = syncdelay;
			}
			vn_syncer_add_to_worklist(vp, delay);
		}
	}
	bufinsvn(bp, listheadp);
}

int
vfs_register(struct vfsconf *vfs)
{
	struct vfsconf *vfsp;
	struct vfsconf **vfspp;

#ifdef DIAGNOSTIC
	/* Paranoia? */
	if (vfs->vfc_refcount != 0)
		printf("vfs_register called with vfc_refcount > 0\n");
#endif

	/* Check if filesystem already known */
	for (vfspp = &vfsconf, vfsp = vfsconf; vfsp;
	    vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next)
		if (strcmp(vfsp->vfc_name, vfs->vfc_name) == 0)
			return (EEXIST);

	if (vfs->vfc_typenum > maxvfsconf)
		maxvfsconf = vfs->vfc_typenum;

	vfs->vfc_next = NULL;

	/* Add to the end of the list */
	*vfspp = vfs;

	/* Call vfs_init() */
	if (vfs->vfc_vfsops->vfs_init)
		(*(vfs->vfc_vfsops->vfs_init))(vfs);

	return 0;
}

int
vfs_unregister(struct vfsconf *vfs)
{
	struct vfsconf *vfsp;
	struct vfsconf **vfspp;
	int maxtypenum;

	/* Find our vfsconf struct */
	for (vfspp = &vfsconf, vfsp = vfsconf; vfsp;
	    vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next) {
		if (strcmp(vfsp->vfc_name, vfs->vfc_name) == 0)
			break;
	}

	if (!vfsp)			/* Not found */
		return (ENOENT);

	if (vfsp->vfc_refcount)		/* In use */
		return (EBUSY);

	/* Remove from list and free */
	*vfspp = vfsp->vfc_next;

	maxtypenum = 0;

	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (vfsp->vfc_typenum > maxtypenum)
			maxtypenum = vfsp->vfc_typenum;

	maxvfsconf = maxtypenum;
	return 0;
}

/*
 * Check if vnode represents a disk device
 */
int
vn_isdisk(struct vnode *vp, int *errp)
{
	if (vp->v_type != VBLK && vp->v_type != VCHR)
		return (0);

	return (1);
}

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
vfs_buf_print(void *b, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct buf *bp = b;

	(*pr)("  vp %p lblkno 0x%llx blkno 0x%llx dev 0x%x\n"
	      "  proc %p error %d flags %lb\n",
	    bp->b_vp, (int64_t)bp->b_lblkno, (int64_t)bp->b_blkno, bp->b_dev,
	    bp->b_proc, bp->b_error, bp->b_flags, B_BITS);

	(*pr)("  bufsize 0x%lx bcount 0x%lx resid 0x%lx\n"
	      "  data %p saveaddr %p dep %p iodone %p\n",
	    bp->b_bufsize, bp->b_bcount, (long)bp->b_resid,
	    bp->b_data, bp->b_saveaddr,
	    LIST_FIRST(&bp->b_dep), bp->b_iodone);

	(*pr)("  dirty {off 0x%x end 0x%x} valid {off 0x%x end 0x%x}\n",
	    bp->b_dirtyoff, bp->b_dirtyend, bp->b_validoff, bp->b_validend);

#ifdef FFS_SOFTUPDATES
	if (full)
		softdep_print(bp, full, pr);
#endif
}

const char *vtypes[] = { VTYPE_NAMES };
const char *vtags[] = { VTAG_NAMES };

void
vfs_vnode_print(void *v, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct vnode *vp = v;

	(*pr)("tag %s(%d) type %s(%d) mount %p typedata %p\n",
	      vp->v_tag > nitems(vtags)? "<unk>":vtags[vp->v_tag], vp->v_tag,
	      vp->v_type > nitems(vtypes)? "<unk>":vtypes[vp->v_type],
	      vp->v_type, vp->v_mount, vp->v_mountedhere);

	(*pr)("data %p usecount %d writecount %d holdcnt %d numoutput %d\n",
	      vp->v_data, vp->v_usecount, vp->v_writecount,
	      vp->v_holdcnt, vp->v_numoutput);

	/* uvm_object_printit(&vp->v_uobj, full, pr); */

	if (full) {
		struct buf *bp;

		(*pr)("clean bufs:\n");
		LIST_FOREACH(bp, &vp->v_cleanblkhd, b_vnbufs) {
			(*pr)(" bp %p\n", bp);
			vfs_buf_print(bp, full, pr);
		}

		(*pr)("dirty bufs:\n");
		LIST_FOREACH(bp, &vp->v_dirtyblkhd, b_vnbufs) {
			(*pr)(" bp %p\n", bp);
			vfs_buf_print(bp, full, pr);
		}
	}
}

void
vfs_mount_print(struct mount *mp, int full,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct vfsconf *vfc = mp->mnt_vfc;
	struct vnode *vp;
	int cnt = 0;

	(*pr)("flags %b\nvnodecovered %p syncer %p data %p\n",
	    mp->mnt_flag, MNT_BITS,
	    mp->mnt_vnodecovered, mp->mnt_syncer, mp->mnt_data);

	(*pr)("vfsconf: ops %p name \"%s\" num %d ref %d flags 0x%x\n",
            vfc->vfc_vfsops, vfc->vfc_name, vfc->vfc_typenum,
	    vfc->vfc_refcount, vfc->vfc_flags);

	(*pr)("statvfs cache: bsize %x iosize %x\nblocks %llu free %llu avail %lld\n",
	    mp->mnt_stat.f_bsize, mp->mnt_stat.f_iosize, mp->mnt_stat.f_blocks,
	    mp->mnt_stat.f_bfree, mp->mnt_stat.f_bavail);

	(*pr)("  files %llu ffiles %llu favail %lld\n", mp->mnt_stat.f_files,
	    mp->mnt_stat.f_ffree, mp->mnt_stat.f_favail);

	(*pr)("  f_fsidx {0x%x, 0x%x} owner %u ctime 0x%llx\n",
	    mp->mnt_stat.f_fsid.val[0], mp->mnt_stat.f_fsid.val[1],
	    mp->mnt_stat.f_owner, mp->mnt_stat.f_ctime);

 	(*pr)("  syncwrites %llu asyncwrites = %llu\n",
	    mp->mnt_stat.f_syncwrites, mp->mnt_stat.f_asyncwrites);

 	(*pr)("  syncreads %llu asyncreads = %llu\n",
	    mp->mnt_stat.f_syncreads, mp->mnt_stat.f_asyncreads);

	(*pr)("  fstype \"%s\" mnton \"%s\" mntfrom \"%s\" mntspec \"%s\"\n",
	    mp->mnt_stat.f_fstypename, mp->mnt_stat.f_mntonname,
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntfromspec);

	(*pr)("locked vnodes:");
	/* XXX would take mountlist lock, except ddb has no context */
	LIST_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes)
		if (VOP_ISLOCKED(vp)) {
			if (!LIST_NEXT(vp, v_mntvnodes))
				(*pr)(" %p", vp);
			else if (!(cnt++ % (72 / (sizeof(void *) * 2 + 4))))
				(*pr)("\n\t%p", vp);
			else
				(*pr)(", %p", vp);
		}
	(*pr)("\n");

	if (full) {
		(*pr)("all vnodes:\n\t");
		/* XXX would take mountlist lock, except ddb has no context */
		LIST_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes)
			if (!LIST_NEXT(vp, v_mntvnodes))
				(*pr)(" %p", vp);
			else if (!(cnt++ % (72 / (sizeof(void *) * 2 + 4))))
				(*pr)(" %p,\n\t", vp);
			else
				(*pr)(" %p,", vp);
		(*pr)("\n");
	}
}
#endif /* DDB */

void
copy_statfs_info(struct statfs *sbp, const struct mount *mp)
{
	const struct statfs *mbp;

	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);

	if (sbp == (mbp = &mp->mnt_stat))
		return;

	sbp->f_fsid = mbp->f_fsid;
	sbp->f_owner = mbp->f_owner;
	sbp->f_flags = mbp->f_flags;
	sbp->f_syncwrites = mbp->f_syncwrites;
	sbp->f_asyncwrites = mbp->f_asyncwrites;
	sbp->f_syncreads = mbp->f_syncreads;
	sbp->f_asyncreads = mbp->f_asyncreads;
	sbp->f_namemax = mbp->f_namemax;
	bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
	bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	bcopy(mp->mnt_stat.f_mntfromspec, sbp->f_mntfromspec, MNAMELEN);
	bcopy(&mp->mnt_stat.mount_info.ufs_args, &sbp->mount_info.ufs_args,
	    sizeof(struct ufs_args));
}
