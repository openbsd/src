/*	$OpenBSD: ioev.c,v 1.4 2012/08/19 10:28:28 eric Exp $	*/
/*      
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"
#include "ioev.h"
#include "iobuf.h"

#ifdef IO_SSL
#	include <openssl/ssl.h>
#endif

enum {
	IO_STATE_NONE,
	IO_STATE_CONNECT,
	IO_STATE_CONNECT_SSL,
	IO_STATE_ACCEPT_SSL,
	IO_STATE_UP,

	IO_STATE_MAX,
};

const char* io_strflags(int);
const char* io_evstr(short);

void	_io_init(void);
void	io_hold(struct io *);
void	io_release(struct io *);
void	io_callback(struct io*, int);
void	io_dispatch(int, short, void *);
void	io_dispatch_connect(int, short, void *);
size_t	io_pending(struct io *);
size_t	io_queued(struct io*);
void	io_reset(struct io *, short, void (*)(int, short, void*));
void	io_frame_enter(const char *, struct io *, int);
void	io_frame_leave(struct io *);

#ifdef IO_SSL
void	ssl_error(const char *); /* XXX external */

void	io_dispatch_accept_ssl(int, short, void *);
void	io_dispatch_connect_ssl(int, short, void *);
void	io_dispatch_read_ssl(int, short, void *);
void	io_dispatch_write_ssl(int, short, void *);
void	io_reload_ssl(struct io *io);
#endif

static struct io	*current = NULL;
static uint64_t		 frame = 0;
static int		_io_debug = 0;

#define io_debug(args...) do { if (_io_debug) printf(args); } while(0)


const char*
io_strio(struct io *io)
{
	static char	buf[128];

	snprintf(buf, sizeof buf, "<io:%p fd=%i to=%i fl=%s ib=%zu ob=%zu>",
			io, io->sock, io->timeout, io_strflags(io->flags),
			io_pending(io), io_queued(io));
	return (buf);
}

#define CASE(x) case x : return #x

const char*
io_strevent(int evt)
{
	static char buf[32];

	switch(evt) {
	CASE(IO_CONNECTED);
	CASE(IO_TLSREADY);
	CASE(IO_DATAIN);
	CASE(IO_LOWAT);
	CASE(IO_DISCONNECTED);
	CASE(IO_TIMEOUT);
	CASE(IO_ERROR);
	default:
		snprintf(buf, sizeof(buf), "IO_? %i", evt);
		return buf;
	}
}

void
io_set_blocking(int fd, int blocking)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "io_set_blocking:fcntl(F_GETFL)");

	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		err(1, "io_set_blocking:fcntl(F_SETFL)");
}

void
io_set_linger(int fd, int linger)
{
	struct linger    l;

	bzero(&l, sizeof(l));
	l.l_onoff = linger ? 1 : 0;
	l.l_linger = linger;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1)
		err(1, "io_set_linger:setsockopt()");
}

/*
 * Event framing must not rely on an io pointer to refer to the "same" io
 * throughout the frame, beacuse this is not always the case:
 *
 * 1) enter(addr0) -> free(addr0) -> leave(addr0) = SEGV
 * 2) enter(addr0) -> free(addr0) -> malloc == addr0 -> leave(addr0) = BAD!
 *
 * In both case, the problem is that the io is freed in the callback, so
 * the pointer becomes invalid. If that happens, the user is required to
 * call io_clear, so we can adapt the frame state there.
 */
void
io_frame_enter(const char *where, struct io *io, int ev)
{
	io_debug("\n=== %" PRIu64 " ===\n"
	    "io_frame_enter(%s, %s, %s)\n",
	    frame, where, io_evstr(ev), io_strio(io));

	if (current)
		errx(1, "io_frame_enter: interleaved frames");

	current = io;

	io_hold(io);
}

