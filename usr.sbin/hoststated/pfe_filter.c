/*	$OpenBSD: pfe_filter.c,v 1.3 2007/01/03 09:42:30 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <limits.h>
#include <fcntl.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "hostated.h"

struct pfdata {
	int			 dev;
	struct pf_anchor	*anchor;
	struct pfioc_trans	 pft;
	struct pfioc_trans_e	 pfte;
};

int	 transaction_init(struct hostated *, const char *);
int	 transaction_commit(struct hostated *);
void	 kill_tables(struct hostated *);

void
init_filter(struct hostated *env)
{
	struct pf_status	status;

	if ((env->pf = calloc(1, sizeof(*(env->pf)))) == NULL)
		fatal("calloc");
	if ((env->pf->dev = open(PF_SOCKET, O_RDWR)) == -1)
		fatal("init_filter: cannot open pf socket");
	if (ioctl(env->pf->dev, DIOCGETSTATUS, &status) == -1)
		fatal("init_filter: DIOCGETSTATUS");
	if (!status.running)
		fatalx("init_filter: pf is disabled");
	log_debug("init_filter: filter init done");
}

void
init_tables(struct hostated *env)
{
	int			 i;
	struct service		*service;
	struct pfr_table	*tables;
	struct pfioc_table	 io;

	if ((tables = calloc(env->servicecount, sizeof(*tables))) == NULL)
		fatal("calloc");
	i = 0;

	TAILQ_FOREACH(service, &env->services, entry) {
		(void)strlcpy(tables[i].pfrt_anchor, HOSTATED_ANCHOR "/",
		    sizeof(tables[i].pfrt_anchor));
		(void)strlcat(tables[i].pfrt_anchor, service->name,
		    sizeof(tables[i].pfrt_anchor));
		(void)strlcpy(tables[i].pfrt_name, service->name,
		    sizeof(tables[i].pfrt_name));
		tables[i].pfrt_flags |= PFR_TFLAG_PERSIST;
		i++;
	}
	if (i != env->servicecount)
		fatalx("init_tables: table count modified");

	memset(&io, 0, sizeof(io));
	io.pfrio_size = env->servicecount;
	io.pfrio_esize = sizeof(*tables);
	io.pfrio_buffer = tables;

	if (ioctl(env->pf->dev, DIOCRADDTABLES, &io) == -1)
		fatal("init_tables: cannot create tables");
	log_debug("created %d tables", io.pfrio_nadd);

	if (io.pfrio_nadd == env->servicecount)
		return;

	/*
	 * clear all tables, since some already existed
	 */
	TAILQ_FOREACH(service, &env->services, entry)
		flush_table(env, service);
}

void
kill_tables(struct hostated *env) {
	struct pfioc_table	 io;
	struct service		*service;

	memset(&io, 0, sizeof(io));
	TAILQ_FOREACH(service, &env->services, entry) {
		(void)strlcpy(io.pfrio_table.pfrt_anchor, HOSTATED_ANCHOR "/",
		    sizeof(io.pfrio_table.pfrt_anchor));
		(void)strlcat(io.pfrio_table.pfrt_anchor, service->name,
		    sizeof(io.pfrio_table.pfrt_anchor));
		if (ioctl(env->pf->dev, DIOCRCLRTABLES, &io) == -1)
			fatal("kill_tables: ioctl faile: ioctl failed");
	}
	log_debug("kill_tables: deleted %d tables", io.pfrio_ndel);
}

