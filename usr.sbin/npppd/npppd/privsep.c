/* $OpenBSD: privsep.c,v 1.3 2011/07/08 06:14:54 yasuoka Exp $ */

/*
 * Copyright (c) 2010 Yasuoka Masahiko <yasuoka@openbsd.org>
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
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <event.h>

#include "pathnames.h"
#include "privsep.h"

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

enum PRIVSEP_CMD {
	PRIVSEP_OPEN,
	PRIVSEP_SOCKET,
	PRIVSEP_BIND,
	PRIVSEP_SENDTO,
	PRIVSEP_UNLINK
};

struct PRIVSEP_OPEN_ARG {
	enum PRIVSEP_CMD cmd;
	char path[PATH_MAX];
	int flags;
	mode_t mode;
};

struct PRIVSEP_SOCKET_ARG {
	enum PRIVSEP_CMD cmd;
	int domain;
	int type;
	int protocol;
};

struct PRIVSEP_BIND_ARG {
	enum PRIVSEP_CMD cmd;
	struct sockaddr_storage name;
	socklen_t namelen;
};

struct PRIVSEP_SENDTO_ARG {
	enum PRIVSEP_CMD cmd;
	size_t len;
	int flags;
	struct sockaddr_storage to;
	socklen_t tolen;
	u_char msg[0];
};

struct PRIVSEP_UNLINK_ARG {
	enum PRIVSEP_CMD cmd;
	char path[PATH_MAX];
};

struct PRIVSEP_COMMON_RESP {
	int retval;
	int rerrno;
};

static void  privsep_priv_main (int, int);
static void  privsep_priv_on_sockio (int, short, void *);
static void  privsep_priv_on_monpipeio (int, short, void *);
static int   privsep_recvfd (void);
static int   privsep_common_resp (void);
static void  privsep_sendfd(int, int, int);
static int   privsep_npppd_check_open (struct PRIVSEP_OPEN_ARG *);
static int   privsep_npppd_check_socket (struct PRIVSEP_SOCKET_ARG *);
static int   privsep_npppd_check_bind (struct PRIVSEP_BIND_ARG *);
static int   privsep_npppd_check_sendto (struct PRIVSEP_SENDTO_ARG *);
static int   privsep_npppd_check_unlink (struct PRIVSEP_UNLINK_ARG *);

static int privsep_sock = -1, privsep_monpipe = -1;;
static pid_t privsep_pid;

int
privsep_init(void)
{
	pid_t pid;
	int pairsock[] = { -1, -1 }, monpipe[] = { -1, -1 }, ival;

	if (socketpair(AF_UNIX, SOCK_DGRAM, PF_UNSPEC, pairsock) == -1)
		return -1;

	ival = PRIVSEP_BUFSIZE;
	if (setsockopt(pairsock[1], SOL_SOCKET, SO_SNDBUF, &ival, sizeof(ival))
	    != 0)
		goto fail;
	if (setsockopt(pairsock[0], SOL_SOCKET, SO_RCVBUF, &ival, sizeof(ival))
	    != 0)
		goto fail;

	/* pipe for monitoring */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, monpipe) == -1)
		goto fail;

	if ((pid = fork()) < 0)
		goto fail;
	else if (pid == 0) {
		setsid();
		/* privileged process */
		setproctitle("[priv]");
		close(pairsock[1]);
		close(monpipe[1]);
		privsep_priv_main(pairsock[0], monpipe[0]);
		_exit(0);
		/* NOTREACHED */
	}
	close(pairsock[0]);
	close(monpipe[0]);
	privsep_sock = pairsock[1];
	privsep_monpipe = monpipe[1];
	privsep_pid = pid;

	return 0;
	/* NOTREACHED */
fail:
	if (pairsock[0] >= 0) {
		close(pairsock[0]);
		close(pairsock[1]);
	}
	if (monpipe[0] >= 0) {
		close(monpipe[0]);
		close(monpipe[1]);
	}

	return -1;
}

