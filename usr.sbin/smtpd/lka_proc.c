/*	$OpenBSD: lka_proc.c,v 1.2 2018/11/01 14:48:49 gilles Exp $	*/

/*
 * Copyright (c) 2018 Gilles Chehade <gilles@poolp.org>
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
#include <sys/tree.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

static int			inited = 0;
static struct dict		processors;


struct processor_instance {
	const char		*name;
	struct io		*io;
};

static void	processor_io(struct io *, int, void *);

void
lka_proc_forked(const char *name, int fd)
{
	struct processor_instance	*processor;

	if (!inited) {
		dict_init(&processors);
		inited = 1;
	}

	processor = xcalloc(1, sizeof *processor);
	processor->name = name;
	processor->io = io_new();
	io_set_fd(processor->io, fd);
	io_set_callback(processor->io, processor_io, processor);
	dict_xset(&processors, name, processor);
}

struct io *
lka_proc_get_io(const char *name)
{
	struct processor_instance	*processor = dict_xget(&processors, name);

	return processor->io;
}

static void
processor_io(struct io *io, int evt, void *arg)
{
	struct processor_instance	*processor = arg;
	char			*line = NULL;
	ssize_t			 len;

	log_trace(TRACE_IO, "processor: %p: %s %s", processor, io_strevent(evt),
	    io_strio(io));

	switch (evt) {
	case IO_DATAIN:
	    nextline:
		line = io_getline(processor->io, &len);
		/* No complete line received */
		if (line == NULL)
			return;

		goto nextline;
	}
}
