/*	$OpenBSD: pfctl_qstats.c,v 1.12 2003/01/27 19:37:46 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <altq/altq.h>
#include <altq/altq_cbq.h>
#include <altq/altq_priq.h>
#include <altq/altq_hfsc.h>

#include "pfctl.h"

union class_stats {
	class_stats_t		cbq_stats;
	struct priq_classstats	priq_stats;
	struct hfsc_classstats	hfsc_stats;
};

struct queue_stats {
	union class_stats	 data;
	struct timeval		 timestamp;
	u_int8_t		 valid;
};

struct pf_altq_node {
	struct pf_altq		 altq;
	struct pf_altq_node	*next;
	struct pf_altq_node	*children;
	struct queue_stats	 qstats;
	struct queue_stats	 qstats_last;
};

int			 pfctl_update_qstats(int, struct pf_altq_node **);
void			 pfctl_insert_altq_node(struct pf_altq_node **,
			    const struct pf_altq, const struct queue_stats);
struct pf_altq_node	*pfctl_find_altq_node(struct pf_altq_node *,
			    const char *, const char *);
void			 pfctl_print_altq_node(int, const struct pf_altq_node *,
			     unsigned, int);
void			 print_cbqstats(struct queue_stats, struct queue_stats);
void			 print_priqstats(struct queue_stats,
			     struct queue_stats);
void			 pfctl_free_altq_node(struct pf_altq_node *);
void			 pfctl_print_altq_nodestat(int,
			    const struct pf_altq_node *);

double	calc_interval(struct timeval *, struct timeval *);
double	calc_rate(u_int64_t, u_int64_t, double);
double	calc_pps(u_int64_t, u_int64_t, double);

int
pfctl_show_altq(int dev, int verbose, int verbose2)
{
	struct pf_altq_node	*root = NULL, *node;

	if (pfctl_update_qstats(dev, &root))
		return (-1);

	for (node = root; node != NULL; node = node->next)
		pfctl_print_altq_node(dev, node, 0, verbose);

	while (verbose2) {
		printf("\n");
		sleep(5);
		if (pfctl_update_qstats(dev, &root))
			return (-1);
		for (node = root; node != NULL; node = node->next)
			pfctl_print_altq_node(dev, node, 0, verbose);
	}
	pfctl_free_altq_node(root);
	return (0);
}

int
pfctl_update_qstats(int dev, struct pf_altq_node **root)
{
	struct pf_altq_node	*node;
	struct pfioc_altq	 pa;
	struct pfioc_qstats	 pq;
	u_int32_t		 mnr, nr;
	struct queue_stats	 qstats;

	memset(&pa, 0, sizeof(pa));
	memset(&pq, 0, sizeof(pq));
	memset(&qstats, 0, sizeof(qstats));
	if (ioctl(dev, DIOCGETALTQS, &pa)) {
		warn("DIOCGETALTQS");
		return (-1);
	}
	mnr = pa.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pa.nr = nr;
		if (ioctl(dev, DIOCGETALTQ, &pa)) {
			warn("DIOCGETALTQ");
			return (-1);
		}
		if (pa.altq.qid > 0) {
			pq.nr = nr;
			pq.ticket = pa.ticket;
			pq.buf = &qstats;
			pq.nbytes = sizeof(qstats);
			if (ioctl(dev, DIOCGETQSTATS, &pq)) {
				warn("DIOCGETQSTATS");
				return (-1);
			}
			qstats.valid = 1;
			gettimeofday(&qstats.timestamp, NULL);
			if ((node = pfctl_find_altq_node(*root, pa.altq.qname,
			    pa.altq.ifname)) != NULL) {
				memcpy(&node->qstats_last, &node->qstats,
				    sizeof(struct queue_stats));
				memcpy(&node->qstats, &qstats,
				    sizeof(qstats));
			} else
				pfctl_insert_altq_node(root, pa.altq, qstats);
		}
	}
	return (0);
}

void
pfctl_insert_altq_node(struct pf_altq_node **root,
    const struct pf_altq altq, const struct queue_stats qstats)
{
	struct pf_altq_node	*node;

	node = calloc(1, sizeof(struct pf_altq_node));
	if (node == NULL)
		err(1, "pfctl_insert_altq_node: calloc");
	memcpy(&node->altq, &altq, sizeof(struct pf_altq));
	memcpy(&node->qstats, &qstats, sizeof(qstats));
	node->next = node->children = NULL;

	if (*root == NULL)
		*root = node;
	else if (!altq.parent[0]) {
		struct pf_altq_node	*prev = *root;

		while (prev->next != NULL)
			prev = prev->next;
		prev->next = node;
	} else {
		struct pf_altq_node	*parent;

		parent = pfctl_find_altq_node(*root, altq.parent, altq.ifname);
		if (parent == NULL)
			errx(1, "parent %s not found", altq.parent);
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
pfctl_find_altq_node(struct pf_altq_node *root, const char *qname,
    const char *ifname)
{
	struct pf_altq_node	*node, *child;

	for (node = root; node != NULL; node = node->next) {
		if (!strcmp(node->altq.qname, qname)
		    && !(strcmp(node->altq.ifname, ifname)))
			return (node);
		if (node->children != NULL) {
			child = pfctl_find_altq_node(node->children, qname,
			    ifname);
			if (child != NULL)
				return (child);
		}
	}
	return (NULL);
}

void
pfctl_print_altq_node(int dev, const struct pf_altq_node *node, unsigned level,
    int verbose)
{
	const struct pf_altq_node	*child;

	if (node == NULL)
		return;

	print_altq(&node->altq, level);

	if (node->children != NULL) {
		printf("{");
		for (child = node->children; child != NULL;
		    child = child->next) {
			printf("%s", child->altq.qname);
			if (child->next != NULL)
				printf(", ");
		}
		printf("}");
	}
	printf("\n");

	if (verbose)
		pfctl_print_altq_nodestat(dev, node);

	for (child = node->children; child != NULL;
	    child = child->next)
		pfctl_print_altq_node(dev, child, level+1, verbose);
}

void
pfctl_print_altq_nodestat(int dev, const struct pf_altq_node *a)
{
	if (a->altq.qid == 0)
		return;

	switch (a->altq.scheduler) {
	case ALTQT_CBQ:
		print_cbqstats(a->qstats, a->qstats_last);
		break;
	case ALTQT_PRIQ:
		print_priqstats(a->qstats, a->qstats_last);
		break;
	}
}

void
print_cbqstats(struct queue_stats cur, struct queue_stats last)
{
	double	interval;

	printf("[ pkts: %10llu  bytes: %10llu  "
	    "dropped pkts: %6llu bytes: %6llu ]\n",
	    cur.data.cbq_stats.xmit_cnt.packets,
	    cur.data.cbq_stats.xmit_cnt.bytes,
	    cur.data.cbq_stats.drop_cnt.packets,
	    cur.data.cbq_stats.drop_cnt.bytes);
	printf("[ qlength: %3d/%3d  borrows: %6u  suspends: %6u ]\n",
	    cur.data.cbq_stats.qcnt, cur.data.cbq_stats.qmax,
	    cur.data.cbq_stats.borrows, cur.data.cbq_stats.delays);

	if (!last.valid)
		return;

	interval = calc_interval(&cur.timestamp, &last.timestamp);
	printf("[ measured: %7.1f packets/s, %s/s ]\n",
	    calc_pps(cur.data.cbq_stats.xmit_cnt.packets,
		last.data.cbq_stats.xmit_cnt.packets, interval),
	    rate2str(calc_rate(cur.data.cbq_stats.xmit_cnt.bytes,
		last.data.cbq_stats.xmit_cnt.bytes, interval)));
}

void
print_priqstats(struct queue_stats cur, struct queue_stats last)
{
	double	interval;

	printf("[ pkts: %10llu  bytes: %10llu  "
	    "dropped pkts: %6llu bytes: %6llu ]\n",
	    cur.data.priq_stats.xmitcnt.packets,
	    cur.data.priq_stats.xmitcnt.bytes,
	    cur.data.priq_stats.dropcnt.packets,
	    cur.data.priq_stats.dropcnt.bytes);
	printf("[ qlength: %3d/%3d ]\n",
	    cur.data.priq_stats.qlength, cur.data.priq_stats.qlimit);

	if (!last.valid)
		return;

	interval = calc_interval(&cur.timestamp, &last.timestamp);
	printf("[ measured: %7.1f packets/s, %s/s ]\n",
	    calc_pps(cur.data.priq_stats.xmitcnt.packets,
		last.data.priq_stats.xmitcnt.packets, interval),
	    rate2str(calc_rate(cur.data.priq_stats.xmitcnt.bytes,
		last.data.priq_stats.xmitcnt.bytes, interval)));
}

void
pfctl_free_altq_node(struct pf_altq_node *node)
{
	while (node != NULL) {
		struct pf_altq_node	*prev;

		if (node->children != NULL)
			pfctl_free_altq_node(node->children);
		prev = node;
		node = node->next;
		free(prev);
	}
}

/* calculate interval in sec */
double
calc_interval(struct timeval *cur_time, struct timeval *last_time)
{
	double	sec;

	sec = (double)(cur_time->tv_sec - last_time->tv_sec) +
	    (double)(cur_time->tv_usec - last_time->tv_usec) / 1000000;
	return (sec);
}

/* calculate rate in bps */
double
calc_rate(u_int64_t new_bytes, u_int64_t last_bytes, double interval)
{
	double	rate;

	rate = (double)(new_bytes - last_bytes) * 8 / interval;
	return (rate);
}

/* calculate packets in second */
double
calc_pps(u_int64_t new_pkts, u_int64_t last_pkts, double interval)
{
	double	pps;

	pps = (double)(new_pkts - last_pkts) / interval;
	return (pps);
}