void
privsep_fini(void)
{
	if (privsep_sock >= 0) {
		close(privsep_sock);
		privsep_sock = -1;
	}
	if (privsep_monpipe >= 0) {
		close(privsep_monpipe);
		privsep_monpipe = -1;
	}
}

pid_t
privsep_priv_pid (void)
{
	return privsep_pid;
}
/***********************************************************************
 * Functions for from jail
 ***********************************************************************/
int
priv_bind(int sock, const struct sockaddr *name, socklen_t namelen)
{
	int retval;
	struct PRIVSEP_BIND_ARG a;
	struct msghdr m;
	struct iovec iov[1];
	struct cmsghdr *cm;
	u_char cm_space[CMSG_LEN(sizeof(int))];

	if (namelen > sizeof(a.name)) {
		errno = EINVAL;
		return -1;
	}
	memset(&m, 0, sizeof(m));

	iov[0].iov_base = &a;
	iov[0].iov_len = sizeof(a);
	a.cmd = PRIVSEP_BIND;
	memcpy(&a.name, name, namelen);
	a.namelen = namelen;

	cm = (struct cmsghdr *)cm_space;
	cm->cmsg_len = sizeof(cm_space);
	cm->cmsg_level = SOL_SOCKET;
	cm->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(cm) = sock;

	m.msg_iov = iov;
	m.msg_iovlen = nitems(iov);
	m.msg_control = cm;
	m.msg_controllen = sizeof(cm_space);

	if ((retval = sendmsg(privsep_sock, &m, 0)) < 0)
		return retval;

	return privsep_common_resp();
}

int
priv_socket(int domain, int type, int protocol)
{
	int retval;
	struct PRIVSEP_SOCKET_ARG a;

	a.cmd = PRIVSEP_SOCKET;
	a.domain = domain;
	a.type = type;
	a.protocol = protocol;
	if ((retval = send(privsep_sock, &a, sizeof(a), 0)) < 0)
		return retval;

	return privsep_recvfd();
}

int
priv_open(const char *path, int flags, mode_t mode)
{
	int retval;
	struct PRIVSEP_OPEN_ARG a;

	a.cmd = PRIVSEP_OPEN;
	strlcpy(a.path, path, sizeof(a.path));
	a.flags = flags;
	a.mode = mode;
	if ((retval = send(privsep_sock, &a, sizeof(a), 0)) < 0)
		return retval;

	return privsep_recvfd();
}

FILE *
priv_fopen(const char *path)
{
	int f;

	if ((f = priv_open(path, O_RDONLY, 0600)) < 0)
		return NULL;

	return fdopen(f, "r");
}

int
priv_sendto(int s, const void *msg, int len, int flags,
    const struct sockaddr *to, socklen_t tolen)
{
	struct PRIVSEP_SENDTO_ARG a;
	struct msghdr m;
	struct iovec iov[2];
	struct cmsghdr *cm;
	u_char cm_space[CMSG_LEN(sizeof(int))];
	int retval;

	if (tolen > sizeof(a.to)) {
		errno = EINVAL;
		return -1;
	}
	memset(&m, 0, sizeof(m));

	iov[0].iov_base = &a;
	/*
	 * Don't assume sizeof(struct PRIVSEP_SENDTO_ARG) equals
	 * offsetof(struct PRIVSEP_SENDTO_ARG, msg).
	 */
	iov[0].iov_len = offsetof(struct PRIVSEP_SENDTO_ARG, msg);
	iov[1].iov_base = (void *)msg;
	iov[1].iov_len = len;

	a.cmd = PRIVSEP_SENDTO;
	a.len = len;
	a.flags = flags;
	a.tolen = tolen;
	if (tolen > 0)
		memcpy(&a.to, to, tolen);

	cm = (struct cmsghdr *)cm_space;
	cm->cmsg_len = sizeof(cm_space);
	cm->cmsg_level = SOL_SOCKET;
	cm->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(cm) = s;

	m.msg_iov = iov;
	m.msg_iovlen = nitems(iov);
	m.msg_control = cm;
	m.msg_controllen = sizeof(cm_space);

	if ((retval = sendmsg(privsep_sock, &m, 0)) < 0)
		return retval;

	return privsep_common_resp();
}

