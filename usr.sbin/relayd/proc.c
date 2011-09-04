/*	$OpenBSD: proc.c,v 1.3 2011/09/04 20:26:58 bluhm Exp $	*/

/*
 * Copyright (c) 2010,2011 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <event.h>

#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "relayd.h"

void	 proc_setup(struct privsep *);
int	 proc_ispeer(struct privsep_proc *, u_int, enum privsep_procid);
void	 proc_shutdown(struct privsep_proc *);
void	 proc_sig_handler(int, short, void *);
void	 proc_range(struct privsep *, enum privsep_procid, int *, int *);

int
proc_ispeer(struct privsep_proc *p, u_int nproc, enum privsep_procid type)
{
	u_int	i;

	for (i = 0; i < nproc; i++)
		if (p[i].p_id == type)
			return (1);
	return (0);
}

void
proc_init(struct privsep *ps, struct privsep_proc *p, u_int nproc)
{
	u_int	 i;

	/*
	 * Called from parent
	 */
	privsep_process = PROC_PARENT;
	ps->ps_instances[PROC_PARENT] = 1;
	ps->ps_title[PROC_PARENT] = "parent";
	ps->ps_pid[PROC_PARENT] = getpid();

	for (i = 0; i < nproc; i++) {
		/* Default to 1 process instance */
		if (ps->ps_instances[p[i].p_id] < 1)
			ps->ps_instances[p[i].p_id] = 1;
		ps->ps_title[p[i].p_id] = p[i].p_title;
	}

	proc_setup(ps);

	/* Engage! */
	for (i = 0; i < nproc; i++)
		ps->ps_pid[p[i].p_id] = (*p[i].p_init)(ps, &p[i]);
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
		killpg(ps->ps_pid[i], SIGTERM);
	}

	do {
		pid = waitpid(WAIT_ANY, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	proc_clear(ps, 1);
}

void
proc_setup(struct privsep *ps)
{
	int	 i, j, n, count, sockpair[2];

	for (i = 0; i < PROC_MAX; i++)
		for (j = 0; j < PROC_MAX; j++) {
			/*
			 * find out how many instances of this peer there are.
			 */
			if (i >= j || ps->ps_instances[i] == 0||
			   ps->ps_instances[j] == 0)
				continue;

			if (ps->ps_instances[i] > 1 &&
			    ps->ps_instances[j] > 1)
				fatalx("N:M peering not supported");

			count = ps->ps_instances[i] * ps->ps_instances[j];

			if ((ps->ps_pipes[i][j] =
			    calloc(count, sizeof(int))) == NULL ||
			    (ps->ps_pipes[j][i] =
			    calloc(count, sizeof(int))) == NULL)
				fatal(NULL);

			for (n = 0; n < count; n++) {
				if (ps->ps_noaction)
					continue;
				if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
				    sockpair) == -1)
					fatal("socketpair");
				ps->ps_pipes[i][j][n] = sockpair[0];
				ps->ps_pipes[j][i][n] = sockpair[1];
				socket_set_blockmode(
				    ps->ps_pipes[i][j][n],
				    BM_NONBLOCK);
				socket_set_blockmode(
				    ps->ps_pipes[j][i][n],
				    BM_NONBLOCK);
			}
		}
}

void
proc_config(struct privsep *ps, struct privsep_proc *p, u_int nproc)
{
	u_int	i, j, src, dst, count, n, instance;

	src = privsep_process;

	/*
	 * close unused pipes
	 */
	for (i = 0; i < PROC_MAX; i++) {
		for (j = 0; j < PROC_MAX; j++) {
			if (i == j ||
			    ps->ps_instances[i] == 0 ||
			    ps->ps_instances[j] == 0)
				continue;

			count = ps->ps_instances[i] * ps->ps_instances[j];

			for (n = 0; n < count; n++) {
				instance = ps->ps_instances[i] > 1 ? n : 0;
				if (i == src &&
				    proc_ispeer(p, nproc, j) &&
				    ps->ps_instance == instance)
					continue;

				if (!ps->ps_noaction)
					close(ps->ps_pipes[i][j][n]);
				ps->ps_pipes[i][j][n] = -1;
			}
		}
	}

	/*
	 * listen on appropriate pipes
	 */
	for (i = 0; i < nproc; i++) {
		dst = p[i].p_id;
		p[i].p_ps = ps;
		p[i].p_env = ps->ps_env;

		if (src == dst)
			fatal("proc_config: cannot peer with oneself");

		count = ps->ps_instances[src] * ps->ps_instances[dst];

		if ((ps->ps_ievs[dst] = calloc(count,
		    sizeof(struct imsgev))) == NULL)
			fatal("proc_config");

		for (n = 0; n < count; n++) {
			if (ps->ps_pipes[src][dst][n] == -1)
				continue;

			imsg_init(&(ps->ps_ievs[dst][n].ibuf),
			    ps->ps_pipes[src][dst][n]);
			ps->ps_ievs[dst][n].handler = proc_dispatch;
			ps->ps_ievs[dst][n].events = EV_READ;
			ps->ps_ievs[dst][n].proc = &p[i];
			ps->ps_ievs[dst][n].data = &ps->ps_ievs[dst][n];
			p[i].p_instance = n;

			event_set(&(ps->ps_ievs[dst][n].ev),
			    ps->ps_ievs[dst][n].ibuf.fd,
			    ps->ps_ievs[dst][n].events,
			    ps->ps_ievs[dst][n].handler,
			    ps->ps_ievs[dst][n].data);
			event_add(&(ps->ps_ievs[dst][n].ev), NULL);
		}
	}
}

