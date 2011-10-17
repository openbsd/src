/*	$OpenBSD: aucat.c,v 1.51 2011/10/17 21:09:11 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aucat.h"
#include "debug.h"


/*
 * read a message, return 0 if not completed
 */
int
aucat_rmsg(struct aucat *hdl, int *eof)
{
	ssize_t n;
	unsigned char *data;

	if (hdl->rstate != RSTATE_MSG) {
		DPRINTF("aucat_rmsg: bad state\n");
		abort();
	}
	while (hdl->rtodo > 0) {
		data = (unsigned char *)&hdl->rmsg;
		data += sizeof(struct amsg) - hdl->rtodo;
		while ((n = read(hdl->fd, data, hdl->rtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				*eof = 1;
				DPERROR("aucat_rmsg: read");
			}
			return 0;
		}
		if (n == 0) {
			DPRINTF("aucat_rmsg: eof\n");
			*eof = 1;
			return 0;
		}
		hdl->rtodo -= n;
	}
	if (ntohl(hdl->rmsg.cmd) == AMSG_DATA) {
		hdl->rtodo = ntohl(hdl->rmsg.u.data.size);
		hdl->rstate = RSTATE_DATA;
	} else {
		hdl->rtodo = sizeof(struct amsg);
		hdl->rstate = RSTATE_MSG;
	}
	return 1;
}

/*
 * write a message, return 0 if not completed
 */
int
aucat_wmsg(struct aucat *hdl, int *eof)
{
	ssize_t n;
	unsigned char *data;

	if (hdl->wstate == WSTATE_IDLE)
		hdl->wstate = WSTATE_MSG;
		hdl->wtodo = sizeof(struct amsg);
	if (hdl->wstate != WSTATE_MSG) {
		DPRINTF("aucat_wmsg: bad state\n");
		abort();
	}
	while (hdl->wtodo > 0) {
		data = (unsigned char *)&hdl->wmsg;
		data += sizeof(struct amsg) - hdl->wtodo;
		while ((n = write(hdl->fd, data, hdl->wtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				*eof = 1;
				DPERROR("aucat_wmsg: write");
			}
			return 0;
		}
		hdl->wtodo -= n;
	}
	if (ntohl(hdl->wmsg.cmd) == AMSG_DATA) {
		hdl->wtodo = ntohl(hdl->wmsg.u.data.size);
		hdl->wstate = WSTATE_DATA;
	} else {
		hdl->wtodo = 0xdeadbeef;
		hdl->wstate = WSTATE_IDLE;
	}
	return 1;
}

size_t
aucat_rdata(struct aucat *hdl, void *buf, size_t len, int *eof)
{
	ssize_t n;

	if (hdl->rstate != RSTATE_DATA) {
		DPRINTF("aucat_rdata: bad state\n");
		abort();
	}
	if (len > hdl->rtodo)
		len = hdl->rtodo;
	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			*eof = 1;
			DPERROR("aucat_rdata: read");
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("aucat_rdata: eof\n");
		*eof = 1;
		return 0;
	}
	hdl->rtodo -= n;
	if (hdl->rtodo == 0) {
		hdl->rstate = RSTATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
	}
	DPRINTF("aucat_rdata: read: n = %zd\n", n);
	return n;
}

size_t
aucat_wdata(struct aucat *hdl, const void *buf, size_t len, unsigned wbpf, int *eof)
{
	ssize_t n;

	switch (hdl->wstate) {
	case WSTATE_IDLE:
		if (len > AMSG_DATAMAX)
			len = AMSG_DATAMAX;
		len -= len % wbpf;
		if (len == 0)
			len = wbpf;
		hdl->wmsg.cmd = htonl(AMSG_DATA);
		hdl->wmsg.u.data.size = htonl(len);
		hdl->wtodo = sizeof(struct amsg);
		hdl->wstate = WSTATE_MSG;
		/* FALLTHROUGH */
	case WSTATE_MSG:
		if (!aucat_wmsg(hdl, eof))
			return 0;
	}
	if (len > hdl->wtodo)
		len = hdl->wtodo;
	if (len == 0) {
		DPRINTF("aucat_wdata: len == 0\n");
		abort();
	}
	while ((n = write(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			*eof = 1;
			DPERROR("aucat_wdata: write");
		}
		return 0;
	}
	DPRINTF("aucat_wdata: write: n = %zd\n", n);
	hdl->wtodo -= n;
	if (hdl->wtodo == 0) {
		hdl->wstate = WSTATE_IDLE;
		hdl->wtodo = 0xdeadbeef;
	}
	return n;
}

int
aucat_mkcookie(unsigned char *cookie)
{
	struct stat sb;
	char buf[PATH_MAX], tmp[PATH_MAX], *path;
	ssize_t len;
	int fd;

	/*
	 * try to load the cookie
	 */
	path = issetugid() ? NULL : getenv("AUCAT_COOKIE");
	if (path == NULL) {
		path = issetugid() ? NULL : getenv("HOME");
		if (path == NULL)
			goto bad_gen;
		snprintf(buf, PATH_MAX, "%s/.aucat_cookie", path);
		path = buf;
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno != ENOENT)
			DPERROR(path);
		goto bad_gen;
	}
	if (fstat(fd, &sb) < 0) {
		DPERROR(path);
		goto bad_close;
	}
	if (sb.st_mode & 0077) {
		DPRINTF("%s has wrong permissions\n", path);
		goto bad_close;
	}
	len = read(fd, cookie, AMSG_COOKIELEN);
	if (len < 0) {
		DPERROR(path);
		goto bad_close;
	}
	if (len != AMSG_COOKIELEN) {
		DPRINTF("%s: short read\n", path);
		goto bad_close;
	}
	close(fd);
	return 1;
bad_close:
	close(fd);
bad_gen:
	/*
	 * generate a new cookie
	 */
	arc4random_buf(cookie, AMSG_COOKIELEN);

	/*
	 * try to save the cookie
	 */
	if (path == NULL)
		return 1;
	if (strlcpy(tmp, path, PATH_MAX) >= PATH_MAX ||
	    strlcat(tmp, ".XXXXXXXX", PATH_MAX) >= PATH_MAX) {
		DPRINTF("%s: too long\n", path);
		return 1;
	}
	fd = mkstemp(tmp);
	if (fd < 0) {
		DPERROR(tmp);
		return 1;
	}
	if (write(fd, cookie, AMSG_COOKIELEN) < 0) {
		DPERROR(tmp);
		unlink(tmp);
		close(fd);
		return 1;
	}
	close(fd);
	if (rename(tmp, path) < 0) {
		DPERROR(tmp);
		unlink(tmp);
	}
	return 1;
}

int
aucat_connect_tcp(struct aucat *hdl, char *host, char *unit)
{
	int s, error, opt;
	struct addrinfo *ailist, *ai, aihints;
	unsigned port;
	char serv[NI_MAXSERV];

	if (sscanf(unit, "%u", &port) != 1) {
		DPRINTF("%s: bad unit number\n", unit);
		return 0;
	}
	snprintf(serv, sizeof(serv), "%u", port + AUCAT_PORT);
	memset(&aihints, 0, sizeof(struct addrinfo));
	aihints.ai_socktype = SOCK_STREAM;
	aihints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(host, serv, &aihints, &ailist);
	if (error) {
		DPRINTF("%s: %s\n", host, gai_strerror(error));
		return 0;
	}
	s = -1;
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) {
			DPERROR("socket");
			continue;
		}
	restart:
		if (connect(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			if (errno == EINTR)
				goto restart;
			DPERROR("connect");
			close(s);
			s = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(ailist);
	if (s < 0)
		return 0;
	opt = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(int)) < 0) {
		DPERROR("setsockopt");
		close(s);
		return 0;
	}
	hdl->fd = s;
	return 1;
}

