/*	$OpenBSD: nfs_vnops.c,v 1.75 2007/06/01 23:47:57 deraadt Exp $	*/
/*	$NetBSD: nfs_vnops.c,v 1.62.4.1 1996/07/08 20:26:52 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vnops.c	8.16 (Berkeley) 5/27/95
 */


/*
 * vnode op calls for Sun NFS version 2 and 3
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/mbuf.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/hash.h>

#include <uvm/uvm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_var.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

/* Defs */
#define	TRUE	1
#define	FALSE	0

/*
 * Global vfs data structures for nfs
 */
int (**nfsv2_vnodeop_p)(void *);
struct vnodeopv_entry_desc nfsv2_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, nfs_lookup },	/* lookup */
	{ &vop_create_desc, nfs_create },	/* create */
	{ &vop_mknod_desc, nfs_mknod },		/* mknod */
	{ &vop_open_desc, nfs_open },		/* open */
	{ &vop_close_desc, nfs_close },		/* close */
	{ &vop_access_desc, nfs_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfs_read },		/* read */
	{ &vop_write_desc, nfs_write },		/* write */
	{ &vop_ioctl_desc, nfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, nfs_poll },		/* poll */
	{ &vop_kqfilter_desc, nfs_kqfilter },	/* kqfilter */
	{ &vop_revoke_desc, nfs_revoke },	/* revoke */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_remove_desc, nfs_remove },	/* remove */
	{ &vop_link_desc, nfs_link },		/* link */
	{ &vop_rename_desc, nfs_rename },	/* rename */
	{ &vop_mkdir_desc, nfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, nfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, nfs_symlink },	/* symlink */
	{ &vop_readdir_desc, nfs_readdir },	/* readdir */
	{ &vop_readlink_desc, nfs_readlink },	/* readlink */
	{ &vop_abortop_desc, vop_generic_abortop },	/* abortop */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_bmap_desc, nfs_bmap },		/* bmap */
	{ &vop_strategy_desc, nfs_strategy },	/* strategy */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, nfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, nfs_advlock },	/* advlock */
	{ &vop_bwrite_desc, nfs_bwrite },
	{ NULL, NULL }
};
struct vnodeopv_desc nfsv2_vnodeop_opv_desc =
	{ &nfsv2_vnodeop_p, nfsv2_vnodeop_entries };

/*
 * Special device vnode ops
 */
int (**spec_nfsv2nodeop_p)(void *);
struct vnodeopv_entry_desc spec_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, spec_vnoperate },
	{ &vop_close_desc, nfsspec_close },	/* close */
	{ &vop_access_desc, nfsspec_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfsspec_read },	/* read */
	{ &vop_write_desc, nfsspec_write },	/* write */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ NULL, NULL }
};
struct vnodeopv_desc spec_nfsv2nodeop_opv_desc =
	{ &spec_nfsv2nodeop_p, spec_nfsv2nodeop_entries };

#ifdef FIFO
int (**fifo_nfsv2nodeop_p)(void *);
struct vnodeopv_entry_desc fifo_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, fifo_vnoperate },
	{ &vop_close_desc, nfsfifo_close },	/* close */
	{ &vop_access_desc, nfsspec_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfsfifo_read },	/* read */
	{ &vop_write_desc, nfsfifo_write },	/* write */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfsfifo_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ &vop_bwrite_desc, vop_generic_bwrite },
	{ NULL, NULL }
};
struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc =
	{ &fifo_nfsv2nodeop_p, fifo_nfsv2nodeop_entries };
#endif /* FIFO */

/*
 * Global variables
 */
extern u_int32_t nfs_true, nfs_false;
extern u_int32_t nfs_xdrneg1;
extern struct nfsstats nfsstats;
extern nfstype nfsv3_type[9];
struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
int nfs_numasync = 0;

/*
 * nfs null call from vfs.
 */
int
nfs_null(vp, cred, procp)
	struct vnode *vp;
	struct ucred *cred;
	struct proc *procp;
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	
	nfsm_reqhead(vp, NFSPROC_NULL, 0);
	nfsm_request(vp, NFSPROC_NULL, procp, cred);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
int
nfs_access(v)
	void *v;
{
	struct vop_access_args *ap = v;
	struct vnode *vp = ap->a_vp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_int32_t mode, rmode;
	int v3 = NFS_ISV3(vp);

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}
	/*
	 * For nfs v3, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about, but
	 * this is better than just returning anything that is lying about
	 * in the cache.
	 */
	if (v3) {
		nfsstats.rpccnt[NFSPROC_ACCESS]++;
		nfsm_reqhead(vp, NFSPROC_ACCESS, NFSX_FH(v3) + NFSX_UNSIGNED);
		nfsm_fhtom(vp, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (ap->a_mode & VREAD)
			mode = NFSV3ACCESS_READ;
		else
			mode = 0;
		if (vp->v_type == VDIR) {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
					 NFSV3ACCESS_DELETE);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_LOOKUP;
		} else {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_EXECUTE;
		}
		*tl = txdr_unsigned(mode);
		nfsm_request(vp, NFSPROC_ACCESS, ap->a_p, ap->a_cred);
		nfsm_postop_attr(vp, attrflag);
		if (!error) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			rmode = fxdr_unsigned(u_int32_t, *tl);
			/*
			 * The NFS V3 spec does not clarify whether or not
			 * the returned access bits can be a superset of
			 * the ones requested, so...
			 */
			if ((rmode & mode) != mode)
				error = EACCES;
		}
		nfsm_reqdone;
		return (error);
	} else
		return (nfsspec_access(ap));
}

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
int
nfs_open(v)
	void *v;
{
	struct vop_open_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	int error;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK) {
#ifdef DIAGNOSTIC
		printf("open eacces vtyp=%d\n",vp->v_type);
#endif
		return (EACCES);
	}

	/*
	 * Initialize read and write creds here, for swapfiles
	 * and other paths that don't set the creds themselves.
	 */

	if (ap->a_mode & FREAD) {
		if (np->n_rcred) {
			crfree(np->n_rcred);
		}
		np->n_rcred = ap->a_cred;
		crhold(np->n_rcred);
	}
	if (ap->a_mode & FWRITE) {
		if (np->n_wcred) {
			crfree(np->n_wcred);
		}
		np->n_wcred = ap->a_cred;
		crhold(np->n_wcred);
	}

	if (np->n_flag & NMODIFIED) {
		if ((error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
			 ap->a_p, 1)) == EINTR)
			return (error);
		uvm_vnp_uncache(vp);
		np->n_attrstamp = 0;
		if (vp->v_type == VDIR)
			np->n_direofoffset = 0;
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return (error);
		np->n_mtime = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return (error);
		if (np->n_mtime != vattr.va_mtime.tv_sec) {
			if (vp->v_type == VDIR)
				np->n_direofoffset = 0;
			if ((error = nfs_vinvalbuf(vp, V_SAVE,
				 ap->a_cred, ap->a_p, 1)) == EINTR)
				return (error);
			uvm_vnp_uncache(vp);
			np->n_mtime = vattr.va_mtime.tv_sec;
		}
	}
	np->n_attrstamp = 0; /* For Open/Close consistency */
	return (0);
}

