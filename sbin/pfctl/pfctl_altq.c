/*	$OpenBSD: pfctl_altq.c,v 1.31 2003/01/09 17:33:19 henning Exp $	*/

/*
 * Copyright (C) 2002
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 * Copyright (C) 2002 Henning Brauer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <altq/altq.h>
#include <altq/altq_cbq.h>
#include <altq/altq_priq.h>
#include <altq/altq_hfsc.h>

#include "pfctl_parser.h"
#include "pfctl.h"

#define is_sc_null(sc)	(((sc) == NULL) || ((sc)->m1 == 0 && (sc)->m2 == 0))

TAILQ_HEAD(altqs, pf_altq) altqs = TAILQ_HEAD_INITIALIZER(altqs);
LIST_HEAD(gen_sc, segment) rtsc, lssc;

static int	eval_pfqueue_cbq(struct pfctl *pf, struct pf_altq *);
static int	cbq_compute_idletime(struct pfctl *, struct pf_altq *);
static int	check_commit_cbq(int, int, struct pf_altq *);
static void	print_cbq_opts(const struct pf_altq *);

static int	eval_pfqueue_priq(struct pfctl *pf, struct pf_altq *);
static int	check_commit_priq(int, int, struct pf_altq *);
static void	print_priq_opts(const struct pf_altq *);

static int	eval_pfqueue_hfsc(struct pfctl *pf, struct pf_altq *);
static int	check_commit_hfsc(int, int, struct pf_altq *);
static void	print_hfsc_opts(const struct pf_altq *);

static void		 gsc_add_sc(struct gen_sc *, struct service_curve *);
static int		 is_gsc_under_sc(struct gen_sc *,
			     struct service_curve *);
static void		 gsc_destroy(struct gen_sc *);
static struct segment	*gsc_getentry(struct gen_sc *, double);
static int		 gsc_add_seg(struct gen_sc *, double, double, double,
			     double);
static double		 sc_x2y(struct service_curve *, double);

static char	*rate2str(double);
u_int32_t	 getifspeed(char *);
u_long		 getifmtu(char *);

void
pfaltq_store(struct pf_altq *a)
{
	struct pf_altq	*altq;

	if ((altq = malloc(sizeof(*altq))) == NULL)
		err(1, "malloc");
	memcpy(altq, a, sizeof(struct pf_altq));
	TAILQ_INSERT_TAIL(&altqs, altq, entries);
}

void
pfaltq_free(struct pf_altq *a)
{
	struct pf_altq	*altq;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(a->ifname, altq->ifname, IFNAMSIZ) == 0 &&
		    strncmp(a->qname, altq->qname, PF_QNAME_SIZE) == 0) {
			TAILQ_REMOVE(&altqs, altq, entries);
			free(altq);
			return;
		}
	}
}

struct pf_altq *
pfaltq_lookup(const char *ifname)
{
	struct pf_altq	*altq;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(ifname, altq->ifname, IFNAMSIZ) == 0 &&
		    altq->qname[0] == 0)
			return (altq);
	}
	return (NULL);
}

struct pf_altq *
qname_to_pfaltq(const char *qname, const char *ifname)
{
	struct pf_altq	*altq;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(ifname, altq->ifname, IFNAMSIZ) == 0 &&
		    strncmp(qname, altq->qname, PF_QNAME_SIZE) == 0)
			return (altq);
	}
	return (NULL);
}

u_int32_t
qname_to_qid(const char *qname, const char *ifname)
{
	struct pf_altq	*altq;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(ifname, altq->ifname, IFNAMSIZ) == 0 &&
		    strncmp(qname, altq->qname, PF_QNAME_SIZE) == 0)
			return (altq->qid);
	}
	return (0);
}

char *
qid_to_qname(u_int32_t qid, const char *ifname)
{
	struct pf_altq	*altq;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(ifname, altq->ifname, IFNAMSIZ) == 0 &&
		    altq->qid == qid)
			return (altq->qname);
	}
	return (NULL);
}

void
print_altq(const struct pf_altq *a, unsigned level)
{
	if (a->qname[0] != NULL) {
		print_queue(a, level);
		return;
	}

	printf("altq on %s ", a->ifname);

	switch(a->scheduler) {
	case ALTQT_CBQ:
		print_cbq_opts(a);
		if (!a->pq_u.cbq_opts.flags)
			printf("cbq ");
		break;
	case ALTQT_PRIQ:
		print_priq_opts(a);
		if (!a->pq_u.priq_opts.flags)
			printf("priq ");
		break;
	case ALTQT_HFSC:
		print_hfsc_opts(a);
		if (!a->pq_u.hfsc_opts.flags)
			printf("hfsc ");
		break;
	}

	printf("bandwidth %s ", rate2str((double)a->ifbandwidth));
	if (a->qlimit != DEFAULT_QLIMIT)
		printf("qlimit %u ", a->qlimit);
	printf("tbrsize %u ", a->tbrsize);
}

void
print_queue(const struct pf_altq *a, unsigned level)
{
	unsigned	i;

	printf("queue ");
	for (i = 0; i < level; ++i)
		printf(" ");
	printf("%s ", a->qname);
	if (a->scheduler == ALTQT_CBQ || a->scheduler == ALTQT_HFSC)
		printf("bandwidth %s ", rate2str((double)a->bandwidth));
	if (a->priority != DEFAULT_PRIORITY)
		printf("priority %u ", a->priority);
	if (a->qlimit != DEFAULT_QLIMIT)
		printf("qlimit %u ", a->qlimit);
	switch (a->scheduler) {
	case ALTQT_CBQ:
		print_cbq_opts(a);
		break;
	case ALTQT_PRIQ:
		print_priq_opts(a);
		break;
	case ALTQT_HFSC:
		print_hfsc_opts(a);
		break;
	}
}

/*
 * eval_pfaltq computes the discipline parameters.
 */
