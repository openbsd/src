/*	$OpenBSD: proc.c,v 1.18 2014/12/16 03:35:49 millert Exp $	*/

/*
 * Copyright (c) 2010 - 2014 Reyk Floeter <reyk@openbsd.org>
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

#include <openssl/ssl.h>

#include "relayd.h"

void	 proc_open(struct privsep *, struct privsep_proc *,
	    struct privsep_proc *, size_t);
void	 proc_close(struct privsep *);
int	 proc_ispeer(struct privsep_proc *, u_int, enum privsep_procid);
void	 proc_shutdown(struct privsep_proc *);
void	 proc_sig_handler(int, short, void *);
void	 proc_range(struct privsep *, enum privsep_procid, int *, int *);

int
proc_ispeer(struct privsep_proc *procs, u_int nproc, enum privsep_procid type)
{
	u_int	i;

	for (i = 0; i < nproc; i++)
		if (procs[i].p_id == type)
			return (1);
	return (0);
}

void
proc_init(struct privsep *ps, struct privsep_proc *procs, u_int nproc)
{
	u_int			 i, j, src, dst;
	struct privsep_pipes	*pp;

	/*
	 * Allocate pipes for all process instances (incl. parent)
	 *
	 * - ps->ps_pipes: N:M mapping
	 * N source processes connected to M destination processes:
	 * [src][instances][dst][instances], for example
	 * [PROC_RELAY][3][PROC_CA][3]
	 *
	 * - ps->ps_pp: per-process 1:M part of ps->ps_pipes
	 * Each process instance has a destination array of socketpair fds:
	 * [dst][instances], for example
	 * [PROC_PARENT][0]
	 */
	for (src = 0; src < PROC_MAX; src++) {
		/* Allocate destination array for each process */
		if ((ps->ps_pipes[src] = calloc(ps->ps_ninstances,
		    sizeof(struct privsep_pipes))) == NULL)
			fatal("proc_init: calloc");

		for (i = 0; i < ps->ps_ninstances; i++) {
			pp = &ps->ps_pipes[src][i];

			for (dst = 0; dst < PROC_MAX; dst++) {
				/* Allocate maximum fd integers */
				if ((pp->pp_pipes[dst] =
				    calloc(ps->ps_ninstances,
				    sizeof(int))) == NULL)
					fatal("proc_init: calloc");

				/* Mark fd as unused */
				for (j = 0; j < ps->ps_ninstances; j++)
					pp->pp_pipes[dst][j] = -1;
			}
		}
	}

	/*
	 * Setup and run the parent and its children
	 */
	privsep_process = PROC_PARENT;
	ps->ps_instances[PROC_PARENT] = 1;
	ps->ps_title[PROC_PARENT] = "parent";
	ps->ps_pid[PROC_PARENT] = getpid();
	ps->ps_pp = &ps->ps_pipes[privsep_process][0];

	for (i = 0; i < nproc; i++) {
		/* Default to 1 process instance */
		if (ps->ps_instances[procs[i].p_id] < 1)
			ps->ps_instances[procs[i].p_id] = 1;
		ps->ps_title[procs[i].p_id] = procs[i].p_title;
	}

	proc_open(ps, NULL, procs, nproc);

	/* Engage! */
	for (i = 0; i < nproc; i++)
		ps->ps_pid[procs[i].p_id] = (*procs[i].p_init)(ps, &procs[i]);
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

	proc_close(ps);
}

void
proc_open(struct privsep *ps, struct privsep_proc *p,
    struct privsep_proc *procs, size_t nproc)
{
	struct privsep_pipes	*pa, *pb;
	int			 fds[2];
	u_int			 i, j, src, proc;

	if (p == NULL)
		src = privsep_process; /* parent */
	else
		src = p->p_id;

	/*
	 * Open socket pairs for our peers
	 */
	for (proc = 0; proc < nproc; proc++) {
		procs[proc].p_ps = ps;
		procs[proc].p_env = ps->ps_env;

		for (i = 0; i < ps->ps_instances[src]; i++) {
			for (j = 0; j < ps->ps_instances[procs[proc].p_id];
			    j++) {
				pa = &ps->ps_pipes[src][i];
				pb = &ps->ps_pipes[procs[proc].p_id][j];

				/* Check if fds are already set by peer */
				if (pa->pp_pipes[procs[proc].p_id][j] != -1)
					continue;

				if (socketpair(AF_UNIX, SOCK_STREAM,
				    PF_UNSPEC, fds) == -1)
					fatal("socketpair");

				socket_set_blockmode(fds[0], BM_NONBLOCK);
				socket_set_blockmode(fds[1], BM_NONBLOCK);

				pa->pp_pipes[procs[proc].p_id][j] = fds[0];
				pb->pp_pipes[src][i] = fds[1];
			}
		}
	}
}

