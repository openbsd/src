/*	$OpenBSD: nfs_vnops.c,v 1.171 2017/02/22 11:42:46 mpi Exp $	*/
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
#include <sys/lock.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/queue.h>
#include <sys/specdev.h>
#include <sys/unistd.h>

#include <miscfs/fifofs/fifo.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_var.h>

#include <uvm/uvm_extern.h>

#include <netinet/in.h>

int nfs_access(void *);
int nfs_advlock(void *);
int nfs_bmap(void *);
int nfs_bwrite(void *);
int nfs_close(void *);
int nfs_commit(struct vnode *, u_quad_t, int, struct proc *);
int nfs_create(void *);
int nfs_flush(struct vnode *, struct ucred *, int, struct proc *, int);
int nfs_fsync(void *);
int nfs_getattr(void *);
int nfs_getreq(struct nfsrv_descript *, struct nfsd *, int);
int nfs_link(void *);
int nfs_lookitup(struct vnode *, char *, int, struct ucred *, struct proc *,
	struct nfsnode **);
int nfs_lookup(void *);
int nfs_mkdir(void *);
int nfs_mknod(void *);
int nfs_mknodrpc(struct vnode *, struct vnode **, struct componentname *,
	struct vattr *);
int nfs_null(struct vnode *, struct ucred *, struct proc *);
int nfs_open(void *);
int nfs_pathconf(void *);
int nfs_poll(void *);
int nfs_print(void *);
int nfs_read(void *);
int nfs_readdir(void *);
int nfs_readdirplusrpc(struct vnode *, struct uio *, struct ucred *, int *,
	struct proc *);
int nfs_readdirrpc(struct vnode *, struct uio *, struct ucred *, int *);
int nfs_remove(void *);
int nfs_removerpc(struct vnode *, char *, int, struct ucred *, struct proc *);
int nfs_rename(void *);
int nfs_renameit(struct vnode *, struct componentname *, struct sillyrename *);
int nfs_renamerpc(struct vnode *, char *, int, struct vnode *, char *, int,
	struct ucred *, struct proc *);
int nfs_rmdir(void *);
int nfs_setattr(void *);
int nfs_setattrrpc(struct vnode *, struct vattr *, struct ucred *,
	struct proc *);
int nfs_sillyrename(struct vnode *, struct vnode *,
			 struct componentname *);
int nfs_strategy(void *);
int nfs_symlink(void *);

void nfs_cache_enter(struct vnode *, struct vnode *, struct componentname *);

int nfsfifo_close(void *);
int nfsfifo_read(void *);
int nfsfifo_reclaim(void *);
int nfsfifo_write(void *);

int nfsspec_access(void *);
int nfsspec_close(void *);
int nfsspec_read(void *);
int nfsspec_write(void *);

/* Global vfs data structures for nfs. */
struct vops nfs_vops = {
	.vop_lookup	= nfs_lookup,
	.vop_create	= nfs_create,
	.vop_mknod	= nfs_mknod,
	.vop_open	= nfs_open,
	.vop_close	= nfs_close,
	.vop_access	= nfs_access,
	.vop_getattr	= nfs_getattr,
	.vop_setattr	= nfs_setattr,
	.vop_read	= nfs_read,
	.vop_write	= nfs_write,
	.vop_ioctl	= nfs_ioctl,
	.vop_poll	= nfs_poll,
	.vop_kqfilter	= nfs_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_fsync	= nfs_fsync,
	.vop_remove	= nfs_remove,
	.vop_link	= nfs_link,
	.vop_rename	= nfs_rename,
	.vop_mkdir	= nfs_mkdir,
	.vop_rmdir	= nfs_rmdir,
	.vop_symlink	= nfs_symlink,
	.vop_readdir	= nfs_readdir,
	.vop_readlink	= nfs_readlink,
	.vop_abortop	= vop_generic_abortop,
	.vop_inactive	= nfs_inactive,
	.vop_reclaim	= nfs_reclaim,
	.vop_lock	= vop_generic_lock,	/* XXX: beck@ must fix this. */
	.vop_unlock	= vop_generic_unlock,
	.vop_bmap	= nfs_bmap,
	.vop_strategy	= nfs_strategy,
	.vop_print	= nfs_print,
	.vop_islocked	= vop_generic_islocked,
	.vop_pathconf	= nfs_pathconf,
	.vop_advlock	= nfs_advlock,
	.vop_bwrite	= nfs_bwrite
};

/* Special device vnode ops. */
struct vops nfs_specvops = {
	.vop_close	= nfsspec_close,
	.vop_access	= nfsspec_access,
	.vop_getattr	= nfs_getattr,
	.vop_setattr	= nfs_setattr,
	.vop_read	= nfsspec_read,
	.vop_write	= nfsspec_write,
	.vop_fsync	= nfs_fsync,
	.vop_inactive	= nfs_inactive,
	.vop_reclaim	= nfs_reclaim,
	.vop_lock	= vop_generic_lock,
	.vop_unlock	= vop_generic_unlock,
	.vop_print	= nfs_print,
	.vop_islocked	= vop_generic_islocked,

	/* XXX: Keep in sync with spec_vops. */
	.vop_lookup	= vop_generic_lookup,
	.vop_create	= spec_badop,
	.vop_mknod	= spec_badop,
	.vop_open	= spec_open,
	.vop_ioctl	= spec_ioctl,
	.vop_poll	= spec_poll,
	.vop_kqfilter	= spec_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_remove	= spec_badop,
	.vop_link	= spec_badop,
	.vop_rename	= spec_badop,
	.vop_mkdir	= spec_badop,
	.vop_rmdir	= spec_badop,
	.vop_symlink	= spec_badop,
	.vop_readdir	= spec_badop,
	.vop_readlink	= spec_badop,
	.vop_abortop	= spec_badop,
	.vop_bmap	= vop_generic_bmap,
	.vop_strategy	= spec_strategy,
	.vop_pathconf	= spec_pathconf,
	.vop_advlock	= spec_advlock,
	.vop_bwrite	= vop_generic_bwrite,
};

