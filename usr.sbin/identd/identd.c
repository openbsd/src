/*	$OpenBSD: identd.c,v 1.5 2013/03/18 04:53:23 dlg Exp $ */

/*
 * Copyright (c) 2013 David Gwynne <dlg@openbsd.org>
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

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>

#define IDENTD_USER "_identd"

#define TIMEOUT_MIN 4
#define TIMEOUT_MAX 240
#define TIMEOUT_DEFAULT 120
#define INPUT_MAX 256

enum ident_client_state {
	S_BEGINNING = 0,
	S_SERVER_PORT,
	S_PRE_COMMA,
	S_POST_COMMA,
	S_CLIENT_PORT,
	S_PRE_EOL,
	S_EOL,

	S_DEAD,
	S_QUEUED
};

#define E_NONE		0
#define E_NOUSER	1
#define E_UNKNOWN	2
#define E_HIDDEN	3

struct ident_client {
	struct {
		/* from the socket */
		struct sockaddr_storage ss;
		socklen_t len;

		/* from the request */
		u_int port;
	} client, server;
	SIMPLEQ_ENTRY(ident_client) entry;
	enum ident_client_state state;
	struct event ev;
	struct event tmo;
	size_t rxbytes;

	char *buf;
	size_t buflen;
	size_t bufoff;
	uid_t uid;
};

struct ident_resolver {
	SIMPLEQ_ENTRY(ident_resolver) entry;
	char *buf;
	size_t buflen;
	u_int error;
};

struct identd_listener {
	struct event ev, pause;
};

void	parent_rd(int, short, void *);
void	parent_wr(int, short, void *);

void	child_rd(int, short, void *);
void	child_wr(int, short, void *);

void	identd_listen(const char *, const char *, int);
void	identd_paused(int, short, void *);
void	identd_accept(int, short, void *);
void	identd_timeout(int, short, void *);
void	identd_request(int, short, void *);
enum ident_client_state
	identd_parse(struct ident_client *, int);
void	identd_resolving(int, short, void *);
void	identd_response(int, short, void *);
int	fetchuid(struct ident_client *);

const char *gethost(struct sockaddr_storage *);
const char *getport(struct sockaddr_storage *);

struct loggers {
	void (*err)(int, const char *, ...);
	void (*errx)(int, const char *, ...);
	void (*warn)(const char *, ...);
	void (*warnx)(const char *, ...);
	void (*info)(const char *, ...);
	void (*debug)(const char *, ...);
};

const struct loggers conslogger = {
	err,
	errx,
	warn,
	warnx,
	warnx, /* info */
	warnx /* debug */
};

void	syslog_err(int, const char *, ...);
void	syslog_errx(int, const char *, ...);
void	syslog_warn(const char *, ...);
void	syslog_warnx(const char *, ...);
void	syslog_info(const char *, ...);
void	syslog_debug(const char *, ...);
void	syslog_vstrerror(int, int, const char *, va_list);

const struct loggers syslogger = {
	syslog_err,
	syslog_errx,
	syslog_warn,
	syslog_warnx,
	syslog_info,
	syslog_debug
};

const struct loggers *logger = &conslogger;

#define lerr(_e, _f...) logger->err((_e), _f)
#define lerrx(_e, _f...) logger->errx((_e), _f)
#define lwarn(_f...) logger->warn(_f)
#define lwarnx(_f...) logger->warnx(_f)
#define linfo(_f...) logger->info(_f)
#define ldebug(_f...) logger->debug(_f)

const char *gethost(struct sockaddr_storage *);
#define sa(_ss) ((struct sockaddr *)(_ss))

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-46d] [-l address] [-p port] "
	    "[-t timeout]\n", __progname);
	exit(1);
}

struct timeval timeout = { TIMEOUT_DEFAULT, 0 };
int debug = 0;
int on = 1;

struct event proc_rd, proc_wr;
union {
	struct {
		SIMPLEQ_HEAD(, ident_resolver) replies;
	} parent;
	struct {
		SIMPLEQ_HEAD(, ident_client) pushing, popping;
	} child;
} sc;

