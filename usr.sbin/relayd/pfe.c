/*	$OpenBSD: pfe.c,v 1.66 2010/11/16 15:31:01 jsg Exp $	*/

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

#include "relayd.h"

void	pfe_sig_handler(int sig, short, void *);
void	pfe_shutdown(void);
void	pfe_setup_events(void);
void	pfe_disable_events(void);
void	pfe_dispatch_imsg(int, short, void *);
void	pfe_dispatch_parent(int, short, void *);
void	pfe_dispatch_relay(int, short, void *);
void	pfe_sync(void);
void	pfe_statistics(int, short, void *);

static struct relayd	*env = NULL;

struct imsgev	*iev_main;
struct imsgev	*iev_hce;
struct imsgev	*iev_relay;

void
pfe_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		pfe_shutdown();
		break;
	case SIGCHLD:
	case SIGHUP:
	case SIGPIPE:
		/* ignore */
		break;
	default:
		fatalx("pfe_sig_handler: unexpected signal");
	}
}

pid_t
pfe(struct relayd *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
    int pipe_parent2relay[RELAY_MAXPROC][2], int pipe_pfe2hce[2],
    int pipe_pfe2relay[RELAY_MAXPROC][2])
{
	pid_t		 pid;
	struct passwd	*pw;
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

	if ((pw = getpwnam(RELAYD_USER)) == NULL)
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
	relayd_process = PROC_PFE;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("pfe: cannot drop privileges");
#endif

	event_init();

	signal_set(&env->sc_evsigint, SIGINT, pfe_sig_handler, env);
	signal_set(&env->sc_evsigterm, SIGTERM, pfe_sig_handler, env);
	signal_set(&env->sc_evsigchld, SIGCHLD, pfe_sig_handler, env);
	signal_set(&env->sc_evsighup, SIGHUP, pfe_sig_handler, env);
	signal_set(&env->sc_evsigpipe, SIGPIPE, pfe_sig_handler, env);

	signal_add(&env->sc_evsigint, NULL);
	signal_add(&env->sc_evsigterm, NULL);
	signal_add(&env->sc_evsigchld, NULL);
	signal_add(&env->sc_evsighup, NULL);
	signal_add(&env->sc_evsigpipe, NULL);

	/* setup pipes */
	close(pipe_pfe2hce[0]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2hce[1]);
	for (i = 0; i < env->sc_prefork_relay; i++) {
		close(pipe_parent2relay[i][0]);
		close(pipe_parent2relay[i][1]);
		close(pipe_pfe2relay[i][0]);
	}

	size = sizeof(struct imsgev);
	if ((iev_hce = calloc(1, size)) == NULL ||
	    (iev_relay = calloc(env->sc_prefork_relay, size)) == NULL ||
	    (iev_main = calloc(1, size)) == NULL)
		fatal("pfe");

	imsg_init(&iev_hce->ibuf, pipe_pfe2hce[1]);
	iev_hce->handler = pfe_dispatch_imsg;
	imsg_init(&iev_main->ibuf, pipe_parent2pfe[1]);
	iev_main->handler = pfe_dispatch_parent;
	for (i = 0; i < env->sc_prefork_relay; i++) {
		imsg_init(&iev_relay[i].ibuf, pipe_pfe2relay[i][1]);
		iev_relay[i].handler = pfe_dispatch_relay;
	}

	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	pfe_setup_events();

	TAILQ_INIT(&ctl_conns);

	if (control_listen(env, iev_main, iev_hce) == -1)
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
	struct imsgev	*iev;
	struct timeval	 tv;

	iev_hce->events = EV_READ;
	event_set(&iev_hce->ev, iev_hce->ibuf.fd, iev_hce->events,
	    iev_hce->handler, iev_hce);
	event_add(&iev_hce->ev, NULL);

	for (i = 0; i < env->sc_prefork_relay; i++) {
		iev = &iev_relay[i];

		iev->events = EV_READ;
		event_set(&iev->ev, iev->ibuf.fd, iev->events,
		    iev->handler, iev);
		event_add(&iev->ev, NULL);
	}

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, pfe_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}

void
pfe_disable_events(void)
{
	int	i;

	event_del(&iev_hce->ev);

	for (i = 0; i < env->sc_prefork_relay; i++)
		event_del(&iev_relay[i].ev);

	event_del(&env->sc_statev);
}

