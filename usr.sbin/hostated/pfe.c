/*	$OpenBSD: pfe.c,v 1.1 2006/12/16 11:45:07 reyk Exp $	*/

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

#include "hostated.h"

void	pfe_sig_handler(int sig, short, void *);
void	pfe_shutdown(void);
void	pfe_dispatch_imsg(int, short, void *);
void	pfe_dispatch_parent(int, short, void *);

void	pfe_sync(void);

static struct hostated	*env = NULL;

struct imsgbuf	*ibuf_main;
struct imsgbuf	*ibuf_hce;

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
pfe(struct hostated *x_env, int pipe_parent2pfe[2], int pipe_parent2hce[2],
	int pipe_pfe2hce[2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("pfe: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	env = x_env;

	if (control_init() == -1)
		fatalx("pfe: control socket setup failed");

	init_filter(env);
	init_tables(env);

	if ((pw = getpwnam(HOSTATED_USER)) == NULL)
		fatal("pfe: getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("pfe: chroot");
	if (chdir("/") == -1)
		fatal("pfe: chdir(\"/\")");

	setproctitle("pf update engine");
	hostated_process = PROC_PFE;

        if (setgroups(1, &pw->pw_gid) ||
            setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
            setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
                fatal("pfe: cannot drop privileges");

	event_init();

	signal_set(&ev_sigint, SIGINT, pfe_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, pfe_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);

	/* setup pipes */
	close(pipe_pfe2hce[0]);
	close(pipe_parent2pfe[0]);
	close(pipe_parent2hce[0]);
	close(pipe_parent2hce[1]);

	if ((ibuf_hce = calloc(1, sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = calloc(1, sizeof(struct imsgbuf))) == NULL)
		fatal("pfe");
	imsg_init(ibuf_hce, pipe_pfe2hce[1], pfe_dispatch_imsg); 
	imsg_init(ibuf_main, pipe_parent2pfe[1], pfe_dispatch_parent); 

	ibuf_hce->events = EV_READ;
	event_set(&ibuf_hce->ev, ibuf_hce->fd, ibuf_hce->events,
		ibuf_hce->handler, ibuf_hce);
	event_add(&ibuf_hce->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
		ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	TAILQ_INIT(&ctl_conns);
	control_listen();

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
		if (n == 0)
			fatalx("pfe_dispatch_imsg: pipe closed");
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

		switch (imsg.hdr.type) {
		case IMSG_HOST_STATUS:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(st))
				fatalx("pfe_dispatch_imsg: invalid request");
			memcpy(&st, imsg.data, sizeof(st));
			if ((host = host_find(env, st.id)) == NULL)
				fatalx("pfe_dispatch_imsg: invalid host id");
			if (host->up == st.up) {
				log_debug("pfe_dispatch_imsg: host %d => %d",
					  host->id, host->up);
				fatalx("pfe_dispatch_imsg: desynchronized");
			}

			if ((table = table_find(env, host->tableid)) == NULL)
				fatalx("pfe_dispatch_imsg: invalid table id");

			log_debug("pfe_dispatch_imsg: state %d for host %u %s",
				  st.up, host->id, host->name);

			if ((st.up == HOST_UNKNOWN && host->up == HOST_DOWN) ||
			    (st.up == HOST_DOWN && host->up == HOST_UNKNOWN)) {
				host->up = st.up;
				break;
			}

			if (st.up == HOST_UP) {
				table->flags |= F_CHANGED;
				table->up++;
				host->flags |= F_ADD;
				host->flags &= ~(F_DEL);
			} else {
				table->up--;
				table->flags |= F_CHANGED;
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

	ibuf = ptr;
	switch (event) {
        case EV_READ:
                if ((n = imsg_read(ibuf)) == -1)
                        fatal("imsg_read error");
                if (n == 0)     /* connection closed */
                        fatalx("pfe_dispatch_parent: pipe closed");
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
                default:
                        log_debug("pfe_dispatch_parent: unexpected imsg %d", 
                                imsg.hdr.type);
                        break;
                }
                imsg_free(&imsg);
        }
}

void
show(struct ctl_conn *c)
{
	struct service	*service;
	struct host	*host;

	TAILQ_FOREACH(service, &env->services, entry) {
		imsg_compose(&c->ibuf, IMSG_CTL_SERVICE, 0, 0,
				service, sizeof(*service));
		if (service->flags & F_DISABLE)
			continue;

		imsg_compose(&c->ibuf, IMSG_CTL_TABLE, 0, 0,
			     service->table, sizeof(*service->table)); 
		if (!(service->table->flags & F_DISABLE))
			TAILQ_FOREACH(host, &service->table->hosts, entry)
				imsg_compose(&c->ibuf, IMSG_CTL_HOST, 0, 0,
					     host, sizeof(*host));

		if (service->backup->id == EMPTY_TABLE)
			continue;
		imsg_compose(&c->ibuf, IMSG_CTL_TABLE, 0, 0,
			     service->backup, sizeof(*service->backup)); 
		if (!(service->backup->flags & F_DISABLE))
			TAILQ_FOREACH(host, &service->backup->hosts, entry)
				imsg_compose(&c->ibuf, IMSG_CTL_HOST, 0, 0,
					     host, sizeof(*host));
	}
	imsg_compose(&c->ibuf, IMSG_CTL_END, 0, 0, NULL, 0);
}


int
disable_service(struct ctl_conn *c, objid_t id)
{
	struct service	*service;

	if ((service = service_find(env, id)) == NULL)
		return (-1);

	if (service->flags & F_DISABLE)
		return (0);

	service->flags |= F_DISABLE;
	service->flags &= ~(F_ADD);
	service->flags |= F_DEL;
	service->table->flags |= F_DISABLE;
	log_debug("disable_service: disabled service %d", service->id);
	pfe_sync();
	return (0);
}

int
enable_service(struct ctl_conn *c, objid_t id)
{
	struct service	*service;

	if ((service = service_find(env, id)) == NULL)
		return (-1);

	if (!(service->flags & F_DISABLE))
		return (0);

	service->flags &= ~(F_DISABLE);
	service->flags &= ~(F_DEL);
	service->flags |= F_ADD;
	log_debug("enable_service: enabled service %d", service->id);

	/* XXX: we're syncing twice */
	if (enable_table(c, service->table->id))
		return (-1);
	if (enable_table(c, service->backup->id))
		return (-1);
	return (0);
}

int
disable_table(struct ctl_conn *c, objid_t id)
{
	struct table	*table;
	struct service	*service;
	struct host	*host;

	if (id == EMPTY_TABLE)
		return (-1);
	if ((table = table_find(env, id)) == NULL)
		return (-1);
	if ((service = service_find(env, table->serviceid)) == NULL)
		fatalx("disable_table: desynchronised");

	if (table->flags & F_DISABLE)
		return (0);
	table->flags |= (F_DISABLE|F_CHANGED);
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose(ibuf_hce, IMSG_TABLE_DISABLE, 0, 0, &id, sizeof(id));
	log_debug("disable_table: disabled table %d", table->id);
	pfe_sync();
	return (0);
}

int
enable_table(struct ctl_conn *c, objid_t id)
{
	struct service	*service;
	struct table	*table;
	struct host	*host;

	if (id == EMPTY_TABLE)
		return (-1);
	if ((table = table_find(env, id)) == NULL)
		return (-1);
	if ((service = service_find(env, table->serviceid)) == NULL)
		fatalx("enable_table: desynchronised");

	if (!(table->flags & F_DISABLE))
		return (0);
	table->flags &= ~(F_DISABLE);
	table->flags |= F_CHANGED;
	table->up = 0;
	TAILQ_FOREACH(host, &table->hosts, entry)
		host->up = HOST_UNKNOWN;
	imsg_compose(ibuf_hce, IMSG_TABLE_ENABLE, 0, 0, &id, sizeof(id));
	log_debug("enable_table: enabled table %d", table->id);
	pfe_sync();
	return (0);
}

int
disable_host(struct ctl_conn *c, objid_t id)
{
	struct host	*host;
	struct table	*table;

	if ((host = host_find(env, id)) == NULL)
		return (-1);

	if (host->flags & F_DISABLE)
		return (0);

	if (host->up == HOST_UP) {
		if ((table = table_find(env, host->tableid)) == NULL)
			fatalx("disable_host: invalid table id");
		table->up--;
		table->flags |= F_CHANGED;
	}

	host->up = HOST_UNKNOWN;
	host->flags |= F_DISABLE;
	host->flags |= F_DEL;
	host->flags &= ~(F_ADD);

	imsg_compose(ibuf_hce, IMSG_HOST_DISABLE, 0, 0, &id, sizeof (id));
	log_debug("disable_host: disabled host %d", host->id);
	pfe_sync();
	return (0);
}

int
enable_host(struct ctl_conn *c, objid_t id)
{
	struct host	*host;

	if ((host = host_find(env, id)) == NULL)
		return (-1);

	if (!(host->flags & F_DISABLE))
		return (0);

	host->up = HOST_UNKNOWN;
	host->flags &= ~(F_DISABLE);
	host->flags &= ~(F_DEL);
	host->flags &= ~(F_ADD);

	imsg_compose(ibuf_hce, IMSG_HOST_ENABLE, 0, 0, &id, sizeof (id));
	log_debug("enable_host: enabled host %d", host->id);
	pfe_sync();
	return (0);
}

void
pfe_sync(void)
{
	struct service	*service;
	struct table	*active;
	int		 backup;

	TAILQ_FOREACH(service, &env->services, entry) {
		backup = (service->flags & F_BACKUP);
		service->flags &= ~(F_BACKUP);
		service->flags &= ~(F_DOWN);

		if (service->flags & F_DISABLE ||
		    (service->table->up == 0 && service->backup->up == 0)) {
			service->flags |= F_DOWN;
			active = NULL;
		} else if (service->table->up == 0 && service->backup->up > 0) {
			service->flags |= F_BACKUP;
			active = service->backup;
			active->flags |= service->table->flags & F_CHANGED;
			active->flags |= service->backup->flags & F_CHANGED;
		} else
			active = service->table;

		if (active != NULL && active->flags & F_CHANGED)
			sync_table(env, service, active);

		service->table->flags &= ~(F_CHANGED);
		service->backup->flags &= ~(F_CHANGED);
		
		if (service->flags & F_DOWN) {
			if (service->flags & F_ACTIVE_RULESET) {
				flush_table(env, service);
				log_debug("pfe_sync: disabling ruleset");
				service->flags &= ~(F_ACTIVE_RULESET);
				sync_ruleset(env, service, 0);
			}
		} else if (!(service->flags & F_ACTIVE_RULESET)) {
			log_debug("pfe_sync: enabling ruleset");
			service->flags |= F_ACTIVE_RULESET;
			sync_ruleset(env, service, 1);
		}
	}
}
