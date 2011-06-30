#ifdef DEBUG
/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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
 * dbg_xxx() routines are used to quickly store traces into a trace buffer.
 * This allows trances to be collected during time sensitive operations without
 * disturbing them. The buffer can be flushed on standard error later, when
 * slow syscalls are no longer disruptive, e.g. at the end of the poll() loop.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "dbg.h"

/*
 * size of the buffer where traces are stored
 */
#define DBG_BUFSZ	8192

/*
 * store a character in the trace buffer
 */
#define DBG_PUTC(c) do {			\
	if (dbg_used < DBG_BUFSZ)		\
		dbg_buf[dbg_used++] = (c);	\
} while (0)

char dbg_buf[DBG_BUFSZ];	/* buffer where traces are stored */
unsigned dbg_used = 0;		/* bytes used in the buffer */
unsigned dbg_sync = 1;		/* if true, flush after each '\n' */

/*
 * write debug info buffer on stderr
 */
void
dbg_flush(void)
{
	if (dbg_used ==  0)
		return;
	write(STDERR_FILENO, dbg_buf, dbg_used);
	dbg_used = 0;
}

/*
 * store a string in the debug buffer
 */
void
dbg_puts(char *msg)
{
	char *p = msg;
	int c;

	while ((c = *p++) != '\0') {
		DBG_PUTC(c);
		if (dbg_sync && c == '\n')
			dbg_flush();
	}
}

/*
 * store a hex in the debug buffer
 */
void
dbg_putx(unsigned long num)
{
	char dig[sizeof(num) * 2], *p = dig, c;
	unsigned ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num & 0xf;
			num >>= 4;
		}
		for (; ndig != 0; ndig--) {
			c = *(--p);
			c += (c < 10) ? '0' : 'a' - 10;
			DBG_PUTC(c);
		}
	} else 
		DBG_PUTC('0');
}

/*
 * store a decimal in the debug buffer
 */
void
dbg_putu(unsigned long num)
{
	char dig[sizeof(num) * 3], *p = dig;
	unsigned ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num % 10;
			num /= 10;
		}
		for (; ndig != 0; ndig--)
			DBG_PUTC(*(--p) + '0');
	} else
		DBG_PUTC('0');
}

/*
 * store a signed integer in the trace buffer
 */
void
dbg_puti(long num)
{
	if (num < 0) {
		DBG_PUTC('-');
		num = -num;
	}
	dbg_putu(num);
}

/*
 * abort program execution after a fatal error, we should
 * put code here to backup user data
 */
void
dbg_panic(void)
{
	dbg_flush();
	abort();
}
#endif
