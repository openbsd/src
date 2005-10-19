/*	$OpenBSD: nfs_subs.c,v 1.55 2005/10/19 16:50:46 pedro Exp $	*/
/*	$NetBSD: nfs_subs.c,v 1.27.4.3 1996/07/08 20:34:24 jtc Exp $	*/

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
 *	@(#)nfs_subs.c	8.8 (Berkeley) 5/22/95
 */


/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/time.h>

#include <uvm/uvm_extern.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsrtt.h>
#include <nfs/nfs_var.h>

#include <miscfs/specfs/specdev.h>

#include <netinet/in.h>

#include <dev/rndvar.h>

#ifdef __GNUC__
#define INLINE __inline
#else
#define INLINE
#endif

int	nfs_attrtimeo(struct nfsnode *np);

/*
 * Data items converted to xdr at startup, since they are constant
 * This is kinda hokey, but may save a little time doing byte swaps
 */
u_int32_t nfs_xdrneg1;
u_int32_t rpc_call, rpc_vers, rpc_reply, rpc_msgdenied, rpc_autherr,
	rpc_mismatch, rpc_auth_unix, rpc_msgaccepted,
	rpc_auth_kerb;
u_int32_t nfs_prog, nfs_true, nfs_false;

/* And other global data */
static u_int32_t nfs_xid = 0;
static u_int32_t nfs_xid_touched = 0;
nfstype nfsv2_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFNON,
		      NFCHR, NFNON };
nfstype nfsv3_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFSOCK,
		      NFFIFO, NFNON };
enum vtype nv2tov_type[8] = { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VNON, VNON };
enum vtype nv3tov_type[8]={ VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO };
int nfs_ticks;

/*
 * Mapping of old NFS Version 2 RPC numbers to generic numbers.
 */
int nfsv3_procid[NFS_NPROCS] = {
	NFSPROC_NULL,
	NFSPROC_GETATTR,
	NFSPROC_SETATTR,
	NFSPROC_NOOP,
	NFSPROC_LOOKUP,
	NFSPROC_READLINK,
	NFSPROC_READ,
	NFSPROC_NOOP,
	NFSPROC_WRITE,
	NFSPROC_CREATE,
	NFSPROC_REMOVE,
	NFSPROC_RENAME,
	NFSPROC_LINK,
	NFSPROC_SYMLINK,
	NFSPROC_MKDIR,
	NFSPROC_RMDIR,
	NFSPROC_READDIR,
	NFSPROC_FSSTAT,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP
};

/*
 * and the reverse mapping from generic to Version 2 procedure numbers
 */
int nfsv2_procid[NFS_NPROCS] = {
	NFSV2PROC_NULL,
	NFSV2PROC_GETATTR,
	NFSV2PROC_SETATTR,
	NFSV2PROC_LOOKUP,
	NFSV2PROC_NOOP,
	NFSV2PROC_READLINK,
	NFSV2PROC_READ,
	NFSV2PROC_WRITE,
	NFSV2PROC_CREATE,
	NFSV2PROC_MKDIR,
	NFSV2PROC_SYMLINK,
	NFSV2PROC_CREATE,
	NFSV2PROC_REMOVE,
	NFSV2PROC_RMDIR,
	NFSV2PROC_RENAME,
	NFSV2PROC_LINK,
	NFSV2PROC_READDIR,
	NFSV2PROC_NOOP,
	NFSV2PROC_STATFS,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
};

/*
 * Maps errno values to nfs error numbers.
 * Use NFSERR_IO as the catch all for ones not specifically defined in
 * RFC 1094.
 */
static u_char nfsrv_v2errmap[ELAST] = {
  NFSERR_PERM,	NFSERR_NOENT,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_NXIO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_ACCES,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_EXIST,	NFSERR_IO,	NFSERR_NODEV,	NFSERR_NOTDIR,
  NFSERR_ISDIR,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_FBIG,	NFSERR_NOSPC,	NFSERR_IO,	NFSERR_ROFS,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_NAMETOL,	NFSERR_IO,	NFSERR_IO,
  NFSERR_NOTEMPTY, NFSERR_IO,	NFSERR_IO,	NFSERR_DQUOT,	NFSERR_STALE,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,
};

/*
 * Maps errno values to nfs error numbers.
 * Although it is not obvious whether or not NFS clients really care if
 * a returned error value is in the specified list for the procedure, the
 * safest thing to do is filter them appropriately. For Version 2, the
 * X/Open XNFS document is the only specification that defines error values
 * for each RPC (The RFC simply lists all possible error values for all RPCs),
 * so I have decided to not do this for Version 2.
 * The first entry is the default error return and the rest are the valid
 * errors for that RPC in increasing numeric order.
 */
static short nfsv3err_null[] = {
	0,
	0,
};