#ifdef FIFO
struct vops nfs_fifovops = {
	.vop_close	= nfsfifo_close,
	.vop_access	= nfsspec_access,
	.vop_getattr	= nfs_getattr,
	.vop_setattr	= nfs_setattr,
	.vop_read	= nfsfifo_read,
	.vop_write	= nfsfifo_write,
	.vop_fsync	= nfs_fsync,
	.vop_inactive	= nfs_inactive,
	.vop_reclaim	= nfsfifo_reclaim,
	.vop_lock	= vop_generic_lock,
	.vop_unlock	= vop_generic_unlock,
	.vop_print	= nfs_print,
	.vop_islocked	= vop_generic_islocked,
	.vop_bwrite	= vop_generic_bwrite,

	/* XXX: Keep in sync with fifo_vops. */
	.vop_lookup	= vop_generic_lookup,
	.vop_create	= fifo_badop,
	.vop_mknod	= fifo_badop,
	.vop_open	= fifo_open,
	.vop_ioctl	= fifo_ioctl,
	.vop_poll	= fifo_poll,
	.vop_kqfilter	= fifo_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_remove	= fifo_badop,
	.vop_link	= fifo_badop,
	.vop_rename	= fifo_badop,
	.vop_mkdir	= fifo_badop,
	.vop_rmdir	= fifo_badop,
	.vop_symlink	= fifo_badop,
	.vop_readdir	= fifo_badop,
	.vop_readlink	= fifo_badop,
	.vop_abortop	= fifo_badop,
	.vop_bmap	= vop_generic_bmap,
	.vop_strategy	= fifo_badop,
	.vop_pathconf	= fifo_pathconf,
	.vop_advlock	= fifo_advlock,
};
#endif /* FIFO */

/*
 * Global variables
 */
extern u_int32_t nfs_true, nfs_false;
extern u_int32_t nfs_xdrneg1;
extern struct nfsstats nfsstats;
extern nfstype nfsv3_type[9];
int nfs_numasync = 0;

void
nfs_cache_enter(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct nfsnode *np;

	if (vp != NULL) {
		np = VTONFS(vp);
		np->n_ctime = np->n_vattr.va_ctime.tv_sec;
	} else {
		np = VTONFS(dvp);
		if (!np->n_ctime)
			np->n_ctime = np->n_vattr.va_mtime.tv_sec;
	}

	cache_enter(dvp, vp, cnp);
}

/*
 * nfs null call from vfs.
 */
int
nfs_null(struct vnode *vp, struct ucred *cred, struct proc *procp)
{
	struct nfsm_info	 info;
	int			 error = 0;

	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(0);
	error = nfs_request(vp, NFSPROC_NULL, &info);
	m_freem(info.nmi_mrep);
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
int
nfs_access(void *v)
{
	struct vop_access_args *ap = v;
	struct vnode *vp = ap->a_vp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, attrflag;
	u_int32_t mode, rmode;
	int v3 = NFS_ISV3(vp);
	int cachevalid;
	struct nfsm_info	info;

	struct nfsnode *np = VTONFS(vp);

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
	 * Check access cache first. If a request has been made for this uid
	 * shortly before, use the cached result.
	 */
	 cachevalid = (np->n_accstamp != -1 &&
	     (time_second - np->n_accstamp) < nfs_attrtimeo(np) &&
	     np->n_accuid == ap->a_cred->cr_uid);

	if (cachevalid) {
		if (!np->n_accerror) {
			if ((np->n_accmode & ap->a_mode) == ap->a_mode)
				return (np->n_accerror);
		} else if ((np->n_accmode & ap->a_mode) == np->n_accmode)
			return (np->n_accerror);
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
		info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(v3) + NFSX_UNSIGNED);
		nfsm_fhtom(&info, vp, v3);
		tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
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

		info.nmi_procp = ap->a_p;
		info.nmi_cred = ap->a_cred;
		error = nfs_request(vp, NFSPROC_ACCESS, &info);

		nfsm_postop_attr(vp, attrflag);
		if (error) {
			m_freem(info.nmi_mrep);
			goto nfsmout;
		}

		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		rmode = fxdr_unsigned(u_int32_t, *tl);
		/*
		 * The NFS V3 spec does not clarify whether or not
		 * the returned access bits can be a superset of
		 * the ones requested, so...
		 */
		if ((rmode & mode) != mode)
			error = EACCES;

		m_freem(info.nmi_mrep);
	} else
		return (nfsspec_access(ap));


	/*
	 * If we got the same result as for a previous, different request, OR
	 * it in. Don't update the timestamp in that case.
	 */
	 if (!error || error == EACCES) {
		if (cachevalid && np->n_accstamp != -1 &&
		    error == np->n_accerror) {
			if (!error)
				np->n_accmode |= ap->a_mode;
			else {
				if ((np->n_accmode & ap->a_mode) == ap->a_mode)
					np->n_accmode = ap->a_mode;
			}
		} else {
			np->n_accstamp = time_second;
			np->n_accuid = ap->a_cred->cr_uid;
			np->n_accmode = ap->a_mode;
			np->n_accerror = error;
		}
	}
nfsmout:
	return (error);
}

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
int
nfs_open(void *v)
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
		error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
		if (error == EINTR)
			return (error);
		uvm_vnp_uncache(vp);
		NFS_INVALIDATE_ATTRCACHE(np);
		if (vp->v_type == VDIR)
			np->n_direofoffset = 0;
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return (error);
		np->n_mtime = vattr.va_mtime;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return (error);
		if (timespeccmp(&np->n_mtime, &vattr.va_mtime, !=)) {
			if (vp->v_type == VDIR)
				np->n_direofoffset = 0;
			error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
			if (error == EINTR)
				return (error);
			uvm_vnp_uncache(vp);
			np->n_mtime = vattr.va_mtime;
		}
	}
	/* For open/close consistency. */
	NFS_INVALIDATE_ATTRCACHE(np);
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
 *		       or commit them (this satisfies 1 and 2 except for the
 *		       case where the server crashes after this close but
 *		       before the commit RPC, which is felt to be "good
 *		       enough". Changing the last argument to nfs_flush() to
 *		       a 1 would force a commit operation, if it is felt a
 *		       commit is necessary now.
 */
int
nfs_close(void *v)
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
		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p);
		NFS_INVALIDATE_ATTRCACHE(np);
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
nfs_getattr(void *v)
{
	struct vop_getattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsm_info	info;
	int32_t t1;
	int error = 0;

	info.nmi_v3 = NFS_ISV3(vp);

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
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3));
	nfsm_fhtom(&info, vp, info.nmi_v3);
	info.nmi_procp = ap->a_p;
	info.nmi_cred = ap->a_cred;
	error = nfs_request(vp, NFSPROC_GETATTR, &info);
	if (!error)
		nfsm_loadattr(vp, ap->a_vap);
	m_freem(info.nmi_mrep);