int
aucat_connect_un(struct aucat *hdl, char *unit)
{
	struct sockaddr_un ca;
	socklen_t len = sizeof(struct sockaddr_un);
	uid_t uid;
	int s;

	uid = geteuid();
	snprintf(ca.sun_path, sizeof(ca.sun_path),
	    "/tmp/aucat-%u/%s%s", uid, AUCAT_PATH, unit);
	ca.sun_family = AF_UNIX;
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		return 0;
	while (connect(s, (struct sockaddr *)&ca, len) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR(ca.sun_path);
		/* try shared server */
		snprintf(ca.sun_path, sizeof(ca.sun_path),
		    "/tmp/aucat/%s%s", AUCAT_PATH, unit);
		while (connect(s, (struct sockaddr *)&ca, len) < 0) {
			if (errno == EINTR)
				continue;
			DPERROR(ca.sun_path);
			close(s);
			return 0;
		}
		break;
	}
	hdl->fd = s;
	return 1;
}

int
aucat_open(struct aucat *hdl, const char *str, unsigned mode)
{
	extern char *__progname;
	int eof, hashost;
	char unit[4], *sep, *opt;
	char host[NI_MAXHOST];

	if (str == NULL)
		str = "0";
	sep = strchr(str, '/');
	if (sep == NULL) {
		hashost = 0;
	} else {
		if (sep - str >= sizeof(host)) {
			DPRINTF("aucat_open: %s: host too long\n", str);
			return 0;
		}
		memcpy(host, str, sep - str);
		host[sep - str] = '\0';
		hashost = 1;
		str = sep + 1;
	}
	sep = strchr(str, '.');
	if (sep == NULL) {
		opt = "default";
		strlcpy(unit, str, sizeof(unit));
	} else {
		opt = sep + 1;
		if (sep - str >= sizeof(unit)) {
			DPRINTF("aucat_init: %s: too long\n", str);
			return 0;
		}
		strlcpy(unit, str, opt - str);
	}
	DPRINTF("aucat_init: trying %s -> %s.%s\n", str, unit, opt);
	if (hashost) {
		if (!aucat_connect_tcp(hdl, host, unit))
			return 0;
	} else {
		if (!aucat_connect_un(hdl, unit))
			return 0;
	}
	if (fcntl(hdl->fd, F_SETFD, FD_CLOEXEC) < 0) {
		DPERROR("FD_CLOEXEC");
		goto bad_connect;
	}
	hdl->rstate = RSTATE_MSG;
	hdl->rtodo = sizeof(struct amsg);
	hdl->wstate = WSTATE_IDLE;
	hdl->wtodo = 0xdeadbeef;

	/*
	 * say hello to server
	 */
	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = htonl(AMSG_AUTH);
	if (!aucat_mkcookie(hdl->wmsg.u.auth.cookie))
		goto bad_connect;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl, &eof))
		goto bad_connect;
	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = htonl(AMSG_HELLO);
	hdl->wmsg.u.hello.version = AMSG_VERSION;
	hdl->wmsg.u.hello.mode = htons(mode);
	strlcpy(hdl->wmsg.u.hello.who, __progname,
	    sizeof(hdl->wmsg.u.hello.who));
	strlcpy(hdl->wmsg.u.hello.opt, opt,
	    sizeof(hdl->wmsg.u.hello.opt));
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl, &eof))
		goto bad_connect;
	hdl->rtodo = sizeof(struct amsg);
	if (!aucat_rmsg(hdl, &eof)) {
		DPRINTF("aucat_init: mode refused\n");
		goto bad_connect;
	}
	if (ntohl(hdl->rmsg.cmd) != AMSG_ACK) {
		DPRINTF("aucat_init: protocol err\n");
		goto bad_connect;
	}
	return 1;
 bad_connect:
	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* retry */
	return 0;
}