int
priv_send(int s, const void *msg, int len, int flags)
{
	return priv_sendto(s, msg, len, flags, NULL, 0);
}

int
priv_unlink(const char *path)
{
	int retval;
	struct PRIVSEP_UNLINK_ARG a;

	a.cmd = PRIVSEP_UNLINK;
	strlcpy(a.path, path, sizeof(a.path));
	if ((retval = send(privsep_sock, &a, sizeof(a), 0)) < 0)
		return retval;

	return privsep_common_resp();
}

static int
privsep_recvfd(void)
{
	struct PRIVSEP_COMMON_RESP r;
	struct msghdr m;
	struct cmsghdr *cm;
	struct iovec iov[1];
	u_char cmsgbuf[256];

	memset(&m, 0, sizeof(m));

	iov[0].iov_base = &r;
	iov[0].iov_len = sizeof(r);
	memset(cmsgbuf, 0, sizeof(cmsgbuf));
	m.msg_iov = iov;
	m.msg_iovlen = nitems(iov);
	m.msg_control = cmsgbuf;
	m.msg_controllen = sizeof(cmsgbuf);
	if (recvmsg(privsep_sock, &m, 0) < sizeof(struct PRIVSEP_COMMON_RESP))
		goto on_error;
	if (r.retval < 0) {
		errno = r.rerrno;
		return r.retval;
	}
	for (cm = CMSG_FIRSTHDR(&m); m.msg_controllen != 0 && cm;
	    cm = CMSG_NXTHDR(&m, cm)) {
		if (cm->cmsg_level == SOL_SOCKET &&
		    cm->cmsg_type == SCM_RIGHTS &&
		    cm->cmsg_len >= CMSG_LEN(sizeof(int))) {
			return *(int *)CMSG_DATA(cm);
		} else
			break;
	}

on_error:
	errno = EACCES;
	return -1;
}

static int
privsep_common_resp(void)
{
	struct PRIVSEP_COMMON_RESP r;

	if (recv(privsep_sock, &r, sizeof(r), 0)
	    < sizeof(struct PRIVSEP_COMMON_RESP)) {
		errno = EACCES;
		return -1;
	}
	if (r.retval != 0)
		errno = r.rerrno;

	return r.retval;
}

/***********************************************************************
 * privileged process
 ***********************************************************************/
static void
privsep_priv_main(int sock, int monpipe)
{
	struct event ev_sock, ev_monpipe;

	event_init();

	event_set(&ev_sock, sock, EV_READ | EV_PERSIST, privsep_priv_on_sockio,
	    NULL);
	event_set(&ev_monpipe, monpipe, EV_READ, privsep_priv_on_monpipeio,
	    NULL);

	if (event_add(&ev_sock, NULL) != 0)
		err(1, "event_add() failed on %s()", __func__);
	if (event_add(&ev_monpipe, NULL) != 0)
		err(1, "event_add() failed on %s()", __func__);

	event_loop(0);
	close(sock);
	close(monpipe);

	exit(EXIT_SUCCESS);
}

