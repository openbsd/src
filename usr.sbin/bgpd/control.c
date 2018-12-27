/*	$OpenBSD: control.c,v 1.93 2018/12/27 20:23:24 remi Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

#define	CONTROL_BACKLOG	5

struct ctl_conn	*control_connbyfd(int);
struct ctl_conn	*control_connbypid(pid_t);
int		 control_close(int);
void		 control_result(struct ctl_conn *, u_int);
ssize_t		 imsg_read_nofd(struct imsgbuf *);

int
control_check(char *path)
{
	struct sockaddr_un	 sun;
	int			 fd;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("%s: socket", __func__);
		return (-1);
	}

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0) {
		log_warnx("control socket %s already in use", path);
		close(fd);
		return (-1);
	}

	close(fd);

	return (0);
}

int
control_init(int restricted, char *path)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask, mode;

	if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
	     0)) == -1) {
		log_warn("control_init: socket");
		return (-1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, path, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path)) {
		log_warn("control_init: socket name too long");
		close(fd);
		return (-1);
	}

	if (unlink(path) == -1)
		if (errno != ENOENT) {
			log_warn("control_init: unlink %s", path);
			close(fd);
			return (-1);
		}

	if (restricted) {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	} else {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
	}

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("control_init: bind: %s", path);
		close(fd);
		umask(old_umask);
		return (-1);
	}

	umask(old_umask);

	if (chmod(path, mode) == -1) {
		log_warn("control_init: chmod: %s", path);
		close(fd);
		unlink(path);
		return (-1);
	}

	return (fd);
}

int
control_listen(int fd)
{
	if (fd != -1 && listen(fd, CONTROL_BACKLOG) == -1) {
		log_warn("control_listen: listen");
		return (-1);
	}

	return (0);
}

void
control_shutdown(int fd)
{
	close(fd);
}

unsigned int
control_accept(int listenfd, int restricted)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*ctl_conn;

	len = sizeof(sun);
	if ((connfd = accept4(listenfd,
	    (struct sockaddr *)&sun, &len,
	    SOCK_NONBLOCK | SOCK_CLOEXEC)) == -1) {
		if (errno == ENFILE || errno == EMFILE) {
			pauseaccept = getmonotime();
			return (0);
		} else if (errno != EWOULDBLOCK && errno != EINTR &&
		    errno != ECONNABORTED)
			log_warn("control_accept: accept");
		return (0);
	}

	if ((ctl_conn = calloc(1, sizeof(struct ctl_conn))) == NULL) {
		log_warn("control_accept");
		close(connfd);
		return (0);
	}

	imsg_init(&ctl_conn->ibuf, connfd);
	ctl_conn->restricted = restricted;

	TAILQ_INSERT_TAIL(&ctl_conns, ctl_conn, entry);

	return (1);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	TAILQ_FOREACH(c, &ctl_conns, entry) {
		if (c->ibuf.fd == fd)
			break;
	}

	return (c);
}

struct ctl_conn *
control_connbypid(pid_t pid)
{
	struct ctl_conn	*c;

	TAILQ_FOREACH(c, &ctl_conns, entry) {
		if (c->ibuf.pid == pid)
			break;
	}

	return (c);
}

int
control_close(int fd)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_close: fd %d: not found", fd);
		return (0);
	}

	if (c->terminate && c->ibuf.pid)
		imsg_ctl_rde(IMSG_CTL_TERMINATE, c->ibuf.pid, NULL, 0);

	msgbuf_clear(&c->ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	close(c->ibuf.fd);
	free(c);
	pauseaccept = 0;
	return (1);
}

int
control_dispatch_msg(struct pollfd *pfd, u_int *ctl_cnt)
{
	struct imsg		 imsg;
	struct ctl_conn		*c;
	ssize_t			 n;
	int			 verbose;
	struct peer		*p;
	struct ctl_neighbor	*neighbor;
	struct ctl_show_rib_request	*ribreq;

	if ((c = control_connbyfd(pfd->fd)) == NULL) {
		log_warn("control_dispatch_msg: fd %d: not found", pfd->fd);
		return (0);
	}

	if (pfd->revents & POLLOUT) {
		if (msgbuf_write(&c->ibuf.w) <= 0 && errno != EAGAIN) {
			*ctl_cnt -= control_close(pfd->fd);
			return (1);
		}
		if (c->throttled && c->ibuf.w.queued < CTL_MSG_LOW_MARK) {
			if (imsg_ctl_rde(IMSG_XON, c->ibuf.pid, NULL, 0) != -1)
				c->throttled = 0;
		}
	}

	if (!(pfd->revents & POLLIN))
		return (0);

	if (((n = imsg_read_nofd(&c->ibuf)) == -1 && errno != EAGAIN) ||
	    n == 0) {
		*ctl_cnt -= control_close(pfd->fd);
		return (1);
	}

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1) {
			*ctl_cnt -= control_close(pfd->fd);
			return (1);
		}

		if (n == 0)
			break;

		if (c->restricted) {
			switch (imsg.hdr.type) {
			case IMSG_CTL_SHOW_NEIGHBOR:
			case IMSG_CTL_SHOW_NEXTHOP:
			case IMSG_CTL_SHOW_INTERFACE:
			case IMSG_CTL_SHOW_RIB_MEM:
			case IMSG_CTL_SHOW_TERSE:
			case IMSG_CTL_SHOW_TIMER:
			case IMSG_CTL_SHOW_NETWORK:
			case IMSG_CTL_SHOW_RIB:
			case IMSG_CTL_SHOW_RIB_PREFIX:
				break;
			default:
				/* clear imsg type to prevent processing */
				imsg.hdr.type = IMSG_NONE;
				control_result(c, CTL_RES_DENIED);
				break;
			}
		}

		switch (imsg.hdr.type) {
		case IMSG_NONE:
			/* message was filtered out, nothing to do */
			break;
		case IMSG_CTL_SHOW_NEIGHBOR:
			c->ibuf.pid = imsg.hdr.pid;
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct ctl_neighbor)) {
				neighbor = imsg.data;
				p = getpeerbyaddr(&neighbor->addr);
				if (p == NULL)
					p = getpeerbydesc(neighbor->descr);
				if (p == NULL) {
					control_result(c, CTL_RES_NOSUCHPEER);
					break;
				}
				if (!neighbor->show_timers) {
					imsg_ctl_rde(imsg.hdr.type,
					    imsg.hdr.pid,
					    p, sizeof(struct peer));
					imsg_ctl_rde(IMSG_CTL_END,
					    imsg.hdr.pid, NULL, 0);
				} else {
					u_int			 i;
					time_t			 d;
					struct ctl_timer	 ct;

					imsg_compose(&c->ibuf,
					    IMSG_CTL_SHOW_NEIGHBOR,
					    0, 0, -1, p, sizeof(*p));
					for (i = 1; i < Timer_Max; i++) {
						if (!timer_running(p, i, &d))
							continue;
						ct.type = i;
						ct.val = d;
						imsg_compose(&c->ibuf,
						    IMSG_CTL_SHOW_TIMER,
						    0, 0, -1, &ct, sizeof(ct));
					}
					imsg_compose(&c->ibuf, IMSG_CTL_END,
					    0, 0, -1, NULL, 0);
				}
			} else {
				for (p = peers; p != NULL; p = p->next)
					imsg_ctl_rde(imsg.hdr.type,
					    imsg.hdr.pid,
					    p, sizeof(struct peer));
				imsg_ctl_rde(IMSG_CTL_END, imsg.hdr.pid,
					NULL, 0);
			}
			break;
		case IMSG_CTL_SHOW_TERSE:
			for (p = peers; p != NULL; p = p->next)
				imsg_compose(&c->ibuf, IMSG_CTL_SHOW_NEIGHBOR,
				    0, 0, -1, p, sizeof(struct peer));
			imsg_compose(&c->ibuf, IMSG_CTL_END, 0, 0, -1, NULL, 0);
			break;
		case IMSG_CTL_FIB_COUPLE:
		case IMSG_CTL_FIB_DECOUPLE:
			imsg_ctl_parent(imsg.hdr.type, imsg.hdr.peerid,
			    0, NULL, 0);
			break;
		case IMSG_CTL_NEIGHBOR_UP:
		case IMSG_CTL_NEIGHBOR_DOWN:
		case IMSG_CTL_NEIGHBOR_CLEAR:
		case IMSG_CTL_NEIGHBOR_RREFRESH:
		case IMSG_CTL_NEIGHBOR_DESTROY:
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct ctl_neighbor)) {
				neighbor = imsg.data;
				neighbor->descr[PEER_DESCR_LEN - 1] = 0;
				p = getpeerbyaddr(&neighbor->addr);
				if (p == NULL)
					p = getpeerbydesc(neighbor->descr);
				if (p == NULL) {
					control_result(c, CTL_RES_NOSUCHPEER);
					break;
				}
				switch (imsg.hdr.type) {
				case IMSG_CTL_NEIGHBOR_UP:
					bgp_fsm(p, EVNT_START);
					p->conf.down = 0;
					p->conf.shutcomm[0] = '\0';
					control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_DOWN:
					p->conf.down = 1;
					strlcpy(p->conf.shutcomm,
					    neighbor->shutcomm,
					    sizeof(neighbor->shutcomm));
					session_stop(p, ERR_CEASE_ADMIN_DOWN);
					control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_CLEAR:
					strlcpy(p->conf.shutcomm,
					    neighbor->shutcomm,
					    sizeof(neighbor->shutcomm));
					if (!p->conf.down) {
						session_stop(p,
						    ERR_CEASE_ADMIN_RESET);
						timer_set(p, Timer_IdleHold,
						    SESSION_CLEAR_DELAY);
					} else {
						session_stop(p,
						    ERR_CEASE_ADMIN_DOWN);
					}
					control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_RREFRESH:
					if (session_neighbor_rrefresh(p))
						control_result(c,
						    CTL_RES_NOCAP);
					else
						control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_DESTROY:
					if (!p->template)
						control_result(c,
						    CTL_RES_BADPEER);
					else if (p->state != STATE_IDLE)
						control_result(c,
						    CTL_RES_BADSTATE);
					else {
						/*
						 * Mark as deleted, will be
						 * collected on next poll loop.
						 */
						p->conf.reconf_action =
						    RECONF_DELETE;
						control_result(c, CTL_RES_OK);
					}
					break;
				default:
					fatal("king bula wants more humppa");
				}
			} else
				log_warnx("got IMSG_CTL_NEIGHBOR_ with "
				    "wrong length");
			break;
		case IMSG_CTL_RELOAD:
		case IMSG_CTL_SHOW_INTERFACE:
		case IMSG_CTL_SHOW_FIB_TABLES:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_ctl_parent(imsg.hdr.type, 0, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_SHOW_NEXTHOP:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_ctl_parent(imsg.hdr.type, imsg.hdr.peerid,
			    imsg.hdr.pid, imsg.data, imsg.hdr.len -
			    IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_PREFIX:
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct ctl_show_rib_request)) {
				ribreq = imsg.data;
				neighbor = &ribreq->neighbor;
				neighbor->descr[PEER_DESCR_LEN - 1] = 0;
				ribreq->peerid = 0;
				p = NULL;
				if (neighbor->addr.aid) {
					p = getpeerbyaddr(&neighbor->addr);
					if (p == NULL) {
						control_result(c,
						    CTL_RES_NOSUCHPEER);
						break;
					}
					ribreq->peerid = p->conf.id;
				} else if (neighbor->descr[0]) {
					p = getpeerbydesc(neighbor->descr);
					if (p == NULL) {
						control_result(c,
						    CTL_RES_NOSUCHPEER);
						break;
					}
					ribreq->peerid = p->conf.id;
				}
				if ((ribreq->flags &
				     (F_CTL_ADJ_OUT | F_CTL_ADJ_IN)) && !p) {
					/*
					 * both in and out tables are only
					 * meaningful if used on a single
					 * peer.
					 */
					control_result(c, CTL_RES_NOSUCHPEER);
					break;
				}
				if ((imsg.hdr.type == IMSG_CTL_SHOW_RIB_PREFIX)
				    && (ribreq->prefix.aid == AID_UNSPEC)) {
					/* malformed request, must specify af */
					control_result(c, CTL_RES_PARSE_ERROR);
					break;
				}
				c->ibuf.pid = imsg.hdr.pid;
				c->terminate = 1;
				imsg_ctl_rde(imsg.hdr.type, imsg.hdr.pid,
				    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			} else
				log_warnx("got IMSG_CTL_SHOW_RIB with "
				    "wrong length");
			break;
		case IMSG_CTL_SHOW_NETWORK:
			c->terminate = 1;
			/* FALLTHROUGH */
		case IMSG_CTL_SHOW_RIB_MEM:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_ctl_rde(imsg.hdr.type, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_NETWORK_ADD:
		case IMSG_NETWORK_ASPATH:
		case IMSG_NETWORK_ATTR:
		case IMSG_NETWORK_REMOVE:
		case IMSG_NETWORK_FLUSH:
		case IMSG_NETWORK_DONE:
		case IMSG_FILTER_SET:
			imsg_ctl_rde(imsg.hdr.type, 0,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(verbose))
				break;

			/* forward to other processes */
			imsg_ctl_parent(imsg.hdr.type, 0, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			imsg_ctl_rde(imsg.hdr.type, 0,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}

	return (0);
}

int
control_imsg_relay(struct imsg *imsg)
{
	struct ctl_conn	*c;

	if ((c = control_connbypid(imsg->hdr.pid)) == NULL)
		return (0);

	/* if command finished no need to send exit message */
	if (imsg->hdr.type == IMSG_CTL_END || imsg->hdr.type == IMSG_CTL_RESULT)
		c->terminate = 0;

	if (!c->throttled && c->ibuf.w.queued > CTL_MSG_HIGH_MARK) {
		if (imsg_ctl_rde(IMSG_XOFF, imsg->hdr.pid, NULL, 0) != -1)
			c->throttled = 1;
	}

	return (imsg_compose(&c->ibuf, imsg->hdr.type, 0, imsg->hdr.pid, -1,
	    imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE));
}

void
control_result(struct ctl_conn *c, u_int code)
{
	imsg_compose(&c->ibuf, IMSG_CTL_RESULT, 0, c->ibuf.pid, -1,
	    &code, sizeof(code));
}

/* This should go into libutil, from smtpd/mproc.c */
ssize_t
imsg_read_nofd(struct imsgbuf *ibuf)
{
	ssize_t	 n;
	char	*buf;
	size_t	 len;

	buf = ibuf->r.buf + ibuf->r.wpos;
	len = sizeof(ibuf->r.buf) - ibuf->r.wpos;

	while ((n = recv(ibuf->fd, buf, len, 0)) == -1) {
		if (errno != EINTR)
			return (n);
	}

	ibuf->r.wpos += n;
	return (n);
}
