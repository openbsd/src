/*	$OpenBSD: utils.h,v 1.8 2024/12/20 07:35:56 ratchov Exp $	*/
/*
 * Copyright (c) 2003-2012 Alexandre Ratchov <alex@caoua.org>
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

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define logx(n, ...)						\
	do {							\
		if (log_level >= (n))				\
			log_do(__VA_ARGS__);			\
	} while (0)

void panic(void);
void log_flush(void);
void log_do(const char *, ...) __attribute__((__format__ (printf, 1, 2)));

void *xmalloc(size_t);
char *xstrdup(char *);
void xfree(void *);

/*
 * Log levels:
 *
 * 0 - fatal errors: bugs, asserts, internal errors.
 * 1 - warnings: bugs in clients, failed allocations, non-fatal errors.
 * 2 - misc information (hardware parameters, incoming clients)
 * 3 - structural changes (eg. new streams, new parameters ...)
 * 4 - data blocks and messages
 */
extern unsigned int log_level;
extern unsigned int log_sync;

#endif