void
io_frame_leave(struct io *io)
{
	io_debug("io_frame_leave(%" PRIu64 ")\n", frame);

	if (current && current != io)
		errx(1, "io_frame_leave: io mismatch");

	/* io has been cleared */
	if (current == NULL)
		goto done;

	/* TODO: There is a possible optimization there:
	 * In a typical half-duplex request/response scenario,
	 * the io is waiting to read a request, and when done, it queues
	 * the response in the output buffer and goes to write mode.
	 * There, the write event is set and will be triggered in the next
	 * event frame.  In most case, the write call could be done
	 * immediatly as part of the last read frame, thus avoiding to go
	 * through the event loop machinery. So, as an optimisation, we
	 * could detect that case here and force an event dispatching.
	 */

	/* Reload the io if it has not been reset already. */
	io_release(io);
	current = NULL;
    done:
	io_debug("=== /%" PRIu64 "\n", frame);

	frame += 1;
}

void
_io_init()
{
	static int init = 0;

	if (init)
		return;

	init = 1;
	_io_debug = getenv("IO_DEBUG") != NULL;
}

void
io_init(struct io *io, int sock, void *arg,
	void(*cb)(struct io*, int), struct iobuf *iobuf)
{
	_io_init();

	memset(io, 0, sizeof *io);

	io->sock = sock;
	io->timeout = -1;
	io->arg = arg;
	io->iobuf = iobuf;
	io->cb = cb;

	if (sock != -1)
		io_reload(io);
}

void
io_clear(struct io *io)
{
	io_debug("io_clear(%p)\n", io);

	/* the current io is virtually dead */
	if (io == current)
		current = NULL;

#ifdef IO_SSL
	if (io->ssl) {
		SSL_free(io->ssl);
		io->ssl = NULL;
	}
#endif

	event_del(&io->ev);
	if (io->sock != -1) {
		close(io->sock);
		io->sock = -1;
	}
}

void
io_hold(struct io *io)
{
	io_debug("io_enter(%p)\n", io);

	if (io->flags & IO_HELD)
		errx(1, "io_hold: io is already held");

	io->flags &= ~IO_RESET;
	io->flags |= IO_HELD;
}

void
io_release(struct io *io)
{
	if (!(io->flags & IO_HELD))
		errx(1, "io_release: io is not held");

	io->flags &= ~IO_HELD;
	if (!(io->flags & IO_RESET))
		io_reload(io);
}

void
io_set_timeout(struct io *io, int msec)
{
	io_debug("io_set_timeout(%p, %i)\n", io, msec);

	io->timeout = msec;
}

void
io_set_lowat(struct io *io, size_t lowat)
{
	io_debug("io_set_lowat(%p, %zu)\n", io, lowat);

	io->lowat = lowat;
}

void
io_pause(struct io *io, int dir)
{
	io_debug("io_pause(%p, %x)\n", io, dir);

	io->flags |= dir & (IO_PAUSE_IN | IO_PAUSE_OUT);
	io_reload(io);
}

void
io_resume(struct io *io, int dir)
{
	io_debug("io_resume(%p, %x)\n", io, dir);

	io->flags &= ~(dir & (IO_PAUSE_IN | IO_PAUSE_OUT));
	io_reload(io);
}

void
io_set_read(struct io *io)
{
	int	mode;

	io_debug("io_set_read(%p)\n", io);

	mode = io->flags & IO_RW;
	if (!(mode == 0 || mode == IO_WRITE))
		errx(1, "io_set_read(): full-duplex or reading");

	io->flags &= ~IO_RW;
	io->flags |= IO_READ;
	io_reload(io);
}

void
io_set_write(struct io *io)
{
	int	mode;

	io_debug("io_set_write(%p)\n", io);

	mode = io->flags & IO_RW;
	if (!(mode == 0 || mode == IO_READ))
		errx(1, "io_set_write(): full-duplex or writing");

	io->flags &= ~IO_RW;
	io->flags |= IO_WRITE;
	io_reload(io);
}

#define IO_READING(io) (((io)->flags & IO_RW) != IO_WRITE)
#define IO_WRITING(io) (((io)->flags & IO_RW) != IO_READ)

/*
 * Setup the necessary events as required by the current io state,
 * honouring duplex mode and i/o pauses.
 */