/*
 * nfs close vnode op
 * What an NFS client should do upon close after writing is a debatable issue.
 * Most NFS clients push delayed writes to the server upon close, basically for
 * two reasons:
 * 1 - So that any write errors may be reported back to the client process
 *     doing the close system call. By far the two most likely errors are
 *     NFSERR_NOSPC and NFSERR_DQUOT to indicate space allocation failure.
 * 2 - To put a worst case upper bound on cache inconsistency between
 *     multiple clients for the file.
 * There is also a consistency problem for Version 2 of the protocol w.r.t.
 * not being able to tell if other clients are writing a file concurrently,
 * since there is no way of knowing if the changed modify time in the reply
 * is only due to the write for this client.
 * (NFS Version 3 provides weak cache consistency data in the reply that
 *  should be sufficient to detect and handle this case.)
 *
 * The current code does the following:
 * for NFS Version 2 - play it safe and flush/invalidate all dirty buffers
 * for NFS Version 3 - flush dirty buffers to the server but don't invalidate
 *                     or commit them (this satisfies 1 and 2 except for the
 *                     case where the server crashes after this close but
 *                     before the commit RPC, which is felt to be "good
 *                     enough". Changing the last argument to nfs_flush() to
 *                     a 1 would force a commit operation, if it is felt a
 *                     commit is necessary now.
 */
/* ARGSUSED */
int
nfs_close(v)
	void *v;
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (vp->v_type == VREG) {
	    if (np->n_flag & NMODIFIED) {
		if (NFS_ISV3(vp)) {
		    error = nfs_flush(vp, ap->a_cred, MNT_WAIT, ap->a_p, 0);
		    np->n_flag &= ~NMODIFIED;
		} else
		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
		np->n_attrstamp = 0;
	    }
	    if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	    }
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
int
nfs_getattr(v)
	void *v;
{
	struct vop_getattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1, t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);
	
	/*
	 * Update local times for special files.
	 */
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;
	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, ap->a_vap) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	nfsm_reqhead(vp, NFSPROC_GETATTR, NFSX_FH(v3));
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_GETATTR, ap->a_p, ap->a_cred);
	if (!error)
		nfsm_loadattr(vp, ap->a_vap);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs setattr call.
 */
int
nfs_setattr(v)
	void *v;
{
	struct vop_setattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	int error = 0;
	u_quad_t tsize = 0;

	/*
	 * Setting of flags is not supported.
	 */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_mtime.tv_sec == VNOVAL &&
			    vap->va_atime.tv_sec == VNOVAL &&
			    vap->va_mode == (mode_t)VNOVAL &&
			    vap->va_uid == (uid_t)VNOVAL &&
			    vap->va_gid == (gid_t)VNOVAL)
				return (0);
 			vap->va_size = VNOVAL;
 			break;
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
 			if (vap->va_size == 0)
 				error = nfs_vinvalbuf(vp, 0,
 				     ap->a_cred, ap->a_p, 1);
			else
				error = nfs_vinvalbuf(vp, V_SAVE,
 				     ap->a_cred, ap->a_p, 1);
			if (error)
				return (error);
 			tsize = np->n_size;
 			np->n_size = np->n_vattr.va_size = vap->va_size;
			uvm_vnp_setsize(vp, np->n_size);
  		};
  	} else if ((vap->va_mtime.tv_sec != VNOVAL ||
		vap->va_atime.tv_sec != VNOVAL) &&
		vp->v_type == VREG &&
  		(error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
		 ap->a_p, 1)) == EINTR)
		return (error);
	error = nfs_setattrrpc(vp, vap, ap->a_cred, ap->a_p);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = np->n_vattr.va_size = tsize;
		uvm_vnp_setsize(vp, np->n_size);
	}

	VN_KNOTE(vp, NOTE_ATTRIB); /* XXX setattrrpc? */

	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
int
nfs_setattrrpc(vp, vap, cred, procp)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *procp;
{
	struct nfsv2_sattr *sp;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	u_int32_t *tl;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	nfsm_reqhead(vp, NFSPROC_SETATTR, NFSX_FH(v3) + NFSX_SATTR(v3));
	nfsm_fhtom(vp, v3);
	if (v3) {
		nfsm_v3attrbuild(vap, TRUE);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		if (vap->va_mode == (mode_t)VNOVAL)
			sp->sa_mode = nfs_xdrneg1;
		else
			sp->sa_mode = vtonfsv2_mode(vp->v_type, vap->va_mode);
		if (vap->va_uid == (uid_t)VNOVAL)
			sp->sa_uid = nfs_xdrneg1;
		else
			sp->sa_uid = txdr_unsigned(vap->va_uid);
		if (vap->va_gid == (gid_t)VNOVAL)
			sp->sa_gid = nfs_xdrneg1;
		else
			sp->sa_gid = txdr_unsigned(vap->va_gid);
		sp->sa_size = txdr_unsigned(vap->va_size);
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(vp, NFSPROC_SETATTR, procp, cred);
	if (v3) {
		nfsm_wcc_data(vp, wccflag);
	} else
		nfsm_loadattr(vp, (struct vattr *)0);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
int
nfs_lookup(v)
	void *v;
{
	struct vop_lookup_args *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct proc *p = cnp->cn_proc;
	int flags;
	struct vnode *newvp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct nfsmount *nmp;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np;
	int lockparent, wantparent, error = 0, attrflag, fhsize;
	int v3 = NFS_ISV3(dvp);

	cnp->cn_flags &= ~PDIRUNLOCK;
	flags = cnp->cn_flags;

	*vpp = NULLVP;
	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);

	/*
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 * If the directory/name pair is found in the name cache,
	 * we have to ensure the directory has not changed from
	 * the time the cache entry has been created. If it has,
	 * the cache entry has to be ignored.
	 */
	if ((error = cache_lookup(dvp, vpp, cnp)) >= 0) {
		struct vattr vattr;
		int err2;

		if (error && error != ENOENT) {
			*vpp = NULLVP;
			return (error);
		}

		if (cnp->cn_flags & PDIRUNLOCK) {
			err2 = vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p);
			if (err2 != 0) {
				*vpp = NULLVP;
				return (err2);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}

		err2 = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, cnp->cn_proc);
		if (err2 != 0) {
			if (error == 0) {
				if (*vpp != dvp)
					vput(*vpp);
				else
					vrele(*vpp);
			}
			*vpp = NULLVP;
			return (err2);
		}

		if (error == ENOENT) {
			if (!VOP_GETATTR(dvp, &vattr, cnp->cn_cred,
			    cnp->cn_proc) && vattr.va_mtime.tv_sec ==
			    VTONFS(dvp)->n_ctime)
				return (ENOENT);
			cache_purge(dvp);
			np->n_ctime = 0;
			goto dorpc;
		}

		newvp = *vpp;
		if (!VOP_GETATTR(newvp, &vattr, cnp->cn_cred, cnp->cn_proc)
			&& vattr.va_ctime.tv_sec == VTONFS(newvp)->n_ctime)
		{
			nfsstats.lookupcache_hits++;
			if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
				cnp->cn_flags |= SAVENAME;
			if ((!lockparent || !(flags & ISLASTCN)) &&
			     newvp != dvp)
				VOP_UNLOCK(dvp, 0, p);
			return (0);
		}
		cache_purge(newvp);
		if (newvp != dvp)
			vput(newvp);
		else
			vrele(newvp);
		*vpp = NULLVP;
	}
dorpc:
	error = 0;
	newvp = NULLVP;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = cnp->cn_namelen;
	nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, cnp->cn_proc, cnp->cn_cred);
	if (error) {
		nfsm_postop_attr(dvp, attrflag);
		m_freem(mrep);
		goto nfsmout;
	}
	nfsm_getfh(fhp, fhsize, v3);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize)) {
			m_freem(mrep);
			return (EISDIR);
		}
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
		*vpp = newvp;
		m_freem(mrep);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (0);
	}

	/*
	 * The postop attr handling is duplicated for each if case,
	 * because it should be done while dvp is locked (unlocking
	 * dvp is different for each case).
	 */

	if (NFS_CMPFH(np, fhp, fhsize)) {
		VREF(dvp);
		newvp = dvp;
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, p);
		cnp->cn_flags |= PDIRUNLOCK;

		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			if (vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p) == 0)
				cnp->cn_flags &= ~PDIRUNLOCK;
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);

		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);

		if (lockparent && (flags & ISLASTCN)) {
			if ((error = vn_lock(dvp, LK_EXCLUSIVE, p))) {
				m_freem(mrep);
				vput(newvp);
				return error;
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}

	} else {
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return error;
		}
		newvp = NFSTOV(np);
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
		if (!lockparent || !(flags & ISLASTCN)) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
	}
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))) {
		np->n_ctime = np->n_vattr.va_ctime.tv_sec;
		cache_enter(dvp, newvp, cnp);
	}
	*vpp = newvp;
	nfsm_reqdone;
	if (error) {
		/*
		 * We get here only because of errors returned by
		 * the RPC. Otherwise we'll have returned above
		 * (the nfsm_* macros will jump to nfsm_reqdone
		 * on error).
		 */
		if (error == ENOENT && (cnp->cn_flags & MAKEENTRY) &&
		    cnp->cn_nameiop != CREATE) {
			if (VTONFS(dvp)->n_ctime == 0)
				VTONFS(dvp)->n_ctime =
				    VTONFS(dvp)->n_vattr.va_mtime.tv_sec;
			cache_enter(dvp, NULL, cnp);
		}
		if (newvp != NULLVP) {
			vrele(newvp);
			if (newvp != dvp)
				VOP_UNLOCK(newvp, 0, p);
		}
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN) && error == ENOENT) {
			if (dvp->v_mount->mnt_flag & MNT_RDONLY)
				error = EROFS;
			else
				error = EJUSTRETURN;
		}
		if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
			cnp->cn_flags |= SAVENAME;
		*vpp = NULL;
	}
	return (error);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
