/*	$OpenBSD: pfctl.c,v 1.94 2002/12/01 19:56:42 mcbride Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfctl_parser.h"
#include "pf_print_state.h"
#include "pfctl_altq.h"

void	 usage(void);
int	 pfctl_enable(int, int);
int	 pfctl_disable(int, int);
int	 pfctl_clear_stats(int, int);
int	 pfctl_clear_rules(int, int);
int	 pfctl_clear_nat(int, int);
int	 pfctl_clear_altq(int, int);
int	 pfctl_clear_states(int, int);
int	 pfctl_kill_states(int, int);
int	 pfctl_get_pool(int, struct pf_pool *, u_int32_t, u_int32_t, int);
void	 pfctl_clear_pool(struct pf_pool *);
int	 pfctl_show_rules(int, int, int);
int	 pfctl_show_nat(int);
int	 pfctl_show_altq(int);
int	 pfctl_show_states(int, u_int8_t, int);
int	 pfctl_show_status(int);
int	 pfctl_show_timeouts(int);
int	 pfctl_show_limits(int);
int	 pfctl_rules(int, char *, int);
int	 pfctl_debug(int, u_int32_t, int);
int	 pfctl_clear_rule_counters(int, int);
int	 pfctl_add_pool(struct pfctl *, struct pf_pool *, sa_family_t);

char	*clearopt;
char	*rulesopt;
char	*showopt;
char	*debugopt;
int	 state_killers;
char	*state_kill[2];
int	 loadopt = PFCTL_FLAG_ALL;

const char *infile;

static const struct {
	const char	*name;
	int		index;
} pf_limits[] = {
	{ "states",	PF_LIMIT_STATES },
	{ "frags",	PF_LIMIT_FRAGS },
	{ NULL,		0 }
};

struct pf_hint {
	const char	*name;
	int		timeout;
};
static const struct pf_hint pf_hint_normal[] = {
	{ "tcp.first",		2 * 60 },
	{ "tcp.opening",	30 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 },
	{ "tcp.finwait",	45 },
	{ "tcp.closed",		90 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_satellite[] = {
	{ "tcp.first",		3 * 60 },
	{ "tcp.opening",	30 + 5 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 + 5 },
	{ "tcp.finwait",	45 + 5 },
	{ "tcp.closed",		90 + 5 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_conservative[] = {
	{ "tcp.first",		60 * 60 },
	{ "tcp.opening",	15 * 60 },
	{ "tcp.established",	5 * 24 * 60 * 60 },
	{ "tcp.closing",	60 * 60 },
	{ "tcp.finwait",	10 * 60 },
	{ "tcp.closed",		3 * 60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_aggressive[] = {
	{ "tcp.first",		30 },
	{ "tcp.opening",	5 },
	{ "tcp.established",	5 * 60 * 60 },
	{ "tcp.closing",	60 },
	{ "tcp.finwait",	30 },
	{ "tcp.closed",		30 },
	{ NULL,			0 }
};

static const struct {
	const char *name;
	const struct pf_hint *hint;
} pf_hints[] = {
	{ "normal",		pf_hint_normal },
	{ "default",		pf_hint_normal },
	{ "satellite",		pf_hint_satellite },
	{ "high-latency",	pf_hint_satellite },
	{ "conservative",	pf_hint_conservative },
	{ "aggressive",		pf_hint_aggressive },
	{ NULL,			NULL }
};

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-AdeqhnNrROvz] [-f file] ", __progname);
	fprintf(stderr, "[-F modifier] [-k host]\n");
	fprintf(stderr, "             ");
	fprintf(stderr, "[-s modifier] [-x level]\n");
	exit(1);
}

int
pfctl_enable(int dev, int opts)
{
	if (ioctl(dev, DIOCSTART)) {
		if (errno == EEXIST)
			errx(1, "pf already enabled");
		else
			err(1, "DIOCSTART");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf enabled\n");

	if (ioctl(dev, DIOCSTARTALTQ)) {
		if (errno == EEXIST)
			errx(1, "altq already enabled");
		else
			err(1, "DIOCSTARTALTQ");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "altq enabled\n");

	return (0);
}

int
pfctl_disable(int dev, int opts)
{
	if (ioctl(dev, DIOCSTOP)) {
		if (errno == ENOENT)
			errx(1, "pf not enabled");
		else
			err(1, "DIOCSTOP");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf disabled\n");

	if (ioctl(dev, DIOCSTOPALTQ)) {
		if (errno == ENOENT)
			errx(1, "altq not enabled");
		else
			err(1, "DIOCSTOPALTQ");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "altq disabled\n");

	return (0);
}

int
pfctl_clear_stats(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSTATUS))
		err(1, "DIOCCLRSTATUS");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf: statistics cleared\n");
	return (0);
}

int
pfctl_clear_rules(int dev, int opts)
{
	struct pfioc_rule pr;

	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket))
		err(1, "DIOCBEGINRULES");
	else if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket))
		err(1, "DIOCCOMMITRULES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "rules cleared\n");
	return (0);
}

int
pfctl_clear_nat(int dev, int opts)
{
	struct pfioc_nat pn;
	struct pfioc_binat pb;
	struct pfioc_rdr pr;

	if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
		err(1, "DIOCBEGINNATS");
	else if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
		err(1, "DIOCCOMMITNATS");
	if (ioctl(dev, DIOCBEGINBINATS, &pb.ticket))
		err(1, "DIOCBEGINBINATS");
	else if (ioctl(dev, DIOCCOMMITBINATS, &pb.ticket))
		err(1, "DIOCCOMMITBINATS");
	else if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
		err(1, "DIOCBEGINRDRS");
	else if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
		err(1, "DIOCCOMMITRDRS");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "nat cleared\n");
	return (0);
}

int
pfctl_clear_altq(int dev, int opts)
{
	struct pfioc_altq pa;

	if (ioctl(dev, DIOCBEGINALTQS, &pa.ticket))
		err(1, "DIOCBEGINALTQS");
	else if (ioctl(dev, DIOCCOMMITALTQS, &pa.ticket))
		err(1, "DIOCCOMMITALTQS");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "altq cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSTATES))
		err(1, "DIOCCLRSTATES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "states cleared\n");
	return (0);
}

int
pfctl_kill_states(int dev, int opts)
{
	struct pfioc_state_kill psk;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	int killed, sources, dests;
	int ret_ga;

	killed = sources = dests = 0;

	memset(&psk, 0, sizeof(psk));
	memset(&psk.psk_src.addr.mask, 0xff, sizeof(psk.psk_src.addr.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));

	if ((ret_ga = getaddrinfo(state_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "%s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp[0] = res[0]; resp[0]; resp[0] = resp[0]->ai_next) {
		if (resp[0]->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp[0]->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp[0]->ai_addr;

		psk.psk_af = resp[0]->ai_family;
		sources++;

		if (psk.psk_af == AF_INET)
			psk.psk_src.addr.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (psk.psk_af == AF_INET6)
			psk.psk_src.addr.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family!?!?!");

		if (state_killers > 1) {
			dests = 0;
			memset(&psk.psk_dst.addr.mask, 0xff,
			    sizeof(psk.psk_dst.addr.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			if ((ret_ga = getaddrinfo(state_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "%s", gai_strerror(ret_ga));
				/* NOTREACHED */
			}
			for (resp[1] = res[1]; resp[1];
			    resp[1] = resp[1]->ai_next) {
				if (resp[1]->ai_addr == NULL)
					continue;
				if (psk.psk_af != resp[1]->ai_family)
					continue;

				if (memcmp(&last_dst, resp[1]->ai_addr,
				    sizeof(last_dst)) == 0)
					continue;
				last_dst = *(struct sockaddr *)resp[1]->ai_addr;

				dests++;

				if (psk.psk_af == AF_INET)
					psk.psk_dst.addr.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (psk.psk_af == AF_INET6)
					psk.psk_dst.addr.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else
					errx(1, "Unknown address family!?!?!");

				if (ioctl(dev, DIOCKILLSTATES, &psk))
					err(1, "DIOCKILLSTATES");
				killed += psk.psk_af;
				/* fixup psk.psk_af */
				psk.psk_af = resp[1]->ai_family;
			}
		} else {
			if (ioctl(dev, DIOCKILLSTATES, &psk))
				err(1, "DIOCKILLSTATES");
			killed += psk.psk_af;
			/* fixup psk.psk_af */
			psk.psk_af = res[0]->ai_family;
		}
	}

	freeaddrinfo(res[0]);
	if (res[1])
		freeaddrinfo(res[1]);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states from %d sources and %d "
		    "destinations\n", killed, sources, dests);
	return (0);
}

