/*	$OpenBSD: netstat.c,v 1.25 2004/04/26 19:22:30 itojun Exp $	*/
/*	$NetBSD: netstat.c,v 1.3 1995/06/18 23:53:07 cgd Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)netstat.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: netstat.c,v 1.25 2004/04/26 19:22:30 itojun Exp $";
#endif /* not lint */

/*
 * netstat
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <netinet/in.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <nlist.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

static void enter(struct inpcb *, struct socket *, int, char *);
static const char *inetname(struct in_addr);
static void inetprint(struct in_addr *, int, char *);
#ifdef INET6
static const char *inet6name(struct in6_addr *);
static void inet6print(struct in6_addr *, int, char *);
#endif

#define	streq(a,b)	(strcmp(a,b)==0)
#define	YMAX(w)		((w)->_maxy-1)

WINDOW *
opennetstat(void)
{
	sethostent(1);
	setnetent(1);
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

struct netinfo {
	struct	netinfo *nif_forw, *nif_prev;
	int	nif_family;
	short	nif_line;		/* line on screen */
	short	nif_seen;		/* 0 when not present in list */
	short	nif_flags;
#define	NIF_LACHG	0x1		/* local address changed */
#define	NIF_FACHG	0x2		/* foreign address changed */
	short	nif_state;		/* tcp state */
	char	*nif_proto;		/* protocol */
	struct	in_addr nif_laddr;	/* local address */
#ifdef INET6
	struct	in6_addr nif_laddr6;	/* local address */
#endif
	long	nif_lport;		/* local port */
	struct	in_addr	nif_faddr;	/* foreign address */
#ifdef INET6
	struct	in6_addr nif_faddr6;	/* foreign address */
#endif
	long	nif_fport;		/* foreign port */
	long	nif_rcvcc;		/* rcv buffer character count */
	long	nif_sndcc;		/* snd buffer character count */
};

static struct {
	struct	netinfo *nif_forw, *nif_prev;
} netcb;

static	int aflag = 0;
static	int lastrow = 1;

void
closenetstat(WINDOW *w)
{
	struct netinfo *p;

	endhostent();
	endnetent();
	p = (struct netinfo *)netcb.nif_forw;
	while (p != (struct netinfo *)&netcb) {
		if (p->nif_line != -1)
			lastrow--;
		p->nif_line = -1;
		p = p->nif_forw;
	}
	if (w != NULL) {
		wclear(w);
		wrefresh(w);
		delwin(w);
	}
}

static struct nlist namelist[] = {
#define	X_TCBTABLE	0		/* no sysctl */
	{ "_tcbtable" },
#define	X_UDBTABLE	1		/* no sysctl */
	{ "_udbtable" },
	{ "" },
};

int
initnetstat(void)
{
	int ret;

	if ((ret = kvm_nlist(kd, namelist)) == -1)
		errx(1, "%s", kvm_geterr(kd));
	else if (ret)
		nlisterr(namelist);
	if (namelist[X_TCBTABLE].n_value == 0) {
		error("No symbols in namelist");
		return(0);
	}
	netcb.nif_forw = netcb.nif_prev = (struct netinfo *)&netcb;
	protos = TCP|UDP;
	return(1);
}

