/*	$OpenBSD: socketvar.h,v 1.88 2018/11/19 13:15:37 visa Exp $	*/
/*	$NetBSD: socketvar.h,v 1.18 1996/02/09 18:25:38 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)socketvar.h	8.1 (Berkeley) 6/2/93
 */

#include <sys/selinfo.h>			/* for struct selinfo */
#include <sys/queue.h>
#include <sys/sigio.h>				/* for struct sigio_ref */
#include <sys/task.h>
#include <sys/timeout.h>

#ifndef	_SOCKLEN_T_DEFINED_
#define	_SOCKLEN_T_DEFINED_
typedef	__socklen_t	socklen_t;	/* length type for network syscalls */
#endif

TAILQ_HEAD(soqhead, socket);

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct socket {
	const struct protosw *so_proto;	/* protocol handle */
	void	*so_pcb;		/* protocol control block */
	u_int	so_state;		/* internal state flags SS_*, below */
	short	so_type;		/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
/*
 * Variables for connection queueing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket	*so_head;	/* back pointer to accept socket */
	struct	soqhead	*so_onq;	/* queue (q or q0) that we're on */
	struct	soqhead	so_q0;		/* queue of partial connections */
	struct	soqhead	so_q;		/* queue of incoming connections */
	struct	sigio_ref so_sigio;	/* async I/O registration */
	TAILQ_ENTRY(socket) so_qe;	/* our queue entry (q or q0) */
	short	so_q0len;		/* partials on so_q0 */
	short	so_qlen;		/* number of connections on so_q */
	short	so_qlimit;		/* max number queued connections */
	short	so_timeo;		/* connection timeout */
	u_long	so_oobmark;		/* chars to oob mark */
	u_int	so_error;		/* error affecting connection */
/*
 * Variables for socket splicing, allocated only when needed.
 */
	struct sosplice {
		struct	socket *ssp_socket;	/* send data to drain socket */
		struct	socket *ssp_soback;	/* back ref to source socket */
		off_t	ssp_len;		/* number of bytes spliced */
		off_t	ssp_max;		/* maximum number of bytes */
		struct	timeval ssp_idletv;	/* idle timeout */
		struct	timeout ssp_idleto;
		struct	task ssp_task;		/* task for somove */
	} *so_sp;
/*
 * Variables for socket buffering.
 */
	struct	sockbuf {
/* The following fields are all zeroed on flush. */
#define	sb_startzero	sb_cc
		u_long	sb_cc;		/* actual chars in buffer */
		u_long	sb_datacc;	/* data only chars in buffer */
		u_long	sb_hiwat;	/* max actual char count */
		u_long  sb_wat;		/* default watermark */
		u_long	sb_mbcnt;	/* chars of mbufs used */
		u_long	sb_mbmax;	/* max chars of mbufs to use */
		long	sb_lowat;	/* low water mark */
		struct mbuf *sb_mb;	/* the mbuf chain */
		struct mbuf *sb_mbtail;	/* the last mbuf in the chain */
		struct mbuf *sb_lastrecord;/* first mbuf of last record in
					      socket buffer */
		u_short	sb_timeo;	/* timeout for read/write */
		short	sb_flags;	/* flags, see below */
/* End area that is zeroed on flush. */
#define	sb_endzero	sb_flags
		int	sb_flagsintr;	/* flags, changed atomically */
		struct	selinfo sb_sel;	/* process selecting read/write */
	} so_rcv, so_snd;
#define	SB_MAX		(2*1024*1024)	/* default for max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC	0x10		/* ASYNC I/O, need signals */
#define	SB_SPLICE	0x20		/* buffer is splice source or drain */
#define	SB_NOINTR	0x40		/* operations not interruptible */
#define	SB_KNOTE	0x80		/* kernel note attached */

	void	(*so_upcall)(struct socket *so, caddr_t arg, int waitf);
	caddr_t	so_upcallarg;		/* Arg for above */
	uid_t	so_euid, so_ruid;	/* who opened the socket */
	gid_t	so_egid, so_rgid;
	pid_t	so_cpid;		/* pid of process that opened socket */
};

/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */
#define	SS_ISDISCONNECTED	0x800	/* socket disconnected from peer */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_ASYNC		0x200	/* async i/o notify */
#define	SS_CONNECTOUT		0x1000	/* connect, not accept, at this end */
#define	SS_ISSENDING		0x2000	/* hint for lower layer */
#define	SS_DNS			0x4000	/* created using SOCK_DNS socket(2) */