int
pfctl_get_pool(int dev, struct pf_pool *pool, u_int32_t nr,
    u_int32_t ticket, int id)
{
	struct pfioc_pooladdr pp;
	struct pf_pooladdr *pa;
	u_int32_t pnr, mpnr;

	pp.r_id = id;
	pp.r_num = nr;
	pp.ticket = ticket;
	if (ioctl(dev, DIOCGETADDRS, &pp)) {
		warnx("DIOCGETADDRS");
		return (-1);
	}
	mpnr = pp.nr;
	TAILQ_INIT(&pool->list);
	for (pnr = 0; pnr < mpnr; ++pnr) {
		pp.nr = pnr;
		if (ioctl(dev, DIOCGETADDR, &pp)) {
			warnx("DIOCGETADDR");
			return (-1);
		}
		pa = calloc(1, sizeof(struct pf_pooladdr));
		if (pa == NULL) {
			err(1, "calloc");
			return (-1);
		}
		bcopy(&pp.addr, pa, sizeof(struct pf_pooladdr));
		TAILQ_INSERT_HEAD(&pool->list, pa, entries);
	}

	return (0);
}

void
pfctl_clear_pool(struct pf_pool *pool)
{
	struct pf_pooladdr *pa;

	while ((pa = TAILQ_FIRST(&pool->list)) != NULL) {
		TAILQ_REMOVE(&pool->list, pa, entries);
		free(pa);
	}
}