void
fetchnetstat(void)
{
	struct inpcbtable pcbtable;
	struct inpcb *head, *prev, *next;
	struct netinfo *p;
	struct inpcb inpcb;
	struct socket sockb;
	struct tcpcb tcpcb;
	void *off;
	int istcp;

	if (namelist[X_TCBTABLE].n_value == 0)
		return;
	for (p = netcb.nif_forw; p != (struct netinfo *)&netcb; p = p->nif_forw)
		p->nif_seen = 0;
	if (protos&TCP) {
		off = NPTR(X_TCBTABLE);
		istcp = 1;
	} else if (protos&UDP) {
		off = NPTR(X_UDBTABLE);
		istcp = 0;
	} else {
		error("No protocols to display");
		return;
	}
again:
	KREAD(off, &pcbtable, sizeof (struct inpcbtable));
	prev = head = (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue;
	next = pcbtable.inpt_queue.cqh_first;
	while (next != head) {
		KREAD(next, &inpcb, sizeof (inpcb));
		if (inpcb.inp_queue.cqe_prev != prev) {
			printf("prev = %p, head = %p, next = %p, inpcb...prev = %p\n",
			    prev, head, next, inpcb.inp_queue.cqe_prev);
			p = netcb.nif_forw;
			for (; p != (struct netinfo *)&netcb; p = p->nif_forw)
				p->nif_seen = 1;
			error("Kernel state in transition");
			return;
		}
		prev = next;
		next = inpcb.inp_queue.cqe_next;

#ifndef INET6
		if (inpcb.inp_flags & INP_IPV6)
			continue;
#endif

		if (!aflag) {
			if (!(inpcb.inp_flags & INP_IPV6) &&
			    inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
				continue;
#ifdef INET6
			if ((inpcb.inp_flags & INP_IPV6) &&
			    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6))
				continue;
#endif
		}
		if (nhosts && !checkhost(&inpcb))
			continue;
		if (nports && !checkport(&inpcb))
			continue;
		KREAD(inpcb.inp_socket, &sockb, sizeof (sockb));
		if (istcp) {
			KREAD(inpcb.inp_ppcb, &tcpcb, sizeof (tcpcb));
			enter(&inpcb, &sockb, tcpcb.t_state, "tcp");
		} else
			enter(&inpcb, &sockb, 0, "udp");
	}
	if (istcp && (protos&UDP)) {
		istcp = 0;
		off = NPTR(X_UDBTABLE);
		goto again;
	}
}

static void
enter(struct inpcb *inp, struct socket *so, int state, char *proto)
{
	struct netinfo *p;

	/*
	 * Only take exact matches, any sockets with
	 * previously unbound addresses will be deleted
	 * below in the display routine because they
	 * will appear as ``not seen'' in the kernel
	 * data structures.
	 */
	for (p = netcb.nif_forw; p != (struct netinfo *)&netcb; p = p->nif_forw) {
#ifdef INET6
		if (p->nif_family == AF_INET && (inp->inp_flags & INP_IPV6))
			continue;
		if (p->nif_family == AF_INET6 && !(inp->inp_flags & INP_IPV6))
			continue;
#endif
		if (!streq(proto, p->nif_proto))
			continue;
		if (p->nif_family == AF_INET) {
			if (p->nif_lport != inp->inp_lport ||
			    p->nif_laddr.s_addr != inp->inp_laddr.s_addr)
				continue;
			if (p->nif_faddr.s_addr == inp->inp_faddr.s_addr &&
			    p->nif_fport == inp->inp_fport)
				break;

		}
#ifdef INET6
		else if (p->nif_family == AF_INET6) {
			if (p->nif_lport != inp->inp_lport ||
			    !IN6_ARE_ADDR_EQUAL(&p->nif_laddr6, &inp->inp_laddr6))
				continue;
			if (IN6_ARE_ADDR_EQUAL(&p->nif_faddr6, &inp->inp_faddr6) &&
			    p->nif_fport == inp->inp_fport)
				break;
		}
#endif
		else
			continue;
	}
	if (p == (struct netinfo *)&netcb) {
		if ((p = malloc(sizeof(*p))) == NULL) {
			error("Out of memory");
			return;
		}
		p->nif_prev = (struct netinfo *)&netcb;
		p->nif_forw = netcb.nif_forw;
		netcb.nif_forw->nif_prev = p;
		netcb.nif_forw = p;
		p->nif_line = -1;
		p->nif_lport = inp->inp_lport;
		p->nif_fport = inp->inp_fport;
		p->nif_proto = proto;
		p->nif_flags = NIF_LACHG|NIF_FACHG;
#ifdef INET6
		if (inp->inp_flags & INP_IPV6) {
			p->nif_laddr6 = inp->inp_laddr6;
			p->nif_faddr6 = inp->inp_faddr6;
			p->nif_family = AF_INET6;
		} else
#endif
		{
			p->nif_laddr = inp->inp_laddr;
			p->nif_faddr = inp->inp_faddr;
			p->nif_family = AF_INET;
		}
	}
	p->nif_rcvcc = so->so_rcv.sb_cc;
	p->nif_sndcc = so->so_snd.sb_cc;
	p->nif_state = state;
	p->nif_seen = 1;
}

/* column locations */
#define	LADDR	0
#define	FADDR	LADDR+23
#define	PROTO	FADDR+23
#define	RCVCC	PROTO+6
#define	SNDCC	RCVCC+7
#define	STATE	SNDCC+7


void
labelnetstat(void)
{
	if (namelist[X_TCBTABLE].n_type == 0)
		return;
	wmove(wnd, 0, 0); wclrtobot(wnd);
	mvwaddstr(wnd, 0, LADDR, "Local Address");
	mvwaddstr(wnd, 0, FADDR, "Foreign Address");
	mvwaddstr(wnd, 0, PROTO, "Proto");
	mvwaddstr(wnd, 0, RCVCC, "Recv-Q");
	mvwaddstr(wnd, 0, SNDCC, "Send-Q");
	mvwaddstr(wnd, 0, STATE, "(state)");
}

void
shownetstat(void)
{
	struct netinfo *p, *q;

	/*
	 * First, delete any connections that have gone
	 * away and adjust the position of connections
	 * below to reflect the deleted line.
	 */
	p = netcb.nif_forw;
	while (p != (struct netinfo *)&netcb) {
		if (p->nif_line == -1 || p->nif_seen) {
			p = p->nif_forw;
			continue;
		}
		wmove(wnd, p->nif_line, 0); wdeleteln(wnd);
		q = netcb.nif_forw;
		for (; q != (struct netinfo *)&netcb; q = q->nif_forw)
			if (q != p && q->nif_line > p->nif_line) {
				q->nif_line--;
				/* this shouldn't be necessary */
				q->nif_flags |= NIF_LACHG|NIF_FACHG;
			}
		lastrow--;
		q = p->nif_forw;
		p->nif_prev->nif_forw = p->nif_forw;
		p->nif_forw->nif_prev = p->nif_prev;
		free(p);
		p = q;
	}
	/*
	 * Update existing connections and add new ones.
	 */
	for (p = netcb.nif_forw; p != (struct netinfo *)&netcb; p = p->nif_forw) {
		if (p->nif_line == -1) {
			/*
			 * Add a new entry if possible.
			 */
			if (lastrow > YMAX(wnd))
				continue;
			p->nif_line = lastrow++;
			p->nif_flags |= NIF_LACHG|NIF_FACHG;
		}
		if (p->nif_flags & NIF_LACHG) {
			wmove(wnd, p->nif_line, LADDR);
			switch (p->nif_family) {
			case AF_INET:
				inetprint(&p->nif_laddr, p->nif_lport,
					p->nif_proto);
				break;
#ifdef INET6
			case AF_INET6:
				inet6print(&p->nif_laddr6, p->nif_lport,
					p->nif_proto);
				break;
#endif
			}
			p->nif_flags &= ~NIF_LACHG;
		}
		if (p->nif_flags & NIF_FACHG) {
			wmove(wnd, p->nif_line, FADDR);
			switch (p->nif_family) {
			case AF_INET:
				inetprint(&p->nif_faddr, p->nif_fport,
					p->nif_proto);
				break;
#ifdef INET6
			case AF_INET6:
				inet6print(&p->nif_faddr6, p->nif_fport,
					p->nif_proto);
				break;
#endif
			}
			p->nif_flags &= ~NIF_FACHG;
		}
		mvwaddstr(wnd, p->nif_line, PROTO, p->nif_proto);
#ifdef INET6
		if (p->nif_family == AF_INET6)
			waddstr(wnd, "6");
#endif
		mvwprintw(wnd, p->nif_line, RCVCC, "%6d", p->nif_rcvcc);
		mvwprintw(wnd, p->nif_line, SNDCC, "%6d", p->nif_sndcc);
		if (streq(p->nif_proto, "tcp")) {
			if (p->nif_state < 0 || p->nif_state >= TCP_NSTATES)
				mvwprintw(wnd, p->nif_line, STATE, "%d",
				    p->nif_state);
			else
				mvwaddstr(wnd, p->nif_line, STATE,
				    tcpstates[p->nif_state]);
		}
		wclrtoeol(wnd);
	}
	if (lastrow < YMAX(wnd)) {
		wmove(wnd, lastrow, 0); wclrtobot(wnd);
		wmove(wnd, YMAX(wnd), 0); wdeleteln(wnd);	/* XXX */
	}
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
static void
inetprint(struct in_addr *in, int port, char *proto)
{
	struct servent *sp = 0;
	char line[80], *cp;

	snprintf(line, sizeof line, "%.*s.", 16, inetname(*in));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		snprintf(cp, sizeof line - strlen(cp), "%.8s",
		    sp ? sp->s_name : "*");
	else
		snprintf(cp, sizeof line - strlen(cp), "%d",
		    ntohs((u_short)port));
	/* pad to full column to clear any garbage */
	cp = strchr(line, '\0');
	while (cp - line < 22 && cp - line < sizeof line-1)
		*cp++ = ' ';
	*cp = '\0';
	waddstr(wnd, line);
}

#ifdef INET6
static void
inet6print(struct in6_addr *in6, int port, char *proto)
{
	struct servent *sp = 0;
	char line[80], *cp;

	snprintf(line, sizeof line, "%.*s.", 16, inet6name(in6));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		snprintf(cp, sizeof line - strlen(cp), "%.8s",
		    sp ? sp->s_name : "*");
	else
		snprintf(cp, sizeof line - strlen(cp), "%d",
		    ntohs((u_short)port));
	/* pad to full column to clear any garbage */
	cp = strchr(line, '\0');
	while (cp - line < 22 && cp - line < sizeof line-1)
		*cp++ = ' ';
	*cp = '\0';
	waddstr(wnd, line);
}
#endif

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
static const char *
inetname(struct in_addr in)
{
	char *cp = 0;
	static char line[50];
	struct hostent *hp;
	struct netent *np;

	if (!nflag && in.s_addr != INADDR_ANY) {
		int net = inet_netof(in);
		int lna = inet_lnaof(in);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
			if (hp)
				cp = hp->h_name;
		}
	}
	if (in.s_addr == INADDR_ANY) {
		strlcpy(line, "*", sizeof line);
	} else if (cp) {
		strlcpy(line, cp, sizeof line);
	} else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)	((x) & 0xff)
		snprintf(line, sizeof line, "%u.%u.%u.%u", C(in.s_addr >> 24),
		    C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}

#ifdef INET6
static const char *
inet6name(struct in6_addr *in6)
{
	static char line[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	int flags;

	flags = nflag ? NI_NUMERICHOST : 0;
	if (IN6_IS_ADDR_UNSPECIFIED(in6))
		return "*";
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *in6;
	if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
	    line, sizeof(line), NULL, 0, flags) == 0)
		return line;
	return "?";
}
#endif

int
cmdnetstat(char *cmd, char *args)
{
	struct netinfo *p;

	if (prefix(cmd, "all")) {
		aflag = !aflag;
		goto fixup;
	}
	if  (prefix(cmd, "numbers") || prefix(cmd, "names")) {
		int new;

		new = prefix(cmd, "numbers");
		if (new == nflag)
			return (1);
		p = netcb.nif_forw;
		for (; p != (struct netinfo *)&netcb; p = p->nif_forw) {
			if (p->nif_line == -1)
				continue;
			p->nif_flags |= NIF_LACHG|NIF_FACHG;
		}
		nflag = new;
		wclear(wnd);
		labelnetstat();
		goto redisplay;
	}
	if (!netcmd(cmd, args))
		return (0);
fixup:
	fetchnetstat();
redisplay:
	shownetstat();
	refresh();
	return (1);
}