static short nfsv3err_getattr[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_setattr[] = {
	NFSERR_IO,
	NFSERR_PERM,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOT_SYNC,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_lookup[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_NAMETOL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_access[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_readlink[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_read[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_NXIO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_write[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_FBIG,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_create[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_mkdir[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_symlink[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_mknod[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_BADTYPE,
	0,
};

static short nfsv3err_remove[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_rmdir[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NOTDIR,
	NFSERR_INVAL,
	NFSERR_ROFS,
	NFSERR_NAMETOL,
	NFSERR_NOTEMPTY,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_rename[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_XDEV,
	NFSERR_NOTDIR,
	NFSERR_ISDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_NAMETOL,
	NFSERR_NOTEMPTY,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_link[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_XDEV,
	NFSERR_NOTDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_NAMETOL,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_readdir[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_readdirplus[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_NOTSUPP,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_fsstat[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_fsinfo[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_pathconf[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short nfsv3err_commit[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	0,
};

static short *nfsrv_v3errmap[] = {
	nfsv3err_null,
	nfsv3err_getattr,
	nfsv3err_setattr,
	nfsv3err_lookup,
	nfsv3err_access,
	nfsv3err_readlink,
	nfsv3err_read,
	nfsv3err_write,
	nfsv3err_create,
	nfsv3err_mkdir,
	nfsv3err_symlink,
	nfsv3err_mknod,
	nfsv3err_remove,
	nfsv3err_rmdir,
	nfsv3err_rename,
	nfsv3err_link,
	nfsv3err_readdir,
	nfsv3err_readdirplus,
	nfsv3err_fsstat,
	nfsv3err_fsinfo,
	nfsv3err_pathconf,
	nfsv3err_commit,
};

extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern struct nfsrtt nfsrtt;
extern struct nfsstats nfsstats;
extern nfstype nfsv2_type[9];
extern nfstype nfsv3_type[9];
extern struct nfsnodehashhead *nfsnodehashtbl;
extern u_long nfsnodehash;

LIST_HEAD(nfsnodehashhead, nfsnode);

struct pool nfsreqpl;

/*
 * Create the header for an rpc request packet
 * The hsiz is the size of the rest of the nfs request header.
 * (just used to decide if a cluster is a good idea)
 */
struct mbuf *
nfsm_reqh(vp, procid, hsiz, bposp)
	struct vnode *vp;
	u_long procid;
	int hsiz;
	caddr_t *bposp;
{
	struct mbuf *mb;
	caddr_t bpos;

	MGET(mb, M_WAIT, MT_DATA);
	if (hsiz >= MINCLSIZE)
		MCLGET(mb, M_WAIT);
	mb->m_len = 0;
	bpos = mtod(mb, caddr_t);
	
	/* Finally, return values */
	*bposp = bpos;
	return (mb);
}

/*
 * Build the RPC header and fill in the authorization info.
 * The authorization string argument is only used when the credentials
 * come from outside of the kernel.
 * Returns the head of the mbuf list.
 */
struct mbuf *
nfsm_rpchead(cr, nmflag, procid, auth_type, auth_len, auth_str, verf_len,
	verf_str, mrest, mrest_len, mbp, xidp)
	struct ucred *cr;
	int nmflag;
	int procid;
	int auth_type;
	int auth_len;
	char *auth_str;
	int verf_len;
	char *verf_str;
	struct mbuf *mrest;
	int mrest_len;
	struct mbuf **mbp;
	u_int32_t *xidp;
{
	struct mbuf *mb;
	u_int32_t *tl;
	caddr_t bpos;
	int i;
	struct mbuf *mreq, *mb2;
	int siz, grpsiz, authsiz;

	authsiz = nfsm_rndup(auth_len);
	MGETHDR(mb, M_WAIT, MT_DATA);
	if ((authsiz + 10 * NFSX_UNSIGNED) >= MINCLSIZE) {
		MCLGET(mb, M_WAIT);
	} else if ((authsiz + 10 * NFSX_UNSIGNED) < MHLEN) {
		MH_ALIGN(mb, authsiz + 10 * NFSX_UNSIGNED);
	} else {
		MH_ALIGN(mb, 8 * NFSX_UNSIGNED);
	}
	mb->m_len = 0;
	mreq = mb;
	bpos = mtod(mb, caddr_t);

	/*
	 * First the RPC header.
	 */
	nfsm_build(tl, u_int32_t *, 8 * NFSX_UNSIGNED);

	/* Get a new (non-zero) xid */

	if ((nfs_xid == 0) && (nfs_xid_touched == 0)) {
		nfs_xid = arc4random();
		nfs_xid_touched = 1;
	} else {
		while ((*xidp = arc4random() % 256) == 0)
			;
		nfs_xid += *xidp;
	}
	    
	*tl++ = *xidp = txdr_unsigned(nfs_xid);
	*tl++ = rpc_call;
	*tl++ = rpc_vers;
	*tl++ = txdr_unsigned(NFS_PROG);
	if (nmflag & NFSMNT_NFSV3)
		*tl++ = txdr_unsigned(NFS_VER3);
	else
		*tl++ = txdr_unsigned(NFS_VER2);
	if (nmflag & NFSMNT_NFSV3)
		*tl++ = txdr_unsigned(procid);
	else
		*tl++ = txdr_unsigned(nfsv2_procid[procid]);

	/*
	 * And then the authorization cred.
	 */
	*tl++ = txdr_unsigned(auth_type);
	*tl = txdr_unsigned(authsiz);
	switch (auth_type) {
	case RPCAUTH_UNIX:
		nfsm_build(tl, u_int32_t *, auth_len);
		*tl++ = 0;		/* stamp ?? */
		*tl++ = 0;		/* NULL hostname */
		*tl++ = txdr_unsigned(cr->cr_uid);
		*tl++ = txdr_unsigned(cr->cr_gid);
		grpsiz = (auth_len >> 2) - 5;
		*tl++ = txdr_unsigned(grpsiz);
		for (i = 0; i < grpsiz; i++)
			*tl++ = txdr_unsigned(cr->cr_groups[i]);
		break;
	case RPCAUTH_KERB4:
		siz = auth_len;
		while (siz > 0) {
			if (M_TRAILINGSPACE(mb) == 0) {
				MGET(mb2, M_WAIT, MT_DATA);
				if (siz >= MINCLSIZE)
					MCLGET(mb2, M_WAIT);
				mb->m_next = mb2;
				mb = mb2;
				mb->m_len = 0;
				bpos = mtod(mb, caddr_t);
			}
			i = min(siz, M_TRAILINGSPACE(mb));
			bcopy(auth_str, bpos, i);
			mb->m_len += i;
			auth_str += i;
			bpos += i;
			siz -= i;
		}
		if ((siz = (nfsm_rndup(auth_len) - auth_len)) > 0) {
			for (i = 0; i < siz; i++)
				*bpos++ = '\0';
			mb->m_len += siz;
		}
		break;
	};

	/*
	 * And the verifier...
	 */
	nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	if (verf_str) {
		*tl++ = txdr_unsigned(RPCAUTH_KERB4);
		*tl = txdr_unsigned(verf_len);
		siz = verf_len;
		while (siz > 0) {
			if (M_TRAILINGSPACE(mb) == 0) {
				MGET(mb2, M_WAIT, MT_DATA);
				if (siz >= MINCLSIZE)
					MCLGET(mb2, M_WAIT);
				mb->m_next = mb2;
				mb = mb2;
				mb->m_len = 0;
				bpos = mtod(mb, caddr_t);
			}
			i = min(siz, M_TRAILINGSPACE(mb));
			bcopy(verf_str, bpos, i);
			mb->m_len += i;
			verf_str += i;
			bpos += i;
			siz -= i;
		}
		if ((siz = (nfsm_rndup(verf_len) - verf_len)) > 0) {
			for (i = 0; i < siz; i++)
				*bpos++ = '\0';
			mb->m_len += siz;
		}
	} else {
		*tl++ = txdr_unsigned(RPCAUTH_NULL);
		*tl = 0;
	}
	mb->m_next = mrest;
	mreq->m_pkthdr.len = authsiz + 10 * NFSX_UNSIGNED + mrest_len;
	mreq->m_pkthdr.rcvif = (struct ifnet *)0;
	*mbp = mb;
	return (mreq);
}

/*
 * copies mbuf chain to the uio scatter/gather list
 */
int
nfsm_mbuftouio(mrep, uiop, siz, dpos)
	struct mbuf **mrep;
	struct uio *uiop;
	int siz;
	caddr_t *dpos;
{
	char *mbufcp, *uiocp;
	int xfer, left, len;
	struct mbuf *mp;
	long uiosiz, rem;
	int error = 0;

	mp = *mrep;
	mbufcp = *dpos;
	len = mtod(mp, caddr_t)+mp->m_len-mbufcp;
	rem = nfsm_rndup(siz)-siz;
	while (siz > 0) {
		if (uiop->uio_iovcnt <= 0 || uiop->uio_iov == NULL)
			return (EFBIG);
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			while (len == 0) {
				mp = mp->m_next;
				if (mp == NULL)
					return (EBADRPC);
				mbufcp = mtod(mp, caddr_t);
				len = mp->m_len;
			}
			xfer = (left > len) ? len : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(mbufcp, uiocp, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(mbufcp, uiocp, xfer);
			else
				copyout(mbufcp, uiocp, xfer);
			left -= xfer;
			len -= xfer;
			mbufcp += xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		if (uiop->uio_iov->iov_len <= siz) {
			uiop->uio_iovcnt--;
			uiop->uio_iov++;
		} else {
			uiop->uio_iov->iov_base += uiosiz;
			uiop->uio_iov->iov_len -= uiosiz;
		}
		siz -= uiosiz;
	}
	*dpos = mbufcp;
	*mrep = mp;
	if (rem > 0) {
		if (len < rem)
			error = nfs_adv(mrep, dpos, rem, len);
		else
			*dpos += rem;
	}
	return (error);
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 * NOTE: can ony handle iovcnt == 1
 */
int
nfsm_uiotombuf(uiop, mq, siz, bpos)
	struct uio *uiop;
	struct mbuf **mq;
	int siz;
	caddr_t *bpos;
{
	char *uiocp;
	struct mbuf *mp, *mp2;
	int xfer, left, mlen;
	int uiosiz, clflg, rem;
	char *cp;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfsm_uiotombuf: iovcnt != 1");
#endif

	if (siz > MLEN)		/* or should it >= MCLBYTES ?? */
		clflg = 1;
	else
		clflg = 0;
	rem = nfsm_rndup(siz)-siz;
	mp = mp2 = *mq;
	while (siz > 0) {
		left = uiop->uio_iov->iov_len;
		uiocp = uiop->uio_iov->iov_base;
		if (left > siz)
			left = siz;
		uiosiz = left;
		while (left > 0) {
			mlen = M_TRAILINGSPACE(mp);
			if (mlen == 0) {
				MGET(mp, M_WAIT, MT_DATA);
				if (clflg)
					MCLGET(mp, M_WAIT);
				mp->m_len = 0;
				mp2->m_next = mp;
				mp2 = mp;
				mlen = M_TRAILINGSPACE(mp);
			}
			xfer = (left > mlen) ? mlen : left;
#ifdef notdef
			/* Not Yet.. */
			if (uiop->uio_iov->iov_op != NULL)
				(*(uiop->uio_iov->iov_op))
				(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
#endif
			if (uiop->uio_segflg == UIO_SYSSPACE)
				bcopy(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			else
				copyin(uiocp, mtod(mp, caddr_t)+mp->m_len, xfer);
			mp->m_len += xfer;
			left -= xfer;
			uiocp += xfer;
			uiop->uio_offset += xfer;
			uiop->uio_resid -= xfer;
		}
		uiop->uio_iov->iov_base += uiosiz;
		uiop->uio_iov->iov_len -= uiosiz;
		siz -= uiosiz;
	}
	if (rem > 0) {
		if (rem > M_TRAILINGSPACE(mp)) {
			MGET(mp, M_WAIT, MT_DATA);
			mp->m_len = 0;
			mp2->m_next = mp;
		}
		cp = mtod(mp, caddr_t)+mp->m_len;
		for (left = 0; left < rem; left++)
			*cp++ = '\0';
		mp->m_len += rem;
		*bpos = cp;
	} else
		*bpos = mtod(mp, caddr_t)+mp->m_len;
	*mq = mp;
	return (0);
}

/*
 * Help break down an mbuf chain by setting the first siz bytes contiguous
 * pointed to by returned val.
 * This is used by the macros nfsm_dissect and nfsm_dissecton for tough
 * cases. (The macros use the vars. dpos and dpos2)
 */
int
nfsm_disct(mdp, dposp, siz, left, cp2)
	struct mbuf **mdp;
	caddr_t *dposp;
	int siz;
	int left;
	caddr_t *cp2;
{
	struct mbuf *mp, *mp2;
	int siz2, xfer;
	caddr_t p;

	mp = *mdp;
	while (left == 0) {
		*mdp = mp = mp->m_next;
		if (mp == NULL)
			return (EBADRPC);
		left = mp->m_len;
		*dposp = mtod(mp, caddr_t);
	}
	if (left >= siz) {
		*cp2 = *dposp;
		*dposp += siz;
	} else if (mp->m_next == NULL) {
		return (EBADRPC);
	} else if (siz > MHLEN) {
		panic("nfs S too big");
	} else {
		MGET(mp2, M_WAIT, MT_DATA);
		mp2->m_next = mp->m_next;
		mp->m_next = mp2;
		mp->m_len -= left;
		mp = mp2;
		*cp2 = p = mtod(mp, caddr_t);
		bcopy(*dposp, p, left);		/* Copy what was left */
		siz2 = siz-left;
		p += left;
		mp2 = mp->m_next;
		/* Loop around copying up the siz2 bytes */
		while (siz2 > 0) {
			if (mp2 == NULL)
				return (EBADRPC);
			xfer = (siz2 > mp2->m_len) ? mp2->m_len : siz2;
			if (xfer > 0) {
				bcopy(mtod(mp2, caddr_t), p, xfer);
				NFSMADV(mp2, xfer);
				mp2->m_len -= xfer;
				p += xfer;
				siz2 -= xfer;
			}
			if (siz2 > 0)
				mp2 = mp2->m_next;
		}
		mp->m_len = siz;
		*mdp = mp2;
		*dposp = mtod(mp2, caddr_t);
	}
	return (0);
}

/*
 * Advance the position in the mbuf chain.
 */
int
nfs_adv(mdp, dposp, offs, left)
	struct mbuf **mdp;
	caddr_t *dposp;
	int offs;
	int left;
{
	struct mbuf *m;
	int s;

	m = *mdp;
	s = left;
	while (s < offs) {
		offs -= s;
		m = m->m_next;
		if (m == NULL)
			return (EBADRPC);
		s = m->m_len;
	}
	*mdp = m;
	*dposp = mtod(m, caddr_t)+offs;
	return (0);
}

/*
 * Copy a string into mbufs for the hard cases...
 */
int
nfsm_strtmbuf(mb, bpos, cp, siz)
	struct mbuf **mb;
	char **bpos;
	char *cp;
	long siz;
{
	struct mbuf *m1 = NULL, *m2;
	long left, xfer, len, tlen;
	u_int32_t *tl;
	int putsize;

	putsize = 1;
	m2 = *mb;
	left = M_TRAILINGSPACE(m2);
	if (left > 0) {
		tl = ((u_int32_t *)(*bpos));
		*tl++ = txdr_unsigned(siz);
		putsize = 0;
		left -= NFSX_UNSIGNED;
		m2->m_len += NFSX_UNSIGNED;
		if (left > 0) {
			bcopy(cp, (caddr_t) tl, left);
			siz -= left;
			cp += left;
			m2->m_len += left;
			left = 0;
		}
	}
	/* Loop around adding mbufs */
	while (siz > 0) {
		MGET(m1, M_WAIT, MT_DATA);
		if (siz > MLEN)
			MCLGET(m1, M_WAIT);
		m1->m_len = NFSMSIZ(m1);
		m2->m_next = m1;
		m2 = m1;
		tl = mtod(m1, u_int32_t *);
		tlen = 0;
		if (putsize) {
			*tl++ = txdr_unsigned(siz);
			m1->m_len -= NFSX_UNSIGNED;
			tlen = NFSX_UNSIGNED;
			putsize = 0;
		}
		if (siz < m1->m_len) {
			len = nfsm_rndup(siz);
			xfer = siz;
			if (xfer < len)
				*(tl+(xfer>>2)) = 0;
		} else {
			xfer = len = m1->m_len;
		}
		bcopy(cp, (caddr_t) tl, xfer);
		m1->m_len = len+tlen;
		siz -= xfer;
		cp += xfer;
	}
	*mb = m1;
	*bpos = mtod(m1, caddr_t)+m1->m_len;
	return (0);
}

/*
 * Called once to initialize data structures...
 */
void
nfs_init()
{
	static struct timeout nfs_timer_to;

	nfsrtt.pos = 0;
	rpc_vers = txdr_unsigned(RPC_VER2);
	rpc_call = txdr_unsigned(RPC_CALL);
	rpc_reply = txdr_unsigned(RPC_REPLY);
	rpc_msgdenied = txdr_unsigned(RPC_MSGDENIED);
	rpc_msgaccepted = txdr_unsigned(RPC_MSGACCEPTED);
	rpc_mismatch = txdr_unsigned(RPC_MISMATCH);
	rpc_autherr = txdr_unsigned(RPC_AUTHERR);
	rpc_auth_unix = txdr_unsigned(RPCAUTH_UNIX);
	rpc_auth_kerb = txdr_unsigned(RPCAUTH_KERB4);
	nfs_prog = txdr_unsigned(NFS_PROG);
	nfs_true = txdr_unsigned(TRUE);
	nfs_false = txdr_unsigned(FALSE);
	nfs_xdrneg1 = txdr_unsigned(-1);
	nfs_ticks = (hz * NFS_TICKINTVL + 500) / 1000;
	if (nfs_ticks < 1)
		nfs_ticks = 1;
#ifdef NFSSERVER
	nfsrv_init(0);			/* Init server data structures */
	nfsrv_initcache();		/* Init the server request cache */
#endif /* NFSSERVER */

	pool_init(&nfsreqpl, sizeof(struct nfsreq), 0, 0, 0, "nfsreqpl",
	    &pool_allocator_nointr);

	/*
	 * Initialize reply list and start timer
	 */
	TAILQ_INIT(&nfs_reqq);

	timeout_set(&nfs_timer_to, nfs_timer, &nfs_timer_to);
	nfs_timer(&nfs_timer_to);

#ifdef NFSCLIENT
	nfs_kqinit();
#endif	
}

#ifdef NFSCLIENT
int
nfs_vfs_init(vfsp)
	struct vfsconf *vfsp;
{
	int i;

	/* Ensure async daemons disabled */
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++)
		nfs_iodwant[i] = (struct proc *)0;
	TAILQ_INIT(&nfs_bufq);
	nfs_nhinit();			/* Init the nfsnode table */

	return (0);
}

/*
 * Attribute cache routines.
 * nfs_loadattrcache() - loads or updates the cache contents from attributes
 *	that are on the mbuf list
 * nfs_getattrcache() - returns valid attributes if found in cache, returns
 *	error otherwise
 */

/*
 * Load the attribute cache (that lives in the nfsnode entry) with
 * the values on the mbuf list and
 * Iff vap not NULL
 *    copy the attributes to *vaper
 */
int
nfs_loadattrcache(vpp, mdp, dposp, vaper)
	struct vnode **vpp;
	struct mbuf **mdp;
	caddr_t *dposp;
	struct vattr *vaper;
{
	struct vnode *vp = *vpp;
	struct vattr *vap;
	struct nfs_fattr *fp;
	extern int (**spec_nfsv2nodeop_p)(void *);
	struct nfsnode *np;
	int32_t t1;
	caddr_t cp2;
	int error = 0;
	int32_t rdev;
	struct mbuf *md;
	enum vtype vtyp;
	mode_t vmode;
	struct timespec mtime;
	struct vnode *nvp;
	int v3 = NFS_ISV3(vp);

	md = *mdp;
	t1 = (mtod(md, caddr_t) + md->m_len) - *dposp;
	error = nfsm_disct(mdp, dposp, NFSX_FATTR(v3), t1, &cp2);
	if (error)
		return (error);
	fp = (struct nfs_fattr *)cp2;
	if (v3) {
		vtyp = nfsv3tov_type(fp->fa_type);
		vmode = fxdr_unsigned(mode_t, fp->fa_mode);
		rdev = makedev(fxdr_unsigned(u_int32_t, fp->fa3_rdev.specdata1),
			fxdr_unsigned(u_int32_t, fp->fa3_rdev.specdata2));
		fxdr_nfsv3time(&fp->fa3_mtime, &mtime);
	} else {
		vtyp = nfsv2tov_type(fp->fa_type);
		vmode = fxdr_unsigned(mode_t, fp->fa_mode);
		if (vtyp == VNON || vtyp == VREG)
			vtyp = IFTOVT(vmode);
		rdev = fxdr_unsigned(int32_t, fp->fa2_rdev);
		fxdr_nfsv2time(&fp->fa2_mtime, &mtime);

		/*
		 * Really ugly NFSv2 kludge.
		 */
		if (vtyp == VCHR && rdev == 0xffffffff)
			vtyp = VFIFO;
	}

	/*
	 * If v_type == VNON it is a new node, so fill in the v_type,
	 * n_mtime fields. Check to see if it represents a special 
	 * device, and if so, check for a possible alias. Once the
	 * correct vnode has been obtained, fill in the rest of the
	 * information.
	 */
	np = VTONFS(vp);
	if (vp->v_type != vtyp) {
		vp->v_type = vtyp;
		if (vp->v_type == VFIFO) {
#ifndef FIFO
			return (EOPNOTSUPP);
#else
			extern int (**fifo_nfsv2nodeop_p)(void *);
			vp->v_op = fifo_nfsv2nodeop_p;
#endif /* FIFO */
		}
		if (vp->v_type == VCHR || vp->v_type == VBLK) {
			vp->v_op = spec_nfsv2nodeop_p;
			nvp = checkalias(vp, (dev_t)rdev, vp->v_mount);
			if (nvp) {
				/*
				 * Discard unneeded vnode, but save its nfsnode.
				 * Since the nfsnode does not have a lock, its
				 * vnode lock has to be carried over.
				 */

				nvp->v_data = vp->v_data;
				vp->v_data = NULL;
				vp->v_op = spec_vnodeop_p;
				vrele(vp);
				vgone(vp);
				/*
				 * Reinitialize aliased node.
				 */
				np->n_vnode = nvp;
				*vpp = vp = nvp;
			}
		}
		np->n_mtime = mtime.tv_sec;
	}
	vap = &np->n_vattr;
	vap->va_type = vtyp;
	vap->va_mode = (vmode & 07777);
	vap->va_rdev = (dev_t)rdev;
	vap->va_mtime = mtime;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	if (v3) {
		vap->va_nlink = fxdr_unsigned(nlink_t, fp->fa_nlink);
		vap->va_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		vap->va_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		vap->va_size = fxdr_hyper(&fp->fa3_size);
		vap->va_blocksize = NFS_FABLKSIZE;
		vap->va_bytes = fxdr_hyper(&fp->fa3_used);
		vap->va_fileid = fxdr_unsigned(int32_t,
		    fp->fa3_fileid.nfsuquad[1]);
		fxdr_nfsv3time(&fp->fa3_atime, &vap->va_atime);
		fxdr_nfsv3time(&fp->fa3_ctime, &vap->va_ctime);
		vap->va_flags = 0;
		vap->va_filerev = 0;
	} else {
		vap->va_nlink = fxdr_unsigned(nlink_t, fp->fa_nlink);
		vap->va_uid = fxdr_unsigned(uid_t, fp->fa_uid);
		vap->va_gid = fxdr_unsigned(gid_t, fp->fa_gid);
		vap->va_size = fxdr_unsigned(u_int32_t, fp->fa2_size);
		vap->va_blocksize = fxdr_unsigned(int32_t, fp->fa2_blocksize);
		vap->va_bytes =
		    (u_quad_t)fxdr_unsigned(int32_t, fp->fa2_blocks) *
		    NFS_FABLKSIZE;
		vap->va_fileid = fxdr_unsigned(int32_t, fp->fa2_fileid);
		fxdr_nfsv2time(&fp->fa2_atime, &vap->va_atime);
		vap->va_flags = 0;
		vap->va_ctime.tv_sec = fxdr_unsigned(u_int32_t,
		    fp->fa2_ctime.nfsv2_sec);
		vap->va_ctime.tv_nsec = 0;
		vap->va_gen = fxdr_unsigned(u_int32_t,fp->fa2_ctime.nfsv2_usec);
		vap->va_filerev = 0;
	}
	if (vap->va_size != np->n_size) {
		if (vap->va_type == VREG) {
			if (np->n_flag & NMODIFIED) {
				if (vap->va_size < np->n_size)
					vap->va_size = np->n_size;
				else
					np->n_size = vap->va_size;
			} else
				np->n_size = vap->va_size;
			uvm_vnp_setsize(vp, np->n_size);
		} else
			np->n_size = vap->va_size;
	}
	np->n_attrstamp = time_second;
	if (vaper != NULL) {
		bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(*vap));
		if (np->n_flag & NCHG) {
			if (np->n_flag & NACC)
				vaper->va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vaper->va_mtime = np->n_mtim;
		}
	}
	return (0);
}

INLINE int
nfs_attrtimeo (np)
	struct nfsnode *np;
{
	struct vnode *vp = np->n_vnode;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int tenthage = (time_second - np->n_mtime) / 10;
	int minto, maxto;

	if (vp->v_type == VDIR) {
		maxto = nmp->nm_acdirmax;
		minto = nmp->nm_acdirmin;
	}
	else {
		maxto = nmp->nm_acregmax;
		minto = nmp->nm_acregmin;
	}

	if (np->n_flag & NMODIFIED || tenthage < minto)
		return minto;
	else if (tenthage < maxto)
		return tenthage;
	else
		return maxto;
}

/*
 * Check the time stamp
 * If the cache is valid, copy contents to *vap and return 0
 * otherwise return an error
 */
int
nfs_getattrcache(vp, vaper)
	struct vnode *vp;
	struct vattr *vaper;
{
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap;

	if (np->n_attrstamp == 0 ||
	    (time_second - np->n_attrstamp) >= nfs_attrtimeo(np)) {
		nfsstats.attrcache_misses++;
		return (ENOENT);
	}
	nfsstats.attrcache_hits++;
	vap = &np->n_vattr;
	if (vap->va_size != np->n_size) {
		if (vap->va_type == VREG) {
			if (np->n_flag & NMODIFIED) {
				if (vap->va_size < np->n_size)
					vap->va_size = np->n_size;
				else
					np->n_size = vap->va_size;
			} else
				np->n_size = vap->va_size;
			uvm_vnp_setsize(vp, np->n_size);
		} else
			np->n_size = vap->va_size;
	}
	bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(struct vattr));
	if (np->n_flag & NCHG) {
		if (np->n_flag & NACC)
			vaper->va_atime = np->n_atim;
		if (np->n_flag & NUPD)
			vaper->va_mtime = np->n_mtim;
	}
	return (0);
}
#endif /* NFSCLIENT */

/*
 * Set up nameidata for a lookup() call and do it
 */
int
nfs_namei(ndp, fhp, len, slp, nam, mdp, dposp, retdirp, p, kerbflag)
	struct nameidata *ndp;
	fhandle_t *fhp;
	int len;
	struct nfssvc_sock *slp;
	struct mbuf *nam;
	struct mbuf **mdp;
	caddr_t *dposp;
	struct vnode **retdirp;
	struct proc *p;
	int kerbflag;
{
	int i, rem;
	struct mbuf *md;
	char *fromcp, *tocp;
	struct vnode *dp;
	int error, rdonly;
	struct componentname *cnp = &ndp->ni_cnd;

	*retdirp = (struct vnode *)0;
	cnp->cn_pnbuf = pool_get(&namei_pool, PR_WAITOK);
	/*
	 * Copy the name from the mbuf list to ndp->ni_pnbuf
	 * and set the various ndp fields appropriately.
	 */
	fromcp = *dposp;
	tocp = cnp->cn_pnbuf;
	md = *mdp;
	rem = mtod(md, caddr_t) + md->m_len - fromcp;
	cnp->cn_hash = 0;
	for (i = 0; i < len; i++) {
		while (rem == 0) {
			md = md->m_next;
			if (md == NULL) {
				error = EBADRPC;
				goto out;
			}
			fromcp = mtod(md, caddr_t);
			rem = md->m_len;
		}
		if (*fromcp == '\0' || *fromcp == '/') {
			error = EACCES;
			goto out;
		}
		cnp->cn_hash += (u_char)*fromcp;
		*tocp++ = *fromcp++;
		rem--;
	}
	*tocp = '\0';
	*mdp = md;
	*dposp = fromcp;
	len = nfsm_rndup(len)-len;
	if (len > 0) {
		if (rem >= len)
			*dposp += len;
		else if ((error = nfs_adv(mdp, dposp, len, rem)) != 0)
			goto out;
	}
	ndp->ni_pathlen = tocp - cnp->cn_pnbuf;
	cnp->cn_nameptr = cnp->cn_pnbuf;
	/*
	 * Extract and set starting directory.
	 */
	error = nfsrv_fhtovp(fhp, FALSE, &dp, ndp->ni_cnd.cn_cred, slp,
	    nam, &rdonly, kerbflag);
	if (error)
		goto out;
	if (dp->v_type != VDIR) {
		vrele(dp);
		error = ENOTDIR;
		goto out;
	}
	VREF(dp);
	*retdirp = dp;
	ndp->ni_startdir = dp;
	if (rdonly)
		cnp->cn_flags |= (NOCROSSMOUNT | RDONLY);
	else
		cnp->cn_flags |= NOCROSSMOUNT;
	/*
	 * And call lookup() to do the real work
	 */
	cnp->cn_proc = p;
	error = lookup(ndp);
	if (error)
		goto out;
	/*
	 * Check for encountering a symbolic link
	 */
	if (cnp->cn_flags & ISSYMLINK) {
		if ((cnp->cn_flags & LOCKPARENT) && ndp->ni_pathlen == 1)
			vput(ndp->ni_dvp);
		else
			vrele(ndp->ni_dvp);
		vput(ndp->ni_vp);
		ndp->ni_vp = NULL;
		error = EINVAL;
		goto out;
	}
	/*
	 * Check for saved name request
	 */
	if (cnp->cn_flags & (SAVENAME | SAVESTART)) {
		cnp->cn_flags |= HASBUF;
		return (0);
	}
out:
	pool_put(&namei_pool, cnp->cn_pnbuf);
	return (error);
}

/*
 * A fiddled version of m_adj() that ensures null fill to a long
 * boundary and only trims off the back end
 */
void
nfsm_adj(mp, len, nul)
	struct mbuf *mp;
	int len;
	int nul;
{
	struct mbuf *m;
	int count, i;
	char *cp;

	/*
	 * Trim from tail.  Scan the mbuf chain,
	 * calculating its length and finding the last mbuf.
	 * If the adjustment only affects this mbuf, then just
	 * adjust and return.  Otherwise, rescan and truncate
	 * after the remaining size.
	 */
	count = 0;
	m = mp;
	for (;;) {
		count += m->m_len;
		if (m->m_next == (struct mbuf *)0)
			break;
		m = m->m_next;
	}
	if (m->m_len > len) {
		m->m_len -= len;
		if (nul > 0) {
			cp = mtod(m, caddr_t)+m->m_len-nul;
			for (i = 0; i < nul; i++)
				*cp++ = '\0';
		}
		return;
	}
	count -= len;
	if (count < 0)
		count = 0;
	/*
	 * Correct length for chain is "count".
	 * Find the mbuf with last data, adjust its length,
	 * and toss data from remaining mbufs on chain.
	 */
	for (m = mp; m; m = m->m_next) {
		if (m->m_len >= count) {
			m->m_len = count;
			if (nul > 0) {
				cp = mtod(m, caddr_t)+m->m_len-nul;
				for (i = 0; i < nul; i++)
					*cp++ = '\0';
			}
			break;
		}
		count -= m->m_len;
	}
	for (m = m->m_next;m;m = m->m_next)
		m->m_len = 0;
}

/*
 * Make these functions instead of macros, so that the kernel text size
 * doesn't get too big...
 */
void
nfsm_srvwcc(nfsd, before_ret, before_vap, after_ret, after_vap, mbp, bposp)
	struct nfsrv_descript *nfsd;
	int before_ret;
	struct vattr *before_vap;
	int after_ret;
	struct vattr *after_vap;
	struct mbuf **mbp;
	char **bposp;
{
	struct mbuf *mb = *mbp, *mb2;
	char *bpos = *bposp;
	u_int32_t *tl;

	if (before_ret) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		nfsm_build(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
		*tl++ = nfs_true;
		txdr_hyper(before_vap->va_size, tl);
		tl += 2;
		txdr_nfsv3time(&(before_vap->va_mtime), tl);
		tl += 2;
		txdr_nfsv3time(&(before_vap->va_ctime), tl);
	}
	*bposp = bpos;
	*mbp = mb;
	nfsm_srvpostopattr(nfsd, after_ret, after_vap, mbp, bposp);
}

void
nfsm_srvpostopattr(nfsd, after_ret, after_vap, mbp, bposp)
	struct nfsrv_descript *nfsd;
	int after_ret;
	struct vattr *after_vap;
	struct mbuf **mbp;
	char **bposp;
{
	struct mbuf *mb = *mbp, *mb2;
	char *bpos = *bposp;
	u_int32_t *tl;
	struct nfs_fattr *fp;

	if (after_ret) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_V3FATTR);
		*tl++ = nfs_true;
		fp = (struct nfs_fattr *)tl;
		nfsm_srvfattr(nfsd, after_vap, fp);
	}
	*mbp = mb;
	*bposp = bpos;
}

void
nfsm_srvfattr(nfsd, vap, fp)
	struct nfsrv_descript *nfsd;
	struct vattr *vap;
	struct nfs_fattr *fp;
{

	fp->fa_nlink = txdr_unsigned(vap->va_nlink);
	fp->fa_uid = txdr_unsigned(vap->va_uid);
	fp->fa_gid = txdr_unsigned(vap->va_gid);
	if (nfsd->nd_flag & ND_NFSV3) {
		fp->fa_type = vtonfsv3_type(vap->va_type);
		fp->fa_mode = vtonfsv3_mode(vap->va_mode);
		txdr_hyper(vap->va_size, &fp->fa3_size);
		txdr_hyper(vap->va_bytes, &fp->fa3_used);
		fp->fa3_rdev.specdata1 = txdr_unsigned(major(vap->va_rdev));
		fp->fa3_rdev.specdata2 = txdr_unsigned(minor(vap->va_rdev));
		fp->fa3_fsid.nfsuquad[0] = 0;
		fp->fa3_fsid.nfsuquad[1] = txdr_unsigned(vap->va_fsid);
		fp->fa3_fileid.nfsuquad[0] = 0;
		fp->fa3_fileid.nfsuquad[1] = txdr_unsigned(vap->va_fileid);
		txdr_nfsv3time(&vap->va_atime, &fp->fa3_atime);
		txdr_nfsv3time(&vap->va_mtime, &fp->fa3_mtime);
		txdr_nfsv3time(&vap->va_ctime, &fp->fa3_ctime);
	} else {
		fp->fa_type = vtonfsv2_type(vap->va_type);
		fp->fa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		fp->fa2_size = txdr_unsigned(vap->va_size);
		fp->fa2_blocksize = txdr_unsigned(vap->va_blocksize);
		if (vap->va_type == VFIFO)
			fp->fa2_rdev = 0xffffffff;
		else
			fp->fa2_rdev = txdr_unsigned(vap->va_rdev);
		fp->fa2_blocks = txdr_unsigned(vap->va_bytes / NFS_FABLKSIZE);
		fp->fa2_fsid = txdr_unsigned(vap->va_fsid);
		fp->fa2_fileid = txdr_unsigned(vap->va_fileid);
		txdr_nfsv2time(&vap->va_atime, &fp->fa2_atime);
		txdr_nfsv2time(&vap->va_mtime, &fp->fa2_mtime);
		txdr_nfsv2time(&vap->va_ctime, &fp->fa2_ctime);
	}
}

/*
 * nfsrv_fhtovp() - convert a fh to a vnode ptr (optionally locked)
 * 	- look up fsid in mount list (if not found ret error)
 *	- get vp and export rights by calling VFS_FHTOVP() and VFS_CHECKEXP()
 *	- if cred->cr_uid == 0 or MNT_EXPORTANON set it to credanon
 *	- if not lockflag unlock it with VOP_UNLOCK()
 */
int
nfsrv_fhtovp(fhp, lockflag, vpp, cred, slp, nam, rdonlyp, kerbflag)
	fhandle_t *fhp;
	int lockflag;
	struct vnode **vpp;
	struct ucred *cred;
	struct nfssvc_sock *slp;
	struct mbuf *nam;
	int *rdonlyp;
	int kerbflag;
{
	struct proc *p = curproc;	/* XXX */
	struct mount *mp;
	int i;
	struct ucred *credanon;
	int error, exflags;
	struct sockaddr_in *saddr;

	*vpp = (struct vnode *)0;
	mp = vfs_getvfs(&fhp->fh_fsid);

	if (!mp)
		return (ESTALE);
	error = VFS_CHECKEXP(mp, nam, &exflags, &credanon);
	if (error)
		return (error);
	error = VFS_FHTOVP(mp, &fhp->fh_fid, vpp);
	if (error)
		return (error);

	saddr = mtod(nam, struct sockaddr_in *);
	if (saddr->sin_family == AF_INET &&
	    (ntohs(saddr->sin_port) >= IPPORT_RESERVED ||
	    (slp->ns_so->so_type == SOCK_STREAM && ntohs(saddr->sin_port) == 20))) {
		vput(*vpp);
		return (NFSERR_AUTHERR | AUTH_TOOWEAK);
	}

	/*
	 * Check/setup credentials.
	 */
	if (exflags & MNT_EXKERB) {
		if (!kerbflag) {
			vput(*vpp);
			return (NFSERR_AUTHERR | AUTH_TOOWEAK);
		}
	} else if (kerbflag) {
		vput(*vpp);
		return (NFSERR_AUTHERR | AUTH_TOOWEAK);
	} else if (cred->cr_uid == 0 || (exflags & MNT_EXPORTANON)) {
		cred->cr_uid = credanon->cr_uid;
		cred->cr_gid = credanon->cr_gid;
		for (i = 0; i < credanon->cr_ngroups && i < NGROUPS; i++)
			cred->cr_groups[i] = credanon->cr_groups[i];
		cred->cr_ngroups = i;
	}
	if (exflags & MNT_EXRDONLY)
		*rdonlyp = 1;
	else
		*rdonlyp = 0;
	if (!lockflag)
		VOP_UNLOCK(*vpp, 0, p);

	return (0);
}

/*
 * This function compares two net addresses by family and returns TRUE
 * if they are the same host.
 * If there is any doubt, return FALSE.
 * The AF_INET family is handled as a special case so that address mbufs
 * don't need to be saved to store "struct in_addr", which is only 4 bytes.
 */
int
netaddr_match(family, haddr, nam)
	int family;
	union nethostaddr *haddr;
	struct mbuf *nam;
{
	struct sockaddr_in *inetaddr;

	switch (family) {
	case AF_INET:
		inetaddr = mtod(nam, struct sockaddr_in *);
		if (inetaddr->sin_family == AF_INET &&
		    inetaddr->sin_addr.s_addr == haddr->had_inetaddr)
			return (1);
		break;
	default:
		break;
	};
	return (0);
}

/*
 * The write verifier has changed (probably due to a server reboot), so all
 * B_NEEDCOMMIT blocks will have to be written again. Since they are on the
 * dirty block list as B_DELWRI, all this takes is clearing the B_NEEDCOMMIT
 * flag. Once done the new write verifier can be set for the mount point.
 */
void
nfs_clearcommit(mp)
	struct mount *mp;
{
	struct vnode *vp, *nvp;
	struct buf *bp, *nbp;
	int s;

	s = splbio();
loop:
	for (vp = LIST_FIRST(&mp->mnt_vnodelist); vp != NULL; vp = nvp) {
		if (vp->v_mount != mp)	/* Paranoia */
			goto loop;
		nvp = LIST_NEXT(vp, v_mntvnodes);
		for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp != NULL; bp = nbp) {
			nbp = LIST_NEXT(bp, b_vnbufs);
			if ((bp->b_flags & (B_BUSY | B_DELWRI | B_NEEDCOMMIT))
				== (B_DELWRI | B_NEEDCOMMIT))
				bp->b_flags &= ~B_NEEDCOMMIT;
		}
	}
	splx(s);
}

void
nfs_merge_commit_ranges(vp)
	struct vnode *vp;
{
	struct nfsnode *np = VTONFS(vp);

	if (!(np->n_commitflags & NFS_COMMIT_PUSHED_VALID)) {
		np->n_pushedlo = np->n_pushlo;
		np->n_pushedhi = np->n_pushhi;
		np->n_commitflags |= NFS_COMMIT_PUSHED_VALID;
	} else {
		if (np->n_pushlo < np->n_pushedlo)
			np->n_pushedlo = np->n_pushlo;
		if (np->n_pushhi > np->n_pushedhi)
			np->n_pushedhi = np->n_pushhi;
	}

	np->n_pushlo = np->n_pushhi = 0;
	np->n_commitflags &= ~NFS_COMMIT_PUSH_VALID;
}

int
nfs_in_committed_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	if (!(np->n_commitflags & NFS_COMMIT_PUSHED_VALID))
		return 0;
	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	return (lo >= np->n_pushedlo && hi <= np->n_pushedhi);
}

int
nfs_in_tobecommitted_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	if (!(np->n_commitflags & NFS_COMMIT_PUSH_VALID))
		return 0;
	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	return (lo >= np->n_pushlo && hi <= np->n_pushhi);
}

void
nfs_add_committed_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	if (!(np->n_commitflags & NFS_COMMIT_PUSHED_VALID)) {
		np->n_pushedlo = lo;
		np->n_pushedhi = hi;
		np->n_commitflags |= NFS_COMMIT_PUSHED_VALID;
	} else {
		if (hi > np->n_pushedhi)
			np->n_pushedhi = hi;
		if (lo < np->n_pushedlo)
			np->n_pushedlo = lo;
	}
}

void
nfs_del_committed_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	if (!(np->n_commitflags & NFS_COMMIT_PUSHED_VALID))
		return;

	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	if (lo > np->n_pushedhi || hi < np->n_pushedlo)
		return;
	if (lo <= np->n_pushedlo)
		np->n_pushedlo = hi;
	else if (hi >= np->n_pushedhi)
		np->n_pushedhi = lo;
	else {
		/*
		 * XXX There's only one range. If the deleted range
		 * is in the middle, pick the largest of the
		 * contiguous ranges that it leaves.
		 */
		if ((np->n_pushedlo - lo) > (hi - np->n_pushedhi))
			np->n_pushedhi = lo;
		else
			np->n_pushedlo = hi;
	}
}

void
nfs_add_tobecommitted_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	if (!(np->n_commitflags & NFS_COMMIT_PUSH_VALID)) {
		np->n_pushlo = lo;
		np->n_pushhi = hi;
		np->n_commitflags |= NFS_COMMIT_PUSH_VALID;
	} else {
		if (lo < np->n_pushlo)
			np->n_pushlo = lo;
		if (hi > np->n_pushhi)
			np->n_pushhi = hi;
	}
}

void
nfs_del_tobecommitted_range(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct nfsnode *np = VTONFS(vp);
	off_t lo, hi;

	if (!(np->n_commitflags & NFS_COMMIT_PUSH_VALID))
		return;

	lo = (off_t)bp->b_blkno * DEV_BSIZE;
	hi = lo + bp->b_dirtyend;

	if (lo > np->n_pushhi || hi < np->n_pushlo)
		return;

	if (lo <= np->n_pushlo)
		np->n_pushlo = hi;
	else if (hi >= np->n_pushhi)
		np->n_pushhi = lo;
	else {
		/*
		 * XXX There's only one range. If the deleted range
		 * is in the middle, pick the largest of the
		 * contiguous ranges that it leaves.
		 */
		if ((np->n_pushlo - lo) > (hi - np->n_pushhi))
			np->n_pushhi = lo;
		else
			np->n_pushlo = hi;
	}
}

/*
 * Map errnos to NFS error numbers. For Version 3 also filter out error
 * numbers not specified for the associated procedure.
 */
int
nfsrv_errmap(nd, err)
	struct nfsrv_descript *nd;
	int err;
{
	short *defaulterrp, *errp;

	if (nd->nd_flag & ND_NFSV3) {
	    if (nd->nd_procnum <= NFSPROC_COMMIT) {
		errp = defaulterrp = nfsrv_v3errmap[nd->nd_procnum];
		while (*++errp) {
			if (*errp == err)
				return (err);
			else if (*errp > err)
				break;
		}
		return ((int)*defaulterrp);
	    } else
		return (err & 0xffff);
	}
	if (err <= ELAST)
		return ((int)nfsrv_v2errmap[err - 1]);
	return (NFSERR_IO);
}

/*
 * Sort the group list in increasing numerical order.
 * (Insertion sort by Chris Torek, who was grossed out by the bubble sort
 *  that used to be here.)
 */
void
nfsrvw_sort(list, num)
        gid_t *list;
        int num;
{
	int i, j;
	gid_t v;

	/* Insertion sort. */
	for (i = 1; i < num; i++) {
		v = list[i];
		/* find correct slot for value v, moving others up */
		for (j = i; --j >= 0 && v < list[j];)
			list[j + 1] = list[j];
		list[j + 1] = v;
	}
}

/*
 * copy credentials making sure that the result can be compared with bcmp().
 */
void
nfsrv_setcred(incred, outcred)
	struct ucred *incred, *outcred;
{
	int i;

	bzero((caddr_t)outcred, sizeof (struct ucred));
	outcred->cr_ref = 1;
	outcred->cr_uid = incred->cr_uid;
	outcred->cr_gid = incred->cr_gid;
	outcred->cr_ngroups = incred->cr_ngroups;
	for (i = 0; i < incred->cr_ngroups; i++)
		outcred->cr_groups[i] = incred->cr_groups[i];
	nfsrvw_sort(outcred->cr_groups, outcred->cr_ngroups);
}
