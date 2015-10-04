/*	$OpenBSD: printconf.c,v 1.2 2015/10/04 22:54:38 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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

#include <stdio.h>
#include <arpa/inet.h>

#include "eigrp.h"
#include "eigrpd.h"
#include "eigrpe.h"
#include "log.h"

void	print_mainconf(struct eigrpd_conf *);
const char *print_no(uint16_t);
void	print_redist_metric(struct redist_metric *);
void	print_redistribute(struct eigrp *);
void	print_iface(struct eigrp_iface *);
void	print_as(struct eigrp *);
void	print_af(struct eigrpd_conf *, int);

void
print_mainconf(struct eigrpd_conf *conf)
{
	printf("router-id %s\n", inet_ntoa(conf->rtr_id));

	if (conf->flags & EIGRPD_FLAG_NO_FIB_UPDATE)
		printf("fib-update no\n");
	else
		printf("fib-update yes\n");

	printf("rdomain %u\n", conf->rdomain);
	printf("fib-priority-internal %u\n", conf->fib_priority_internal);
	printf("fib-priority-external %u\n", conf->fib_priority_external);
}

const char *
print_no(uint16_t type)
{
	if (type & REDIST_NO)
		return ("no ");
	else
		return ("");
}

void
print_redist_metric(struct redist_metric *metric)
{
	printf(" %u %u %u %u %u", metric->bandwidth, metric->delay,
	    metric->reliability, metric->load, metric->mtu);
}

void
print_redistribute(struct eigrp *eigrp)
{
	struct redistribute	*r;

	if (eigrp->dflt_metric) {
		printf("\t\tdefault-metric");
		print_redist_metric(eigrp->dflt_metric);
		printf("\n");
	}

	SIMPLEQ_FOREACH(r, &eigrp->redist_list, entry) {
		switch (r->type & ~REDIST_NO) {
		case REDIST_STATIC:
			printf("\t\t%sredistribute static", print_no(r->type));
			break;
		case REDIST_RIP:
			printf("\t\t%sredistribute rip", print_no(r->type));
			break;
		case REDIST_OSPF:
			printf("\t\t%sredistribute ospf", print_no(r->type));
			break;
		case REDIST_CONNECTED:
			printf("\t\t%sredistribute connected",
			    print_no(r->type));
			break;
		case REDIST_DEFAULT:
			printf("\t\t%sredistribute default", print_no(r->type));
			break;
		case REDIST_ADDR:
			printf("\t\t%sredistribute %s/%u",
			    print_no(r->type), log_addr(r->af, &r->addr),
			    r->prefixlen);
			break;
		}

		if (r->metric)
			print_redist_metric(r->metric);
		printf("\n");
	}
}

void
print_iface(struct eigrp_iface *ei)
{
	printf("\t\tinterface %s {\n", ei->iface->name);
	printf("\t\t\thello-interval %u\n", ei->hello_interval);
	printf("\t\t\tholdtime %u\n", ei->hello_holdtime);
	printf("\t\t\tdelay %u\n", ei->delay);
	printf("\t\t\tbandwidth %u\n", ei->bandwidth);
	printf("\t\t\tsplit-horizon %s\n", (ei->splithorizon) ? "yes" : "no");
	if (ei->passive)
		printf("\t\t\tpassive\n");
	printf("\t\t}\n");
}

void
print_as(struct eigrp *eigrp)
{
	struct eigrp_iface	*ei;

	printf("\tautonomous-system %u {\n", eigrp->as);
	printf("\t\tk-values %u %u %u %u %u %u\n", eigrp->kvalues[0],
	    eigrp->kvalues[1], eigrp->kvalues[2], eigrp->kvalues[3],
	    eigrp->kvalues[4], eigrp->kvalues[5]);
	printf("\t\tactive-timeout %u\n", eigrp->active_timeout);
	printf("\t\tmaximum-hops %u\n", eigrp->maximum_hops);
	printf("\t\tmaximum-paths %u\n", eigrp->maximum_paths);
	printf("\t\tvariance %u\n", eigrp->variance);
	print_redistribute(eigrp);
	printf("\n");
	TAILQ_FOREACH(ei, &eigrp->ei_list, e_entry)
		print_iface(ei);
	printf("\t}\n");
}

void
print_af(struct eigrpd_conf *conf, int af)
{
	struct eigrp	*eigrp;

	printf("address-family %s {\n", af_name(af));
	TAILQ_FOREACH(eigrp, &conf->instances, entry)
		if (eigrp->af == af)
			print_as(eigrp);
	printf("}\n\n");
}

void
print_config(struct eigrpd_conf *conf)
{
	printf("\n");
	print_mainconf(conf);
	printf("\n");

	print_af(conf, AF_INET);
	print_af(conf, AF_INET6);
}
