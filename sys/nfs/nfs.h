/*	$NetBSD: nfs.h,v 1.10 1996/02/18 11:53:38 fvdl Exp $	*/
/*
 * Copyright (c) 1989, 1993, 1995
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
 *	@(#)nfs.h	8.4 (Berkeley) 5/1/95
 */

#ifndef _NFS_NFS_H_
#define _NFS_NFS_H_

/*
 * Tunable constants for nfs
 */

#define	NFS_MAXIOVEC	34
#define NFS_TICKINTVL	5		/* Desired time for a tick (msec) */
#define NFS_HZ		(hz / nfs_ticks) /* Ticks/sec */
#define	NFS_TIMEO	(1 * NFS_HZ)	/* Default timeout = 1 second */
#define	NFS_MINTIMEO	(1 * NFS_HZ)	/* Min timeout to use */
#define	NFS_MAXTIMEO	(60 * NFS_HZ)	/* Max timeout to backoff to */
#define	NFS_MINIDEMTIMEO (5 * NFS_HZ)	/* Min timeout for non-idempotent ops*/
#define	NFS_MAXREXMIT	100		/* Stop counting after this many */
#define	NFS_MAXWINDOW	1024		/* Max number of outstanding requests */
#define	NFS_RETRANS	10		/* Num of retrans for soft mounts */
#define	NFS_MAXGRPS	16		/* Max. size of groups list */
#ifndef NFS_MINATTRTIMO
#define	NFS_MINATTRTIMO 5		/* Attribute cache timeout in sec */
#endif
#ifndef NFS_MAXATTRTIMO
#define	NFS_MAXATTRTIMO 60
#endif
#define	NFS_WSIZE	8192		/* Def. write data size <= 8192 */
#define	NFS_RSIZE	8192		/* Def. read data size <= 8192 */
#define NFS_READDIRSIZE	8192		/* Def. readdir size */
#define	NFS_DEFRAHEAD	1		/* Def. read ahead # blocks */
#define	NFS_MAXRAHEAD	4		/* Max. read ahead # blocks */
#define	NFS_MAXUIDHASH	64		/* Max. # of hashed uid entries/mp */
#define	NFS_MAXASYNCDAEMON 	20	/* Max. number async_daemons runable */
#define NFS_MAXGATHERDELAY	100	/* Max. write gather delay (msec) */
#ifndef NFS_GATHERDELAY
#define NFS_GATHERDELAY		10	/* Default write gather delay (msec) */
#endif
/*
 * Ideally, NFS_DIRBLKSIZ should be bigger, but I've seen servers with
 * broken NFS/ethernet drivers that won't work with anything bigger (Linux..)
 */
#define	NFS_DIRBLKSIZ	1024		/* Must be a multiple of DIRBLKSIZ */
#define NFS_READDIRBLKSIZ	512	/* Size of read dir blocks. XXX */

/*
 * Oddballs
 */
#define	NMOD(a)		((a) % nfs_asyncdaemons)
#define NFS_CMPFH(n, f, s) \
	((n)->n_fhsize == (s) && !bcmp((caddr_t)(n)->n_fhp, (caddr_t)(f), (s)))
#define NFS_ISV3(v)	(VFSTONFS((v)->v_mount)->nm_flag & NFSMNT_NFSV3)
#define NFS_SRVMAXDATA(n) \
		(((n)->nd_flag & ND_NFSV3) ? (((n)->nd_nam2) ? \
		 NFS_MAXDGRAMDATA : NFS_MAXDATA) : NFS_V2MAXDATA)

/*
 * sys/malloc.h needs M_NFSDIROFF, M_NFSRVDESC and M_NFSBIGFH added.
 * The VA_EXCLUSIVE flag should be added for va_vaflags and set for an
 * exclusive create.
 */
