/*	$OpenBSD: un.h,v 1.8 2003/06/02 23:28:22 millert Exp $	*/
/*	$NetBSD: un.h,v 1.11 1996/02/04 02:12:47 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)un.h	8.1 (Berkeley) 6/2/93
 */

#ifndef _SYS_UN_H_
#define	_SYS_UN_H_

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
	unsigned char	sun_len;	/* sockaddr len including null */
	unsigned char	sun_family;	/* AF_UNIX */
	char	sun_path[104];		/* path name (gag) */
};

#ifdef _KERNEL
struct unpcb;
struct socket;

int	unp_attach(struct socket *so);
int	unp_bind(struct unpcb *unp, struct mbuf *nam, struct proc *p);
int	unp_connect(struct socket *so, struct mbuf *nam, struct proc *p);
int	unp_connect2(struct socket *so, struct socket *so2);
void	unp_detach(struct unpcb *unp);
void	unp_discard(struct file *fp);
void	unp_disconnect(struct unpcb *unp);
void	unp_drop(struct unpcb *unp, int errno);
void	unp_gc(void);
void	unp_mark(struct file *fp);
void	unp_scan(struct mbuf *m0, void (*op)(struct file *), int);
void	unp_shutdown(struct unpcb *unp);
int 	unp_externalize(struct mbuf *);
int	unp_internalize(struct mbuf *, struct proc *);
void 	unp_dispose(struct mbuf *);
#else /* !_KERNEL */

/* actual length of an initialized sockaddr_un */
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif /* _KERNEL */
#endif /* !_SYS_UN_H_ */
