/*	$OpenBSD: binedit.c,v 1.1 2016/07/30 10:56:13 schwarze Exp $ */
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
#include <ctype.h>
#include <endian.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>

static int32_t	 getint(const char **);
static int	 copybyte(const char);


int
main(int argc, char *argv[])
{
	const char	*cmd;	/* Command string from the command line. */
	int32_t		 pos;	/* Characters read so far. */
	int32_t		 dest;	/* Number of characters to be read. */
	int32_t		 val;	/* Value to be written. */
	int32_t		 i;	/* Auxiliary for reading and writing. */

	if (argc != 2)
		errx(1, "usage: binedit command_string");
	cmd = argv[1];
	dest = pos = val = 0;
	while (*cmd != '\0') {
		switch (*cmd++) {
		case 'a':  /* Advance to destination. */
			while (pos < dest) {
				pos++;
				if (copybyte('a') == EOF)
					errx(1, "a: EOF");
			}
			break;
		case 'c':  /* Copy. */
			i = getint(&cmd);
			pos += i;
			while (i--)
				if (copybyte('c') == EOF)
					errx(1, "c: EOF");
			break;
		case 'd':  /* Set destination. */
			dest = val;
			break;
		case 'f':  /* Finish. */
			if (*cmd != '\0')
				errx(1, "%s: not the last command", cmd - 1);
			while (copybyte('f') != EOF)
				continue;
			break;
		case 'i':  /* Increment. */
			i = getint(&cmd);
			if (i == 0)
				i = 1;
			val += i;
			break;
		case 'r':  /* Read. */
			pos += sizeof(i);
			if (fread(&i, sizeof(i), 1, stdin) != 1) {
				if (ferror(stdin))
					err(1, "r: fread");
				else
					errx(1, "r: EOF");
			}
			val = be32toh(i);
			break;
		case 's':  /* Skip. */
			i = getint(&cmd);
			pos += i;
			while (i--) {
				if (getchar() == EOF) {
					if (ferror(stdin))
						err(1, "s: getchar");
					else
						errx(1, "s: EOF");
				}
			}
			break;
		case 'w':  /* Write one integer. */
			if (*cmd == '-' || *cmd == '+' ||
			    isdigit((unsigned char)*cmd))
				val = getint(&cmd);
			i = htobe32(val);
			if (fwrite(&i, sizeof(i), 1, stdout) != 1)
				err(1, "w: fwrite");
			break;
		default:
			errx(1, "%c: invalid command", cmd[-1]);
		}
	}
	return 0;
}

static int32_t
getint(const char **cmd)
{
	int32_t	 res;
	int	 minus;

	res = 0;
	minus = 0;
	if (**cmd == '-') {
		minus = 1;
		(*cmd)++;
	} else if (**cmd == '+')
		(*cmd)++;
	while(isdigit((unsigned char)**cmd))
		res = res * 10 + *(*cmd)++ - '0';
	return minus ? -res : res;
}

static int
copybyte(const char cmd)
{
	int	 ch;

	if ((ch = getchar()) == EOF) {
		if (ferror(stdin))
			err(1, "%c: getchar", cmd);
	} else if (putchar(ch) == EOF)
		err(1, "%c: putchar", cmd);
	return ch;
}