int
pfctl_show_rules(int dev, int opts, int format)
{
	struct pfioc_rule pr;
	u_int32_t nr, mnr;

	if (ioctl(dev, DIOCGETRULES, &pr)) {
		warnx("DIOCGETRULES");
		return (-1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			warnx("DIOCGETRULE");
			return (-1);
		}

		if (pfctl_get_pool(dev, &pr.rule.rt_pool,
		    nr, pr.ticket, PF_POOL_RULE_RT) != 0)
			return (-1);

		switch (format) {
		case 1:
			if (pr.rule.label[0]) {
				if (opts & PF_OPT_VERBOSE)
					print_rule(&pr.rule);
				else
					printf("%s ", pr.rule.label);
				printf("%llu %llu %llu\n",
				    pr.rule.evaluations, pr.rule.packets,
				    pr.rule.bytes);
			}
			break;
		default:
			print_rule(&pr.rule);
			if (opts & PF_OPT_VERBOSE)
				printf("[ Evaluations: %-8llu  Packets: %-8llu  "
				    "Bytes: %-10llu  States: %-6u]\n\n",
				    pr.rule.evaluations, pr.rule.packets,
				    pr.rule.bytes, pr.rule.states);
		}
		pfctl_clear_pool(&pr.rule.rt_pool);
	}
	return (0);
}

int
pfctl_show_altq(int dev)
{
	struct pf_altq_node *root = NULL;

	struct pfioc_altq pa;
	u_int32_t mnr, nr;

	if (ioctl(dev, DIOCGETALTQS, &pa)) {
		warnx("DIOCGETALTQS");
		return (-1);
	}
	mnr = pa.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pa.nr = nr;
		if (ioctl(dev, DIOCGETALTQ, &pa)) {
			warnx("DIOCGETALTQ");
			return (-1);
		}
		pfctl_insert_altq_node(&root, pa.altq);
	}
	for (; root != NULL; root = root->next)
		pfctl_print_altq_node(root, 0);
	pfctl_free_altq_node(root);
	return (0);
}

