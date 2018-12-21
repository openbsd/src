/*	$OpenBSD: lka_proc.c,v 1.6 2018/12/21 19:07:47 gilles Exp $	*/

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
	char			*name;
	struct io		*io;
	int			 ready;
};

static void	processor_io(struct io *, int, void *);
int		lka_filter_process_response(const char *, const char *);

int
lka_proc_ready(void)
{
	void	*iter;
	struct processor_instance	*pi;

	iter = NULL;
	while (dict_iter(&processors, &iter, NULL, (void **)&pi))
		if (!pi->ready)
			return 0;
	return 1;
}

void
lka_proc_forked(const char *name, int fd)
{
	struct processor_instance	*processor;

	if (!inited) {
		dict_init(&processors);
		inited = 1;
	}

	processor = xcalloc(1, sizeof *processor);
	processor->name = xstrdup(name);
	processor->io = io_new();

	io_set_nonblocking(fd);

	io_set_fd(processor->io, fd);
	io_set_callback(processor->io, processor_io, processor->name);
	dict_xset(&processors, name, processor);
}

struct io *
lka_proc_get_io(const char *name)
{
	struct processor_instance *processor;

	processor = dict_xget(&processors, name);

	return processor->io;
}

static void
processor_register(const char *name, const char *line)
{
	struct processor_instance *processor;

	processor = dict_xget(&processors, name);

	if (strcasecmp(line, "register|ready") == 0) {
		processor->ready = 1;
		return;
	}

	if (strncasecmp(line, "register|report|", 16) == 0) {
		lka_report_register_hook(name, line+16);
		return;
	}

	if (strncasecmp(line, "register|filter|", 16) == 0) {
		lka_filter_register_hook(name, line+16);
		return;
	}
}

static void
processor_io(struct io *io, int evt, void *arg)
{
	const char		*name = arg;
	char			*line = NULL;
	ssize_t			 len;

	switch (evt) {
	case IO_DATAIN:
	    nextline:
		line = io_getline(io, &len);
		/* No complete line received */
		if (line == NULL)
			return;

		if (strncasecmp("register|", line, 9) == 0)
			processor_register(name, line);
		else if (! lka_filter_process_response(name, line))
			fatalx("misbehaving filter");

		goto nextline;
	}
}
