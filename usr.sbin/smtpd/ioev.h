/*	$OpenBSD: ioev.h,v 1.5 2014/12/24 13:51:31 eric Exp $	*/
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

#include <event.h>

enum {
	IO_CONNECTED = 0, 	/* connection successful	*/
	IO_TLSREADY,		/* TLS started successfully	*/
	IO_TLSVERIFIED,		/* XXX - needs more work	*/
	IO_TLSERROR,		/* XXX - needs more work	*/
	IO_DATAIN,		/* new data in input buffer	*/
	IO_LOWAT,		/* output queue running low	*/
	IO_DISCONNECTED,	/* error?			*/
	IO_TIMEOUT,		/* error?			*/
	IO_ERROR,		/* details?			*/
};

#define IO_READ			0x01
#define IO_WRITE		0x02
#define IO_RW			(IO_READ | IO_WRITE)
#define IO_PAUSE_IN		0x04
#define IO_PAUSE_OUT		0x08
#define IO_RESET		0x10  /* internal */
#define IO_HELD			0x20  /* internal */

struct iobuf;
struct io {
	int		 sock;
	void		*arg;
	void		(*cb)(struct io*, int);
	struct iobuf	*iobuf;
	size_t		 lowat;
	int		 timeout;
	int		 flags;
	int		 state;
	struct event	 ev;
	void		*ssl;
	const char	*error; /* only valid immediately on callback */
};

void io_set_blocking(int, int);
void io_set_linger(int, int);

void io_init(struct io*, int, void*, void(*)(struct io*, int), struct iobuf*);
void io_clear(struct io*);
void io_set_read(struct io *);
void io_set_write(struct io *);
void io_set_timeout(struct io *, int);
void io_set_lowat(struct io *, size_t);
void io_pause(struct io *, int);
void io_resume(struct io *, int);
void io_reload(struct io *);
int io_connect(struct io *, const struct sockaddr *, const struct sockaddr *);
int io_start_tls(struct io *, void *);
const char* io_strio(struct io *);
const char* io_strevent(int);
