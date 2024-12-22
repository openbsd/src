/*	$OpenBSD: utils.c,v 1.7 2024/12/22 14:17:45 ratchov Exp $	*/
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
/*
 * logx() quickly stores traces into a trace buffer.
 * This allows traces to be collected during time sensitive operations without
 * disturbing them. The buffer can be flushed on standard error later, when
 * slow syscalls are no longer disruptive, e.g. at the end of the poll() loop.
 */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"

/*
 * log buffer size
 */
#define LOG_BUFSZ	8192

char log_buf[LOG_BUFSZ];	/* buffer where traces are stored */
size_t log_used = 0;		/* bytes used in the buffer */
unsigned int log_sync = 1;	/* if true, flush after each '\n' */

/*
 * write the log buffer on stderr
 */
void
log_flush(void)
{
	if (log_used == 0)
		return;
	write(STDERR_FILENO, log_buf, log_used);
	log_used = 0;
}

/*
 * log a single line to stderr
 */
void
log_do(const char *fmt, ...)
{
	va_list ap;
	int n, save_errno = errno;

	va_start(ap, fmt);
	n = vsnprintf(log_buf + log_used, sizeof(log_buf) - log_used, fmt, ap);
	va_end(ap);

	if (n != -1) {
		log_used += n;

		if (log_used >= sizeof(log_buf))
			log_used = sizeof(log_buf) - 1;
		log_buf[log_used++] = '\n';

		if (log_sync)
			log_flush();
	}
	errno = save_errno;
}

/*
 * abort program execution after a fatal error
 */
void
panic(void)
{
	log_flush();
	(void)kill(getpid(), SIGABRT);
	_exit(1);
}

/*
 * allocate 'size' bytes of memory (with size > 0). This functions never
 * fails (and never returns NULL), if there isn't enough memory then
 * abort the program.
 */
void *
xmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL) {
		logx(0, "failed to allocate %zu bytes", size);
		panic();
	}
	return p;
}

/*
 * free memory allocated with xmalloc()
 */
void
xfree(void *p)
{
#ifdef DEBUG
	if (p == NULL) {
		logx(0, "xfree with NULL arg");
		panic();
	}
#endif
	free(p);
}

/*
 * xmalloc-style strdup(3)
 */
char *
xstrdup(char *s)
{
	size_t size;
	void *p;

	size = strlen(s) + 1;
	p = xmalloc(size);
	memcpy(p, s, size);
	return p;
}
