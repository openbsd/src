/*	$OpenBSD: pfctl_altq.c,v 1.6 2002/11/19 17:41:19 henning Exp $	*/
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <altq/altq.h>
#include <altq/altq_cbq.h>

#include "pfctl_parser.h"
#include "pfctl_altq.h"

static int eval_pfqueue_cbq(struct pfctl *pf, struct pf_altq *);
static int cbq_compute_idletime(struct pfctl *, struct pf_altq *);
static int check_commit_cbq(int, int, struct pf_altq *);
static void print_cbq_opts(const struct pf_altq *);
static char *rate2str(double);

TAILQ_HEAD(altqs, pf_altq) altqs = TAILQ_HEAD_INITIALIZER(altqs);

void
pfaltq_store(struct pf_altq *a)
{
	struct pf_altq *altq;

	if ((altq = malloc(sizeof(*altq))) == NULL)
		err(1, "malloc");
	memcpy(altq, a, sizeof(struct pf_altq));
	TAILQ_INSERT_TAIL(&altqs, altq, entries);
}

void
pfaltq_free(struct pf_altq *a)
{
	struct pf_altq *altq;

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
	struct pf_altq *altq;

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
	struct pf_altq *altq;

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
	struct pf_altq *altq;

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
	struct pf_altq *altq;

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

	printf("altq on %s scheduler %u bandwidth %s tbrsize %u",
	    a->ifname, a->scheduler, rate2str((double)a->ifbandwidth),
	    a->tbrsize);
}

void
print_queue(const struct pf_altq *a, unsigned level)
{
	unsigned i;

	for (i = 0; i < level; ++i)
		printf("  ");
	printf("queue %s bandwidth %s priority %u", a->qname,
	    rate2str((double)a->bandwidth), a->priority);
/*	printf("queue on %s %s parent 0x%x priority %u bandwidth %s"
	    " qlimit %u qid 0x%x\n",
	    a->ifname, a->qname, a->parent_qid, a->priority,
	    rate2str((double)a->bandwidth), a->qlimit, a->qid); */
	switch (a->scheduler) {
	case ALTQT_CBQ:
		print_cbq_opts(a);
		break;
	}
}

int
eval_pfaltq(struct pfctl *pf, struct pf_altq *pa)
{
	u_int rate, size;

	/* if tbrsize is not specified, use heuristics */
	if (pa->tbrsize == 0) {
		rate = pa->ifbandwidth;
		if (rate <= 1 * 1024 * 1024)
			size = 1;
		else if (rate <= 10 * 1024 * 1024)
			size = 4;
		else if (rate <= 200 * 1024 * 1024)
			size = 8;
		else
			size = 24;
		size = size * 1500;  /* assume the default mtu is 1500 */
		pa->tbrsize = size;
	}
	return (0);
}

int
check_commit_altq(int dev, int opts)
{
	struct pf_altq *altq;
	int error = 0;

	TAILQ_FOREACH(altq, &altqs, entries) {
		if (altq->qname[0] == 0) {
			switch (altq->scheduler) {
			case ALTQT_CBQ:
				error = check_commit_cbq(dev, opts, altq);
				break;
			default:
				break;
			}
		}
	}
	return (error);
}