void
io_reload(struct io *io)
{
	short	events;

	/* io will be reloaded at release time */
	if (io->flags & IO_HELD)
		return;

#ifdef IO_SSL
	if (io->ssl) {
		io_reload_ssl(io);
		return;
	}
#endif

	io_debug("io_reload(%p)\n", io);

	events = 0;
	if (IO_READING(io) && !(io->flags & IO_PAUSE_IN))
		events = EV_READ;
	if (IO_WRITING(io) && !(io->flags & IO_PAUSE_OUT) && io_queued(io))
		events |= EV_WRITE;

	io_reset(io, events, io_dispatch);
}

/* Set the requested event. */
void
io_reset(struct io *io, short events, void (*dispatch)(int, short, void*))
{
	struct timeval	tv, *ptv;

	io_debug("io_reset(%p, %s, %p) -> %s\n",
	    io, io_evstr(events), dispatch, io_strio(io));

	/*
	 * Indicate that the event has already been reset so that reload
	 * is not called on frame_leave.
	 */
	io->flags |= IO_RESET;

	event_del(&io->ev);

	/*
	 * The io is paused by the user, so we don't want the timeout to be
	 * effective.
	 */
	if (events == 0)
		return;

	event_set(&io->ev, io->sock, events, dispatch, io);
	if (io->timeout >= 0) {
		tv.tv_sec = io->timeout / 1000;
		tv.tv_usec = (io->timeout % 1000) * 1000;
		ptv = &tv;
	} else
		ptv = NULL;

        event_add(&io->ev, ptv);
}

size_t
io_pending(struct io *io)
{
	return iobuf_len(io->iobuf);
}

size_t
io_queued(struct io *io)
{
	return iobuf_queued(io->iobuf);
}

const char*
io_strflags(int flags)
{
	static char	buf[64];

	buf[0] = '\0';

	switch(flags & IO_RW) {
	case 0:
		strlcat(buf, "rw", sizeof buf);
		break;
	case IO_READ:
		strlcat(buf, "R", sizeof buf);
		break;
	case IO_WRITE:
		strlcat(buf, "W", sizeof buf);
		break;
	case IO_RW:
		strlcat(buf, "RW", sizeof buf);
		break;
	}

	if (flags & IO_PAUSE_IN)
		strlcat(buf, ",F_PI", sizeof buf);
	if (flags & IO_PAUSE_OUT)
		strlcat(buf, ",F_PO", sizeof buf);

	return buf;
}

const char*
io_evstr(short ev)
{
	static char	buf[64];
	char		buf2[16];
	int		n;

	n = 0;
	buf[0] = '\0';

	if (ev == 0) {
		strlcat(buf, "<NONE>", sizeof(buf));
		return buf;
	}

	if (ev & EV_TIMEOUT) {
		if (n)
			strlcat(buf, "|", sizeof(buf));
		strlcat(buf, "EV_TIMEOUT", sizeof(buf));
		ev &= ~EV_TIMEOUT;
		n++;
	}

	if (ev & EV_READ) {
		if (n)
			strlcat(buf, "|", sizeof(buf));
		strlcat(buf, "EV_READ", sizeof(buf));
		ev &= ~EV_READ;
		n++;
	}

	if (ev & EV_WRITE) {
		if (n)
			strlcat(buf, "|", sizeof(buf));
		strlcat(buf, "EV_WRITE", sizeof(buf));
		ev &= ~EV_WRITE;
		n++;
	}

	if (ev & EV_SIGNAL) {
		if (n)
			strlcat(buf, "|", sizeof(buf));
		strlcat(buf, "EV_SIGNAL", sizeof(buf));
		ev &= ~EV_SIGNAL;
		n++;
	}

	if (ev) {
		if (n)
			strlcat(buf, "|", sizeof(buf));
		strlcat(buf, "EV_?=0x", sizeof(buf));
		snprintf(buf2, sizeof(buf2), "%hx", ev);
		strlcat(buf, buf2, sizeof(buf));
	}

	return buf;
}