nfsmout: 
	return (error);
}

/*
 * nfs setattr call.
 */
int
nfs_setattr(void *v)
{
	struct vop_setattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	int hint = NOTE_ATTRIB;
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
	    vap->va_gid != (gid_t)VNOVAL ||
	    vap->va_atime.tv_nsec != VNOVAL ||
	    vap->va_mtime.tv_nsec != VNOVAL ||
	    vap->va_mode != (mode_t)VNOVAL) &&
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
			if (vap->va_mtime.tv_nsec == VNOVAL &&
			    vap->va_atime.tv_nsec == VNOVAL &&
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
				     ap->a_cred, ap->a_p);
			else
				error = nfs_vinvalbuf(vp, V_SAVE,
				     ap->a_cred, ap->a_p);
			if (error)
				return (error);
			tsize = np->n_size;
			np->n_size = np->n_vattr.va_size = vap->va_size;
			uvm_vnp_setsize(vp, np->n_size);
		};
	} else if ((vap->va_mtime.tv_nsec != VNOVAL ||
		vap->va_atime.tv_nsec != VNOVAL) &&
		vp->v_type == VREG &&
		(error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
		    ap->a_p)) == EINTR)
		return (error);
	error = nfs_setattrrpc(vp, vap, ap->a_cred, ap->a_p);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = np->n_vattr.va_size = tsize;
		uvm_vnp_setsize(vp, np->n_size);
	}

	if (vap->va_size != VNOVAL && vap->va_size < tsize)
		hint |= NOTE_TRUNCATE;

	VN_KNOTE(vp, hint); /* XXX setattrrpc? */

	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
int
nfs_setattrrpc(struct vnode *vp, struct vattr *vap, struct ucred *cred,
    struct proc *procp)
{
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	int32_t t1;
	caddr_t cp2;
	u_int32_t *tl;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int v3 = NFS_ISV3(vp);

	info.nmi_v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(v3) + NFSX_SATTR(v3));
	nfsm_fhtom(&info, vp, v3);

	if (info.nmi_v3) {
		nfsm_v3attrbuild(&info.nmi_mb, vap, 1);
		tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		sp = nfsm_build(&info.nmi_mb, NFSX_V2SATTR);
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

	info.nmi_procp = procp;
	info.nmi_cred = cred;
	error = nfs_request(vp, NFSPROC_SETATTR, &info);

	if (info.nmi_v3)
		nfsm_wcc_data(vp, wccflag);
	else if (error == 0)
		nfsm_loadattr(vp, NULL);

	m_freem(info.nmi_mrep);
nfsmout: 
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
int
nfs_lookup(void *v)
{
	struct vop_lookup_args *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct proc *p = cnp->cn_proc;
	struct nfsm_info	info;
	int flags;
	struct vnode *newvp;
	u_int32_t *tl;
	int32_t t1;
	struct nfsmount *nmp;
	caddr_t cp2;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np;
	int lockparent, wantparent, error = 0, attrflag, fhsize;

	info.nmi_v3 = NFS_ISV3(dvp);

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
				VOP_UNLOCK(dvp, p);
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
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);

	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp, NFSPROC_LOOKUP, &info);

	if (error) {
		if (info.nmi_v3)
			nfsm_postop_attr(dvp, attrflag);
		m_freem(info.nmi_mrep);
		goto nfsmout;
	}

	nfsm_getfh(fhp, fhsize, info.nmi_v3);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize)) {
			m_freem(info.nmi_mrep);
			return (EISDIR);
		}
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(info.nmi_mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		if (info.nmi_v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, NULL);
		*vpp = newvp;
		m_freem(info.nmi_mrep);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent) {
			VOP_UNLOCK(dvp, p);
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
		vref(dvp);
		newvp = dvp;
		if (info.nmi_v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, NULL);
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, p);
		cnp->cn_flags |= PDIRUNLOCK;

		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			if (vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p) == 0)
				cnp->cn_flags &= ~PDIRUNLOCK;
			m_freem(info.nmi_mrep);
			return (error);
		}
		newvp = NFSTOV(np);

		if (info.nmi_v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, NULL);

		if (lockparent && (flags & ISLASTCN)) {
			if ((error = vn_lock(dvp, LK_EXCLUSIVE, p))) {
				m_freem(info.nmi_mrep);
				vput(newvp);
				return error;
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}

	} else {
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(info.nmi_mrep);
			return error;
		}
		newvp = NFSTOV(np);
		if (info.nmi_v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, NULL);
		if (!lockparent || !(flags & ISLASTCN)) {
			VOP_UNLOCK(dvp, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
	}

	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))) {
		nfs_cache_enter(dvp, newvp, cnp);
	}

	*vpp = newvp;
	m_freem(info.nmi_mrep);

nfsmout: 
	if (error) {
		/*
		 * We get here only because of errors returned by
		 * the RPC. Otherwise we'll have returned above
		 * (the nfsm_* macros will jump to nfsmout
		 * on error).
		 */
		if (error == ENOENT && (cnp->cn_flags & MAKEENTRY) &&
		    cnp->cn_nameiop != CREATE) {
			nfs_cache_enter(dvp, NULL, cnp);
		}
		if (newvp != NULLVP) {
			vrele(newvp);
			if (newvp != dvp)
				VOP_UNLOCK(newvp, p);
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
nfs_read(void *v)
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
nfs_readlink(void *v)
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
nfs_readlinkrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, len, attrflag;

	info.nmi_v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3));
	nfsm_fhtom(&info, vp, info.nmi_v3);

	info.nmi_procp = curproc;
	info.nmi_cred = cred;
	error = nfs_request(vp, NFSPROC_READLINK, &info);

	if (info.nmi_v3)
		nfsm_postop_attr(vp, attrflag);
	if (!error) {
		nfsm_strsiz(len, NFS_MAXPATHLEN);
		nfsm_mtouio(uiop, len);
	}

	m_freem(info.nmi_mrep);