int
eval_pfaltq(struct pfctl *pf, struct pf_altq *pa, u_int32_t bw_absolute,
    u_int16_t bw_percent)
{
	u_int	rate, size, errors = 0;

	if (bw_absolute > 0)
		pa->ifbandwidth = bw_absolute;
	else
		if ((rate = getifspeed(pa->ifname)) == 0) {
			fprintf(stderr, "cannot determine interface bandwidth "
			    "for %s, specify an absolute bandwidth\n",
			    pa->ifname);
			errors++;
		} else
			if (bw_percent > 0)
				pa->ifbandwidth = rate / 100 * bw_percent;
			else
				pa->ifbandwidth = rate;

	/* if tbrsize is not specified, use heuristics */
	if (pa->tbrsize == 0) {
		rate = pa->ifbandwidth;
		if (rate <= 1 * 1000 * 1000)
			size = 1;
		else if (rate <= 10 * 1000 * 1000)
			size = 4;
		else if (rate <= 200 * 1000 * 1000)
			size = 8;
		else
			size = 24;
		size = size * getifmtu(pa->ifname);
		pa->tbrsize = size;
	}
	return (errors);
}

/*
 * check_commit_altq does consistency check for each interface
 */
int
check_commit_altq(int dev, int opts)
{
	struct pf_altq	*altq;
	int		 error = 0;

	/* call the discipline check for each interface. */
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (altq->qname[0] == 0) {
			switch (altq->scheduler) {
			case ALTQT_CBQ:
				error = check_commit_cbq(dev, opts, altq);
				break;
			case ALTQT_PRIQ:
				error = check_commit_priq(dev, opts, altq);
				break;
			case ALTQT_HFSC:
				error = check_commit_hfsc(dev, opts, altq);
				break;
			default:
				break;
			}
		}
	}
	return (error);
}

/*
 * eval_pfqueue computes the queue parameters.
 */