int
main(int argc, char *argv[])
{
	extern char *__progname;
	const char *errstr = NULL;

	int		 c;
	struct passwd	*pw;

	char *addr = NULL;
	char *port = "auth";
	int family = AF_UNSPEC;

	int pair[2];
	pid_t parent;
	int sibling;

	while ((c = getopt(argc, argv, "46dl:p:t:")) != -1) {
		switch (c) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'd':
			debug = 1;
			break;
		case 'l':
			addr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 't':
			timeout.tv_sec = strtonum(optarg,
			    TIMEOUT_MIN, TIMEOUT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "timeout %s is %s", optarg, errstr);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (geteuid() != 0)
		errx(1, "need root privileges");

	if (socketpair(AF_UNIX, SOCK_SEQPACKET, PF_UNSPEC, pair) == -1)
		err(1, "socketpair");

	pw = getpwnam(IDENTD_USER);
	if (pw == NULL)
		err(1, "no %s user", IDENTD_USER);

	if (!debug && daemon(1, 0) == -1)
		err(1, "daemon");

	parent = fork();
	switch (parent) {
	case -1:
		lerr(1, "fork");

	case 0:
		/* child */
		setproctitle("listener");
		close(pair[1]);
		sibling = pair[0];
		break;

	default:
		/* parent */
		setproctitle("resolver");
		close(pair[0]);
		sibling = pair[1];
		break;
	}

	if (!debug) {
		openlog(__progname, LOG_PID|LOG_NDELAY, LOG_DAEMON);
		tzset();
		logger = &syslogger;
	}

	event_init();

	if (ioctl(sibling, FIONBIO, &on) == -1)
		lerr(1, "sibling ioctl(FIONBIO)");

	if (parent) {
		SIMPLEQ_INIT(&sc.parent.replies);

		event_set(&proc_rd, sibling, EV_READ | EV_PERSIST,
		    parent_rd, NULL);
		event_set(&proc_wr, sibling, EV_WRITE,
		    parent_wr, NULL);
	} else {
		SIMPLEQ_INIT(&sc.child.pushing);
		SIMPLEQ_INIT(&sc.child.popping);

		identd_listen(addr, port, family);

		if (chroot(pw->pw_dir) == -1)
			lerr(1, "chroot(%s)", pw->pw_dir);

		if (chdir("/") == -1)
			lerr(1, "chdir(%s)", pw->pw_dir);

		event_set(&proc_rd, sibling, EV_READ | EV_PERSIST,
		    child_rd, NULL);
		event_set(&proc_wr, sibling, EV_WRITE,
		    child_wr, NULL);
	}

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		lerr(1, "unable to revoke privs");

	event_add(&proc_rd, NULL);

	event_dispatch();

	return (0);
}

void
parent_rd(int fd, short events, void *arg)
{
	struct ident_resolver *r;
	struct passwd *pw;
	ssize_t n;
	uid_t uid;

	n = read(fd, &uid, sizeof(uid));
	switch (n) {
	case -1:
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return;
		default:
			lerr(1, "parent read");
		}
		break;
	case 0:
		lerrx(1, "child has gone");
	case sizeof(uid):
		break;
	default:
		lerrx(1, "unexpected %zd data from child", n);
	}

	event_add(&proc_wr, NULL);

	r = calloc(1, sizeof(*r));
	if (r == NULL)
		lerr(1, "resolver alloc");

	pw = getpwuid(uid);
	if (pw == NULL) {
		r->error = E_NOUSER;
	} else {
		n = asprintf(&r->buf, "%s", pw->pw_name);
		if (n == -1)
			r->error = E_UNKNOWN;
		else {
			r->error = E_NONE;
			r->buflen = n;
		}
	}

	SIMPLEQ_INSERT_TAIL(&sc.parent.replies, r, entry);
	event_add(&proc_wr, NULL);
}