nfsmout: 
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(struct vnode *vp, struct uio *uiop)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	struct nfsmount *nmp;
	int error = 0, len, retlen, tsiz, eof, attrflag;

	info.nmi_v3 = NFS_ISV3(vp);

	eof = 0;

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff && !info.nmi_v3)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
		    NFSX_UNSIGNED * 3);
		nfsm_fhtom(&info, vp, info.nmi_v3);
		tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED * 3);
		if (info.nmi_v3) {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}

		info.nmi_procp = curproc;
		info.nmi_cred = VTONFS(vp)->n_rcred;
		error = nfs_request(vp, NFSPROC_READ, &info);
		if (info.nmi_v3)
			nfsm_postop_attr(vp, attrflag);
		if (error) {
			m_freem(info.nmi_mrep);
			goto nfsmout;
		}

		if (info.nmi_v3) {
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else {
			nfsm_loadattr(vp, NULL);
		}

		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(info.nmi_mrep);
		tsiz -= retlen;
		if (info.nmi_v3) {
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
nfs_writerpc(struct vnode *vp, struct uio *uiop, int *iomode, int *must_commit)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1, backup;
	caddr_t cp2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
	int committed = NFSV3WRITE_FILESYNC;

	info.nmi_v3 = NFS_ISV3(vp);

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	*must_commit = 0;
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff && !info.nmi_v3)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;
		info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3)
		    + 5 * NFSX_UNSIGNED + nfsm_rndup(len));
		nfsm_fhtom(&info, vp, info.nmi_v3);
		if (info.nmi_v3) {
			tl = nfsm_build(&info.nmi_mb, 5 * NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else {
			u_int32_t x;

			tl = nfsm_build(&info.nmi_mb, 4 * NFSX_UNSIGNED);
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;	/* "begin offset" */
			*tl++ = x;	/* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;	/* total to this offset */
			*tl = x;	/* size of this write */

		}
		nfsm_uiotombuf(&info.nmi_mb, uiop, len);

		info.nmi_procp = curproc;
		info.nmi_cred = VTONFS(vp)->n_wcred;
		error = nfs_request(vp, NFSPROC_WRITE, &info);
		if (info.nmi_v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_wcc_data(vp, wccflag);
		}

		if (error) {
			m_freem(info.nmi_mrep);
			goto nfsmout;
		}

		if (info.nmi_v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED
				+ NFSX_V3WRITEVERF);
			rlen = fxdr_unsigned(int, *tl++);
			if (rlen == 0) {
				error = NFSERR_IO;
				break;
			} else if (rlen < len) {
				backup = len - rlen;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base -
				    backup;
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
				bcopy(tl, nmp->nm_verf,
				    NFSX_V3WRITEVERF);
				nmp->nm_flag |= NFSMNT_HASWRITEVERF;
			} else if (bcmp(tl,
			    nmp->nm_verf, NFSX_V3WRITEVERF)) {
				*must_commit = 1;
				bcopy(tl, nmp->nm_verf,
				    NFSX_V3WRITEVERF);
			}
		} else {
			nfsm_loadattr(vp, NULL);
		}
		if (wccflag)
		    VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime;
		m_freem(info.nmi_mrep);
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
nfs_mknodrpc(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct vattr *vap)
{
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	struct vnode *newvp = NULL;
	struct nfsnode *np = NULL;
	char *cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	u_int32_t rdev;

	info.nmi_v3 = NFS_ISV3(dvp);

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
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    4 * NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen) +
	    NFSX_SATTR(info.nmi_v3));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);

	if (info.nmi_v3) {
		tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
		*tl++ = vtonfsv3_type(vap->va_type);
		nfsm_v3attrbuild(&info.nmi_mb, vap, 0);
		if (vap->va_type == VCHR || vap->va_type == VBLK) {
			tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(major(vap->va_rdev));
			*tl = txdr_unsigned(minor(vap->va_rdev));
		}
	} else {
		sp = nfsm_build(&info.nmi_mb, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = rdev;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}

	KASSERT(cnp->cn_proc == curproc);
	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp, NFSPROC_MKNOD, &info);
	if (!error) {
		nfsm_mtofh(dvp, newvp, info.nmi_v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = NULL;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (info.nmi_v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(info.nmi_mrep);

nfsmout: 
	if (error) {
		if (newvp)
			vrele(newvp);
	} else {
		if (cnp->cn_flags & MAKEENTRY)
			nfs_cache_enter(dvp, newvp, cnp);
		*vpp = newvp;
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	vrele(dvp);
	return (error);
}

/*
 * nfs mknod vop
 * just call nfs_mknodrpc() to do the work.
 */
int
nfs_mknod(void *v)
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

int
nfs_create(void *v)
{
	struct vop_create_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	struct nfsnode *np = NULL;
	struct vnode *newvp = NULL;
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0, fmode = 0;

	info.nmi_v3 = NFS_ISV3(dvp);

	/*
	 * Oops, not for me..
	 */
	if (vap->va_type == VSOCK)
		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));

	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= O_EXCL;

again:
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    2 * NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen) +
	    NFSX_SATTR(info.nmi_v3));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (info.nmi_v3) {
		tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
		if (fmode & O_EXCL) {
			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
			tl = nfsm_build(&info.nmi_mb, NFSX_V3CREATEVERF);
			arc4random_buf(tl, sizeof(*tl) * 2);
		} else {
			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
			nfsm_v3attrbuild(&info.nmi_mb, vap, 0);
		}
	} else {
		sp = nfsm_build(&info.nmi_mb, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = 0;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}

	KASSERT(cnp->cn_proc == curproc);
	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp, NFSPROC_CREATE, &info);
	if (!error) {
		nfsm_mtofh(dvp, newvp, info.nmi_v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = NULL;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (info.nmi_v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(info.nmi_mrep);

nfsmout: 
	if (error) {
		if (info.nmi_v3 && (fmode & O_EXCL) && error == NFSERR_NOTSUPP) {
			fmode &= ~O_EXCL;
			goto again;
		}
		if (newvp)
			vrele(newvp);
	} else if (info.nmi_v3 && (fmode & O_EXCL))
		error = nfs_setattrrpc(newvp, vap, cnp->cn_cred, cnp->cn_proc);
	if (!error) {
		if (cnp->cn_flags & MAKEENTRY)
			nfs_cache_enter(dvp, newvp, cnp);
		*ap->a_vpp = newvp;
	}
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
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
nfs_remove(void *v)
{
	struct vop_remove_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

#ifdef DIAGNOSTIC
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
		error = nfs_vinvalbuf(vp, 0, cnp->cn_cred, cnp->cn_proc);
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
	NFS_INVALIDATE_ATTRCACHE(np);
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
nfs_removeit(struct sillyrename *sp)
{
	/*
	 * Make sure that the directory vnode is still valid.
	 * XXX we should lock sp->s_dvp here.
	 *
	 * NFS can potentially try to nuke a silly *after* the directory
	 * has already been pushed out on a forced unmount. Since the silly
	 * is going to go away anyway, this is fine.
	 */
	if (sp->s_dvp->v_type == VBAD)
		return (0);
	return (nfs_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		NULL));
}

/*
 * Nfs remove rpc, called from nfs_remove() and nfs_removeit().
 */
int
nfs_removerpc(struct vnode *dvp, char *name, int namelen, struct ucred *cred,
    struct proc *proc)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;

	info.nmi_v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	     NFSX_UNSIGNED + nfsm_rndup(namelen));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);

	info.nmi_procp = proc;
	info.nmi_cred = cred;
	error = nfs_request(dvp, NFSPROC_REMOVE, &info);
	if (info.nmi_v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(info.nmi_mrep);

nfsmout: 
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	return (error);
}

/*
 * nfs file rename call
 */
int
nfs_rename(void *v)
{
	struct vop_rename_args	*ap = v;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

#ifdef DIAGNOSTIC
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
nfs_renameit(struct vnode *sdvp, struct componentname *scnp,
    struct sillyrename *sp)
{
	return (nfs_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen,
		sdvp, sp->s_name, sp->s_namlen, scnp->cn_cred, curproc));
}

/*
 * Do an nfs rename rpc. Called from nfs_rename() and nfs_renameit().
 */
int
nfs_renamerpc(struct vnode *fdvp, char *fnameptr, int fnamelen,
    struct vnode *tdvp, char *tnameptr, int tnamelen, struct ucred *cred,
    struct proc *proc)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, fwccflag = NFSV3_WCCRATTR, twccflag = NFSV3_WCCRATTR;

	info.nmi_v3 = NFS_ISV3(fdvp);

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead((NFSX_FH(info.nmi_v3) +
	    NFSX_UNSIGNED) * 2 + nfsm_rndup(fnamelen) + nfsm_rndup(tnamelen));
	nfsm_fhtom(&info, fdvp, info.nmi_v3);
	nfsm_strtom(fnameptr, fnamelen, NFS_MAXNAMLEN);
	nfsm_fhtom(&info, tdvp, info.nmi_v3);
	nfsm_strtom(tnameptr, tnamelen, NFS_MAXNAMLEN);

	info.nmi_procp = proc;
	info.nmi_cred = cred;
	error = nfs_request(fdvp, NFSPROC_RENAME, &info);
	if (info.nmi_v3) {
		nfsm_wcc_data(fdvp, fwccflag);
		nfsm_wcc_data(tdvp, twccflag);
	}
	m_freem(info.nmi_mrep);

nfsmout: 
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	if (!fwccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(fdvp));
	if (!twccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(tdvp));
	return (error);
}

