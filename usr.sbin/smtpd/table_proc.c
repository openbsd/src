/*	$OpenBSD: table_proc.c,v 1.5 2015/01/20 17:37:54 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
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

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

struct table_proc_priv {
	pid_t		pid;
	struct imsgbuf	ibuf;
};

static struct imsg	 imsg;
static size_t		 rlen;
static char		*rdata;

extern char	**environ;

static void
table_proc_call(struct table_proc_priv *p)
{
	ssize_t	n;

	if (imsg_flush(&p->ibuf) == -1) {
		log_warn("warn: table-proc: imsg_flush");
		fatalx("table-proc: exiting");
	}

	while (1) {
		if ((n = imsg_get(&p->ibuf, &imsg)) == -1) {
			log_warn("warn: table-proc: imsg_get");
			break;
		}
		if (n) {
			rlen = imsg.hdr.len - IMSG_HEADER_SIZE;
			rdata = imsg.data;

			if (imsg.hdr.type != PROC_TABLE_OK) {
				log_warnx("warn: table-proc: bad response");
				break;
			}
			return;
		}

		if ((n = imsg_read(&p->ibuf)) == -1) {
			log_warn("warn: table-proc: imsg_read");
			break;
		}

		if (n == 0) {
			log_warnx("warn: table-proc: pipe closed");
			break;
		}
	}

	fatalx("table-proc: exiting");
}

static void
table_proc_read(void *dst, size_t len)
{
	if (len > rlen) {
		log_warnx("warn: table-proc: bad msg len");
		fatalx("table-proc: exiting");
	}

	if (dst)
		memmove(dst, rdata, len);

	rlen -= len;
	rdata += len;
}

static void
table_proc_end(void)
{
	if (rlen) {
		log_warnx("warn: table-proc: bogus data");
		fatalx("table-proc: exiting");
	}
	imsg_free(&imsg);
}

/*
 * API
 */

static void *
table_proc_open(struct table *table)
{
	struct table_proc_priv	*priv;
	struct table_open_params op;
	int			 fd;

	priv = xcalloc(1, sizeof(*priv), "table_proc_open");

	fd = fork_proc_backend("table", table->t_config, table->t_name);
	if (fd == -1)
		fatalx("table-proc: exiting");

	imsg_init(&priv->ibuf, fd);

	memset(&op, 0, sizeof op);
	op.version = PROC_TABLE_API_VERSION;
	(void)strlcpy(op.name, table->t_name, sizeof op.name);
	imsg_compose(&priv->ibuf, PROC_TABLE_OPEN, 0, 0, -1, &op, sizeof op);

	table_proc_call(priv);
	table_proc_end();

	return (priv);
}

static int
table_proc_update(struct table *table)
{
	struct table_proc_priv	*priv = table->t_handle;
	int r;

	imsg_compose(&priv->ibuf, PROC_TABLE_UPDATE, 0, 0, -1, NULL, 0);

	table_proc_call(priv);
	table_proc_read(&r, sizeof(r));
	table_proc_end();

	return (r);
}

static void
table_proc_close(void *arg)
{
	struct table_proc_priv	*priv = arg;

	imsg_compose(&priv->ibuf, PROC_TABLE_CLOSE, 0, 0, -1, NULL, 0);
	imsg_flush(&priv->ibuf);
}

static int
imsg_add_params(struct ibuf *buf, struct dict *params)
{
	size_t count;
	const char *key;
	char *value;
	void *iter;

	count = 0;
	if (params)
		count = dict_count(params);

	if (imsg_add(buf, &count, sizeof(count)) == -1)
		return (-1);

	if (count == 0)
		return (0);

	iter = NULL;
	while (dict_iter(params, &iter, &key, (void **)&value)) {
		if (imsg_add(buf, key, strlen(key) + 1) == -1)
			return (-1);
		if (imsg_add(buf, value, strlen(value) + 1) == -1)
			return (-1);
	}

	return (0);
}

static int
table_proc_lookup(void *arg, struct dict *params, const char *k, enum table_service s,
    union lookup *lk)
{
	struct table_proc_priv	*priv = arg;
	struct ibuf		*buf;
	int			 r;

	buf = imsg_create(&priv->ibuf,
	    lk ? PROC_TABLE_LOOKUP : PROC_TABLE_CHECK, 0, 0,
	    sizeof(s) + strlen(k) + 1);

	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &s, sizeof(s)) == -1)
		return (-1);
	if (imsg_add_params(buf, params) == -1)
		return (-1);
	if (imsg_add(buf, k, strlen(k) + 1) == -1)
		return (-1);
	imsg_close(&priv->ibuf, buf);

	table_proc_call(priv);
	table_proc_read(&r, sizeof(r));

	if (r == 1 && lk) {
		if (rlen == 0) {
			log_warnx("warn: table-proc: empty response");
			fatalx("table-proc: exiting");
		}
		if (rdata[rlen - 1] != '\0') {
			log_warnx("warn: table-proc: not NUL-terminated");
			fatalx("table-proc: exiting");
		}
		r = table_parse_lookup(s, k, rdata, lk);
		table_proc_read(NULL, rlen);
	}

	table_proc_end();

	return (r);
}

static int
table_proc_fetch(void *arg, struct dict *params, enum table_service s, union lookup *lk)
{
	struct table_proc_priv	*priv = arg;
	struct ibuf		*buf;
	int			 r;

	buf = imsg_create(&priv->ibuf, PROC_TABLE_FETCH, 0, 0, sizeof(s));
	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &s, sizeof(s)) == -1)
		return (-1);
	if (imsg_add_params(buf, params) == -1)
		return (-1);
	imsg_close(&priv->ibuf, buf);

	table_proc_call(priv);
	table_proc_read(&r, sizeof(r));

	if (r == 1) {
		if (rlen == 0) {
			log_warnx("warn: table-proc: empty response");
			fatalx("table-proc: exiting");
		}
		if (rdata[rlen - 1] != '\0') {
			log_warnx("warn: table-proc: not NUL-terminated");
			fatalx("table-proc: exiting");
		}
		r = table_parse_lookup(s, NULL, rdata, lk);
		table_proc_read(NULL, rlen);
	}

	table_proc_end();

	return (r);
}

struct table_backend table_backend_proc = {
	K_ANY,
	NULL,
	table_proc_open,
	table_proc_update,
	table_proc_close,
	table_proc_lookup,
	table_proc_fetch,
};