int
pfctl_show_nat(int dev)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	struct pfioc_binat pb;
	u_int32_t mnr, nr;

	if (ioctl(dev, DIOCGETNATS, &pn)) {
		warnx("DIOCGETNATS");
		return (-1);
	}
	mnr = pn.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pn.nr = nr;
		if (ioctl(dev, DIOCGETNAT, &pn)) {
			warnx("DIOCGETNAT");
			return (-1);
		}
		if (pfctl_get_pool(dev, &pn.nat.rpool, nr,
		    pn.ticket, PF_POOL_NAT_R) != 0)
			return (-1);
		print_nat(&pn.nat);
		pfctl_clear_pool(&pn.nat.rpool);
	}
	if (ioctl(dev, DIOCGETRDRS, &pr)) {
		warnx("DIOCGETRDRS");
		return (-1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRDR, &pr)) {
			warnx("DIOCGETRDR");
			return (-1);
		}
		if (pfctl_get_pool(dev, &pr.rdr.rpool, nr,
		    pr.ticket, PF_POOL_RDR_R) != 0)
			return (-1);
		print_rdr(&pr.rdr);
		pfctl_clear_pool(&pr.rdr.rpool);
	}
	if (ioctl(dev, DIOCGETBINATS, &pb)) {
		warnx("DIOCGETBINATS");
		return (-1);
	}
	mnr = pb.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pb.nr = nr;
		if (ioctl(dev, DIOCGETBINAT, &pb)) {
			warnx("DIOCGETBINAT");
			return (-1);
		}
		print_binat(&pb.binat);
	}
	return (0);
}

int
pfctl_show_states(int dev, u_int8_t proto, int opts)
{
	struct pfioc_states ps;
	struct pf_state *p;
	char *inbuf = NULL;
	unsigned len = 0;
	int i;

	for (;;) {
		ps.ps_len = len;
		if (len) {
			ps.ps_buf = inbuf = realloc(inbuf, len);
			if (inbuf == NULL)
				err(1, "realloc");
		}
		if (ioctl(dev, DIOCGETSTATES, &ps) < 0) {
			warnx("DIOCGETSTATES");
			return (-1);
		}
		if (ps.ps_len + sizeof(struct pfioc_state) < len)
			break;
		if (len == 0 && ps.ps_len == 0)
			return (0);
		if (len == 0 && ps.ps_len != 0)
			len = ps.ps_len;
		if (ps.ps_len == 0)
			return (0);	/* no states */
		len *= 2;
	}
	p = ps.ps_states;
	for (i = 0; i < ps.ps_len; i += sizeof(*p)) {
		if (!proto || (p->proto == proto))
			print_state(p, opts);
		p++;
	}
	return (0);
}

int
pfctl_show_status(int dev)
{
	struct pf_status status;

	if (ioctl(dev, DIOCGETSTATUS, &status)) {
		warnx("DIOCGETSTATUS");
		return (-1);
	}
	print_status(&status);
	return (0);
}

int
pfctl_show_timeouts(int dev)
{
	struct pfioc_tm pt;
	int i;

	for (i = 0; pf_timeouts[i].name; i++) {
		pt.timeout = pf_timeouts[i].timeout;
		if (ioctl(dev, DIOCGETTIMEOUT, &pt))
			err(1, "DIOCGETTIMEOUT");
		printf("%-20s %10ds\n", pf_timeouts[i].name, pt.seconds);
	}
	return (0);

}

int
pfctl_show_limits(int dev)
{
	struct pfioc_limit pl;
	int i;

	for (i = 0; pf_limits[i].name; i++) {
		pl.index = i;
		if (ioctl(dev, DIOCGETLIMIT, &pl))
			err(1, "DIOCGETLIMIT");
		printf("%-10s ", pf_limits[i].name);
		if (pl.limit == UINT_MAX)
			printf("unlimited\n");
		else
			printf("hard limit %6u\n", pl.limit);
	}
	return (0);
}