void
parent_wr(int fd, short events, void *arg)
{
	struct ident_resolver *r = SIMPLEQ_FIRST(&sc.parent.replies);
	struct iovec iov[2];
	int iovcnt = 0;
	ssize_t n;

	iov[iovcnt].iov_base = &r->error;
	iov[iovcnt].iov_len = sizeof(r->error);
	iovcnt++;

	if (r->buflen > 0) {
		iov[iovcnt].iov_base = r->buf;
		iov[iovcnt].iov_len = r->buflen;
		iovcnt++;
	}

	n = writev(fd, iov, iovcnt);
	if (n == -1) {
		if (errno == EAGAIN) {
			event_add(&proc_wr, NULL);
			return;
		}
		lerr(1, "parent write");
	}

	if (n != sizeof(r->error) + r->buflen)
		lerrx(1, "unexpected parent write length %zd", n);

	SIMPLEQ_REMOVE_HEAD(&sc.parent.replies, entry);

	if (r->buflen > 0)
		free(r->buf);

	free(r);
}

void
child_rd(int fd, short events, void *arg)
{
	struct ident_client *c;
	struct {
		u_int error;
		char buf[512];
	} reply;
	ssize_t n;

	n = read(fd, &reply, sizeof(reply));
	switch (n) {
	case -1:
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return;
		default:
			lerr(1, "child read");
		}
		break;
	case 0:
		lerrx(1, "parent has gone");
	default:
		break;
	}

	c = SIMPLEQ_FIRST(&sc.child.popping);
	if (c == NULL)
		lerrx(1, "unsolicited data from parent");

	SIMPLEQ_REMOVE_HEAD(&sc.child.popping, entry);

	if (n < sizeof(reply.error))
		lerrx(1, "short data from parent");

	/* check if something went wrong while the parent was working */
	if (c->state == S_DEAD) {
		free(c);
		return;
	}
	c->state = S_DEAD;

	switch (reply.error) {
	case E_NONE:
		n = asprintf(&c->buf, "%u , %u : USERID : UNIX : %s\r\n",
		    c->server.port, c->client.port, reply.buf);
		break;
	case E_NOUSER:
		n = asprintf(&c->buf, "%u , %u : ERROR : NO-USER\r\n",
		    c->server.port, c->client.port);
		break;
	case E_UNKNOWN:
		n = asprintf(&c->buf, "%u , %u : ERROR : UNKNOWN-ERROR\r\n",
		    c->server.port, c->client.port);
		break;
	case E_HIDDEN:
		n = asprintf(&c->buf, "%u , %u : ERROR : HIDDEN-USER\r\n",
		    c->server.port, c->client.port);
		break;
	default:
		lerrx(1, "unexpected error from parent %u", reply.error);
	}
	if (n == -1)
		goto fail;

	c->buflen = n;

	fd = EVENT_FD(&c->ev);
	event_del(&c->ev);
	event_set(&c->ev, fd, EV_READ | EV_WRITE | EV_PERSIST,
	    identd_response, c);
	event_add(&c->ev, NULL);
	return;

fail:
	evtimer_del(&c->tmo);
	event_del(&c->ev);
	close(fd);
	free(c);
}

void
child_wr(int fd, short events, void *arg)
{
	struct ident_client *c = SIMPLEQ_FIRST(&sc.child.pushing);
	ssize_t n;

	n = write(fd, &c->uid, sizeof(c->uid));
	switch (n) {
	case -1:
		if (errno == EAGAIN) {
			event_add(&proc_wr, NULL);
			return;
		}
		lerr(1, "child write");
	case sizeof(c->uid):
		break;
	default:
		lerrx(1, "unexpected child write length %zd", n);
	}

	SIMPLEQ_REMOVE_HEAD(&sc.child.pushing, entry);
	SIMPLEQ_INSERT_TAIL(&sc.child.popping, c, entry);

	if (!SIMPLEQ_EMPTY(&sc.child.pushing))
		event_add(&proc_wr, NULL);
}

