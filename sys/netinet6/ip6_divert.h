/*      $OpenBSD: ip6_divert.h,v 1.1 2009/11/05 20:50:14 michele Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
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

#ifndef _IP6_DIVERT_H_
#define _IP6_DIVERT_H_

struct div6stat {
	u_long	divs_ipackets;	/* total input packets */
	u_long	divs_noport;	/* no socket on port */
	u_long	divs_fullsock;	/* not delivered, input socket full */
	u_long	divs_opackets;	/* total output packets */
	u_long	divs_errors;	/* generic errors */
};

/*
 * Names for divert sysctl objects
 */
#define	DIVERT6CTL_RECVSPACE	1	/* receive buffer space */
#define	DIVERT6CTL_SENDSPACE	2	/* send buffer space */
#define	DIVERT6CTL_STATS	3	/* divert statistics */
#define	DIVERT6CTL_MAXID	4

#define	DIVERT6CTL_NAMES { \
	{ 0, 0 }, \
	{ "recvspace",	CTLTYPE_INT }, \
	{ "sendspace",	CTLTYPE_INT }, \
	{ "stats",	CTLTYPE_STRUCT } \
}

#define	DIVERT6CTL_VARS { \
	NULL, \
	&divert6_recvspace, \
	&divert6_sendspace, \
	NULL \
}

#ifdef _KERNEL
extern struct	inpcbtable	divb6table;
extern struct	div6stat		div6stat;

void	 divert6_init(void);
int	 divert6_input(struct mbuf **, int *, int);
void	 divert6_packet(struct mbuf *, int);
int	 divert6_output(struct mbuf *, ...);
int	 divert6_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	 divert6_usrreq(struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *, struct proc *);

#endif /* _KERNEL */
#endif /* _IP6_DIVERT_H_ */
