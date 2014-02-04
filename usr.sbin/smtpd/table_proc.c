/*	$OpenBSD: table_proc.c,v 1.2 2014/02/04 13:55:34 eric Exp $	*/

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
#include <sys/param.h>
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
	int			 sp[2];
	struct table_proc_priv	*priv;
	char			*environ_new[2];
	struct table_open_params op;

	errno = 0;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) < 0) {
		log_warn("warn: table-proc: socketpair");
		return (NULL);
	}
	priv = xcalloc(1, sizeof(*priv), "table_proc_open");

	if ((priv->pid = fork()) == -1) {
		log_warn("warn: table-proc: fork");
		goto err;
	}

	if (priv->pid == 0) {
		/* child process */
		dup2(sp[0], STDIN_FILENO);
		if (closefrom(STDERR_FILENO + 1) < 0)
			exit(1);

		environ_new[0] = "PATH=" _PATH_DEFPATH;
		environ_new[1] = (char *)NULL;
		environ = environ_new;
		execle("/bin/sh", "/bin/sh", "-c", table->t_config, (char *)NULL,
		    environ_new);
		fatal("execl");
	}

	/* parent process */
	close(sp[0]);
	imsg_init(&priv->ibuf, sp[1]);

	memset(&op, 0, sizeof op);
	op.version = PROC_TABLE_API_VERSION;
	(void)strlcpy(op.name, table->t_name, sizeof op.name);
	imsg_compose(&priv->ibuf, PROC_TABLE_OPEN, 0, 0, -1, &op, sizeof op);

	table_proc_call(priv);
	table_proc_end();

	return (priv);
err:
	free(priv);
	close(sp[0]);
	close(sp[1]);
	return (NULL);
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
table_proc_lookup(void *arg, const char *k, enum table_service s,
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
table_proc_fetch(void *arg, enum table_service s, union lookup *lk)
{
	struct table_proc_priv	*priv = arg;
	struct ibuf		*buf;
	int			 r;

	buf = imsg_create(&priv->ibuf, PROC_TABLE_FETCH, 0, 0, sizeof(s));
	if (buf == NULL)
		return (-1);
	if (imsg_add(buf, &s, sizeof(s)) == -1)
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
