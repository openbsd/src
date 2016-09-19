/*	$OpenBSD: proc.c,v 1.4 2016/09/19 09:14:38 rzalamena Exp $	*/

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <event.h>
#include <imsg.h>

#include "proc.h"

void	 proc_exec(struct privsep *, struct privsep_proc *, unsigned int,
	    int, char **);
void	 proc_connectpeer(struct privsep *, enum privsep_procid, int,
	    struct privsep_pipes *);
void	 proc_setup(struct privsep *, struct privsep_proc *, unsigned int);
void	 proc_open(struct privsep *, int, int);
void	 proc_accept(struct privsep *, int, enum privsep_procid,
	    unsigned int);
void	 proc_close(struct privsep *);
int	 proc_ispeer(struct privsep_proc *, unsigned int, enum privsep_procid);
void	 proc_shutdown(struct privsep_proc *);
void	 proc_sig_handler(int, short, void *);
void	 proc_range(struct privsep *, enum privsep_procid, int *, int *);
int	 proc_dispatch_null(int, struct privsep_proc *, struct imsg *);

int
proc_ispeer(struct privsep_proc *procs, unsigned int nproc,
    enum privsep_procid type)
{
	unsigned int	i;

	for (i = 0; i < nproc; i++)
		if (procs[i].p_id == type)
			return (1);
	return (0);
}

enum privsep_procid
proc_getid(struct privsep_proc *procs, unsigned int nproc,
    const char *proc_name)
{
	struct privsep_proc	*p;
	unsigned int		 proc;

	for (proc = 0; proc < nproc; proc++) {
		p = &procs[proc];
		if (strcmp(p->p_title, proc_name))
			continue;

		return (p->p_id);
	}

	return (PROC_MAX);
}

void
proc_exec(struct privsep *ps, struct privsep_proc *procs, unsigned int nproc,
    int argc, char **argv)
{
	unsigned int		 proc, nargc, i, proc_i;
	char			**nargv;
	struct privsep_proc	*p;
	char			 num[32];
	int			 fd;

	/* Prepare the new process argv. */
	nargv = calloc(argc + 5, sizeof(char *));
	if (nargv == NULL)
		fatal("%s: calloc", __func__);

	/* Copy call argument first. */
	nargc = 0;
	nargv[nargc++] = argv[0];

	/* Set process name argument and save the position. */
	nargv[nargc++] = "-P";
	proc_i = nargc;
	nargc++;

	/* Point process instance arg to stack and copy the original args. */
	nargv[nargc++] = "-I";
	nargv[nargc++] = num;
	for (i = 1; i < (unsigned int) argc; i++)
		nargv[nargc++] = argv[i];

	nargv[nargc] = NULL;

	for (proc = 0; proc < nproc; proc++) {
		p = &procs[proc];

		/* Update args with process title. */
		nargv[proc_i] = (char *)(uintptr_t)p->p_title;

		/* Fire children processes. */
		for (i = 0; i < ps->ps_instances[p->p_id]; i++) {
			/* Update the process instance number. */
			snprintf(num, sizeof(num), "%u", i);

			fd = ps->ps_pipes[p->p_id][i].pp_pipes[PROC_PARENT][0];
			ps->ps_pipes[p->p_id][i].pp_pipes[PROC_PARENT][0] = -1;

			switch (fork()) {
			case -1:
				fatal("%s: fork", __func__);
				break;
			case 0:
				/* Prepare parent socket. */
				dup2(fd, PROC_PARENT_SOCK_FILENO);

				execvp(argv[0], nargv);
				fatal("%s: execvp", __func__);
				break;
			default:
				/* Close child end. */
				close(fd);
				break;
			}
		}
	}
	free(nargv);
}

void
proc_connectpeer(struct privsep *ps, enum privsep_procid id, int inst,
    struct privsep_pipes *pp)
{
	unsigned int		 i, j;
	struct privsep_fd	 pf;