#ifndef M_NFSRVDESC
#define M_NFSRVDESC	M_TEMP
#endif
#ifndef M_NFSDIROFF
#define M_NFSDIROFF	M_TEMP
#endif
#ifndef M_NFSBIGFH
#define M_NFSBIGFH	M_TEMP
#endif
#ifndef VA_EXCLUSIVE
#define VA_EXCLUSIVE	0
#endif

/*
 * The B_INVAFTERWRITE flag should be set to whatever is required by the
 * buffer cache code to say "Invalidate the block after it is written back".
 */
#define	B_INVAFTERWRITE	B_INVAL

/*
 * The IO_METASYNC flag should be implemented for local file systems.
 * (Until then, it is nothin at all.)
 */
#ifndef IO_METASYNC
#define IO_METASYNC	0
#endif

/*
 * Set the attribute timeout based on how recently the file has been modified.
 */
#define	NFS_ATTRTIMEO(np) \
	((((np)->n_flag & NMODIFIED) || \
	 (time.tv_sec - (np)->n_mtime) / 10 < NFS_MINATTRTIMO) ? NFS_MINATTRTIMO : \
	 ((time.tv_sec - (np)->n_mtime) / 10 > NFS_MAXATTRTIMO ? NFS_MAXATTRTIMO : \
	  (time.tv_sec - (np)->n_mtime) / 10))

/*
 * Expected allocation sizes for major data structures. If the actual size
 * of the structure exceeds these sizes, then malloc() will be allocating
 * almost twice the memory required. This is used in nfs_init() to warn
 * the sysadmin that the size of a structure should be reduced.
 * (These sizes are always a power of 2. If the kernel malloc() changes
 *  to one that does not allocate space in powers of 2 size, then this all
 *  becomes bunk!)
 */
#define NFS_NODEALLOC	256
#define NFS_MNTALLOC	512
#define NFS_SVCALLOC	256
#define NFS_UIDALLOC	128

/*
 * Structures for the nfssvc(2) syscall. Not that anyone but nfsd and mount_nfs
 * should ever try and use it.
 */
struct nfsd_args {
	int	sock;		/* Socket to serve */
	caddr_t	name;		/* Client addr for connection based sockets */
	int	namelen;	/* Length of name */
};

struct nfsd_srvargs {
	struct nfsd	*nsd_nfsd;	/* Pointer to in kernel nfsd struct */
	uid_t		nsd_uid;	/* Effective uid mapped to cred */
	u_int32_t	nsd_haddr;	/* Ip address of client */
	struct ucred	nsd_cr;		/* Cred. uid maps to */
	int		nsd_authlen;	/* Length of auth string (ret) */
	u_char		*nsd_authstr;	/* Auth string (ret) */
	int		nsd_verflen;	/* and the verfier */
	u_char		*nsd_verfstr;
	struct timeval	nsd_timestamp;	/* timestamp from verifier */
	u_int32_t	nsd_ttl;	/* credential ttl (sec) */
	NFSKERBKEY_T	nsd_key;	/* Session key */
};

struct nfsd_cargs {
	char		*ncd_dirp;	/* Mount dir path */
	uid_t		ncd_authuid;	/* Effective uid */
	int		ncd_authtype;	/* Type of authenticator */
	int		ncd_authlen;	/* Length of authenticator string */
	u_char		*ncd_authstr;	/* Authenticator string */
	int		ncd_verflen;	/* and the verifier */
	u_char		*ncd_verfstr;
	NFSKERBKEY_T	ncd_key;	/* Session key */
};

/*
 * Stats structure
 */
