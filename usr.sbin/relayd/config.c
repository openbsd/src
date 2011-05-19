/*	$OpenBSD: config.c,v 1.1 2011/05/19 08:56:49 reyk Exp $	*/

/*
 * Copyright (c) 2011 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <net/route.h>

#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <ifaddrs.h>

#include <openssl/ssl.h>

#include "relayd.h"

int
config_init(struct relayd *env)
{
	struct privsep	*ps = env->sc_ps;
	u_int		 what;

	/* Global configuration */
	if (privsep_process == PROC_PARENT) {
		env->sc_timeout.tv_sec = CHECK_TIMEOUT / 1000;
		env->sc_timeout.tv_usec = (CHECK_TIMEOUT % 1000) * 1000;
		env->sc_interval.tv_sec = CHECK_INTERVAL;
		env->sc_interval.tv_usec = 0;
		env->sc_prefork_relay = RELAY_NUMPROC;
		env->sc_statinterval.tv_sec = RELAY_STATINTERVAL;

		ps->ps_what[PROC_PARENT] = CONFIG_ALL;
		ps->ps_what[PROC_PFE] = CONFIG_ALL & ~CONFIG_PROTOS;
		ps->ps_what[PROC_HCE] = CONFIG_TABLES;
		ps->ps_what[PROC_RELAY] =
		    CONFIG_TABLES|CONFIG_RELAYS|CONFIG_PROTOS;
	}

	/* Other configuration */
	what = ps->ps_what[privsep_process];
	if (what & CONFIG_TABLES) {
		if ((env->sc_tables =
		    calloc(1, sizeof(*env->sc_tables))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_tables);

		memset(&env->sc_empty_table, 0, sizeof(env->sc_empty_table));
		env->sc_empty_table.conf.id = EMPTY_TABLE;
		env->sc_empty_table.conf.flags |= F_DISABLE;
		(void)strlcpy(env->sc_empty_table.conf.name, "empty",
		    sizeof(env->sc_empty_table.conf.name));

	}
	if (what & CONFIG_RDRS) {
		if ((env->sc_rdrs =
		    calloc(1, sizeof(*env->sc_rdrs))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_rdrs);

	}
	if (what & CONFIG_RELAYS) {
		if ((env->sc_relays =
		    calloc(1, sizeof(*env->sc_relays))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_relays);
	}
	if (what & CONFIG_PROTOS) {
		if ((env->sc_protos =
		    calloc(1, sizeof(*env->sc_protos))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_protos);

		bzero(&env->sc_proto_default, sizeof(env->sc_proto_default));
		env->sc_proto_default.id = EMPTY_ID;
		env->sc_proto_default.flags = F_USED;
		env->sc_proto_default.cache = RELAY_CACHESIZE;
		env->sc_proto_default.tcpflags = TCPFLAG_DEFAULT;
		env->sc_proto_default.tcpbacklog = RELAY_BACKLOG;
		env->sc_proto_default.sslflags = SSLFLAG_DEFAULT;
		(void)strlcpy(env->sc_proto_default.sslciphers,
		    SSLCIPHERS_DEFAULT,
		    sizeof(env->sc_proto_default.sslciphers));
		env->sc_proto_default.type = RELAY_PROTO_TCP;
		(void)strlcpy(env->sc_proto_default.name, "default",
		    sizeof(env->sc_proto_default.name));
		RB_INIT(&env->sc_proto_default.request_tree);
		RB_INIT(&env->sc_proto_default.response_tree);
	}
	if (what & CONFIG_RTS) {
		if ((env->sc_rts =
		    calloc(1, sizeof(*env->sc_rts))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_rts);
	}
	if (what & CONFIG_ROUTES) {
		if ((env->sc_routes =
		    calloc(1, sizeof(*env->sc_routes))) == NULL)
			return (-1);
		TAILQ_INIT(env->sc_routes);
	}

	return (0);
}

void
config_purge(struct relayd *env, u_int reset)
{
	struct privsep		*ps = env->sc_ps;
	struct table		*table;
	struct rdr		*rdr;
	struct address		*virt;
	struct protocol		*proto;
	struct relay		*rlay;
	struct netroute		*nr;
	struct router		*rt;
	u_int			 what;

	what = ps->ps_what[privsep_process] & reset;

	if (what & CONFIG_TABLES && env->sc_tables != NULL) {
		while ((table = TAILQ_FIRST(env->sc_tables)) != NULL)
			purge_table(env->sc_tables, table);
		env->sc_tablecount = 0;
	}
	if (what & CONFIG_RDRS && env->sc_rdrs != NULL) {
		while ((rdr = TAILQ_FIRST(env->sc_rdrs)) != NULL) {
			TAILQ_REMOVE(env->sc_rdrs, rdr, entry);
			while ((virt = TAILQ_FIRST(&rdr->virts)) != NULL) {
				TAILQ_REMOVE(&rdr->virts, virt, entry);
				free(virt);
			}
			free(rdr);
		}
		env->sc_rdrcount = 0;
	}
	if (what & CONFIG_RELAYS && env->sc_relays != NULL) {
		while ((rlay = TAILQ_FIRST(env->sc_relays)) != NULL)
			purge_relay(env, rlay);
		env->sc_relaycount = 0;
	}
	if (what & CONFIG_PROTOS && env->sc_protos != NULL) {
		while ((proto = TAILQ_FIRST(env->sc_protos)) != NULL) {
			TAILQ_REMOVE(env->sc_protos, proto, entry);
			purge_tree(&proto->request_tree);
			purge_tree(&proto->response_tree);
			if (proto->style != NULL)
				free(proto->style);
			free(proto);
		}
		env->sc_protocount = 0;
	}
	if (what & CONFIG_RTS && env->sc_rts != NULL) {
		while ((rt = TAILQ_FIRST(env->sc_rts)) != NULL) {
			TAILQ_REMOVE(env->sc_rts, rt, rt_entry);
			while ((nr = TAILQ_FIRST(&rt->rt_netroutes)) != NULL) {
				TAILQ_REMOVE(&rt->rt_netroutes, nr, nr_entry);
				TAILQ_REMOVE(env->sc_routes, nr, nr_route);
				free(nr);
				env->sc_routecount--;
			}
			free(rt);
		}
		env->sc_routercount = 0;
	}
	if (what & CONFIG_ROUTES && env->sc_routes != NULL) {
		while ((nr = TAILQ_FIRST(env->sc_routes)) != NULL) {
			if ((rt = nr->nr_router) != NULL)
				TAILQ_REMOVE(&rt->rt_netroutes, nr, nr_entry);
			TAILQ_REMOVE(env->sc_routes, nr, nr_route);
			free(nr);
		}
		env->sc_routecount = 0;
	}
}

int
config_setreset(struct relayd *env, u_int reset)
{
	struct privsep	*ps = env->sc_ps;
	int		 id;

	for (id = 0; id < PROC_MAX; id++) {
		if ((reset & ps->ps_what[id]) == 0 ||
		    id == privsep_process)
			continue;
		proc_compose_imsg(ps, id, -1, IMSG_CTL_RESET, -1,
		    &reset, sizeof(reset));
	}

	return (0);
}

int
config_getreset(struct relayd *env, struct imsg *imsg)
{
	u_int		 mode;

	IMSG_SIZE_CHECK(imsg, &mode);
	memcpy(&mode, imsg->data, sizeof(mode));

	config_purge(env, mode);

	return (0);
}

int
config_getcfg(struct relayd *env, struct imsg *imsg)
{
	struct privsep		*ps = env->sc_ps;
	struct table		*tb;
	struct host		*h, *ph;
	struct ctl_flags	 cf;

	if (IMSG_DATA_SIZE(imsg) != sizeof(cf))
		return (0); /* ignore */

	/* Update runtime flags */
	memcpy(&cf, imsg->data, sizeof(cf));
	env->sc_opts = cf.cf_opts;
	env->sc_flags = cf.cf_flags;

	if (ps->ps_what[privsep_process] & CONFIG_TABLES) {
		/* Update the tables */
		TAILQ_FOREACH(tb, env->sc_tables, entry) {
			TAILQ_FOREACH(h, &tb->hosts, entry) {
				if (h->conf.parentid && (ph = host_find(env,
				    h->conf.parentid)) != NULL) {
					SLIST_INSERT_HEAD(&ph->children,
					    h, child);
				}
			}
		}
	}

	if (env->sc_flags & (F_SSL|F_SSLCLIENT))
		ssl_init(env);

	if (privsep_process != PROC_PARENT)
		proc_compose_imsg(env->sc_ps, PROC_PARENT, -1,
		    IMSG_CFG_DONE, -1, NULL, 0);

	return (0);
}

int
config_settable(struct relayd *env, struct table *tb)
{
	struct privsep	*ps = env->sc_ps;
	struct host	*host;
	int		 id, c;
	struct iovec	 iov[2];

	for (id = 0; id < PROC_MAX; id++) {
		if ((ps->ps_what[id] & CONFIG_TABLES) == 0 ||
		    id == privsep_process)
			continue;

		/* XXX need to send table to pfe for control socket */
		if (id == PROC_HCE && tb->conf.check == CHECK_NOCHECK)
			continue;

		DPRINTF("%s: sending table %s %d to %s", __func__,
		    tb->conf.name, tb->conf.id, env->sc_ps->ps_title[id]);

		c = 0;
		iov[c].iov_base = &tb->conf;
		iov[c++].iov_len = sizeof(tb->conf);
		if (tb->sendbuf != NULL) {
			iov[c].iov_base = tb->sendbuf;
			iov[c++].iov_len = strlen(tb->sendbuf);
		}

		proc_composev_imsg(ps, id, -1, IMSG_CFG_TABLE, -1, iov, c);

		TAILQ_FOREACH(host, &tb->hosts, entry) {
			proc_compose_imsg(ps, id, -1, IMSG_CFG_HOST, -1,
			    &host->conf, sizeof(host->conf));
		}
	}

	return (0);
}

int
config_gettable(struct relayd *env, struct imsg *imsg)
{
	struct table		*tb;
	size_t			 sb;
	u_int8_t		*p = imsg->data;
	size_t			 s;

	if ((tb = calloc(1, sizeof(*tb))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &tb->conf);
	memcpy(&tb->conf, p, sizeof(tb->conf));
	s = sizeof(tb->conf);

	sb = IMSG_DATA_SIZE(imsg) - s;
	if (sb > 0)
		tb->sendbuf = get_string(p + s, sb);

	TAILQ_INIT(&tb->hosts);
	TAILQ_INSERT_TAIL(env->sc_tables, tb, entry);

	env->sc_tablecount++;

	DPRINTF("%s: %s %d received table %d (%s)", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    tb->conf.id, tb->conf.name);

	return (0);
}

int
config_gethost(struct relayd *env, struct imsg *imsg)
{
	struct table		*tb;
	struct host		*host;

	if ((host = calloc(1, sizeof(*host))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &host->conf);
	memcpy(&host->conf, imsg->data, sizeof(host->conf));

	if (host_find(env, host->conf.id) != NULL) {
		log_debug("%s: host %d already exists",
		    __func__, host->conf.id);
		free(host);
		return (-1);
	}

	if ((tb = table_find(env, host->conf.tableid)) == NULL) {
		log_debug("%s: "
		    "received host for unknown table %d", __func__,
		    host->conf.tableid);
		free(host);
		return (-1);
	}

	host->tablename = tb->conf.name;
	host->cte.s = -1;

	SLIST_INIT(&host->children);
	TAILQ_INSERT_TAIL(&tb->hosts, host, entry);

	DPRINTF("%s: %s %d received host %s for table %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    host->conf.name, tb->conf.name);

	return (0);
}

int
config_setrdr(struct relayd *env, struct rdr *rdr)
{
	struct privsep	*ps = env->sc_ps;
	struct address	*virt;
	int		 id;

	for (id = 0; id < PROC_MAX; id++) {
		if ((ps->ps_what[id] & CONFIG_RDRS) == 0 ||
		    id == privsep_process)
			continue;

		DPRINTF("%s: sending rdr %s to %s", __func__,
		    rdr->conf.name, ps->ps_title[id]);

		proc_compose_imsg(ps, id, -1, IMSG_CFG_RDR, -1,
		    &rdr->conf, sizeof(rdr->conf));

		TAILQ_FOREACH(virt, &rdr->virts, entry) {
			virt->rdrid = rdr->conf.id;
			proc_compose_imsg(ps, id, -1, IMSG_CFG_VIRT, -1,
			    virt, sizeof(*virt));
		}
	}

	return (0);
}

int
config_getrdr(struct relayd *env, struct imsg *imsg)
{
	struct rdr		*rdr;

	if ((rdr = calloc(1, sizeof(*rdr))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &rdr->conf);
	memcpy(&rdr->conf, imsg->data, sizeof(rdr->conf));

	if ((rdr->table = table_find(env, rdr->conf.table_id)) == NULL) {
		log_debug("%s: table not found", __func__);
		free(rdr);
		return (-1);
	}
	if ((rdr->backup = table_find(env, rdr->conf.backup_id)) == NULL) {
		rdr->conf.backup_id = EMPTY_TABLE;
		rdr->backup = &env->sc_empty_table;
	}

	TAILQ_INIT(&rdr->virts);
	TAILQ_INSERT_TAIL(env->sc_rdrs, rdr, entry);

	env->sc_rdrcount++;

	DPRINTF("%s: %s %d received rdr %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    rdr->conf.name);

	return (0);
}

int
config_getvirt(struct relayd *env, struct imsg *imsg)
{
	struct rdr	*rdr;
	struct address	*virt;

	IMSG_SIZE_CHECK(imsg, virt);

	if ((virt = calloc(1, sizeof(*virt))) == NULL)
		return (-1);
	memcpy(virt, imsg->data, sizeof(*virt));

	if ((rdr = rdr_find(env, virt->rdrid)) == NULL) {
		log_debug("%s: rdr not found", __func__);
		free(virt);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&rdr->virts, virt, entry);

	DPRINTF("%s: %s %d received address for rdr %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    rdr->conf.name);

	return (0);
}

int
config_setrt(struct relayd *env, struct router *rt)
{
	struct privsep	*ps = env->sc_ps;
	struct netroute	*nr;
	int		 id;

	for (id = 0; id < PROC_MAX; id++) {
		if ((ps->ps_what[id] & CONFIG_RTS) == 0 ||
		    id == privsep_process)
			continue;

		DPRINTF("%s: sending router %s to %s tbl %d", __func__,
		    rt->rt_conf.name, ps->ps_title[id], rt->rt_conf.gwtable);

		proc_compose_imsg(ps, id, -1, IMSG_CFG_ROUTER, -1,
		    &rt->rt_conf, sizeof(rt->rt_conf));

		TAILQ_FOREACH(nr, &rt->rt_netroutes, nr_entry) {
			proc_compose_imsg(ps, id, -1, IMSG_CFG_ROUTE, -1,
			    &nr->nr_conf, sizeof(nr->nr_conf));
		}
	}

	return (0);
}

int
config_getrt(struct relayd *env, struct imsg *imsg)
{
	struct router		*rt;

	if ((rt = calloc(1, sizeof(*rt))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &rt->rt_conf);
	memcpy(&rt->rt_conf, imsg->data, sizeof(rt->rt_conf));

	if ((rt->rt_gwtable = table_find(env, rt->rt_conf.gwtable)) == NULL) {
		log_debug("%s: table not found", __func__);
		free(rt);
		return (-1);
	}

	TAILQ_INIT(&rt->rt_netroutes);
	TAILQ_INSERT_TAIL(env->sc_rts, rt, rt_entry);

	env->sc_routercount++;

	DPRINTF("%s: %s %d received router %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    rt->rt_conf.name);

	return (0);
}

int
config_getroute(struct relayd *env, struct imsg *imsg)
{
	struct router		*rt;
	struct netroute		*nr;

	if ((nr = calloc(1, sizeof(*nr))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &nr->nr_conf);
	memcpy(&nr->nr_conf, imsg->data, sizeof(nr->nr_conf));

	if (route_find(env, nr->nr_conf.id) != NULL) {
		log_debug("%s: route %d already exists",
		    __func__, nr->nr_conf.id);
		free(nr);
		return (-1);
	}

	if ((rt = router_find(env, nr->nr_conf.routerid)) == NULL) {
		log_debug("%s: received route for unknown router", __func__);
		free(nr);
		return (-1);
	}

	nr->nr_router = rt;

	TAILQ_INSERT_TAIL(env->sc_routes, nr, nr_route);
	TAILQ_INSERT_TAIL(&rt->rt_netroutes, nr, nr_entry);

	env->sc_routecount++;

	DPRINTF("%s: %s %d received route %d for router %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    nr->nr_conf.id, rt->rt_conf.name);

	return (0);
}

int
config_setproto(struct relayd *env, struct protocol *proto)
{
	struct privsep		*ps = env->sc_ps;
	int			 id;
	struct iovec		 iov[2];
	size_t			 c;

	for (id = 0; id < PROC_MAX; id++) {
		if ((ps->ps_what[id] & CONFIG_PROTOS) == 0 ||
		    id == privsep_process)
			continue;

		DPRINTF("%s: sending protocol %s to %s", __func__,
		    proto->name, ps->ps_title[id]);

		c = 0;
		iov[c].iov_base = proto;
		iov[c++].iov_len = sizeof(*proto);

		if (proto->style != NULL) {
			iov[c].iov_base = proto->style;
			iov[c++].iov_len = strlen(proto->style);
		}

		/* XXX struct protocol should be split */
		proc_composev_imsg(ps, id, -1, IMSG_CFG_PROTO, -1, iov, c);

		/* Now send all the protocol key/value nodes */
		if (config_setprotonode(env, id, proto,
		    RELAY_DIR_REQUEST) == -1 ||
		    config_setprotonode(env, id, proto,
		    RELAY_DIR_RESPONSE) == -1)
			return (-1);
	}

	return (0);
}

int
config_getproto(struct relayd *env, struct imsg *imsg)
{
	struct protocol		*proto;
	size_t			 styl;

	if ((proto = calloc(1, sizeof(*proto))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, proto);
	memcpy(proto, imsg->data, sizeof(*proto));

	styl = IMSG_DATA_SIZE(imsg) - sizeof(*proto);
	if (styl > 0)
		proto->style = get_string((char *)imsg->data +
		    sizeof(*proto), styl);

	proto->request_nodes = 0;
	proto->response_nodes = 0;
	RB_INIT(&proto->request_tree);
	RB_INIT(&proto->response_tree);

	TAILQ_INSERT_TAIL(env->sc_protos, proto, entry);

	env->sc_protocount++;

	DPRINTF("%s: %s %d received protocol %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    proto->name);

	return (0);
}

int
config_setprotonode(struct relayd *env, enum privsep_procid id,
    struct protocol *proto, enum direction dir)
{
	struct privsep		*ps = env->sc_ps;
	struct iovec		 iov[IOV_MAX];
	size_t			 c, sz;
	struct protonode	*proot, *pn;
	struct proto_tree	*tree;

	if (dir == RELAY_DIR_RESPONSE)
		tree = &proto->response_tree;
	else
		tree = &proto->request_tree;

	sz = c = 0;
	RB_FOREACH(proot, proto_tree, tree) {
		PROTONODE_FOREACH(pn, proot, entry) {
			pn->conf.protoid = proto->id;
			pn->conf.dir = dir;
			pn->conf.keylen = pn->key ? strlen(pn->key) : 0;
			pn->conf.valuelen = pn->value ? strlen(pn->value) : 0;
			pn->conf.len = sizeof(*pn) +
			    pn->conf.keylen + pn->conf.valuelen;

			if (pn->conf.len > (MAX_IMSGSIZE - IMSG_HEADER_SIZE))
				return (-1);

			if (c && ((c + 3) >= IOV_MAX || (sz + pn->conf.len) >
			    (MAX_IMSGSIZE - IMSG_HEADER_SIZE))) {
				proc_composev_imsg(ps, id, -1,
				    IMSG_CFG_PROTONODE, -1, iov, c);
				c = sz = 0;
			}

			iov[c].iov_base = pn;
			iov[c++].iov_len = sizeof(*pn);
			if (pn->conf.keylen) {
				iov[c].iov_base = pn->key;
				iov[c++].iov_len = pn->conf.keylen;
			}
			if (pn->conf.valuelen) {
				iov[c].iov_base = pn->value;
				iov[c++].iov_len = pn->conf.valuelen;
			}
			sz += pn->conf.len;
		}
	}

	if (c && sz)
		proc_composev_imsg(ps, id, -1, IMSG_CFG_PROTONODE, -1, iov, c);

	return (0);
}

int
config_getprotonode(struct relayd *env, struct imsg *imsg)
{
	struct protocol		*proto = NULL;
	struct protonode	 pn;
	size_t			 z, s, c = 0;
	u_int8_t		*p = imsg->data;

	bzero(&pn, sizeof(pn));

	IMSG_SIZE_CHECK(imsg, &pn);
	for (z = 0; z < (IMSG_DATA_SIZE(imsg) - sizeof(pn)); z += pn.conf.len) {
		s = z;
		memcpy(&pn, p + s, sizeof(pn));
		s += sizeof(pn);

		if ((proto = proto_find(env, pn.conf.protoid)) == NULL) {
			log_debug("%s: unknown protocol %d", __func__,
			    pn.conf.protoid);
			return (-1);
		}

		pn.key = pn.value = NULL;
		bzero(&pn.entry, sizeof(pn.entry));
		bzero(&pn.nodes, sizeof(pn.nodes));
		bzero(&pn.head, sizeof(pn.head));

		if (pn.conf.keylen) {
			pn.key = get_string(p + s, pn.conf.keylen);
			s += pn.conf.keylen;
		}
		if (pn.conf.valuelen) {
			pn.value = get_string(p + s, pn.conf.valuelen);
			s += pn.conf.valuelen;
		}

		if (protonode_add(pn.conf.dir, proto, &pn) == -1) {
			log_debug("%s: failed to add protocol node", __func__);
			return (-1);
		}
		c++;
	}

	if (!c)
		return (0);

	DPRINTF("%s: %s %d received %d nodes for protocol %s", __func__,
	    env->sc_ps->ps_title[privsep_process], env->sc_ps->ps_instance,
	    c, proto->name);

	return (0);
}

int
config_setrelay(struct relayd *env, struct relay *rlay)
{
	struct privsep	*ps = env->sc_ps;
	int		 id;
	int		 fd, n, m;
	struct iovec	 iov[4];
	size_t		 c;

	/* opens listening sockets etc. */
	if (relay_privinit(rlay) == -1)
		return (-1);

	for (id = 0; id < PROC_MAX; id++) {
		if ((ps->ps_what[id] & CONFIG_RELAYS) == 0 ||
		    id == privsep_process)
			continue;

		DPRINTF("%s: sending relay %s to %s fd %d", __func__,
		    rlay->rl_conf.name, ps->ps_title[id], rlay->rl_s);

		c = 0;
		iov[c].iov_base = &rlay->rl_conf;
		iov[c++].iov_len = sizeof(rlay->rl_conf);
		if (rlay->rl_conf.ssl_cert_len) {
			iov[c].iov_base = rlay->rl_ssl_cert;
			iov[c++].iov_len = rlay->rl_conf.ssl_cert_len;
		}
		if (rlay->rl_conf.ssl_key_len) {
			iov[c].iov_base = rlay->rl_ssl_key;
			iov[c++].iov_len = rlay->rl_conf.ssl_key_len;
		}
		if (rlay->rl_conf.ssl_ca_len) {
			iov[c].iov_base = rlay->rl_ssl_ca;
			iov[c++].iov_len = rlay->rl_conf.ssl_ca_len;
		}

		if (id == PROC_RELAY) {
			/* XXX imsg code will close the fd after 1st call */
			n = -1;
			proc_range(ps, id, &n, &m);
			for (n = 0; n < m; n++) {
				if ((fd = dup(rlay->rl_s)) == -1)
					return (-1);
				proc_composev_imsg(ps, id, n,
				    IMSG_CFG_RELAY, fd, iov, c);
			}
		} else {
			proc_composev_imsg(ps, id, -1, IMSG_CFG_RELAY, -1,
			    iov, c);
		}
	}

	close(rlay->rl_s);
	rlay->rl_s = -1;

	return (0);
}

int
config_getrelay(struct relayd *env, struct imsg *imsg)
{
	struct privsep		*ps = env->sc_ps;
	struct relay		*rlay;
	u_int8_t		*p = imsg->data;
	size_t			 s;

	if ((rlay = calloc(1, sizeof(*rlay))) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &rlay->rl_conf);
	memcpy(&rlay->rl_conf, p, sizeof(rlay->rl_conf));
	s = sizeof(rlay->rl_conf);

	rlay->rl_s = imsg->fd;

	if (ps->ps_what[privsep_process] & CONFIG_PROTOS) {
		if (rlay->rl_conf.proto == EMPTY_ID)
			rlay->rl_proto = &env->sc_proto_default;
		else if ((rlay->rl_proto =
		    proto_find(env, rlay->rl_conf.proto)) == NULL) {
			log_debug("%s: unknown protocol", __func__);
			goto fail;
		}
	}

	if (rlay->rl_conf.dsttable != EMPTY_ID &&
	    (rlay->rl_dsttable = table_find(env,
	    rlay->rl_conf.dsttable)) == NULL) {
		log_debug("%s: unknown table", __func__);
		goto fail;
	}

	rlay->rl_backuptable = &env->sc_empty_table;
	if (rlay->rl_conf.backuptable != EMPTY_ID &&
	    (rlay->rl_backuptable = table_find(env,
	    rlay->rl_conf.backuptable)) == NULL) {
		log_debug("%s: unknown backup table", __func__);
		goto fail;
	}

	if ((u_int)(IMSG_DATA_SIZE(imsg) - s) <
	    (rlay->rl_conf.ssl_cert_len +
	    rlay->rl_conf.ssl_key_len +
	    rlay->rl_conf.ssl_ca_len)) {
		log_debug("%s: invalid message length", __func__);
		goto fail;
	}

	if (rlay->rl_conf.ssl_cert_len) {
		if ((rlay->rl_ssl_cert = get_data(p + s,
		    rlay->rl_conf.ssl_cert_len)) == NULL)
			goto fail;
		s += rlay->rl_conf.ssl_cert_len;
	}
	if (rlay->rl_conf.ssl_key_len) {
		if ((rlay->rl_ssl_key = get_data(p + s,
		    rlay->rl_conf.ssl_key_len)) == NULL)
			goto fail;
		s += rlay->rl_conf.ssl_key_len;
	}
	if (rlay->rl_conf.ssl_ca_len) {
		if ((rlay->rl_ssl_ca = get_data(p + s,
		    rlay->rl_conf.ssl_ca_len)) == NULL)
			goto fail;
		s += rlay->rl_conf.ssl_ca_len;
	}

	TAILQ_INSERT_TAIL(env->sc_relays, rlay, rl_entry);

	env->sc_relaycount++;

	DPRINTF("%s: %s %d received relay %s", __func__,
	    ps->ps_title[privsep_process], ps->ps_instance,
	    rlay->rl_conf.name);

	return (0);

 fail:
	if (rlay->rl_ssl_cert)
		free(rlay->rl_ssl_cert);
	if (rlay->rl_ssl_key)
		free(rlay->rl_ssl_key);
	if (rlay->rl_ssl_ca)
		free(rlay->rl_ssl_ca);
	close(rlay->rl_s);
	free(rlay);
	return (-1);
}