int
eval_pfqueue(struct pfctl *pf, struct pf_altq *pa, u_int32_t bw_absolute,
    u_int16_t bw_percent)
{
	/* should be merged with expand_queue */
	struct pf_altq	*if_pa, *parent;
	int		 error = 0;

	/* find the corresponding interface and copy fields used by queues */
	if_pa = pfaltq_lookup(pa->ifname);
	if (if_pa == NULL)
		errx(1, "altq not defined on %s", pa->ifname);
	pa->scheduler = if_pa->scheduler;
	pa->ifbandwidth = if_pa->ifbandwidth;

	parent = NULL;
	if (pa->parent[0] != 0) {
		parent = qname_to_pfaltq(pa->parent, pa->ifname);
		if (parent == NULL)
			errx(1, "parent %s not found for %s",
			    pa->parent, pa->qname);
		pa->parent_qid = parent->qid;
	}
	if (pa->qlimit == 0)
		pa->qlimit = DEFAULT_QLIMIT;

	if (pa->scheduler == ALTQT_CBQ || pa->scheduler == ALTQT_HFSC) {
		if (bw_absolute > 0)
			pa->bandwidth = bw_absolute;
		else if (bw_percent > 0 && parent != NULL)
			pa->bandwidth = parent->bandwidth / 100 * bw_percent;
		else
			errx(1, "bandwidth for %s invalid (%d / %d)", pa->qname,
			    bw_absolute, bw_percent);

		if (pa->bandwidth > pa->ifbandwidth)
			errx(1, "bandwidth for %s higher than interface",
			    pa->qname);
		if (parent != NULL && pa->bandwidth > parent->bandwidth)
			errx(1, "bandwidth for %s higher than parent",
			    pa->qname);
	}

	switch (pa->scheduler) {
	case ALTQT_CBQ:
		error = eval_pfqueue_cbq(pf, pa);
		break;
	case ALTQT_PRIQ:
		error = eval_pfqueue_priq(pf, pa);
		break;
	case ALTQT_HFSC:
		error = eval_pfqueue_hfsc(pf, pa);
		break;
	default:
		break;
	}
	return (error);
}

/*
 * CBQ support functions
 */
#define	RM_FILTER_GAIN	5	/* log2 of gain, e.g., 5 => 31/32 */
#define	RM_NS_PER_SEC	(1000000000)

static int
eval_pfqueue_cbq(struct pfctl *pf, struct pf_altq *pa)
{
	struct cbq_opts	*opts;
	u_int		 ifmtu;

	ifmtu = getifmtu(pa->ifname);
	opts = &pa->pq_u.cbq_opts;

	if (opts->pktsize == 0) {	/* use default */
		opts->pktsize = ifmtu;
		if (opts->pktsize > MCLBYTES)	/* do what TCP does */
			opts->pktsize &= ~MCLBYTES;
	} else if (opts->pktsize > ifmtu)
		opts->pktsize = ifmtu;
	if (opts->maxpktsize == 0)	/* use default */
		opts->maxpktsize = ifmtu;
	else if (opts->maxpktsize > ifmtu)
		opts->pktsize = ifmtu;

	if (opts->pktsize > opts->maxpktsize)
		opts->pktsize = opts->maxpktsize;

	if (pa->parent[0] == 0)
		opts->flags |= (CBQCLF_ROOTCLASS | CBQCLF_WRR);

	cbq_compute_idletime(pf, pa);
	return (0);
}

/*
 * compute ns_per_byte, maxidle, minidle, and offtime
 */
