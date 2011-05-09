/*	$OpenBSD: proc.c,v 1.6 2011/05/09 11:15:18 reyk Exp $	*/
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
#include <sys/wait.h>
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

void	 proc_setup(struct privsep *);
void	 proc_shutdown(struct privsep_proc *);
void	 proc_sig_handler(int, short, void *);

void
proc_init(struct privsep *ps, struct privsep_proc *p, u_int nproc)
{
	u_int	 i;

	/*
	 * Called from parent
	 */
	privsep_process = PROC_PARENT;
	ps->ps_title[PROC_PARENT] = "parent";
	ps->ps_pid[PROC_PARENT] = getpid();

	proc_setup(ps);

	/* Engage! */
	for (i = 0; i < nproc; i++, p++) {
		ps->ps_title[p->p_id] = p->p_title;
		ps->ps_pid[p->p_id] = (*p->p_init)(ps, p);
	}
}

void
proc_kill(struct privsep *ps)
{
	pid_t		 pid;
	u_int		 i;

	if (privsep_process != PROC_PARENT)
		return;

	for (i = 0; i < PROC_MAX; i++) {
		if (ps->ps_pid[i] == 0)
			continue;
		kill(ps->ps_pid[i], SIGTERM);
	}

	do {
		pid = waitpid(WAIT_ANY, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));
}

void
proc_setup(struct privsep *ps)
{
	int	 i, j, sockpair[2];

	for (i = 0; i < PROC_MAX; i++)
		for (j = 0; j < PROC_MAX; j++) {
			if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
			    sockpair) == -1)
				fatal("sockpair");
			ps->ps_pipes[i][j] = sockpair[0];
			ps->ps_pipes[j][i] = sockpair[1];
			socket_set_blockmode(ps->ps_pipes[i][j],
			    BM_NONBLOCK);
			socket_set_blockmode(ps->ps_pipes[j][i],
			    BM_NONBLOCK);
		}
}

void
proc_config(struct privsep *ps, struct privsep_proc *p, u_int nproc)
{
	u_int	 src, dst, i, j, k, found;

	src = privsep_process;

	/*
	 * close unused pipes
	 */
	for (i = 0; i < PROC_MAX; i++) {
		if (i != privsep_process) {
			for (j = 0; j < PROC_MAX; j++) {
				close(ps->ps_pipes[i][j]);
				ps->ps_pipes[i][j] = -1;
			}
		} else {
			for (j = found = 0; j < PROC_MAX; j++, found = 0) {
				for (k = 0; k < nproc; k++) {
					if (p[k].p_id == j)
						found++;
				}
				if (!found) {
					close(ps->ps_pipes[i][j]);
					ps->ps_pipes[i][j] = -1;
				}
			}
		}
	}

	/*
	 * listen on appropriate pipes
	 */
	for (i = 0; i < nproc; i++, p++) {
		dst = p->p_id;
		p->p_ps = ps;
		p->p_env = ps->ps_env;

		imsg_init(&ps->ps_ievs[dst].ibuf,
		    ps->ps_pipes[src][dst]);
		ps->ps_ievs[dst].handler = proc_dispatch;
		ps->ps_ievs[dst].events = EV_READ;
		ps->ps_ievs[dst].data = p;
		ps->ps_ievs[dst].name = p->p_title;
		event_set(&ps->ps_ievs[dst].ev,
		    ps->ps_ievs[dst].ibuf.fd,
		    ps->ps_ievs[dst].events,
		    ps->ps_ievs[dst].handler,
		    ps->ps_ievs[dst].data);
		event_add(&ps->ps_ievs[dst].ev, NULL);
	}
}

void
proc_shutdown(struct privsep_proc *p)
{
	struct privsep	*ps = p->p_ps;

	if (p->p_id == PROC_CONTROL && ps)
		control_cleanup(&ps->ps_csock);

	log_info("%s exiting", p->p_title);
	_exit(0);
}

