/*	$OpenBSD: parser.h,v 1.4 2010/06/14 17:41:18 jsg Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
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

enum actions {
	NONE,
	LOAD,
	RELOAD,
	MONITOR,
	LOG_VERBOSE,
	LOG_BRIEF,
	COUPLE,
	DECOUPLE,
	ACTIVE,
	PASSIVE,
	RESETALL,
	RESETCA,
	RESETPOLICY,
	RESETSA,
	RESETUSER,
	CA,
	CA_CREATE,
	CA_DELETE,
	CA_INSTALL,
	CA_CERTIFICATE,
	CA_CERT_CREATE,
	CA_CERT_DELETE,
	CA_CERT_INSTALL,
	CA_CERT_EXPORT,
	CA_CERT_REVOKE,
	CA_KEY_CREATE,
	CA_KEY_DELETE,
	CA_KEY_INSTALL,
	CA_KEY_IMPORT,
	SHOW_CA,
	SHOW_CA_CERTIFICATES
};

struct parse_result {
	enum actions	 action;
	struct imsgbuf	*ibuf;
	char		*filename;
	char 		*caname;
	char		*host;
	int		 htype;
};

#define HOST_IPADDR	1
#define HOST_FQDN	2

struct parse_result	*parse(int, char *[]);
