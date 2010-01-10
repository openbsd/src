#ifdef DEBUG
/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 	- Redistributions of source code must retain the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer in the documentation and/or other
 * 	  materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