/*
 * nfs hard link create call
 */
int
nfs_link(void *v)
{
	struct vop_link_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, attrflag = 0;

	info.nmi_v3 = NFS_ISV3(vp);

	if (dvp->v_mount != vp->v_mount) {
		pool_put(&namei_pool, cnp->cn_pnbuf);
		vput(dvp);
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, cnp->cn_cred, MNT_WAIT, cnp->cn_proc);

	nfsstats.rpccnt[NFSPROC_LINK]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(2 * NFSX_FH(info.nmi_v3) +
	    NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(&info, vp, info.nmi_v3);
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);

	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(vp, NFSPROC_LINK, &info);
	if (info.nmi_v3) {
		nfsm_postop_attr(vp, attrflag);
		nfsm_wcc_data(dvp, wccflag);
	}
	m_freem(info.nmi_mrep);
nfsmout: 
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!attrflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(vp));
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));

	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	vput(dvp);
	return (error);
}

/*
 * nfs symbolic link create call
 */
int
nfs_symlink(void *v)
{
	struct vop_symlink_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int slen, error = 0, wccflag = NFSV3_WCCRATTR, gotvp;
	struct vnode *newvp = NULL;

	info.nmi_v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    2 * NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen) + nfsm_rndup(slen) +
	    NFSX_SATTR(info.nmi_v3));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (info.nmi_v3)
		nfsm_v3attrbuild(&info.nmi_mb, vap, 0);
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
	if (!info.nmi_v3) {
		sp = nfsm_build(&info.nmi_mb, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VLNK, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}

	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp, NFSPROC_SYMLINK, &info);
	if (info.nmi_v3) {
		if (!error)
			nfsm_mtofh(dvp, newvp, info.nmi_v3, gotvp);
		nfsm_wcc_data(dvp, wccflag);
	}
	m_freem(info.nmi_mrep);

nfsmout: 
	if (newvp)
		vrele(newvp);
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	VN_KNOTE(dvp, NOTE_WRITE);
	vrele(dvp);
	return (error);
}

/*
 * nfs make dir call
 */
int
nfs_mkdir(void *v)
{
	struct vop_mkdir_args *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int len;
	struct nfsnode *np = NULL;
	struct vnode *newvp = NULL;
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int gotvp = 0;

	info.nmi_v3 = NFS_ISV3(dvp);

	len = cnp->cn_namelen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    NFSX_UNSIGNED + nfsm_rndup(len) + NFSX_SATTR(info.nmi_v3));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);

	if (info.nmi_v3) {
		nfsm_v3attrbuild(&info.nmi_mb, vap, 0);
	} else {
		sp = nfsm_build(&info.nmi_mb, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VDIR, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}

	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp, NFSPROC_MKDIR, &info);
	if (!error)
		nfsm_mtofh(dvp, newvp, info.nmi_v3, gotvp);
	if (info.nmi_v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(info.nmi_mrep);

nfsmout: 
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));

	if (error == 0 && newvp == NULL) {
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
		if (cnp->cn_flags & MAKEENTRY)
			nfs_cache_enter(dvp, newvp, cnp);
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
nfs_rmdir(void *v)
{
	struct vop_rmdir_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;

	info.nmi_v3 = NFS_ISV3(dvp);

	if (dvp == vp) {
		vrele(dvp);
		vrele(dvp);
		pool_put(&namei_pool, cnp->cn_pnbuf);
		return (EINVAL);
	}

	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) +
	    NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);

	info.nmi_procp = cnp->cn_proc;
	info.nmi_cred = cnp->cn_cred;
	error = nfs_request(dvp,  NFSPROC_RMDIR, &info);
	if (info.nmi_v3)
		nfsm_wcc_data(dvp, wccflag);
	m_freem(info.nmi_mrep);