struct nfsstats {
	int	attrcache_hits;
	int	attrcache_misses;
	int	lookupcache_hits;
	int	lookupcache_misses;
	int	direofcache_hits;
	int	direofcache_misses;
	int	biocache_reads;
	int	read_bios;
	int	read_physios;
	int	biocache_writes;
	int	write_bios;
	int	write_physios;
	int	biocache_readlinks;
	int	readlink_bios;
	int	biocache_readdirs;
	int	readdir_bios;
	int	rpccnt[NFS_NPROCS];
	int	rpcretries;
	int	srvrpccnt[NFS_NPROCS];
	int	srvrpc_errs;
	int	srv_errs;
	int	rpcrequests;
	int	rpctimeouts;
	int	rpcunexpected;
	int	rpcinvalid;
	int	srvcache_inproghits;
	int	srvcache_idemdonehits;
	int	srvcache_nonidemdonehits;
	int	srvcache_misses;
	int	srvnqnfs_leases;
	int	srvnqnfs_maxleases;
	int	srvnqnfs_getleases;
	int	srvvop_writes;
};

/*
 * Flags for nfssvc() system call.
 */
#define	NFSSVC_BIOD	0x002
#define	NFSSVC_NFSD	0x004
#define	NFSSVC_ADDSOCK	0x008
#define	NFSSVC_AUTHIN	0x010
#define	NFSSVC_GOTAUTH	0x040
#define	NFSSVC_AUTHINFAIL 0x080
#define	NFSSVC_MNTD	0x100

/*
 * fs.nfs sysctl(3) identifiers
 */
#define NFS_NFSSTATS	1		/* struct: struct nfsstats */

#define FS_NFS_NAMES { \
		       { 0, 0 }, \
		       { "nfsstats", CTLTYPE_STRUCT }, \
}

/*
 * The set of signals the interrupt an I/O in progress for NFSMNT_INT mounts.
 * What should be in this set is open to debate, but I believe that since
 * I/O system calls on ufs are never interrupted by signals the set should
 * be minimal. My reasoning is that many current programs that use signals
 * such as SIGALRM will not expect file I/O system calls to be interrupted
 * by them and break.
 */
#ifdef _KERNEL

struct uio; struct buf; struct vattr; struct nameidata;	/* XXX */

#define	NFSINT_SIGMASK	(sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
			 sigmask(SIGHUP)|sigmask(SIGQUIT))

/*
 * Socket errors ignored for connectionless sockets??
 * For now, ignore them all
 */
#define	NFSIGNORE_SOERROR(s, e) \
		((e) != EINTR && (e) != ERESTART && (e) != EWOULDBLOCK && \
		((s) & PR_CONNREQUIRED) == 0)

/*
 * Nfs outstanding request list element
 */
struct nfsreq {
	TAILQ_ENTRY(nfsreq) r_chain;
	struct mbuf	*r_mreq;
	struct mbuf	*r_mrep;
	struct mbuf	*r_md;
	caddr_t		r_dpos;
	struct nfsmount *r_nmp;
	struct vnode	*r_vp;
	u_int32_t	r_xid;
	int		r_flags;	/* flags on request, see below */
	int		r_retry;	/* max retransmission count */
	int		r_rexmit;	/* current retrans count */
	int		r_timer;	/* tick counter on reply */
	int		r_procnum;	/* NFS procedure number */
	int		r_rtt;		/* RTT for rpc */
	struct proc	*r_procp;	/* Proc that did I/O system call */
};

/*
 * Queue head for nfsreq's
 */
TAILQ_HEAD(, nfsreq) nfs_reqq;

/* Flag values for r_flags */
#define R_TIMING	0x01		/* timing request (in mntp) */
#define R_SENT		0x02		/* request has been sent */
#define	R_SOFTTERM	0x04		/* soft mnt, too many retries */
#define	R_INTR		0x08		/* intr mnt, signal pending */
#define	R_SOCKERR	0x10		/* Fatal error on socket */
#define	R_TPRINTFMSG	0x20		/* Did a tprintf msg. */
#define	R_MUSTRESEND	0x40		/* Must resend request */
#define	R_GETONEREP	0x80		/* Probe for one reply only */

/*
 * A list of nfssvc_sock structures is maintained with all the sockets
 * that require service by the nfsd.
 * The nfsuid structs hang off of the nfssvc_sock structs in both lru
 * and uid hash lists.
 */
