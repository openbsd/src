/*	$OpenBSD: odsyntax.c,v 1.19 2010/03/23 08:43:03 fgsch Exp $	*/
/*	$NetBSD: odsyntax.c,v 1.15 2001/12/07 15:14:29 bjh21 Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hexdump.h"

#define PADDING	"         "

int deprecated;

static void odoffset(int, char ***);
static void posixtypes(char *);
static void odadd(const char *);


/*
 * formats used for -t
 */
static const char *fmt[4][4] = {
	{
		"16/1 \"%3d \" \"\\n\"",
		"8/2  \"  %05d \" \"\\n\"",
		"4/4  \"     %010d \" \"\\n\"",
		"2/8  \" %019d \" \"\\n\""
	}, {
		"16/1 \"%03o \" \"\\n\"",
		"8/2  \" %06o \" \"\\n\"",
		"4/4  \"    %011o\" \"\\n\"",
		"2/8  \" %022o \" \"\\n\""
	}, {
		"16/1 \"%03u \" \"\\n\"",
		"8/2  \"  %05u \" \"\\n\"",
		"4/4  \"     %010u \" \"\\n\"",
		"2/8  \" %020u \" \"\\n\""
	}, {
		"16/1 \" %02x \" \"\\n\"",
		"8/2  \"   %04x \" \"\\n\"",
		"4/4  \"       %08x \" \"\\n\"",
		"2/8  \" %16x \" \"\\n\""
	}
};

void
oldsyntax(int argc, char ***argvp)
{
	static char empty[] = "", padding[] = PADDING;
	int ch;
	char *p, **argv;

#define	TYPE_OFFSET	7
	add("\"%07.7_Ao\n\"");
	add("\"%07.7_ao  \"");

	deprecated = 1;
	argv = *argvp;
	while ((ch = getopt(argc, argv,
	    "A:aBbcDdeFfHhIij:LlN:OoPpst:wvXx")) != -1)
		switch (ch) {
		case 'A':
			switch (*optarg) {
			case 'd': case 'o': case 'x':
				fshead->nextfu->fmt[TYPE_OFFSET] = *optarg;
				fshead->nextfs->nextfu->fmt[TYPE_OFFSET] =
				    *optarg;
				break;
			case 'n':
				fshead->nextfu->fmt = empty;
				fshead->nextfs->nextfu->fmt = padding;
				break;
			default:
				errx(1, "%s: invalid address base", optarg);
			}
			break;
		case 'a':
			odadd("16/1 \"%3_u \" \"\\n\"");
			break;
		case 'B':
		case 'o':
			odadd("8/2 \" %06o \" \"\\n\"");
			break;
		case 'b':
			odadd("16/1 \"%03o \" \"\\n\"");
			break;
		case 'c':
			odadd("16/1 \"%3_c \" \"\\n\"");
			break;
		case 'd':
			odadd("8/2 \"  %05u \" \"\\n\"");
			break;
		case 'D':
			odadd("4/4 \"     %010u \" \"\\n\"");
			break;
		case 'e':
		case 'F':
			odadd("2/8 \"          %21.14e \" \"\\n\"");
			break;
			
		case 'f':
			odadd("4/4 \" %14.7e \" \"\\n\"");
			break;
		case 'H':
		case 'X':
			odadd("4/4 \"       %08x \" \"\\n\"");
			break;
		case 'h':
		case 'x':
			odadd("8/2 \"   %04x \" \"\\n\"");
			break;
		case 'I':
		case 'L':
		case 'l':
			odadd("4/4 \"    %11d \" \"\\n\"");
			break;
		case 'i':
			odadd("8/2 \" %6d \" \"\\n\"");
			break;
		case 'j':
			if ((skip = strtol(optarg, &p, 0)) < 0)
				errx(1, "%s: bad skip value", optarg);
			switch(*p) {
			case 'b':
				skip *= 512;
				break;
			case 'k':
				skip *= 1024;
				break;
			case 'm':
				skip *= 1048576;
				break;
			}
			break;
		case 'N':
			if ((length = atoi(optarg)) < 0)
				errx(1, "%s: bad length value", optarg);
			break;
		case 'O':
			odadd("4/4 \"    %011o \" \"\\n\"");
			break;
		case 't':
			posixtypes(optarg);
			break;
		case 'v':
			vflag = ALL;
			break;
		case 'P':
		case 'p':
		case 's':
		case 'w':
		case '?':
		default:
			warnx("od(1) has been deprecated for hexdump(1).");
			if (ch != '?')
				warnx(
				    "hexdump(1) compatibility doesn't"
				    " support the -%c option%s",
				    ch, ch == 's' ? "; see strings(1)." : ".");
			oldusage();
		}

	if (fshead->nextfs->nextfs == NULL)
		odadd(" 8/2 \"%06o \" \"\\n\"");

	argc -= optind;
	*argvp += optind;

	if (argc)
		odoffset(argc, argvp);
}

/*
 * Interpret a POSIX-style -t argument.
 */
