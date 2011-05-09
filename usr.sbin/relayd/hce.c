/*	$OpenBSD: hce.c,v 1.59 2011/05/09 12:08:47 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <pwd.h>

#include <openssl/ssl.h>

#include "relayd.h"

void	 hce_init(struct privsep *, struct privsep_proc *p, void *);
void	 hce_sig_handler(int sig, short, void *);
void	 hce_launch_checks(int, short, void *);
void	 hce_setup_events(void);
void	 hce_disable_events(void);

int	 hce_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 hce_dispatch_pfe(int, struct privsep_proc *, struct imsg *);

static struct relayd *env = NULL;
int			 running = 0;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	hce_dispatch_parent },
	{ "pfe",	PROC_PFE,	hce_dispatch_pfe },
};

pid_t
hce(struct privsep *ps, struct privsep_proc *p)
{
	env = ps->ps_env;

	/* this is needed for icmp tests */
	icmp_init(env);

	return (proc_run(ps, p, procs, nitems(procs), hce_init, NULL));
}

void
hce_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	purge_config(env, PURGE_RDRS|PURGE_RELAYS|PURGE_PROTOS);

	env->sc_id = getpid() & 0xffff;

	/* Allow maximum available sockets for TCP checks */
	socket_rlimit(-1);

	hce_setup_events();
}

void
hce_setup_events(void)
{
	struct timeval	 tv;
	struct table	*table;

	snmp_init(env, PROC_PARENT);

	if (!TAILQ_EMPTY(env->sc_tables)) {
		evtimer_set(&env->sc_ev, hce_launch_checks, env);
		bzero(&tv, sizeof(tv));
		evtimer_add(&env->sc_ev, &tv);
	}

	if (env->sc_flags & F_SSL) {
		ssl_init(env);
		TAILQ_FOREACH(table, env->sc_tables, entry) {
			if (!(table->conf.flags & F_SSL))
				continue;
			table->ssl_ctx = ssl_ctx_create(env);
		}
	}
}

void
hce_disable_events(void)
{
	struct table	*table;
	struct host	*host;

	evtimer_del(&env->sc_ev);
	TAILQ_FOREACH(table, env->sc_tables, entry) {
		TAILQ_FOREACH(host, &table->hosts, entry) {
			host->he = HCE_ABORT;
			event_del(&host->cte.ev);
			close(host->cte.s);
		}
	}
	if (env->sc_has_icmp) {
		event_del(&env->sc_icmp_send.ev);
		event_del(&env->sc_icmp_recv.ev);
	}
	if (env->sc_has_icmp6) {
		event_del(&env->sc_icmp6_send.ev);
		event_del(&env->sc_icmp6_recv.ev);
	}
}

void
hce_launch_checks(int fd, short event, void *arg)
{
	struct host		*host;
	struct table		*table;
	struct timeval		 tv;

	/*
	 * notify pfe checks are done and schedule next check
	 */
	proc_compose_imsg(env->sc_ps, PROC_PFE, -1, IMSG_SYNC, -1, NULL, 0);
	TAILQ_FOREACH(table, env->sc_tables, entry) {
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if ((host->flags & F_CHECK_DONE) == 0)
				host->he = HCE_INTERVAL_TIMEOUT;
			host->flags &= ~(F_CHECK_SENT|F_CHECK_DONE);
			event_del(&host->cte.ev);
		}
	}

	if (gettimeofday(&tv, NULL) == -1)
		fatal("hce_launch_checks: gettimeofday");

	TAILQ_FOREACH(table, env->sc_tables, entry) {
		if (table->conf.flags & F_DISABLE)
			continue;
		if (table->conf.skip_cnt) {
			if (table->skipped++ > table->conf.skip_cnt)
				table->skipped = 0;
			if (table->skipped != 1)
				continue;
		}
		if (table->conf.check == CHECK_NOCHECK)
			fatalx("hce_launch_checks: unknown check type");

		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (host->flags & F_DISABLE || host->conf.parentid)
				continue;
			bcopy(&tv, &host->cte.tv_start,
			    sizeof(host->cte.tv_start));
			switch (table->conf.check) {
			case CHECK_ICMP:
				schedule_icmp(env, host);
				break;
			case CHECK_SCRIPT:
				check_script(env, host);
				break;
			default:
				/* Any other TCP-style checks */
				host->last_up = host->up;
				host->cte.host = host;
				host->cte.table = table;
				check_tcp(&host->cte);
				break;
			}
		}
	}
	check_icmp(env, &tv);

	bcopy(&env->sc_interval, &tv, sizeof(tv));
	evtimer_add(&env->sc_ev, &tv);
}