	for (i = 0; i < PROC_MAX; i++) {
		/* Parent is already connected with everyone. */
		if (i == PROC_PARENT)
			continue;

		for (j = 0; j < ps->ps_instances[i]; j++) {
			/* Don't send socket to child itself. */
			if (i == (unsigned int)id &&
			    j == (unsigned int)inst)
				continue;
			if (pp->pp_pipes[i][j] == -1)
				continue;

			pf.pf_procid = i;
			pf.pf_instance = j;
			proc_compose_imsg(ps, id, inst, IMSG_CTL_PROCFD,
			    -1, pp->pp_pipes[i][j], &pf, sizeof(pf));
			pp->pp_pipes[i][j] = -1;
		}
	}
}

/* Inter-connect all process except with ourself. */
void
proc_connect(struct privsep *ps)
{
	unsigned int		 src, i, j;
	struct privsep_pipes	*pp;
	struct imsgev		*iev;

	/* Listen on appropriate pipes. */
	src = privsep_process;
	pp = &ps->ps_pipes[src][ps->ps_instance];

	for (i = 0; i < PROC_MAX; i++) {
		/* Don't listen to ourself. */
		if (i == src)
			continue;

		for (j = 0; j < ps->ps_instances[i]; j++) {
			if (pp->pp_pipes[i][j] == -1)
				continue;

			iev = &ps->ps_ievs[i][j];
			imsg_init(&iev->ibuf, pp->pp_pipes[i][j]);
			event_set(&iev->ev, iev->ibuf.fd, iev->events,
			    iev->handler, iev->data);
			event_add(&iev->ev, NULL);
		}
	}

	/* Exchange pipes between process. */
	for (i = 0; i < PROC_MAX; i++) {
		/* Parent is already connected with everyone. */
		if (i == PROC_PARENT)
			continue;

		for (j = 0; j < ps->ps_instances[i]; j++)
			proc_connectpeer(ps, i, j, &ps->ps_pipes[i][j]);
	}
}

void
proc_init(struct privsep *ps, struct privsep_proc *procs, unsigned int nproc,
    int argc, char **argv, enum privsep_procid proc_id)
{
	struct privsep_proc	*p = NULL;
	unsigned int		 proc;
	unsigned int		 src, dst;

	if (proc_id == PROC_PARENT) {
		privsep_process = PROC_PARENT;
		proc_setup(ps, procs, nproc);

		/* Open socketpair()s for everyone. */
		for (src = 0; src < PROC_MAX; src++)
			for (dst = 0; dst < PROC_MAX; dst++)
				proc_open(ps, src, dst);

		/* Engage! */
		proc_exec(ps, procs, nproc, argc, argv);
		return;
	}

	/* Initialize a child */
	for (proc = 0; proc < nproc; proc++) {
		if (procs[proc].p_id != proc_id)
			continue;
		p = &procs[proc];
		break;
	}
	if (p == NULL || p->p_init == NULL)
		fatalx("%s: process %d missing process initialization",
		    __func__, proc_id);

	p->p_init(ps, p);

	fatalx("failed to initiate child process");
}

