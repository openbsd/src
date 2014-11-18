/*	$OpenBSD: nfs_serv.c,v 1.100 2014/11/18 02:22:33 tedu Exp $	*/
/*     $NetBSD: nfs_serv.c,v 1.34 1997/05/12 23:37:12 fvdl Exp $       */

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
 *	@(#)nfs_serv.c	8.7 (Berkeley) 5/14/95
 */

/*
 * nfs version 2 and 3 server calls to vnode ops
 * - these routines generally have 3 phases
 *   1 - break down and validate rpc request in mbuf list
 *   2 - do the vnode ops for the request
 *       (surprisingly ?? many are very similar to syscalls in vfs_syscalls.c)
 *   3 - build the rpc reply in an mbuf list
 *   nb:
 *	- do not mix the phases, since the nfsm_?? macros can return failures
 *	  on a bad rpc or similar and do not do any vrele() or vput()'s
 *
 *      - the nfsm_reply() macro generates an nfs rpc reply with the nfs
 *	error number iff error != 0 whereas
 *	returning an error from the server function implies a fatal error
 *	such as a badly constructed rpc request that should be dropped without
 *	a reply.
 *	For Version 3, nfsm_reply() does not return for the error case, since
 *	most version 3 rpcs return more than the status for error cases.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/unistd.h>

#include <ufs/ufs/dir.h>

#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_var.h>

/* Global vars */
extern u_int32_t nfs_xdrneg1;
extern u_int32_t nfs_false, nfs_true;
extern enum vtype nv3tov_type[8];
extern struct nfsstats nfsstats;
extern nfstype nfsv2_type[9];
extern nfstype nfsv3_type[9];

/*
 * nfs v3 access service
 */
int
nfsrv3_access(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct nfsm_info	info;
	struct ucred *cred = &nfsd->nd_cr;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, getret;
	char *cp2;
	struct vattr va;
	u_long testmode, nfsmode;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, 1, NULL, &info);
		error = 0;
		goto nfsmout;
	}
	nfsmode = fxdr_unsigned(u_int32_t, *tl);
	if ((nfsmode & NFSV3ACCESS_READ) &&
		nfsrv_access(vp, VREAD, cred, rdonly, procp, 0))
		nfsmode &= ~NFSV3ACCESS_READ;
	if (vp->v_type == VDIR)
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
			NFSV3ACCESS_DELETE);
	else
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VWRITE, cred, rdonly, procp, 0))
		nfsmode &= ~testmode;
	if (vp->v_type == VDIR)
		testmode = NFSV3ACCESS_LOOKUP;
	else
		testmode = NFSV3ACCESS_EXECUTE;
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VEXEC, cred, rdonly, procp, 0))
		nfsmode &= ~testmode;
	getret = VOP_GETATTR(vp, &va, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_POSTOPATTR(1) + NFSX_UNSIGNED);
	nfsm_srvpostop_attr(nfsd, getret, &va, &info);
	tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
	*tl = txdr_unsigned(nfsmode);
nfsmout:
	return(error);
}

/*
 * nfs getattr service
 */
int
nfsrv_getattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct nfsm_info	info;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly;
	char *cp2;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(0);
		error = 0;
		goto nfsmout;
	}
	error = VOP_GETATTR(vp, &va, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	if (error) {
		error = 0;
		goto nfsmout;
	}
	fp = nfsm_build(&info.nmi_mb, NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	nfsm_srvfattr(nfsd, &va, fp);
nfsmout:
	return(error);
}

/*
 * nfs setattr service
 */
int
nfsrv_setattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct nfsm_info	info;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, preat;
	struct nfsv2_sattr *sp;
	struct nfs_fattr *fp;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, preat_ret = 1, postat_ret = 1;
	int gcheck = 0;
	char *cp2;
	struct timespec guard;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	VATTR_NULL(&va);
	if (info.nmi_v3) {
		va.va_vaflags |= VA_UTIMES_NULL;
		error = nfsm_srvsattr(&info.nmi_md, &va, info.nmi_mrep, &info.nmi_dpos);
		if (error)
			goto nfsmout;
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		gcheck = fxdr_unsigned(int, *tl);
		if (gcheck) {
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &guard);
		}
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		/*
		 * Nah nah nah nah na nah
		 * There is a bug in the Sun client that puts 0xffff in the mode
		 * field of sattr when it should put in 0xffffffff. The u_short
		 * doesn't sign extend.
		 * --> check the low order 2 bytes for 0xffff
		 */
		if ((fxdr_unsigned(int, sp->sa_mode) & 0xffff) != 0xffff)
			va.va_mode = nfstov_mode(sp->sa_mode);
		if (sp->sa_uid != nfs_xdrneg1)
			va.va_uid = fxdr_unsigned(uid_t, sp->sa_uid);
		if (sp->sa_gid != nfs_xdrneg1)
			va.va_gid = fxdr_unsigned(gid_t, sp->sa_gid);
		if (sp->sa_size != nfs_xdrneg1)
			va.va_size = fxdr_unsigned(u_quad_t, sp->sa_size);
		if (sp->sa_atime.nfsv2_sec != nfs_xdrneg1) {
#ifdef notyet
			fxdr_nfsv2time(&sp->sa_atime, &va.va_atime);
#else
			va.va_atime.tv_sec =
				fxdr_unsigned(u_int32_t,sp->sa_atime.nfsv2_sec);
			va.va_atime.tv_nsec = 0;
#endif
		}
		if (sp->sa_mtime.nfsv2_sec != nfs_xdrneg1)
			fxdr_nfsv2time(&sp->sa_mtime, &va.va_mtime);

	}

	/*
	 * Now that we have all the fields, lets do it.
	 */
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc(nfsd, preat_ret, &preat, postat_ret, &va, &info);
		error = 0;
		goto nfsmout;
	}
	if (info.nmi_v3) {
		error = preat_ret = VOP_GETATTR(vp, &preat, cred, procp);
		if (!error && gcheck &&
			(preat.va_ctime.tv_sec != guard.tv_sec ||
			 preat.va_ctime.tv_nsec != guard.tv_nsec))
			error = NFSERR_NOT_SYNC;
		if (error) {
			vput(vp);
			nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
			nfsm_srvwcc(nfsd, preat_ret, &preat, postat_ret, &va,
			    &info);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * If the size is being changed write acces is required, otherwise
	 * just check for a read only file system.
	 */
	if (va.va_size == ((u_quad_t)((quad_t) -1))) {
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			error = EROFS;
			goto out;
		}
	} else {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		} else if ((error = nfsrv_access(vp, VWRITE, cred, rdonly,
			procp, 1)) != 0)
			goto out;
	}
	error = VOP_SETATTR(vp, &va, cred, procp);
	postat_ret = VOP_GETATTR(vp, &va, cred, procp);
	if (!error)
		error = postat_ret;
out:
	vput(vp);
	nfsm_reply(NFSX_WCCORFATTR(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvwcc(nfsd, preat_ret, &preat, postat_ret, &va,
		    &info);
		error = 0;
		goto nfsmout;
	} else {
		fp = nfsm_build(&info.nmi_mb, NFSX_V2FATTR);
		nfsm_srvfattr(nfsd, &va, fp);
	}
nfsmout:
	return(error);
}