static void
privsep_priv_on_sockio(int sock, short evmask, void *ctx)
{
	int retval, fdesc;
	u_char rbuf[PRIVSEP_BUFSIZE], rcmsgbuf[128];
	struct iovec riov[1];
	struct msghdr rmsg;
	struct cmsghdr *cm;

	if (evmask & EV_READ) {
		fdesc = -1;

		memset(&rmsg, 0, sizeof(rmsg));
		riov[0].iov_base = rbuf;
		riov[0].iov_len = sizeof(rbuf);
		rmsg.msg_iov = riov;
		rmsg.msg_iovlen = nitems(riov);
		rmsg.msg_control = rcmsgbuf;
		rmsg.msg_controllen = sizeof(rcmsgbuf);

		if ((retval = recvmsg(sock, &rmsg, 0)) < 0) {
			event_loopexit(NULL);
			return;
		}

		for (cm = CMSG_FIRSTHDR(&rmsg); rmsg.msg_controllen != 0 && cm;
		    cm = CMSG_NXTHDR(&rmsg, cm)) {
			if (cm->cmsg_level == SOL_SOCKET &&
			    cm->cmsg_type == SCM_RIGHTS &&
			    cm->cmsg_len >= CMSG_LEN(sizeof(int))) {
				fdesc = *(int *)CMSG_DATA(cm);
			}
		}

		switch (*(enum PRIVSEP_CMD *)rbuf) {
		case PRIVSEP_OPEN: {
			struct PRIVSEP_OPEN_ARG	*a;

			a = (struct PRIVSEP_OPEN_ARG *)rbuf;
			if (privsep_npppd_check_open(a))
				privsep_sendfd(sock, -1, EACCES);
			else
				privsep_sendfd(sock,
				    open(a->path, a->flags, a->mode), 0);
		    }
			break;
		case PRIVSEP_SOCKET: {
			struct PRIVSEP_SOCKET_ARG *a;

			a = (struct PRIVSEP_SOCKET_ARG *)rbuf;
			if (privsep_npppd_check_socket(a))
				privsep_sendfd(sock, -1, EACCES);
			else
				privsep_sendfd(sock,
				    socket(a->domain, a->type, a->protocol), 0);
		    }
			break;
		case PRIVSEP_UNLINK: {
			struct PRIVSEP_UNLINK_ARG *a;
			struct PRIVSEP_COMMON_RESP r;

			a = (struct PRIVSEP_UNLINK_ARG *)rbuf;
			if (privsep_npppd_check_unlink(a)) {
				r.retval = -1;
				r.rerrno = EACCES;
			} else {
				if ((r.retval = unlink(a->path)) != 0)
					r.rerrno = errno;
			}
			(void)send(sock, &r, sizeof(r), 0);
		    }
			break;
		case PRIVSEP_BIND: {
			struct PRIVSEP_BIND_ARG	*a;
			struct PRIVSEP_COMMON_RESP r;

			a = (struct PRIVSEP_BIND_ARG *)rbuf;
			if (fdesc < 0) {
				r.rerrno = EINVAL;
				r.retval = -1;
			} else if (privsep_npppd_check_bind(a)) {
				r.rerrno = EACCES;
				r.retval = -1;
			} else {
				if ((r.retval = bind(fdesc,
				    (struct sockaddr *)&a->name, a->namelen))
				    != 0)
					r.rerrno = errno;
				close(fdesc);
				fdesc = -1;
			}
			(void)send(sock, &r, sizeof(r), 0);
		    }
			break;
		case PRIVSEP_SENDTO: {
			struct PRIVSEP_SENDTO_ARG *a;
			struct PRIVSEP_COMMON_RESP r;

			a = (struct PRIVSEP_SENDTO_ARG *)rbuf;
			if (retval < sizeof(struct PRIVSEP_SENDTO_ARG) ||
			    retval < offsetof(struct PRIVSEP_SENDTO_ARG,
				    msg[a->len])) {
				r.rerrno = EMSGSIZE;
				r.retval = -1;
			} else if (fdesc < 0) {
				r.rerrno = EINVAL;
				r.retval = -1;
			} else if (privsep_npppd_check_sendto(a)) {
				r.rerrno = EACCES;
				r.retval = -1;
			} else {
				if (a->tolen > 0)
					r.retval = sendto(fdesc, a->msg, a->len,
					    a->flags, (struct sockaddr *)&a->to,
					    a->tolen);
				else
					r.retval = send(fdesc, a->msg, a->len,
					    a->flags);
				if (r.retval < 0)
					r.rerrno = errno;
				close(fdesc);
				fdesc = -1;
			}
			(void)send(sock, &r, sizeof(r), 0);
		    }
			break;
		}
	}
}

