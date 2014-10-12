/*	$OpenBSD: rfc2822.h,v 1.1 2014/10/12 16:19:30 gilles Exp $	*/

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

#ifndef _RFC2822_H_
#define	_RFC2822_H_

#define	RFC2822_MAX_LINE_SIZE		998

struct rfc2822_line {
	TAILQ_ENTRY(rfc2822_line)	next;
	char				buffer[RFC2822_MAX_LINE_SIZE+1];
};

struct rfc2822_header {
	char				name[RFC2822_MAX_LINE_SIZE+1];
	TAILQ_HEAD(, rfc2822_line)	lines;
};

struct rfc2822_hdr_cb {
	TAILQ_ENTRY(rfc2822_hdr_cb)	next;

	char				name[RFC2822_MAX_LINE_SIZE+1];
	void			      (*func)(const struct rfc2822_header *, void *);
	void			       *arg;
};

struct rfc2822_line_cb {
	void			      (*func)(const char *, void *);
	void			       *arg;
};

struct rfc2822_parser {
	uint8_t					in_hdrs;	/* in headers */

	TAILQ_HEAD(hdr_cb, rfc2822_hdr_cb)	hdr_cb;

	uint8_t					in_hdr;		/* in specific header */
	struct rfc2822_header			header;

	struct rfc2822_hdr_cb		        hdr_dflt_cb;
	struct rfc2822_line_cb		        body_line_cb;
};


void	rfc2822_parser_init(struct rfc2822_parser *);
int	rfc2822_parser_feed(struct rfc2822_parser *, const char *);
void	rfc2822_parser_reset(struct rfc2822_parser *);
void	rfc2822_parser_release(struct rfc2822_parser *);
int	rfc2822_header_callback(struct rfc2822_parser *, const char *,
    void (*)(const struct rfc2822_header *, void *), void *);
void	rfc2822_header_default_callback(struct rfc2822_parser *,
    void (*)(const struct rfc2822_header *, void *), void *);
void	rfc2822_body_callback(struct rfc2822_parser *,
    void (*)(const char *, void *), void *);

#endif
