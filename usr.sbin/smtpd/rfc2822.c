/*	$OpenBSD: rfc2822.c,v 1.2 2014/10/15 19:23:29 gilles Exp $	*/

/*
 * Copyright (c) 2014 Gilles Chehade <gilles@poolp.org>
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rfc2822.h"

/* default no-op callbacks */
static void hdr_dflt_cb(const struct rfc2822_header *hdr, void *arg) {}
static void body_dflt_cb(const char *line, void *arg) {}

static void
header_reset(struct rfc2822_header *hdr)
{
	struct rfc2822_line	*line;

	while ((line = TAILQ_FIRST(&hdr->lines))) {
		TAILQ_REMOVE(&hdr->lines, line, next);
		free(line);
	}
}

static void
header_callback(struct rfc2822_parser *rp)
{
	struct rfc2822_hdr_cb	*hdr_cb;

	TAILQ_FOREACH(hdr_cb, &rp->hdr_cb, next)
	    if (strcasecmp(hdr_cb->name, rp->header.name) == 0) {
		    hdr_cb->func(&rp->header, hdr_cb->arg);
		    header_reset(&rp->header);
		    rp->in_hdr = 0;
		    return;
	    }

	rp->hdr_dflt_cb.func(&rp->header, rp->hdr_dflt_cb.arg);
	header_reset(&rp->header);
	rp->in_hdr = 0;
	return;
}

static void
body_callback(struct rfc2822_parser *rp, const char *line)
{
	rp->body_line_cb.func(line, rp->body_line_cb.arg);
}

static int
parser_feed_header(struct rfc2822_parser *rp, char *line)
{
	struct rfc2822_line	*hdrline;
	char			*pos;

	/* new header */
	if (! isspace(*line) && *line != '\0') {
		rp->in_hdr = 1;
		if ((pos = strchr(line, ':')) == NULL)
			return 0;
		memset(rp->header.name, 0, sizeof rp->header.name);
		(void)memcpy(rp->header.name, line, pos - line);
		return parser_feed_header(rp, pos + 1);
	}

	/* continuation */
	if (! rp->in_hdr)
		return 0;

	/* append line to header */
	if ((hdrline = calloc(1, sizeof *hdrline)) == NULL)
		return -1;
	(void)strlcpy(hdrline->buffer, line, sizeof hdrline->buffer);
	TAILQ_INSERT_TAIL(&rp->header.lines, hdrline, next);
	return 1;
}

static int
parser_feed_body(struct rfc2822_parser *rp, const char *line)
{
	/* for now, we only support per-line callbacks */
	body_callback(rp, line);
	return 1;
}


void
rfc2822_parser_init(struct rfc2822_parser *rp)
{
	memset(rp, 0, sizeof *rp);
	TAILQ_INIT(&rp->hdr_cb);
	TAILQ_INIT(&rp->header.lines);
	rfc2822_header_default_callback(rp, hdr_dflt_cb, NULL);
	rfc2822_body_callback(rp, body_dflt_cb, NULL);
}

void
rfc2822_parser_reset(struct rfc2822_parser *rp)
{
	header_reset(&rp->header);
	rp->in_hdrs = 1;
}

void
rfc2822_parser_release(struct rfc2822_parser *rp)
{
	struct rfc2822_hdr_cb	*cb;

	rfc2822_parser_reset(rp);
	while ((cb = TAILQ_FIRST(&rp->hdr_cb))) {
		TAILQ_REMOVE(&rp->hdr_cb, cb, next);
		free(cb);
	}
}

int
rfc2822_parser_feed(struct rfc2822_parser *rp, const char *line)
{
	char			buffer[RFC2822_MAX_LINE_SIZE+1];

	/* in header and line is not a continuation, execute callback */
	if (rp->in_hdr && (*line == '\0' || !isspace(*line)))
		header_callback(rp);

	/* no longer in headers */
	if (*line == '\0')
		rp->in_hdrs = 0;

	if (rp->in_hdrs) {
		/* line exceeds RFC maximum size requirement */
		if (strlcpy(buffer, line, sizeof buffer) >= sizeof buffer)
			return 0;
		return parser_feed_header(rp, buffer);
	}

	/* don't enforce line max length on content, too many MUA break */
	return parser_feed_body(rp, line);
}

int
rfc2822_header_callback(struct rfc2822_parser *rp, const char *header,
    void (*func)(const struct rfc2822_header *, void *), void *arg)
{
	struct rfc2822_hdr_cb  *cb;
	struct rfc2822_hdr_cb  *cb_tmp;
	char			buffer[RFC2822_MAX_LINE_SIZE+1];

	/* line exceeds RFC maximum size requirement */
	if (strlcpy(buffer, header, sizeof buffer) >= sizeof buffer)
		return 0;

	TAILQ_FOREACH_SAFE(cb, &rp->hdr_cb, next, cb_tmp) {
		if (strcasecmp(cb->name, buffer) == 0) {
			TAILQ_REMOVE(&rp->hdr_cb, cb, next);
			free(cb);
		}
	}

	if ((cb = calloc(1, sizeof *cb)) == NULL)
		return -1;
	(void)strlcpy(cb->name, buffer, sizeof cb->name);
	cb->func = func;
	cb->arg  = arg;
	TAILQ_INSERT_TAIL(&rp->hdr_cb, cb, next);
	return 1;
}

void
rfc2822_header_default_callback(struct rfc2822_parser *rp,
    void (*func)(const struct rfc2822_header *, void *), void *arg)
{
	struct rfc2822_hdr_cb	*cb;

	cb = &rp->hdr_dflt_cb;
	cb->func = func;
	cb->arg  = arg;
}

void
rfc2822_body_callback(struct rfc2822_parser *rp,
    void (*func)(const char *, void *), void *arg)
{
	struct rfc2822_line_cb	*cb;

	cb = &rp->body_line_cb;
	cb->func = func;
	cb->arg  = arg;
}

