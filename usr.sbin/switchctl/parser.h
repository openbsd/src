/*	$OpenBSD: parser.h,v 1.4 2016/11/24 09:23:11 reyk Exp $	*/

/*
 * Copyright (c) 2007-2015 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _SWITCHCTL_PARSER_H
#define _SWITCHCTL_PARSER_H

enum actions {
	NONE,
	FLOW_ADD,
	FLOW_DELETE,
	FLOW_MODIFY,
	DUMP_DESC,
	DUMP_FEATURES,
	DUMP_FLOWS,
	DUMP_TABLES,
	SHOW_SUM,
	SHOW_SWITCHES,
	SHOW_MACS,
	LOAD,
	RELOAD,
	MONITOR,
	LOG_VERBOSE,
	LOG_BRIEF,
	RESETALL,
	CONNECT,
	DISCONNECT
};

struct parse_result {
	enum actions		 action;
	struct imsgbuf		*ibuf;
	char			*path;
	struct switch_address	 uri;
	struct sockaddr_storage	 addr;
	struct oflowmod_ctx	 fctx;
	struct ibuf		*fbuf;
	int			 table;
	int			 quiet;
	int			 verbose;
};

#define HOST_IPADDR	1
#define HOST_FQDN	2

struct parse_result	*parse(int, char *[]);
void			 ofpclient(struct parse_result *, struct passwd *);

#endif /* _SWITCHCTL_PARSER_H */
