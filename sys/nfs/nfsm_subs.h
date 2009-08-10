/*	$OpenBSD: nfsm_subs.h,v 1.43 2009/08/10 09:18:31 blambert Exp $	*/
/*	$NetBSD: nfsm_subs.h,v 1.10 1996/03/20 21:59:56 fvdl Exp $	*/

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
 *	@(#)nfsm_subs.h	8.2 (Berkeley) 3/30/95
 */


#ifndef _NFS_NFSM_SUBS_H_
#define _NFS_NFSM_SUBS_H_

struct nfsm_info {
	struct mbuf	 *nmi_mreq;
	struct mbuf	 *nmi_mrep;

	struct proc	 *nmi_procp;	/* XXX XXX XXX */
	struct ucred	 *nmi_cred;	/* XXX XXX XXX */

	/* Setting up / Tearing down. */
	struct mbuf	 *nmi_md;
	struct mbuf	 *nmi_mb;
	caddr_t		  nmi_dpos;

	int		  nmi_v3;  
};

#define nfsm_dissect(a, c, s) {						\
	t1 = mtod(info.nmi_md, caddr_t) + info.nmi_md->m_len -		\
	    info.nmi_dpos;						\
	if (t1 >= (s)) {						\
		(a) = (c)(info.nmi_dpos);				\
		info.nmi_dpos += (s);					\
	} else if ((t1 =						\
		  nfsm_disct(&info.nmi_md, &info.nmi_dpos, (s), t1,	\
		      &cp2)) != 0) {					\
		error = t1;						\
		m_freem(info.nmi_mrep);					\
		goto nfsmout;						\
	} else {							\
		(a) = (c)cp2;						\
	}								\
}

#define nfsm_srvpostop_fh(f) {						\
	tl = nfsm_build(&info.nmi_mb, 2 * NFSX_UNSIGNED + NFSX_V3FH);	\
	*tl++ = nfs_true;						\
	*tl++ = txdr_unsigned(NFSX_V3FH);				\
	bcopy((caddr_t)(f), (caddr_t)tl, NFSX_V3FH);			\
}

#define nfsm_mtofh(d, v, v3, f)	{					\
	struct nfsnode *ttnp; nfsfh_t *ttfhp; int ttfhsize;		\
	if (v3) {							\
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);		\
		(f) = fxdr_unsigned(int, *tl);				\
	} else								\
		(f) = 1;						\
	if (f) {							\
		nfsm_getfh(ttfhp, ttfhsize, (v3));			\
		if ((t1 = nfs_nget((d)->v_mount, ttfhp, ttfhsize, 	\
		    &ttnp)) != 0) {					\
			error = t1;					\
			m_freem(info.nmi_mrep);				\
			goto nfsmout;					\
		}							\
		(v) = NFSTOV(ttnp);					\
	}								\
	if (v3) {							\
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);		\
		if (f)							\
			(f) = fxdr_unsigned(int, *tl);			\
		else if (fxdr_unsigned(int, *tl))			\
			nfsm_adv(NFSX_V3FATTR);				\
	}								\
	if (f)								\
		nfsm_loadattr((v), NULL);				\
}

#define nfsm_getfh(f, s, v3) {						\
	if (v3) {							\
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);		\
		if (((s) = fxdr_unsigned(int, *tl)) <= 0 ||		\
			(s) > NFSX_V3FHMAX) {				\
			m_freem(info.nmi_mrep);				\
			error = EBADRPC;				\
			goto nfsmout;					\
		}							\
	} else								\
		(s) = NFSX_V2FH;					\
	nfsm_dissect((f), nfsfh_t *, nfsm_rndup(s)); 			\
}

#define nfsm_loadattr(v, a) {						\
	struct vnode *ttvp = (v);					\
	if ((t1 = nfs_loadattrcache(&ttvp, &info.nmi_md,		\
	    &info.nmi_dpos, (a))) != 0) {				\
		error = t1;						\
		m_freem(info.nmi_mrep);					\
		goto nfsmout;						\
	}								\
	(v) = ttvp;							\
}

#define nfsm_postop_attr(v, f) { if (info.nmi_mrep != NULL) {		\
	struct vnode *ttvp = (v);					\
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);			\
	if (((f) = fxdr_unsigned(int, *tl)) != 0) {			\
		if ((t1 = nfs_loadattrcache(&ttvp, &info.nmi_md,	\
		    &info.nmi_dpos, NULL)) != 0) {			\
			error = t1;					\
			(f) = 0;					\
			m_freem(info.nmi_mrep);				\
			goto nfsmout;					\
		}							\
		(v) = ttvp;						\
	}								\
} }

/* Used as (f) for nfsm_wcc_data() */
#define NFSV3_WCCRATTR	0
#define NFSV3_WCCCHK	1