int
nfs_read(v)
	void *v;
{
	struct vop_read_args *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred));
}

/*
 * nfs readlink call
 */
int
nfs_readlink(v)
	void *v;
{
	struct vop_readlink_args *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VLNK)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs_readlinkrpc(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, len, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	nfsm_reqhead(vp, NFSPROC_READLINK, NFSX_FH(v3));
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_READLINK, uiop->uio_procp, cred);
	if (v3)
		nfsm_postop_attr(vp, attrflag);
	if (!error) {
		nfsm_strsiz(len, NFS_MAXPATHLEN);
		nfsm_mtouio(uiop, len);
	}
	nfsm_reqdone;
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(vp, uiop)
	struct vnode *vp;
	struct uio *uiop;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	int error = 0, len, retlen, tsiz, eof, attrflag;
	int v3 = NFS_ISV3(vp);

#ifndef nolint
	eof = 0;
#endif
	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff && !v3)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_READ, NFSX_FH(v3) + NFSX_UNSIGNED * 3);
		nfsm_fhtom(vp, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED * 3);
		if (v3) {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}
		nfsm_request(vp, NFSPROC_READ, uiop->uio_procp,
		    VTONFS(vp)->n_rcred);
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (error) {
				m_freem(mrep);
				goto nfsmout;
			}
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else
			nfsm_loadattr(vp, (struct vattr *)0);
		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		tsiz -= retlen;
		if (v3) {
			if (eof || retlen == 0)
				tsiz = 0;
		} else if (retlen < len)
			tsiz = 0;
	}
nfsmout:
	return (error);
}

/*
 * nfs write call
 */
int
nfs_writerpc(vp, uiop, iomode, must_commit)
	struct vnode *vp;
	struct uio *uiop;
	int *iomode, *must_commit;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2, backup;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
	int v3 = NFS_ISV3(vp), committed = NFSV3WRITE_FILESYNC;

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	*must_commit = 0;
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff && !v3)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_WRITE,
			NFSX_FH(v3) + 5 * NFSX_UNSIGNED + nfsm_rndup(len));
		nfsm_fhtom(vp, v3);
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else {
			u_int32_t x;

			nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;      /* "begin offset" */
			*tl++ = x;      /* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;      /* total to this offset */
			*tl = x;        /* size of this write */

		}
		nfsm_uiotom(uiop, len);
		nfsm_request(vp, NFSPROC_WRITE, uiop->uio_procp,
		    VTONFS(vp)->n_wcred);
		if (v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_wcc_data(vp, wccflag);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_V3WRITEVERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					break;
				} else if (rlen < len) {
					backup = len - rlen;
					(char *)uiop->uio_iov->iov_base -= backup;
					uiop->uio_iov->iov_len += backup;
					uiop->uio_offset -= backup;
					uiop->uio_resid += backup;
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest committment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSV3WRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSV3WRITE_DATASYNC &&
					commit == NFSV3WRITE_UNSTABLE)
					committed = commit;
				if ((nmp->nm_flag & NFSMNT_HASWRITEVERF) == 0) {
				    bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
					NFSX_V3WRITEVERF);
				    nmp->nm_flag |= NFSMNT_HASWRITEVERF;
				} else if (bcmp((caddr_t)tl,
				    (caddr_t)nmp->nm_verf, NFSX_V3WRITEVERF)) {
				    *must_commit = 1;
				    bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
					NFSX_V3WRITEVERF);
				}
			}
		} else
		    nfsm_loadattr(vp, (struct vattr *)0);
		if (wccflag)
		    VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime.tv_sec;
		m_freem(mrep);
		tsiz -= len;
	}
nfsmout:
	*iomode = committed;
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
int
nfs_mknodrpc(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *np;
	char *cp2;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_int32_t rdev;
	int v3 = NFS_ISV3(dvp);

	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
	else if (vap->va_type == VFIFO || vap->va_type == VSOCK)
		rdev = nfs_xdrneg1;
	else {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (EOPNOTSUPP);
	}
	nfsstats.rpccnt[NFSPROC_MKNOD]++;
	nfsm_reqhead(dvp, NFSPROC_MKNOD, NFSX_FH(v3) + 4 * NFSX_UNSIGNED +
		+ nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl++ = vtonfsv3_type(vap->va_type);
		nfsm_v3attrbuild(vap, FALSE);
		if (vap->va_type == VCHR || vap->va_type == VBLK) {
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(major(vap->va_rdev));
			*tl = txdr_unsigned(minor(vap->va_rdev));
		}
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = rdev;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKNOD, cnp->cn_proc, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = (struct vnode *)0;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	if (error) {
		if (newvp)
			vrele(newvp);
	} else {
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, newvp, cnp);
		*vpp = newvp;
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	return (error);
}

/*
 * nfs mknod vop
 * just call nfs_mknodrpc() to do the work.
 */
/* ARGSUSED */
int
nfs_mknod(v)
	void *v;
{
	struct vop_mknod_args *ap = v;
	struct vnode *newvp;
	int error;

	error = nfs_mknodrpc(ap->a_dvp, &newvp, ap->a_cnp, ap->a_vap);
	if (!error)
		vrele(newvp);

	VN_KNOTE(ap->a_dvp, NOTE_WRITE);

	return (error);
}

static u_long create_verf;
/*
 * nfs file create call
 */
int
nfs_create(v)
	void *v;
{
	struct vop_create_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct nfsnode *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0, fmode = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	/*
	 * Oops, not for me..
	 */
	if (vap->va_type == VSOCK)
		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));

#ifdef VA_EXCLUSIVE
	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= O_EXCL;