/* callbacks for rule/nat/rdr/addr */
int
pfctl_add_pool(struct pfctl *pf, struct pf_pool *p, sa_family_t af)
{
	struct pf_pooladdr *pa;

	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCBEGINADDRS, &pf->paddr.ticket))
			err(1, "DIOCBEGINADDRS");
	}

	pf->paddr.af = af;
	TAILQ_FOREACH(pa, &p->list, entries) {
		memcpy(&pf->paddr.addr, pa, sizeof(struct pf_pooladdr));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCADDADDR, &pf->paddr))
				err(1, "DIOCADDADDR");
		}
	}
	return (0);
}

int
pfctl_add_rule(struct pfctl *pf, struct pf_rule *r)
{
	if ((loadopt & (PFCTL_FLAG_FILTER | PFCTL_FLAG_ALL)) != 0) {
		if (pfctl_add_pool(pf, &r->rt_pool, r->af))
			return (1);
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			memcpy(&pf->prule->rule, r, sizeof(pf->prule->rule));
			pf->prule->pool_ticket = pf->paddr.ticket;
			if (ioctl(pf->dev, DIOCADDRULE, pf->prule))
				err(1, "DIOCADDRULE");
		}
		if (pf->opts & PF_OPT_VERBOSE)
			print_rule(r);
		pfctl_clear_pool(&r->rt_pool);
	}
	return (0);
}

int
pfctl_add_nat(struct pfctl *pf, struct pf_nat *n)
{
	if ((loadopt & (PFCTL_FLAG_NAT | PFCTL_FLAG_ALL)) != 0) {
		if (pfctl_add_pool(pf, &n->rpool, n->af))
			return (1);
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			memcpy(&pf->pnat->nat, n, sizeof(pf->pnat->nat));
			pf->pnat->pool_ticket = pf->paddr.ticket;
			if (ioctl(pf->dev, DIOCADDNAT, pf->pnat))
				err(1, "DIOCADDNAT");
		}
		if (pf->opts & PF_OPT_VERBOSE)
			print_nat(n);
		pfctl_clear_pool(&n->rpool);
	}
	return (0);
}

int
pfctl_add_binat(struct pfctl *pf, struct pf_binat *b)
{
	if ((loadopt & (PFCTL_FLAG_NAT | PFCTL_FLAG_ALL)) != 0) {
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			memcpy(&pf->pbinat->binat, b,
			    sizeof(pf->pbinat->binat));
			if (ioctl(pf->dev, DIOCADDBINAT, pf->pbinat))
				err(1, "DIOCADDBINAT");
		}
		if (pf->opts & PF_OPT_VERBOSE)
			print_binat(b);
	}
	return (0);
}

int
pfctl_add_rdr(struct pfctl *pf, struct pf_rdr *r)
{
	if ((loadopt & (PFCTL_FLAG_NAT | PFCTL_FLAG_ALL)) != 0) {
		if (pfctl_add_pool(pf, &r->rpool, r->af))
			return (1);
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			memcpy(&pf->prdr->rdr, r, sizeof(pf->prdr->rdr));
			pf->prdr->pool_ticket = pf->paddr.ticket;
			if (ioctl(pf->dev, DIOCADDRDR, pf->prdr))
				err(1, "DIOCADDRDR");
		}
		if (pf->opts & PF_OPT_VERBOSE)
			print_rdr(r);
		pfctl_clear_pool(&r->rpool);
	}
	return (0);
}

int
pfctl_add_altq(struct pfctl *pf, struct pf_altq *a)
{
	if ((loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) != 0) {
		memcpy(&pf->paltq->altq, a, sizeof(struct pf_altq));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCADDALTQ, pf->paltq))
				err(1, "DIOCADDALTQ");
		}
		pfaltq_store(&pf->paltq->altq);
	}
	return (0);
}