void
pfe_dispatch_imsg(int fd, short event, void *ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	struct host		*host;
	struct table		*table;
	struct ctl_status	 st;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("pfe_dispatch_imsg: imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("pfe_dispatch_imsg: msgbuf_write");
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
			host->he = st.he;
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
			for (n = 0; n < env->sc_prefork_relay; n++)
				imsg_compose_event(&iev_relay[n],
				    IMSG_HOST_STATUS, 0, 0, -1, &st,
				    sizeof(st));

			if ((table = table_find(env, host->conf.tableid))
			    == NULL)
				fatalx("pfe_dispatch_imsg: invalid table id");

			log_debug("pfe_dispatch_imsg: state %d for host %u %s",
			    st.up, host->conf.id, host->conf.name);

			/*
			 * Do not change the table state when the host
			 * state switches between UNKNOWN and DOWN.
			 */
			if (HOST_ISUP(st.up)) {
				table->conf.flags |= F_CHANGED;
				table->up++;
				host->flags |= F_ADD;
				host->flags &= ~(F_DEL);
			} else if (HOST_ISUP(host->up)) {
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
	imsg_event_add(iev);
}

void
pfe_dispatch_parent(int fd, short event, void * ptr)
{
	struct imsgev	*iev;
	struct imsgbuf	*ibuf;
	struct imsg	 imsg;
	ssize_t		 n;

	static struct rdr	*rdr = NULL;
	static struct table	*table = NULL;
	struct host		*host, *parent;
	struct address		*virt;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}


	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
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
			    sizeof(struct relayd) + IMSG_HEADER_SIZE)
				fatalx("corrupted reload data");
			pfe_disable_events();
			purge_config(env, PURGE_RDRS|PURGE_TABLES);
			merge_config(env, (struct relayd *)imsg.data);
			/*
			 * no relays when reconfiguring yet.
			 */
			env->sc_relays = NULL;
			env->sc_protos = NULL;

			env->sc_tables = calloc(1, sizeof(*env->sc_tables));
			env->sc_rdrs = calloc(1, sizeof(*env->sc_rdrs));
			if (env->sc_tables == NULL || env->sc_rdrs == NULL)
				fatal(NULL);

			TAILQ_INIT(env->sc_tables);
			TAILQ_INIT(env->sc_rdrs);
			break;
		case IMSG_RECONF_TABLE:
			if ((table = calloc(1, sizeof(*table))) == NULL)
				fatal(NULL);
			memcpy(&table->conf, imsg.data, sizeof(table->conf));
			TAILQ_INIT(&table->hosts);
			TAILQ_INSERT_TAIL(env->sc_tables, table, entry);
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
		case IMSG_RECONF_RDR:
			if ((rdr = calloc(1, sizeof(*rdr))) == NULL)
				fatal(NULL);
			memcpy(&rdr->conf, imsg.data,
			    sizeof(rdr->conf));
			rdr->table = table_find(env,
			     rdr->conf.table_id);
			if (rdr->conf.backup_id == EMPTY_TABLE)
				rdr->backup = &env->sc_empty_table;
			else
				rdr->backup = table_find(env,
				    rdr->conf.backup_id);
			if (rdr->table == NULL || rdr->backup == NULL)
				fatal("pfe_dispatch_parent:"
				    " corrupted configuration");
			log_debug("pfe_dispatch_parent: rdr->table: %s",
			    rdr->table->conf.name);
			log_debug("pfe_dispatch_parent: rdr->backup: %s",
			    rdr->backup->conf.name);
			TAILQ_INIT(&rdr->virts);
			TAILQ_INSERT_TAIL(env->sc_rdrs, rdr, entry);
			break;
		case IMSG_RECONF_VIRT:
			if ((virt = calloc(1, sizeof(*virt))) == NULL)
				fatal(NULL);
			memcpy(virt, imsg.data, sizeof(*virt));
			TAILQ_INSERT_TAIL(&rdr->virts, virt, entry);
			break;
		case IMSG_RECONF_END:
			log_warnx("pfe: configuration reloaded");
			init_tables(env);
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
	imsg_event_add(iev);
}

