/*	$OpenBSD: safe_sprintf.c,v 1.1 1997/12/03 05:21:43 millert Exp $	*/

/******************************************************************************
 * Copyright 1997 by Thomas E. Dickey <dickey@clark.net>                      *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("Id: safe_sprintf.c,v 1.4 1997/11/01 23:01:09 tom Exp $")

#if USE_SAFE_SPRINTF

typedef enum { Flags, Width, Prec, Type, Format } PRINTF;

#define VA_INTGR(type) ival = va_arg(ap, type)
#define VA_FLOAT(type) fval = va_arg(ap, type)
#define VA_POINT(type) pval = (void *)va_arg(ap, type)

/*
 * Scan a variable-argument list for printf to determine the number of
 * characters that would be emitted.
 */
static int
_nc_printf_length(const char *fmt, va_list ap)
{
	size_t length = BUFSIZ;
	char *buffer;
	char *format;
	int len = 0;

	if (fmt == 0 || *fmt == '\0')
		return -1;
	if ((format = malloc(strlen(fmt)+1)) == 0)
		return -1;
	if ((buffer = malloc(length)) == 0) {
		free(format);
		return -1;
	}

	while (*fmt != '\0') {
		if (*fmt == '%') {
			PRINTF state = Flags;
			char *pval   = "";
			double fval  = 0.0;
			int done     = FALSE;
			int ival     = 0;
			int prec     = -1;
			int type     = 0;
			int used     = 0;
			int width    = -1;
			size_t f     = 0;

			format[f++] = *fmt;
			while (*++fmt != '\0' && len >= 0 && !done) {
				format[f++] = *fmt;

				if (isdigit(*fmt)) {
					int num = *fmt - '0';
					if (state == Flags && num != 0)
						state = Width;
					if (state == Width) {
						if (width < 0)
							width = 0;
						width = (width * 10) + num;
					} else if (state == Prec) {
						if (prec < 0)
							prec = 0;
						prec = (prec * 10) + num;
					}
				} else if (*fmt == '*') {
					VA_INTGR(int);
					if (state == Flags)
						state = Width;
					if (state == Width) {
						width = ival;
					} else if (state == Prec) {
						prec = ival;
					}
					sprintf(&format[--f], "%d", ival);
					f = strlen(format);
				} else if (isalpha(*fmt)) {
					done = TRUE;
					switch (*fmt) {
					case 'Z': /* FALLTHRU */
					case 'h': /* FALLTHRU */
					case 'l': /* FALLTHRU */
					case 'L': /* FALLTHRU */
						done = FALSE;
						type = *fmt;
						break;
					case 'i': /* FALLTHRU */
					case 'd': /* FALLTHRU */
					case 'u': /* FALLTHRU */
					case 'x': /* FALLTHRU */
					case 'X': /* FALLTHRU */
						if (type == 'l')
							VA_INTGR(long);
						else if (type == 'Z')
							VA_INTGR(size_t);
						else
							VA_INTGR(int);
						used = 'i';
						break;
					case 'f': /* FALLTHRU */
					case 'e': /* FALLTHRU */
					case 'E': /* FALLTHRU */
					case 'g': /* FALLTHRU */
					case 'G': /* FALLTHRU */
						if (type == 'L')
							VA_FLOAT(long double);
						else
							VA_FLOAT(double);
						used = 'f';
						break;
					case 'c':
						VA_INTGR(int);
						used = 'i';
						break;
					case 's':
						VA_POINT(char *);
						if (prec < 0)
							prec = strlen(pval);
						if (prec > (int)length) {
							length = length + prec;
							buffer = realloc(buffer, length);
							if (buffer == 0) {
								free(format);
								return -1;
							}
						}
						used = 'p';
						break;
					case 'p':
						VA_POINT(void *);
						used = 'p';
						break;
					case 'n':
						VA_POINT(int *);
						used = 0;
						break;
					default:
						break;
					}
				} else if (*fmt == '.') {
					state = Prec;
				} else if (*fmt == '%') {
					done = TRUE;
					used = 'p';
				}
			}
			format[f] = '\0';
			switch (used) {
			case 'i':
				sprintf(buffer, format, ival);
				break;
			case 'f':
				sprintf(buffer, format, fval);
				break;
			default:
				sprintf(buffer, format, pval);
				break;
			}
			len += (int)strlen(buffer);
		} else {
			fmt++;
			len++;
		}
	}

	free(buffer);
	free(format);
	return len;
}
#endif

/*
 * Wrapper for vsprintf that allocates a buffer big enough to hold the result.
 */
char *
_nc_printf_string(const char *fmt, va_list ap)
{
#if USE_SAFE_SPRINTF
	char *buf = 0;
	int len = _nc_printf_length(fmt, ap);

	if (len > 0) {
		buf = malloc(len+1);
		vsprintf(buf, fmt, ap);
	}
#else
	static int rows, cols;
	static char *buf;
	static size_t len;

	if (screen_lines > rows || screen_columns > cols) {
		if (screen_lines   > rows) rows = screen_lines;
		if (screen_columns > cols) cols = screen_columns;
		len = (rows * (cols + 1)) + 1;
		if (buf == 0)
			buf = malloc(len);
		else
			buf = realloc(buf, len);
	}

	if (buf != 0) {
# if HAVE_VSNPRINTF
		vsnprintf(buf, len, fmt, ap);	/* GNU extension */
# else
		vsprintf(buf, fmt, ap);		/* ANSI */
# endif
#endif
	}
	return buf;
}