/*
 * nfs lookup rpc
 */
int
nfsrv_lookup(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct nameidata nd;
	struct vnode *vp, *dirp;
	struct nfsm_info	info;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, len, dirattr_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct vattr va, dirattr;

	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_mreq = NULL;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md, &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirattr_ret = VOP_GETATTR(dirp, &dirattr, cred,
				procp);
		vrele(dirp);
	}
	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3));
		nfsm_srvpostop_attr(nfsd, dirattr_ret, &dirattr, &info);
		return (0);
	}
	vrele(nd.ni_startdir);
	pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	vp = nd.ni_vp;
	memset(fhp, 0, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fhp->fh_fid);
	if (!error)
		error = VOP_GETATTR(vp, &va, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_SRVFH(info.nmi_v3) + NFSX_POSTOPORFATTR(info.nmi_v3)
	    + NFSX_POSTOPATTR(info.nmi_v3));
	if (error) {
		nfsm_srvpostop_attr(nfsd, dirattr_ret, &dirattr, &info);
		error = 0;
		goto nfsmout;
	}
	nfsm_srvfhtom(&info.nmi_mb, fhp, info.nmi_v3);
	if (v3) {
		nfsm_srvpostop_attr(nfsd, 0, &va, &info);
		nfsm_srvpostop_attr(nfsd, dirattr_ret, &dirattr, &info);
	} else {
		fp = nfsm_build(&info.nmi_mb, NFSX_V2FATTR);
		nfsm_srvfattr(nfsd, &va, fp);
	}
nfsmout:
	return(error);
}

/*
 * nfs readlink service
 */
int
nfsrv_readlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec iov;
	struct mbuf *mp = NULL;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, tlen, len = 0, getret;
	char *cp2;
	struct vnode *vp;
	struct vattr attr;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio uio;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	memset(&uio, 0, sizeof(uio));

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, 1, NULL, &info);
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type != VLNK) {
		if (info.nmi_v3)
			error = EINVAL;
		else
			error = ENXIO;
		goto out;
	}

	MGET(mp, M_WAIT, MT_DATA);
	MCLGET(mp, M_WAIT);		/* MLEN < NFS_MAXPATHLEN < MCLBYTES */
	mp->m_len = NFS_MAXPATHLEN;
	len = NFS_MAXPATHLEN;
	iov.iov_base = mtod(mp, caddr_t);
	iov.iov_len = mp->m_len;

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = NFS_MAXPATHLEN;
	uio.uio_rw = UIO_READ;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_procp = NULL;

	error = VOP_READLINK(vp, &uio, cred);
out:
	getret = VOP_GETATTR(vp, &attr, cred, procp);
	vput(vp);
	if (error && mp)
		m_freem(mp);
	nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) + NFSX_UNSIGNED);
	if (info.nmi_v3) {
		nfsm_srvpostop_attr(nfsd, getret, &attr, &info);
		if (error) {
			error = 0;
			goto nfsmout;
		}
	}
	if (uio.uio_resid > 0) {
		len -= uio.uio_resid;
		tlen = nfsm_rndup(len);
		nfsm_adj(mp, NFS_MAXPATHLEN-tlen, tlen-len);
	}
	tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	info.nmi_mb->m_next = mp;

nfsmout:
	return (error);
}

/*
 * nfs read service
 */
int
nfsrv_read(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct mbuf *m;
	struct nfs_fattr *fp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int i, reqlen;
	int error = 0, rdonly, cnt, len, left, siz, tlen, getret = 1;
	char *cp2;
	struct mbuf *m2;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	struct vattr va;
	off_t off;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (info.nmi_v3) {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *tl);
	}

	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	reqlen = fxdr_unsigned(int32_t, *tl);
	if (reqlen > (NFS_SRVMAXDATA(nfsd)) || reqlen <= 0) {
		error = EBADRPC;
		nfsm_reply(0);
	}

	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error)
		goto bad;

	if (vp->v_type != VREG) {
		if (info.nmi_v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
	    if ((error = nfsrv_access(vp, VREAD, cred, rdonly, procp, 1)) != 0)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, procp, 1);
	}
	getret = VOP_GETATTR(vp, &va, cred, procp);
	if (!error)
		error = getret;
	if (error)
		goto vbad;

	if (off >= va.va_size)
		cnt = 0;
	else if ((off + reqlen) > va.va_size)
		cnt = va.va_size - off;
	else
		cnt = reqlen;
	nfsm_reply(NFSX_POSTOPORFATTR(info.nmi_v3) + 3 * NFSX_UNSIGNED+nfsm_rndup(cnt));
	if (info.nmi_v3) {
		tl = nfsm_build(&info.nmi_mb, NFSX_V3FATTR + 4 * NFSX_UNSIGNED);
		*tl++ = nfs_true;
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V3FATTR / sizeof (u_int32_t));
	} else {
		tl = nfsm_build(&info.nmi_mb, NFSX_V2FATTR + NFSX_UNSIGNED);
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V2FATTR / sizeof (u_int32_t));
	}
	len = left = nfsm_rndup (cnt);
	if (cnt > 0) {
		struct iovec *iv, *iv2;
		size_t ivlen;
		/*
		 * Generate the mbuf list with the uio_iov ref. to it.
		 */
		i = 0;
		m = m2 = info.nmi_mb;
		while (left > 0) {
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				left -= siz;
				i++;
			}
			if (left > 0) {
				MGET(m, M_WAIT, MT_DATA);
				if (left >= MINCLSIZE)
					MCLGET(m, M_WAIT);
				m->m_len = 0;
				m2->m_next = m;
				m2 = m;
			}
		}
		iv = mallocarray(i, sizeof(*iv), M_TEMP, M_WAITOK);
		ivlen = i * sizeof(*iv);
		uiop->uio_iov = iv2 = iv;
		m = info.nmi_mb;
		left = len;
		i = 0;
		while (left > 0) {
			if (m == NULL)
				panic("nfsrv_read iov");
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				iv->iov_base = mtod(m, caddr_t) + m->m_len;
				iv->iov_len = siz;
				m->m_len += siz;
				left -= siz;
				iv++;
				i++;
			}
			m = m->m_next;
		}
		uiop->uio_iovcnt = i;
		uiop->uio_offset = off;
		uiop->uio_resid = len;
		uiop->uio_rw = UIO_READ;
		uiop->uio_segflg = UIO_SYSSPACE;
		error = VOP_READ(vp, uiop, IO_NODELOCKED, cred);
		off = uiop->uio_offset;
		free(iv2, M_TEMP, ivlen);
		if (error || (getret = VOP_GETATTR(vp, &va, cred, procp)) != 0){
			if (!error)
				error = getret;
			m_freem(info.nmi_mreq);
			goto vbad;
		}
	} else
		uiop->uio_resid = 0;
	vput(vp);
	nfsm_srvfattr(nfsd, &va, fp);
	tlen = len - uiop->uio_resid;
	cnt = cnt < tlen ? cnt : tlen;
	tlen = nfsm_rndup (cnt);
	if (len != tlen || tlen != cnt)
		nfsm_adj(info.nmi_mb, len - tlen, tlen - cnt);
	if (info.nmi_v3) {
		*tl++ = txdr_unsigned(cnt);
		if (len < reqlen)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
	}
	*tl = txdr_unsigned(cnt);