static void
privsep_priv_on_monpipeio(int sock, short evmask, void *ctx)
{
	/* called when the monitoring pipe is closed or broken */
	event_loopexit(NULL);
}

static void
privsep_sendfd(int sock, int fdesc, int rerrno)
{
	struct PRIVSEP_COMMON_RESP r;
	struct msghdr msg;
	struct cmsghdr *cm;
	struct iovec iov[1];
	u_char cm_space[CMSG_LEN(sizeof(int))];

	memset(&msg, 0, sizeof(msg));
	cm = (struct cmsghdr *)cm_space;
	iov[0].iov_base = &r;
	iov[0].iov_len = sizeof(r);
	r.rerrno = 0;
	r.retval = fdesc;

	if (fdesc < 0) {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		r.rerrno = (rerrno == 0)? errno : rerrno;
	} else {
		cm->cmsg_len = sizeof(cm_space);
		cm->cmsg_level = SOL_SOCKET;
		cm->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cm) = r.retval;
		msg.msg_control = cm;
		msg.msg_controllen = sizeof(cm_space);
	}
	msg.msg_iov = iov;
	msg.msg_iovlen = nitems(iov);
	(void)sendmsg(sock, &msg, 0);
	if (fdesc >= 0)
		close(fdesc);
}

static int
startswith(const char *str, const char *prefix)
{
	return (strncmp(str, prefix, strlen(prefix)) == 0)? 1 : 0;
}

static int
privsep_npppd_check_open(struct PRIVSEP_OPEN_ARG *arg)
{
	int i;
	struct _allow_paths {
		const char *path;
		int path_is_prefix;
		int readonly;
	} const allow_paths[] = {
		{ NPPPD_DIR "/",	1,	1 },
		{ "/dev/bpf",		1,	0 },
		{ "/etc/resolv.conf",	0,	1 }
	};

	for (i = 0; i < nitems(allow_paths); i++) {
		if (allow_paths[i].path_is_prefix) {
			if (!startswith(arg->path, allow_paths[i].path))
				continue;
		} else if (strcmp(arg->path, allow_paths[i].path) != 0)
			continue;
		if (allow_paths[i].readonly) {
		    	if ((arg->flags & O_ACCMODE) != O_RDONLY)
				continue;
		}
 		return 0;
	}
	return 1;
}

static int
privsep_npppd_check_socket(struct PRIVSEP_SOCKET_ARG *arg)
{
	/* npppd uses routing socket */
	if (arg->domain == PF_ROUTE && arg->type == SOCK_RAW &&
	    arg->protocol  == AF_UNSPEC)
		return 0;

	/* npppd uses raw ip socket for GRE */
	if (arg->domain == AF_INET && arg->type == SOCK_RAW &&
	    arg->protocol == IPPROTO_GRE)
		return 0;

	/* L2TP uses PF_KEY socket to delete IPsec-SA */
	if (arg->domain == PF_KEY && arg->type == SOCK_RAW &&
	    arg->protocol == PF_KEY_V2)
		return 0;

	return 1;
}

static int
privsep_npppd_check_bind(struct PRIVSEP_BIND_ARG *arg)
{
	/* npppd uses /var/run/npppd_ctl as UNIX domain socket */
	if (arg->name.ss_family == AF_UNIX &&
	    startswith(((struct sockaddr_un *)&arg->name)->sun_path,
		    "/var/run/"))
		return 0;

	return 1;
}

static int
privsep_npppd_check_sendto(struct PRIVSEP_SENDTO_ARG *arg)
{
	/* for reply npppdctl's request */
	if (arg->flags == 0 && arg->tolen > 0 &&
	    arg->to.ss_family == AF_UNIX)
		return 0;

	/* for sending a routing socket message. */
	if (arg->flags == 0 && arg->tolen == 0)
		return 0;

	return 1;
}

static int
privsep_npppd_check_unlink(struct PRIVSEP_UNLINK_ARG *arg)
{
	/* npppd unlink the /var/run/npppd_ctl */
	if (startswith(arg->path, "/var/run/"))
		return 0;

	return 1;
}
