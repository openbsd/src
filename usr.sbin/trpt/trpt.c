/*	$OpenBSD: trpt.c,v 1.8 2000/02/25 23:32:55 deraadt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1988, 1993
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)trpt.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#define PRUREQUESTS
#include <sys/protosw.h>
#include <sys/file.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#define	TCPTIMERS
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#define	TANAMES
#include <netinet/tcp_debug.h>

#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <errno.h>
#include <kvm.h>
#include <nlist.h>
#include <paths.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

struct nlist nl[] = {
#define	N_TCP_DEBUG	0
	{ "_tcp_debug" },
#define	N_TCP_DEBX	1
	{ "_tcp_debx" },
	{ NULL },
};

static caddr_t tcp_pcbs[TCP_NDEBUG];
static n_time ntime;
static int aflag, follow, sflag, tflag;

extern	char *__progname;

int	main __P((int, char *[]));
void	dotrace __P((caddr_t));
void	tcp_trace __P((short, short, struct tcpcb *, struct tcpcb *,
	    struct tcpiphdr *, int));
int	numeric __P((const void *, const void *));
void	usage __P((void));

kvm_t	*kd;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i, jflag, npcbs;
	char *system, *core, *cp, errbuf[_POSIX2_LINE_MAX];

	system = core = NULL;

	jflag = npcbs = 0;
	while ((ch = getopt(argc, argv, "afjM:N:p:st")) != -1) {
		switch (ch) {
		case 'a':
			++aflag;
			break;
		case 'f':
			++follow;
			setlinebuf(stdout);
			break;
		case 'j':
			++jflag;
			break;
		case 'p':
			if (npcbs >= TCP_NDEBUG)
				errx(1, "too many pcbs specified");
			errno = 0;
			tcp_pcbs[npcbs++] = (caddr_t)strtoul(optarg, &cp, 16);
			if (*cp != '\0' || errno == ERANGE)
				errx(1, "invalid address: %s", optarg);
			break;
		case 's':
			++sflag;
			break;
		case 't':
			++tflag;
			break;
		case 'N':
			system = optarg;
			break;
		case 'M':
			core = optarg;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	/*
	 * Discard setgid privileged if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (core != NULL || system != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	kd = kvm_openfiles(system, core, NULL, O_RDONLY, errbuf);
	if (kd == NULL)
		errx(1, "can't open kmem: %s", errbuf);

	setegid(getgid());
	setgid(getgid());

	if (kvm_nlist(kd, nl))
		errx(2, "%s: no namelist", system ? system : _PATH_UNIX);

	if (kvm_read(kd, nl[N_TCP_DEBX].n_value, (char *)&tcp_debx,
	    sizeof(tcp_debx)) != sizeof(tcp_debx))
		errx(3, "tcp_debx: %s", kvm_geterr(kd));

	if (kvm_read(kd, nl[N_TCP_DEBUG].n_value, (char *)tcp_debug,
	    sizeof(tcp_debug)) != sizeof(tcp_debug))
		errx(3, "tcp_debug: %s", kvm_geterr(kd));

	/*
	 * If no control blocks have been specified, figure
	 * out how many distinct one we have and summarize
	 * them in tcp_pcbs for sorting the trace records
	 * below.
	 */
	if (npcbs == 0) {
		for (i = 0; i < TCP_NDEBUG; i++) {
			struct tcp_debug *td = &tcp_debug[i];
			int j;

			if (td->td_tcb == 0)
				continue;
			for (j = 0; j < npcbs; j++)
				if (tcp_pcbs[j] == td->td_tcb)
					break;
			if (j >= npcbs)
				tcp_pcbs[npcbs++] = td->td_tcb;
		}
		if (npcbs == 0)
			exit(0);
	}
	qsort(tcp_pcbs, npcbs, sizeof(caddr_t), numeric);
	if (jflag) {
		for (i = 0;;) {
			printf("%lx", (long)tcp_pcbs[i]);
			if (++i == npcbs)
				break;
			fputs(", ", stdout);
		}
		putchar('\n');
	} else {
		for (i = 0; i < npcbs; i++) {
			printf("\n%lx:\n", (long)tcp_pcbs[i]);
			dotrace(tcp_pcbs[i]);
		}
	}
	exit(0);
}