nfsmout:
	return(error);

vbad:
	vput(vp);
bad:
	nfsm_reply(0);
	nfsm_srvpostop_attr(nfsd, getret, &va, &info);
	return (0);
}

/*
 * nfs write service
 */
int
nfsrv_write(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfsm_info	info;
	int i, cnt;
	struct mbuf *mp;
	struct nfs_fattr *fp;
	struct vattr va, forat;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, retlen, zeroing, adjust;
	int stable = NFSV3WRITE_FILESYNC;
	char *cp2;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	off_t off;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	if (info.nmi_mrep == NULL) {
		*mrq = NULL;
		return (0);
	}
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (info.nmi_v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 3;
		stable = fxdr_unsigned(int, *tl++);
	} else {
		nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
	}
	retlen = len = fxdr_unsigned(int32_t, *tl);
	cnt = i = 0;

	/*
	 * For NFS Version 2, it is not obvious what a write of zero length
	 * should do, but I might as well be consistent with Version 3,
	 * which is to return ok so long as there are no permission problems.
	 */
	if (len > 0) {
	    zeroing = 1;
	    mp = info.nmi_mrep;
	    while (mp) {
		if (mp == info.nmi_md) {
			zeroing = 0;
			adjust = info.nmi_dpos - mtod(mp, caddr_t);
			mp->m_len -= adjust;
			if (mp->m_len > 0 && adjust > 0)
				mp->m_data += adjust;
		}
		if (zeroing)
			mp->m_len = 0;
		else if (mp->m_len > 0) {
			i += mp->m_len;
			if (i > len) {
				mp->m_len -= (i - len);
				zeroing	= 1;
			}
			if (mp->m_len > 0)
				cnt++;
		}
		mp = mp->m_next;
	    }
	}
	if (len > NFS_MAXDATA || len < 0 || i < len) {
		error = EIO;
		goto bad;
	}
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error)
		goto bad;
	if (info.nmi_v3)
		forat_ret = VOP_GETATTR(vp, &forat, cred, procp);
	if (vp->v_type != VREG) {
		if (info.nmi_v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		goto vbad;
	}
	error = nfsrv_access(vp, VWRITE, cred, rdonly, procp, 1);
	if (error)
		goto vbad;

	if (len > 0) {
	    struct iovec *iv, *ivp;
	    size_t ivlen;

	    ivp = mallocarray(cnt, sizeof(*ivp), M_TEMP, M_WAITOK);
	    ivlen = cnt * sizeof(*ivp);
	    uiop->uio_iov = iv = ivp;
	    uiop->uio_iovcnt = cnt;
	    mp = info.nmi_mrep;
	    while (mp) {
		if (mp->m_len > 0) {
			ivp->iov_base = mtod(mp, caddr_t);
			ivp->iov_len = mp->m_len;
			ivp++;
		}
		mp = mp->m_next;
	    }

	    if (stable == NFSV3WRITE_UNSTABLE)
		ioflags = IO_NODELOCKED;
	    else if (stable == NFSV3WRITE_DATASYNC)
		ioflags = (IO_SYNC | IO_NODELOCKED);
	    else
		ioflags = (IO_SYNC | IO_NODELOCKED);
	    uiop->uio_resid = len;
	    uiop->uio_rw = UIO_WRITE;
	    uiop->uio_segflg = UIO_SYSSPACE;
	    uiop->uio_procp = NULL;
	    uiop->uio_offset = off;
	    error = VOP_WRITE(vp, uiop, ioflags, cred);
	    nfsstats.srvvop_writes++;
	    free(iv, M_TEMP, ivlen);
	}
	aftat_ret = VOP_GETATTR(vp, &va, cred, procp);
	vput(vp);
	if (!error)
		error = aftat_ret;
	nfsm_reply(NFSX_PREOPATTR(info.nmi_v3) + NFSX_POSTOPORFATTR(info.nmi_v3) +
		2 * NFSX_UNSIGNED + NFSX_WRITEVERF(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvwcc(nfsd, forat_ret, &forat, aftat_ret, &va, &info);
		if (error) {
			error = 0;
			goto nfsmout;
		}
		tl = nfsm_build(&info.nmi_mb, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(retlen);
		if (stable == NFSV3WRITE_UNSTABLE)
			*tl++ = txdr_unsigned(stable);
		else
			*tl++ = txdr_unsigned(NFSV3WRITE_FILESYNC);
		/*
		 * Actually, there is no need to txdr these fields,
		 * but it may make the values more human readable,
		 * for debugging purposes.
		 */
		*tl++ = txdr_unsigned(boottime.tv_sec);
		*tl = txdr_unsigned(boottime.tv_nsec/1000);
	} else {
		fp = nfsm_build(&info.nmi_mb, NFSX_V2FATTR);
		nfsm_srvfattr(nfsd, &va, fp);
	}
nfsmout:
	return(error);

vbad:
	vput(vp);
bad:
	nfsm_reply(0);
	nfsm_srvwcc(nfsd, forat_ret, &forat, aftat_ret, &va, &info);
	return (0);
}

/*
 * nfs create service
 * now does a truncate to 0 length via. setattr if it already exists
 */
int
nfsrv_create(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va, dirfor, diraft;
	struct nfsv2_sattr *sp;
	struct nfsm_info	info;
	u_int32_t *tl;
	struct nameidata nd;
	caddr_t cp;
	int32_t t1;
	int error = 0, len, tsize, dirfor_ret = 1, diraft_ret = 1;
	dev_t rdev = 0;
	int how, exclusive_flag = 0;
	char *cp2;
	struct vnode *vp = NULL, *dirp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t tempsize;
	u_char cverf[NFSX_V3CREATEVERF];

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	nd.ni_cnd.cn_nameiop = 0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred, procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		if (dirp)
			vrele(dirp);
		return (0);
	}

	VATTR_NULL(&va);
	if (info.nmi_v3) {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		how = fxdr_unsigned(int, *tl);
		switch (how) {
		case NFSV3CREATE_GUARDED:
			if (nd.ni_vp) {
				error = EEXIST;
				break;
			}
		case NFSV3CREATE_UNCHECKED:
			error = nfsm_srvsattr(&info.nmi_md, &va, info.nmi_mrep,
			    &info.nmi_dpos);
			if (error)
				goto nfsmout;
			break;
		case NFSV3CREATE_EXCLUSIVE:
			nfsm_dissect(cp, caddr_t, NFSX_V3CREATEVERF);
			bcopy(cp, cverf, NFSX_V3CREATEVERF);
			exclusive_flag = 1;
			if (nd.ni_vp == NULL)
				va.va_mode = 0;
			break;
		};
		va.va_type = VREG;
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		va.va_type = IFTOVT(fxdr_unsigned(u_int32_t, sp->sa_mode));
		if (va.va_type == VNON)
			va.va_type = VREG;
		va.va_mode = nfstov_mode(sp->sa_mode);
		switch (va.va_type) {
		case VREG:
			tsize = fxdr_unsigned(int32_t, sp->sa_size);
			if (tsize != -1)
				va.va_size = (u_quad_t)tsize;
			break;
		case VCHR:
		case VBLK:
		case VFIFO:
			rdev = (dev_t)fxdr_unsigned(int32_t, sp->sa_size);
			break;
		default:
			break;
		};
	}

	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ??
	 */
	if (nd.ni_vp == NULL) {
		if (va.va_type == VREG || va.va_type == VSOCK) {
			vrele(nd.ni_startdir);
			error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
			if (!error) {
				pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
				if (exclusive_flag) {
					exclusive_flag = 0;
					VATTR_NULL(&va);
					bcopy(cverf, (caddr_t)&va.va_atime,
						NFSX_V3CREATEVERF);
					error = VOP_SETATTR(nd.ni_vp, &va, cred,
						procp);
				}
			}
		} else if (va.va_type == VCHR || va.va_type == VBLK ||
			va.va_type == VFIFO) {
			if (va.va_type == VCHR && rdev == 0xffffffff)
				va.va_type = VFIFO;
			if (va.va_type != VFIFO &&
			    (error = suser_ucred(cred))) {
				vrele(nd.ni_startdir);
				pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				vput(nd.ni_dvp);
				nfsm_reply(0);
				error = 0;
				goto nfsmout;
			} else
				va.va_rdev = (dev_t)rdev;
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd,
			    &va);
			if (error) {
				vrele(nd.ni_startdir);
				pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
				nfsm_reply(0);
				error = 0;
				goto nfsmout;
			}
			nd.ni_cnd.cn_nameiop = LOOKUP;
			nd.ni_cnd.cn_flags &= ~(LOCKPARENT | SAVESTART);
			nd.ni_cnd.cn_proc = procp;
			nd.ni_cnd.cn_cred = cred;
			if ((error = vfs_lookup(&nd)) != 0) {
				pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
				nfsm_reply(0);
				error = 0;
				goto nfsmout;
			}
			
			pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
			if (nd.ni_cnd.cn_flags & ISSYMLINK) {
				vrele(nd.ni_dvp);
				vput(nd.ni_vp);
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				error = EINVAL;
				nfsm_reply(0);
				error = 0;
				goto nfsmout;
			}
		} else {
			vrele(nd.ni_startdir);
			pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
			error = ENXIO;
		}
		vp = nd.ni_vp;
	} else {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
		vp = nd.ni_vp;
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (va.va_size != -1) {
			error = nfsrv_access(vp, VWRITE, cred,
			    (nd.ni_cnd.cn_flags & RDONLY), procp, 0);
			if (!error) {
				tempsize = va.va_size;
				VATTR_NULL(&va);
				va.va_size = tempsize;
				error = VOP_SETATTR(vp, &va, cred,
					 procp);
			}
			if (error)
				vput(vp);
		}
	}
	if (!error) {
		memset(fhp, 0, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred, procp);
		vput(vp);
	}
	if (info.nmi_v3) {
		if (exclusive_flag && !error &&
			bcmp(cverf, (caddr_t)&va.va_atime, NFSX_V3CREATEVERF))
			error = EEXIST;
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	nfsm_reply(NFSX_SRVFH(info.nmi_v3) + NFSX_FATTR(info.nmi_v3)
	    + NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(nfsd, 0, &va, &info);
		}
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
	} else {
		nfsm_srvfhtom(&info.nmi_mb, fhp, info.nmi_v3);
		fp = nfsm_build(&info.nmi_mb, NFSX_V2FATTR);
		nfsm_srvfattr(nfsd, &va, fp);
	}
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	if (nd.ni_cnd.cn_nameiop) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	}
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vput(nd.ni_vp);
	return (error);
}