#ifdef _KERNEL

#include <lib/libkern/libkern.h>

/*
 * Values for sounlock()/sofree().
 */
#define SL_NOUNLOCK	0x00
#define SL_LOCKED	0x42

void	soassertlocked(struct socket *);

/*
 * Macros for sockets and socket buffering.
 */

#define isspliced(so)		((so)->so_sp && (so)->so_sp->ssp_socket)
#define issplicedback(so)	((so)->so_sp && (so)->so_sp->ssp_soback)

/*
 * Do we need to notify the other side when I/O is possible?
 */
static inline int
sb_notify(struct socket *so, struct sockbuf *sb)
{
	int flags = (sb->sb_flags | sb->sb_flagsintr);

	KASSERT(sb == &so->so_rcv || sb == &so->so_snd);
	soassertlocked(so);
	return ((flags & (SB_WAIT|SB_SEL|SB_ASYNC|SB_SPLICE|SB_KNOTE)) != 0);
}

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.
 */
static inline long
sbspace(struct socket *so, struct sockbuf *sb)
{
	KASSERT(sb == &so->so_rcv || sb == &so->so_snd);
	return lmin(sb->sb_hiwat - sb->sb_cc, sb->sb_mbmax - sb->sb_mbcnt);
}

/* do we have to send all at once on a socket? */
#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* are we sending on this socket? */
#define	soissending(so) \
    ((so)->so_state & SS_ISSENDING)

/* can we read something from so? */
static inline int
soreadable(struct socket *so)
{
	soassertlocked(so);
	if (isspliced(so))
		return 0;
	return (so->so_state & SS_CANTRCVMORE) || so->so_qlen || so->so_error ||
	    so->so_rcv.sb_cc >= so->so_rcv.sb_lowat;
}

/* can we write something to so? */
#define	sowriteable(so) \
    ((sbspace((so), &(so)->so_snd) >= (so)->so_snd.sb_lowat && \
	(((so)->so_state & SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags & PR_CONNREQUIRED)==0)) || \
    ((so)->so_state & SS_CANTSENDMORE) || (so)->so_error)

/* adjust counters in sb reflecting allocation of m */
#define	sballoc(sb, m) do {						\
	(sb)->sb_cc += (m)->m_len;					\
	if ((m)->m_type != MT_CONTROL && (m)->m_type != MT_SONAME)	\
		(sb)->sb_datacc += (m)->m_len;				\
	(sb)->sb_mbcnt += MSIZE;					\
	if ((m)->m_flags & M_EXT)					\
		(sb)->sb_mbcnt += (m)->m_ext.ext_size;			\
} while (/* CONSTCOND */ 0)

/* adjust counters in sb reflecting freeing of m */
#define	sbfree(sb, m) do {						\
	(sb)->sb_cc -= (m)->m_len;					\
	if ((m)->m_type != MT_CONTROL && (m)->m_type != MT_SONAME)	\
		(sb)->sb_datacc -= (m)->m_len;				\
	(sb)->sb_mbcnt -= MSIZE;					\
	if ((m)->m_flags & M_EXT)					\
		(sb)->sb_mbcnt -= (m)->m_ext.ext_size;			\
} while (/* CONSTCOND */ 0)

/*
 * Set lock on sockbuf sb; sleep if lock is already held.
 * Unless SB_NOINTR is set on sockbuf, sleep is interruptible.
 * Returns error without lock if sleep is interrupted.
 */
int sblock(struct socket *, struct sockbuf *, int);

/* release lock on sockbuf sb */
void sbunlock(struct socket *, struct sockbuf *);

#define	SB_EMPTY_FIXUP(sb) do {						\
	if ((sb)->sb_mb == NULL) {					\
		(sb)->sb_mbtail = NULL;					\
		(sb)->sb_lastrecord = NULL;				\
	}								\
} while (/*CONSTCOND*/0)

extern u_long sb_max;

extern struct pool	socket_pool;

struct mbuf;
struct sockaddr;
struct proc;
struct msghdr;
struct stat;
struct knote;

/*
 * File operations on sockets.
 */
