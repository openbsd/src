/*	$OpenBSD: netstat.c,v 1.45 2015/03/12 01:03:00 claudio Exp $	*/
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

#include <kvm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <nlist.h>
#include <paths.h>
#include "systat.h"
#include "engine.h"

#define	TCP	0x1
#define	UDP	0x2
#define	OTHER	0x4

struct netinfo {
	union {
		struct	in_addr nif_laddr;	/* local address */
		struct	in6_addr nif_laddr6;	/* local address */
	} l;
	union {
		struct	in_addr	nif_faddr;	/* foreign address */
		struct	in6_addr nif_faddr6;	/* foreign address */
	} f;
	long	nif_rcvcc;		/* rcv buffer character count */
	long	nif_sndcc;		/* snd buffer character count */
	short	nif_lport;		/* local port */
	short	nif_fport;		/* foreign port */
	short	nif_state;		/* tcp state */
	short	nif_family;
	short	nif_proto;		/* protocol */
	short	nif_ipproto;
};

#define nif_laddr  l.nif_laddr
#define nif_laddr6 l.nif_laddr6
#define nif_faddr  f.nif_faddr
#define nif_faddr6 f.nif_faddr6

static void enter(struct kinfo_file *);
static int kf_comp(const void *, const void *);
static void inetprint(struct in_addr *, int, char *, field_def *);
static void inet6print(struct in6_addr *, int, char *, field_def *);
static void shownetstat(struct netinfo *p);

void print_ns(void);
int read_ns(void);
int select_ns(void);
int ns_keyboard_callback(int);

#define	streq(a,b)	(strcmp(a,b)==0)

static	int aflag = 0;

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
		ni = reallocarray(netinfos, a, sizeof(*ni));
		if (ni == NULL)
			return NULL;
		netinfos = ni;
		num_alloc = a;
	}

	return &netinfos[num_ns++];
}

static void
enter(struct kinfo_file *kf)
{
#define s6_addr32 __u6_addr.__u6_addr32
	struct netinfo *p;

	/* first filter out unwanted sockets */
	if (kf->so_family != AF_INET && kf->so_family != AF_INET6)
		return;

	switch (kf->so_protocol) {
	case IPPROTO_TCP:
		if ((protos & TCP) == 0)
			return;
		break;
	case IPPROTO_UDP:
		if ((protos & UDP) == 0)
			return;
		break;
	default:
		if ((protos & OTHER) == 0)
			return;
		break;
	}

	if (!aflag) {
		struct in6_addr faddr6;

		switch (kf->so_family) {
		case AF_INET:
			if (kf->inp_faddru[0] == INADDR_ANY)
				return;
			break;
		case AF_INET6:
			faddr6.s6_addr32[0] = kf->inp_faddru[0];
			faddr6.s6_addr32[1] = kf->inp_faddru[1];
			faddr6.s6_addr32[2] = kf->inp_faddru[2];
			faddr6.s6_addr32[3] = kf->inp_faddru[3];
			if (IN6_IS_ADDR_UNSPECIFIED(&faddr6))
				return;
			break;
		}
	}

	/* finally enter the socket to the table */
	p = next_ns();
	if (p == NULL) {
		error("Out of Memory!");
		return;
	}

	p->nif_lport = kf->inp_lport;
	p->nif_fport = kf->inp_fport;
	p->nif_proto = kf->so_protocol;
	p->nif_ipproto = kf->inp_proto;

	switch (kf->so_family) {
	case AF_INET:
		p->nif_family = AF_INET;
		p->nif_laddr.s_addr = kf->inp_laddru[0];
		p->nif_faddr.s_addr = kf->inp_faddru[0];
		break;
	case AF_INET6:
		p->nif_family = AF_INET6;
		p->nif_laddr6.s6_addr32[0] = kf->inp_laddru[0];
		p->nif_laddr6.s6_addr32[1] = kf->inp_laddru[1];
		p->nif_laddr6.s6_addr32[2] = kf->inp_laddru[2];
		p->nif_laddr6.s6_addr32[3] = kf->inp_laddru[3];
		p->nif_faddr6.s6_addr32[0] = kf->inp_faddru[0];
		p->nif_faddr6.s6_addr32[1] = kf->inp_faddru[1];
		p->nif_faddr6.s6_addr32[2] = kf->inp_faddru[2];
		p->nif_faddr6.s6_addr32[3] = kf->inp_faddru[3];
		break;
	}

	p->nif_rcvcc = kf->so_rcv_cc;
	p->nif_sndcc = kf->so_snd_cc;
	p->nif_state = kf->t_state;
#undef s6_addr32
}