/*
 * nfs v3 mknod service
 */
int
nfsrv_mknod(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nfsm_info	info;
	u_int32_t *tl;
	struct nameidata nd;
	int32_t t1;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	u_int32_t major, minor;
	enum vtype vtyp;
	char *cp2;
	struct vnode *vp, *dirp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	nd.ni_cnd.cn_nameiop = 0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md, &info.nmi_dpos, &dirp, procp);
	if (dirp)
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred, procp);
	if (error) {
		nfsm_reply(NFSX_WCCDATA(1));
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		if (dirp)
			vrele(dirp);
		return (0);
	}

	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	vtyp = nfsv3tov_type(*tl);
	if (vtyp != VCHR && vtyp != VBLK && vtyp != VSOCK && vtyp != VFIFO) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
		error = NFSERR_BADTYPE;
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		vput(nd.ni_dvp);
		goto out;
	}
	VATTR_NULL(&va);
	error = nfsm_srvsattr(&info.nmi_md, &va, info.nmi_mrep, &info.nmi_dpos);
	if (error)
		goto nfsmout;
	if (vtyp == VCHR || vtyp == VBLK) {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		major = fxdr_unsigned(u_int32_t, *tl++);
		minor = fxdr_unsigned(u_int32_t, *tl);
		va.va_rdev = makedev(major, minor);
	}

	/*
	 * Iff doesn't exist, create it.
	 */
	if (nd.ni_vp) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
		error = EEXIST;
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		vput(nd.ni_dvp);
		goto out;
	}
	va.va_type = vtyp;
	if (vtyp == VSOCK) {
		vrele(nd.ni_startdir);
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
		if (!error)
			pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	} else {
		if (va.va_type != VFIFO &&
		    (error = suser_ucred(cred))) {
			vrele(nd.ni_startdir);
			pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
			goto out;
		}
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
		if (error) {
			vrele(nd.ni_startdir);
			goto out;
		}
		nd.ni_cnd.cn_nameiop = LOOKUP;
		nd.ni_cnd.cn_flags &= ~(LOCKPARENT | SAVESTART);
		nd.ni_cnd.cn_proc = procp;
		nd.ni_cnd.cn_cred = procp->p_ucred;
		error = vfs_lookup(&nd);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
		if (error)
			goto out;
		if (nd.ni_cnd.cn_flags & ISSYMLINK) {
			vrele(nd.ni_dvp);
			vput(nd.ni_vp);
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			error = EINVAL;
		}
	}
out:
	vp = nd.ni_vp;
	if (!error) {
		memset(fhp, 0, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred, procp);
		vput(vp);
	}
	diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
	vrele(dirp);
	nfsm_reply(NFSX_SRVFH(1) + NFSX_POSTOPATTR(1) + NFSX_WCCDATA(1));
	if (!error) {
		nfsm_srvpostop_fh(fhp);
		nfsm_srvpostop_attr(nfsd, 0, &va, &info);
	}
	nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft, &info);
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	if (nd.ni_cnd.cn_nameiop) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	}
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vput(nd.ni_vp);
	return (error);
}

/*
 * nfs remove service
 */
int
nfsrv_remove(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nameidata nd;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	char *cp2;
	struct vnode *vp, *dirp;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md, &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred, procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}

	if (!error) {
		vp = nd.ni_vp;
		if (vp->v_type == VDIR &&
		    (error = suser_ucred(cred)) != 0)
			goto out;
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (vp->v_flag & VROOT) {
			error = EBUSY;
			goto out;
		}
		if (vp->v_flag & VTEXT)
			uvm_vnp_uncache(vp);
out:
		if (!error) {
			error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
		} else {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			if (nd.ni_dvp == vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			vput(vp);
		}
	}
	if (dirp && info.nmi_v3) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		return (0);
	}