void
identd_listen(const char *addr, const char *port, int family)
{
	struct identd_listener *l = NULL;

	struct addrinfo hints, *res, *res0;
	int error;
	int s;
	int serrno;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(addr, port, &hints, &res0);
	if (error)
		lerrx(1, "%s/%s: %s", addr, port, gai_strerror(error));

	for (res = res0; res != NULL; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
		    &on, sizeof(on)) == -1)
			err(1, "listener setsockopt(SO_REUSEADDR)");

		if (bind(s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "bind";
			serrno = errno;
			close(s);
			errno = serrno;
			continue;
		}

		if (ioctl(s, FIONBIO, &on) == -1)
			err(1, "listener ioctl(FIONBIO)");

		if (listen(s, 5) == -1)
			err(1, "listen");

		l = calloc(1, sizeof(*l));
		if (l == NULL)
			err(1, "listener ev alloc");

		event_set(&l->ev, s, EV_READ | EV_PERSIST, identd_accept, l);
		event_add(&l->ev, NULL);
		evtimer_set(&l->pause, identd_paused, l);
	}
	if (l == NULL)
		err(1, "%s", cause);

	freeaddrinfo(res0);
}

void
identd_paused(int fd, short events, void *arg)
{
	struct identd_listener *l = arg;
	event_add(&l->ev, NULL);
}

void
identd_accept(int fd, short events, void *arg)
{
	struct identd_listener *l = arg;
	struct sockaddr_storage ss;
	struct timeval pause = { 1, 0 };
	struct ident_client *c = NULL;
	socklen_t len;
	int s;

	len = sizeof(ss);
	s = accept(fd, sa(&ss), &len);
	if (s == -1) {
		switch (errno) {
		case EINTR:
		case EWOULDBLOCK:
		case ECONNABORTED:
			return;
		case EMFILE:
		case ENFILE:
			event_del(&l->ev);
			evtimer_add(&l->pause, &pause);
			return;

		default:
			lerr(1, "accept");
		}
	}

	if (ioctl(s, FIONBIO, &on) == -1)
		lerr(1, "client ioctl(FIONBIO)");

	c = calloc(1, sizeof(*c));
	if (c == NULL) {
		lwarn("client alloc");
		close(fd);
		return;
	}

	memcpy(&c->client.ss, &ss, len);
	c->client.len = len;
	ldebug("client: %s", gethost(&ss));

	/* lookup the local ip it connected to */
	c->server.len = sizeof(c->server.ss);
	if (getsockname(s, sa(&c->server.ss), &c->server.len) == -1)
		lerr(1, "getsockname");

	event_set(&c->ev, s, EV_READ | EV_PERSIST, identd_request, c);
	event_add(&c->ev, NULL);

	event_set(&c->tmo, s, 0, identd_timeout, c);
	event_add(&c->tmo, &timeout);
}

void
identd_timeout(int fd, short events, void *arg)
{
	struct ident_client *c = arg;

	event_del(&c->ev);
	close(fd);
	if (c->buf != NULL)
		free(c->buf);

	if (c->state == S_QUEUED) /* it is queued for resolving */
		c->state = S_DEAD;
	else
		free(c);
}

void
identd_request(int fd, short events, void *arg)
{
	struct ident_client *c = arg;
	char buf[64];
	ssize_t n, i;
	char *errstr = "INVALID-PORT";

	n = read(fd, buf, sizeof(buf));
	switch (n) {
	case -1:
		switch (errno) {
		case EINTR:
		case EAGAIN:
			return;
		default:
			lwarn("%s read", gethost(&c->client.ss));
			goto fail;
		}
		break;

	case 0:
		ldebug("%s closed connection", gethost(&c->client.ss));
		goto fail;
	default:
		break;
	}

	c->rxbytes += n;
	if (c->rxbytes >= INPUT_MAX)
		goto fail;

	for (i = 0; c->state < S_EOL && i < n; i++)
		c->state = identd_parse(c, buf[i]);

	if (c->state == S_DEAD)
		goto error;
	if (c->state != S_EOL)
		return;

	if (c->server.port < 1 || c->client.port < 1)
		goto error;

	if (fetchuid(c) == -1) {
		errstr = "NO-USER";
		goto error;
	}

	SIMPLEQ_INSERT_TAIL(&sc.child.pushing, c, entry);
	c->state = S_QUEUED;

	event_del(&c->ev);
	event_set(&c->ev, fd, EV_READ | EV_PERSIST, identd_resolving, c);
	event_add(&c->ev, NULL);

	event_add(&proc_wr, NULL);
	return;

error:
	n = asprintf(&c->buf, "%u , %u : ERROR : %s\r\n",
	    c->server.port, c->client.port, errstr);
	if (n == -1)
		goto fail;

	c->buflen = n;

	event_del(&c->ev);
	event_set(&c->ev, fd, EV_READ | EV_WRITE | EV_PERSIST,
	    identd_response, c);
	event_add(&c->ev, NULL);
	return;

fail:
	evtimer_del(&c->tmo);
	event_del(&c->ev);
	close(fd);
	free(c);
}