void
aucat_close(struct aucat *hdl, int eof)
{
	char dummy[1];

	if (!eof) {
		AMSG_INIT(&hdl->wmsg);
		hdl->wmsg.cmd = htonl(AMSG_BYE);
		hdl->wtodo = sizeof(struct amsg);
		if (!aucat_wmsg(hdl, &eof))
			goto bad_close;
		while (read(hdl->fd, dummy, 1) < 0 && errno == EINTR)
			; /* nothing */
	}
 bad_close:
	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* nothing */
}

int
aucat_setfl(struct aucat *hdl, int nbio, int *eof)
{
	if (fcntl(hdl->fd, F_SETFL, nbio ? O_NONBLOCK : 0) < 0) {
		DPERROR("aucat_setfl: fcntl");
		*eof = 1;
		return 0;
	}
	return 1;
}

int
aucat_pollfd(struct aucat *hdl, struct pollfd *pfd, int events)
{
	if (hdl->rstate == RSTATE_MSG)
		events |= POLLIN;
	pfd->fd = hdl->fd;
	pfd->events = events;
	return 1;
}

int
aucat_revents(struct aucat *hdl, struct pollfd *pfd)
{
	int revents = pfd->revents;

	DPRINTF("aucat_revents: revents: %x\n", revents);
	return revents;
}
