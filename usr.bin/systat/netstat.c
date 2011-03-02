/*	$OpenBSD: netstat.c,v 1.34 2011/03/02 06:48:17 jasper Exp $	*/
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
#include "engine.h"

struct netinfo {
	union {
		struct	in_addr nif_laddr;	/* local address */
		struct	in6_addr nif_laddr6;	/* local address */
	} l;
	union {
		struct	in_addr	nif_faddr;	/* foreign address */
		struct	in6_addr nif_faddr6;	/* foreign address */
	} f;
	char	*nif_proto;		/* protocol */
	long	nif_rcvcc;		/* rcv buffer character count */
	long	nif_sndcc;		/* snd buffer character count */
	short	nif_lport;		/* local port */
	short	nif_fport;		/* foreign port */
	short	nif_state;		/* tcp state */
	short	nif_family;
};

#define nif_laddr  l.nif_laddr
#define nif_laddr6 l.nif_laddr6
#define nif_faddr  f.nif_faddr
#define nif_faddr6 f.nif_faddr6

static void enter(struct inpcb *, struct socket *, int, char *);
static void inetprint(struct in_addr *, int, char *, field_def *);
static void inet6print(struct in6_addr *, int, char *, field_def *);
static void shownetstat(struct netinfo *p);

void print_ns(void);
int read_ns(void);
int select_ns(void);
int ns_keyboard_callback(int);

#define	streq(a,b)	(strcmp(a,b)==0)

static	int aflag = 0;

static struct nlist namelist[] = {
#define	X_TCBTABLE	0		/* no sysctl */
	{ "_tcbtable" },
#define	X_UDBTABLE	1		/* no sysctl */
	{ "_udbtable" },
	{ "" },
};
#define ADD_ALLOC  1000


int protos;

struct netinfo *netinfos = NULL;
size_t num_ns = 0;
static size_t num_alloc = 0;


field_def fields_ns[] = {
	{"LOCAL ADDRESS", 20, 45, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"FOREIGN ADDRESS", 20, 45, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"PROTO", 4, 9, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"RECV-Q", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"SEND-Q", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"STATE", 5, 11, 6, FLD_ALIGN_LEFT, -1, 0, 0, 0},
};

#define FLD_NS_LOCAL	FIELD_ADDR(fields_ns,0)
#define FLD_NS_FOREIGN	FIELD_ADDR(fields_ns,1)
#define FLD_NS_PROTO	FIELD_ADDR(fields_ns,2)
#define FLD_NS_RECV_Q	FIELD_ADDR(fields_ns,3)
#define FLD_NS_SEND_Q	FIELD_ADDR(fields_ns,4)
#define FLD_NS_STATE	FIELD_ADDR(fields_ns,5)

/* Define views */
field_def *view_ns_0[] = {
	FLD_NS_LOCAL, FLD_NS_FOREIGN, FLD_NS_PROTO,
	FLD_NS_RECV_Q, FLD_NS_SEND_Q, FLD_NS_STATE, NULL
};

/* Define view managers */
struct view_manager netstat_mgr = {
	"Netstat", select_ns, read_ns, NULL, print_header,
	print_ns, ns_keyboard_callback, NULL, NULL
};

field_view views_ns[] = {
	{view_ns_0, "netstat", '0', &netstat_mgr},
	{NULL, NULL, 0, NULL}
};




struct netinfo *
next_ns(void)
{
	if (num_alloc <= num_ns) {
		struct netinfo *ni;
		size_t a = num_alloc + ADD_ALLOC;
		if (a < num_alloc)
			return NULL;
		ni = realloc(netinfos, a * sizeof(*ni));
		if (ni == NULL)
			return NULL;
		netinfos = ni;
		num_alloc = a;
	}

	return &netinfos[num_ns++];
}

static void
enter(struct inpcb *inp, struct socket *so, int state, char *proto)
{
	struct netinfo *p;

	p = next_ns();
	if (p == NULL) {
		error("Out of Memory!");
		return;
	}

	p->nif_lport = inp->inp_lport;
	p->nif_fport = inp->inp_fport;
	p->nif_proto = proto;
	
	if (inp->inp_flags & INP_IPV6) {
		p->nif_laddr6 = inp->inp_laddr6;
		p->nif_faddr6 = inp->inp_faddr6;
		p->nif_family = AF_INET6;
	} else {
		p->nif_laddr = inp->inp_laddr;
		p->nif_faddr = inp->inp_faddr;
		p->nif_family = AF_INET;
	}

	p->nif_rcvcc = so->so_rcv.sb_cc;
	p->nif_sndcc = so->so_snd.sb_cc;
	p->nif_state = state;
}


/* netstat callback functions */

int
select_ns(void)
{
	static int init = 0;
	if (kd == NULL) {
		num_disp = 1;
		return (0);
	}

	if (!init) {
		sethostent(1);
		setnetent(1);
		init = 1;
	}

	num_disp = num_ns;
	return (0);
}

