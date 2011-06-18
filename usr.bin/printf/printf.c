/*	$OpenBSD: printf.c,v 1.18 2011/06/18 02:36:22 guenther Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <errno.h>
#include <err.h>

static int	 print_escape_str(const char *);
static int	 print_escape(const char *);

static int	 getchr(void);
static double	 getdouble(void);
static int	 getint(void);
static long	 getlong(void);
static unsigned long getulong(void);
static char	*getstr(void);
static char	*mklong(const char *, int); 
static void      check_conversion(const char *, const char *);
static void	 usage(void); 
     
static int	rval;
static char  **gargv;

#define isodigit(c)	((c) >= '0' && (c) <= '7')
#define octtobin(c)	((c) - '0')
#define hextobin(c)	((c) >= 'A' && (c) <= 'F' ? c - 'A' + 10 : (c) >= 'a' && (c) <= 'f' ? c - 'a' + 10 : c - '0')

#define PF(f, func) { \
	if (fieldwidth) \
		if (precision) \
			(void)printf(f, fieldwidth, precision, func); \
		else \
			(void)printf(f, fieldwidth, func); \
	else if (precision) \
		(void)printf(f, precision, func); \
	else \
		(void)printf(f, func); \
}

int
main(int argc, char *argv[])
{
	char *fmt, *start;
	int fieldwidth, precision;
	char convch, nextch;
	char *format;

	setlocale (LC_ALL, "");

	/* Need to accept/ignore "--" option. */
	if (argc > 1 && strcmp(argv[1], "--") == 0) {
		argc--;
		argv++;
	}

	if (argc < 2) {
		usage();
		return (1);
	}

	format = *++argv;
	gargv = ++argv;

#define SKIP1	"#-+ 0"
#define SKIP2	"0123456789"
	do {
		/*
		 * Basic algorithm is to scan the format string for conversion
		 * specifications -- once one is found, find out if the field
		 * width or precision is a '*'; if it is, gather up value. 
		 * Note, format strings are reused as necessary to use up the
		 * provided arguments, arguments of zero/null string are 
		 * provided to use up the format string.
		 */

		/* find next format specification */
		for (fmt = format; *fmt; fmt++) {
			switch (*fmt) {
			case '%':
				start = fmt++;

				if (*fmt == '%') {
					putchar ('%');
					break;
				} else if (*fmt == 'b') {
					char *p = getstr();
					if (print_escape_str(p)) {
						return (rval);
					}
					break;
				}

				/* skip to field width */
				for (; strchr(SKIP1, *fmt); ++fmt)
					;
				if (*fmt == '*') {
					++fmt;
					fieldwidth = getint();
				} else
					fieldwidth = 0;

				/* skip to field precision */
				for (; strchr(SKIP2, *fmt); ++fmt)
					;
				precision = 0;
				if (*fmt == '.') {
					++fmt;
					if (*fmt == '*') {
						++fmt;
						precision = getint();
					}
					for (; strchr(SKIP2, *fmt); ++fmt)
						;
				}

				if (!*fmt) {
					warnx ("missing format character");
					return(1);
				}

				convch = *fmt;
				nextch = *(fmt + 1);
				*(fmt + 1) = '\0';
				switch(convch) {
				case 'c': {
					char p = getchr();
					PF(start, p);
					break;
				}
				case 's': {
					char *p = getstr();
					PF(start, p);
					break;
				}
				case 'd':
				case 'i': {
					long p;
					char *f = mklong(start, convch);
					if (!f) {
						warnx("out of memory");
						return (1);
					}
					p = getlong();
					PF(f, p);
					break;
				}
				case 'o':
				case 'u':
				case 'x':
				case 'X': {
					unsigned long p;
					char *f = mklong(start, convch);
					if (!f) {
						warnx("out of memory");
						return (1);
					}
					p = getulong();
					PF(f, p);
					break;
				}
				case 'a':
				case 'A':
				case 'e':
				case 'E':
				case 'f':
				case 'F':
				case 'g':
				case 'G': {
					double p = getdouble();
					PF(start, p);
					break;
				}
				default:
					warnx ("%s: invalid directive", start);
					return(1);
				}
				*(fmt + 1) = nextch;
				break;

			case '\\':
				fmt += print_escape(fmt);
				break;

			default:
				putchar (*fmt);
				break;
			}
		}
	} while (gargv > argv && *gargv);

	return (rval);
}


/*
 * Print SysV echo(1) style escape string 
 *	Halts processing string and returns 1 if a \c escape is encountered.
 */
