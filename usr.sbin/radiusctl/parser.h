/*	$OpenBSD: parser.h,v 1.2 2020/02/24 07:07:11 dlg Exp $	*/

/* This file is derived from OpenBSD:src/usr.sbin/ikectl/parser.h 1.9 */
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

#ifndef _RADIUSCTL_PARSER_H
#define _RADIUSCTL_PARSER_H

enum actions {
	NONE,
	TEST
};

enum auth_method {
	PAP,
	CHAP,
	MSCHAPV2
};

#define TEST_TRIES_MIN		1
#define TEST_TRIES_MAX		32
#define TEST_TRIES_DEFAULT	3

#define TEST_INTERVAL_MIN	1
#define TEST_INTERVAL_MAX	10
#define TEST_INTERVAL_DEFAULT	2

#define TEST_MAXWAIT_MIN	3
#define TEST_MAXWAIT_MAX	60
#define TEST_MAXWAIT_DEFAULT	8

struct parse_result {
	enum actions		 action;
	const char		*hostname;
	const char		*secret;
	const char		*username;
	const char		*password;
	u_short			 port;
	int			 nas_port;
	enum auth_method	 auth_method;

	/* number of packets to try sending */
	unsigned int		 tries;
	/* how long between packet sends */
	struct timeval		 interval;
	/* overall process wait time for a reply */
	struct timeval		 maxwait;
};

struct parse_result	*parse(int, char *[]);

#endif /* _RADIUSCTL_PARSER_H */