static void
posixtypes(char *type_string)
{
	int x, y, nbytes;

	while (*type_string) {
		switch (*type_string) {
		case 'a':
			type_string++;
			odadd("16/1 \"%3_u \" \"\\n\"");
			break;
		case 'c':
			type_string++;
			odadd("16/1 \"%3_c \" \"\\n\"");
			break;
		case 'f':
			type_string++;
			if        (*type_string == 'F' ||
				   *type_string == '4') {
				type_string++;
				odadd("4/4 \" %14.7e\" \"\\n\"");
			} else if (*type_string == 'L' ||
				   *type_string == '8') {
				type_string++;
				odadd("2/8 \" %16.14e\" \"\\n\"");
			} else if (*type_string == 'D')
				/* long doubles vary in size */
				oldusage();
			else
				odadd("2/8 \" %16.14e\" \"\\n\"");
			break;
		case 'd':
			x = 0;
			goto extensions;
		case 'o':
			x = 1;
			goto extensions;
		case 'u':
			x = 2;
			goto extensions;
		case 'x':
			x = 3;
		extensions:
			type_string++;
			y = 2;
			if (isupper(*type_string)) {
				switch(*type_string) {
				case 'C':
					nbytes = sizeof(char);
					break;
				case 'S':
					nbytes = sizeof(short);
					break;
				case 'I':
					nbytes = sizeof(int);
					break;
				case 'L':
					nbytes = sizeof(long);
					break;
				default:
					warnx("Bad type-size qualifier '%c'",
					    *type_string);
					oldusage();
				}
				type_string++;
			} else if (isdigit(*type_string))
				nbytes = strtol(type_string, &type_string, 10);
			else
				nbytes = 4;

			switch (nbytes) {
			case 1:
				y = 0;
				break;
			case 2:
				y = 1;
				break;
			case 4:
				y = 2;
				break;
			case 8:
				y = 3;
				break;
			default:
				warnx("%d-byte integer formats are not "
				    "supported", nbytes);
				oldusage();
			}
			odadd(fmt[x][y]);
			break;
		default:
			oldusage();
		}
	}
}

void
oldusage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-aBbcDdeFfHhIiLlOovXx] [-A base] "
	    "[-j offset] [-N length] [-t type_string]\n"
	    "\t[[+]offset[.][Bb]] [file ...]\n", __progname);
	exit(1);
}

static void
odoffset(int argc, char ***argvp)
{
	char *num, *p;
	int base;
	char *end;

	/*
	 * The offset syntax of od(1) was genuinely bizarre.  First, if
	 * it started with a plus it had to be an offset.  Otherwise, if
	 * there were at least two arguments, a number or lower-case 'x'
	 * followed by a number makes it an offset.  By default it was
	 * octal; if it started with 'x' or '0x' it was hex.  If it ended
	 * in a '.', it was decimal.  If a 'b' or 'B' was appended, it
	 * multiplied the number by 512 or 1024 byte units.  There was
	 * no way to assign a block count to a hex offset.
	 *
	 * We assume it's a file if the offset is bad.
	 */
	p = argc == 1 ? (*argvp)[0] : (*argvp)[1];
	if (!p)
		return;

	if (*p != '+' && (argc < 2 ||
	    (!isdigit((unsigned char)p[0]) &&
	    (p[0] != 'x' || !isxdigit((unsigned char)p[1])))))
		return;

	base = 0;
	/*
	 * skip over leading '+', 'x[0-9a-fA-f]' or '0x', and
	 * set base.
	 */
	if (p[0] == '+')
		++p;
	if (p[0] == 'x' && isxdigit((unsigned char)p[1])) {
		++p;
		base = 16;
	} else if (p[0] == '0' && p[1] == 'x') {
		p += 2;
		base = 16;
	}

	/* skip over the number */
	if (base == 16)
		for (num = p; isxdigit((unsigned char)*p); ++p);
	else
		for (num = p; isdigit((unsigned char)*p); ++p);

	/* check for no number */
	if (num == p)
		return;

	/* if terminates with a '.', base is decimal */
	if (*p == '.') {
		if (base)
			return;
		base = 10;
	}

	skip = strtol(num, &end, base ? base : 8);

	/* if end isn't the same as p, we got a non-octal digit */
	if (end != p) {
		skip = 0;
		return;
	}

	if (*p) {
		if (*p == 'B') {
			skip *= 1024;
			++p;
		} else if (*p == 'b') {
			skip *= 512;
			++p;
		}
	}
	if (*p) {
		skip = 0;
		return;
	}
	/*
	 * If the offset uses a non-octal base, the base of the offset
	 * is changed as well.  This isn't pretty, but it's easy.
	 */
	if (base == 16) {
		fshead->nextfu->fmt[TYPE_OFFSET] = 'x';
		fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'x';
	} else if (base == 10) {
		fshead->nextfu->fmt[TYPE_OFFSET] = 'd';
		fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'd';
	}

	/* Terminate file list. */
	(*argvp)[1] = NULL;
}

static void
odadd(const char *fmt)
{
	static int needpad;

	if (needpad)
		add("\""PADDING"\"");
	add(fmt);
	needpad = 1;
}
