/*	$OpenBSD: trsp.c,v 1.7 1998/07/08 22:13:33 deraadt Exp $	*/

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
 * Copyright (c) 1985, 1993
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
"@(#) Copyright (c) 1985, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)trsp.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#define PRUREQUESTS
#include <sys/protosw.h>

#include <net/route.h>
#include <net/if.h>

#define TCPSTATES
#include <netinet/tcp_fsm.h>
#define	TCPTIMERS
#include <netinet/tcp_timer.h>

#include <netns/ns.h>
#include <netns/sp.h>
#include <netns/idp.h>
#include <netns/spidp.h>
#include <netns/spp_timer.h>
#include <netns/spp_var.h>
#include <netns/ns_pcb.h>
#include <netns/idp_var.h>
#define SANAMES
#include <netns/spp_debug.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <kvm.h>
#include <nlist.h>
#include <paths.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

u_int32_t ntime;
int	sflag;
int	tflag;
int	jflag;
int	aflag;
int	zflag;
struct	nlist nl[] = {
#define	N_SPP_DEBUG	0
	{ "_spp_debug" },
#define	N_SPP_DEBX	1
	{ "_spp_debx" },
	{ NULL },
};

struct	spp_debug spp_debug[SPP_NDEBUG];
caddr_t	spp_pcbs[SPP_NDEBUG];
int	spp_debx;

kvm_t	*kd;

extern	char *__progname;

