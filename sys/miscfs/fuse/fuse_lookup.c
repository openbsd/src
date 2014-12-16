/* $OpenBSD: fuse_lookup.c,v 1.10 2014/12/16 18:30:04 tedu Exp $ */
/*
 * Copyright (c) 2012-2013 Sylvestre Gallon <ccna.syl@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/fusebuf.h>

#include "fusefs_node.h"
#include "fusefs.h"

int fusefs_lookup(void *);

int
fusefs_lookup(void *v)
{
	struct vop_lookup_args *ap = v;
	struct vnode *vdp;	/* vnode for directory being searched */
	struct fusefs_node *dp;	/* inode for directory being searched */
	struct fusefs_mnt *fmp;	/* file system that directory is in */
	int lockparent;		/* 1 => lockparent flag is set */
	struct vnode *tdp;	/* returned by VOP_VGET */
	struct fusebuf *fbuf = NULL;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct ucred *cred = cnp->cn_cred;
	int flags;
	int nameiop = cnp->cn_nameiop;
	int wantparent;
	int error = 0;
	uint64_t nid;

	flags = cnp->cn_flags;
	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	fmp = (struct fusefs_mnt *)dp->ufs_ino.i_ump;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT | WANTPARENT);

	if ((error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc)) != 0)
		return (error);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	if ((error = cache_lookup(vdp, vpp, cnp)) >= 0)
		return (error);

	if (flags & ISDOTDOT) {
		/* got ".." */
		nid = dp->parent;
		if (nid == 0)
			return (ENOENT);
	} else if (cnp->cn_namelen == 1 && *(cnp->cn_nameptr) == '.') {
		nid = dp->ufs_ino.i_number;
	} else {
		if (!fmp->sess_init)
			return (ENOENT);

		/* got a real entry */
		fbuf = fb_setup(cnp->cn_namelen + 1, dp->ufs_ino.i_number,
		    FBT_LOOKUP, p);

		memcpy(fbuf->fb_dat, cnp->cn_nameptr, cnp->cn_namelen);
		fbuf->fb_dat[cnp->cn_namelen] = '\0';

		error = fb_queue(fmp->dev, fbuf);

		/* tsleep return */
		if (error == EWOULDBLOCK)
			goto out;

		if (error) {
			if ((nameiop == CREATE || nameiop == RENAME) &&
			    (flags & ISLASTCN)) {
				if (vdp->v_mount->mnt_flag & MNT_RDONLY)
					return (EROFS);

				cnp->cn_flags |= SAVENAME;

				if (!lockparent) {
					VOP_UNLOCK(vdp, 0, p);
					cnp->cn_flags |= PDIRUNLOCK;
				}

				error = EJUSTRETURN;
				goto out;
			}

			error = ENOENT;
			goto out;
		}

		nid = fbuf->fb_vattr.va_fileid;
	}

	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc);
		if (error != 0) {
			fb_delete(fbuf);
			return (error);
		}

		cnp->cn_flags |= SAVENAME;
	}

	if (nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if ((error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc)) != 0) {
			fb_delete(fbuf);
			return (error);
		}

		if (nid == VTOI(vdp)->ufs_ino.i_number) {
			error = EISDIR;
			goto out;
		}

		error = VFS_VGET(fmp->mp, nid, &tdp);
		if (error)
			goto out;

		tdp->v_type = IFTOVT(fbuf->fb_vattr.va_mode);
		VTOI(tdp)->vtype = tdp->v_type;
		*vpp = tdp;
		cnp->cn_flags |= SAVENAME;

		goto out;
	}

	if (flags & ISDOTDOT) {
		VOP_UNLOCK(vdp, 0, p);	/* race to get the inode */
		cnp->cn_flags |= PDIRUNLOCK;

		error = VFS_VGET(fmp->mp, nid, &tdp);

		if (error) {
			if (vn_lock(vdp, LK_EXCLUSIVE | LK_RETRY, p) == 0)
				cnp->cn_flags &= ~PDIRUNLOCK;

			return (error);
		}

		if (lockparent && (flags & ISLASTCN)) {
			if ((error = vn_lock(vdp, LK_EXCLUSIVE, p))) {
				vput(tdp);
				return (error);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
		*vpp = tdp;

	} else if (nid == dp->ufs_ino.i_number) {
		vref(vdp);
		*vpp = vdp;
		error = 0;
	} else {
		error = VFS_VGET(fmp->mp, nid, &tdp);

		if (!error) {
			tdp->v_type = IFTOVT(fbuf->fb_vattr.va_mode);
			VTOI(tdp)->vtype = tdp->v_type;
		}

		update_vattr(fmp->mp, &fbuf->fb_vattr);

		if (error) {
			fb_delete(fbuf);
			return (error);
		}

		if (vdp != NULL && vdp->v_type == VDIR)
			VTOI(tdp)->parent = dp->ufs_ino.i_number;

		if (!lockparent || !(flags & ISLASTCN)) {
			VOP_UNLOCK(vdp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}

		*vpp = tdp;
	}

out:
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE &&
	    nameiop != DELETE)
		cache_enter(vdp, *vpp, cnp);

	fb_delete(fbuf);
	return (error);
}