static int
print_escape_str(const char *str)
{
	int value;
	int c;

	while (*str) {
		if (*str == '\\') {
			str++;
			/* 
			 * %b string octal constants are not like those in C.
			 * They start with a \0, and are followed by 0, 1, 2, 
			 * or 3 octal digits. 
			 */
			if (*str == '0') {
				str++;
				for (c = 3, value = 0; c-- && isodigit(*str); str++) {
					value <<= 3;
					value += octtobin(*str);
				}
				putchar (value);
				str--;
			} else if (*str == 'c') {
				return 1;
			} else {
				str--;			
				str += print_escape(str);
			}
		} else {
			putchar (*str);
		}
		str++;
	}

	return 0;
}

/*
 * Print "standard" escape characters 
 */
static int
print_escape(const char *str)
{
	const char *start = str;
	int value;
	int c;

	str++;

	switch (*str) {
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		for (c = 3, value = 0; c-- && isodigit(*str); str++) {
			value <<= 3;
			value += octtobin(*str);
		}
		putchar(value);
		return str - start - 1;
		/* NOTREACHED */

	case 'x':
		str++;
		for (value = 0; isxdigit(*str); str++) {
			value <<= 4;
			value += hextobin(*str);
		}
		if (value > UCHAR_MAX) {
			warnx ("escape sequence out of range for character");
			rval = 1;
		}
		putchar (value);
		return str - start - 1;
		/* NOTREACHED */

	case '\\':			/* backslash */
		putchar('\\');
		break;

	case '\'':			/* single quote */
		putchar('\'');
		break;

	case '"':			/* double quote */
		putchar('"');
		break;

	case 'a':			/* alert */
		putchar('\a');
		break;

	case 'b':			/* backspace */
		putchar('\b');
		break;

	case 'e':			/* escape */
#ifdef __GNUC__
		putchar('\e');
#else
		putchar(033);
#endif
		break;

	case 'f':			/* form-feed */
		putchar('\f');
		break;

	case 'n':			/* newline */
		putchar('\n');
		break;

	case 'r':			/* carriage-return */
		putchar('\r');
		break;

	case 't':			/* tab */
		putchar('\t');
		break;

	case 'v':			/* vertical-tab */
		putchar('\v');
		break;

	case '\0':
		warnx("null escape sequence");
		rval = 1;
		return 0;

	default:
		putchar(*str);
		warnx("unknown escape sequence `\\%c'", *str);
		rval = 1;
	}

	return 1;
}

static char *
mklong(const char *str, int ch)
{
	static char *copy;
	static int copysize;
	int len;	

	len = strlen(str) + 2;
	if (copysize < len) {
		char *newcopy;
		copysize = len + 256;

		newcopy = realloc(copy, copysize);
		if (newcopy == NULL) {
			copysize = 0;
			free(copy);
			copy = NULL;
			return (NULL);
		}
		copy = newcopy;
	}
	(void) memmove(copy, str, len - 3);
	copy[len - 3] = 'l';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return (copy);	
}

static int
getchr(void)
{
	if (!*gargv)
		return((int)'\0');
	return((int)**gargv++);
}

static char *
getstr(void)
{
	if (!*gargv)
		return("");
	return(*gargv++);
}

static char *number = "+-.0123456789";
static int
getint(void)
{
	if (!*gargv)
		return(0);

	if (strchr(number, **gargv))
		return(atoi(*gargv++));

	return 0;
}

static long
getlong(void)
{
	long val;
	char *ep;

	if (!*gargv)
		return(0L);

	if (**gargv == '\"' || **gargv == '\'')
		return (long) *((*gargv++)+1);

	errno = 0;
	val = strtol (*gargv, &ep, 0);
	check_conversion(*gargv++, ep);
	return val;
}

static unsigned long
getulong(void)
{
	unsigned long val;
	char *ep;

	if (!*gargv)
		return(0UL);

	if (**gargv == '\"' || **gargv == '\'')
		return (unsigned long) *((*gargv++)+1);

	errno = 0;
	val = strtoul (*gargv, &ep, 0);
	check_conversion(*gargv++, ep);
	return val;
}

static double
getdouble(void)
{
	double val;
	char *ep;

	if (!*gargv)
		return(0.0);

	if (**gargv == '\"' || **gargv == '\'')
		return (double) *((*gargv++)+1);

	errno = 0;
	val = strtod (*gargv, &ep);
	check_conversion(*gargv++, ep);
	return val;
}

static void
check_conversion(const char *s, const char *ep)
{
	if (*ep) {
		if (ep == s)
			warnx ("%s: expected numeric value", s);
		else
			warnx ("%s: not completely converted", s);
		rval = 1;
	} else if (errno == ERANGE) {
		warnx ("%s: %s", s, strerror(ERANGE));
		rval = 1;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: printf format [arg ...]\n");
}
