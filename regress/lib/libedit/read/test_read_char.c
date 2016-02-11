/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include "read.c"

/*
 * Glue for unit tests of libedit/read.c.
 * Rather than linking in all the various libedit modules,
 * provide dummies for those functions called in read.c.
 * Most aren't actually called in read_char().
 * Requires "make obj && make depend" in src/lib/libedit.
 */

#define EL EditLine *el __attribute__((__unused__))
#define UU __attribute__((__unused__))

int ch_enlargebufs(EL, size_t addlen UU) { return 1; }
void ch_reset(EL, int mclear UU) { }
void el_resize(EL) { }
int el_set(EL, int op UU, ...) { return 0; }
void re_clear_display(EL) { }
void re_clear_lines(EL) { }
void re_refresh(EL) { }
void re_refresh_cursor(EL) { }
void sig_clr(EL) { }
void sig_set(EL) { }
void terminal__flush(EL) { }
void terminal_beep(EL) { }
int tty_cookedmode(EL) { return 0; }
int tty_rawmode(EL) { return 0; }

int
keymacro_get(EL, Char *ch, keymacro_value_t *val)
{
	val->str = NULL;
	*ch = '\0';
	return XK_STR;
}

#undef EL
#undef UU

/*
 * Unit test steering program for editline/read.c, read_char().
 * Reads from standard input until read_char() returns 0.
 * Writes the code points read to standard output in %x format.
 * If EILSEQ is set after read_char(), indicating that there were some
 * garbage bytes before the character, the code point gets * prefixed.
 * The return value is indicated by appending to the code point:
 * a comma for 1, a full stop for 0, [%d] otherwise.
 * Errors out on unexpected failure (setlocale failure, malloc
 * failure, or unexpected errno).
 * Since ENOMSG is very unlikely to occur, it is used to make
 * sure that read_char() doesn't clobber errno.
 */

int
main(void)
{
	EditLine el;
	int irc;
	Char cp;

	if (setlocale(LC_CTYPE, "") == NULL)
		err(1, "setlocale");
	el.el_flags = CHARSET_IS_UTF8;
	el.el_infd = STDIN_FILENO;
	if ((el.el_signal = calloc(1, sizeof(*el.el_signal))) == NULL)
		err(1, NULL);
	do {
		errno = ENOMSG;
		irc = read_char(&el, &cp);
		switch (errno) {
		case ENOMSG:
			break;
		case EILSEQ:
			putchar('*');
			break;
		default:
			err(1, NULL);
		}
		printf("%x", cp);
		switch (irc) {
		case 1:
			putchar(',');
			break;
		case 0:
			putchar('.');
			break;
		default:
			printf("[%d]", irc);
			break;
		}
	} while (irc != 0);
	return 0;
}
