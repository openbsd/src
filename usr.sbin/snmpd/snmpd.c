/*	$OpenBSD: snmpd.c,v 1.16 2013/10/16 16:05:03 blambert Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/tree.h>

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

#include "snmpd.h"
#include "mib.h"

__dead void	 usage(void);

void		 snmpd_sig_handler(int, short, void *);
void		 snmpd_shutdown(struct snmpd *);
void		 snmpd_dispatch_snmpe(int, short, void *);
int		 check_child(pid_t, const char *);
void		 snmpd_generate_engineid(struct snmpd *);

struct snmpd	*snmpd_env;

int		 pipe_parent2snmpe[2];
struct imsgev	*iev_snmpe;
pid_t		 snmpe_pid = 0;

void
snmpd_sig_handler(int sig, short event, void *arg)
{
	struct snmpd	*env = arg;
	int			 die = 0;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		if (check_child(snmpe_pid, "snmp engine")) {
			snmpe_pid = 0;
			die  = 1;
		}
		if (die)
			snmpd_shutdown(env);
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

	fprintf(stderr, "usage: %s [-dNnv] [-D macro=value] "
	    "[-f file] [-r path]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int			 c;
	struct snmpd		*env;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct event		 ev_sigchld;
	struct event		 ev_sighup;
	int			 debug = 0;
	u_int			 flags = 0;
	int			 noaction = 0;
	const char		*conffile = CONF_FILE;

	smi_init();

	log_init(1);	/* log to stderr until daemonized */

	while ((c = getopt(argc, argv, "dD:nNf:r:v")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			noaction++;
			break;
		case 'N':
			flags |= SNMPD_F_NONAMES;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			flags |= SNMPD_F_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	if ((env = parse_config(conffile, flags)) == NULL)
		exit(1);
	snmpd_env = env;

	if (noaction) {
		fprintf(stderr, "configuration ok\n");
		exit(0);
	}

	if (geteuid())
		errx(1, "need root privileges");

	if (getpwnam(SNMPD_USER) == NULL)
		errx(1, "unknown user %s", SNMPD_USER);

	log_init(debug);

	if (!debug) {
		if (daemon(1, 0) == -1)
			err(1, "failed to daemonize");
	}

	gettimeofday(&env->sc_starttime, NULL);
	env->sc_engine_boots = 0;

	log_info("startup");

	pf_init();

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
	    pipe_parent2snmpe) == -1)
		fatal("socketpair");

	session_socket_blockmode(pipe_parent2snmpe[0], BM_NONBLOCK);
	session_socket_blockmode(pipe_parent2snmpe[1], BM_NONBLOCK);

	snmpd_generate_engineid(env);

	snmpe_pid = snmpe(env, pipe_parent2snmpe);
	setproctitle("parent");

	event_init();

	signal_set(&ev_sigint, SIGINT, snmpd_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, snmpd_sig_handler, env);
	signal_set(&ev_sigchld, SIGCHLD, snmpd_sig_handler, env);
	signal_set(&ev_sighup, SIGHUP, snmpd_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_parent2snmpe[1]);

	if ((iev_snmpe = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal(NULL);

	imsg_init(&iev_snmpe->ibuf, pipe_parent2snmpe[0]);
	iev_snmpe->handler = snmpd_dispatch_snmpe;
	iev_snmpe->data = iev_snmpe;

	iev_snmpe->events = EV_READ;
	event_set(&iev_snmpe->ev, iev_snmpe->ibuf.fd, iev_snmpe->events,
	    iev_snmpe->handler, iev_snmpe);
	event_add(&iev_snmpe->ev, NULL);

	event_dispatch();

	return (0);
}

void
snmpd_shutdown(struct snmpd *env)
{
	struct control_sock *rcs;
	pid_t	pid;

	if (snmpe_pid)
		kill(snmpe_pid, SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	control_cleanup(&env->sc_csock);
	while ((rcs = TAILQ_FIRST(&env->sc_rcsocks)) != NULL) {
		TAILQ_REMOVE(&env->sc_rcsocks, rcs, cs_entry);
		control_cleanup(rcs);
		free(rcs);
	}
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
imsg_event_add(struct imsgev *iev)
{
	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

void
snmpd_dispatch_snmpe(int fd, short event, void * ptr)
{
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = ptr;
	ibuf = &iev->ibuf;
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(iev);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("snmpd_dispatch_relay: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("snmpd_dispatch_relay: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

int
snmpd_socket_af(struct sockaddr_storage *ss, in_port_t port)
{
	int	 s;

	switch (ss->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)ss)->sin_port = port;
		((struct sockaddr_in *)ss)->sin_len =
		    sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)ss)->sin6_port = port;
		((struct sockaddr_in6 *)ss)->sin6_len =
		    sizeof(struct sockaddr_in6);
		break;
	default:
		return (-1);
	}

	s = socket(ss->ss_family, SOCK_DGRAM, IPPROTO_UDP);
	return (s);
}

void
snmpd_generate_engineid(struct snmpd *env)
{
	u_int32_t		 oid_enterprise, rnd, tim;

	/* RFC 3411 */
	memset(env->sc_engineid, 0, sizeof(env->sc_engineid));
	oid_enterprise = htonl(OIDVAL_openBSD_eid);
	memcpy(env->sc_engineid, &oid_enterprise, sizeof(oid_enterprise));
	env->sc_engineid[0] |= SNMP_ENGINEID_NEW;
	env->sc_engineid_len = sizeof(oid_enterprise);

	/* XXX alternatively configure engine id via snmpd.conf */
	env->sc_engineid[(env->sc_engineid_len)++] = SNMP_ENGINEID_FMT_EID;
	rnd = arc4random();
	memcpy(&env->sc_engineid[env->sc_engineid_len], &rnd, sizeof(rnd));
	env->sc_engineid_len += sizeof(rnd);

	tim = htonl(env->sc_starttime.tv_sec);
	memcpy(&env->sc_engineid[env->sc_engineid_len], &tim, sizeof(tim));
	env->sc_engineid_len += sizeof(tim);
}

u_long
snmpd_engine_time(void)
{
	struct timeval	 now;

	/*
	 * snmpEngineBoots should be stored in a non-volatile storage.
	 * snmpEngineTime is the number of seconds since snmpEngineBoots
	 * was last incremented. We don't rely on non-volatile storage.
	 * snmpEngineBoots is set to zero and snmpEngineTime to the system
	 * clock. Hence, the tuple (snmpEngineBoots, snmpEngineTime) is
	 * still unique and protects us against replay attacks. It only
	 * 'expires' a little bit sooner than the RFC3414 method.
	 */
	gettimeofday(&now, NULL);
	return now.tv_sec;
}

char *
tohexstr(u_int8_t *str, int len)
{
#define MAXHEXSTRLEN		256
	static char hstr[2 * MAXHEXSTRLEN + 1];
	char *r = hstr;

	if (len > MAXHEXSTRLEN)
		len = MAXHEXSTRLEN;	/* truncate */
	while (len-- > 0)
		r += snprintf(r, len * 2, "%0*x", 2, *str++);
	*r = '\0';
	return hstr;
}