static int
cbq_compute_idletime(struct pfctl *pf, struct pf_altq *pa)
{
	struct cbq_opts	*opts;
	double		 maxidle_s, maxidle, minidle;
	double		 offtime, nsPerByte, ifnsPerByte, ptime, cptime;
	double		 z, g, f, gton, gtom, maxrate;
	u_int		 minburst, maxburst;

	opts = &pa->pq_u.cbq_opts;
	ifnsPerByte = (1.0 / (double)pa->ifbandwidth) * RM_NS_PER_SEC * 8;
	minburst = opts->minburst;
	maxburst = opts->maxburst;

	if (pa->bandwidth == 0)
		f = 0.0001;	/* small enough? */
	else
		f = ((double) pa->bandwidth / (double) pa->ifbandwidth);

	nsPerByte = ifnsPerByte / f;
	ptime = (double)opts->pktsize * ifnsPerByte;
	maxrate = f * ((double)pa->ifbandwidth / 8.0);
	cptime = ptime * (1.0 - f) / f;

	if (nsPerByte * (double)opts->maxpktsize > (double)INT_MAX) {
		/*
		 * this causes integer overflow in kernel!
		 * (bandwidth < 6Kbps when max_pkt_size=1500)
		 */
		if (pa->bandwidth != 0 && (pf->opts & PF_OPT_QUIET) == 0)
			warnx("queue bandwidth must be larger than 6Kb");
			fprintf(stderr, "cbq: queue %s is too slow!\n",
			    pa->qname);
		nsPerByte = (double)(INT_MAX / opts->maxpktsize);
	}

	if (maxburst == 0) {  /* use default */
		if (cptime > 10.0 * 1000000)
			maxburst = 4;
		else
			maxburst = 16;
	}
	if (minburst == 0)  /* use default */
		minburst = 2;
	if (minburst > maxburst)
		minburst = maxburst;

	z = (double)(1 << RM_FILTER_GAIN);
	g = (1.0 - 1.0 / z);
	gton = pow(g, (double)maxburst);
	gtom = pow(g, (double)(minburst-1));
	maxidle = ((1.0 / f - 1.0) * ((1.0 - gton) / gton));
	maxidle_s = (1.0 - g);
	if (maxidle > maxidle_s)
		maxidle = ptime * maxidle;
	else
		maxidle = ptime * maxidle_s;
	if (minburst)
		offtime = cptime * (1.0 + 1.0/(1.0 - g) * (1.0 - gtom) / gtom);
	else
		offtime = cptime;
	minidle = -((double)opts->maxpktsize * (double)nsPerByte);

	/* scale parameters */
	maxidle = ((maxidle * 8.0) / nsPerByte) * pow(2, RM_FILTER_GAIN);
	offtime = (offtime * 8.0) / nsPerByte * pow(2, RM_FILTER_GAIN);
	minidle = ((minidle * 8.0) / nsPerByte) * pow(2, RM_FILTER_GAIN);

	maxidle = maxidle / 1000.0;
	offtime = offtime / 1000.0;
	minidle = minidle / 1000.0;

	opts->minburst = minburst;
	opts->maxburst = maxburst;
	opts->ns_per_byte = (u_int) nsPerByte;
	opts->maxidle = (u_int) fabs(maxidle);
	opts->minidle = (int)minidle;
	opts->offtime = (u_int) fabs(offtime);

	return (0);
}

static int
check_commit_cbq(int dev, int opts, struct pf_altq *pa)
{
	struct pf_altq	*altq;
	int		 root_class, default_class;
	int		 error = 0;

	/*
	 * check if cbq has one root queue and one default queue
	 * for this interface
	 */
	root_class = default_class = 0;
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) != 0)
			continue;
		if (altq->qname[0] == 0)  /* this is for interface */
			continue;
		if (altq->pq_u.cbq_opts.flags & CBQCLF_ROOTCLASS)
			root_class++;
		if (altq->pq_u.cbq_opts.flags & CBQCLF_DEFCLASS)
			default_class++;
	}
	if (root_class != 1) {
		warnx("should have one root queue on %s", pa->ifname);
		error++;
	}
	if (default_class != 1) {
		warnx("should have one default queue on %s", pa->ifname);
		error++;
	}
	return (error);
}

static void
print_cbq_opts(const struct pf_altq *a)
{
	const struct cbq_opts	*opts;

	opts = &a->pq_u.cbq_opts;
	if (opts->flags) {
		printf("cbq(");
		if (opts->flags & CBQCLF_RED)
			printf(" red");
		if (opts->flags & CBQCLF_ECN)
			printf(" ecn");
		if (opts->flags & CBQCLF_RIO)
			printf(" rio");
		if (opts->flags & CBQCLF_CLEARDSCP)
			printf(" cleardscp");
		if (opts->flags & CBQCLF_FLOWVALVE)
			printf(" flowvalve");
		if (opts->flags & CBQCLF_BORROW)
			printf(" borrow");
		if (opts->flags & CBQCLF_WRR)
			printf(" wrr");
		if (opts->flags & CBQCLF_EFFICIENT)
			printf(" efficient");
		if (opts->flags & CBQCLF_ROOTCLASS)
			printf(" root");
		if (opts->flags & CBQCLF_DEFCLASS)
			printf(" default");
		if (opts->flags & CBQCLF_CTLCLASS)
			printf(" control");
		printf(" ) ");
	}
}