nfsmout:
	return(error);
}

/*
 * nfs rename service
 */
int
nfsrv_rename(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, len, len2, fdirfor_ret = 1, fdiraft_ret = 1;
	int tdirfor_ret = 1, tdiraft_ret = 1;
	char *cp2;
	struct nameidata fromnd, tond;
	struct vnode *fvp = NULL, *tvp, *tdvp, *fdirp = NULL;
	struct vnode *tdirp = NULL;
	struct vattr fdirfor, fdiraft, tdirfor, tdiraft;
	nfsfh_t fnfh, tnfh;
	fhandle_t *ffhp, *tfhp;
	uid_t saved_uid;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	ffhp = &fnfh.fh_generic;
	tfhp = &tnfh.fh_generic;
	fromnd.ni_cnd.cn_nameiop = 0;
	tond.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(ffhp);
	nfsm_srvnamesiz(len);

	/*
	 * Remember our original uid so that we can reset cr_uid before
	 * the second nfs_namei() call, in case it is remapped.
	 */
	saved_uid = cred->cr_uid;
	fromnd.ni_cnd.cn_cred = cred;
	fromnd.ni_cnd.cn_nameiop = DELETE;
	fromnd.ni_cnd.cn_flags = WANTPARENT | SAVESTART;
	error = nfs_namei(&fromnd, ffhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &fdirp, procp);
	if (fdirp) {
		if (info.nmi_v3)
			fdirfor_ret = VOP_GETATTR(fdirp, &fdirfor, cred,
				procp);
		else {
			vrele(fdirp);
			fdirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(2 * NFSX_WCCDATA(info.nmi_v3));
		nfsm_srvwcc(nfsd, fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft,
		    &info);
		nfsm_srvwcc(nfsd, tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft,
		    &info);
		if (fdirp)
			vrele(fdirp);
		return (0);
	}

	fvp = fromnd.ni_vp;
	nfsm_srvmtofh(tfhp);
	nfsm_strsiz(len2, NFS_MAXNAMLEN);
	cred->cr_uid = saved_uid;
	tond.ni_cnd.cn_cred = cred;
	tond.ni_cnd.cn_nameiop = RENAME;
	tond.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART;
	error = nfs_namei(&tond, tfhp, len2, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &tdirp, procp);
	if (tdirp) {
		if (info.nmi_v3)
			tdirfor_ret = VOP_GETATTR(tdirp, &tdirfor, cred,
				procp);
		else {
			vrele(tdirp);
			tdirp = NULL;
		}
	}
	if (error) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = info.nmi_v3 ? EEXIST : EISDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = info.nmi_v3 ? EEXIST : ENOTDIR;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			error = info.nmi_v3 ? EXDEV : ENOTEMPTY;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		error = info.nmi_v3 ? EXDEV : ENOTEMPTY;
		goto out;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		error = info.nmi_v3 ? EXDEV : ENOTEMPTY;
		goto out;
	}
	if (fvp == tdvp)
		error = info.nmi_v3 ? EINVAL : ENOTEMPTY;
	/*
	 * If source is the same as the destination (that is the
	 * same vnode with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_cnd.cn_namelen == tond.ni_cnd.cn_namelen &&
	    !bcmp(fromnd.ni_cnd.cn_nameptr, tond.ni_cnd.cn_nameptr,
	      fromnd.ni_cnd.cn_namelen))
		error = -1;
out:
	if (!error) {
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		if (error == -1)
			error = 0;
	}
	vrele(tond.ni_startdir);
	pool_put(&namei_pool, tond.ni_cnd.cn_pnbuf);
out1:
	if (fdirp) {
		fdiraft_ret = VOP_GETATTR(fdirp, &fdiraft, cred, procp);
		vrele(fdirp);
	}
	if (tdirp) {
		tdiraft_ret = VOP_GETATTR(tdirp, &tdiraft, cred, procp);
		vrele(tdirp);
	}
	vrele(fromnd.ni_startdir);
	pool_put(&namei_pool, fromnd.ni_cnd.cn_pnbuf);
	nfsm_reply(2 * NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvwcc(nfsd, fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft,
		    &info);
		nfsm_srvwcc(nfsd, tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft,
		    &info);
	}
	return (0);

nfsmout:
	if (fdirp)
		vrele(fdirp);
	if (tdirp)
		vrele(tdirp);
	if (tond.ni_cnd.cn_nameiop) {
		vrele(tond.ni_startdir);
		pool_put(&namei_pool, tond.ni_cnd.cn_pnbuf);
	}
	if (fromnd.ni_cnd.cn_nameiop) {
		if (fromnd.ni_startdir)
			vrele(fromnd.ni_startdir);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);

		/*
		 * XXX: Workaround the fact that fromnd.ni_dvp can point
		 * to the same vnode as fdirp.
		 */
		if (fromnd.ni_dvp != NULL && fromnd.ni_dvp != fdirp)
			vrele(fromnd.ni_dvp);
		if (fvp)
			vrele(fvp);
	}
	return (error);
}

/*
 * nfs link service
 */
int
nfsrv_link(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct nfsm_info	info;
	struct ucred *cred = &nfsd->nd_cr;
	struct nameidata nd;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, len, dirfor_ret = 1, diraft_ret = 1;
	int getret = 1;
	char *cp2;
	struct vnode *vp, *xp, *dirp = NULL;
	struct vattr dirfor, diraft, at;
	nfsfh_t nfh, dnfh;
	fhandle_t *fhp, *dfhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	dfhp = &dnfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvmtofh(dfhp);
	nfsm_srvnamesiz(len);

	error = nfsrv_fhtovp(fhp, 0, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) +
		    NFSX_WCCDATA(info.nmi_v3));
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type == VDIR && (error = suser_ucred(cred)) != 0)
		goto out1;
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, dfhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error)
		goto out1;
	xp = nd.ni_vp;
	if (xp != NULL) {
		error = EEXIST;
		goto out;
	}
	xp = nd.ni_dvp;
	if (vp->v_mount != xp->v_mount)
		error = EXDEV;
out:
	if (!error) {
		error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
	}
out1:
	if (info.nmi_v3)
		getret = VOP_GETATTR(vp, &at, cred, procp);
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	vrele(vp);
	nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) + NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		     &info);
		error = 0;
	}
nfsmout:
	return(error);
}

/*
 * nfs symbolic link service
 */