int	soo_read(struct file *, struct uio *, int);
int	soo_write(struct file *, struct uio *, int);
int	soo_ioctl(struct file *, u_long, caddr_t, struct proc *);
int	soo_poll(struct file *, int events, struct proc *);
int	soo_kqfilter(struct file *, struct knote *);
int 	soo_close(struct file *, struct proc *);
int	soo_stat(struct file *, struct stat *, struct proc *);
void	sbappend(struct socket *, struct sockbuf *, struct mbuf *);
void	sbappendstream(struct socket *, struct sockbuf *, struct mbuf *);
int	sbappendaddr(struct socket *, struct sockbuf *,
	    const struct sockaddr *, struct mbuf *, struct mbuf *);
int	sbappendcontrol(struct socket *, struct sockbuf *, struct mbuf *,
	    struct mbuf *);
void	sbappendrecord(struct socket *, struct sockbuf *, struct mbuf *);
void	sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n);
struct mbuf *
	sbcreatecontrol(caddr_t p, int size, int type, int level);
void	sbdrop(struct socket *, struct sockbuf *, int);
void	sbdroprecord(struct sockbuf *sb);
void	sbflush(struct socket *, struct sockbuf *);
void	sbinsertoob(struct sockbuf *sb, struct mbuf *m0);
void	sbrelease(struct socket *, struct sockbuf *);
int	sbcheckreserve(u_long cnt, u_long defcnt);
int	sbchecklowmem(void);
int	sbreserve(struct socket *, struct sockbuf *, u_long);
int	sbwait(struct socket *, struct sockbuf *sb);
int	sb_lock(struct sockbuf *sb);
void	soinit(void);
int	soabort(struct socket *so);
int	soaccept(struct socket *so, struct mbuf *nam);
int	sobind(struct socket *so, struct mbuf *nam, struct proc *p);
void	socantrcvmore(struct socket *so);
void	socantsendmore(struct socket *so);
int	soclose(struct socket *, int);
int	soconnect(struct socket *so, struct mbuf *nam);
int	soconnect2(struct socket *so1, struct socket *so2);
int	socreate(int dom, struct socket **aso, int type, int proto);
int	sodisconnect(struct socket *so);
void	sofree(struct socket *so, int);
int	sogetopt(struct socket *so, int level, int optname, struct mbuf *m);
void	sohasoutofband(struct socket *so);
void	soisconnected(struct socket *so);
void	soisconnecting(struct socket *so);
void	soisdisconnected(struct socket *so);
void	soisdisconnecting(struct socket *so);
int	solisten(struct socket *so, int backlog);
struct socket *sonewconn(struct socket *head, int connstatus);
void	soqinsque(struct socket *head, struct socket *so, int q);
int	soqremque(struct socket *so, int q);
int	soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
	    struct mbuf **mp0, struct mbuf **controlp, int *flagsp,
	    socklen_t controllen);
int	soreserve(struct socket *so, u_long sndcc, u_long rcvcc);
void	sorflush(struct socket *so);
int	sosend(struct socket *so, struct mbuf *addr, struct uio *uio,
	    struct mbuf *top, struct mbuf *control, int flags);
int	sosetopt(struct socket *so, int level, int optname, struct mbuf *m);
int	soshutdown(struct socket *so, int how);
void	sowakeup(struct socket *so, struct sockbuf *sb);
void	sorwakeup(struct socket *);
void	sowwakeup(struct socket *);
int	sockargs(struct mbuf **, const void *, size_t, int);

int	sosleep(struct socket *, void *, int, const char *, int);
int	solock(struct socket *);
void	sounlock(struct socket *, int);

int	sendit(struct proc *, int, struct msghdr *, int, register_t *);
int	recvit(struct proc *, int, struct msghdr *, caddr_t,
		    register_t *);
int	doaccept(struct proc *, int, struct sockaddr *, socklen_t *, int,
	    register_t *);

#ifdef SOCKBUF_DEBUG
void	sblastrecordchk(struct sockbuf *, const char *);
#define	SBLASTRECORDCHK(sb, where)	sblastrecordchk((sb), (where))

void	sblastmbufchk(struct sockbuf *, const char *);
#define	SBLASTMBUFCHK(sb, where)	sblastmbufchk((sb), (where))
void	sbcheck(struct sockbuf *sb);
#define	SBCHECK(sb)			sbcheck(sb)
#else
#define	SBLASTRECORDCHK(sb, where)	/* nothing */
#define	SBLASTMBUFCHK(sb, where)	/* nothing */
#define	SBCHECK(sb)			/* nothing */
#endif /* SOCKBUF_DEBUG */

#endif /* _KERNEL */