/*
 * PRIQ support functions
 */
static int
eval_pfqueue_priq(struct pfctl *pf, struct pf_altq *pa)
{
	struct pf_altq	*altq;

	if (pa->priority >= PRIQ_MAXPRI) {
		warnx("priority out of range: max %d", PRIQ_MAXPRI - 1);
		return (-1);
	}
	/* the priority should be unique for the interface */
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) == 0 &&
		    altq->qname[0] != 0 && altq->priority == pa->priority) {
			warnx("%s and %s have the same priority",
			    altq->qname, pa->qname);
			return (-1);
		}
	}

	return (0);
}

static int
check_commit_priq(int dev, int opts, struct pf_altq *pa)
{
	struct pf_altq	*altq;
	int		 default_class;
	int		 error = 0;

	/*
	 * check if priq has one default class for this interface
	 */
	default_class = 0;
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) != 0)
			continue;
		if (altq->qname[0] == 0)  /* this is for interface */
			continue;
		if (altq->pq_u.priq_opts.flags & PRCF_DEFAULTCLASS)
			default_class++;
	}
	if (default_class != 1) {
		warnx("should have one default queue on %s", pa->ifname);
		error++;
	}
	return (error);
}

static void
print_priq_opts(const struct pf_altq *a)
{
	const struct priq_opts	*opts;

	opts = &a->pq_u.priq_opts;

	if (opts->flags) {
		printf("priq(");
		if (opts->flags & PRCF_RED)
			printf(" red");
		if (opts->flags & PRCF_ECN)
			printf(" ecn");
		if (opts->flags & PRCF_RIO)
			printf(" rio");
		if (opts->flags & PRCF_CLEARDSCP)
			printf(" cleardscp");
		if (opts->flags & PRCF_DEFAULTCLASS)
			printf(" default");
		printf(" ) ");
	}
}

/*
 * HFSC support functions
 */