int
nfsrv_symlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nameidata nd;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	struct nfsv2_sattr *sp;
	char *pathcp = NULL, *cp2;
	struct uio io;
	struct iovec iv;
	int error = 0, len, len2, dirfor_ret = 1, diraft_ret = 1;
	struct vnode *dirp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	nd.ni_cnd.cn_nameiop = 0;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | SAVESTART;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error)
		goto out;
	VATTR_NULL(&va);
	if (info.nmi_v3)
		error = nfsm_srvsattr(&info.nmi_md, &va, info.nmi_mrep,
		    &info.nmi_dpos);
		if (error)
			goto nfsmout;
	nfsm_strsiz(len2, NFS_MAXPATHLEN);
	pathcp = malloc(len2 + 1, M_TEMP, M_WAITOK);
	iv.iov_base = pathcp;
	iv.iov_len = len2;
	io.uio_resid = len2;
	io.uio_offset = 0;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = NULL;
	nfsm_mtouio(&io, len2);
	if (!info.nmi_v3) {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		va.va_mode = fxdr_unsigned(u_int16_t, sp->sa_mode);
	}
	*(pathcp + len2) = '\0';
	if (nd.ni_vp) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va, pathcp);
	if (error)
		vrele(nd.ni_startdir);
	else {
	    if (info.nmi_v3) {
		nd.ni_cnd.cn_nameiop = LOOKUP;
		nd.ni_cnd.cn_flags &= ~(LOCKPARENT | SAVESTART | FOLLOW);
		nd.ni_cnd.cn_flags |= (NOFOLLOW | LOCKLEAF);
		nd.ni_cnd.cn_proc = procp;
		nd.ni_cnd.cn_cred = cred;
		error = vfs_lookup(&nd);
		if (!error) {
			memset(fhp, 0, sizeof(nfh));
			fhp->fh_fsid = nd.ni_vp->v_mount->mnt_stat.f_fsid;
			error = VFS_VPTOFH(nd.ni_vp, &fhp->fh_fid);
			if (!error)
				error = VOP_GETATTR(nd.ni_vp, &va, cred,
					procp);
			vput(nd.ni_vp);
		}
	    } else
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	}
out:
	if (pathcp)
		free(pathcp, M_TEMP, 0);
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	nfsm_reply(NFSX_SRVFH(info.nmi_v3) + NFSX_POSTOPATTR(info.nmi_v3)
	    + NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(nfsd, 0, &va, &info);
		}
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
	}
	return (0);
nfsmout:
	if (nd.ni_cnd.cn_nameiop) {
		vrele(nd.ni_startdir);
		pool_put(&namei_pool, nd.ni_cnd.cn_pnbuf);
	}
	if (dirp)
		vrele(dirp);
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	if (pathcp)
		free(pathcp, M_TEMP, 0);
	return (error);
}

/*
 * nfs mkdir service
 */
int
nfsrv_mkdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nfs_fattr *fp;
	struct nameidata nd;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	char *cp2;
	struct vnode *vp, *dirp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred, procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		if (dirp)
			vrele(dirp);
		return (0);
	}

	VATTR_NULL(&va);
	if (info.nmi_v3) {
		error = nfsm_srvsattr(&info.nmi_md, &va, info.nmi_mrep,
		    &info.nmi_dpos);
		if (error)
			goto nfsmout;
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		va.va_mode = nfstov_mode(*tl++);
	}
	va.va_type = VDIR;
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		error = EEXIST;
		goto out;
	}
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
	if (!error) {
		vp = nd.ni_vp;
		memset(fhp, 0, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred, procp);
		vput(vp);
	}
out:
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	nfsm_reply(NFSX_SRVFH(info.nmi_v3) + NFSX_POSTOPATTR(info.nmi_v3) +
	    NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(nfsd, 0, &va, &info);
		}
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
	} else {
		nfsm_srvfhtom(&info.nmi_mb, fhp, info.nmi_v3);
		fp = nfsm_build(&info.nmi_mb, NFSX_V2FATTR);
		nfsm_srvfattr(nfsd, &va, fp);
	}
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp)
		vrele(nd.ni_vp);
	return (error);
}

/*
 * nfs rmdir service
 */
int
nfsrv_rmdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, len, dirfor_ret = 1, diraft_ret = 1;
	char *cp2;
	struct vnode *vp, *dirp = NULL;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct nameidata nd;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, fhp, len, slp, nam, &info.nmi_md,
	    &info.nmi_dpos, &dirp, procp);
	if (dirp) {
		if (info.nmi_v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred,
				procp);
		else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		if (dirp)
			vrele(dirp);
		return (0);
	}
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred, procp);
		vrele(dirp);
	}
	nfsm_reply(NFSX_WCCDATA(info.nmi_v3));
	if (info.nmi_v3) {
		nfsm_srvwcc(nfsd, dirfor_ret, &dirfor, diraft_ret, &diraft,
		    &info);
		error = 0;
	}
nfsmout:
	return(error);
}

