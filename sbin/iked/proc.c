/*	$OpenBSD: proc.c,v 1.3 2011/05/05 12:17:10 reyk Exp $	*/
/*	$vantronix: proc.c,v 1.11 2010/06/01 16:45:56 jsg Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <event.h>

#include <openssl/rand.h>

#include "iked.h"

void	 proc_shutdown(struct privsep_proc *);
void	 proc_sig_handler(int, short, void *);

void
init_procs(struct iked *env, struct privsep_proc *p, u_int nproc)
{
	u_int	 i;

	privsep_process = PROC_PARENT;
	init_pipes(env);

	for (i = 0; i < nproc; i++, p++) {
		env->sc_title[p->id] = p->title;
		env->sc_pid[p->id] = (*p->init)(env, p);
	}
}

void
kill_procs(struct iked *env)
{
	u_int	 i;

	if (privsep_process != PROC_PARENT)
		return;

	for (i = 0; i < PROC_MAX; i++) {
		if (env->sc_pid[i] == 0)
			continue;
		kill(env->sc_pid[i], SIGTERM);
	}
}

void
init_pipes(struct iked *env)
{
	int	 i, j, fds[2];

	for (i = 0; i < PROC_MAX; i++)
		for (j = 0; j < PROC_MAX; j++) {
			if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
			    fds) == -1)
				fatal("socketpair");
			env->sc_pipes[i][j] = fds[0];
			env->sc_pipes[j][i] = fds[1];
			socket_set_blockmode(env->sc_pipes[i][j],
			    BM_NONBLOCK);
			socket_set_blockmode(env->sc_pipes[j][i],
			    BM_NONBLOCK);
		}
}

void
config_pipes(struct iked *env, struct privsep_proc *p, u_int nproc)
{
	u_int	 i, j, k, found;

	for (i = 0; i < PROC_MAX; i++) {
		if (i != privsep_process) {
			for (j = 0; j < PROC_MAX; j++) {
				close(env->sc_pipes[i][j]);
				env->sc_pipes[i][j] = -1;
			}
		} else {
			for (j = found = 0; j < PROC_MAX; j++, found = 0) {
				for (k = 0; k < nproc; k++) {
					if (p[k].id == j)
						found++;
				}
				if (!found) {
					close(env->sc_pipes[i][j]);
					env->sc_pipes[i][j] = -1;
				}
			}
		}
	}
}

void
config_procs(struct iked *env, struct privsep_proc *p, u_int nproc)
{
	u_int	src, dst, i;

	/*
	 * listen on appropriate pipes
	 */
	for (i = 0; i < nproc; i++, p++) {
		src = privsep_process;
		dst = p->id;
		p->env = env;

		imsg_init(&env->sc_ievs[dst].ibuf,
		    env->sc_pipes[src][dst]);
		env->sc_ievs[dst].handler = dispatch_proc;
		env->sc_ievs[dst].events = EV_READ;
		env->sc_ievs[dst].data = p;
		env->sc_ievs[dst].name = p->title;
		event_set(&env->sc_ievs[dst].ev,
		    env->sc_ievs[dst].ibuf.fd,
		    env->sc_ievs[dst].events,
		    env->sc_ievs[dst].handler,
		    env->sc_ievs[dst].data);
		event_add(&env->sc_ievs[dst].ev, NULL);
	}
}

void
proc_shutdown(struct privsep_proc *p)
{
	struct iked	*env = p->env;

	if (p->id == PROC_CONTROL && env)
		control_cleanup(&env->sc_csock);

	log_info("%s exiting", p->title);
	_exit(0);
}

void
proc_sig_handler(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		proc_shutdown((struct privsep_proc *)arg);
		break;
	case SIGCHLD:
	case SIGHUP:
	case SIGPIPE:
		/* ignore */
		break;
	default:
		fatalx("proc_sig_handler: unexpected signal");
		/* NOTREACHED */
	}
}

pid_t
run_proc(struct iked *env, struct privsep_proc *p,
    struct privsep_proc *procs, u_int nproc,
    void (*init)(struct iked *, void *), void *arg)
{
	pid_t		 pid;
	struct passwd	*pw;
	const char	*root;
	u_int32_t	 seed[256];

	switch (pid = fork()) {
	case -1:
		fatal("run_proc: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	pw = env->sc_pw;

	if (p->id == PROC_CONTROL) {
		if (control_init(env, &env->sc_csock) == -1)
			fatalx(p->title);
	}

	/* Change root directory */
	if (p->chroot != NULL)
		root = p->chroot;
	else
		root = pw->pw_dir;

#ifndef DEBUG
	if (chroot(root) == -1)
		fatal("run_proc: chroot");
	if (chdir("/") == -1)
		fatal("run_proc: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
	if (p->chroot != NULL) {
		if (chroot(root) == -1)
			fatal("run_proc: chroot");
		if (chdir("/") == -1)
			fatal("run_proc: chdir(\"/\")");
	}
#endif

	privsep_process = p->id;
	setproctitle("%s", p->title);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("run_proc: cannot drop privileges");
#endif

	event_init();

	signal_set(&env->sc_evsigint, SIGINT, proc_sig_handler, p);
	signal_set(&env->sc_evsigterm, SIGTERM, proc_sig_handler, p);
	signal_set(&env->sc_evsigchld, SIGCHLD, proc_sig_handler, p);
	signal_set(&env->sc_evsighup, SIGHUP, proc_sig_handler, p);
	signal_set(&env->sc_evsigpipe, SIGPIPE, proc_sig_handler, p);

	signal_add(&env->sc_evsigint, NULL);
	signal_add(&env->sc_evsigterm, NULL);
	signal_add(&env->sc_evsigchld, NULL);
	signal_add(&env->sc_evsighup, NULL);
	signal_add(&env->sc_evsigpipe, NULL);

	config_pipes(env, procs, nproc);
	config_procs(env, procs, nproc);

	arc4random_buf(seed, sizeof(seed));
	RAND_seed(seed, sizeof(seed));

	if (p->id == PROC_CONTROL) {
		TAILQ_INIT(&ctl_conns);
		if (control_listen(&env->sc_csock) == -1)
			fatalx(p->title);
	}

	if (init != NULL)
		init(env, arg);

	event_dispatch();

	proc_shutdown(p);

	return (0);
}

void
dispatch_proc(int fd, short event, void *arg)
{
	struct privsep_proc	*p = (struct privsep_proc *)arg;
	struct iked		*env = p->env;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 verbose;

	iev = &env->sc_ievs[p->id];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal(p->title);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal(p->title);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal(p->title);
		if (n == 0)
			break;

		/*
		 * Check the message with the program callback
		 */
		if ((p->cb)(fd, p, &imsg) == 0) {
			/* Message was handled by the callback, continue */
			imsg_free(&imsg);
			continue;
		}

		/*
		 * Generic message handling
		 */
		switch (imsg.hdr.type) {
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &verbose);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			log_warnx("%s: %s got imsg %d", __func__, p->title,
			    imsg.hdr.type);
			fatalx(p->title);
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}