/* netstat callback functions */

int
select_ns(void)
{
	num_disp = num_ns;
	return (0);
}

static int type_map[] = { -1, 2, 3, 1, 4, 5 };

static int
kf_comp(const void *a, const void *b)
{
	const struct kinfo_file *ka = a, *kb = b;

	if (ka->so_family != kb->so_family) {
		/* AF_INET < AF_INET6 < AF_LOCAL */
		if (ka->so_family == AF_INET)
			return (-1);
		if (ka->so_family == AF_LOCAL)
			return (1);
		if (kb->so_family == AF_LOCAL)
			return (-1);
		return (1);
	}
	if (ka->so_family == AF_LOCAL) {
		if (type_map[ka->so_type] < type_map[kb->so_type])
			return (-1);
		if (type_map[ka->so_type] > type_map[kb->so_type])
			return (1);
	} else if (ka->so_family == AF_INET || ka->so_family == AF_INET6) {
		if (ka->so_protocol < kb->so_protocol)
			return (-1);
		if (ka->so_protocol > kb->so_protocol)
			return (1);
		if (ka->so_type == SOCK_DGRAM || ka->so_type == SOCK_STREAM) {
			/* order sockets by remote port desc */
			if (ka->inp_fport > kb->inp_fport)
				return (-1);
			if (ka->inp_fport < kb->inp_fport)
				return (1);
		} else if (ka->so_type == SOCK_RAW) {
			if (ka->inp_proto > kb->inp_proto)
				return (-1);
			if (ka->inp_proto < kb->inp_proto)
				return (1);
		}
	}
	return (0);
}


int
read_ns(void)
{
	struct kinfo_file *kf;
	int i, fcnt;

	if (kd == NULL) {
		error("Failed to initialize KVM!");
		return (0);
	}
	kf = kvm_getfiles(kd, KERN_FILE_BYFILE, DTYPE_SOCKET,
	    sizeof(*kf), &fcnt);
	if (kf == NULL) {
		error("Out of Memory!");
		return (0);
	}

	/* sort sockets by AF, proto and type */
	qsort(kf, fcnt, sizeof(*kf), kf_comp);

	num_ns = 0;

	for (i = 0; i < fcnt; i++)
		enter(&kf[i]);

	num_disp = num_ns;
	return 0;
}

void
print_ns(void)
{
	int n, count = 0;

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

	protos = TCP|UDP|OTHER;
	for (v = views_ns; v->name != NULL; v++)
		add_view(v);

	return(1);
}

static void
shownetstat(struct netinfo *p)
{
	char *proto = NULL;

	switch (p->nif_proto) {
	case IPPROTO_TCP:
		proto = "tcp";
		break;
	case IPPROTO_UDP:
		proto = "udp";
		break;
	}

	switch (p->nif_family) {
	case AF_INET:
		inetprint(&p->nif_laddr, p->nif_lport,
			  proto, FLD_NS_LOCAL);
		inetprint(&p->nif_faddr, p->nif_fport,
			  proto, FLD_NS_FOREIGN);
		break;
	case AF_INET6:
		inet6print(&p->nif_laddr6, p->nif_lport,
			   proto, FLD_NS_LOCAL);
		inet6print(&p->nif_faddr6, p->nif_fport,
			   proto, FLD_NS_FOREIGN);
		break;
	}
 
	tb_start();
	switch (p->nif_proto) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		tbprintf(proto);
		if (p->nif_family == AF_INET6)
			tbprintf("6");
		break;
	case IPPROTO_DIVERT:
		tbprintf("divert");
		if (p->nif_family == AF_INET6)
			tbprintf("6");
		break;
	default:
		tbprintf("%d", p->nif_ipproto);
		break;
	}

	print_fld_tb(FLD_NS_PROTO);

	print_fld_size(FLD_NS_RECV_Q, p->nif_rcvcc);
	print_fld_size(FLD_NS_SEND_Q, p->nif_sndcc);

	if (p->nif_proto == IPPROTO_TCP) {
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
ns_keyboard_callback(int ch)
{
	switch (ch) {
	case 'a':
		aflag = !aflag;
		gotsig_alarm = 1;
		break;
	case 'n':
		nflag = !nflag;
		gotsig_alarm = 1;
		break;
	case 'o':
		protos ^= OTHER;
		gotsig_alarm = 1;
		break;
	case 'r':
		aflag = 0;
		nflag = 1;
		protos = TCP|UDP;
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