void
pfe_dispatch_relay(int fd, short event, void * ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	struct ctl_natlook	 cnl;
	struct ctl_stats	 crs;
	struct relay		*rlay;

	iev = ptr;
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
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
			if (cnl.proc > env->sc_prefork_relay)
				fatalx("pfe_dispatch_relay: "
				    "invalid relay proc");
			if (natlook(env, &cnl) != 0)
				cnl.in = -1;
			imsg_compose_event(&iev_relay[cnl.proc], IMSG_NATLOOK,
			    0, 0, -1, &cnl, sizeof(cnl));
			break;
		case IMSG_STATISTICS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(crs))
				fatalx("invalid imsg header len");
			bcopy(imsg.data, &crs, sizeof(crs));
			if (crs.proc > env->sc_prefork_relay)
				fatalx("pfe_dispatch_relay: "
				    "invalid relay proc");
			if ((rlay = relay_find(env, crs.id)) == NULL)
				fatalx("pfe_dispatch_relay: invalid relay id");
			bcopy(&crs, &rlay->rl_stats[crs.proc], sizeof(crs));
			rlay->rl_stats[crs.proc].interval =
			    env->sc_statinterval.tv_sec;
			break;
		default:
			log_debug("pfe_dispatch_relay: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
show(struct ctl_conn *c)
{
	struct rdr	*rdr;
	struct host	*host;
	struct relay	*rlay;
	struct router	*rt;
	struct netroute	*nr;

	if (env->sc_rdrs == NULL)
		goto relays;
	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		imsg_compose_event(&c->iev, IMSG_CTL_RDR, 0, 0, -1,
		    rdr, sizeof(*rdr));
		if (rdr->conf.flags & F_DISABLE)
			continue;

		imsg_compose_event(&c->iev, IMSG_CTL_RDR_STATS, 0, 0, -1,
		    &rdr->stats, sizeof(rdr->stats));

		imsg_compose_event(&c->iev, IMSG_CTL_TABLE, 0, 0, -1,
		    rdr->table, sizeof(*rdr->table));
		if (!(rdr->table->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rdr->table->hosts, entry)
				imsg_compose_event(&c->iev, IMSG_CTL_HOST,
				    0, 0, -1, host, sizeof(*host));

		if (rdr->backup->conf.id == EMPTY_TABLE)
			continue;
		imsg_compose_event(&c->iev, IMSG_CTL_TABLE, 0, 0, -1,
		    rdr->backup, sizeof(*rdr->backup));
		if (!(rdr->backup->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rdr->backup->hosts, entry)
				imsg_compose_event(&c->iev, IMSG_CTL_HOST,
				    0, 0, -1, host, sizeof(*host));
	}
relays:
	if (env->sc_relays == NULL)
		goto routers;
	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		rlay->rl_stats[env->sc_prefork_relay].id = EMPTY_ID;
		imsg_compose_event(&c->iev, IMSG_CTL_RELAY, 0, 0, -1,
		    rlay, sizeof(*rlay));
		imsg_compose_event(&c->iev, IMSG_CTL_RELAY_STATS, 0, 0, -1,
		    &rlay->rl_stats, sizeof(rlay->rl_stats));

		if (rlay->rl_dsttable == NULL)
			continue;
		imsg_compose_event(&c->iev, IMSG_CTL_TABLE, 0, 0, -1,
		    rlay->rl_dsttable, sizeof(*rlay->rl_dsttable));
		if (!(rlay->rl_dsttable->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rlay->rl_dsttable->hosts, entry)
				imsg_compose_event(&c->iev, IMSG_CTL_HOST,
				    0, 0, -1, host, sizeof(*host));

		if (rlay->rl_conf.backuptable == EMPTY_TABLE)
			continue;
		imsg_compose_event(&c->iev, IMSG_CTL_TABLE, 0, 0, -1,
		    rlay->rl_backuptable, sizeof(*rlay->rl_backuptable));
		if (!(rlay->rl_backuptable->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rlay->rl_backuptable->hosts, entry)
				imsg_compose_event(&c->iev, IMSG_CTL_HOST,
				    0, 0, -1, host, sizeof(*host));
	}

routers:
	if (env->sc_rts == NULL)
		goto end;
	TAILQ_FOREACH(rt, env->sc_rts, rt_entry) {
		imsg_compose_event(&c->iev, IMSG_CTL_ROUTER, 0, 0, -1,
		    rt, sizeof(*rt));
		if (rt->rt_conf.flags & F_DISABLE)
			continue;

		TAILQ_FOREACH(nr, &rt->rt_netroutes, nr_entry)
			imsg_compose_event(&c->iev, IMSG_CTL_NETROUTE,
			    0, 0, -1, nr, sizeof(*nr));
		imsg_compose_event(&c->iev, IMSG_CTL_TABLE, 0, 0, -1,
		    rt->rt_gwtable, sizeof(*rt->rt_gwtable));
		if (!(rt->rt_gwtable->conf.flags & F_DISABLE))
			TAILQ_FOREACH(host, &rt->rt_gwtable->hosts, entry)
				imsg_compose_event(&c->iev, IMSG_CTL_HOST,
				    0, 0, -1, host, sizeof(*host));
	}

end:
	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

void
show_sessions(struct ctl_conn *c)
{
	int		 n, proc, done;
	struct imsg	 imsg;

	for (proc = 0; proc < env->sc_prefork_relay; proc++) {
		/*
		 * Request all the running sessions from the process
		 */
		imsg_compose_event(&iev_relay[proc],
		    IMSG_CTL_SESSION, 0, 0, -1, NULL, 0);
		while (iev_relay[proc].ibuf.w.queued)
			if (msgbuf_write(&iev_relay[proc].ibuf.w) < 0)
				fatalx("write error");

		/*
		 * Wait for the reply and forward the messages to the
		 * control connection.
		 */
		done = 0;
		while (!done) {
			do {
				if ((n = imsg_read(&iev_relay[proc].ibuf)) == -1)
					fatalx("imsg_read error");
			} while (n == -2); /* handle non-blocking I/O */
			while (!done) {
				if ((n = imsg_get(&iev_relay[proc].ibuf,
				    &imsg)) == -1)
					fatalx("imsg_get error");
				if (n == 0)
					break;
				switch (imsg.hdr.type) {
				case IMSG_CTL_SESSION:
					imsg_compose_event(&c->iev,
					    IMSG_CTL_SESSION, proc, 0, -1,
					    imsg.data, sizeof(struct rsession));
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

	imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
}

int
disable_rdr(struct ctl_conn *c, struct ctl_id *id)
{
	struct rdr	*rdr;

	if (id->id == EMPTY_ID)
		rdr = rdr_findbyname(env, id->name);
	else
		rdr = rdr_find(env, id->id);
	if (rdr == NULL)
		return (-1);
	id->id = rdr->conf.id;

	if (rdr->conf.flags & F_DISABLE)
		return (0);

	rdr->conf.flags |= F_DISABLE;
	rdr->conf.flags &= ~(F_ADD);
	rdr->conf.flags |= F_DEL;
	rdr->table->conf.flags |= F_DISABLE;
	log_debug("disable_rdr: disabled rdr %d", rdr->conf.id);
	pfe_sync();
	return (0);
}

int
enable_rdr(struct ctl_conn *c, struct ctl_id *id)
{
	struct rdr	*rdr;
	struct ctl_id	 eid;

	if (id->id == EMPTY_ID)
		rdr = rdr_findbyname(env, id->name);
	else
		rdr = rdr_find(env, id->id);
	if (rdr == NULL)
		return (-1);
	id->id = rdr->conf.id;

	if (!(rdr->conf.flags & F_DISABLE))
		return (0);

	rdr->conf.flags &= ~(F_DISABLE);
	rdr->conf.flags &= ~(F_DEL);
	rdr->conf.flags |= F_ADD;
	log_debug("enable_rdr: enabled rdr %d", rdr->conf.id);

	bzero(&eid, sizeof(eid));

	/* XXX: we're syncing twice */
	eid.id = rdr->table->conf.id;
	if (enable_table(c, &eid) == -1)
		return (-1);
	if (rdr->backup->conf.id == EMPTY_ID)
		return (0);
	eid.id = rdr->backup->conf.id;
	if (enable_table(c, &eid) == -1)
		return (-1);
	return (0);
}

int
disable_table(struct ctl_conn *c, struct ctl_id *id)
{
	struct table	*table;
	struct host	*host;
	int		 n;

	if (id->id == EMPTY_ID)
		table = table_findbyname(env, id->name);
	else
		table = table_find(env, id->id);
	if (table == NULL)
		return (-1);
	id->id = table->conf.id;
	if (table->conf.rdrid > 0 && rdr_find(env, table->conf.rdrid) == NULL)
		fatalx("disable_table: desynchronised");

	if (table->conf.flags & F_DISABLE)
		return (0);
	table->conf.flags |= (F_DISABLE|F_CHANGED);
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose_event(iev_hce, IMSG_TABLE_DISABLE, 0, 0, -1,
	    &table->conf.id, sizeof(table->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->sc_prefork_relay; n++)
		imsg_compose_event(&iev_relay[n],
		    IMSG_TABLE_DISABLE, 0, 0, -1,
		    &table->conf.id, sizeof(table->conf.id));
	log_debug("disable_table: disabled table %d", table->conf.id);
	pfe_sync();
	return (0);
}

int
enable_table(struct ctl_conn *c, struct ctl_id *id)
{
	struct table	*table;
	struct host	*host;
	int		 n;

	if (id->id == EMPTY_ID)
		table = table_findbyname(env, id->name);
	else
		table = table_find(env, id->id);
	if (table == NULL)
		return (-1);
	id->id = table->conf.id;

	if (table->conf.rdrid > 0 && rdr_find(env, table->conf.rdrid) == NULL)
		fatalx("enable_table: desynchronised");

	if (!(table->conf.flags & F_DISABLE))
		return (0);
	table->conf.flags &= ~(F_DISABLE);
	table->conf.flags |= F_CHANGED;
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose_event(iev_hce, IMSG_TABLE_ENABLE, 0, 0, -1,
	    &table->conf.id, sizeof(table->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->sc_prefork_relay; n++)
		imsg_compose_event(&iev_relay[n],
		    IMSG_TABLE_ENABLE, 0, 0, -1,
		    &table->conf.id, sizeof(table->conf.id));
	log_debug("enable_table: enabled table %d", table->conf.id);
	pfe_sync();
	return (0);
}

int
disable_host(struct ctl_conn *c, struct ctl_id *id, struct host *host)
{
	struct host	*h;
	struct table	*table;
	int		 n;

	if (host == NULL) {
		if (id->id == EMPTY_ID)
			host = host_findbyname(env, id->name);
		else
			host = host_find(env, id->id);
		if (host == NULL || host->conf.parentid)
			return (-1);
	}
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

	imsg_compose_event(iev_hce, IMSG_HOST_DISABLE, 0, 0, -1,
	    &host->conf.id, sizeof(host->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->sc_prefork_relay; n++)
		imsg_compose_event(&iev_relay[n],
		    IMSG_HOST_DISABLE, 0, 0, -1,
		    &host->conf.id, sizeof(host->conf.id));
	log_debug("disable_host: disabled host %d", host->conf.id);

	if (!host->conf.parentid) {
		/* Disable all children */
		SLIST_FOREACH(h, &host->children, child)
			disable_host(c, id, h);
		pfe_sync();
	}
	return (0);
}

int
enable_host(struct ctl_conn *c, struct ctl_id *id, struct host *host)
{
	struct host	*h;
	int		 n;

	if (host == NULL) {
		if (id->id == EMPTY_ID)
			host = host_findbyname(env, id->name);
		else
			host = host_find(env, id->id);
		if (host == NULL || host->conf.parentid)
			return (-1);
	}
	id->id = host->conf.id;

	if (!(host->flags & F_DISABLE))
		return (0);

	host->up = HOST_UNKNOWN;
	host->flags &= ~(F_DISABLE);
	host->flags &= ~(F_DEL);
	host->flags &= ~(F_ADD);

	imsg_compose_event(iev_hce, IMSG_HOST_ENABLE, 0, 0, -1,
	    &host->conf.id, sizeof (host->conf.id));
	/* Forward to relay engine(s) */
	for (n = 0; n < env->sc_prefork_relay; n++)
		imsg_compose_event(&iev_relay[n],
		    IMSG_HOST_ENABLE, 0, 0, -1,
		    &host->conf.id, sizeof(host->conf.id));
	log_debug("enable_host: enabled host %d", host->conf.id);

	if (!host->conf.parentid) {
		/* Enable all children */
		SLIST_FOREACH(h, &host->children, child)
			enable_host(c, id, h);
		pfe_sync();
	}
	return (0);
}

void
pfe_sync(void)
{
	struct rdr		*rdr;
	struct table		*active;
	struct table		*table;
	struct ctl_id		 id;
	struct imsg		 imsg;
	struct ctl_demote	 demote;
	struct router		*rt;

	bzero(&id, sizeof(id));
	bzero(&imsg, sizeof(imsg));
	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		rdr->conf.flags &= ~(F_BACKUP);
		rdr->conf.flags &= ~(F_DOWN);

		if (rdr->conf.flags & F_DISABLE ||
		    (rdr->table->up == 0 && rdr->backup->up == 0)) {
			rdr->conf.flags |= F_DOWN;
			active = NULL;
		} else if (rdr->table->up == 0 && rdr->backup->up > 0) {
			rdr->conf.flags |= F_BACKUP;
			active = rdr->backup;
			active->conf.flags |=
			    rdr->table->conf.flags & F_CHANGED;
			active->conf.flags |=
			    rdr->backup->conf.flags & F_CHANGED;
		} else
			active = rdr->table;

		if (active != NULL && active->conf.flags & F_CHANGED) {
			id.id = active->conf.id;
			imsg.hdr.type = IMSG_CTL_TABLE_CHANGED;
			imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
			imsg.data = &id;
			sync_table(env, rdr, active);
			control_imsg_forward(&imsg);
		}

		if (rdr->conf.flags & F_DOWN) {
			if (rdr->conf.flags & F_ACTIVE_RULESET) {
				flush_table(env, rdr);
				log_debug("pfe_sync: disabling ruleset");
				rdr->conf.flags &= ~(F_ACTIVE_RULESET);
				id.id = rdr->conf.id;
				imsg.hdr.type = IMSG_CTL_PULL_RULESET;
				imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
				imsg.data = &id;
				sync_ruleset(env, rdr, 0);
				control_imsg_forward(&imsg);
			}
		} else if (!(rdr->conf.flags & F_ACTIVE_RULESET)) {
			log_debug("pfe_sync: enabling ruleset");
			rdr->conf.flags |= F_ACTIVE_RULESET;
			id.id = rdr->conf.id;
			imsg.hdr.type = IMSG_CTL_PUSH_RULESET;
			imsg.hdr.len = sizeof(id) + IMSG_HEADER_SIZE;
			imsg.data = &id;
			sync_ruleset(env, rdr, 1);
			control_imsg_forward(&imsg);
		}
	}

	TAILQ_FOREACH(rt, env->sc_rts, rt_entry) {
		rt->rt_conf.flags &= ~(F_BACKUP);
		rt->rt_conf.flags &= ~(F_DOWN);

		if ((rt->rt_gwtable->conf.flags & F_CHANGED))
			sync_routes(env, rt);
	}

	TAILQ_FOREACH(table, env->sc_tables, entry) {
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
		imsg_compose_event(iev_main, IMSG_DEMOTE, 0, 0, -1,
		    &demote, sizeof(demote));
	}
}

void
pfe_statistics(int fd, short events, void *arg)
{
	struct rdr		*rdr;
	struct ctl_stats	*cur;
	struct timeval		 tv, tv_now;
	int			 resethour, resetday;
	u_long			 cnt;

	timerclear(&tv);
	if (gettimeofday(&tv_now, NULL) == -1)
		fatal("pfe_statistics: gettimeofday");

	TAILQ_FOREACH(rdr, env->sc_rdrs, entry) {
		cnt = check_table(env, rdr, rdr->table);
		if (rdr->conf.backup_id != EMPTY_TABLE)
			cnt += check_table(env, rdr, rdr->backup);

		resethour = resetday = 0;

		cur = &rdr->stats;
		cur->last = cnt > cur->cnt ? cnt - cur->cnt : 0;

		cur->cnt = cnt;
		cur->tick++;
		cur->avg = (cur->last + cur->avg) / 2;
		cur->last_hour += cur->last;
		if ((cur->tick % (3600 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_hour = (cur->last_hour + cur->avg_hour) / 2;
			resethour++;
		}
		cur->last_day += cur->last;
		if ((cur->tick % (86400 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_day = (cur->last_day + cur->avg_day) / 2;
			resethour++;
		}
		if (resethour)
			cur->last_hour = 0;
		if (resetday)
			cur->last_day = 0;

		rdr->stats.interval = env->sc_statinterval.tv_sec;
	}

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, pfe_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}