#endif
again:
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	nfsm_reqhead(dvp, NFSPROC_CREATE, NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
		nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (fmode & O_EXCL) {
			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
			nfsm_build(tl, u_int32_t *, NFSX_V3CREATEVERF);
			if (TAILQ_FIRST(&in_ifaddr))
				*tl++ = TAILQ_FIRST(&in_ifaddr)->ia_addr.sin_addr.s_addr;
			else
				*tl++ = create_verf;
			*tl = ++create_verf;
		} else {
			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
			nfsm_v3attrbuild(vap, FALSE);
		}
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = 0;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_CREATE, cnp->cn_proc, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = (struct vnode *)0;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	if (error) {
		if (v3 && (fmode & O_EXCL) && error == NFSERR_NOTSUPP) {
			fmode &= ~O_EXCL;
			goto again;
		}
		if (newvp)
			vrele(newvp);
	} else if (v3 && (fmode & O_EXCL))
		error = nfs_setattrrpc(newvp, vap, cnp->cn_cred, cnp->cn_proc);
	if (!error) {
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, newvp, cnp);
		*ap->a_vpp = newvp;
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vrele(dvp);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
int
nfs_remove(v)
	void *v;
{
	struct vop_remove_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

#ifndef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("nfs_remove: no name");
	if (vp->v_usecount < 1)
		panic("nfs_remove: bad v_usecount");
#endif
	if (vp->v_type == VDIR)
		error = EPERM;
	else if (vp->v_usecount == 1 || (np->n_sillyrename &&
	    VOP_GETATTR(vp, &vattr, cnp->cn_cred, cnp->cn_proc) == 0 &&
	    vattr.va_nlink > 1)) {
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * throw away biocache buffers, mainly to avoid
		 * unnecessary delayed writes later.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_cred, cnp->cn_proc, 1);
		/* Do the rpc */
		if (error != EINTR)
			error = nfs_removerpc(dvp, cnp->cn_nameptr,
				cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc);
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	} else if (!np->n_sillyrename)
		error = nfs_sillyrename(dvp, vp, cnp);
	pool_put(&namei_pool, cnp->cn_pnbuf);
	np->n_attrstamp = 0;
	vrele(dvp);
	vrele(vp);

	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);

	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs_removeit(sp)
	struct sillyrename *sp;
{

	return (nfs_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		(struct proc *)0));
}

/*
 * Nfs remove rpc, called from nfs_remove() and nfs_removeit().
 */
int
nfs_removerpc(dvp, name, namelen, cred, proc)
	struct vnode *dvp;
	char *name;
	int namelen;
	struct ucred *cred;
	struct proc *proc;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	nfsm_reqhead(dvp, NFSPROC_REMOVE,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(namelen));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_REMOVE, proc, cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs file rename call
 */
int
nfs_rename(v)
	void *v;
{
	struct vop_rename_args  *ap = v;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

#ifndef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("nfs_rename: no name");
#endif
	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	/*
	 * If the tvp exists and is in use, sillyrename it before doing the
	 * rename of the new file over it.
	 */
	if (tvp && tvp->v_usecount > 1 && !VTONFS(tvp)->n_sillyrename &&
	    tvp->v_type != VDIR && !nfs_sillyrename(tdvp, tvp, tcnp)) {
		VN_KNOTE(tvp, NOTE_DELETE);
		vrele(tvp);
		tvp = NULL;
	}

	error = nfs_renamerpc(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
		tdvp, tcnp->cn_nameptr, tcnp->cn_namelen, tcnp->cn_cred,
		tcnp->cn_proc);

	VN_KNOTE(fdvp, NOTE_WRITE);
	VN_KNOTE(tdvp, NOTE_WRITE);

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
int
nfs_renameit(sdvp, scnp, sp)
	struct vnode *sdvp;
	struct componentname *scnp;
	struct sillyrename *sp;
{
	return (nfs_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen,
		sdvp, sp->s_name, sp->s_namlen, scnp->cn_cred, scnp->cn_proc));
}

/*
 * Do an nfs rename rpc. Called from nfs_rename() and nfs_renameit().
 */