#define nfsm_wcc_data(v, f) do { if (info.nmi_mrep != NULL) {		\
	struct timespec	 _mtime;					\
	int		 ttattrf, ttretf = 0;				\
									\
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);			\
	if (*tl == nfs_true) {						\
		nfsm_dissect(tl, u_int32_t *, 6 * NFSX_UNSIGNED);	\
		fxdr_nfsv3time(tl + 2, &_mtime);			\
		if (f) {						\
			ttretf = timespeccmp(&VTONFS(v)->n_mtime,	\
			    &_mtime, !=);				\
		}							\
	}								\
	nfsm_postop_attr((v), ttattrf);					\
	if (f) {							\
		(f) = ttretf;						\
	} else {							\
		(f) = ttattrf;						\
	}								\
} } while (0)

#define nfsm_strsiz(s,m) {						\
	nfsm_dissect(tl,u_int32_t *,NFSX_UNSIGNED);			\
	if (((s) = fxdr_unsigned(int32_t,*tl)) > (m)) {			\
		m_freem(info.nmi_mrep);					\
		error = EBADRPC;					\
		goto nfsmout;						\
	}								\
}

#define nfsm_srvnamesiz(s) {						\
	nfsm_dissect(tl,u_int32_t *,NFSX_UNSIGNED);			\
	if (((s) = fxdr_unsigned(int32_t,*tl)) > NFS_MAXNAMLEN) 	\
		error = NFSERR_NAMETOL;					\
	if ((s) <= 0)							\
		error = EBADRPC;					\
	if (error)							\
		nfsm_reply(0);						\
}

#define nfsm_mtouio(p,s)						\
	if ((s) > 0 &&							\
	    (t1 = nfsm_mbuftouio(&info.nmi_md,(p),(s),			\
	        &info.nmi_dpos)) != 0) {				\
		error = t1;						\
		m_freem(info.nmi_mrep);					\
		goto nfsmout;						\
	}

#define nfsm_rndup(a)	(((a)+3)&(~0x3))

#define nfsm_strtom(a,s,m)						\
	if ((s) > (m)) {						\
		m_freem(info.nmi_mreq);					\
		error = ENAMETOOLONG;					\
		goto nfsmout;						\
	}								\
	nfsm_strtombuf(&info.nmi_mb, (a), (s))

#define nfsm_reply(s) {							\
	nfsd->nd_repstat = error;					\
	if (error && !(nfsd->nd_flag & ND_NFSV3))			\
	   (void) nfs_rephead(0, nfsd, slp, error,			\
		&info.nmi_mreq, &info.nmi_mb);				\
	else								\
	   (void) nfs_rephead((s), nfsd, slp, error,			\
		&info.nmi_mreq, &info.nmi_mb);				\
	if (info.nmi_mrep != NULL) {					\
		m_freem(info.nmi_mrep);					\
		info.nmi_mrep = NULL;					\
	}								\
	*mrq = info.nmi_mreq;						\
	if (error && (!(nfsd->nd_flag & ND_NFSV3) || error == EBADRPC))	\
		return(0);						\
}

#define nfsm_writereply(s, v3) {					\
	nfsd->nd_repstat = error;					\
	if (error && !(v3))						\
	   (void) nfs_rephead(0, nfsd, slp, error, &info.nmi_mreq,	\
	       &info.nmi_mb);						\
	else								\
	   (void) nfs_rephead((s), nfsd, slp, error, &info.nmi_mreq,	\
	       &info.nmi_mb);						\
}

#define nfsm_adv(s) {							\
	t1 = mtod(info.nmi_md, caddr_t) + info.nmi_md->m_len -		\
	    info.nmi_dpos;						\
	if (t1 >= (s)) {						\
		info.nmi_dpos += (s);					\
	} else if ((t1 = nfs_adv(&info.nmi_md, &info.nmi_dpos,		\
	      (s), t1)) != 0) {						\
		error = t1;						\
		m_freem(info.nmi_mrep);					\
		goto nfsmout;						\
	}								\
}

#define nfsm_srvmtofh(f) {						\
	if (nfsd->nd_flag & ND_NFSV3) {					\
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);		\
		if (fxdr_unsigned(int, *tl) != NFSX_V3FH) {		\
			error = EBADRPC;				\
			nfsm_reply(0);					\
		}							\
	}								\
	nfsm_dissect(tl, u_int32_t *, NFSX_V3FH);			\
	bcopy((caddr_t)tl, (caddr_t)(f), NFSX_V3FH);			\
	if ((nfsd->nd_flag & ND_NFSV3) == 0)				\
	nfsm_adv(NFSX_V2FH - NFSX_V3FH);				\
}

#endif
