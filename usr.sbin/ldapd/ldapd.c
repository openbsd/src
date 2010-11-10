/*	$OpenBSD: ldapd.c,v 1.8 2010/11/10 08:00:54 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
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
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <bsd_auth.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <login_cap.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ldapd.h"

void		 usage(void);
void		 ldapd_sig_handler(int fd, short why, void *data);
void		 ldapd_sigchld_handler(int sig, short why, void *data);
static void	 ldapd_imsgev(struct imsgev *iev, int code, struct imsg *imsg);
static void	 ldapd_auth_request(struct imsgev *iev, struct imsg *imsg);
static void	 ldapd_open_request(struct imsgev *iev, struct imsg *imsg);
static void	 ldapd_log_verbose(struct imsg *imsg);

struct ldapd_stats	 stats;
pid_t			 ldape_pid;

void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file] [-s file]\n", __progname);
	exit(1);
}

void
ldapd_sig_handler(int sig, short why, void *data)
{
	log_info("ldapd: got signal %d", sig);
	if (sig == SIGINT || sig == SIGTERM)
		event_loopexit(NULL);
}

void
ldapd_sigchld_handler(int sig, short why, void *data)
{
	pid_t		 pid;
	int		 status;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) != 0) {
		if (pid == -1) {
			if (errno == EINTR)
				continue;
			if (errno != ECHILD)
				log_warn("waitpid");
			break;
		}

		if (WIFEXITED(status))
			log_debug("child %d exited with status %d",
			    pid, WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			log_debug("child %d exited due to signal %d",
			    pid, WTERMSIG(status));
		else
			log_debug("child %d terminated abnormally", pid);

		if (pid == ldape_pid) {
			log_info("ldapd: lost ldap server");
			event_loopexit(NULL);
			break;
		}
	}
}

/* set socket non-blocking */
void
fd_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	int rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (rc == -1) {
		log_warn("failed to set fd %i non-blocking", fd);
	}
}