int	main __P((int, char *[]));
void	dotrace __P((caddr_t));
int	numeric __P((const void *, const void *));
void	spp_trace __P((short, short, struct sppcb *, struct sppcb *,
	    struct spidp *, int));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, i, npcbs = 0;
	char *system, *core, *cp, errbuf[_POSIX2_LINE_MAX];

	system = core = NULL;

	while ((ch = getopt(argc, argv, "azstjp:N:M:")) != -1) {
		switch (ch) {
		case 'a':
			++aflag;
			break;
		case 'z':
			++zflag;
			break;
		case 's':
			++sflag;
			break;
		case 't':
			++tflag;
			break;
		case 'j':
			++jflag;
			break;
		case 'p':
			if (npcbs >= SPP_NDEBUG)
				errx(1, "too many pcbs specified");
			errno = 0;
			spp_pcbs[npcbs++] = (caddr_t)strtoul(optarg, &cp, 16);
			if (*cp != '\0' || errno == ERANGE)
				errx(1, "invalid address: %s", optarg);
			break;
		case 'N':
			system = optarg;
			break;
		case 'M':
			core = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (core != NULL || system != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	kd = kvm_openfiles(system, core, NULL, zflag ? O_RDWR : O_RDONLY,
	    errbuf);
	if (kd == NULL)
		errx(1, "can't open kmem: %s", errbuf);

	setegid(getgid());
	setgid(getgid());

	if (kvm_nlist(kd, nl))
		errx(2, "%s: no namelist", system ? system : _PATH_UNIX);

	if (kvm_read(kd, nl[N_SPP_DEBX].n_value, &spp_debx,
	    sizeof(spp_debx)) != sizeof(spp_debx))
		errx(3, "spp_debx: %s", kvm_geterr(kd));
	printf("spp_debx=%d\n", spp_debx);

	if (kvm_read(kd, nl[N_SPP_DEBUG].n_value, spp_debug,
	    sizeof(spp_debug)) != sizeof(spp_debug))
		errx(3, "spp_debug: %s", kvm_geterr(kd));

	/*
	 * Here, we just want to clear out the old trace data and start over.
	 */
	if (zflag) {
		spp_debx = 0;
		(void) memset(spp_debug, 0, sizeof(spp_debug));

		if (kvm_write(kd, nl[N_SPP_DEBX].n_value, &spp_debx,
		    sizeof(spp_debx)) != sizeof(spp_debx))
			errx(4, "write spp_debx: %s", kvm_geterr(kd));
		
		if (kvm_write(kd, nl[N_SPP_DEBUG].n_value, spp_debug,
		    sizeof(spp_debug)) != sizeof(spp_debug))
			errx(4, "write spp_debug: %s", kvm_geterr(kd));

		exit(0);
	}

	/*
	 * If no control blocks have been specified, figure
	 * out how many distinct one we have and summarize
	 * them in spp_pcbs for sorting the trace records
	 * below.
	 */
	if (npcbs == 0) {
		for (i = 0; i < SPP_NDEBUG; i++) {
			struct spp_debug *sd = &spp_debug[i];
			int j;

			if (sd->sd_cb == 0)
				continue;
			for (j = 0; j < npcbs; j++)
				if (spp_pcbs[j] == sd->sd_cb)
					break;
			if (j >= npcbs)
				spp_pcbs[npcbs++] = sd->sd_cb;
		}
	}
	qsort(spp_pcbs, npcbs, sizeof (caddr_t), numeric);
	if (jflag) {
		cp = "";

		for (i = 0; i < npcbs; i++) {
			printf("%s%lx", cp, (long)spp_pcbs[i]);
			cp = ", ";
		}
		if (*cp)
			putchar('\n');
	} else {
		for (i = 0; i < npcbs; i++) {
			printf("\n%lx:\n", (long)spp_pcbs[i]);
			dotrace(spp_pcbs[i]);
		}
	}
	exit(0);
}

void
dotrace(sppcb)
	caddr_t sppcb;
{
	struct spp_debug *sd;
	int i;

	for (i = spp_debx % SPP_NDEBUG; i < SPP_NDEBUG; i++) {
		sd = &spp_debug[i];
		if (sppcb && sd->sd_cb != sppcb)
			continue;
		ntime = ntohl(sd->sd_time);
		spp_trace(sd->sd_act, sd->sd_ostate, (struct sppcb *)sd->sd_cb,
		    &sd->sd_sp, &sd->sd_si, sd->sd_req);
	}
	for (i = 0; i < spp_debx % SPP_NDEBUG; i++) {
		sd = &spp_debug[i];
		if (sppcb && sd->sd_cb != sppcb)
			continue;
		ntime = ntohl(sd->sd_time);
		spp_trace(sd->sd_act, sd->sd_ostate, (struct sppcb *)sd->sd_cb,
		    &sd->sd_sp, &sd->sd_si, sd->sd_req);
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
spp_trace(act, ostate, asp, sp, si, req)
	short act, ostate;
	struct sppcb *asp, *sp;
	struct spidp *si;
	int req;
{
	u_int16_t seq, ack, len, alo;
	int flags;
	char *cp;

	if (ostate >= TCP_NSTATES)
		ostate = 0;
	if (act > SA_DROP)
		act = SA_DROP;
	printf("\n");
	printf("%03d %s:%s", (ntime/10) % 1000, tcpstates[ostate],
	    sanames[act]);

	if (si != 0) {
		seq = si->si_seq;
		ack = si->si_ack;
		alo = si->si_alo;
		len = si->si_len;
		switch (act) {
		case SA_RESPOND:
		case SA_OUTPUT:
				NTOHS(seq);
				NTOHS(ack);
				NTOHS(alo);
				NTOHS(len);
		case SA_INPUT:
		case SA_DROP:
			if (aflag)
				printf("\n\tsna=%s\tdna=%s",
				    ns_ntoa(si->si_sna), ns_ntoa(si->si_dna));
			printf("\n\t");
#define p1(name, f) { \
	printf("%s = %x, ", name, f); \
}
			p1("seq", seq);
			p1("ack", ack);
			p1("alo", alo);
			p1("len", len);
			flags = si->si_cc;
			printf("flags=%x", flags);
#define pf(name, f) { \
	if (flags & f) { \
		printf("%s%s", cp, name); \
		cp = ","; \
	} \
}
			if (flags) {
				cp = "<";
				pf("SP_SP", SP_SP);
				pf("SP_SA", SP_SA);
				pf("SP_OB", SP_OB);
				pf("SP_EM", SP_EM);
				printf(">");
			}
			printf(", ");
#define p2(name, f) { \
	printf("%s = %x, ", name, f); \
}
			p2("sid", si->si_sid);
			p2("did", si->si_did);
			p2("dt", si->si_dt);
			printf("\n\tsna=%s\tdna=%s", ns_ntoa(si->si_sna),
			    ns_ntoa(si->si_dna));
		}
	}
	if(act == SA_USER) {
		printf("\treq=%s", prurequests[req&0xff]);
		if ((req & 0xff) == PRU_SLOWTIMO)
			printf("<%s>", tcptimers[req>>8]);
	}
	printf(" -> %s", tcpstates[sp->s_state]);

	/* print out internal state of sp !?! */
	printf("\n");
	if (sp == 0)
		return;
#define p3(name, f)  { \
	printf("%s = %x, ", name, f); \
}
	if (sflag) {
		printf("\t");
		p3("rack", sp->s_rack);
		p3("ralo", sp->s_ralo);
		p3("smax", sp->s_smax);
		p3("snxt", sp->s_snxt);
		p3("flags", sp->s_flags);
#undef pf
#define pf(name, f) { \
	if (flags & f) { \
		printf("%s%s", cp, name); \
		cp = ","; \
	} \
}
		flags = sp->s_flags;
		if (flags || sp->s_oobflags) {
			cp = "<";
			pf("ACKNOW", SF_ACKNOW);
			pf("DELACK", SF_DELACK);
			pf("HI", SF_HI);
			pf("HO", SF_HO);
			pf("PI", SF_PI);
			pf("WIN", SF_WIN);
			pf("RXT", SF_RXT);
			pf("RVD", SF_RVD);
			flags = sp->s_oobflags;
			pf("SOOB", SF_SOOB);
			pf("IOOB", SF_IOOB);
			printf(">");
		}
	}
	/* print out timers? */
	if (tflag) {
		int i;

		cp = "\t";

		printf("\n\tTIMERS: ");
		p3("idle", sp->s_idle);
		p3("force", sp->s_force);
		p3("rtseq", sp->s_rtseq);
		for (i = 0; i < TCPT_NTIMERS; i++) {
			if (sp->s_timer[i] == 0)
				continue;
			printf("%s%s=%d", cp, tcptimers[i], sp->s_timer[i]);
			if (i == TCPT_REXMT)
				printf(" (s_rxtshft=%d)", sp->s_rxtshift);
			cp = ", ";
		}
		if (*cp != '\t')
			putchar('\n');
	}
}

void
usage()
{

	fprintf(stderr, "usage: %s [-azstj] [-p hex-address]"
	    " [-N system] [-M core]\n", __progname);
	exit(1);
}
