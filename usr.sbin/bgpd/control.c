/*	$OpenBSD: control.c,v 1.69 2010/05/03 13:09:38 claudio Exp $ */

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

#define	CONTROL_BACKLOG	5

struct ctl_conn	*control_connbyfd(int);
struct ctl_conn	*control_connbypid(pid_t);
int		 control_close(int);
void		 control_result(struct ctl_conn *, u_int);

int
control_init(int restricted, char *path)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask, mode;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("control_init: socket");
		return (-1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

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

	session_socket_blockmode(fd, BM_NONBLOCK);

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

void
control_cleanup(const char *path)
{
	if (path)
		unlink(path);
}

unsigned int
control_accept(int listenfd, int restricted)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*ctl_conn;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("control_accept: accept");
		return (0);
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	if ((ctl_conn = malloc(sizeof(struct ctl_conn))) == NULL) {
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

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.fd != fd;
	    c = TAILQ_NEXT(c, entry))
		;	/* nothing */

	return (c);
}

struct ctl_conn *
control_connbypid(pid_t pid)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.pid != pid;
	    c = TAILQ_NEXT(c, entry))
		;	/* nothing */

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

	msgbuf_clear(&c->ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	close(c->ibuf.fd);
	free(c);

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

	if (pfd->revents & POLLOUT)
		if (msgbuf_write(&c->ibuf.w) < 0) {
			*ctl_cnt -= control_close(pfd->fd);
			return (1);
		}

	if (!(pfd->revents & POLLIN))
		return (0);

	if ((n = imsg_read(&c->ibuf)) == -1 || n == 0) {
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
			case IMSG_CTL_SHOW_RIB:
			case IMSG_CTL_SHOW_RIB_AS:
			case IMSG_CTL_SHOW_RIB_PREFIX:
			case IMSG_CTL_SHOW_RIB_MEM:
			case IMSG_CTL_SHOW_RIB_COMMUNITY:
			case IMSG_CTL_SHOW_NETWORK:
			case IMSG_CTL_SHOW_TERSE:
			case IMSG_CTL_SHOW_TIMER:
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
					imsg_compose_rde(imsg.hdr.type,
					    imsg.hdr.pid,
					    p, sizeof(struct peer));
					imsg_compose_rde(IMSG_CTL_END,
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
					imsg_compose_rde(imsg.hdr.type,
					    imsg.hdr.pid,
					    p, sizeof(struct peer));
				imsg_compose_rde(IMSG_CTL_END, imsg.hdr.pid,
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
			imsg_compose_parent(imsg.hdr.type, imsg.hdr.peerid,
			    0, NULL, 0);
			break;
		case IMSG_CTL_NEIGHBOR_UP:
		case IMSG_CTL_NEIGHBOR_DOWN:
		case IMSG_CTL_NEIGHBOR_CLEAR:
		case IMSG_CTL_NEIGHBOR_RREFRESH:
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
					control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_DOWN:
					session_stop(p, ERR_CEASE_ADMIN_DOWN);
					control_result(c, CTL_RES_OK);
					break;
				case IMSG_CTL_NEIGHBOR_CLEAR:
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
			imsg_compose_parent(imsg.hdr.type, 0, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_SHOW_NEXTHOP:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_compose_parent(imsg.hdr.type, imsg.hdr.peerid,
			    imsg.hdr.pid, imsg.data, imsg.hdr.len -
			    IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_AS:
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
				if ((ribreq->flags & F_CTL_ADJ_IN) && p &&
				    !p->conf.softreconfig_in) {
					/*
					 * if no neighbor was specified we
					 * try our best.
					 */
					control_result(c, CTL_RES_NOCAP);
					break;
				}
				if ((imsg.hdr.type == IMSG_CTL_SHOW_RIB_PREFIX)
				    && (ribreq->prefix.aid == AID_UNSPEC)) {
					/* malformed request, must specify af */
					control_result(c, CTL_RES_PARSE_ERROR);
					break;
				}
				c->ibuf.pid = imsg.hdr.pid;
				imsg_compose_rde(imsg.hdr.type, imsg.hdr.pid,
				    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			} else
				log_warnx("got IMSG_CTL_SHOW_RIB with "
				    "wrong length");
			break;
		case IMSG_CTL_SHOW_RIB_MEM:
		case IMSG_CTL_SHOW_RIB_COMMUNITY:
		case IMSG_CTL_SHOW_NETWORK:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_compose_rde(imsg.hdr.type, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_NETWORK_ADD:
		case IMSG_NETWORK_REMOVE:
		case IMSG_NETWORK_FLUSH:
		case IMSG_NETWORK_DONE:
		case IMSG_FILTER_SET:
			imsg_compose_rde(imsg.hdr.type, 0,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(verbose))
				break;

			/* forward to other processes */
			imsg_compose_parent(imsg.hdr.type, 0, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			imsg_compose_rde(imsg.hdr.type, 0,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
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

	return (imsg_compose(&c->ibuf, imsg->hdr.type, 0, imsg->hdr.pid, -1,
	    imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE));
}

void
control_result(struct ctl_conn *c, u_int code)
{
	imsg_compose(&c->ibuf, IMSG_CTL_RESULT, 0, c->ibuf.pid, -1,
	    &code, sizeof(code));
}