#ifndef NFS_UIDHASHSIZ
#define	NFS_UIDHASHSIZ	29	/* Tune the size of nfssvc_sock with this */
#endif
#define	NUIDHASH(sock, uid) \
	(&(sock)->ns_uidhashtbl[(uid) % NFS_UIDHASHSIZ])
#ifndef NFS_WDELAYHASHSIZ
#define	NFS_WDELAYHASHSIZ 16	/* and with this */
#endif
#define	NWDELAYHASH(sock, f) \
	(&(sock)->ns_wdelayhashtbl[(*((u_long *)(f))) % NFS_WDELAYHASHSIZ])
#ifndef NFS_MUIDHASHSIZ
#define NFS_MUIDHASHSIZ	67	/* Tune the size of nfsmount with this */
#endif
#define	NMUIDHASH(nmp, uid) \
	(&(nmp)->nm_uidhashtbl[(uid) % NFS_MUIDHASHSIZ])
#define	NFSNOHASH(fhsum) \
	(&nfsnodehashtbl[(fhsum) & nfsnodehash])

/*
 * Network address hash list element
 */
union nethostaddr {
	u_int32_t had_inetaddr;
	struct mbuf *had_nam;
};

struct nfsuid {
	TAILQ_ENTRY(nfsuid) nu_lru;	/* LRU chain */
	LIST_ENTRY(nfsuid) nu_hash;	/* Hash list */
	int		nu_flag;	/* Flags */
	union nethostaddr nu_haddr;	/* Host addr. for dgram sockets */
	struct ucred	nu_cr;		/* Cred uid mapped to */
	int		nu_expire;	/* Expiry time (sec) */
	struct timeval	nu_timestamp;	/* Kerb. timestamp */
	u_int32_t	nu_nickname;	/* Nickname on server */
	NFSKERBKEY_T	nu_key;		/* and session key */
};

#define	nu_inetaddr	nu_haddr.had_inetaddr
#define	nu_nam		nu_haddr.had_nam
/* Bits for nu_flag */
#define	NU_INETADDR	0x1
#define NU_NAM		0x2
#define NU_NETFAM(u)	(((u)->nu_flag & NU_INETADDR) ? AF_INET : AF_ISO)

struct nfssvc_sock {
	TAILQ_ENTRY(nfssvc_sock) ns_chain;	/* List of all nfssvc_sock's */
	TAILQ_HEAD(, nfsuid) ns_uidlruhead;
	struct file	*ns_fp;
	struct socket	*ns_so;
	struct mbuf	*ns_nam;
	struct mbuf	*ns_raw;
	struct mbuf	*ns_rawend;
	struct mbuf	*ns_rec;
	struct mbuf	*ns_recend;
	struct mbuf	*ns_frag;
	int		ns_flag;
	int		ns_solock;
	int		ns_cc;
	int		ns_reclen;
	int		ns_numuids;
	u_int32_t	ns_sref;
	LIST_HEAD(, nfsrv_descript) ns_tq;	/* Write gather lists */
	LIST_HEAD(, nfsuid) ns_uidhashtbl[NFS_UIDHASHSIZ];
	LIST_HEAD(nfsrvw_delayhash, nfsrv_descript) ns_wdelayhashtbl[NFS_WDELAYHASHSIZ];
};

/* Bits for "ns_flag" */
#define	SLP_VALID	0x01
#define	SLP_DOREC	0x02
#define	SLP_NEEDQ	0x04
#define	SLP_DISCONN	0x08
#define	SLP_GETSTREAM	0x10
#define	SLP_LASTFRAG	0x20
#define SLP_ALLFLAGS	0xff

TAILQ_HEAD(, nfssvc_sock) nfssvc_sockhead;
int nfssvc_sockhead_flag;
#define	SLP_INIT	0x01
#define	SLP_WANTINIT	0x02