void
proc_accept(struct privsep *ps, int fd, enum privsep_procid dst,
    unsigned int n)
{
	struct privsep_pipes	*pp = ps->ps_pp;
	struct imsgev		*iev;

	if (ps->ps_ievs[dst] == NULL) {
#if DEBUG > 1
		log_debug("%s: %s src %d %d to dst %d %d not connected",
		    __func__, ps->ps_title[privsep_process],
		    privsep_process, ps->ps_instance + 1,
		    dst, n + 1);
#endif
		close(fd);
		return;
	}

	if (pp->pp_pipes[dst][n] != -1) {
		log_warnx("%s: duplicated descriptor", __func__);
		close(fd);
		return;
	} else
		pp->pp_pipes[dst][n] = fd;

	iev = &ps->ps_ievs[dst][n];
	imsg_init(&iev->ibuf, fd);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

void
proc_setup(struct privsep *ps, struct privsep_proc *procs, unsigned int nproc)
{
	unsigned int		 i, j, src, dst, id;
	struct privsep_pipes	*pp;

	/* Initialize parent title, ps_instances and procs. */
	ps->ps_title[PROC_PARENT] = "parent";

	for (src = 0; src < PROC_MAX; src++)
		/* Default to 1 process instance */
		if (ps->ps_instances[src] < 1)
			ps->ps_instances[src] = 1;

	for (src = 0; src < nproc; src++) {
		procs[src].p_ps = ps;
		if (procs[src].p_cb == NULL)
			procs[src].p_cb = proc_dispatch_null;

		id = procs[src].p_id;
		ps->ps_title[id] = procs[src].p_title;
		if ((ps->ps_ievs[id] = calloc(ps->ps_instances[id],
		    sizeof(struct imsgev))) == NULL)
			fatal(__func__);

		/* With this set up, we are ready to call imsg_init(). */
		for (i = 0; i < ps->ps_instances[id]; i++) {
			ps->ps_ievs[id][i].handler = proc_dispatch;
			ps->ps_ievs[id][i].events = EV_READ;
			ps->ps_ievs[id][i].proc = &procs[src];
			ps->ps_ievs[id][i].data = &ps->ps_ievs[id][i];
		}
	}

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
		if ((ps->ps_pipes[src] = calloc(ps->ps_instances[src],
		    sizeof(struct privsep_pipes))) == NULL)
			fatal("%s: calloc", __func__);

		for (i = 0; i < ps->ps_instances[src]; i++) {
			pp = &ps->ps_pipes[src][i];

			for (dst = 0; dst < PROC_MAX; dst++) {
				/* Allocate maximum fd integers */
				if ((pp->pp_pipes[dst] =
				    calloc(ps->ps_instances[dst],
				    sizeof(int))) == NULL)
					fatal("%s: calloc", __func__);

				/* Mark fd as unused */
				for (j = 0; j < ps->ps_instances[dst]; j++)
					pp->pp_pipes[dst][j] = -1;
			}
		}
	}

	ps->ps_pp = &ps->ps_pipes[privsep_process][ps->ps_instance];
}

void
proc_kill(struct privsep *ps)
{
	char		*cause;
	pid_t		 pid;
	int		 len, status;

	if (privsep_process != PROC_PARENT)
		return;

	proc_close(ps);

	do {
		pid = waitpid(WAIT_ANY, &status, 0);
		if (pid <= 0)
			continue;

		if (WIFSIGNALED(status)) {
			len = asprintf(&cause, "terminated; signal %d",
			    WTERMSIG(status));
		} else if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				len = asprintf(&cause, "exited abnormally");
			else
				len = 0;
		} else
			len = -1;

		if (len == 0) {
			/* child exited OK, don't print a warning message */
		} else if (len != -1) {
			log_warnx("lost child: pid %u %s", pid, cause);
			free(cause);
		} else
			log_warnx("lost child: pid %u", pid);
	} while (pid != -1 || (pid == -1 && errno == EINTR));
}

void
proc_open(struct privsep *ps, int src, int dst)
{
	struct privsep_pipes	*pa, *pb;
	int			 fds[2];
	unsigned int		 i, j;

	for (i = 0; i < ps->ps_instances[src]; i++) {
		for (j = 0; j < ps->ps_instances[dst]; j++) {
			/* Don't create sockets for ourself. */
			if (src == dst && i == j)
				continue;

			pa = &ps->ps_pipes[src][i];
			pb = &ps->ps_pipes[dst][j];
			if (pb->pp_pipes[dst][j] != -1)
				continue;

			if (socketpair(AF_UNIX,
			    SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
			    PF_UNSPEC, fds) == -1)
				fatal(__func__);

			pa->pp_pipes[dst][j] = fds[0];
			pb->pp_pipes[src][i] = fds[1];
		}
	}
}