int
pfctl_rules(int dev, char *filename, int opts)
{
	FILE *fin;
	struct pfioc_nat	pn;
	struct pfioc_binat	pb;
	struct pfioc_rdr	pr;
	struct pfioc_rule	pl;
	struct pfioc_altq	pa;
	struct pfctl		pf;

	if (strcmp(filename, "-") == 0) {
		fin = stdin;
		infile = "stdin";
	} else {
		fin = fopen(filename, "r");
		infile = filename;
	}
	if (fin == NULL) {
		warn("%s", filename);
		return (1);
	}
	if ((opts & PF_OPT_NOACTION) == 0) {
		if ((loadopt & (PFCTL_FLAG_NAT | PFCTL_FLAG_ALL)) != 0) {
			if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
				err(1, "DIOCBEGINNATS");
			if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
				err(1, "DIOCBEGINRDRS");
			if (ioctl(dev, DIOCBEGINBINATS, &pb.ticket))
				err(1, "DIOCBEGINBINATS");
		}
		if (((loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) != 0) &&
		    ioctl(dev, DIOCBEGINALTQS, &pa.ticket))
			err(1, "DIOCBEGINALTQS");
		if (((loadopt & (PFCTL_FLAG_FILTER | PFCTL_FLAG_ALL)) != 0) &&
		    ioctl(dev, DIOCBEGINRULES, &pl.ticket))
			err(1, "DIOCBEGINRULES");
	}
	/* fill in callback data */
	pf.dev = dev;
	pf.opts = opts;
	pf.pnat = &pn;
	pf.pbinat = &pb;
	pf.prdr = &pr;
	pf.paltq = &pa;
	pf.prule = &pl;
	pf.rule_nr = 0;
	if (parse_rules(fin, &pf) < 0)
		errx(1, "Syntax error in file: pf rules not loaded");
	if ((loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) != 0)
		if (check_commit_altq(dev, opts) != 0)
			errx(1, "errors in altq config");
	if ((opts & PF_OPT_NOACTION) == 0) {
		if ((loadopt & (PFCTL_FLAG_NAT | PFCTL_FLAG_ALL)) != 0) {
			if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
				err(1, "DIOCCOMMITNATS");
			if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
				err(1, "DIOCCOMMITRDRS");
			if (ioctl(dev, DIOCCOMMITBINATS, &pb.ticket))
				err(1, "DIOCCOMMITBINATS");
		}
		if (((loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) != 0) &&
		    ioctl(dev, DIOCCOMMITALTQS, &pa.ticket))
			err(1, "DIOCCOMMITALTQS");
		if (((loadopt & (PFCTL_FLAG_FILTER | PFCTL_FLAG_ALL)) != 0) &&
		    ioctl(dev, DIOCCOMMITRULES, &pl.ticket))
			err(1, "DIOCCOMMITRULES");
#if 0
		if ((opts & PF_OPT_QUIET) == 0) {
			fprintf(stderr, "%u nat entries loaded\n", n);
			fprintf(stderr, "%u rdr entries loaded\n", r);
			fprintf(stderr, "%u binat entries loaded\n", b);
			fprintf(stderr, "%u rules loaded\n", n);
		}
#endif
	}
	if (fin != stdin)
		fclose(fin);
	return (0);
}

int
pfctl_set_limit(struct pfctl *pf, const char *opt, unsigned int limit)
{
	struct pfioc_limit pl;
	int i;

	if ((loadopt & (PFCTL_FLAG_OPTION | PFCTL_FLAG_ALL)) != 0) {
		for (i = 0; pf_limits[i].name; i++) {
			if (strcasecmp(opt, pf_limits[i].name) == 0) {
				pl.index = i;
				pl.limit = limit;
				if ((pf->opts & PF_OPT_NOACTION) == 0) {
					if (ioctl(pf->dev, DIOCSETLIMIT, &pl)) {
						if (errno == EBUSY) {
							warnx("Current pool "
							    "size exceeds "
							    "requested "
							    "hard limit");
							return (1);
						} else
							err(1, "DIOCSETLIMIT");
					}
				}
				break;
			}
		}
		if (pf_limits[i].name == NULL) {
			warnx("Bad pool name.");
			return (1);
		}
	}
	return (0);
}

