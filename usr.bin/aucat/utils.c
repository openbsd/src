/*	$OpenBSD: utils.c,v 1.1 2015/01/21 08:43:55 ratchov Exp $	*/
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
 * log_xxx() routines are used to quickly store traces into a trace buffer.
 * This allows trances to be collected during time sensitive operations without
 * disturbing them. The buffer can be flushed on standard error later, when
 * slow syscalls are no longer disruptive, e.g. at the end of the poll() loop.
 */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"

/*
 * log buffer size
 */
#define LOG_BUFSZ	8192

/*
 * store a character in the log
 */
#define LOG_PUTC(c) do {			\
	if (log_used < LOG_BUFSZ)		\
		log_buf[log_used++] = (c);	\
} while (0)

char log_buf[LOG_BUFSZ];	/* buffer where traces are stored */
unsigned int log_used = 0;	/* bytes used in the buffer */
unsigned int log_sync = 1;	/* if true, flush after each '\n' */

/*
 * write the log buffer on stderr
 */
void
log_flush(void)
{
	if (log_used ==  0)
		return;
	write(STDERR_FILENO, log_buf, log_used);
	log_used = 0;
}

/*
 * store a string in the log
 */
void
log_puts(char *msg)
{
	char *p = msg;
	int c;

	while ((c = *p++) != '\0') {
		LOG_PUTC(c);
		if (log_sync && c == '\n')
			log_flush();
	}
}

/*
 * store a hex in the log
 */
void
log_putx(unsigned long num)
{
	char dig[sizeof(num) * 2], *p = dig, c;
	unsigned int ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num & 0xf;
			num >>= 4;
		}
		for (; ndig != 0; ndig--) {
			c = *(--p);
			c += (c < 10) ? '0' : 'a' - 10;
			LOG_PUTC(c);
		}
	} else 
		LOG_PUTC('0');
}

/*
 * store a unsigned decimal in the log
 */
void
log_putu(unsigned long num)
{
	char dig[sizeof(num) * 3], *p = dig;
	unsigned int ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num % 10;
			num /= 10;
		}
		for (; ndig != 0; ndig--)
			LOG_PUTC(*(--p) + '0');
	} else
		LOG_PUTC('0');
}

/*
 * store a signed decimal in the log
 */
void
log_puti(long num)
{
	if (num < 0) {
		LOG_PUTC('-');
		num = -num;
	}
	log_putu(num);
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
 * allocate a (small) abount of memory, and abort if it fails
 */
void *
xmalloc(size_t size)
{
	void *p;
	
	p = malloc(size);
	if (p == NULL) {
		log_puts("failed to allocate ");
		log_putx(size);
		log_puts(" bytes\n");
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