void
sync_table(struct hostated *env, struct service *service, struct table *table)
{
	int			 i;
	struct pfioc_table	 io;
	struct pfr_addr		*addlist;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sain6;
	struct host		*host;

	if (table == NULL)
		return;

	if (table->up == 0) {
		flush_table(env, service);
		return;
	}

	if ((addlist = calloc(table->up, sizeof(*addlist))) == NULL)
		fatal("calloc");

	memset(&io, 0, sizeof(io));
	io.pfrio_esize = sizeof(struct pfr_addr);
	io.pfrio_size = table->up;
	io.pfrio_size2 = 0;
	io.pfrio_buffer = addlist;
	(void)strlcpy(io.pfrio_table.pfrt_anchor, HOSTATED_ANCHOR "/",
	    sizeof(io.pfrio_table.pfrt_anchor));
	(void)strlcat(io.pfrio_table.pfrt_anchor, service->name,
	    sizeof(io.pfrio_table.pfrt_anchor));
	(void)strlcpy(io.pfrio_table.pfrt_name, service->name,
	    sizeof(io.pfrio_table.pfrt_name));

	i = 0;
	TAILQ_FOREACH(host, &table->hosts, entry) {
		if (host->up != 1)
			continue;
		memset(&(addlist[i]), 0, sizeof(addlist[i]));
		switch (host->ss.ss_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&host->ss;
			addlist[i].pfra_af = AF_INET;
			memcpy(&(addlist[i].pfra_ip4addr), &sain->sin_addr,
			    sizeof(sain->sin_addr));
			addlist[i].pfra_net = 32;
			break;
		case AF_INET6:
			sain6 = (struct sockaddr_in6 *)&host->ss;
			addlist[i].pfra_af = AF_INET6;
			memcpy(&(addlist[i].pfra_ip6addr), &sain6->sin6_addr,
			    sizeof(sain6->sin6_addr));
			addlist[i].pfra_net = 128;
			break;
		default:
			fatalx("sync_table: unknown address family");
			break;
		}
		i++;
	}
	if (i != table->up)
		fatalx("sync_table: desynchronized");

	if (ioctl(env->pf->dev, DIOCRSETADDRS, &io) == -1)
		fatal("sync_table: cannot set address list");

	log_debug("sync_table: table %s: %d added, %d deleted, %d changed",
	    io.pfrio_table.pfrt_name,
	    io.pfrio_nadd, io.pfrio_ndel, io.pfrio_nchange);
}

void
flush_table(struct hostated *env, struct service *service)
{
	struct pfioc_table	io;

	memset(&io, 0, sizeof(io));
	(void)strlcpy(io.pfrio_table.pfrt_anchor, HOSTATED_ANCHOR "/",
	    sizeof(io.pfrio_table.pfrt_anchor));
	(void)strlcat(io.pfrio_table.pfrt_anchor, service->name,
	    sizeof(io.pfrio_table.pfrt_anchor));
	(void)strlcpy(io.pfrio_table.pfrt_name, service->name,
	    sizeof(io.pfrio_table.pfrt_name));
	if (ioctl(env->pf->dev, DIOCRCLRADDRS, &io) == -1)
		fatal("flush_table: cannot flush table");
	log_debug("flush_table: flushed table %s", service->name);
	return;
}

int
transaction_init(struct hostated *env, const char *anchor)
{
	env->pf->pft.size = 1;
	env->pf->pft.esize = sizeof env->pf->pfte;
	env->pf->pft.array = &env->pf->pfte;

	memset(&env->pf->pfte, 0, sizeof env->pf->pfte);
	strlcpy(env->pf->pfte.anchor, anchor, PF_ANCHOR_NAME_SIZE);
	env->pf->pfte.rs_num = PF_RULESET_RDR;

	if (ioctl(env->pf->dev, DIOCXBEGIN, &env->pf->pft) == -1)
		return (-1);
	return (0);
}

int
transaction_commit(struct hostated *env)
{
	if (ioctl(env->pf->dev, DIOCXCOMMIT, &env->pf->pft) == -1)
		return (-1);
	return (0);
}