int
eval_pfqueue(struct pfctl *pf, struct pf_altq *pa, u_int32_t bw_absolute,
    u_int16_t bw_percent)
{
	/* should be merged with expand_queue */
	struct pf_altq *if_pa, *parent;
	int error = 0;

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
		pa->qlimit = 50;

	if (bw_absolute > 0)
		pa->bandwidth = bw_absolute;
	else if (bw_percent > 0 && parent != NULL)
		pa->bandwidth = parent->bandwidth / 100 * bw_percent;
	else
		errx(1, "bandwidth for %s invalid (%d / %d)", pa->qname,
		    bw_absolute, bw_percent);

	/*
	 * admission control: bandwidth should be smaller than the
	 * interface bandwidth and the parent bandwidth
	 */
	if (pa->bandwidth > pa->ifbandwidth)
		errx(1, "bandwidth for %s higher than interface", pa->qname);
	if (parent != NULL && pa->bandwidth > parent->bandwidth)
		errx(1, "bandwidth for %s higher than parent", pa->qname);

	switch (pa->scheduler) {
	case ALTQT_CBQ:
		error = eval_pfqueue_cbq(pf, pa);
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
	struct cbq_opts *opts;
	u_int ifmtu;

#if 1
	ifmtu = 1500;	/* should be obtained from the interface */
#endif
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

	if (pa->parent[0] == 0 || strcasecmp("NULL", pa->parent) == 0)
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
	struct cbq_opts *opts;
	double maxidle_s, maxidle, minidle,
	    offtime, nsPerByte, ifnsPerByte, ptime, cptime;
	double z, g, f, gton, gtom, maxrate;
	u_int minburst, maxburst;

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
			fprintf(stderr, "cbq: class %s is too slow!\n",
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
	struct pf_altq *altq;
	int root_class, default_class;
	int error = 0;

	/*
	 * check if cbq has one root class and one default class
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
		warnx("should have one root class on %s", pa->ifname);
		error++;
	}
	if (default_class != 1) {
		warnx("should have one default class on %s", pa->ifname);
		error++;
	}
	return (error);
}

static void
print_cbq_opts(const struct pf_altq *a)
{
	const struct cbq_opts *opts;

	opts = &a->pq_u.cbq_opts;

/*	printf("  cbq options: minburst %u maxburst %u"
	    " pktsize %u maxpktsize %u\n",
	    opts->minburst, opts->maxburst,
	    opts->pktsize, opts->maxpktsize);

	printf("        ns_per_byte %u maxidle %u minidle %d offtime %u\n",
	    opts->ns_per_byte, opts->maxidle, opts->minidle, opts->offtime);
*/

	if (opts->flags) {
		printf(" cbq(");
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
		printf(" )");
	}
}

void
pfctl_insert_altq_node(struct pf_altq_node **root,
    const struct pf_altq altq)
{
	struct pf_altq_node *node;

	node = calloc(1, sizeof(struct pf_altq_node));
	if (node == NULL) {
		errx(1, "pfctl_insert_altq_node: calloc");
		return;
	}
	memcpy(&node->altq, &altq, sizeof(struct pf_altq));
	node->next = node->children = NULL;

	if (*root == NULL)
		*root = node;
	else if (!altq.parent[0]) {
		struct pf_altq_node *prev = *root;

		while (prev->next != NULL)
			prev = prev->next;
		prev->next = node;
	} else {
		struct pf_altq_node *parent;

		parent = pfctl_find_altq_node(*root, altq.parent);
		if (parent == NULL) {
			errx(1, "parent %s not found", altq.parent);
			return;
		}
		if (parent->children == NULL)
			parent->children = node;
		else {
			struct pf_altq_node *prev = parent->children;

			while (prev->next != NULL)
				prev = prev->next;
			prev->next = node;
		}
	}
}

struct pf_altq_node *
pfctl_find_altq_node(struct pf_altq_node *root, const char *qname)
{
	struct pf_altq_node *node, *child;

	for (node = root; node != NULL; node = node->next) {
		if (!strcmp(node->altq.qname, qname))
			return (node);
		if (node->children != NULL) {
			child = pfctl_find_altq_node(node->children, qname);
			if (child != NULL)
				return (child);
		}
	}
	return (NULL);
}

void
pfctl_print_altq_node(const struct pf_altq_node *node, unsigned level)
{
	const struct pf_altq_node *child;

	if (node == NULL)
		return;

	print_altq(&node->altq, level);

	if (node->children != NULL) {
		printf(" {");
		for (child = node->children; child != NULL;
		    child = child->next) {
			printf("%s", child->altq.qname);
			if (child->next != NULL)
				printf(", ");
		}
		printf("}");
	}
	printf("\n");
	for (child = node->children; child != NULL;
	    child = child->next)
		pfctl_print_altq_node(child, level+2);
}

void
pfctl_free_altq_node(struct pf_altq_node *node)
{
	while (node != NULL) {
		struct pf_altq_node *prev;

		if (node->children != NULL)
			pfctl_free_altq_node(node->children);
		prev = node;
		node = node->next;
		free(prev);
	}
}

/*
 * misc utilities
 */
#define	R2S_BUFS	8
#define	RATESTR_MAX	16
static char *
rate2str(double rate)
{
	char *buf;
	static char r2sbuf[R2S_BUFS][RATESTR_MAX];  /* ring bufer */
	static int idx = 0;

	buf = r2sbuf[idx++];
	if (idx == R2S_BUFS)
		idx = 0;

	if (rate == 0.0)
		snprintf(buf, RATESTR_MAX, "0");
	else if (rate >= 1024 * 1024 * 1024)
		snprintf(buf, RATESTR_MAX, "%.2fGb",
		    rate / (1024.0 * 1024.0 * 1024.0));
	else if (rate >= 1024 * 1024)
		snprintf(buf, RATESTR_MAX, "%.2fMb", rate / (1024.0 * 1024.0));
	else if (rate >= 1024)
		snprintf(buf, RATESTR_MAX, "%.2fKb", rate / 1024.0);
	else
		snprintf(buf, RATESTR_MAX, "%db", (int)rate);
	return (buf);
}