int
pfctl_set_timeout(struct pfctl *pf, const char *opt, int seconds)
{
	struct pfioc_tm pt;
	int i;

	if ((loadopt & (PFCTL_FLAG_OPTION | PFCTL_FLAG_ALL)) != 0) {
		for (i = 0; pf_timeouts[i].name; i++) {
			if (strcasecmp(opt, pf_timeouts[i].name) == 0) {
				pt.timeout = pf_timeouts[i].timeout;
				break;
			}
		}

		if (pf_timeouts[i].name == NULL) {
			warnx("Bad timeout name.");
			return (1);
		}

		pt.seconds = seconds;
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCSETTIMEOUT, &pt))
				err(1, "DIOCSETTIMEOUT");
		}
	}
	return (0);
}

int
pfctl_set_optimization(struct pfctl *pf, const char *opt)
{
	const struct pf_hint *hint;
	int i, r;

	if ((loadopt & (PFCTL_FLAG_OPTION | PFCTL_FLAG_ALL)) != 0) {
		for (i = 0; pf_hints[i].name; i++)
			if (strcasecmp(opt, pf_hints[i].name) == 0)
				break;

		hint = pf_hints[i].hint;
		if (hint == NULL) {
			warnx("Bad hint name.");
			return (1);
		}

		for (i = 0; hint[i].name; i++)
			if ((r = pfctl_set_timeout(pf, hint[i].name,
			    hint[i].timeout)))
				return (r);
	}
	return (0);
}

int
pfctl_set_logif(struct pfctl *pf, char *ifname)
{
	struct pfioc_if pi;

	if ((loadopt & (PFCTL_FLAG_OPTION | PFCTL_FLAG_ALL)) != 0) {
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (!strcmp(ifname, "none"))
				bzero(pi.ifname, sizeof(pi.ifname));
			else
				strlcpy(pi.ifname, ifname, sizeof(pi.ifname));
			if (ioctl(pf->dev, DIOCSETSTATUSIF, &pi))
				return (1);
		}
	}
	return (0);
}

int
pfctl_debug(int dev, u_int32_t level, int opts)
{
	if (ioctl(dev, DIOCSETDEBUG, &level))
		err(1, "DIOCSETDEBUG");
	if ((opts & PF_OPT_QUIET) == 0) {
		fprintf(stderr, "debug level set to '");
		switch (level) {
		case PF_DEBUG_NONE:
			fprintf(stderr, "none");
			break;
		case PF_DEBUG_URGENT:
			fprintf(stderr, "urgent");
			break;
		case PF_DEBUG_MISC:
			fprintf(stderr, "misc");
			break;
		default:
			fprintf(stderr, "<invalid>");
			break;
		}
		fprintf(stderr, "'\n");
	}
	return (0);
}

int
pfctl_clear_rule_counters(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRRULECTRS))
		err(1, "DIOCCLRRULECTRS");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf: rule counters cleared\n");
	return (0);
}