/*
 * nfs readdir service
 * - mallocs what it thinks is enough to read
 *	count rounded up to a multiple of NFS_DIRBLKSIZ <= NFS_MAXREADDIR
 * - calls VOP_READDIR()
 * - loops around building the reply
 *	if the output generated exceeds count break out of loop
 * - it only knows that it has encountered eof when the VOP_READDIR()
 *	reads nothing
 * - as such one readdir rpc will return eof false although you are there
 *	and then the next will return eof
 * - it trims out records with d_fileno == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * NB: It is tempting to set eof to true if the VOP_READDIR() reads less
 *	than requested, but this may not apply to all filesystems. For
 *	example, client NFS does not { although it is never remote mounted
 *	anyhow }
 *     The alternate call nfsrv_readdirplus() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
struct flrep {
	nfsuint64 fl_off;
	u_int32_t fl_postopok;
	u_int32_t fl_fattr[NFSX_V3FATTR / sizeof (u_int32_t)];
	u_int32_t fl_fhok;
	u_int32_t fl_fhsize;
	u_int32_t fl_nfh[NFSX_V3FH / sizeof (u_int32_t)];
};

int
nfsrv_readdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct dirent *dp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	int len, nlen, pad, xfer, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly;
	u_quad_t off, toff, verf;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (info.nmi_v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		toff = fxdr_hyper(tl);
		tl += 2;
		verf = fxdr_hyper(tl);
		tl += 2;
	} else {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		toff = fxdr_unsigned(u_quad_t, *tl++);
	}
	off = toff;
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	if (cnt > xfer)
		cnt = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	if (info.nmi_v3)
		error = getret = VOP_GETATTR(vp, &at, cred, procp);
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, procp, 0);
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3));
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, procp);
	rbuf = malloc(fullsiz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = NULL;
	eofflag = 0;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, procp);
	error = VOP_READDIR(vp, &io, cred, &eofflag);

	off = (off_t)io.uio_offset;
	if (info.nmi_v3) {
		getret = VOP_GETATTR(vp, &at, cred, procp);
		if (!error)
			error = getret;
	}

	VOP_UNLOCK(vp, 0, procp);
	if (error) {
		vrele(vp);
		free(rbuf, M_TEMP, fullsiz);
		nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3));
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) + NFSX_COOKIEVERF(info.nmi_v3) +
				2 * NFSX_UNSIGNED);
			if (info.nmi_v3) {
				nfsm_srvpostop_attr(nfsd, getret, &at, &info);
				tl = nfsm_build(&info.nmi_mb, 4 * NFSX_UNSIGNED);
				txdr_hyper(at.va_filerev, tl);
				tl += 2;
			} else
				tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			free(rbuf, M_TEMP, fullsiz);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;

	while (cpos < cend && dp->d_fileno == 0) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	if (cpos >= cend) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	len = 3 * NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) + NFSX_COOKIEVERF(info.nmi_v3) + siz);
	if (info.nmi_v3) {
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
		txdr_hyper(at.va_filerev, tl);
	}

	/* Loop through the records and build reply */
	while (cpos < cend) {
		if (dp->d_fileno != 0) {
			nlen = dp->d_namlen;
			pad = nfsm_padlen(nlen);
			len += (4 * NFSX_UNSIGNED + nlen + pad);
			if (info.nmi_v3)
				len += 2 * NFSX_UNSIGNED;
			if (len > cnt) {
				eofflag = 0;
				break;
			}
			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			tl = nfsm_build(&info.nmi_mb,
			    (info.nmi_v3 ? 3 : 2) * NFSX_UNSIGNED);
			*tl++ = nfs_true;
			if (info.nmi_v3)
				txdr_hyper(dp->d_fileno, tl);
			else
				*tl = txdr_unsigned((u_int32_t)dp->d_fileno);
	
			/* And copy the name */
			nfsm_strtombuf(&info.nmi_mb, dp->d_name, nlen);
	
			/* Finish off the record */
			if (info.nmi_v3) {
				tl = nfsm_build(&info.nmi_mb, 2*NFSX_UNSIGNED);
				txdr_hyper(dp->d_off, tl);
			} else {
				tl = nfsm_build(&info.nmi_mb, NFSX_UNSIGNED);
				*tl = txdr_unsigned((u_int32_t)dp->d_off);
			}
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	vrele(vp);
	tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
	*tl++ = nfs_false;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	free(rbuf, M_TEMP, fullsiz);
nfsmout:
	return(error);
}

int
nfsrv_readdirplus(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct dirent *dp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp, *nvp;
	struct flrep fl;
	nfsfh_t nfh;
	fhandle_t *fhp, *nfhp = (fhandle_t *)fl.fl_nfh;
	struct uio io;
	struct iovec iv;
	struct vattr va, at, *vap = &va;
	struct nfs_fattr *fp;
	int len, nlen, pad, xfer, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, dirlen;
	u_quad_t off, toff, verf;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
	toff = fxdr_hyper(tl);
	tl += 2;
	verf = fxdr_hyper(tl);
	tl += 2;
	siz = fxdr_unsigned(int, *tl++);
	cnt = fxdr_unsigned(int, *tl);
	off = toff;
	siz = ((siz + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	if (cnt > xfer)
		cnt = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	error = getret = VOP_GETATTR(vp, &at, cred, procp);
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, procp, 0);
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, procp);

	rbuf = malloc(fullsiz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_procp = NULL;
	eofflag = 0;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, procp);
	error = VOP_READDIR(vp, &io, cred, &eofflag);

	off = (u_quad_t)io.uio_offset;
	getret = VOP_GETATTR(vp, &at, cred, procp);

	VOP_UNLOCK(vp, 0, procp);

	if (!error)
		error = getret;
	if (error) {
		vrele(vp);
		free(rbuf, M_TEMP, fullsiz);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF +
				2 * NFSX_UNSIGNED);
			nfsm_srvpostop_attr(nfsd, getret, &at, &info);
			tl = nfsm_build(&info.nmi_mb, 4 * NFSX_UNSIGNED);
			txdr_hyper(at.va_filerev, tl);
			tl += 2;
			*tl++ = nfs_false;
			*tl = nfs_true;
			free(rbuf, M_TEMP, fullsiz);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;

	while (cpos < cend && dp->d_fileno == 0) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	if (cpos >= cend) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	/*
	 * struct READDIRPLUS3resok {
	 *     postop_attr dir_attributes;
	 *     cookieverf3 cookieverf;
	 *     dirlistplus3 reply;
	 * }
	 *
	 * struct dirlistplus3 {
	 *     entryplus3  *entries;
	 *     bool eof;
	 *  }
	 */	
	dirlen = len = NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF + 2 * NFSX_UNSIGNED;
	nfsm_reply(cnt);
	nfsm_srvpostop_attr(nfsd, getret, &at, &info);
	tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
	txdr_hyper(at.va_filerev, tl);

	/* Loop through the records and build reply */
	while (cpos < cend) {
		if (dp->d_fileno != 0) {
			nlen = dp->d_namlen;
			pad = nfsm_padlen(nlen);

			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp))
				goto invalid;
			memset(nfhp, 0, NFSX_V3FH);
			nfhp->fh_fsid =
				nvp->v_mount->mnt_stat.f_fsid;
			if (VFS_VPTOFH(nvp, &nfhp->fh_fid)) {
				vput(nvp);
				goto invalid;
			}
			if (VOP_GETATTR(nvp, vap, cred, procp)) {
				vput(nvp);
				goto invalid;
			}
			vput(nvp);

			/*
			 * If either the dircount or maxcount will be
			 * exceeded, get out now. Both of these lengths
			 * are calculated conservatively, including all
			 * XDR overheads.
			 *
			 * Each entry:
			 * 2 * NFSX_UNSIGNED for fileid3
			 * 1 * NFSX_UNSIGNED for length of name
			 * nlen + pad == space the name takes up
			 * 2 * NFSX_UNSIGNED for the cookie
			 * 1 * NFSX_UNSIGNED to indicate if file handle present
			 * 1 * NFSX_UNSIGNED for the file handle length
			 * NFSX_V3FH == space our file handle takes up
			 * NFSX_V3POSTOPATTR == space the attributes take up
			 * 1 * NFSX_UNSIGNED for next pointer
			 */
			len += (8 * NFSX_UNSIGNED + nlen + pad + NFSX_V3FH +
				NFSX_V3POSTOPATTR);
			dirlen += (6 * NFSX_UNSIGNED + nlen + pad);
			if (len > cnt || dirlen > fullsiz) {
				eofflag = 0;
				break;
			}

			tl = nfsm_build(&info.nmi_mb, 3 * NFSX_UNSIGNED);
			*tl++ = nfs_true;
			txdr_hyper(dp->d_fileno, tl);

			/* And copy the name */
			nfsm_strtombuf(&info.nmi_mb, dp->d_name, nlen);

			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			fp = (struct nfs_fattr *)&fl.fl_fattr;
			nfsm_srvfattr(nfsd, vap, fp);
			fl.fl_fhsize = txdr_unsigned(NFSX_V3FH);
			fl.fl_fhok = nfs_true;
			fl.fl_postopok = nfs_true;
			txdr_hyper(dp->d_off, fl.fl_off.nfsuquad);

			/* Now copy the flrep structure out. */
			nfsm_buftombuf(&info.nmi_mb, &fl, sizeof(struct flrep));
		}
invalid:
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
	}
	vrele(vp);
	tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED);
	*tl++ = nfs_false;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	free(rbuf, M_TEMP, fullsiz);
nfsmout:
	return(error);
}

/*
 * nfs commit service
 */
int
nfsrv_commit(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr bfor, aft;
	struct vnode *vp;
	struct nfsm_info	info;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, for_ret = 1, aft_ret = 1, cnt;
	char *cp2;
	u_quad_t off;
	
	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);

	/*
	 * XXX At this time VOP_FSYNC() does not accept offset and byte
	 * count parameters, so these arguments are useless (someday maybe).
	 */
	off = fxdr_hyper(tl);
	tl += 2;
	cnt = fxdr_unsigned(int, *tl);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc(nfsd, for_ret, &bfor, aft_ret, &aft, &info);
		error = 0;
		goto nfsmout;
	}
	for_ret = VOP_GETATTR(vp, &bfor, cred, procp);
	error = VOP_FSYNC(vp, cred, MNT_WAIT, procp);
	aft_ret = VOP_GETATTR(vp, &aft, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_V3WCCDATA + NFSX_V3WRITEVERF);
	nfsm_srvwcc(nfsd, for_ret, &bfor, aft_ret, &aft, &info);
	if (!error) {
		tl = nfsm_build(&info.nmi_mb, NFSX_V3WRITEVERF);
		*tl++ = txdr_unsigned(boottime.tv_sec);
		*tl = txdr_unsigned(boottime.tv_nsec/1000);
	} else
		error = 0;
