/*	$OpenBSD: pfe.c,v 1.35 2007/09/27 07:32:50 pyr Exp $	*/

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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include <openssl/ssl.h>

#include "hoststated.h"

void	pfe_sig_handler(int sig, short, void *);
void	pfe_shutdown(void);
void	pfe_setup_events(void);
void	pfe_disable_events(void);
void	pfe_dispatch_imsg(int, short, void *);
void	pfe_dispatch_parent(int, short, void *);
void	pfe_dispatch_relay(int, short, void *);

void	pfe_sync(void);

static struct hoststated	*env = NULL;

struct imsgbuf	*ibuf_main;
struct imsgbuf	*ibuf_hce;
struct imsgbuf	*ibuf_relay;

void
pfe_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		pfe_shutdown();
	default:
		fatalx("pfe_sig_handler: unexpected signal");
	}
}

pid_t
pfe(struct hoststated *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
    int pipe_parent2relay[RELAY_MAXPROC][2], int pipe_pfe2hce[2],
    int pipe_pfe2relay[RELAY_MAXPROC][2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	int		 i;
	size_t		 size;

	switch (pid = fork()) {
	case -1:
		fatal("pfe: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	env = x_env;
	purge_config(env, PURGE_PROTOS);

	if (control_init() == -1)
		fatalx("pfe: control socket setup failed");

	init_filter(env);
	init_tables(env);

	if ((pw = getpwnam(HOSTSTATED_USER)) == NULL)
		fatal("pfe: getpwnam");

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("pfe: chroot");
	if (chdir("/") == -1)
		fatal("pfe: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG mode
#endif

	setproctitle("pf update engine");
	hoststated_process = PROC_PFE;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("pfe: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, pfe_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, pfe_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* setup pipes */
	close(pipe_pfe2hce[0]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2hce[1]);
	for (i = 0; i < env->prefork_relay; i++) {
		close(pipe_parent2relay[i][0]);
		close(pipe_parent2relay[i][1]);
		close(pipe_pfe2relay[i][0]);
	}

	size = sizeof(struct imsgbuf);
	if ((ibuf_hce = calloc(1, size)) == NULL ||
	    (ibuf_relay = calloc(env->prefork_relay, size)) == NULL ||
	    (ibuf_main = calloc(1, size)) == NULL)
		fatal("pfe");

	imsg_init(ibuf_hce, pipe_pfe2hce[1], pfe_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2pfe[1], pfe_dispatch_parent);
	for (i = 0; i < env->prefork_relay; i++)
		imsg_init(&ibuf_relay[i], pipe_pfe2relay[i][1],
		    pfe_dispatch_relay);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	pfe_setup_events();

	TAILQ_INIT(&ctl_conns);

	if (control_listen(env, ibuf_main) == -1)
		fatalx("pfe: control socket listen failed");

	/* Initial sync */
	pfe_sync();

	event_dispatch();
	pfe_shutdown();

	return (0);
}

void
pfe_shutdown(void)
{
	flush_rulesets(env);
	log_info("pf update engine exiting");
	_exit(0);
}

void
pfe_setup_events(void)
{
	int		 i;
	struct imsgbuf	*ibuf;

	ibuf_hce->events = EV_READ;
	event_set(&ibuf_hce->ev, ibuf_hce->fd, ibuf_hce->events,
	    ibuf_hce->handler, ibuf_hce);
	event_add(&ibuf_hce->ev, NULL);

	for (i = 0; i < env->prefork_relay; i++) {
		ibuf = &ibuf_relay[i];

		ibuf->events = EV_READ;
		event_set(&ibuf->ev, ibuf->fd, ibuf->events,
		    ibuf->handler, ibuf);
		event_add(&ibuf->ev, NULL);
	}
}

void
pfe_disable_events(void)
{
	int	i;

	event_del(&ibuf_hce->ev);

	for (i = 0; i < env->prefork_relay; i++)
		event_del(&ibuf_relay[i].ev);
}

void
pfe_dispatch_imsg(int fd, short event, void *ptr)
{
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	struct host		*host;
	struct table		*table;
	struct ctl_status	 st;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("pfe_dispatch_imsg: imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("pfe_dispatch_imsg: msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("pfe_dispatch_imsg: unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("pfe_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		control_imsg_forward(&imsg);
		switch (imsg.hdr.type) {
		case IMSG_HOST_STATUS:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(st))
				fatalx("pfe_dispatch_imsg: invalid request");
			memcpy(&st, imsg.data, sizeof(st));
			if ((host = host_find(env, st.id)) == NULL)
				fatalx("pfe_dispatch_imsg: invalid host id");
			if (host->flags & F_DISABLE)
				break;
			host->retry_cnt = st.retry_cnt;
			if (st.up != HOST_UNKNOWN) {
				host->check_cnt++;
				if (st.up == HOST_UP)
					host->up_cnt++;
			}
			if (host->check_cnt != st.check_cnt) {
				log_debug("pfe_dispatch_imsg: host %d => %d",
				    host->conf.id, host->up);
				fatalx("pfe_dispatch_imsg: desynchronized");
			}

			if (host->up == st.up)
				break;

			/* Forward to relay engine(s) */
			for (n = 0; n < env->prefork_relay; n++)
				imsg_compose(&ibuf_relay[n],
				    IMSG_HOST_STATUS, 0, 0, -1, &st,
				        sizeof(st));

			if ((table = table_find(env, host->conf.tableid))
			    == NULL)
				fatalx("pfe_dispatch_imsg: invalid table id");

			log_debug("pfe_dispatch_imsg: state %d for host %u %s",
			    st.up, host->conf.id, host->conf.name);

			if ((st.up == HOST_UNKNOWN && !HOST_ISUP(host->up)) ||
			    (!HOST_ISUP(st.up) && host->up == HOST_UNKNOWN)) {
				host->up = st.up;
				break;
			}

			if (st.up == HOST_UP) {
				table->conf.flags |= F_CHANGED;
				table->up++;
				host->flags |= F_ADD;
				host->flags &= ~(F_DEL);
				host->up = HOST_UP;
			} else {
				table->up--;
				table->conf.flags |= F_CHANGED;
				host->flags |= F_DEL;
				host->flags &= ~(F_ADD);
			}
			host->up = st.up;
			break;
		case IMSG_SYNC:
			pfe_sync();
			break;
		default:
			log_debug("pfe_dispatch_imsg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
pfe_dispatch_parent(int fd, short event, void * ptr)
{
	struct imsgbuf	*ibuf;
	struct imsg	 imsg;
	ssize_t		 n;

	static struct service	*service = NULL;
	static struct table	*table = NULL;
	struct host		*host;
	struct address		*virt;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("pfe_dispatch_parent: unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("pfe_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RECONF:
			log_debug("pfe: reloading configuration");
			if (imsg.hdr.len !=
			    sizeof(struct hoststated) + IMSG_HEADER_SIZE)
				fatalx("corrupted reload data");
			pfe_disable_events();
			purge_config(env, PURGE_EVERYTHING);
			merge_config(env, (struct hoststated *)imsg.data);

			env->tables = calloc(1, sizeof(*env->tables));
			env->services = calloc(1, sizeof(*env->services));
			if (env->tables == NULL || env->services == NULL)
				fatal(NULL);

			TAILQ_INIT(env->tables);
			TAILQ_INIT(env->services);
			break;
		case IMSG_RECONF_TABLE:
			if ((table = calloc(1, sizeof(*table))) == NULL)
				fatal(NULL);
			memcpy(&table->conf, imsg.data, sizeof(table->conf));
			TAILQ_INIT(&table->hosts);
			TAILQ_INSERT_TAIL(env->tables, table, entry);
			break;
		case IMSG_RECONF_HOST:
			if ((host = calloc(1, sizeof(*host))) == NULL)
				fatal(NULL);
			memcpy(&host->conf, imsg.data, sizeof(host->conf));
			host->tablename = table->conf.name;
			TAILQ_INSERT_TAIL(&table->hosts, host, entry);
			break;
		case IMSG_RECONF_SERVICE:
			if ((service = calloc(1, sizeof(*service))) == NULL)
				fatal(NULL);
			memcpy(&service->conf, imsg.data,
			    sizeof(service->conf));
			service->table = table_find(env,
			     service->conf.table_id);
			if (service->conf.backup_id == EMPTY_TABLE)
				service->backup = &env->empty_table;
			else
				service->backup = table_find(env,
				    service->conf.backup_id);
			if (service->table == NULL || service->backup == NULL)
				fatal("pfe_dispatch_parent:"
				    " corrupted configuration");
			log_debug("pfe_dispatch_parent: service->table: %s",
			    service->table->conf.name);
			log_debug("pfe_dispatch_parent: service->backup: %s",
			    service->backup->conf.name);
			TAILQ_INIT(&service->virts);
			TAILQ_INSERT_TAIL(env->services, service, entry);
			break;
		case IMSG_RECONF_VIRT:
			if ((virt = calloc(1, sizeof(*virt))) == NULL)
				fatal(NULL);
			memcpy(virt, imsg.data, sizeof(*virt));
			TAILQ_INSERT_TAIL(&service->virts, virt, entry);
			break;
		case IMSG_RECONF_END:
			log_warnx("pfe: configuration reloaded");
			pfe_setup_events();
			pfe_sync();
			break;
		default:
			log_debug("pfe_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
pfe_dispatch_relay(int fd, short event, void * ptr)
{
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct ctl_natlook	 cnl;
	struct ctl_stats	 crs;
	struct relay		*rlay;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("pfe_dispatch_relay: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NATLOOK:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(cnl))
				fatalx("invalid imsg header len");
			bcopy(imsg.data, &cnl, sizeof(cnl));
			if (cnl.proc > env->prefork_relay)
				fatalx("pfe_dispatch_relay: "
				    "invalid relay proc");
			if (natlook(env, &cnl) != 0)
				cnl.in = -1;
			imsg_compose(&ibuf_relay[cnl.proc], IMSG_NATLOOK, 0, 0,
			    -1, &cnl, sizeof(cnl));
			break;
		case IMSG_STATISTICS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(crs))
				fatalx("invalid imsg header len");
			bcopy(imsg.data, &crs, sizeof(crs));
			if (crs.proc > env->prefork_relay)
				fatalx("pfe_dispatch_relay: "
				    "invalid relay proc");
			if ((rlay = relay_find(env, crs.id)) == NULL)
				fatalx("pfe_dispatch_relay: invalid relay id");
			bcopy(&crs, &rlay->stats[crs.proc], sizeof(crs));
			rlay->stats[crs.proc].interval =
			    env->statinterval.tv_sec;
			break;
		default:
			log_debug("pfe_dispatch_relay: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
show(struct ctl_conn *c)
{
	struct service	*service;
	struct host	*host;
	struct relay	*rlay;

	TAILQ_FOREACH(service, env->services, entry) {
		imsg_compose(&c->ibuf, IMSG_CTL_SERVICE, 0, 0, -1,
		    service, sizeof(*service));
		if (service->conf.flags & F_DISABLE)
			continue;

		imsg_compose(&c->ibuf, IMSG_CTL_TABLE, 0, 0, -1,
		    service->table, sizeof(*service->table));
		if (!(service->table->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &service->table->hosts, entry)
				imsg_compose(&c->ibuf, IMSG_CTL_HOST, 0, 0, -1,
				    host, sizeof(*host));

		if (service->backup->conf.id == EMPTY_TABLE)
			continue;
		imsg_compose(&c->ibuf, IMSG_CTL_TABLE, 0, 0, -1,
		    service->backup, sizeof(*service->backup));
		if (!(service->backup->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &service->backup->hosts, entry)
				imsg_compose(&c->ibuf, IMSG_CTL_HOST, 0, 0, -1,
				    host, sizeof(*host));
	}
	TAILQ_FOREACH(rlay, &env->relays, entry) {
		rlay->stats[env->prefork_relay].id = EMPTY_ID;
		imsg_compose(&c->ibuf, IMSG_CTL_RELAY, 0, 0, -1,
		    rlay, sizeof(*rlay));
		imsg_compose(&c->ibuf, IMSG_CTL_STATISTICS, 0, 0, -1,
		    &rlay->stats, sizeof(rlay->stats));

		if (rlay->dsttable == NULL)
			continue;
		imsg_compose(&c->ibuf, IMSG_CTL_TABLE, 0, 0, -1,
		    rlay->dsttable, sizeof(*rlay->dsttable));
		if (!(rlay->dsttable->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rlay->dsttable->hosts, entry)
				imsg_compose(&c->ibuf, IMSG_CTL_HOST, 0, 0, -1,
				    host, sizeof(*host));
	}

	imsg_compose(&c->ibuf, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

void
show_sessions(struct ctl_conn *c)
{
	int		 n, proc, done;
	struct imsg	 imsg;

	for (proc = 0; proc < env->prefork_relay; proc++) {
		/*
		 * Request all the running sessions from the process
		 */
		imsg_compose(&ibuf_relay[proc],
		    IMSG_CTL_SESSION, 0, 0, -1, NULL, 0);
		while (ibuf_relay[proc].w.queued)
			if (msgbuf_write(&ibuf_relay[proc].w) < 0)
				fatalx("write error");

		/*
		 * Wait for the reply and forward the messages to the
		 * control connection.
		 */
		done = 0;
		while (!done) {
			do {
				if ((n = imsg_read(&ibuf_relay[proc])) == -1)
					fatalx("imsg_read error");
			} while (n == -2); /* handle non-blocking I/O */
			while (!done) {
				if ((n = imsg_get(&ibuf_relay[proc],
				    &imsg)) == -1)
					fatalx("imsg_get error");
				if (n == 0)
					break;
				switch (imsg.hdr.type) {
				case IMSG_CTL_SESSION:
					imsg_compose(&c->ibuf,
					    IMSG_CTL_SESSION, proc, 0, -1,
					    imsg.data, sizeof(struct session));
					break;
				case IMSG_CTL_END:
					done = 1;
					break;
				default:
					fatalx("wrong message for session");
					break;
				}
				imsg_free(&imsg);
			}
		}
	}

	imsg_compose(&c->ibuf, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

int
disable_service(struct ctl_conn *c, struct ctl_id *id)
{
	struct service	*service;

	if (id->id == EMPTY_ID)
		service = service_findbyname(env, id->name);
	else
		service = service_find(env, id->id);
	if (service == NULL)
		return (-1);
	id->id = service->conf.id;

	if (service->conf.flags & F_DISABLE)
		return (0);

	service->conf.flags |= F_DISABLE;
	service->conf.flags &= ~(F_ADD);
	service->conf.flags |= F_DEL;
	service->table->conf.flags |= F_DISABLE;
	log_debug("disable_service: disabled service %d", service->conf.id);
	pfe_sync();
	return (0);
}

int
enable_service(struct ctl_conn *c, struct ctl_id *id)
{
	struct service	*service;
	struct ctl_id	 eid;

	if (id->id == EMPTY_ID)
		service = service_findbyname(env, id->name);
	else
		service = service_find(env, id->id);
	if (service == NULL)
		return (-1);
	id->id = service->conf.id;

	if (!(service->conf.flags & F_DISABLE))
		return (0);

	service->conf.flags &= ~(F_DISABLE);
	service->conf.flags &= ~(F_DEL);
	service->conf.flags |= F_ADD;
	log_debug("enable_service: enabled service %d", service->conf.id);

	bzero(&eid, sizeof(eid));

	/* XXX: we're syncing twice */
	eid.id = service->table->conf.id;
	if (enable_table(c, &eid) == -1)
		return (-1);
	if (service->backup->conf.id == EMPTY_ID)
		return (0);
	eid.id = service->backup->conf.id;
	if (enable_table(c, &eid) == -1)
		return (-1);
	return (0);
}

int
disable_table(struct ctl_conn *c, struct ctl_id *id)
{
	struct table	*table;
	struct service	*service;
	struct host	*host;

	if (id->id == EMPTY_ID)
		table = table_findbyname(env, id->name);
	else
		table = table_find(env, id->id);
	if (table == NULL)
		return (-1);
	id->id = table->conf.id;
	if ((service = service_find(env, table->conf.serviceid)) == NULL)
		fatalx("disable_table: desynchronised");

	if (table->conf.flags & F_DISABLE)
		return (0);
	table->conf.flags |= (F_DISABLE|F_CHANGED);
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose(ibuf_hce, IMSG_TABLE_DISABLE, 0, 0, -1,
	    &table->conf.id, sizeof(table->conf.id));
	log_debug("disable_table: disabled table %d", table->conf.id);
	pfe_sync();
	return (0);
}

int
enable_table(struct ctl_conn *c, struct ctl_id *id)
{
	struct service	*service;
	struct table	*table;
	struct host	*host;

	if (id->id == EMPTY_ID)
		table = table_findbyname(env, id->name);
	else
		table = table_find(env, id->id);
	if (table == NULL)
		return (-1);
	id->id = table->conf.id;

	if ((service = service_find(env, table->conf.serviceid)) == NULL)
		fatalx("enable_table: desynchronised");

	if (!(table->conf.flags & F_DISABLE))
		return (0);
	table->conf.flags &= ~(F_DISABLE);
	table->conf.flags |= F_CHANGED;
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose(ibuf_hce, IMSG_TABLE_ENABLE, 0, 0, -1,
	    &table->conf.id, sizeof(table->conf.id));
	log_debug("enable_table: enabled table %d", table->conf.id);
	pfe_sync();
	return (0);
}

int
disable_host(struct ctl_conn *c, struct ctl_id *id)
{
	struct host	*host;
	struct table	*table;
	int		 n;

	if (id->id == EMPTY_ID)
		host = host_findbyname(env, id->name);
	else
		host = host_find(env, id->id);
	if (host == NULL)
		return (-1);
	id->id = host->conf.id;

	if (host->flags & F_DISABLE)
		return (0);

	if (host->up == HOST_UP) {
		if ((table = table_find(env, host->conf.tableid)) == NULL)
			fatalx("disable_host: invalid table id");
		table->up--;
		table->conf.flags |= F_CHANGED;
	}

	host->up = HOST_UNKNOWN;
	host->flags |= F_DISABLE;
	host->flags |= F_DEL;
	host->flags &= ~(F_ADD);
	host->check_cnt = 0;
	host->up_cnt = 0;

	imsg_compose(ibuf_hce, IMSG_HOST_DISABLE, 0, 0, -1,
	    &host->conf.id, sizeof(host->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->prefork_relay; n++)
		imsg_compose(&ibuf_relay[n],
		    IMSG_HOST_DISABLE, 0, 0, -1,
		    &host->conf.id, sizeof(host->conf.id));
	log_debug("disable_host: disabled host %d", host->conf.id);
	pfe_sync();
	return (0);
}

int
enable_host(struct ctl_conn *c, struct ctl_id *id)
{
	struct host	*host;
	int		 n;

	if (id->id == EMPTY_ID)
		host = host_findbyname(env, id->name);
	else
		host = host_find(env, id->id);
	if (host == NULL)
		return (-1);
	id->id = host->conf.id;

	if (!(host->flags & F_DISABLE))
		return (0);

	host->up = HOST_UNKNOWN;
	host->flags &= ~(F_DISABLE);
	host->flags &= ~(F_DEL);
	host->flags &= ~(F_ADD);

	imsg_compose(ibuf_hce, IMSG_HOST_ENABLE, 0, 0, -1, 
	    &host->conf.id, sizeof (host->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->prefork_relay; n++)
		imsg_compose(&ibuf_relay[n],
		    IMSG_HOST_ENABLE, 0, 0, -1,
		    &host->conf.id, sizeof(host->conf.id));
	log_debug("enable_host: enabled host %d", host->conf.id);
	pfe_sync();
	return (0);
}

void
pfe_sync(void)
{
	struct service		*service;
	struct table		*active;
	struct table		*table;
	struct ctl_id		 id;
	struct imsg		 imsg;
	struct ctl_demote	 demote;

	bzero(&id, sizeof(id));
	bzero(&imsg, sizeof(imsg));
	TAILQ_FOREACH(service, env->services, entry) {
		service->conf.flags &= ~(F_BACKUP);
		service->conf.flags &= ~(F_DOWN);

		if (service->conf.flags & F_DISABLE ||
		    (service->table->up == 0 && service->backup->up == 0)) {
			service->conf.flags |= F_DOWN;
			active = NULL;
		} else if (service->table->up == 0 && service->backup->up > 0) {
			service->conf.flags |= F_BACKUP;
			active = service->backup;
			active->conf.flags |=
			    service->table->conf.flags & F_CHANGED;
			active->conf.flags |=
			    service->backup->conf.flags & F_CHANGED;
		} else
			active = service->table;

		if (active != NULL && active->conf.flags & F_CHANGED) {
			id.id = active->conf.id;
			imsg.hdr.type = IMSG_CTL_TABLE_CHANGED;
			imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
			imsg.data = &id;
			sync_table(env, service, active);
			control_imsg_forward(&imsg);
		}

		if (service->conf.flags & F_DOWN) {
			if (service->conf.flags & F_ACTIVE_RULESET) {
				flush_table(env, service);
				log_debug("pfe_sync: disabling ruleset");
				service->conf.flags &= ~(F_ACTIVE_RULESET);
				id.id = service->conf.id;
				imsg.hdr.type = IMSG_CTL_PULL_RULESET;
				imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
				imsg.data = &id;
				sync_ruleset(env, service, 0);
				control_imsg_forward(&imsg);
			}
		} else if (!(service->conf.flags & F_ACTIVE_RULESET)) {
			log_debug("pfe_sync: enabling ruleset");
			service->conf.flags |= F_ACTIVE_RULESET;
			id.id = service->conf.id;
			imsg.hdr.type = IMSG_CTL_PUSH_RULESET;
			imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
			imsg.data = &id;
			sync_ruleset(env, service, 1);
			control_imsg_forward(&imsg);
		}
	}

	TAILQ_FOREACH(table, env->tables, entry) {
		/*
		 * clean up change flag.
		 */
		table->conf.flags &= ~(F_CHANGED);
		
		/*
		 * handle demotion.
		 */
		if ((table->conf.flags & F_DEMOTE) == 0)
			continue;
		demote.level = 0;
		if (table->up && table->conf.flags & F_DEMOTED) {
			demote.level = -1;
			table->conf.flags &= ~F_DEMOTED;
		}
		else if (!table->up && !(table->conf.flags & F_DEMOTED)) {
			demote.level = 1;
			table->conf.flags |= F_DEMOTED;
		}
		if (demote.level == 0)
			continue;
		log_debug("pfe_sync: demote %d table '%s' group '%s'",
		    demote.level, table->conf.name, table->conf.demote_group);
		(void)strlcpy(demote.group, table->conf.demote_group,
		    sizeof(demote.group));
		imsg_compose(ibuf_main, IMSG_DEMOTE, 0, 0, -1,
		    &demote, sizeof(demote));
	}
}