void
proc_sig_handler(int sig, short event, void *arg)
{
	struct privsep_proc	*p = arg;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		proc_shutdown(p);
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
proc_run(struct privsep *ps, struct privsep_proc *p,
    struct privsep_proc *procs, u_int nproc,
    void (*init)(struct privsep *, void *), void *arg)
{
	pid_t		 pid;
	struct passwd	*pw;
	const char	*root;
	u_int32_t	 seed[256];

	switch (pid = fork()) {
	case -1:
		fatal("proc_run: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	pw = ps->ps_pw;

	if (p->p_id == PROC_CONTROL) {
		if (control_init(ps, &ps->ps_csock) == -1)
			fatalx(p->p_title);
	}

	/* Change root directory */
	if (p->p_chroot != NULL)
		root = p->p_chroot;
	else
		root = pw->pw_dir;

#ifndef DEBUG
	if (chroot(root) == -1)
		fatal("proc_run: chroot");
	if (chdir("/") == -1)
		fatal("proc_run: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
	if (p->p_chroot != NULL) {
		if (chroot(root) == -1)
			fatal("proc_run: chroot");
		if (chdir("/") == -1)
			fatal("proc_run: chdir(\"/\")");
	}
#endif

	privsep_process = p->p_id;

	setproctitle("%s", p->p_title);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("proc_run: cannot drop privileges");
#endif

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, proc_sig_handler, p);
	signal_set(&ps->ps_evsigterm, SIGTERM, proc_sig_handler, p);
	signal_set(&ps->ps_evsigchld, SIGCHLD, proc_sig_handler, p);
	signal_set(&ps->ps_evsighup, SIGHUP, proc_sig_handler, p);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, proc_sig_handler, p);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);

	proc_config(ps, procs, nproc);

	arc4random_buf(seed, sizeof(seed));
	RAND_seed(seed, sizeof(seed));

	if (p->p_id == PROC_CONTROL) {
		TAILQ_INIT(&ctl_conns);
		if (control_listen(&ps->ps_csock) == -1)
			fatalx(p->p_title);
	}

	if (init != NULL)
		init(ps, arg);

	event_dispatch();

	proc_shutdown(p);

	return (0);
}

void
proc_dispatch(int fd, short event, void *arg)
{
	struct privsep_proc	*p = (struct privsep_proc *)arg;
	struct privsep		*ps = p->p_ps;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 verbose;
	const char		*title;

	title = ps->ps_title[privsep_process];
	iev = &ps->ps_ievs[p->p_id];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal(title);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal(title);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal(title);
		if (n == 0)
			break;

		/*
		 * Check the message with the program callback
		 */
		if ((p->p_cb)(fd, p, &imsg) == 0) {
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
			log_warnx("%s: %s got imsg %d", __func__, p->p_title,
			    imsg.hdr.type);
			fatalx(title);
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type, u_int32_t peerid,
    pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) == -1)
		return (ret);
	imsg_event_add(iev);
	return (ret);
}

int
imsg_composev_event(struct imsgev *iev, u_int16_t type, u_int32_t peerid,
    pid_t pid, int fd, const struct iovec *iov, int iovcnt)
{
	int	ret;

	if ((ret = imsg_composev(&iev->ibuf, type, peerid,
	    pid, fd, iov, iovcnt)) == -1)
		return (ret);
	imsg_event_add(iev);
	return (ret);
}

int
proc_compose_imsg(struct iked *env, enum privsep_procid id,
    u_int16_t type, int fd, void *data, u_int16_t datalen)
{
	return (imsg_compose_event(&env->sc_ps.ps_ievs[id],
	    type, -1, 0, fd, data, datalen));
}

int
proc_composev_imsg(struct iked *env, enum privsep_procid id,
    u_int16_t type, int fd, const struct iovec *iov, int iovcnt)
{
	return (imsg_composev_event(&env->sc_ps.ps_ievs[id],
	    type, -1, 0, fd, iov, iovcnt));
}

int
proc_forward_imsg(struct iked *env, struct imsg *imsg,
    enum privsep_procid id)
{
	return (proc_compose_imsg(env, id, imsg->hdr.type,
	    imsg->fd, imsg->data, IMSG_DATA_SIZE(imsg)));
}

void
proc_flush_imsg(struct iked *env, enum privsep_procid id)
{
	imsg_flush(&env->sc_ps.ps_ievs[id].ibuf);
}