nfsmout:
	return(error);
}

/*
 * nfs statfs service
 */
int
nfsrv_statfs(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct statfs *sf;
	struct nfs_statfs *sfp;
	struct nfsm_info	info;
	u_int32_t *tl;
	int32_t t1;
	int error = 0, rdonly, getret = 1;
	char *cp2;
	struct vnode *vp;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct statfs statfs;
	u_quad_t tval;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	sf = &statfs;
	error = VFS_STATFS(vp->v_mount, sf, procp);
	getret = VOP_GETATTR(vp, &at, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_POSTOPATTR(info.nmi_v3) + NFSX_STATFS(info.nmi_v3));
	if (info.nmi_v3)
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	sfp = nfsm_build(&info.nmi_mb, NFSX_STATFS(info.nmi_v3));
	if (info.nmi_v3) {
		tval = (u_quad_t)sf->f_blocks;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_tbytes);
		tval = (u_quad_t)sf->f_bfree;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_fbytes);
		tval = (u_quad_t)sf->f_bavail;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_abytes);
		tval = (u_quad_t)sf->f_files;
		txdr_hyper(tval, &sfp->sf_tfiles);
		tval = (u_quad_t)sf->f_ffree;
		txdr_hyper(tval, &sfp->sf_ffiles);
		txdr_hyper(tval, &sfp->sf_afiles);
		sfp->sf_invarsec = 0;
	} else {
		sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
		sfp->sf_bsize = txdr_unsigned(sf->f_bsize);
		sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
		sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
		sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	}
nfsmout:
	return(error);
}

/*
 * nfs fsinfo service
 */
int
nfsrv_fsinfo(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfsm_info	info;
	u_int32_t *tl;
	struct nfsv3_fsinfo *sip;
	int32_t t1;
	int error = 0, rdonly, getret = 1, pref;
	char *cp2;
	struct vnode *vp;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	getret = VOP_GETATTR(vp, &at, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3FSINFO);
	nfsm_srvpostop_attr(nfsd, getret, &at, &info);
	sip = nfsm_build(&info.nmi_mb, NFSX_V3FSINFO);

	/*
	 * XXX
	 * There should be file system VFS OP(s) to get this information.
	 * For now, assume ufs.
	 */
	if (slp->ns_so->so_type == SOCK_DGRAM)
		pref = NFS_MAXDGRAMDATA;
	else
		pref = NFS_MAXDATA;
	sip->fs_rtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_rtpref = txdr_unsigned(pref);
	sip->fs_rtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_wtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_wtpref = txdr_unsigned(pref);
	sip->fs_wtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_dtpref = txdr_unsigned(pref);
	sip->fs_maxfilesize.nfsuquad[0] = 0xffffffff;
	sip->fs_maxfilesize.nfsuquad[1] = 0xffffffff;
	sip->fs_timedelta.nfsv3_sec = 0;
	sip->fs_timedelta.nfsv3_nsec = txdr_unsigned(1);
	sip->fs_properties = txdr_unsigned(NFSV3FSINFO_LINK |
		NFSV3FSINFO_SYMLINK | NFSV3FSINFO_HOMOGENEOUS |
		NFSV3FSINFO_CANSETTIME);
nfsmout:
	return(error);
}

/*
 * nfs pathconf service
 */
int
nfsrv_pathconf(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct mbuf *nam = nfsd->nd_nam;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfsm_info	info;
	u_int32_t *tl;
	struct nfsv3_pathconf *pc;
	int32_t t1;
	int error = 0, rdonly, getret = 1;
	register_t linkmax, namemax, chownres, notrunc;
	char *cp2;
	struct vnode *vp;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(nfsd, getret, &at, &info);
		error = 0;
		goto nfsmout;
	}
	error = VOP_PATHCONF(vp, _PC_LINK_MAX, &linkmax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NAME_MAX, &namemax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_CHOWN_RESTRICTED, &chownres);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NO_TRUNC, &notrunc);
	getret = VOP_GETATTR(vp, &at, cred, procp);
	vput(vp);
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3PATHCONF);
	nfsm_srvpostop_attr(nfsd, getret, &at, &info);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	pc = nfsm_build(&info.nmi_mb, NFSX_V3PATHCONF);

	pc->pc_linkmax = txdr_unsigned(linkmax);
	pc->pc_namemax = txdr_unsigned(namemax);
	pc->pc_notrunc = txdr_unsigned(notrunc);
	pc->pc_chownrestricted = txdr_unsigned(chownres);

	/*
	 * These should probably be supported by VOP_PATHCONF(), but
	 * until msdosfs is exportable (why would you want to?), the
	 * Unix defaults should be ok.
	 */
	pc->pc_caseinsensitive = nfs_false;
	pc->pc_casepreserving = nfs_true;
nfsmout:
	return(error);
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
int
nfsrv_null(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{
	struct nfsm_info	info;
	int error = NFSERR_RETVOID;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	nfsm_reply(0);
	return (0);
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
int
nfsrv_noop(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct proc *procp, struct mbuf **mrq)
{	
	struct nfsm_info	info;
	int error;

	info.nmi_mreq = NULL;
	info.nmi_mrep = nfsd->nd_mrep;
	info.nmi_md = nfsd->nd_md;
	info.nmi_dpos = nfsd->nd_dpos;
	info.nmi_v3 = (nfsd->nd_flag & ND_NFSV3);

	if (nfsd->nd_repstat)
		error = nfsd->nd_repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
	return (0);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client.
 * You cannot just use vn_writechk() and VOP_ACCESS() for two reasons:
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the
 *     write case
 * 2 - The owner is to be given access irrespective of mode bits for some
 *     operations, so that processes that chmod after opening a file don't
 *     break. I don't like this because it opens a security hole, but since
 *     the nfs server opens a security hole the size of a barn door anyhow,
 *     what the heck. A notable exception to this rule is when VOP_ACCESS()
 *     returns EPERM (e.g. when a file is immutable) which is always an
 *     error.
 */
int
nfsrv_access(struct vnode *vp, int flags, struct ucred *cred, int rdonly,
    struct proc *p, int override)
{
	struct vattr vattr;
	int error;

	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
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
		 * If there's shared text associated with
		 * the inode, try to free it up once.  If
		 * we fail, we can't allow writing.
		 */
		if ((vp->v_flag & VTEXT) && !uvm_vnp_uncache(vp))
			return (ETXTBSY);
	}
	error = VOP_ACCESS(vp, flags, cred, p);
	/*
	 * Allow certain operations for the owner (reads and writes
	 * on files that are already open).
	 */
	if (override && error == EACCES &&
	    VOP_GETATTR(vp, &vattr, cred, p) == 0 &&
	    cred->cr_uid == vattr.va_uid)
		error = 0;
	return error;
}
