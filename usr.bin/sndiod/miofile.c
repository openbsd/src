/*	$OpenBSD: miofile.c,v 1.9 2021/11/01 14:43:25 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/time.h>

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndio.h>
#include "defs.h"
#include "fdpass.h"
#include "file.h"
#include "midi.h"
#include "miofile.h"
#include "utils.h"

int  port_mio_pollfd(void *, struct pollfd *);
int  port_mio_revents(void *, struct pollfd *);
void port_mio_in(void *);
void port_mio_out(void *);
void port_mio_hup(void *);

struct fileops port_mio_ops = {
	"mio",
	port_mio_pollfd,
	port_mio_revents,
	port_mio_in,
	port_mio_out,
	port_mio_hup
};

int
port_mio_open(struct port *p)
{
	p->mio.hdl = fdpass_mio_open(p->num, p->midi->mode);
	if (p->mio.hdl == NULL)
		return 0;
	p->mio.file = file_new(&port_mio_ops, p, "port", mio_nfds(p->mio.hdl));
	return 1;
}

void
port_mio_close(struct port *p)
{
	file_del(p->mio.file);
	mio_close(p->mio.hdl);
}

int
port_mio_pollfd(void *addr, struct pollfd *pfd)
{
	struct port *p = addr;
	struct midi *ep = p->midi;
	int events = 0;

	if (ep->mode & MODE_MIDIIN)
		events |= POLLIN;
	if ((ep->mode & MODE_MIDIOUT) && ep->obuf.used > 0)
		events |= POLLOUT;
	return mio_pollfd(p->mio.hdl, pfd, events);
}

int
port_mio_revents(void *addr, struct pollfd *pfd)
{
	struct port *p = addr;

	return mio_revents(p->mio.hdl, pfd);
}

void
port_mio_in(void *arg)
{
	unsigned char data[MIDI_BUFSZ];
	struct port *p = arg;
	struct midi *ep = p->midi;
	int n;

	for (;;) {
		n = mio_read(p->mio.hdl, data, MIDI_BUFSZ);
		if (n == 0)
			break;
		midi_in(ep, data, n);
	}
}

void
port_mio_out(void *arg)
{
	struct port *p = arg;
	struct midi *ep = p->midi;
	unsigned char *data;
	int n, count;

	for (;;) {
		data = abuf_rgetblk(&ep->obuf, &count);
		if (count == 0)
			break;
		n = mio_write(p->mio.hdl, data, count);
		if (n == 0)
			break;
		abuf_rdiscard(&ep->obuf, n);
		if (n < count)
			break;
	}
	if (p->state == PORT_DRAIN && ep->obuf.used == 0)
		port_close(p);
	midi_fill(ep);
}

void
port_mio_hup(void *arg)
{
	struct port *p = arg;

	port_migrate(p);
	midi_abort(p->midi);
	if (p->state != PORT_CFG)
		port_close(p);
}
