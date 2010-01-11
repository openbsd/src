/*	$OpenBSD: hce.c,v 1.54 2010/01/11 06:40:14 jsg Exp $	*/

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

__dead void hce_shutdown(void);
void	hce_sig_handler(int sig, short, void *);
void	hce_dispatch_imsg(int, short, void *);
void	hce_dispatch_parent(int, short, void *);
void	hce_launch_checks(int, short, void *);
void	hce_setup_events(void);
void	hce_disable_events(void);

static struct relayd *env = NULL;
struct imsgev		*iev_pfe;
struct imsgev		*iev_main;
int			 running = 0;

void
hce_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		hce_shutdown();
		break;
	default:
		fatalx("hce_sig_handler: unexpected signal");
	}
}

pid_t
hce(struct relayd *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
    int pipe_parent2relay[RELAY_MAXPROC][2], int pipe_pfe2hce[2],
    int pipe_pfe2relay[RELAY_MAXPROC][2])
{
	pid_t		 pid;
	struct passwd	*pw;
	int		 i;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("hce: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	env = x_env;
	purge_config(env, PURGE_RDRS|PURGE_RELAYS|PURGE_PROTOS);

	if ((pw = getpwnam(RELAYD_USER)) == NULL)
		fatal("hce: getpwnam");

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("hce: chroot");
	if (chdir("/") == -1)
		fatal("hce: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG mode
#endif

	setproctitle("host check engine");
	relayd_process = PROC_HCE;

	/* this is needed for icmp tests */
	icmp_init(env);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("hce: can't drop privileges");
#endif

	event_init();

	if ((iev_pfe = calloc(1, sizeof(struct imsgev))) == NULL ||
	    (iev_main = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("hce");
	imsg_init(&iev_pfe->ibuf, pipe_pfe2hce[0]);
	iev_pfe->handler = hce_dispatch_imsg;
	imsg_init(&iev_main->ibuf, pipe_parent2hce[1]);
	iev_main->handler = hce_dispatch_parent;

	iev_pfe->events = EV_READ;
	event_set(&iev_pfe->ev, iev_pfe->ibuf.fd, iev_pfe->events,
	    iev_pfe->handler, iev_pfe);
	event_add(&iev_pfe->ev, NULL);

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	signal_set(&ev_sigint, SIGINT, hce_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, hce_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_pfe2hce[1]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2pfe[1]);
	for (i = 0; i < env->sc_prefork_relay; i++) {
		close(pipe_parent2relay[i][0]);
		close(pipe_parent2relay[i][1]);
		close(pipe_pfe2relay[i][0]);
		close(pipe_pfe2relay[i][1]);
	}

	hce_setup_events();
	event_dispatch();
	hce_shutdown();

	return (0);
}

void
hce_setup_events(void)
{
	struct timeval	 tv;
	struct table	*table;

	snmp_init(env, iev_main);

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
	imsg_compose_event(iev_pfe, IMSG_SYNC, 0, 0, -1, NULL, 0);
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
			switch (table->conf.check) {
			case CHECK_ICMP:
				schedule_icmp(env, host);
				break;
			case CHECK_SCRIPT:
				check_script(host);
				break;
			default:
				/* Any other TCP-style checks */
				host->last_up = host->up;
				host->cte.host = host;
				host->cte.table = table;
				bcopy(&tv, &host->cte.tv_start,
				    sizeof(host->cte.tv_start));
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
		log_debug("hce_notify_done: host %s retry %d",
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
		log_debug("hce_notify_done: %s (%s)", host->conf.name, msg);

	imsg_compose_event(iev_pfe, IMSG_HOST_STATUS,
	    0, 0, -1, &st, sizeof(st));
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
		snmp_hosttrap(table, host);

	host->last_up = host->up;

	if (SLIST_EMPTY(&host->children))
		return;

	/* Notify for all other hosts that inherit the state from this one */
	SLIST_FOREACH(h, &host->children, child) {
		h->up = hostup;
		hce_notify_done(h, he);
	}
}

void
hce_shutdown(void)
{
	log_info("host check engine exiting");
	_exit(0);
}

void
hce_dispatch_imsg(int fd, short event, void *ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	objid_t			 id;
	struct host		*host;
	struct table		*table;
	int			 verbose;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("hce_dispatch_imsg: imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("hce_dispatch_imsg: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("hce_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_HOST_DISABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((host = host_find(env, id)) == NULL)
				fatalx("hce_dispatch_imsg: desynchronized");
			host->flags |= F_DISABLE;
			host->up = HOST_UNKNOWN;
			host->check_cnt = 0;
			host->up_cnt = 0;
			host->he = HCE_NONE;
			break;
		case IMSG_HOST_ENABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((host = host_find(env, id)) == NULL)
				fatalx("hce_dispatch_imsg: desynchronized");
			host->flags &= ~(F_DISABLE);
			host->up = HOST_UNKNOWN;
			host->he = HCE_NONE;
			break;
		case IMSG_TABLE_DISABLE:
			memcpy(&id, imsg.data, sizeof(id));
			if ((table = table_find(env, id)) == NULL)
				fatalx("hce_dispatch_imsg: desynchronized");
			table->conf.flags |= F_DISABLE;
			TAILQ_FOREACH(host, &table->hosts, entry)
				host->up = HOST_UNKNOWN;
			break;
		case IMSG_TABLE_ENABLE:
			memcpy(&id, imsg.data, sizeof(id));
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
		case IMSG_CTL_LOG_VERBOSE:
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_debug("hce_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
hce_dispatch_parent(int fd, short event, void * ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	struct ctl_script	 scr;
	ssize_t			 n;
	size_t			 len;
	static struct table	*table = NULL;
	struct host		*host, *parent;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("hce_dispatch_parent: imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("hce_dispatch_parent: msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("hce_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SCRIPT:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(scr))
				fatalx("hce_dispatch_parent: "
				    "invalid size of script request");
			bcopy(imsg.data, &scr, sizeof(scr));
			script_done(env, &scr);
			break;
		case IMSG_RECONF:
			log_debug("hce: reloading configuration");
			if (imsg.hdr.len !=
			    sizeof(struct relayd) + IMSG_HEADER_SIZE)
				fatalx("corrupted reload data");
			hce_disable_events();
			purge_config(env, PURGE_TABLES);
			merge_config(env, (struct relayd *)imsg.data);

			env->sc_tables = calloc(1, sizeof(*env->sc_tables));
			if (env->sc_tables == NULL)
				fatal(NULL);

			TAILQ_INIT(env->sc_tables);
			break;
		case IMSG_RECONF_TABLE:
			if ((table = calloc(1, sizeof(*table))) == NULL)
				fatal(NULL);
			memcpy(&table->conf, imsg.data, sizeof(table->conf));
			TAILQ_INIT(&table->hosts);
			TAILQ_INSERT_TAIL(env->sc_tables, table, entry);
			break;
		case IMSG_RECONF_SENDBUF:
			len = imsg.hdr.len - IMSG_HEADER_SIZE;
			table->sendbuf = calloc(1, len);
			(void)strlcpy(table->sendbuf, (char *)imsg.data, len);
			break;
		case IMSG_RECONF_HOST:
			if ((host = calloc(1, sizeof(*host))) == NULL)
				fatal(NULL);
			memcpy(&host->conf, imsg.data, sizeof(host->conf));
			host->tablename = table->conf.name;
			TAILQ_INSERT_TAIL(&table->hosts, host, entry);
			if (host->conf.parentid) {
				parent = host_find(env, host->conf.parentid);
				SLIST_INSERT_HEAD(&parent->children,
				    host, child);
			}
			break;
		case IMSG_RECONF_END:
			log_warnx("hce: configuration reloaded");
			hce_setup_events();
			break;
		default:
			log_debug("hce_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}
