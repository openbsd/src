/*	$OpenBSD: unpcb.h,v 1.17 2019/07/15 12:28:06 bluhm Exp $	*/
/*	$NetBSD: unpcb.h,v 1.6 1994/06/29 06:46:08 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)unpcb.h	8.1 (Berkeley) 6/2/93
 */

/*
 * Protocol control block for an active
 * instance of a UNIX internal protocol.
 *
 * A socket may be associated with an vnode in the
 * file system.  If so, the unp_vnode pointer holds
 * a reference count to this vnode, which should be vrele'd
 * when the socket goes away.
 *
 * A socket may be connected to another socket, in which
 * case the control block of the socket to which it is connected
 * is given by unp_conn.
 *
 * A socket may be referenced by a number of sockets (e.g. several
 * sockets may be connected to a datagram socket.)  These sockets
 * are in a linked list starting with unp_refs, linked through
 * unp_nextref and null-terminated.  Note that a socket may be referenced
 * by a number of other sockets and may also reference a socket (not
 * necessarily one which is referencing it).  This generates
 * the need for unp_refs and unp_nextref to be separate fields.
 *
 * Stream sockets keep copies of receive sockbuf sb_cc and sb_mbcnt
 * so that changes in the sockbuf may be computed to modify
 * back pressure on the sender accordingly.
 */

struct	unpcb {
	struct	socket *unp_socket;	/* pointer back to socket */
	struct	vnode *unp_vnode;	/* if associated with file */
	struct	file *unp_file;		/* backpointer for unp_gc() */
	struct	unpcb *unp_conn;	/* control block of connected socket */
	ino_t	unp_ino;		/* fake inode number */
	SLIST_HEAD(,unpcb) unp_refs;	/* referencing socket linked list */
	SLIST_ENTRY(unpcb) unp_nextref;	/* link in unp_refs list */
	struct	mbuf *unp_addr;		/* bound address of socket */
	long	unp_msgcount;		/* references from socket rcv buf */
	int	unp_flags;		/* this unpcb contains peer eids */
	struct	sockpeercred unp_connid;/* id of peer process */
	struct	timespec unp_ctime;	/* holds creation time */
	LIST_ENTRY(unpcb) unp_link;	/* link in per-AF list of sockets */
};

/*
 * flag bits in unp_flags
 */
#define UNP_FEIDS	0x01		/* unp_connid contains information */
#define UNP_FEIDSBIND	0x02		/* unp_connid was set by a bind */
#define UNP_GCMARK	0x04		/* mark during unp_gc() */
#define UNP_GCDEFER	0x08		/* ref'd, but not marked in this pass */
#define UNP_GCDEAD	0x10		/* unref'd in this pass */

#define	sotounpcb(so)	((struct unpcb *)((so)->so_pcb))

#ifdef _KERNEL
struct fdpass {
	struct file	*fp;
	int		 flags;
};

int	uipc_usrreq(struct socket *, int , struct mbuf *,
			 struct mbuf *, struct mbuf *, struct proc *);
int	uipc_attach(struct socket *, int);
int	uipc_detach(struct socket *);

void	unp_init(void);
int	unp_bind(struct unpcb *, struct mbuf *, struct proc *);
int	unp_connect(struct socket *, struct mbuf *, struct proc *);
int	unp_connect2(struct socket *, struct socket *);
void	unp_detach(struct unpcb *);
void	unp_disconnect(struct unpcb *);
void	unp_drop(struct unpcb *, int);
void	unp_gc(void *);
void	unp_shutdown(struct unpcb *);
int 	unp_externalize(struct mbuf *, socklen_t, int);
int	unp_internalize(struct mbuf *, struct proc *);
void 	unp_dispose(struct mbuf *);
#endif /* _KERNEL */