void
proc_listen(struct privsep *ps, struct privsep_proc *procs, size_t nproc)
{
	u_int			 i, dst, src, n, m;
	struct privsep_pipes	*pp;

	/*
	 * Close unused pipes
	 */
	for (src = 0; src < PROC_MAX; src++) {
		for (n = 0; n < ps->ps_instances[src]; n++) {
			/* Ingore current process */
			if (src == (u_int)privsep_process &&
			    n == ps->ps_instance)
				continue;

			pp = &ps->ps_pipes[src][n];

			for (dst = 0; dst < PROC_MAX; dst++) {
				if (src == dst)
					continue;
				for (m = 0; m < ps->ps_instances[dst]; m++) {
					if (pp->pp_pipes[dst][m] == -1)
						continue;

					/* Close and invalidate fd */
					close(pp->pp_pipes[dst][m]);
					pp->pp_pipes[dst][m] = -1;
				}
			}
		}
	}

	src = privsep_process;
	ps->ps_pp = pp = &ps->ps_pipes[src][ps->ps_instance];

	/*
	 * Listen on appropriate pipes
	 */
	for (i = 0; i < nproc; i++) {
		dst = procs[i].p_id;

		if (src == dst)
			fatal("proc_listen: cannot peer with oneself");

		if ((ps->ps_ievs[dst] = calloc(ps->ps_instances[dst],
		    sizeof(struct imsgev))) == NULL)
			fatal("proc_open");

		for (n = 0; n < ps->ps_instances[dst]; n++) {
			if (pp->pp_pipes[dst][n] == -1)
				continue;

			imsg_init(&(ps->ps_ievs[dst][n].ibuf),
			    pp->pp_pipes[dst][n]);
			ps->ps_ievs[dst][n].handler = proc_dispatch;
			ps->ps_ievs[dst][n].events = EV_READ;
			ps->ps_ievs[dst][n].proc = &procs[i];
			ps->ps_ievs[dst][n].data = &ps->ps_ievs[dst][n];
			procs[i].p_instance = n;

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
proc_close(struct privsep *ps)
{
	u_int			 dst, n;
	struct privsep_pipes	*pp;

	if (ps == NULL)
		return;

	pp = ps->ps_pp;

	for (dst = 0; dst < PROC_MAX; dst++) {
		if (ps->ps_ievs[dst] == NULL)
			continue;

		for (n = 0; n < ps->ps_instances[dst]; n++) {
			if (pp->pp_pipes[dst][n] == -1)
				continue;

			/* Cancel the fd, close and invalidate the fd */
			event_del(&(ps->ps_ievs[dst][n].ev));
			imsg_clear(&(ps->ps_ievs[dst][n].ibuf));
			close(pp->pp_pipes[dst][n]);
			pp->pp_pipes[dst][n] = -1;
		}
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

	proc_close(ps);

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
	case SIGUSR1:
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
	pid_t			 pid;
	struct passwd		*pw;
	const char		*root;
	struct control_sock	*rcs;
	u_int			 n;

	if (ps->ps_noaction)
		return (0);

	proc_open(ps, p, procs, nproc);

	/* Fork child handlers */
	switch (pid = fork()) {
	case -1:
		fatal("proc_run: cannot fork");
	case 0:
		/* Set the process group of the current process */
		setpgid(0, 0);
		break;
	default:
		return (pid);
	}

	pw = ps->ps_pw;

	if (p->p_id == PROC_CONTROL && ps->ps_instance == 0) {
		if (control_init(ps, &ps->ps_csock) == -1)
			fatalx(p->p_title);
		TAILQ_FOREACH(rcs, &ps->ps_rcsocks, cs_entry)
			if (control_init(ps, rcs) == -1)
				fatalx(p->p_title);
	}

	/* Change root directory */
	if (p->p_chroot != NULL)
		root = p->p_chroot;
	else
		root = pw->pw_dir;

	if (chroot(root) == -1)
		fatal("proc_run: chroot");
	if (chdir("/") == -1)
		fatal("proc_run: chdir(\"/\")");

	privsep_process = p->p_id;

	setproctitle("%s", p->p_title);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("proc_run: cannot drop privileges");

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
	signal_set(&ps->ps_evsigusr1, SIGUSR1, proc_sig_handler, p);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsigchld, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	proc_listen(ps, procs, nproc);

	if (p->p_id == PROC_CONTROL && ps->ps_instance == 0) {
		TAILQ_INIT(&ctl_conns);
		if (control_listen(&ps->ps_csock) == -1)
			fatalx(p->p_title);
		TAILQ_FOREACH(rcs, &ps->ps_rcsocks, cs_entry)
			if (control_listen(rcs) == -1)
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
	struct imsgev		*iev = arg;
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
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
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
		/* Use a range of all target instances */
		*n = 0;
		*m = ps->ps_instances[id];
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
	for (; n < m; n++) {
		if (imsg_compose_event(&ps->ps_ievs[id][n],
		    type, -1, 0, fd, data, datalen) == -1)
			return (-1);
	}

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

struct imsgbuf *
proc_ibuf(struct privsep *ps, enum privsep_procid id, int n)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	return (&ps->ps_ievs[id][n].ibuf);
}

struct imsgev *
proc_iev(struct privsep *ps, enum privsep_procid id, int n)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	return (&ps->ps_ievs[id][n]);
}