void
hce_notify_done(struct host *host, enum host_error he)
{
	struct table		*table;
	struct ctl_status	 st;
	struct timeval		 tv_now, tv_dur;
	u_long			 duration;
	u_int			 logopt;
	struct host		*h;
	int			 hostup;
	const char		*msg;

	hostup = host->up;
	host->he = he;

	if (host->up == HOST_DOWN && host->retry_cnt) {
		log_debug("%s: host %s retry %d", __func__,
		    host->conf.name, host->retry_cnt);
		host->up = host->last_up;
		host->retry_cnt--;
	} else
		host->retry_cnt = host->conf.retry;
	if (host->up != HOST_UNKNOWN) {
		host->check_cnt++;
		if (host->up == HOST_UP)
			host->up_cnt++;
	}
	st.id = host->conf.id;
	st.up = host->up;
	st.check_cnt = host->check_cnt;
	st.retry_cnt = host->retry_cnt;
	st.he = he;
	host->flags |= (F_CHECK_SENT|F_CHECK_DONE);
	msg = host_error(he);
	if (msg)
		log_debug("%s: %s (%s)", __func__, host->conf.name, msg);

	proc_compose_imsg(env->sc_ps, PROC_PFE, -1, IMSG_HOST_STATUS,
	    -1, &st, sizeof(st));
	if (host->up != host->last_up)
		logopt = RELAYD_OPT_LOGUPDATE;
	else
		logopt = RELAYD_OPT_LOGNOTIFY;

	if (gettimeofday(&tv_now, NULL) == -1)
		fatal("hce_notify_done: gettimeofday");
	timersub(&tv_now, &host->cte.tv_start, &tv_dur);
	if (timercmp(&host->cte.tv_start, &tv_dur, >))
		duration = (tv_dur.tv_sec * 1000) + (tv_dur.tv_usec / 1000.0);
	else
		duration = 0;

	if ((table = table_find(env, host->conf.tableid)) == NULL)
		fatalx("hce_notify_done: invalid table id");

	if (env->sc_opts & logopt) {
		log_info("host %s, check %s%s (%lums), state %s -> %s, "
		    "availability %s",
		    host->conf.name, table_check(table->conf.check),
		    (table->conf.flags & F_SSL) ? " use ssl" : "", duration,
		    host_status(host->last_up), host_status(host->up),
		    print_availability(host->check_cnt, host->up_cnt));
	}

	if (host->last_up != host->up)
		snmp_hosttrap(env, table, host);

	host->last_up = host->up;

	if (SLIST_EMPTY(&host->children))
		return;

	/* Notify for all other hosts that inherit the state from this one */
	SLIST_FOREACH(h, &host->children, child) {
		h->up = hostup;
		hce_notify_done(h, he);
	}
}

int
hce_dispatch_pfe(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	objid_t			 id;
	struct host		*host;
	struct table		*table;

	switch (imsg->hdr.type) {
	case IMSG_HOST_DISABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((host = host_find(env, id)) == NULL)
			fatalx("hce_dispatch_imsg: desynchronized");
		host->flags |= F_DISABLE;
		host->up = HOST_UNKNOWN;
		host->check_cnt = 0;
		host->up_cnt = 0;
		host->he = HCE_NONE;
		break;
	case IMSG_HOST_ENABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((host = host_find(env, id)) == NULL)
			fatalx("hce_dispatch_imsg: desynchronized");
		host->flags &= ~(F_DISABLE);
		host->up = HOST_UNKNOWN;
		host->he = HCE_NONE;
		break;
	case IMSG_TABLE_DISABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((table = table_find(env, id)) == NULL)
			fatalx("hce_dispatch_imsg: desynchronized");
		table->conf.flags |= F_DISABLE;
		TAILQ_FOREACH(host, &table->hosts, entry)
			host->up = HOST_UNKNOWN;
		break;
	case IMSG_TABLE_ENABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((table = table_find(env, id)) == NULL)
			fatalx("hce_dispatch_imsg: desynchronized");
		table->conf.flags &= ~(F_DISABLE);
		TAILQ_FOREACH(host, &table->hosts, entry)
			host->up = HOST_UNKNOWN;
		break;
	case IMSG_CTL_POLL:
		evtimer_del(&env->sc_ev);
		TAILQ_FOREACH(table, env->sc_tables, entry)
			table->skipped = 0;
		hce_launch_checks(-1, EV_TIMEOUT, env);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
hce_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct ctl_script	 scr;
	size_t			 len;
	static struct table	*table = NULL;
	struct host		*host, *parent;

	switch (imsg->hdr.type) {
	case IMSG_SCRIPT:
		IMSG_SIZE_CHECK(imsg, &scr);
		bcopy(imsg->data, &scr, sizeof(scr));
		script_done(env, &scr);
		break;
	case IMSG_RECONF:
		IMSG_SIZE_CHECK(imsg, env);
		log_debug("%s: reloading configuration", __func__);
		hce_disable_events();
		purge_config(env, PURGE_TABLES);
		merge_config(env, (struct relayd *)imsg->data);

		env->sc_tables = calloc(1, sizeof(*env->sc_tables));
		if (env->sc_tables == NULL)
			fatal(NULL);

		TAILQ_INIT(env->sc_tables);
		break;
	case IMSG_RECONF_TABLE:
		if ((table = calloc(1, sizeof(*table))) == NULL)
			fatal(NULL);
		memcpy(&table->conf, imsg->data, sizeof(table->conf));
		TAILQ_INIT(&table->hosts);
		TAILQ_INSERT_TAIL(env->sc_tables, table, entry);
		break;
	case IMSG_RECONF_SENDBUF:
		len = imsg->hdr.len - IMSG_HEADER_SIZE;
		table->sendbuf = calloc(1, len);
		(void)strlcpy(table->sendbuf, (char *)imsg->data, len);
		break;
	case IMSG_RECONF_HOST:
		if ((host = calloc(1, sizeof(*host))) == NULL)
			fatal(NULL);
		memcpy(&host->conf, imsg->data, sizeof(host->conf));
		host->tablename = table->conf.name;
		TAILQ_INSERT_TAIL(&table->hosts, host, entry);
		if (host->conf.parentid) {
			parent = host_find(env, host->conf.parentid);
			SLIST_INSERT_HEAD(&parent->children,
			    host, child);
		}
		break;
	case IMSG_RECONF_END:
		log_warnx("%s: configuration reloaded", __func__);
		hce_setup_events();
		break;
	default:
		return (-1);
	}

	return (0);
}