void
io_dispatch(int fd, short ev, void *humppa)
{
	struct io	*io = humppa;
	size_t		 w;
	ssize_t		 n;

	io_frame_enter("io_dispatch", io, ev);

	if (ev == EV_TIMEOUT) {
		io_callback(io, IO_TIMEOUT);
		goto leave;
	}

	if (ev & EV_WRITE && (w = io_queued(io))) {
		if ((n = iobuf_write(io->iobuf, io->sock)) < 0) {
			if (n == IO_ERROR)
				log_warn("io_dispatch: iobuf_write");
			io_callback(io, n == IOBUF_CLOSED ?
			    IO_DISCONNECTED : IO_ERROR);
			goto leave;
		}
		if (w > io->lowat && w - n <= io->lowat)
			io_callback(io, IO_LOWAT);
	}

	if (ev & EV_READ) {
		if ((n = iobuf_read(io->iobuf, io->sock)) < 0) {
			if (n == IO_ERROR)
				log_warn("io_dispatch: iobuf_read");
			io_callback(io, n == IOBUF_CLOSED ?
			    IO_DISCONNECTED : IO_ERROR);
			goto leave;
		}
		if (n)
			io_callback(io, IO_DATAIN);
	}

   leave:
	io_frame_leave(io);
}

void
io_callback(struct io *io, int evt)
{
	io->cb(io, evt);
}

int
io_connect(struct io *io, const struct sockaddr *sa)
{
	int	sock, errno_save;

	if ((sock = socket(sa->sa_family, SOCK_STREAM, 0)) == -1)
		goto fail;

	io_set_blocking(sock, 0);
	io_set_linger(sock, 0);

	if (connect(sock, sa, sa->sa_len) == -1)
		if (errno != EINPROGRESS)
			goto fail;

	io->sock = sock;
	io_reset(io, EV_WRITE, io_dispatch_connect);

	return (sock);

    fail:
	if (sock != -1) {
		errno_save = errno;
		close(sock);
		errno = errno_save;
	}
	return (-1);
}

void
io_dispatch_connect(int fd, short ev, void *humppa)
{
	struct io	*io = humppa;

	io_frame_enter("io_dispatch_connect", io, ev);

	if (ev == EV_TIMEOUT) {
		close(fd);
		io->sock = -1;
		io_callback(io, IO_TIMEOUT);
	} else {
		io->state = IO_STATE_UP;
		io_callback(io, IO_CONNECTED);
	}

	io_frame_leave(io);
}

#ifdef IO_SSL

int
io_start_tls(struct io *io, void *ssl)
{
	int	mode;

	mode = io->flags & IO_RW;
	if (mode == 0 || mode == IO_RW)
		errx(1, "io_start_tls(): full-duplex or unset");

	if (io->ssl)
		errx(1, "io_start_tls(): SSL already started");
	io->ssl = ssl;

	if (SSL_set_fd(io->ssl, io->sock) == 0) {
		ssl_error("io_start_ssl:SSL_set_fd");
		return (-1);
	}

	if (mode == IO_WRITE) {
		io->state = IO_STATE_CONNECT_SSL;
		SSL_set_connect_state(io->ssl);
		io_reset(io, EV_READ | EV_WRITE, io_dispatch_connect_ssl);
	} else {
		io->state = IO_STATE_ACCEPT_SSL;
		SSL_set_accept_state(io->ssl);
		io_reset(io, EV_READ | EV_WRITE, io_dispatch_accept_ssl);
	}

	return (0);
}

void
io_dispatch_accept_ssl(int fd, short event, void *humppa)
{
	struct io	*io = humppa;
	int		 e, ret;

	io_frame_enter("io_dispatch_accept_ssl", io, event);

	if (event == EV_TIMEOUT) {
		io_callback(io, IO_TIMEOUT);
		goto leave;
	}

	if ((ret = SSL_accept(io->ssl)) > 0) {
		io->state = IO_STATE_UP;
		io_callback(io, IO_TLSREADY);
		goto leave;
	}

	switch ((e = SSL_get_error(io->ssl, ret))) {
	case SSL_ERROR_WANT_READ:
		io_reset(io, EV_READ, io_dispatch_accept_ssl);
		break;
	case SSL_ERROR_WANT_WRITE:
		io_reset(io, EV_WRITE, io_dispatch_accept_ssl);
		break;
	default:
		ssl_error("io_dispatch_accept_ssl:SSL_accept");
		io_callback(io, IO_ERROR);
		break;
	}

    leave:
	io_frame_leave(io);
}

