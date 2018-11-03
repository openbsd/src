/*	$OpenBSD: lka_proc.c,v 1.3 2018/11/03 13:47:46 gilles Exp $	*/

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
static int	processor_response(const char *);

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

		if (! processor_response(line))
			fatalx("misbehaving filter");

		goto nextline;
	}
}

static int
processor_response(const char *line)
{
	uint64_t reqid;
	char buffer[LINE_MAX];
	char *ep = NULL;
	char *qid = NULL;
	char *response = NULL;
	char *parameter = NULL;

	(void)strlcpy(buffer, line, sizeof buffer);
	if ((ep = strchr(buffer, '|')) == NULL)
		return 0;
	*ep = 0;

	if (strcmp(buffer, "filter-response") != 0)
		return 1;

	qid = ep+1;
	if ((ep = strchr(qid, '|')) == NULL)
		return 0;
	*ep = 0;

	reqid = strtoull(qid, &ep, 16);
	if (qid[0] == '\0' || *ep != '\0')
		return 0;
	if (errno == ERANGE && reqid == ULONG_MAX)
		return 0;

	response = ep+1;
	if ((ep = strchr(response, '|'))) {
		parameter = ep + 1;
		*ep = 0;
	}

	if (strcmp(response, "proceed") != 0 &&
	    strcmp(response, "reject") != 0 &&
	    strcmp(response, "disconnect") != 0 &&
	    strcmp(response, "rewrite") != 0)
		return 0;

	if (strcmp(response, "proceed") == 0 &&
	    parameter)
		return 0;

	if (strcmp(response, "proceed") != 0 &&
	    parameter == NULL)
		return 0;

	return lka_filter_response(reqid, response, parameter);
}