int
nfs_renamerpc(fdvp, fnameptr, fnamelen, tdvp, tnameptr, tnamelen, cred, proc)
	struct vnode *fdvp;
	char *fnameptr;
	int fnamelen;
	struct vnode *tdvp;
	char *tnameptr;
	int tnamelen;
	struct ucred *cred;
	struct proc *proc;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, fwccflag = NFSV3_WCCRATTR, twccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(fdvp);

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(fdvp, NFSPROC_RENAME,
		(NFSX_FH(v3) + NFSX_UNSIGNED)*2 + nfsm_rndup(fnamelen) +
		nfsm_rndup(tnamelen));
	nfsm_fhtom(fdvp, v3);
	nfsm_strtom(fnameptr, fnamelen, NFS_MAXNAMLEN);
	nfsm_fhtom(tdvp, v3);
	nfsm_strtom(tnameptr, tnamelen, NFS_MAXNAMLEN);
	nfsm_request(fdvp, NFSPROC_RENAME, proc, cred);
	if (v3) {
		nfsm_wcc_data(fdvp, fwccflag);
		nfsm_wcc_data(tdvp, twccflag);
	}
	nfsm_reqdone;
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	if (!fwccflag)
		VTONFS(fdvp)->n_attrstamp = 0;
	if (!twccflag)
		VTONFS(tdvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs hard link create call
 */
int
nfs_link(v)
	void *v;
{
	struct vop_link_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, attrflag = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3;

	if (dvp->v_mount != vp->v_mount) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		if (vp == dvp)
			vrele(dvp);
		else
			vput(dvp);
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, cnp->cn_cred, MNT_WAIT, cnp->cn_proc);

	v3 = NFS_ISV3(vp);
	nfsstats.rpccnt[NFSPROC_LINK]++;
	nfsm_reqhead(vp, NFSPROC_LINK,
		NFSX_FH(v3)*2 + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(vp, v3);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LINK, cnp->cn_proc, cnp->cn_cred);
	if (v3) {
		nfsm_postop_attr(vp, attrflag);
		nfsm_wcc_data(dvp, wccflag);
	}
	nfsm_reqdone;
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!attrflag)
		VTONFS(vp)->n_attrstamp = 0;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;

	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	vput(dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs symbolic link create call
 */
int
nfs_symlink(v)
	void *v;
{
	struct vop_symlink_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int slen, error = 0, wccflag = NFSV3_WCCRATTR, gotvp;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vnode *newvp = (struct vnode *)0;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	nfsm_reqhead(dvp, NFSPROC_SYMLINK, NFSX_FH(v3) + 2*NFSX_UNSIGNED +
	    nfsm_rndup(cnp->cn_namelen) + nfsm_rndup(slen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3)
		nfsm_v3attrbuild(vap, FALSE);
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
	if (!v3) {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VLNK, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_SYMLINK, cnp->cn_proc, cnp->cn_cred);
	if (v3) {
		if (!error)
			nfsm_mtofh(dvp, newvp, v3, gotvp);
		nfsm_wcc_data(dvp, wccflag);
	}
	nfsm_reqdone;
	if (newvp)
		vrele(newvp);
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	VN_KNOTE(dvp, NOTE_WRITE);
	vrele(dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs make dir call
 */
int
nfs_mkdir(v)
	void *v;
{
	struct vop_mkdir_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	int len;
	struct nfsnode *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	len = cnp->cn_namelen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	nfsm_reqhead(dvp, NFSPROC_MKDIR,
	  NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_v3attrbuild(vap, FALSE);
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VDIR, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKDIR, cnp->cn_proc, cnp->cn_cred);
	if (!error)
		nfsm_mtofh(dvp, newvp, v3, gotvp);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry
	 * if we can succeed in looking up the directory.
	 */
	if (error == EEXIST || (!error && !gotvp)) {
		if (newvp) {
			vrele(newvp);
			newvp = (struct vnode *)0;
		}
		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
			cnp->cn_proc, &np);
		if (!error) {
			newvp = NFSTOV(np);
			if (newvp->v_type != VDIR)
				error = EEXIST;
		}
	}
	if (error) {
		if (newvp)
			vrele(newvp);
	} else {
		VN_KNOTE(dvp, NOTE_WRITE|NOTE_LINK);
		*ap->a_vpp = newvp;
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	vrele(dvp);
	return (error);
}

/*
 * nfs remove directory call
 */
int
nfs_rmdir(v)
	void *v;
{
	struct vop_rmdir_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	if (dvp == vp) {
		vrele(dvp);
		vrele(dvp);
		pool_put(&namei_pool, cnp->cn_pnbuf);
		return (EINVAL);
	}
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	nfsm_reqhead(dvp, NFSPROC_RMDIR,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_RMDIR, cnp->cn_proc, cnp->cn_cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;

	VN_KNOTE(dvp, NOTE_WRITE|NOTE_LINK);
	VN_KNOTE(vp, NOTE_DELETE);

	cache_purge(dvp);
	cache_purge(vp);
	vrele(vp);
	vrele(dvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}


/*
 * The readdir logic below has a big design bug. It stores the NFS cookie in 
 * the returned uio->uio_offset but does not store the verifier (it cannot).
 * Instead, the code stores the verifier in the nfsnode and applies that
 * verifies to all cookies, no matter what verifier was originally with
 * the cookie.
 *
 * From a practical standpoint, this is not a problem since almost all
 * NFS servers do not change the validity of cookies across deletes
 * and inserts.
 */

struct nfs_dirent {
	u_int32_t cookie[2];
	struct dirent dirent;
};

#define	NFS_DIRHDSIZ    (sizeof (struct nfs_dirent) - (MAXNAMLEN + 1))
#define NFS_DIRENT_OVERHEAD  offsetof(struct nfs_dirent, dirent)

/*
 * nfs readdir call
 */
int
nfs_readdir(v)
	void *v;
{
	struct vop_readdir_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct uio *uio = ap->a_uio;
	int tresid, error;
	struct vattr vattr;
	u_long *cookies = NULL;
	int ncookies = 0, cnt;
	u_int64_t  newoff = uio->uio_offset;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct uio readdir_uio;
	struct iovec readdir_iovec;
	struct proc * p = uio->uio_procp;
	int done = 0, eof = 0;
	struct ucred *cred = ap->a_cred;
	void *data;

	if (vp->v_type != VDIR)
		return (EPERM);
	/*
	 * First, check for hit on the EOF offset cache
	 */
	if (np->n_direofoffset != 0 && 
	    uio->uio_offset == np->n_direofoffset) {
		if (VOP_GETATTR(vp, &vattr, ap->a_cred, uio->uio_procp) == 0 &&
		    np->n_mtime == vattr.va_mtime.tv_sec) {
			nfsstats.direofcache_hits++;
			*ap->a_eofflag = 1;
			return (0);
		}
	}

	if (uio->uio_resid < NFS_FABLKSIZE)
		return (EINVAL);

	tresid = uio->uio_resid;

	if (uio->uio_rw != UIO_READ)
		return (EINVAL);

	if (ap->a_cookies) {
		ncookies = uio->uio_resid / 20;

		MALLOC(cookies, u_long *, sizeof(*cookies) * ncookies,
		       M_TEMP, M_WAITOK);
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}

	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);

	cnt = 5;

	MALLOC(data, void *, NFS_DIRBLKSIZ, M_TEMP, 
	    M_WAITOK);

	do {
		struct nfs_dirent *ndp = data;

		readdir_iovec.iov_len = NFS_DIRBLKSIZ;
		readdir_iovec.iov_base = data;
		readdir_uio.uio_offset = newoff;
		readdir_uio.uio_iov = &readdir_iovec;
		readdir_uio.uio_iovcnt = 1;
		readdir_uio.uio_segflg = UIO_SYSSPACE;
		readdir_uio.uio_rw = UIO_READ;
		readdir_uio.uio_resid = NFS_DIRBLKSIZ;
		readdir_uio.uio_procp = curproc;

		if (nmp->nm_flag & NFSMNT_RDIRPLUS) {
			error = nfs_readdirplusrpc(vp, &readdir_uio, cred, 
			    &eof);
			if (error == NFSERR_NOTSUPP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, &readdir_uio, cred, &eof);

		if (error == NFSERR_BAD_COOKIE)
			error = EINVAL;

		while (error == 0 && 
		    (ap->a_cookies == NULL || ncookies != 0) &&
		    ndp < (struct nfs_dirent *)readdir_iovec.iov_base) {
			struct dirent *dp = &ndp->dirent;
			int reclen = dp->d_reclen;

			dp->d_reclen -= NFS_DIRENT_OVERHEAD;

			if (uio->uio_resid < dp->d_reclen) {
				eof = 0;
				done = 1;
				break;
			}

			error = uiomove((caddr_t)dp, dp->d_reclen, uio);
			if (error) 
				break;
			
			newoff = fxdr_hyper(&ndp->cookie[0]);
			
			if (ap->a_cookies != NULL) {
				*cookies = newoff;
				cookies++;
				ncookies--;
			}

			ndp = (struct nfs_dirent *)((u_int8_t *)ndp + reclen);
		}
	} while (!error && !done && !eof && cnt--);
	
	FREE(data, M_TEMP);
	data = NULL;
	
	if (ap->a_cookies) {
		if (error) {
			FREE(*ap->a_cookies, M_TEMP); 
			*ap->a_cookies = NULL;
			*ap->a_ncookies = 0;
		} else {
			*ap->a_ncookies -= ncookies;
		}
	}
	
	if (!error) 
		uio->uio_offset = newoff;

	if (!error && (eof || uio->uio_resid == tresid)) {
		nfsstats.direofcache_misses++;
		*ap->a_eofflag = 1;
		return (0);
	}

	*ap->a_eofflag = 0;
	return (error);
}


/*
 * The function below stuff the cookies in after the name
 */

/*
 * Readdir rpc call.
 */
int
nfs_readdirrpc(struct vnode *vp, 
    struct uio *uiop, 
    struct ucred *cred,
    int *end_of_directory)
{
	int len, left;
	struct nfs_dirent *ndp = NULL;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag;
	int v3 = NFS_ISV3(vp);

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || 
		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
		panic("nfs readdirrpc bad uio");
#endif

	txdr_hyper(uiop->uio_offset, &cookie.nfsuquad[0]);

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_READDIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		nfsm_reqhead(vp, NFSPROC_READDIR, NFSX_FH(v3) +
			NFSX_READDIR(v3));
		nfsm_fhtom(vp, v3);
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			*tl++ = cookie.nfsuquad[0];
			*tl++ = cookie.nfsuquad[1];
			if (cookie.nfsuquad[0] == 0 &&
			    cookie.nfsuquad[1] == 0) {
				*tl++ = 0;
				*tl++ = 0;
			} else {
				*tl++ = dnp->n_cookieverf.nfsuquad[0];
				*tl++ = dnp->n_cookieverf.nfsuquad[1];
			}
		} else {
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = cookie.nfsuquad[1];
		}
		*tl = txdr_unsigned(nmp->nm_readdirsize);
		nfsm_request(vp, NFSPROC_READDIR, uiop->uio_procp, cred);
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				dnp->n_cookieverf.nfsuquad[0] = *tl++;
				dnp->n_cookieverf.nfsuquad[1] = *tl;
			} else {
				m_freem(mrep);
				goto nfsmout;
			}
		}
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
				fileno = fxdr_hyper(tl);
				len = fxdr_unsigned(int, *(tl + 2));
			} else {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				fileno = fxdr_unsigned(u_quad_t, *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len + 1);
			left = NFS_READDIRBLKSIZ - blksiz;
			if ((tlen + NFS_DIRHDSIZ) > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base += left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if ((tlen + NFS_DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				ndp = (struct nfs_dirent *)
				    uiop->uio_iov->iov_base;
				dp = &ndp->dirent;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen + NFS_DIRHDSIZ;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == NFS_READDIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= NFS_DIRHDSIZ;
				(char *)uiop->uio_iov->iov_base += NFS_DIRHDSIZ;
				uiop->uio_iov->iov_len -= NFS_DIRHDSIZ;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';	/* null terminate */
				uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
			} else {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
			}
			if (bigenough) {
				if (v3) {
					ndp->cookie[0] = cookie.nfsuquad[0] =
					    *tl++;
				} else
					ndp->cookie[0] = 0;

				ndp->cookie[1] = cookie.nfsuquad[1] = *tl++;
			} else if (v3)
				tl += 2;
			else
				tl++;
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_READDIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_READDIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		(char *)uiop->uio_iov->iov_base += left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough) {
		dnp->n_direofoffset = fxdr_hyper(&cookie.nfsuquad[0]);
		if (end_of_directory) *end_of_directory = 1;
	} else {
		if (uiop->uio_resid > 0)
			printf("EEK! readdirrpc resid > 0\n");
	}

nfsmout:
	return (error);
}

/*
 * NFS V3 readdir plus RPC. Used in place of nfs_readdirrpc().
 */
int
nfs_readdirplusrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred, 
    int *end_of_directory)
{
	int len, left;
	struct nfs_dirent *ndirp = NULL;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp;
	caddr_t bpos, dpos, cp2, dpossav1, dpossav2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2, *mdsav1, *mdsav2;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	nfsfh_t *fhp;
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
	int attrflag, fhsize;

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || 
		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
		panic("nfs readdirplusrpc bad uio");
#endif
	ndp->ni_dvp = vp;
	newvp = NULLVP;

	txdr_hyper(uiop->uio_offset, &cookie.nfsuquad[0]);
	
        /*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_READDIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		nfsstats.rpccnt[NFSPROC_READDIRPLUS]++;
		nfsm_reqhead(vp, NFSPROC_READDIRPLUS,
			NFSX_FH(1) + 6 * NFSX_UNSIGNED);
		nfsm_fhtom(vp, 1);
 		nfsm_build(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
		*tl++ = cookie.nfsuquad[0];
		*tl++ = cookie.nfsuquad[1];
		if (cookie.nfsuquad[0] == 0 &&
		    cookie.nfsuquad[1] == 0) {
			*tl++ = 0;
			*tl++ = 0;
		} else {
			*tl++ = dnp->n_cookieverf.nfsuquad[0];
			*tl++ = dnp->n_cookieverf.nfsuquad[1];
		}
		*tl++ = txdr_unsigned(nmp->nm_readdirsize);
		*tl = txdr_unsigned(nmp->nm_rsize);
		nfsm_request(vp, NFSPROC_READDIRPLUS, uiop->uio_procp, cred);
		nfsm_postop_attr(vp, attrflag);
		if (error) {
			m_freem(mrep);
			goto nfsmout;
		}
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		more_dirs = fxdr_unsigned(int, *tl);
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			fileno = fxdr_hyper(tl);
			len = fxdr_unsigned(int, *(tl + 2));
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len + 1);
			left = NFS_READDIRBLKSIZ - blksiz;
			if ((tlen + NFS_DIRHDSIZ) > left) {
				dp->d_reclen += left;
				(char *)uiop->uio_iov->iov_base += left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if ((tlen + NFS_DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				ndirp = (struct nfs_dirent *)
				    uiop->uio_iov->iov_base;
				dp = &ndirp->dirent;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen + NFS_DIRHDSIZ;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == NFS_READDIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= NFS_DIRHDSIZ;
				(char *)uiop->uio_iov->iov_base += NFS_DIRHDSIZ;
				uiop->uio_iov->iov_len -= NFS_DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';
				uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			if (bigenough) {
				ndirp->cookie[0] = cookie.nfsuquad[0] = *tl++;
				ndirp->cookie[1] = cookie.nfsuquad[1] = *tl++;
			} else
				tl += 2;

			/*
			 * Since the attributes are before the file handle
			 * (sigh), we must skip over the attributes and then
			 * come back and get them.
			 */
			attrflag = fxdr_unsigned(int, *tl);
			if (attrflag) {
			    dpossav1 = dpos;
			    mdsav1 = md;
			    nfsm_adv(NFSX_V3FATTR);
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    doit = fxdr_unsigned(int, *tl);
			    if (doit) {
				nfsm_getfh(fhp, fhsize, 1);
				if (NFS_CMPFH(dnp, fhp, fhsize)) {
				    VREF(vp);
				    newvp = vp;
				    np = dnp;
				} else {
				    error = nfs_nget(vp->v_mount, fhp,
					fhsize, &np);
				    if (error)
					doit = 0;
				    else
					newvp = NFSTOV(np);
				}
			    }
			    if (doit && bigenough) {
				dpossav2 = dpos;
				dpos = dpossav1;
				mdsav2 = md;
				md = mdsav1;
				nfsm_loadattr(newvp, (struct vattr *)0);
				dpos = dpossav2;
				md = mdsav2;
				dp->d_type =
				    IFTODT(VTTOIF(np->n_vattr.va_type));
				if (cnp->cn_namelen <= NCHNAMLEN) {
					ndp->ni_vp = newvp;
					cnp->cn_hash =
					    hash32_str(cnp->cn_nameptr,
					        HASHINIT);
					cache_enter(ndp->ni_dvp, ndp->ni_vp,
					    cnp);
				}
			    }
			} else {
			    /* Just skip over the file handle */
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    i = fxdr_unsigned(int, *tl);
			    nfsm_adv(nfsm_rndup(i));
			}
			if (newvp != NULLVP) {
			    vrele(newvp);
			    newvp = NULLVP;
			}
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_READDIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_READDIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		(char *)uiop->uio_iov->iov_base += left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough) {
		dnp->n_direofoffset = fxdr_hyper(&cookie.nfsuquad[0]);
		if (end_of_directory) *end_of_directory = 1;
	} else {
		if (uiop->uio_resid > 0)
			printf("EEK! readdirplusrpc resid > 0\n");
	}

nfsmout:
	if (newvp != NULLVP)
		vrele(newvp);
	return (error);
}

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
int
nfs_sillyrename(dvp, vp, cnp)
	struct vnode *dvp, *vp;
	struct componentname *cnp;
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;

	cache_purge(dvp);
	np = VTONFS(vp);
	MALLOC(sp, struct sillyrename *, sizeof (struct sillyrename),
		M_NFSREQ, M_WAITOK);
	sp->s_cred = crdup(cnp->cn_cred);
	sp->s_dvp = dvp;
	VREF(dvp);

	if (vp->v_type == VDIR) {
#ifdef DIAGNOSTIC
		printf("nfs: sillyrename dir\n");
#endif
		error = EINVAL;
		goto bad;
	}

	/* Fudge together a funny name */
	sp->s_namlen = snprintf(sp->s_name, sizeof sp->s_name,
	    ".nfsA%05x4.4", cnp->cn_proc->p_pid);
	if (sp->s_namlen > sizeof sp->s_name)
		sp->s_namlen = strlen(sp->s_name);

	/* Try lookitups until we get one that isn't there */
	while (nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_proc, (struct nfsnode **)0) == 0) {
		sp->s_name[4]++;
		if (sp->s_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	error = nfs_renameit(dvp, cnp, sp);
	if (error)
		goto bad;
	error = nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_proc, &np);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	crfree(sp->s_cred);
	FREE((caddr_t)sp, M_NFSREQ);
	return (error);
}

/*
 * Look up a file name and optionally either update the file handle or
 * allocate an nfsnode, depending on the value of npp.
 * npp == NULL	--> just do the lookup
 * *npp == NULL --> allocate a new nfsnode and make sure attributes are
 *			handled too
 * *npp != NULL --> update the file handle in the vnode
 */
int
nfs_lookitup(dvp, name, len, cred, procp, npp)
	struct vnode *dvp;
	char *name;
	int len;
	struct ucred *cred;
	struct proc *procp;
	struct nfsnode **npp;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	caddr_t bpos, dpos, cp2;
	int error = 0, fhlen, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	nfsfh_t *nfhp;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, procp, cred);
	if (npp && !error) {
		nfsm_getfh(nfhp, fhlen, v3);
		if (*npp) {
		    np = *npp;
		    if (np->n_fhsize > NFS_SMALLFH && fhlen <= NFS_SMALLFH) {
			free((caddr_t)np->n_fhp, M_NFSBIGFH);
			np->n_fhp = &np->n_fh;
		    } else if (np->n_fhsize <= NFS_SMALLFH && fhlen>NFS_SMALLFH)
			np->n_fhp =(nfsfh_t *)malloc(fhlen,M_NFSBIGFH,M_WAITOK);
		    bcopy((caddr_t)nfhp, (caddr_t)np->n_fhp, fhlen);
		    np->n_fhsize = fhlen;
		    newvp = NFSTOV(np);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
		    VREF(dvp);
		    newvp = dvp;
		} else {
		    error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np);
		    if (error) {
			m_freem(mrep);
			return (error);
		    }
		    newvp = NFSTOV(np);
		}
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			if (!attrflag && *npp == NULL) {
				m_freem(mrep);
				vrele(newvp);
				return (ENOENT);
			}
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
	}
	nfsm_reqdone;
	if (npp && *npp == NULL) {
		if (error) {
			if (newvp)
				vrele(newvp);
		} else
			*npp = np;
	}
	return (error);
}