/*
 * One of these structures is allocated for each nfsd.
 */
struct nfsd {
	TAILQ_ENTRY(nfsd) nfsd_chain;	/* List of all nfsd's */
	int		nfsd_flag;	/* NFSD_ flags */
	struct nfssvc_sock *nfsd_slp;	/* Current socket */
	int		nfsd_authlen;	/* Authenticator len */
	u_char		nfsd_authstr[RPCAUTH_MAXSIZ]; /* Authenticator data */
	int		nfsd_verflen;	/* and the Verifier */
	u_char		nfsd_verfstr[RPCVERF_MAXSIZ];
	struct proc	*nfsd_procp;	/* Proc ptr */
	struct nfsrv_descript *nfsd_nd;	/* Associated nfsrv_descript */
};

/* Bits for "nfsd_flag" */
#define	NFSD_WAITING	0x01
#define	NFSD_REQINPROG	0x02
#define	NFSD_NEEDAUTH	0x04
#define	NFSD_AUTHFAIL	0x08

/*
 * This structure is used by the server for describing each request.
 * Some fields are used only when write request gathering is performed.
 */
struct nfsrv_descript {
	u_quad_t		nd_time;	/* Write deadline (usec) */
	off_t			nd_off;		/* Start byte offset */
	off_t			nd_eoff;	/* and end byte offset */
	LIST_ENTRY(nfsrv_descript) nd_hash;	/* Hash list */
	LIST_ENTRY(nfsrv_descript) nd_tq;		/* and timer list */
	LIST_HEAD(,nfsrv_descript) nd_coalesce;	/* coalesced writes */
	struct mbuf		*nd_mrep;	/* Request mbuf list */
	struct mbuf		*nd_md;		/* Current dissect mbuf */
	struct mbuf		*nd_mreq;	/* Reply mbuf list */
	struct mbuf		*nd_nam;	/* and socket addr */
	struct mbuf		*nd_nam2;	/* return socket addr */
	caddr_t			nd_dpos;	/* Current dissect pos */
	int			nd_procnum;	/* RPC # */
	int			nd_stable;	/* storage type */
	int			nd_flag;	/* nd_flag */
	int			nd_len;		/* Length of this write */
	int			nd_repstat;	/* Reply status */
	u_int32_t		nd_retxid;	/* Reply xid */
	u_int32_t		nd_duration;	/* Lease duration */
	struct timeval		nd_starttime;	/* Time RPC initiated */
	fhandle_t		nd_fh;		/* File handle */
	struct ucred		nd_cr;		/* Credentials */
};

/* Bits for "nd_flag" */
#define	ND_READ		LEASE_READ
#define ND_WRITE	LEASE_WRITE
#define ND_CHECK	0x04
#define ND_LEASE	(ND_READ | ND_WRITE | ND_CHECK)
#define ND_NFSV3	0x08
#define ND_NQNFS	0x10
#define ND_KERBNICK	0x20
#define ND_KERBFULL	0x40
#define ND_KERBAUTH	(ND_KERBNICK | ND_KERBFULL)

TAILQ_HEAD(, nfsd) nfsd_head;
int nfsd_head_flag;
#define	NFSD_CHECKSLP	0x01

/*
 * These macros compare nfsrv_descript structures.
 */
#define NFSW_CONTIG(o, n) \
		((o)->nd_eoff >= (n)->nd_off && \
		 !bcmp((caddr_t)&(o)->nd_fh, (caddr_t)&(n)->nd_fh, NFSX_V3FH))

#define NFSW_SAMECRED(o, n) \
	(((o)->nd_flag & ND_KERBAUTH) == ((n)->nd_flag & ND_KERBAUTH) && \
 	 !bcmp((caddr_t)&(o)->nd_cr, (caddr_t)&(n)->nd_cr, \
		sizeof (struct ucred)))

#endif	/* _KERNEL */

#endif /* _NFS_NFS_H */