static int
eval_pfqueue_hfsc(struct pfctl *pf, struct pf_altq *pa)
{
	struct pf_altq		*altq, *parent;
	struct hfsc_opts	*opts;
	struct service_curve	 sc;

	if (pa->parent[0] == 0) {
		/* this is for dummy root */
		pa->qid = HFSC_ROOTCLASS_HANDLE;
		pa->pq_u.hfsc_opts.lssc_m2 = pa->ifbandwidth;
		return (0);
	}

	opts = &pa->pq_u.hfsc_opts;
	LIST_INIT(&rtsc);
	LIST_INIT(&lssc);

	/* if link_share is not specified, use bandwidth */
	if (opts->lssc_m2 == 0)
		opts->lssc_m2 = pa->bandwidth;

	if ((opts->rtsc_m1 > 0 && opts->rtsc_m2 == 0) ||
	    (opts->lssc_m1 > 0 && opts->lssc_m2 == 0) ||
	    (opts->ulsc_m1 > 0 && opts->ulsc_m2 == 0)) {
		warnx("m2 is zero for %s", pa->qname);
		return (-1);
	}

	if ((opts->rtsc_m1 < opts->rtsc_m2 && opts->rtsc_m1 != 0) ||
	    (opts->rtsc_m1 < opts->rtsc_m2 && opts->rtsc_m1 != 0) ||
	    (opts->rtsc_m1 < opts->rtsc_m2 && opts->rtsc_m1 != 0)) {
		warnx("m1 must be zero for convex curve: %s", pa->qname);
		return (-1);
	}

	/*
	 * admission control:
	 * for the real-time service curve, the sum of the service curves
	 * should not exceed 80% of the interface bandwidth.  20% is reserved
	 * not to over-commit the actual interface bandwidth.
	 * for the link-sharing service curve, the sum of the child service
	 * curve should not exceed the parent service curve.
	 * for the upper-limit service curve, the assigned bandwidth should
	 * be smaller than the interface bandwidth, and the upper-limit should
	 * be larger than the real-time service curve when both are defined.
	 */
	parent = qname_to_pfaltq(pa->parent, pa->ifname);
	if (parent == NULL)
		errx(1, "parent %s not found for %s", pa->parent, pa->qname);

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) != 0)
			continue;
		if (altq->qname[0] == 0)  /* this is for interface */
			continue;

		/* if the class has a real-time service curve, add it. */
		if (opts->rtsc_m2 != 0 && altq->pq_u.hfsc_opts.rtsc_m2 != 0) {
			sc.m1 = altq->pq_u.hfsc_opts.rtsc_m1;
			sc.d  = altq->pq_u.hfsc_opts.rtsc_d;
			sc.m2 = altq->pq_u.hfsc_opts.rtsc_m2;
			gsc_add_sc(&rtsc, &sc);
		}

		if (strncmp(altq->parent, pa->parent, PF_QNAME_SIZE) != 0)
			continue;

		/* if the class has a link-sharing service curve, add it. */
		if (opts->lssc_m2 != 0 && altq->pq_u.hfsc_opts.lssc_m2 != 0) {
			sc.m1 = altq->pq_u.hfsc_opts.lssc_m1;
			sc.d  = altq->pq_u.hfsc_opts.lssc_d;
			sc.m2 = altq->pq_u.hfsc_opts.lssc_m2;
			gsc_add_sc(&lssc, &sc);
		}
	}

	/* check the real-time service curve.  reserve 20% of interface bw */
	if (opts->rtsc_m2 != 0) {
		sc.m1 = 0;
		sc.d  = 0;
		sc.m2 = pa->ifbandwidth / 100 * 80;
		if (!is_gsc_under_sc(&rtsc, &sc)) {
			warnx("real-time sc exceeds the interface bandwidth");
			goto err_ret;
		}
	}

	/* check the link-sharing service curve. */
	if (opts->lssc_m2 != 0) {
		sc.m1 = parent->pq_u.hfsc_opts.lssc_m1;
		sc.d  = parent->pq_u.hfsc_opts.lssc_d;
		sc.m2 = parent->pq_u.hfsc_opts.lssc_m2;
		if (!is_gsc_under_sc(&lssc, &sc)) {
			warnx("link-sharing sc exceeds parent's sc");
			goto err_ret;
		}
	}

	/* check the upper-limit service curve. */
	if (opts->ulsc_m2 != 0) {
		if (opts->ulsc_m1 > pa->ifbandwidth ||
		    opts->ulsc_m2 > pa->ifbandwidth) {
			warnx("upper-limit larger than interface bandwidth");
			goto err_ret;
		}
		if (opts->rtsc_m2 != 0 && opts->rtsc_m2 > opts->ulsc_m2) {
			warnx("upper-limit sc smaller than real-time sc");
			goto err_ret;
		}
	}

	gsc_destroy(&rtsc);
	gsc_destroy(&lssc);

	return (0);

err_ret:
	gsc_destroy(&rtsc);
	gsc_destroy(&lssc);
	return (-1);
}

static int
check_commit_hfsc(int dev, int opts, struct pf_altq *pa)
{
	struct pf_altq	*altq, *def = NULL;
	int		 default_class;
	int		 error = 0;

	/* check if hfsc has one default queue for this interface */
	default_class = 0;
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) != 0)
			continue;
		if (altq->qname[0] == 0)  /* this is for interface */
			continue;
		if (altq->parent[0] == 0)  /* dummy root */
			continue;
		if (altq->pq_u.hfsc_opts.flags & HFCF_DEFAULTCLASS) {
			default_class++;
			def = altq;
		}
	}
	if (default_class != 1) {
		warnx("should have one default queue on %s", pa->ifname);
		error++;
	}
	/* make sure the default queue is a leaf */
	TAILQ_FOREACH(altq, &altqs, entries) {
		if (strncmp(altq->ifname, pa->ifname, IFNAMSIZ) != 0)
			continue;
		if (altq->qname[0] == 0)  /* this is for interface */
			continue;
		if (strncmp(altq->parent, def->qname, PF_QNAME_SIZE) == 0) {
			warnx("default queue is not a leaf");
			error++;
		}
	}
	return (error);
}