/*
 * Nfs Version 3 commit rpc
 */
int
nfs_commit(vp, offset, cnt, procp)
	struct vnode *vp;
	u_quad_t offset;
	int cnt;
	struct proc *procp;
{
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1, t2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	
	if ((nmp->nm_flag & NFSMNT_HASWRITEVERF) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;
	nfsm_reqhead(vp, NFSPROC_COMMIT, NFSX_FH(1));
	nfsm_fhtom(vp, 1);
	nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nfsm_request(vp, NFSPROC_COMMIT, procp, VTONFS(vp)->n_wcred);
	nfsm_wcc_data(vp, wccflag);
	if (!error) {
		nfsm_dissect(tl, u_int32_t *, NFSX_V3WRITEVERF);
		if (bcmp((caddr_t)nmp->nm_verf, (caddr_t)tl,
			NFSX_V3WRITEVERF)) {
			bcopy((caddr_t)tl, (caddr_t)nmp->nm_verf,
				NFSX_V3WRITEVERF);
			error = NFSERR_STALEWRITEVERF;
		}
	}
	nfsm_reqdone;
	return (error);
}

/*
 * Kludge City..
 * - make nfs_bmap() essentially a no-op that does no translation
 * - do nfs_strategy() by doing I/O with nfs_readrpc/nfs_writerpc
 *   (Maybe I could use the process's page mapping, but I was concerned that
 *    Kernel Write might not be enabled and also figured copyout() would do
 *    a lot more work than bcopy() and also it currently happens in the
 *    context of the swapper process (2).
 */
int
nfs_bmap(v)
	void *v;
{
	struct vop_bmap_args *ap = v;
	struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
	return (0);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
int
nfs_strategy(v)
	void *v;
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	struct proc *p;
	int error = 0;

	if ((bp->b_flags & (B_PHYS|B_ASYNC)) == (B_PHYS|B_ASYNC))
		panic("nfs physio/async");
	if (bp->b_flags & B_ASYNC)
		p = NULL;
	else
		p = curproc;	/* XXX */
	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 || nfs_asyncio(bp))
		error = nfs_doio(bp, p);
	return (error);
}