int
main(int argc, char *argv[])
{
	int			 c;
	int			 debug = 0, verbose = 0;
	int			 configtest = 0, skip_chroot = 0;
	int			 pipe_parent2ldap[2];
	char			*conffile = CONFFILE;
	char			*csockpath = LDAPD_SOCKET;
	struct passwd		*pw = NULL;
	struct imsgev		*iev_ldape;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct event		 ev_sigchld;
	struct event		 ev_sighup;

	log_init(1);		/* log to stderr until daemonized */

	while ((c = getopt(argc, argv, "dhvD:f:ns:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0) {
				warnx("could not parse macro definition %s",
				    optarg);
			}
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'h':
			usage();
			/* NOTREACHED */
		case 'n':
			configtest = 1;
			break;
		case 's':
			csockpath = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	if (argc > 0)
		usage();

	log_verbose(verbose);
	stats.started_at = time(0);
	ssl_init();

	if (parse_config(conffile) != 0)
		exit(2);

	if (configtest) {
		fprintf(stderr, "configuration ok\n");
		exit(0);
	}

	if (geteuid()) {
		if (!debug)
			errx(1, "need root privileges");
		skip_chroot = 1;
	}

	if (!skip_chroot && (pw = getpwnam(LDAPD_USER)) == NULL)
		err(1, "%s", LDAPD_USER);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	log_init(debug);
	log_info("startup");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_parent2ldap) != 0)
		fatal("socketpair");

	fd_nonblock(pipe_parent2ldap[0]);
	fd_nonblock(pipe_parent2ldap[1]);

	ldape_pid = ldape(pw, csockpath, pipe_parent2ldap);

	setproctitle("auth");
	event_init();

	signal_set(&ev_sigint, SIGINT, ldapd_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, ldapd_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, ldapd_sigchld_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, ldapd_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_parent2ldap[1]);

	if ((iev_ldape = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("calloc");
	imsgev_init(iev_ldape, pipe_parent2ldap[0], NULL, ldapd_imsgev);

	event_dispatch();
	log_debug("ldapd: exiting");

	return 0;
}

static void
ldapd_imsgev(struct imsgev *iev, int code, struct imsg *imsg)
{
	switch (code) {
	case IMSGEV_IMSG:
		log_debug("%s: got imsg %i on fd %i",
		    __func__, imsg->hdr.type, iev->ibuf.fd);
		switch (imsg->hdr.type) {
		case IMSG_LDAPD_AUTH:
			ldapd_auth_request(iev, imsg);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			ldapd_log_verbose(imsg);
			break;
		case IMSG_LDAPD_OPEN:
			ldapd_open_request(iev, imsg);
			break;
		default:
			log_debug("%s: unexpected imsg %d",
			    __func__, imsg->hdr.type);
			break;
		}
		break;
	case IMSGEV_EREAD:
	case IMSGEV_EWRITE:
	case IMSGEV_EIMSG:
		fatal("imsgev read/write error");
		break;
	case IMSGEV_DONE:
		event_loopexit(NULL);
		break;
	}
}

static int
ldapd_auth_classful(char *name, char *password)
{
	login_cap_t		*lc = NULL;
	char			*class = NULL, *style = NULL;
	auth_session_t		*as;

	if ((class = strchr(name, '#')) == NULL) {
		log_debug("regular auth");
		return auth_userokay(name, NULL, "auth-ldap", password);
	}
	*class++ = '\0';

	if ((lc = login_getclass(class)) == NULL) {
		log_debug("login_getclass(%s) for [%s] failed", class, name);
		return 0;
	}
	if ((style = login_getstyle(lc, style, "auth-ldap")) == NULL) {
		log_debug("login_getstyle() for [%s] failed", name);
		login_close(lc);
		return 0;
	}
	if (password) {
		if ((as = auth_open()) == NULL) {
			login_close(lc);
			return 0;
		}
		auth_setitem(as, AUTHV_SERVICE, "response");
		auth_setdata(as, "", 1);
		auth_setdata(as, password, strlen(password) + 1);
		memset(password, 0, strlen(password));
	} else
		as = NULL;

	as = auth_verify(as, style, name, lc->lc_class, (char *)NULL);
	login_close(lc);
	return (as != NULL ? auth_close(as) : 0);
}

static void
ldapd_auth_request(struct imsgev *iev, struct imsg *imsg)
{
	struct auth_req		*areq = imsg->data;
	struct auth_res		 ares;

	if (imsg->hdr.len != sizeof(*areq) + IMSG_HEADER_SIZE)
		fatal("invalid size of auth request");

	/* make sure name and password are null-terminated */
	areq->name[sizeof(areq->name) - 1] = '\0';
	areq->password[sizeof(areq->password) - 1] = '\0';

	log_debug("authenticating [%s]", areq->name);
	ares.ok = ldapd_auth_classful(areq->name, areq->password);
	ares.fd = areq->fd;
	ares.msgid = areq->msgid;
	bzero(areq, sizeof(*areq));
	imsgev_compose(iev, IMSG_LDAPD_AUTH_RESULT, 0, 0, -1, &ares,
	    sizeof(ares));
}

static void
ldapd_log_verbose(struct imsg *imsg)
{
	int	 verbose;

	if (imsg->hdr.len != sizeof(verbose) + IMSG_HEADER_SIZE)
		fatal("invalid size of log verbose request");

	bcopy(imsg->data, &verbose, sizeof(verbose));
	log_verbose(verbose);
}

static void
ldapd_open_request(struct imsgev *iev, struct imsg *imsg)
{
	struct open_req		*oreq = imsg->data;
	int			 oflags, fd;

	if (imsg->hdr.len != sizeof(*oreq) + IMSG_HEADER_SIZE)
		fatal("invalid size of open request");

	/* make sure path is null-terminated */
	oreq->path[MAXPATHLEN] = '\0';

	if (strncmp(oreq->path, DATADIR, strlen(DATADIR)) != 0) {
		log_warnx("refusing to open file %s", oreq->path);
		fatal("ldape sent invalid open request");
	}

	if (oreq->rdonly)
		oflags = O_RDONLY;
	else
		oflags = O_RDWR | O_CREAT | O_APPEND;

	log_debug("opening [%s]", oreq->path);
	fd = open(oreq->path, oflags | O_NOFOLLOW, 0600);
	if (fd == -1)
		log_warn("%s", oreq->path);

	imsgev_compose(iev, IMSG_LDAPD_OPEN_RESULT, 0, 0, fd, oreq,
	    sizeof(*oreq));
}