void
proc_clear(struct privsep *ps, int purge)
{
	u_int	 src = privsep_process, dst, n, count;

	if (ps == NULL)
		return;

	for (dst = 0; dst < PROC_MAX; dst++) {
		if (src == dst || ps->ps_ievs[dst] == NULL)
			continue;

		count = ps->ps_instances[src] * ps->ps_instances[dst];

		for (n = 0; n < count; n++) {
			if (ps->ps_pipes[src][dst][n] == -1)
				continue;
			if (purge) {
				event_del(&(ps->ps_ievs[dst][n].ev));
				imsg_clear(&(ps->ps_ievs[dst][n].ibuf));
				close(ps->ps_pipes[src][dst][n]);
			} else
				imsg_flush(&(ps->ps_ievs[dst][n].ibuf));
		}
		if (purge)
			free(ps->ps_ievs[dst]);
	}
}

void
proc_shutdown(struct privsep_proc *p)
{
	struct privsep	*ps = p->p_ps;

	if (p->p_id == PROC_CONTROL && ps)
		control_cleanup(&ps->ps_csock);

	if (p->p_shutdown != NULL)
		(*p->p_shutdown)();

	proc_clear(ps, 1);

	log_info("%s exiting, pid %d", p->p_title, getpid());

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
    void (*init)(struct privsep *, struct privsep_proc *, void *), void *arg)
{
	pid_t		 pid;
	struct passwd	*pw;
	const char	*root;
	u_int32_t	 seed[256];
	u_int		 n;

	if (ps->ps_noaction)
		return (0);

	switch (pid = fork()) {
	case -1:
		fatal("run_proc: cannot fork");
	case 0:
		/* Set the process group of the current process */
		setpgrp(0, getpid());
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
		fatal("run_proc: chroot");
	if (chdir("/") == -1)
		fatal("run_proc: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
	if (p->p_chroot != NULL) {
		if (chroot(root) == -1)
			fatal("run_proc: chroot");
		if (chdir("/") == -1)
			fatal("run_proc: chdir(\"/\")");
	}
#endif

	privsep_process = p->p_id;

	setproctitle("%s", p->p_title);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("run_proc: cannot drop privileges");
#endif

	/* Fork child handlers */
	for (n = 1; n < ps->ps_instances[p->p_id]; n++) {
		if (fork() == 0) {
			ps->ps_instance = p->p_instance = n;
			break;
		}
	}

#ifdef DEBUG
	log_debug("%s: %s %d/%d, pid %d", __func__, p->p_title,
	    ps->ps_instance + 1, ps->ps_instances[p->p_id], getpid());
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

	if (p->p_id == PROC_CONTROL && ps->ps_instance == 0) {
		TAILQ_INIT(&ctl_conns);
		if (control_listen(&ps->ps_csock) == -1)
			fatalx(p->p_title);
	}

	if (init != NULL)
		init(ps, p, arg);

	event_dispatch();

	proc_shutdown(p);

	return (0);
}

void
proc_dispatch(int fd, short event, void *arg)
{
	struct imsgev		*iev = (struct imsgev *)arg;
	struct privsep_proc	*p = iev->proc;
	struct privsep		*ps = p->p_ps;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 verbose;
	const char		*title;

	title = ps->ps_title[privsep_process];
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

#if DEBUG > 1
		log_debug("%s: %s %d got imsg %d from %s %d",
		    __func__, title, ps->ps_instance + 1,
		    imsg.hdr.type, p->p_title, p->p_instance);
#endif

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
			log_warnx("%s: %s %d got invalid imsg %d from %s %d",
			    __func__, title, ps->ps_instance + 1,
			    imsg.hdr.type, p->p_title, p->p_instance);
			fatalx(title);
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

/*
 * imsg helper functions
 */

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

void
proc_range(struct privsep *ps, enum privsep_procid id, int *n, int *m)
{
	if (*n == -1) {
		/*
		 * -1 means that the current process is
		 * N:1 - one of many processes connected to a single peer,
		 *       so find the right slot of the peer.
		 * 1:N - a single process connected to many peers,
		 *       so find the range of slots of all peers.
		 */
		if (ps->ps_instances[privsep_process] <=
		    ps->ps_instances[id]) {
			*n = 0;
			*m = ps->ps_instances[id];
			return;
		}

		*n = ps->ps_instance;
		*m = ps->ps_instance + 1;
	} else {
		/* Use only a single slot of the specified peer process */
		*m = *n + 1;
	}
}

int
proc_compose_imsg(struct privsep *ps, enum privsep_procid id, int n,
    u_int16_t type, int fd, void *data, u_int16_t datalen)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	for (; n < m; n++)
		if (imsg_compose_event(&ps->ps_ievs[id][n],
		    type, -1, 0, fd, data, datalen) == -1)
			return (-1);

	return (0);
}

int
proc_composev_imsg(struct privsep *ps, enum privsep_procid id, int n,
    u_int16_t type, int fd, const struct iovec *iov, int iovcnt)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	for (; n < m; n++)
		if (imsg_composev_event(&ps->ps_ievs[id][n],
		    type, -1, 0, fd, iov, iovcnt) == -1)
			return (-1);

	return (0);
}

int
proc_forward_imsg(struct privsep *ps, struct imsg *imsg,
    enum privsep_procid id, int n)
{
	return (proc_compose_imsg(ps, id, n, imsg->hdr.type,
	    imsg->fd, imsg->data, IMSG_DATA_SIZE(imsg)));
}

void
proc_flush_imsg(struct privsep *ps, enum privsep_procid id, int n)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	for (; n < m; n++)
		imsg_flush(&ps->ps_ievs[id][n].ibuf);
}

struct imsgbuf *
proc_ibuf(struct privsep *ps, enum privsep_procid id, int n)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	return (&ps->ps_ievs[id][n].ibuf);
}
