/*	$OpenBSD: snmpd.c,v 1.40 2018/11/05 11:59:05 mestre Exp $	*/

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
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

#include "snmpd.h"
#include "mib.h"

__dead void	 usage(void);

void	 snmpd_shutdown(struct snmpd *);
void	 snmpd_sig_handler(int, short, void *);
int	 snmpd_dispatch_snmpe(int, struct privsep_proc *, struct imsg *);
void	 snmpd_generate_engineid(struct snmpd *);
int	 check_child(pid_t, const char *);

struct snmpd	*snmpd_env;

static struct privsep_proc procs[] = {
	{ "snmpe", PROC_SNMPE, snmpd_dispatch_snmpe, snmpe, snmpe_shutdown },
	{ "traphandler", PROC_TRAP, snmpd_dispatch_traphandler, traphandler,
	    traphandler_shutdown }
};

void
snmpd_sig_handler(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;
	struct snmpd	*env = ps->ps_env;
	int		 die = 0, status, fail, id;
	pid_t		pid;
	char		*cause;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			int len;

			pid = waitpid(WAIT_ANY, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				len = asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					len = asprintf(&cause,
					    "exited abnormally");
				} else
					len = asprintf(&cause, "exited okay");
			} else
				fatalx("unexpected cause of SIGCHLD");

			if (len == -1)
				fatal("asprintf");
			
			for (id = 0; id < PROC_MAX; id++) {
				if (pid == ps->ps_pid[id] &&
				    check_child(ps->ps_pid[id],
				    ps->ps_title[id])) {
					die  = 1;
					if (fail)
						log_warnx("lost child: %s %s",
						    ps->ps_title[id], cause);
					break;
				}
			}
			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			snmpd_shutdown(env);
		break;
	case SIGHUP:
		/* reconfigure */
		break;
	case SIGUSR1:
		/* ignore */
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dNnv] [-D macro=value] "
	    "[-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 c;
	struct snmpd	*env;
	int		 debug = 0, verbose = 0;
	u_int		 flags = 0;
	int		 noaction = 0;
	const char	*conffile = CONF_FILE;
	struct privsep	*ps;
	int		 proc_id = PROC_PARENT, proc_instance = 0;
	int		 argc0 = argc;
	char		**argv0 = argv;
	const char	*errp, *title = NULL;

	smi_init();

	/* log to stderr until daemonized */
	log_init(1, LOG_DAEMON);

	while ((c = getopt(argc, argv, "dD:nNf:I:P:v")) != -1) {
		switch (c) {
		case 'd':
			debug++;
			flags |= SNMPD_F_DEBUG;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			noaction = 1;
			break;
		case 'N':
			flags |= SNMPD_F_NONAMES;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'I':
			proc_instance = strtonum(optarg, 0,
			    PROC_MAX_INSTANCES, &errp);
			if (errp)
				fatalx("invalid process instance");
			break;
		case 'P':
			title = optarg;
			proc_id = proc_getid(procs, nitems(procs), title);
			if (proc_id == PROC_MAX)
				fatalx("invalid process name");
			break;
		case 'v':
			verbose++;
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

	ps = &env->sc_ps;
	ps->ps_env = env;
	snmpd_env = env;
	ps->ps_instance = proc_instance;
	if (title)
		ps->ps_title[proc_id] = title;

	if (noaction) {
		fprintf(stderr, "configuration ok\n");
		exit(0);
	}

	if (geteuid())
		errx(1, "need root privileges");

	if ((ps->ps_pw = getpwnam(SNMPD_USER)) == NULL)
		errx(1, "unknown user %s", SNMPD_USER);

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	gettimeofday(&env->sc_starttime, NULL);
	env->sc_engine_boots = 0;

	pf_init();
	snmpd_generate_engineid(env);

	proc_init(ps, procs, nitems(procs), argc0, argv0, proc_id);
	if (!debug && daemon(0, 0) == -1)
		err(1, "failed to daemonize");

	log_procinit("parent");
	log_info("startup");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, snmpd_sig_handler, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, snmpd_sig_handler, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, snmpd_sig_handler, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, snmpd_sig_handler, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, snmpd_sig_handler, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, snmpd_sig_handler, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	proc_connect(ps);

	if (pledge("stdio dns sendfd proc exec id", NULL) == -1)
		fatal("pledge");

	event_dispatch();

	log_debug("%d parent exiting", getpid());

	return (0);
}

void
snmpd_shutdown(struct snmpd *env)
{
	proc_kill(&env->sc_ps);

	free(env);

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

int
snmpd_dispatch_snmpe(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CTL_RELOAD:
		/* XXX notyet */
	default:
		break;
	}

	return (-1);
}

int
snmpd_socket_af(struct sockaddr_storage *ss, in_port_t port, int ipproto)
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

	if (ipproto == IPPROTO_TCP)
		s = socket(ss->ss_family,
		    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	else
		s = socket(ss->ss_family,
		    SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);

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
tohexstr(u_int8_t *bstr, int len)
{
#define MAXHEXSTRLEN		256
	static char hstr[2 * MAXHEXSTRLEN + 1];
	static const char hex[] = "0123456789abcdef";
	int i;

	if (len > MAXHEXSTRLEN)
		len = MAXHEXSTRLEN;	/* truncate */
	for (i = 0; i < len; i++) {
		hstr[i + i] = hex[bstr[i] >> 4];
		hstr[i + i + 1] = hex[bstr[i] & 0x0f];
	}
	hstr[i + i] = '\0';
	return hstr;
}