static void
print_hfsc_opts(const struct pf_altq *a)
{
	const struct hfsc_opts	*opts;

	opts = &a->pq_u.hfsc_opts;

	printf("hfsc(");
	if (opts->flags & HFCF_RED)
		printf(" red");
	if (opts->flags & HFCF_ECN)
		printf(" ecn");
	if (opts->flags & HFCF_RIO)
		printf(" rio");
	if (opts->flags & HFCF_CLEARDSCP)
		printf(" cleardscp");
	if (opts->flags & HFCF_DEFAULTCLASS)
		printf(" default");
	if (opts->rtsc_m2 != 0) {
		if (opts->rtsc_d != 0)
			printf(" realtime(%s %ums %s)",
			    rate2str((double)opts->rtsc_m1), opts->rtsc_d,
			    rate2str((double)opts->rtsc_m2));
		else
			printf(" realtime %s",
			    rate2str((double)opts->rtsc_m2));
	}
	if (opts->lssc_m2 != 0) {
		if (opts->lssc_d != 0)
			printf(" linkshare(%s %ums %s)",
			    rate2str((double)opts->lssc_m1), opts->lssc_d,
			    rate2str((double)opts->lssc_m2));
		else
			printf(" linkshare %s",
			    rate2str((double)opts->lssc_m2));
	}
	if (opts->ulsc_m2 != 0) {
		if (opts->ulsc_d != 0)
			printf(" upperlimit(%s %ums %s)",
			    rate2str((double)opts->ulsc_m1), opts->ulsc_d,
			    rate2str((double)opts->ulsc_m2));
			else
				printf(" upperlimit(%s)",
				    rate2str((double)opts->ulsc_m2));
	}
	printf(" ) ");
}

/*
 * admission control using generalized service curve
 */
#define	INFINITY	HUGE_VAL  /* positive infinity defined in <math.h> */

/* add a new service curve to a generilized service curve */
static void
gsc_add_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	if (is_sc_null(sc))
		return;
	if (sc->d != 0)
		gsc_add_seg(gsc, 0, 0, (double)sc->d, (double)sc->m1);
	gsc_add_seg(gsc, (double)sc->d, 0, INFINITY, (double)sc->m2);
}

/*
 * check whether all points of a generalized service curve have
 * their y-coordinates no larger than a given two-piece linear
 * service curve.
 */
static int
is_gsc_under_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	struct segment	*s, *last, *end;
	double		 y;

	if (is_sc_null(sc)) {
		if (LIST_EMPTY(gsc))
			return (1);
		LIST_FOREACH(s, gsc, _next) {
			if (s->m != 0)
				return (0);
		}
		return (1);
	}
	/*
	 * gsc has a dummy entry at the end with x = INFINITY.
	 * loop through up to this dummy entry.
	 */
	end = gsc_getentry(gsc, INFINITY);
	if (end == NULL)
		return (1);
	last = NULL;
	for (s = LIST_FIRST(gsc); s != end; s = LIST_NEXT(s, _next)) {
		if (s->y > sc_x2y(sc, s->x))
			return (0);
		last = s;
	}
	/* last now holds the real last segment */
	if (last == NULL)
		return (1);
	if (last->m > sc->m2)
		return (0);
	if (last->x < sc->d && last->m > sc->m1) {
		y = last->y + (sc->d - last->x) * last->m;
		if (y > sc_x2y(sc, sc->d))
			return (0);
	}
	return (1);
}

static void
gsc_destroy(struct gen_sc *gsc)
{
	struct segment	*s;

	while ((s = LIST_FIRST(gsc)) != NULL) {
		LIST_REMOVE(s, _next);
		free(s);
	}
}

/*
 * return a segment entry starting at x.
 * if gsc has no entry starting at x, a new entry is created at x.
 */