void
identd_resolving(int fd, short events, void *arg)
{
	struct ident_client *c = arg;
	char buf[64];
	ssize_t n;

	/*
	 * something happened while we're waiting for the parent to lookup
	 * the user.
	 */

	n = read(fd, buf, sizeof(buf));
	switch (n) {
	case -1:
		switch (errno) {
		case EINTR:
		case EAGAIN:
			return;
		default:
			lerrx(1, "resolving read");
		}
		/* NOTREACHED */
	case 0:
		ldebug("%s closed connection during resolving",
		    gethost(&c->client.ss));
		break;
	default:
		c->rxbytes += n;
		if (c->rxbytes >= INPUT_MAX)
			break;

		/* ignore extra input */
		return;
	}

	evtimer_del(&c->tmo);
	event_del(&c->ev);
	close(fd);
	c->state = S_DEAD; /* on the resolving queue */
}

enum ident_client_state
identd_parse(struct ident_client *c, int ch)
{
	enum ident_client_state s = c->state;

	switch (s) {
	case S_BEGINNING:
		/* ignore leading space */
		if (ch == '\t' || ch == ' ')
			return (s);

		if (ch == '0' || !isdigit(ch))
			return (S_DEAD);

		c->server.port = ch - '0';
		return (S_SERVER_PORT);

	case S_SERVER_PORT:
		if (ch == '\t' || ch == ' ')
			return (S_PRE_COMMA);
		if (ch == ',')
			return (S_POST_COMMA);

		if (!isdigit(ch))
			return (S_DEAD);

		c->server.port *= 10;
		c->server.port += ch - '0';
		if (c->server.port > 65535)
			return (S_DEAD);

		return (s);

	case S_PRE_COMMA:
		if (ch == '\t' || ch == ' ')
			return (s);
		if (ch == ',')
			return (S_POST_COMMA);

		return (S_DEAD);

	case S_POST_COMMA:
		if (ch == '\t' || ch == ' ')
			return (s);

		if (ch == '0' || !isdigit(ch))
			return (S_DEAD);

		c->client.port = ch - '0';
		return (S_CLIENT_PORT);

	case S_CLIENT_PORT:
		if (ch == '\t' || ch == ' ')
			return (S_PRE_EOL);
		if (ch == '\r' || ch == '\n')
			return (S_EOL);

		if (!isdigit(ch))
			return (S_DEAD);

		c->client.port *= 10;
		c->client.port += ch - '0';
		if (c->client.port > 65535)
			return (S_DEAD);

		return (s);

	case S_PRE_EOL:
		if (ch == '\t' || ch == ' ')
			return (s);
		if (ch == '\r' || ch == '\n')
			return (S_EOL);

		return (S_DEAD);

	case S_EOL:
		/* ignore trailing garbage */
		return (s);

	default:
		return (S_DEAD);
	}
}

