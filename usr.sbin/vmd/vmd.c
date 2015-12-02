/*	$OpenBSD: vmd.c,v 1.9 2015/12/02 09:14:25 reyk Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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
#include <sys/wait.h>
#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "proc.h"
#include "vmd.h"

__dead void usage(void);

int	 main(int, char **);
int	 vmd_configure(void);
void	 vmd_sighdlr(int sig, short event, void *arg);
void	 vmd_shutdown(void);
int	 vmd_control_run(void);

struct vmd	*env;

static struct privsep_proc procs[] = {
	{ "control",	PROC_CONTROL,	vmm_dispatch_control, control },
};

void
vmd_sighdlr(int sig, short event, void *arg)
{
	struct privsep	*ps = arg;
	int		 die = 0, status, fail, id;
	pid_t		 pid;
	char		*cause;
	const char	*title = "vm";

	switch (sig) {
	case SIGHUP:
		log_info("%s: ignoring SIGHUP", __func__);
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			int len;

			pid = waitpid(-1, &status, WNOHANG);
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
				if (pid == ps->ps_pid[id]) {
					die = 1;
					title = ps->ps_title[id];
					break;
				}
			}
			if (fail)
				log_warnx("lost child: %s %s", title, cause);

			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			vmd_shutdown();
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-dv]\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct privsep	*ps;
	int		 ch;

	if ((env = calloc(1, sizeof(*env))) == NULL)
		fatal("calloc: env");

	while ((ch = getopt(argc, argv, "dvn")) != -1) {
		switch (ch) {
		case 'd':
			env->vmd_debug = 2;
			break;
		case 'v':
			env->vmd_verbose++;
			break;
		case 'n':
			env->vmd_noaction = 1;
			break;
		default:
			usage();
		}
	}

	/* check for root privileges */
	if (geteuid())
		fatalx("need root privileges");

	SLIST_INIT(&env->vmd_vmstate);

	ps = &env->vmd_ps;
	ps->ps_env = env;
	TAILQ_INIT(&ps->ps_rcsocks);

	if ((ps->ps_pw =  getpwnam(VMD_USER)) == NULL)
		fatal("unknown user %s", VMD_USER);

	/* Configure the control socket */
	ps->ps_csock.cs_name = SOCKET_NAME;

	/* Open /dev/vmm */
	env->vmd_fd = open(VMM_NODE, O_RDWR);
	if (env->vmd_fd == -1)
		fatal("can't open vmm device node %s", VMM_NODE);

	/* log to stderr until daemonized */
	log_init(env->vmd_debug ? env->vmd_debug : 1, LOG_DAEMON);

	if (!env->vmd_debug && daemon(0, 0) == -1)
		fatal("can't daemonize");

	ps->ps_ninstances = 1;
	proc_init(ps, procs, nitems(procs));

	setproctitle("parent");
	log_procinit("parent");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigchld, SIGCHLD, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, vmd_sighdlr, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	proc_listen(ps, procs, nitems(procs));

	if (vmd_configure() == -1)
		fatalx("configuration failed");

	event_dispatch();

	log_debug("parent exiting");

	return (0);
}

int
vmd_configure(void)
{
#if 0
	if (parse_config(env->sc_conffile, env) == -1) {
		proc_kill(&env->sc_ps);
		exit(1);
	}
#endif

	if (env->vmd_noaction) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&env->vmd_ps);
		exit(0);
	}

	return (0);
}

void
vmd_shutdown(void)
{
	proc_kill(&env->vmd_ps);
	free(env);

	log_warnx("parent terminating");
	exit(0);
}