nfsmout: 
	pool_put(&namei_pool, cnp->cn_pnbuf);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));

	VN_KNOTE(dvp, NOTE_WRITE|NOTE_LINK);
	VN_KNOTE(vp, NOTE_DELETE);

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

#define	NFS_DIRHDSIZ	(sizeof (struct nfs_dirent) - (MAXNAMLEN + 1))
#define NFS_DIRENT_OVERHEAD  offsetof(struct nfs_dirent, dirent)

/*
 * nfs readdir call
 */
int
nfs_readdir(void *v)
{
	struct vop_readdir_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct uio *uio = ap->a_uio;
	int tresid, error = 0;
	struct vattr vattr;
	int cnt;
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
		    timespeccmp(&np->n_mtime, &vattr.va_mtime, ==)) {
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

	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);

	cnt = 5;

	/* M_ZERO to avoid leaking kernel data in dirent padding */
	data = malloc(NFS_DIRBLKSIZ, M_TEMP, M_WAITOK|M_ZERO);
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
			    &eof, p);
			if (error == NFSERR_NOTSUPP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, &readdir_uio, cred, &eof);

		if (error == NFSERR_BAD_COOKIE)
			error = EINVAL;

		while (error == 0 &&
		    ndp < (struct nfs_dirent *)readdir_iovec.iov_base) {
			struct dirent *dp = &ndp->dirent;
			int reclen = dp->d_reclen;

			dp->d_reclen -= NFS_DIRENT_OVERHEAD;
			dp->d_off = fxdr_hyper(&ndp->cookie[0]);

			if (uio->uio_resid < dp->d_reclen) {
				eof = 0;
				done = 1;
				break;
			}

			if ((error = uiomove(dp, dp->d_reclen, uio)))
				break;

			newoff = fxdr_hyper(&ndp->cookie[0]);

			ndp = (struct nfs_dirent *)((u_int8_t *)ndp + reclen);
		}
	} while (!error && !done && !eof && cnt--);

	free(data, M_TEMP, NFS_DIRBLKSIZ);
	data = NULL;

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
nfs_readdirrpc(struct vnode *vp, struct uio *uiop, struct ucred *cred,
    int *end_of_directory)
{
	int len, left;
	struct nfs_dirent *ndp = NULL;
	struct dirent *dp = NULL;
	struct nfsm_info	info;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1;
	caddr_t cp2;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag;

	info.nmi_v3 = NFS_ISV3(vp);

#ifdef DIAGNOSTIC
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
		info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3)
		    + NFSX_READDIR(info.nmi_v3));
		nfsm_fhtom(&info, vp, info.nmi_v3);
		if (info.nmi_v3) {
			tl = nfsm_build(&info.nmi_mb, 5 * NFSX_UNSIGNED);
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
			tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
			*tl++ = cookie.nfsuquad[1];
		}
		*tl = txdr_unsigned(nmp->nm_readdirsize);

		info.nmi_procp = uiop->uio_procp;
		info.nmi_cred = cred;
		error = nfs_request(vp, NFSPROC_READDIR, &info);
		if (info.nmi_v3)
			nfsm_postop_attr(vp, attrflag);

		if (error) {
			m_freem(info.nmi_mrep);
			goto nfsmout;
		}

		if (info.nmi_v3) {
			nfsm_dissect(tl, u_int32_t *,
			    2 * NFSX_UNSIGNED);
			dnp->n_cookieverf.nfsuquad[0] = *tl++;
			dnp->n_cookieverf.nfsuquad[1] = *tl;
		}

		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to dirent form */
		while (more_dirs && bigenough) {
			if (info.nmi_v3) {
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
				m_freem(info.nmi_mrep);
				goto nfsmout;
			}
			tlen = DIRENT_RECSIZE(len) + NFS_DIRENT_OVERHEAD;
			left = NFS_READDIRBLKSIZ - blksiz;
			if (tlen > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base += left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if (tlen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				ndp = (struct nfs_dirent *)
				    uiop->uio_iov->iov_base;
				dp = &ndp->dirent;
				dp->d_fileno = fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen;
				dp->d_type = DT_UNKNOWN;
				blksiz += tlen;
				if (blksiz == NFS_READDIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= NFS_DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base +
				    NFS_DIRHDSIZ;
				uiop->uio_iov->iov_len -= NFS_DIRHDSIZ;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= NFS_DIRHDSIZ + len;
				*cp = '\0';	/* null terminate */
				uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			if (info.nmi_v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
			} else {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
			}
			if (bigenough) {
				if (info.nmi_v3) {
					ndp->cookie[0] = cookie.nfsuquad[0] =
					    *tl++;
				} else
					ndp->cookie[0] = 0;

				ndp->cookie[1] = cookie.nfsuquad[1] = *tl++;
			} else if (info.nmi_v3)
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
		m_freem(info.nmi_mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_READDIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_READDIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    left;
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
    int *end_of_directory, struct proc *p)
{
	int len, left;
	struct nfs_dirent *ndirp = NULL;
	struct dirent *dp = NULL;
	struct nfsm_info	info;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1;
	struct vnode *newvp;
	caddr_t cp2, dpossav1, dpossav2;
	struct mbuf *mdsav1, *mdsav2;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	nfsuint64 cookie;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	nfsfh_t *fhp;
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
	int attrflag, fhsize;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 ||
		(uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
		panic("nfs readdirplusrpc bad uio");
#endif
	NDINIT(ndp, 0, 0, UIO_SYSSPACE, NULL, p);
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
		info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(1) + 6 * NFSX_UNSIGNED);
		nfsm_fhtom(&info, vp, 1);
		tl = nfsm_build(&info.nmi_mb, 6 * NFSX_UNSIGNED);
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

		info.nmi_procp = uiop->uio_procp;
		info.nmi_cred = cred;
		error = nfs_request(vp, NFSPROC_READDIRPLUS, &info);
		nfsm_postop_attr(vp, attrflag);
		if (error) {
			m_freem(info.nmi_mrep);
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
				m_freem(info.nmi_mrep);
				goto nfsmout;
			}
			tlen = DIRENT_RECSIZE(len) + NFS_DIRENT_OVERHEAD;
			left = NFS_READDIRBLKSIZ - blksiz;
			if (tlen > left) {
				dp->d_reclen += left;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base + left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				blksiz = 0;
			}
			if (tlen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				ndirp = (struct nfs_dirent *)
				    uiop->uio_iov->iov_base;
				dp = &ndirp->dirent;
				dp->d_fileno = fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen;
				dp->d_type = DT_UNKNOWN;
				blksiz += tlen;
				if (blksiz == NFS_READDIRBLKSIZ)
					blksiz = 0;
				uiop->uio_resid -= NFS_DIRHDSIZ;
				uiop->uio_iov->iov_base =
				    (char *)uiop->uio_iov->iov_base +
				    NFS_DIRHDSIZ;
				uiop->uio_iov->iov_len -= NFS_DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= NFS_DIRHDSIZ + len;
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
				dpossav1 = info.nmi_dpos;
				mdsav1 = info.nmi_md;
				nfsm_adv(NFSX_V3FATTR);
				nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
				doit = fxdr_unsigned(int, *tl);
				if (doit) {
					nfsm_getfh(fhp, fhsize, 1);
					if (NFS_CMPFH(dnp, fhp, fhsize)) {
						vref(vp);
						newvp = vp;
						np = dnp;
					} else {
						error = nfs_nget(vp->v_mount,
						    fhp, fhsize, &np);
						if (error)
							doit = 0;
						else
							newvp = NFSTOV(np);
					}
				}
				if (doit && bigenough) {
					dpossav2 = info.nmi_dpos;
					info.nmi_dpos = dpossav1;
					mdsav2 = info.nmi_md;
					info.nmi_md = mdsav1;
					nfsm_loadattr(newvp, NULL);
					info.nmi_dpos = dpossav2;
					info.nmi_md = mdsav2;
					dp->d_type = IFTODT(
						VTTOIF(np->n_vattr.va_type));
					if (cnp->cn_namelen <=
					    NAMECACHE_MAXLEN) {
						ndp->ni_vp = newvp;
						cache_purge(ndp->ni_dvp);
						nfs_cache_enter(ndp->ni_dvp,
						    ndp->ni_vp, cnp);
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
		m_freem(info.nmi_mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_READDIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_READDIRBLKSIZ - blksiz;
		dp->d_reclen += left;
		uiop->uio_iov->iov_base = (char *)uiop->uio_iov->iov_base +
		    left;
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
nfs_sillyrename(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;

	cache_purge(dvp);
	np = VTONFS(vp);
	sp = malloc(sizeof(*sp), M_NFSREQ, M_WAITOK);
	sp->s_cred = crdup(cnp->cn_cred);
	sp->s_dvp = dvp;
	vref(dvp);

	if (vp->v_type == VDIR) {
#ifdef DIAGNOSTIC
		printf("nfs: sillyrename dir\n");
#endif
		error = EINVAL;
		goto bad;
	}

	/* Try lookitups until we get one that isn't there */
	while (1) {
		/* Fudge together a funny name */
		u_int32_t rnd[2];

		arc4random_buf(&rnd, sizeof rnd);
		sp->s_namlen = snprintf(sp->s_name, sizeof sp->s_name,
		    ".nfs%08X%08X", rnd[0], rnd[1]);
		if (sp->s_namlen > sizeof sp->s_name)
			sp->s_namlen = strlen(sp->s_name);

		if (nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		    cnp->cn_proc, NULL))
			break;
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
	free(sp, M_NFSREQ, sizeof(*sp));
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
nfs_lookitup(struct vnode *dvp, char *name, int len, struct ucred *cred,
    struct proc *procp, struct nfsnode **npp)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	struct vnode *newvp = NULL;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	caddr_t cp2;
	int error = 0, fhlen, attrflag = 0;
	nfsfh_t *nfhp;

	info.nmi_v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(info.nmi_v3) + NFSX_UNSIGNED +
	    nfsm_rndup(len));
	nfsm_fhtom(&info, dvp, info.nmi_v3);
	nfsm_strtom(name, len, NFS_MAXNAMLEN);

	info.nmi_procp = procp;
	info.nmi_cred = cred;
	error = nfs_request(dvp, NFSPROC_LOOKUP, &info);
	if (error && !info.nmi_v3) {
		m_freem(info.nmi_mrep);
		goto nfsmout;
	}

	if (npp && !error) {
		nfsm_getfh(nfhp, fhlen, info.nmi_v3);
		if (*npp) {
			np = *npp;
			np->n_fhp = &np->n_fh;
			bcopy(nfhp, np->n_fhp, fhlen);
			np->n_fhsize = fhlen;
			newvp = NFSTOV(np);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
			vref(dvp);
			newvp = dvp;
			np = dnp;
		} else {
			error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np);
			if (error) {
				m_freem(info.nmi_mrep);
				return (error);
			}
			newvp = NFSTOV(np);
		}
		if (info.nmi_v3) {
			nfsm_postop_attr(newvp, attrflag);
			if (!attrflag && *npp == NULL) {
				m_freem(info.nmi_mrep);
				vrele(newvp);
				return (ENOENT);
			}
		} else
			nfsm_loadattr(newvp, NULL);
	}
	m_freem(info.nmi_mrep);
nfsmout:
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
nfs_commit(struct vnode *vp, u_quad_t offset, int cnt, struct proc *procp)
{
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;

	if ((nmp->nm_flag & NFSMNT_HASWRITEVERF) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;
	info.nmi_mb = info.nmi_mreq = nfsm_reqhead(NFSX_FH(1));
	nfsm_fhtom(&info, vp, 1);

	tl = nfsm_build(&info.nmi_mb, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);

	info.nmi_procp = procp;
	info.nmi_cred = VTONFS(vp)->n_wcred;
	error = nfs_request(vp, NFSPROC_COMMIT, &info);
	nfsm_wcc_data(vp, wccflag);

	if (!error) {
		nfsm_dissect(tl, u_int32_t *, NFSX_V3WRITEVERF);
		if (bcmp(nmp->nm_verf, tl,
			NFSX_V3WRITEVERF)) {
			bcopy(tl, nmp->nm_verf,
				NFSX_V3WRITEVERF);
			error = NFSERR_STALEWRITEVERF;
		}
	}
	m_freem(info.nmi_mrep);

nfsmout:
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
nfs_bmap(void *v)
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
nfs_strategy(void *v)
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
	if ((bp->b_flags & B_ASYNC) == 0 || nfs_asyncio(bp, 0))
		error = nfs_doio(bp, p);
	return (error);
}

/*
 * fsync vnode op. Just call nfs_flush() with commit == 1.
 */
int
nfs_fsync(void *v)
{
	struct vop_fsync_args *ap = v;

	return (nfs_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p, 1));
}

/*
 * Flush all the blocks associated with a vnode.
 *	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
int
nfs_flush(struct vnode *vp, struct ucred *cred, int waitfor, struct proc *p,
    int commit)
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
			if (bvecpos >= NFS_COMMITBVECSIZ)
				break;
			if ((bp->b_flags & (B_BUSY | B_DELWRI | B_NEEDCOMMIT))
			    != (B_DELWRI | B_NEEDCOMMIT)) {
				nbp = LIST_NEXT(bp, b_vnbufs);
				continue;
			}
			bremfree(bp);
			bp->b_flags |= B_WRITEINPROG;
			buf_acquire(bp);
			nbp = LIST_NEXT(bp, b_vnbufs);

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
		bcstats.pendingwrites++;
		bcstats.numwrites++;
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
			if (retv) {
				if (i == 0)
					bcstats.pendingwrites--;
				brelse(bp);
			} else {
				if (i > 0)
					bcstats.pendingwrites++;
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
			error = tsleep(bp, slpflag | (PRIBIO + 1),
				"nfsfsync", slptimeo);
			splx(s);
			if (error) {
				if (nfs_sigintr(nmp, NULL, p))
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
		if (passone || !commit) {
			bp->b_flags |= B_ASYNC;
		} else {
			bp->b_flags |= (B_ASYNC|B_WRITEINPROG|B_NEEDCOMMIT);
		}
		buf_acquire(bp);
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
			if (nfs_sigintr(nmp, NULL, p))
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
 * Fake it. For v3 we could ask the server, but such code
 * hasn't been written yet.
 */
/* ARGSUSED */
int
nfs_pathconf(void *v)
{
	struct vop_pathconf_args *ap = v;
	struct nfsmount *nmp = VFSTONFS(ap->a_vp->v_mount);
	int error = 0;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
	case _PC_ALLOC_SIZE_MIN:
		*ap->a_retval = NFS_FABLKSIZE;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		break;
	case _PC_REC_INCR_XFER_SIZE:
		*ap->a_retval = min(nmp->nm_rsize, nmp->nm_wsize);
		break;
	case _PC_REC_MAX_XFER_SIZE:
		*ap->a_retval = -1; /* means ``unlimited'' */
		break;
	case _PC_REC_MIN_XFER_SIZE:
		*ap->a_retval = min(nmp->nm_rsize, nmp->nm_wsize);
		break;
	case _PC_REC_XFER_ALIGN:
		*ap->a_retval = PAGE_SIZE;
		break;
	case _PC_SYMLINK_MAX:
		*ap->a_retval = MAXPATHLEN;
		break;
	case _PC_2_SYMLINKS:
		*ap->a_retval = 1;
		break;
	case _PC_TIMESTAMP_RESOLUTION:
		*ap->a_retval = NFS_ISV3(ap->a_vp) ? 1 : 1000;
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
int
nfs_advlock(void *v)
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
nfs_print(void *v)
{
	struct vop_print_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %lld fsid 0x%lx",
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
nfs_bwrite(void *v)
{
	struct vop_bwrite_args *ap = v;

	return (nfs_writebp(ap->a_bp, 1));
}

/*
 * This is a clone of vop_generic_bwrite(), except that B_WRITEINPROG isn't set unless
 * the force flag is one and it also handles the B_NEEDCOMMIT flag.
 */
int
nfs_writebp(struct buf *bp, int force)
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
		++p->p_ru.ru_oublock;

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
		 * If it's already been committed by somebody else,
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
			bcstats.pendingwrites++;
			bcstats.numwrites++;
			retv = nfs_commit(bp->b_vp, off, cnt, curproc);
			bp->b_flags &= ~B_WRITEINPROG;

			if (retv == 0) {
				if (pushedrange)
					nfs_merge_commit_ranges(vp);
				else
					nfs_add_committed_range(vp, bp);
			} else
				bcstats.pendingwrites--;
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
		buf_flip_dma(bp);
		if (force)
			bp->b_flags |= B_WRITEINPROG;
		VOP_STRATEGY(bp);
	}

	if( (oldflags & B_ASYNC) == 0) {
		int rtval;

		bp->b_flags |= B_RAW;
		rtval = biowait(bp);
		if (!(oldflags & B_DELWRI) && p) {
			++p->p_ru.ru_oublock;
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
nfsspec_access(void *v)
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

	return (vaccess(vp->v_type, va.va_mode, va.va_uid, va.va_gid,
	    ap->a_mode, ap->a_cred));
}

int
nfs_poll(void *v)
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
nfsspec_read(void *v)
{
	struct vop_read_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (spec_read(ap));
}

/*
 * Write wrapper for special devices.
 */
int
nfsspec_write(void *v)
{
	struct vop_write_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (spec_write(ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the nfsnode then do device close.
 */
int
nfsspec_close(void *v)
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
	return (spec_close(ap));
}

#ifdef FIFO
/*
 * Read wrapper for fifos.
 */
int
nfsfifo_read(void *v)
{
	struct vop_read_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (fifo_read(ap));
}

/*
 * Write wrapper for fifos.
 */
int
nfsfifo_write(void *v)
{
	struct vop_write_args *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (fifo_write(ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
int
nfsfifo_close(void *v)
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

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
	return (fifo_close(ap));
}

int
nfsfifo_reclaim(void *v)
{
	fifo_reclaim(v);
	return (nfs_reclaim(v));
}
#endif /* ! FIFO */