void
identd_response(int fd, short events, void *arg)
{
	struct ident_client *c = arg;
	char buf[64];
	ssize_t n;

	if (events & EV_READ) {
		n = read(fd, buf, sizeof(buf));
		switch (n) {
		case -1:
			switch (errno) {
			case EINTR:
			case EAGAIN:
				/* meh, try a write */
				break;
			default:
				lerrx(1, "response read");
			}
			break;
		case 0:
			ldebug("%s closed connection during response",
			    gethost(&c->client.ss));
			goto done;
		default:
			c->rxbytes += n;
			if (c->rxbytes >= INPUT_MAX)
				goto done;

			/* ignore extra input */
			break;
		}
	}

	if (!(events & EV_WRITE))
		return; /* try again later */

	n = write(fd, c->buf + c->bufoff, c->buflen - c->bufoff);
	if (n == -1) {
		switch (errno) {
		case EAGAIN:
			return; /* try again later */
		default:
			lerr(1, "response write");
		}
	}

	c->bufoff += n;
	if (c->bufoff != c->buflen)
		return; /* try again later */

done:
	evtimer_del(&c->tmo);
	event_del(&c->ev);
	close(fd);
	free(c->buf);
	free(c);
}

void
syslog_vstrerror(int e, int priority, const char *fmt, va_list ap)
{
	char *s;
  
	if (vasprintf(&s, fmt, ap) == -1) {
		syslog(LOG_EMERG, "unable to alloc in syslog_vstrerror");
		exit(1);
	}
 
	syslog(priority, "%s: %s", s, strerror(e));
 
	free(s);
}
 
void
syslog_err(int ecode, const char *fmt, ...)
{
	va_list ap;
 
	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_EMERG, fmt, ap);
	va_end(ap);
 
	exit(ecode);
}
 
void
syslog_errx(int ecode, const char *fmt, ...)
{
	va_list ap;
 
	va_start(ap, fmt);
	vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);

	exit(ecode);  
}

void
syslog_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_WARNING, fmt, ap);
	va_end(ap);   
}

void
syslog_warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);
}

void
syslog_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_INFO, fmt, ap);
	va_end(ap);
}

void
syslog_debug(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

const char *
gethost(struct sockaddr_storage *ss)
{
	struct sockaddr *sa = (struct sockaddr *)ss;
	static char buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf),
	    NULL, 0, NI_NUMERICHOST) != 0)
		return ("(unknown)");

	return (buf);
}

const char *
getport(struct sockaddr_storage *ss)
{
	struct sockaddr *sa = (struct sockaddr *)ss;
	static char buf[NI_MAXSERV];

	if (getnameinfo(sa, sa->sa_len, NULL, 0, buf, sizeof(buf),
	    NI_NUMERICSERV) != 0)
		return ("(unknown)");

	return (buf);
}

int
fetchuid(struct ident_client *c)
{
	struct tcp_ident_mapping tir;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_IDENT };
	struct sockaddr_in *s4;
	struct sockaddr_in6 *s6;
	int err = 0;
	size_t len;

	memset(&tir, 0, sizeof(tir));
	memcpy(&tir.faddr, &c->client.ss, sizeof(&tir.faddr));
	memcpy(&tir.laddr, &c->server.ss, sizeof(&tir.laddr));

	switch (c->server.ss.ss_family) {
	case AF_INET:
		s4 = (struct sockaddr_in *)&tir.faddr;
		s4->sin_port = htons(c->client.port);

		s4 = (struct sockaddr_in *)&tir.laddr;
		s4->sin_port = htons(c->server.port);
		break;
	case AF_INET6:
		s6 = (struct sockaddr_in6 *)&tir.faddr;
		s6->sin6_port = htons(c->client.port);

		s6 = (struct sockaddr_in6 *)&tir.laddr;
		s6->sin6_port = htons(c->server.port);
		break;
	default:
		lerrx(1, "unexpected family %d", c->server.ss.ss_family);
	}

	len = sizeof(tir);
	err = sysctl(mib, sizeof(mib) / sizeof(mib[0]), &tir, &len, NULL, 0);
	if (err == -1)
		lerr(1, "sysctl");

	if (tir.ruid == -1)
		return (-1);

	c->uid = tir.ruid;
	return (0);
}
