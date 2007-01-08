/*	$OpenBSD: hoststated.c,v 1.7 2007/01/08 20:46:18 reyk Exp $	*/

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
#include <sys/param.h>
#include <sys/wait.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>

#include "hostated.h"

__dead void	 usage(void);

void		 main_sig_handler(int, short, void *);
void		 main_shutdown(void);
void		 main_dispatch_pfe(int, short, void *);
void		 main_dispatch_hce(int, short, void *);
int		 check_child(pid_t, const char *);

int		 pipe_parent2pfe[2];
int		 pipe_parent2hce[2];
int		 pipe_pfe2hce[2];

struct imsgbuf	*ibuf_pfe;
struct imsgbuf	*ibuf_hce;

pid_t		 pfe_pid = 0;
pid_t		 hce_pid = 0;

void
main_sig_handler(int sig, short event, void *arg)
{
	int		 die = 0;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
	case SIGCHLD:
		if (check_child(pfe_pid, "pf udpate engine")) {
			pfe_pid = 0;
			die  = 1;
		}
		if (check_child(hce_pid, "host check engine")) {
			hce_pid = 0;
			die  = 1;
		}
		if (die)
			main_shutdown();
		break;
	case SIGHUP:
		/* reconfigure */
		break;
	default:
		fatalx("unexpected signal");
	}
}

/* __dead is for lint */
__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "%s [-dnv] [-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 c;
	int		 debug;
	u_int32_t	 opts;
	struct hostated	 env;
	const char	*conffile;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;

	opts = 0;
	debug = 0;
	conffile = CONF_FILE;
	bzero(&env, sizeof (env));

	for (;(c = getopt(argc, argv, "dnf:v")) != -1;) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			opts |= HOSTATED_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			opts |= HOSTATED_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	log_init(debug);

	if (parse_config(&env, conffile, opts))
		exit(1);

	if (env.opts & HOSTATED_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	if (geteuid())
		errx(1, "need root privileges");

	if (getpwnam(HOSTATED_USER) == NULL)
		errx(1, "unknown user %s", HOSTATED_USER);

	if (!debug)
		daemon(1, 0);

	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2pfe) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2hce) == -1)
		fatal("socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_pfe2hce) == -1)
		fatal("socketpair");

	session_socket_blockmode(pipe_parent2pfe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2pfe[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2hce[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2hce[1], BM_NONBLOCK);
	session_socket_blockmode(pipe_pfe2hce[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_pfe2hce[1], BM_NONBLOCK);

	pfe_pid = pfe(&env, pipe_parent2pfe, pipe_parent2hce, pipe_pfe2hce);
	hce_pid = hce(&env, pipe_parent2pfe, pipe_parent2hce, pipe_pfe2hce);

	setproctitle("parent");

	event_init();

	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);

	close(pipe_parent2pfe[1]);
	close(pipe_parent2hce[1]);
	close(pipe_pfe2hce[0]);
	close(pipe_pfe2hce[1]);

	if ((ibuf_pfe = calloc(1, sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_hce = calloc(1, sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);

	imsg_init(ibuf_pfe, pipe_parent2pfe[0], main_dispatch_pfe);
	imsg_init(ibuf_hce, pipe_parent2hce[0], main_dispatch_hce);

	ibuf_pfe->events = EV_READ;
	event_set(&ibuf_pfe->ev, ibuf_pfe->fd, ibuf_pfe->events,
	    ibuf_pfe->handler, ibuf_pfe);
	event_add(&ibuf_pfe->ev, NULL);

	ibuf_hce->events = EV_READ;
	event_set(&ibuf_hce->ev, ibuf_hce->fd, ibuf_hce->events,
	    ibuf_hce->handler, ibuf_hce);
	event_add(&ibuf_hce->ev, NULL);

	event_dispatch();

	return (0);
}

void
main_shutdown(void)
{
	pid_t	pid;

	if (pfe_pid)
		kill(pfe_pid, SIGTERM);
	if (hce_pid)
		kill(hce_pid, SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	control_cleanup();
	log_info("terminating");
	exit(0);
}

int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("check_child: lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("check_child: lost child: %s terminated; "
			    "signal %d", pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

void
imsg_event_add(struct imsgbuf *ibuf)
{
	ibuf->events = EV_READ;
	if (ibuf->w.queued)
		ibuf->events |= EV_WRITE;

	event_del(&ibuf->ev);
	event_set(&ibuf->ev, ibuf->fd, ibuf->events, ibuf->handler, ibuf);
	event_add(&ibuf->ev, NULL);
}

void
main_dispatch_pfe(int fd, short event, void *ptr)
{
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0)
			fatalx("parent: pipe closed");
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
			fatal("main_dispatch_pfe: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("main_dispatch_pfe: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
main_dispatch_hce(int fd, short event, void * ptr)
{
	struct imsgbuf          *ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = ptr;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)     /* connection closed */
			fatalx("parent: pipe closed");
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
			fatal("main_dispatch_hce: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("main_dispatch_hce: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
}

struct host *
host_find(struct hostated *env, objid_t id)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, &env->tables, entry)
		TAILQ_FOREACH(host, &table->hosts, entry)
			if (host->id == id)
				return (host);
	return (NULL);
}

struct table *
table_find(struct hostated *env, objid_t id)
{
	struct table	*table;

	TAILQ_FOREACH(table, &env->tables, entry)
		if (table->id == id)
			return (table);
	return (NULL);
}

struct service *
service_find(struct hostated *env, objid_t id)
{
	struct service	*service;

	TAILQ_FOREACH(service, &env->services, entry)
		if (service->id == id)
			return (service);
	return (NULL);
}

struct host *
host_findbyname(struct hostated *env, const char *name)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, &env->tables, entry)
		TAILQ_FOREACH(host, &table->hosts, entry)
			if (strcmp(host->name, name) == 0)
				return (host);
	return (NULL);
}

struct table *
table_findbyname(struct hostated *env, const char *name)
{
	struct table	*table;

	TAILQ_FOREACH(table, &env->tables, entry)
		if (strcmp(table->name, name) == 0)
			return (table);
	return (NULL);
}

struct service *
service_findbyname(struct hostated *env, const char *name)
{
	struct service	*service;

	TAILQ_FOREACH(service, &env->services, entry)
		if (strcmp(service->name, name) == 0)
			return (service);
	return (NULL);
}