static struct segment *
gsc_getentry(struct gen_sc *gsc, double x)
{
	struct segment	*new, *prev, *s;

	prev = NULL;
	LIST_FOREACH(s, gsc, _next) {
		if (s->x == x)
			return (s);	/* matching entry found */
		else if (s->x < x)
			prev = s;
		else
			break;
	}

	/* we have to create a new entry */
	if ((new = calloc(1, sizeof(struct segment))) == NULL)
		return (NULL);

	new->x = x;
	if (x == INFINITY || s == NULL)
		new->d = 0;
	else if (s->x == INFINITY)
		new->d = INFINITY;
	else
		new->d = s->x - x;
	if (prev == NULL) {
		/* insert the new entry at the head of the list */
		new->y = 0;
		new->m = 0;
		LIST_INSERT_HEAD(gsc, new, _next);
	} else {
		/*
		 * the start point intersects with the segment pointed by
		 * prev.  divide prev into 2 segments
		 */
		if (x == INFINITY) {
			prev->d = INFINITY;
			if (prev->m == 0)
				new->y = prev->y;
			else
				new->y = INFINITY;
		} else {
			prev->d = x - prev->x;
			new->y = prev->d * prev->m + prev->y;
		}
		new->m = prev->m;
		LIST_INSERT_AFTER(prev, new, _next);
	}
	return (new);
}

/* add a segment to a generalized service curve */
static int
gsc_add_seg(struct gen_sc *gsc, double x, double y, double d, double m)
{
	struct segment	*start, *end, *s;
	double		 x2;

	if (d == INFINITY)
		x2 = INFINITY;
	else
		x2 = x + d;
	start = gsc_getentry(gsc, x);
	end   = gsc_getentry(gsc, x2);
	if (start == NULL || end == NULL)
		return (-1);

	for (s = start; s != end; s = LIST_NEXT(s, _next)) {
		s->m += m;
		s->y += y + (s->x - x) * m;
	}

	end = gsc_getentry(gsc, INFINITY);
	for (; s != end; s = LIST_NEXT(s, _next)) {
		s->y += m * d;
	}

	return (0);
}

/* get y-projection of a service curve */
static double
sc_x2y(struct service_curve *sc, double x)
{
	double	y;

	if (x <= (double)sc->d)
		/* y belongs to the 1st segment */
		y = x * (double)sc->m1;
	else
		/* y belongs to the 2nd segment */
		y = (double)sc->d * (double)sc->m1
			+ (x - (double)sc->d) * (double)sc->m2;
	return (y);
}

/*
 * misc utilities
 */
#define	R2S_BUFS	8
#define	RATESTR_MAX	16

static char *
rate2str(double rate)
{
	char		*buf;
	static char	 r2sbuf[R2S_BUFS][RATESTR_MAX];  /* ring bufer */
	static int	 idx = 0;

	buf = r2sbuf[idx++];
	if (idx == R2S_BUFS)
		idx = 0;

	if (rate == 0.0)
		snprintf(buf, RATESTR_MAX, "0");
	else if (rate >= 1000 * 1000 * 1000)
		snprintf(buf, RATESTR_MAX, "%.2fGb",
		    rate / (1000.0 * 1000.0 * 1000.0));
	else if (rate >= 1000 * 1000)
		snprintf(buf, RATESTR_MAX, "%.2fMb", rate / (1000.0 * 1000.0));
	else if (rate >= 1000)
		snprintf(buf, RATESTR_MAX, "%.2fKb", rate / 1000.0);
	else
		snprintf(buf, RATESTR_MAX, "%db", (int)rate);
	return (buf);
}

u_int32_t
getifspeed(char *ifname)
{
	int		s;
	struct ifreq	ifr;
	struct if_data	ifrdat;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_data = (caddr_t)&ifrdat;
	if (ioctl(s, SIOCGIFDATA, (caddr_t)&ifr) == -1)
		err(1, "SIOCGIFDATA");
	if (shutdown(s, SHUT_RDWR) == -1)
		err(1, "shutdown");
	return ((u_int32_t)ifrdat.ifi_baudrate);
}

u_long
getifmtu(char *ifname)
{
	int		s;
	struct ifreq	ifr;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) == -1)
		err(1, "SIOCGIFMTU");
	if (shutdown(s, SHUT_RDWR) == -1)
		err(1, "shutdown");
	if (ifr.ifr_mtu > 0)
		return (ifr.ifr_mtu);
	else {
		warnx("could not get mtu for %s, assuming 1500", ifname);
		return (1500);
	}
}