int
main(int argc, char *argv[])
{
	int error = 0;
	int dev = -1;
	int ch;
	int mode = O_RDONLY;
	int opts = 0;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "Adeqf:F:hk:nNOrRs:vx:z")) != -1) {
		switch (ch) {
		case 'd':
			opts |= PF_OPT_DISABLE;
			mode = O_RDWR;
			break;
		case 'e':
			opts |= PF_OPT_ENABLE;
			mode = O_RDWR;
			break;
		case 'q':
			opts |= PF_OPT_QUIET;
			break;
		case 'F':
			clearopt = optarg;
			mode = O_RDWR;
			break;
		case 'k':
			if (state_killers >= 2) {
				warnx("can only specify -k twice");
				usage();
				/* NOTREACHED */
			}
			state_kill[state_killers++] = optarg;
			mode = O_RDWR;
			break;
		case 'n':
			opts |= PF_OPT_NOACTION;
			break;
		case 'N':
			loadopt &= ~PFCTL_FLAG_ALL;
			loadopt |= PFCTL_FLAG_NAT;
			break;
		case 'r':
			opts |= PF_OPT_USEDNS;
			break;
		case 'f':
			rulesopt = optarg;
			mode = O_RDWR;
			break;
		case 'A':
			loadopt &= ~PFCTL_FLAG_ALL;
			loadopt |= PFCTL_FLAG_ALTQ;
			break;
		case 'R':
			loadopt &= ~PFCTL_FLAG_ALL;
			loadopt |= PFCTL_FLAG_FILTER;
			break;
		case 'O':
			loadopt &= ~PFCTL_FLAG_ALL;
			loadopt |= PFCTL_FLAG_OPTION;
			break;
		case 's':
			showopt = optarg;
			break;
		case 'v':
			opts |= PF_OPT_VERBOSE;
			break;
		case 'x':
			debugopt = optarg;
			mode = O_RDWR;
			break;
		case 'z':
			opts |= PF_OPT_CLRRULECTRS;
			mode = O_RDWR;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (argc != optind) {
		warnx("unknown command line argument: %s ...", argv[optind]);
		usage();
		/* NOTREACHED */
	}

	if (opts & PF_OPT_NOACTION)
		mode = O_RDONLY;
	if ((opts & PF_OPT_NOACTION) == 0) {
		dev = open("/dev/pf", mode);
		if (dev == -1)
			err(1, "open(\"/dev/pf\")");
	} else {
		/* turn off options */
		opts &= ~ (PF_OPT_DISABLE | PF_OPT_ENABLE);
		clearopt = showopt = debugopt = NULL;
	}

	if (opts & PF_OPT_DISABLE)
		if (pfctl_disable(dev, opts))
			error = 1;

	if (clearopt != NULL) {
		switch (*clearopt) {
		case 'r':
			pfctl_clear_rules(dev, opts);
			break;
		case 'n':
			pfctl_clear_nat(dev, opts);
			break;
		case 'q':
			pfctl_clear_altq(dev, opts);
			break;
		case 's':
			pfctl_clear_states(dev, opts);
			break;
		case 'i':
			pfctl_clear_stats(dev, opts);
			break;
		case 'a':
			pfctl_clear_rules(dev, opts);
			pfctl_clear_nat(dev, opts);
			pfctl_clear_altq(dev, opts);
			pfctl_clear_states(dev, opts);
			pfctl_clear_stats(dev, opts);
			break;
		default:
			warnx("Unknown flush modifier '%s'", clearopt);
			error = 1;
		}
	}
	if (state_killers)
		pfctl_kill_states(dev, opts);

	if (rulesopt != NULL)
		if (pfctl_rules(dev, rulesopt, opts))
			error = 1;

	if (showopt != NULL) {
		switch (*showopt) {
		case 'r':
			pfctl_show_rules(dev, opts, 0);
			break;
		case 'l':
			pfctl_show_rules(dev, opts, 1);
			break;
		case 'n':
			pfctl_show_nat(dev);
			break;
		case 'q':
			pfctl_show_altq(dev);
			break;
		case 's':
			pfctl_show_states(dev, 0, opts);
			break;
		case 'i':
			pfctl_show_status(dev);
			break;
		case 't':
			pfctl_show_timeouts(dev);
			break;
		case 'm':
			pfctl_show_limits(dev);
			break;
		case 'a':
			pfctl_show_rules(dev, opts, 0);
			pfctl_show_nat(dev);
			pfctl_show_altq(dev);
			pfctl_show_states(dev, 0, opts);
			pfctl_show_status(dev);
			pfctl_show_rules(dev, opts, 1);
			pfctl_show_timeouts(dev);
			pfctl_show_limits(dev);
			break;
		default:
			warnx("Unknown show modifier '%s'", showopt);
			error = 1;
		}
	}

	if (opts & PF_OPT_ENABLE)
		if (pfctl_enable(dev, opts))
			error = 1;

	if (debugopt != NULL) {
		switch (*debugopt) {
		case 'n':
			pfctl_debug(dev, PF_DEBUG_NONE, opts);
			break;
		case 'u':
			pfctl_debug(dev, PF_DEBUG_URGENT, opts);
			break;
		case 'm':
			pfctl_debug(dev, PF_DEBUG_MISC, opts);
			break;
		default:
			warnx("Unknown debug level '%s'", debugopt);
			error = 1;
		}
	}

	if (opts & PF_OPT_CLRRULECTRS) {
		if (pfctl_clear_rule_counters(dev, opts))
			error = 1;
	}
	close(dev);
	exit(error);
}