/*
 * fsync vnode op. Just call nfs_flush() with commit == 1.
 */
/* ARGSUSED */
int
nfs_fsync(v)
	void *v;
{
	struct vop_fsync_args *ap = v;

	return (nfs_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p, 1));
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
int
nfs_flush(vp, cred, waitfor, p, commit)
	struct vnode *vp;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
	int commit;
{
	struct nfsnode *np = VTONFS(vp);
	struct buf *bp;
	int i;
	struct buf *nbp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int s, error = 0, slptimeo = 0, slpflag = 0, retv, bvecpos;
	int passone = 1;
	u_quad_t off = (u_quad_t)-1, endoff = 0, toff;
#ifndef NFS_COMMITBVECSIZ
#define NFS_COMMITBVECSIZ	20
#endif
	struct buf *bvec[NFS_COMMITBVECSIZ];

	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
	if (!commit)
		passone = 0;
	/*
	 * A b_flags == (B_DELWRI | B_NEEDCOMMIT) block has been written to the
	 * server, but nas not been committed to stable storage on the server
	 * yet. On the first pass, the byte range is worked out and the commit
	 * rpc is done. On the second pass, nfs_writebp() is called to do the
	 * job.
	 */
again:
	bvecpos = 0;
	if (NFS_ISV3(vp) && commit) {
		s = splbio();
		for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp != NULL; bp = nbp) {
			nbp = LIST_NEXT(bp, b_vnbufs);
			if (bvecpos >= NFS_COMMITBVECSIZ)
				break;
			if ((bp->b_flags & (B_BUSY | B_DELWRI | B_NEEDCOMMIT))
				!= (B_DELWRI | B_NEEDCOMMIT))
				continue;
			bremfree(bp);
			bp->b_flags |= (B_BUSY | B_WRITEINPROG);
			/*
			 * A list of these buffers is kept so that the
			 * second loop knows which buffers have actually
			 * been committed. This is necessary, since there
			 * may be a race between the commit rpc and new
			 * uncommitted writes on the file.
			 */
			bvec[bvecpos++] = bp;
			toff = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
				bp->b_dirtyoff;
			if (toff < off)
				off = toff;
			toff += (u_quad_t)(bp->b_dirtyend - bp->b_dirtyoff);
			if (toff > endoff)
				endoff = toff;
		}
		splx(s);
	}
	if (bvecpos > 0) {
		/*
		 * Commit data on the server, as required.
		 */
		retv = nfs_commit(vp, off, (int)(endoff - off), p);
		if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(vp->v_mount);
		/*
		 * Now, either mark the blocks I/O done or mark the
		 * blocks dirty, depending on whether the commit
		 * succeeded.
		 */
		for (i = 0; i < bvecpos; i++) {
			bp = bvec[i];
			bp->b_flags &= ~(B_NEEDCOMMIT | B_WRITEINPROG);
			if (retv)
			    brelse(bp);
			else {
			    s = splbio();
			    buf_undirty(bp);
			    vp->v_numoutput++;
			    bp->b_flags |= B_ASYNC;
			    bp->b_flags &= ~(B_READ|B_DONE|B_ERROR);
			    bp->b_dirtyoff = bp->b_dirtyend = 0;
			    biodone(bp);
			    splx(s);
			}
		}
	}

	/*
	 * Start/do any write(s) that are required.
	 */