int
read_ns(void)
{
	struct inpcbtable pcbtable;
	struct inpcb *head, *prev, *next;
	struct inpcb inpcb;
	struct socket sockb;
	struct tcpcb tcpcb;
	void *off;
	int istcp;

	if (kd == NULL) {
		return (0);
	}

	num_ns = 0;

	if (namelist[X_TCBTABLE].n_value == 0)
		return 0;

	if (protos & TCP) {
		off = NPTR(X_TCBTABLE);
		istcp = 1;
	} else if (protos & UDP) {
		off = NPTR(X_UDBTABLE);
		istcp = 0;
	} else {
		error("No protocols to display");
		return 0;
	}

again:
	KREAD(off, &pcbtable, sizeof (struct inpcbtable));

	prev = head = (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue;
	next = CIRCLEQ_FIRST(&pcbtable.inpt_queue);

	while (next != head) {
		KREAD(next, &inpcb, sizeof (inpcb));
		if (CIRCLEQ_PREV(&inpcb, inp_queue) != prev) {
			error("Kernel state in transition");
			return 0;
		}
		prev = next;
		next = CIRCLEQ_NEXT(&inpcb, inp_queue);

		if (!aflag) {
			if (!(inpcb.inp_flags & INP_IPV6) &&
			    inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
				continue;
			if ((inpcb.inp_flags & INP_IPV6) &&
			    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6))
				continue;
		}
		KREAD(inpcb.inp_socket, &sockb, sizeof (sockb));
		if (istcp) {
			KREAD(inpcb.inp_ppcb, &tcpcb, sizeof (tcpcb));
			enter(&inpcb, &sockb, tcpcb.t_state, "tcp");
		} else
			enter(&inpcb, &sockb, 0, "udp");
	}
	if (istcp && (protos & UDP)) {
		istcp = 0;
		off = NPTR(X_UDBTABLE);
		goto again;
	}

	num_disp = num_ns;
	return 0;
}

void
print_ns(void)
{
	int n, count = 0;

	if (kd == NULL) {
		print_fld_str(FLD_NS_LOCAL, "Failed to initialize KVM!");
		print_fld_str(FLD_NS_FOREIGN, "Failed to initialize KVM!");
		end_line();
		return;
	}

	for (n = dispstart; n < num_disp; n++) {
		shownetstat(netinfos + n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}


int
initnetstat(void)
{
	field_view *v;
	int ret;

	if (kd) {
		if ((ret = kvm_nlist(kd, namelist)) == -1)
			errx(1, "%s", kvm_geterr(kd));
		else if (ret)
			nlisterr(namelist);

		if (namelist[X_TCBTABLE].n_value == 0) {
			error("No symbols in namelist");
			return(0);
		}
	}
	protos = TCP|UDP;

	for (v = views_ns; v->name != NULL; v++)
		add_view(v);

	return(1);
}

static void
shownetstat(struct netinfo *p)
{
	switch (p->nif_family) {
	case AF_INET:
		inetprint(&p->nif_laddr, p->nif_lport,
			  p->nif_proto, FLD_NS_LOCAL);
		inetprint(&p->nif_faddr, p->nif_fport,
			  p->nif_proto, FLD_NS_FOREIGN);
		break;
	case AF_INET6:
		inet6print(&p->nif_laddr6, p->nif_lport,
			   p->nif_proto, FLD_NS_LOCAL);
		inet6print(&p->nif_faddr6, p->nif_fport,
			   p->nif_proto, FLD_NS_FOREIGN);
		break;
	}
 
	tb_start();
	tbprintf("%s", p->nif_proto);
	if (p->nif_family == AF_INET6)
		tbprintf("6");

	print_fld_tb(FLD_NS_PROTO);

	print_fld_size(FLD_NS_RECV_Q, p->nif_rcvcc);
	print_fld_size(FLD_NS_SEND_Q, p->nif_sndcc);

	if (streq(p->nif_proto, "tcp")) {
		if (p->nif_state < 0 || p->nif_state >= TCP_NSTATES)
			print_fld_uint(FLD_NS_STATE, p->nif_state);
		else
			print_fld_str(FLD_NS_STATE, tcpstates[p->nif_state]);
	}
	end_line();
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
static void
inetprint(struct in_addr *in, int port, char *proto, field_def *fld)
{
	struct servent *sp = 0;

	tb_start();
	tbprintf("%s", inetname(*in));

	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		tbprintf(":%s", sp ? sp->s_name : "*");
	else
		tbprintf(":%d", ntohs((u_short)port));

	print_fld_tb(fld);
}

static void
inet6print(struct in6_addr *in6, int port, char *proto, field_def *fld)
{
	struct servent *sp = 0;

	tb_start();

	tbprintf("%s", inet6name(in6));
	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		tbprintf(":%s", sp ? sp->s_name : "*");
	else
		tbprintf(":%d", ntohs((u_short)port));

	print_fld_tb(fld);
}

int
kvm_ckread(void *a, void *b, size_t l)
{
	if (kvm_read(kd, (u_long)a, b, l) != l) {
		if (verbose)
			error("error reading kmem at %x\n", a);
		return (0);
	} else
		return (1);
}


int
ns_keyboard_callback(int ch)
{
	switch (ch) {
	case 'n':
		nflag = !nflag;
		gotsig_alarm = 1;
		break;
	case 't':
		protos ^= TCP;
		gotsig_alarm = 1;
		break;
	case 'u':
		protos ^= UDP;
		gotsig_alarm = 1;
		break;
	default:
		return keyboard_callback(ch);
	};

	return 1;
}

