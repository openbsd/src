/* $OpenBSD: doas.h,v 1.19 2021/11/30 20:08:15 tobias Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
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

struct rule {
	int action;
	int options;
	const char *ident;
	const char *target;
	const char *cmd;
	const char **cmdargs;
	const char **envlist;
};

extern struct rule **rules;
extern size_t nrules;
extern int parse_error;

extern const char *formerpath;

struct passwd;

char **prepenv(const struct rule *, const struct passwd *,
    const struct passwd *);

#define PERMIT	1
#define DENY	2

#define NOPASS		0x1
#define KEEPENV		0x2
#define PERSIST		0x4
#define NOLOG		0x8

#define AUTH_FAILED	-1
#define AUTH_OK		0
#define AUTH_RETRIES	3