void
dotrace(tcpcb)
	caddr_t tcpcb;
{
	struct tcp_debug *td;
	int prev_debx = tcp_debx;
	int i;

 again:
	if (--tcp_debx < 0)
		tcp_debx = TCP_NDEBUG - 1;
	for (i = prev_debx % TCP_NDEBUG; i < TCP_NDEBUG; i++) {
		td = &tcp_debug[i];
		if (tcpcb && td->td_tcb != tcpcb)
			continue;
		ntime = ntohl(td->td_time);
		tcp_trace(td->td_act, td->td_ostate,
		    (struct tcpcb *)td->td_tcb, &td->td_cb, &td->td_ti,
		    td->td_req);
		if (i == tcp_debx)
			goto done;
	}
	for (i = 0; i <= tcp_debx % TCP_NDEBUG; i++) {
		td = &tcp_debug[i];
		if (tcpcb && td->td_tcb != tcpcb)
			continue;
		ntime = ntohl(td->td_time);
		tcp_trace(td->td_act, td->td_ostate,
		    (struct tcpcb *)td->td_tcb, &td->td_cb, &td->td_ti,
		    td->td_req);
	}
 done:
	if (follow) {
		prev_debx = tcp_debx + 1;
		if (prev_debx >= TCP_NDEBUG)
			prev_debx = 0;
		do {
			sleep(1);
			if (kvm_read(kd, nl[N_TCP_DEBX].n_value,
			    (char *)&tcp_debx, sizeof(tcp_debx)) !=
			    sizeof(tcp_debx))
				errx(3, "tcp_debx: %s", kvm_geterr(kd));
		} while (tcp_debx == prev_debx);

		if (kvm_read(kd, nl[N_TCP_DEBUG].n_value, (char *)tcp_debug,
		    sizeof(tcp_debug)) != sizeof(tcp_debug))
			errx(3, "tcp_debug: %s", kvm_geterr(kd));

		goto again;
	}
}

/*
 * Tcp debug routines
 */
/*ARGSUSED*/
void
tcp_trace(act, ostate, atp, tp, ti, req)
	short act, ostate;
	struct tcpcb *atp, *tp;
	struct tcpiphdr *ti;
	int req;
{
	tcp_seq seq, ack;
	int flags, len, win, timer;

	printf("%03d %s:%s ", (ntime/10) % 1000, tcpstates[ostate],
	    tanames[act]);
	switch (act) {
	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:
		if (aflag) {
			printf("(src=%s,%u, ",
			    inet_ntoa(ti->ti_src), ntohs(ti->ti_sport));
			printf("dst=%s,%u)",
			    inet_ntoa(ti->ti_dst), ntohs(ti->ti_dport));
		}
		seq = ti->ti_seq;
		ack = ti->ti_ack;
		len = ti->ti_len;
		win = ti->ti_win;
		if (act == TA_OUTPUT) {
			NTOHL(seq);
			NTOHL(ack);
			NTOHS(len);
			NTOHS(win);
		}
		if (act == TA_OUTPUT)
			len -= sizeof(struct tcphdr);
		if (len)
			printf("[%x..%x)", seq, seq + len);
		else
			printf("%x", seq);
		printf("@%x", ack);
		if (win)
			printf("(win=%x)", win);
		flags = ti->ti_flags;
		if (flags) {
			register char *cp = "<";
#define	pf(flag, string) { \
	if (ti->ti_flags&flag) { \
		(void)printf("%s%s", cp, string); \
		cp = ","; \
	} \
}
			pf(TH_SYN, "SYN");
			pf(TH_ACK, "ACK");
			pf(TH_FIN, "FIN");
			pf(TH_RST, "RST");
			pf(TH_PUSH, "PUSH");
			pf(TH_URG, "URG");
			printf(">");
		}
		break;
	case TA_USER:
		timer = req >> 8;
		req &= 0xff;
		printf("%s", prurequests[req]);
		if (req == PRU_SLOWTIMO || req == PRU_FASTTIMO)
			printf("<%s>", tcptimers[timer]);
		break;
	}
	printf(" -> %s", tcpstates[tp->t_state]);
	/* print out internal state of tp !?! */
	printf("\n");
	if (sflag) {
		printf("\trcv_nxt %x rcv_wnd %lx snd_una %x snd_nxt %x snd_max %x\n",
		    tp->rcv_nxt, tp->rcv_wnd, tp->snd_una, tp->snd_nxt,
		    tp->snd_max);
		printf("\tsnd_wl1 %x snd_wl2 %x snd_wnd %lx\n", tp->snd_wl1,
		    tp->snd_wl2, tp->snd_wnd);
	}
	/* print out timers? */
	if (tflag) {
		register char *cp = "\t";
		register int i;

		for (i = 0; i < TCPT_NTIMERS; i++) {
			if (tp->t_timer[i] == 0)
				continue;
			printf("%s%s=%d", cp, tcptimers[i], tp->t_timer[i]);
			if (i == TCPT_REXMT)
				printf(" (t_rxtshft=%d)", tp->t_rxtshift);
			cp = ", ";
		}
		if (*cp != '\t')
			putchar('\n');
	}
}

int
numeric(v1, v2)
	const void *v1, *v2;
{
	const caddr_t *c1 = v1;
	const caddr_t *c2 = v2;
	int rv;

	if (*c1 < *c2)
		rv = -1;
	else if (*c1 > *c2)
		rv = 1;
	else
		rv = 0;

	return (rv);
}

void
usage()
{

	(void) fprintf(stderr, "usage: %s [-afjst] [-p hex-address]"
	    " [-N system] [-M core]\n", __progname);
	exit(1);
}