void
proc_close(struct privsep *ps)
{
	unsigned int		 dst, n;
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

	exit(0);
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

void
proc_run(struct privsep *ps, struct privsep_proc *p,
    struct privsep_proc *procs, unsigned int nproc,
    void (*run)(struct privsep *, struct privsep_proc *, void *), void *arg)
{
	struct passwd		*pw = ps->ps_pw;
	const char		*root;
	struct control_sock	*rcs;

	if (ps->ps_noaction)
		exit(0);

	log_procinit(p->p_title);

	/* Set the process group of the current process */
	setpgid(0, 0);

	if (p->p_id == PROC_CONTROL && ps->ps_instance == 0) {
		if (control_init(ps, &ps->ps_csock) == -1)
			fatalx(__func__);
		TAILQ_FOREACH(rcs, &ps->ps_rcsocks, cs_entry)
			if (control_init(ps, rcs) == -1)
				fatalx(__func__);
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

	proc_setup(ps, procs, nproc);
	proc_accept(ps, PROC_PARENT_SOCK_FILENO, PROC_PARENT, 0);
	if (p->p_id == PROC_CONTROL && ps->ps_instance == 0) {
		TAILQ_INIT(&ctl_conns);
		if (control_listen(&ps->ps_csock) == -1)
			fatalx(__func__);
		TAILQ_FOREACH(rcs, &ps->ps_rcsocks, cs_entry)
			if (control_listen(rcs) == -1)
				fatalx(__func__);
	}

	DPRINTF("%s: %s %d/%d, pid %d", __func__, p->p_title,
	    ps->ps_instance + 1, ps->ps_instances[p->p_id], getpid());

	if (run != NULL)
		run(ps, p, arg);

	event_dispatch();

	proc_shutdown(p);
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
	struct privsep_fd	 pf;

	title = ps->ps_title[privsep_process];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal(__func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			fatal(__func__);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal(__func__);
		if (n == 0)
			break;

#if DEBUG > 1
		log_debug("%s: %s %d got imsg %d peerid %d from %s %d",
		    __func__, title, ps->ps_instance + 1,
		    imsg.hdr.type, imsg.hdr.peerid, p->p_title, imsg.hdr.pid);
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
		case IMSG_CTL_PROCFD:
			IMSG_SIZE_CHECK(&imsg, &pf);
			memcpy(&pf, imsg.data, sizeof(pf));
			proc_accept(ps, imsg.fd, pf.pf_procid,
			    pf.pf_instance);
			break;
		default:
			log_warnx("%s: %s %d got invalid imsg %d peerid %d "
			    "from %s %d",
			    __func__, title, ps->ps_instance + 1,
			    imsg.hdr.type, imsg.hdr.peerid,
			    p->p_title, imsg.hdr.pid);
			fatalx(__func__);
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

int
proc_dispatch_null(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	return (-1);
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
imsg_compose_event(struct imsgev *iev, uint16_t type, uint32_t peerid,
    pid_t pid, int fd, void *data, uint16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) == -1)
		return (ret);
	imsg_event_add(iev);
	return (ret);
}

int
imsg_composev_event(struct imsgev *iev, uint16_t type, uint32_t peerid,
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
    uint16_t type, uint32_t peerid, int fd, void *data, uint16_t datalen)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	for (; n < m; n++) {
		if (imsg_compose_event(&ps->ps_ievs[id][n],
		    type, peerid, ps->ps_instance + 1, fd, data, datalen) == -1)
			return (-1);
	}

	return (0);
}

int
proc_compose(struct privsep *ps, enum privsep_procid id,
    uint16_t type, void *data, uint16_t datalen)
{
	return (proc_compose_imsg(ps, id, -1, type, -1, -1, data, datalen));
}

int
proc_composev_imsg(struct privsep *ps, enum privsep_procid id, int n,
    uint16_t type, uint32_t peerid, int fd, const struct iovec *iov, int iovcnt)
{
	int	 m;

	proc_range(ps, id, &n, &m);
	for (; n < m; n++)
		if (imsg_composev_event(&ps->ps_ievs[id][n],
		    type, peerid, ps->ps_instance + 1, fd, iov, iovcnt) == -1)
			return (-1);

	return (0);
}

int
proc_composev(struct privsep *ps, enum privsep_procid id,
    uint16_t type, const struct iovec *iov, int iovcnt)
{
	return (proc_composev_imsg(ps, id, -1, type, -1, -1, iov, iovcnt));
}

int
proc_forward_imsg(struct privsep *ps, struct imsg *imsg,
    enum privsep_procid id, int n)
{
	return (proc_compose_imsg(ps, id, n, imsg->hdr.type,
	    imsg->hdr.peerid, imsg->fd, imsg->data, IMSG_DATA_SIZE(imsg)));
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