void
io_dispatch_connect_ssl(int fd, short event, void *humppa)
{
	struct io	*io = humppa;
	int		 e, ret;

	io_frame_enter("io_dispatch_connect_ssl", io, event);

	if (event == EV_TIMEOUT) {
		io_callback(io, IO_TIMEOUT);
		goto leave;
	}

	if ((ret = SSL_connect(io->ssl)) > 0) {
		io->state = IO_STATE_UP;
		io_callback(io, IO_TLSREADY);
		goto leave;
	}

	switch ((e = SSL_get_error(io->ssl, ret))) {
	case SSL_ERROR_WANT_READ:
		io_reset(io, EV_READ, io_dispatch_connect_ssl);
		break;
	case SSL_ERROR_WANT_WRITE:
		io_reset(io, EV_WRITE, io_dispatch_connect_ssl);
		break;
	default:
		ssl_error("io_dispatch_connect_ssl:SSL_connect");
		io_callback(io, IO_ERROR);
		break;
	}

    leave:
	io_frame_leave(io);
}

void
io_dispatch_read_ssl(int fd, short event, void *humppa)
{
	struct io	*io = humppa;
	int		 n;

	io_frame_enter("io_dispatch_read_ssl", io, event);

	if (event == EV_TIMEOUT) {
		io_callback(io, IO_TIMEOUT);
		goto leave;
	}

	switch ((n = iobuf_read_ssl(io->iobuf, (SSL*)io->ssl))) {
	case IOBUF_WANT_READ:
		io_reset(io, EV_READ, io_dispatch_read_ssl);
		break;
	case IOBUF_WANT_WRITE:
		io_reset(io, EV_WRITE, io_dispatch_read_ssl);
		break;
	case IOBUF_CLOSED:
		io_callback(io, IO_DISCONNECTED);
		break;
	case IOBUF_ERROR:
		ssl_error("io_dispatch_read_ssl:SSL_read");
		io_callback(io, IO_ERROR);
		break;
	default:
		io_debug("io_dispatch_read_ssl(...) -> r=%i\n", n);
		io_callback(io, IO_DATAIN);
	}

    leave:
	io_frame_leave(io);
}

void
io_dispatch_write_ssl(int fd, short event, void *humppa)
{
	struct io	*io = humppa;
	int		 n;
	size_t		 w2, w;

	io_frame_enter("io_dispatch_write_ssl", io, event);

	if (event == EV_TIMEOUT) {
		io_callback(io, IO_TIMEOUT);
		goto leave;
	}

	w = io_queued(io);
	switch ((n = iobuf_write_ssl(io->iobuf, (SSL*)io->ssl))) {
	case IOBUF_WANT_READ:
		io_reset(io, EV_READ, io_dispatch_write_ssl);
		break;
	case IOBUF_WANT_WRITE:
		io_reset(io, EV_WRITE, io_dispatch_write_ssl);
		break;
	case IOBUF_CLOSED:
		io_callback(io, IO_DISCONNECTED);
		break;
	case IOBUF_ERROR:
		ssl_error("io_dispatch_write_ssl:SSL_write");
		io_callback(io, IO_ERROR);
		break;
	default:
		io_debug("io_dispatch_write_ssl(...) -> w=%i\n", n);
		w2 = io_queued(io);
		if (w > io->lowat && w2 <= io->lowat)
			io_callback(io, IO_LOWAT);
		break;
	}

    leave:
	io_frame_leave(io);
}

void
io_reload_ssl(struct io *io)
{
	void	(*dispatch)(int, short, void*) = NULL;

	switch(io->state) {
	case IO_STATE_CONNECT_SSL:
		dispatch = io_dispatch_connect_ssl;
		break;
	case IO_STATE_ACCEPT_SSL:
		dispatch = io_dispatch_accept_ssl;
		break;
	case IO_STATE_UP:
		if ((io->flags & IO_RW) == IO_READ)
			dispatch = io_dispatch_read_ssl;
		else {
			if (io_queued(io) == 0)
				return; /* nothing to write */
			dispatch = io_dispatch_write_ssl;
		}
		break;
	default:
		errx(1, "io_reload_ssl(): bad state");
	}

	io_reset(io, EV_READ | EV_WRITE, dispatch);
}

#endif /* IO_SSL */