void
sync_ruleset(struct hostated *env, struct service *service, int enable)
{
	struct pfioc_rule	 rio;
	struct pfioc_pooladdr	 pio;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sain6;
	struct address		*address;
	char			 anchor[PF_ANCHOR_NAME_SIZE];

	bzero(anchor, sizeof(anchor));
	(void)strlcpy(anchor, HOSTATED_ANCHOR "/", sizeof(anchor));
	(void)strlcat(anchor, service->name, sizeof(anchor));
	transaction_init(env, anchor);

	if (!enable) {
		transaction_commit(env);
		log_debug("sync_ruleset: rules removed");
		return;
	}

	TAILQ_FOREACH(address, &service->virts, entry) {
		memset(&rio, 0, sizeof(rio));
		memset(&pio, 0, sizeof(pio));
		(void)strlcpy(rio.anchor, anchor, sizeof(rio.anchor));

		rio.ticket = env->pf->pfte.ticket;
		if (ioctl(env->pf->dev, DIOCBEGINADDRS, &pio) == -1)
			fatal("sync_ruleset: cannot initialise address pool");

		rio.pool_ticket = pio.ticket;
		rio.rule.af = address->ss.ss_family;
		rio.rule.proto = IPPROTO_TCP;
		rio.rule.src.addr.type = PF_ADDR_ADDRMASK;
		rio.rule.dst.addr.type = PF_ADDR_ADDRMASK;
		rio.rule.dst.port_op = PF_OP_EQ;
		rio.rule.dst.port[0] = address->port;
		rio.rule.rtableid = -1; /* stay in the main routing table */
		rio.rule.action = PF_RDR;
		if (strlen(service->tag))
			(void)strlcpy(rio.rule.tagname, service->tag,
			    sizeof(rio.rule.tagname));
		if (strlen(address->ifname))
			(void)strlcpy(rio.rule.ifname, address->ifname,
			    sizeof(rio.rule.ifname));

		if (address->ss.ss_family == AF_INET) {
			sain = (struct sockaddr_in *)&address->ss;

			rio.rule.dst.addr.v.a.addr.addr32[0] =
			    sain->sin_addr.s_addr;
			rio.rule.dst.addr.v.a.mask.addr32[0] = 0xffffffff;

		} else {
			sain6 = (struct sockaddr_in6 *)&address->ss;

			memcpy(&rio.rule.dst.addr.v.a.addr.v6,
			    &sain6->sin6_addr.s6_addr,
			    sizeof(sain6->sin6_addr.s6_addr));
			memset(&rio.rule.dst.addr.v.a.mask.addr8, 0xff, 16);
		}

		pio.addr.addr.type = PF_ADDR_TABLE;
		(void)strlcpy(pio.addr.addr.v.tblname, service->name,
		    sizeof(pio.addr.addr.v.tblname));
		if (ioctl(env->pf->dev, DIOCADDADDR, &pio) == -1)
			fatal("sync_ruleset: cannot add address to pool");

		rio.rule.rpool.proxy_port[0] = service->table->port;
		rio.rule.rpool.port_op = PF_OP_EQ;
		rio.rule.rpool.opts = PF_POOL_ROUNDROBIN;
		if (service->flags & F_STICKY)
			rio.rule.rpool.opts |= PF_POOL_STICKYADDR;

		if (ioctl(env->pf->dev, DIOCADDRULE, &rio) == -1)
			fatal("cannot add rule");
		log_debug("sync_ruleset: rule added");
	}
	transaction_commit(env);
}

void
flush_rulesets(struct hostated *env)
{
	struct service	*service;
	char		 anchor[PF_ANCHOR_NAME_SIZE];

	kill_tables(env);
	TAILQ_FOREACH(service, &env->services, entry) {
		strlcpy(anchor, HOSTATED_ANCHOR "/", sizeof(anchor));
		strlcat(anchor, service->name, sizeof(anchor));
		transaction_init(env, anchor);
		transaction_commit(env);
	}
	strlcpy(anchor, HOSTATED_ANCHOR, sizeof(anchor));
	transaction_init(env, anchor);
	transaction_commit(env);
	log_debug("flush_rulesets: flushed rules");
}