loop:
	s = splbio();
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp != NULL; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_flags & B_BUSY) {
			if (waitfor != MNT_WAIT || passone)
				continue;
			bp->b_flags |= B_WANTED;
			error = tsleep((caddr_t)bp, slpflag | (PRIBIO + 1),
				"nfsfsync", slptimeo);
			splx(s);
			if (error) {
			    if (nfs_sigintr(nmp, (struct nfsreq *)0, p))
				return (EINTR);
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			}
			goto loop;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("nfs_fsync: not dirty");
		if ((passone || !commit) && (bp->b_flags & B_NEEDCOMMIT))
			continue;
		bremfree(bp);
		if (passone || !commit)
		    bp->b_flags |= (B_BUSY|B_ASYNC);
		else
		    bp->b_flags |= (B_BUSY|B_ASYNC|B_WRITEINPROG|B_NEEDCOMMIT);
		splx(s);
		VOP_BWRITE(bp);
		goto loop;
	}
	splx(s);
	if (passone) {
		passone = 0;
		goto again;
	}
	if (waitfor == MNT_WAIT) {
 loop2:
	        s = splbio();
		error = vwaitforio(vp, slpflag, "nfs_fsync", slptimeo);
		splx(s);
		if (error) {
			if (nfs_sigintr(nmp, (struct nfsreq *)0, p))
				return (EINTR);
			if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			}
			goto loop2;
		}

		if (LIST_FIRST(&vp->v_dirtyblkhd) && commit) {
#if 0
			vprint("nfs_fsync: dirty", vp);
#endif
			goto loop;
		}
	}
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
	return (error);
}

/*
 * Return POSIX pathconf information applicable to nfs.
 *
 * The NFS V2 protocol doesn't support this, so just return EINVAL
 * for V2.
 */
/* ARGSUSED */
int
nfs_pathconf(v)
	void *v;
{
#if 0
	struct vop_pathconf_args *ap = v;
#endif

	return (EINVAL);
}

/*
 * NFS advisory byte-level locks.
 */
int
nfs_advlock(v)
	void *v;
{
	struct vop_advlock_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	return (lf_advlock(&np->n_lockf, np->n_size, ap->a_id, ap->a_op,
	    ap->a_fl, ap->a_flags));
}

/*
 * Print out the contents of an nfsnode.
 */
int
nfs_print(v)
	void *v;
{
	struct vop_print_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %ld fsid 0x%lx",
		np->n_vattr.va_fileid, np->n_vattr.va_fsid);
#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif
	printf("\n");
	return (0);
}

/*
 * Just call nfs_writebp() with the force argument set to 1.
 */
int
nfs_bwrite(v)
	void *v;
{
	struct vop_bwrite_args *ap = v;

	return (nfs_writebp(ap->a_bp, 1));
}

/*
 * This is a clone of vop_generic_bwrite(), except that B_WRITEINPROG isn't set unless
 * the force flag is one and it also handles the B_NEEDCOMMIT flag.
 */
int
nfs_writebp(bp, force)
	struct buf *bp;
	int force;
{
	int oldflags = bp->b_flags, retv = 1;
	struct proc *p = curproc;	/* XXX */
	off_t off;
	size_t cnt;
	int   s;
	struct vnode *vp;
	struct nfsnode *np;

	if(!(bp->b_flags & B_BUSY))
		panic("bwrite: buffer is not busy???");

	vp = bp->b_vp;
	np = VTONFS(vp);

	bp->b_flags &= ~(B_READ|B_DONE|B_ERROR);

	s = splbio();
	buf_undirty(bp);

	if ((oldflags & B_ASYNC) && !(oldflags & B_DELWRI) && p)
		++p->p_stats->p_ru.ru_oublock;

	bp->b_vp->v_numoutput++;
	splx(s);

	/*
	 * If B_NEEDCOMMIT is set, a commit rpc may do the trick. If not
	 * an actual write will have to be scheduled via. VOP_STRATEGY().
	 * If B_WRITEINPROG is already set, then push it with a write anyhow.
	 */
	if ((oldflags & (B_NEEDCOMMIT | B_WRITEINPROG)) == B_NEEDCOMMIT) {
		off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE + bp->b_dirtyoff;
		cnt = bp->b_dirtyend - bp->b_dirtyoff;

		rw_enter_write(&np->n_commitlock);
		if (!(bp->b_flags & B_NEEDCOMMIT)) {
			rw_exit_write(&np->n_commitlock);
			return (0);
		}

		/*
		 * If it's already been commited by somebody else,
		 * bail.
		 */
		if (!nfs_in_committed_range(vp, bp)) {
			int pushedrange = 0;
			/*
			 * Since we're going to do this, push as much
			 * as we can.
			 */

			if (nfs_in_tobecommitted_range(vp, bp)) {
				pushedrange = 1;
				off = np->n_pushlo;
				cnt = np->n_pushhi - np->n_pushlo;
			}

			bp->b_flags |= B_WRITEINPROG;
			retv = nfs_commit(bp->b_vp, off, cnt, bp->b_proc);
			bp->b_flags &= ~B_WRITEINPROG;

			if (retv == 0) {
				if (pushedrange)
					nfs_merge_commit_ranges(vp);
				else 
					nfs_add_committed_range(vp, bp);
			}
		} else
			retv = 0; /* It has already been commited. */

		rw_exit_write(&np->n_commitlock);
		if (!retv) {
			bp->b_dirtyoff = bp->b_dirtyend = 0;
			bp->b_flags &= ~B_NEEDCOMMIT;
			s = splbio();
			biodone(bp);
			splx(s);
		} else if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(bp->b_vp->v_mount);
	}
	if (retv) {
		if (force)
			bp->b_flags |= B_WRITEINPROG;
		VOP_STRATEGY(bp);
	}

	if( (oldflags & B_ASYNC) == 0) {
		int rtval = biowait(bp);
		if (!(oldflags & B_DELWRI) && p) {
			++p->p_stats->p_ru.ru_oublock;
		}
		brelse(bp);
		return (rtval);
	} 

	return (0);
}

/*
 * nfs special file access vnode op.
 * Essentially just get vattr and then imitate iaccess() since the device is
 * local to the client.
 */
int
nfsspec_access(v)
	void *v;
{
	struct vop_access_args *ap = v;
	struct vattr va;
	struct vnode *vp = ap->a_vp;
	int error;

        /*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	error = VOP_GETATTR(vp, &va, ap->a_cred, ap->a_p);
	if (error)
		return (error);

	return (vaccess(va.va_mode, va.va_uid, va.va_gid, ap->a_mode,
	    ap->a_cred));
}

/* ARGSUSED */
int
nfs_poll(v)
        void *v;
{
	struct vop_poll_args *ap = v;

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Read wrapper for special devices.
 */
int
nfsspec_read(v)
	void *v;
{
	struct vop_read_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
nfsspec_write(v)
	void *v;
{
	struct vop_write_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the nfsnode then do device close.
 */
int
nfsspec_close(v)
	void *v;
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_close), ap));
}

#ifdef FIFO
/*
 * Read wrapper for fifos.
 */
int
nfsfifo_read(v)
	void *v;
{
	struct vop_read_args *ap = v;
	extern int (**fifo_vnodeop_p)(void *);
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifos.
 */
int
nfsfifo_write(v)
	void *v;
{
	struct vop_write_args *ap = v;
	extern int (**fifo_vnodeop_p)(void *);
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
int
nfsfifo_close(v)
	void *v;
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	extern int (**fifo_vnodeop_p)(void *);

	if (np->n_flag & (NACC | NUPD)) {
		if (np->n_flag & NACC) {
			getnanotime(&np->n_atim);
		}
		if (np->n_flag & NUPD) {
			getnanotime(&np->n_mtim);
		}
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), ap));
}

int
nfsfifo_reclaim(void *v)
{
	fifo_reclaim(v);
	return (nfs_reclaim(v));
}
#endif /* ! FIFO */
