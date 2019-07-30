/*	$OpenBSD: uuencode.c,v 1.13 2015/10/09 01:37:09 deraadt Exp $	*/
/*	$FreeBSD: uuencode.c,v 1.18 2004/01/22 07:23:35 grehan Exp $	*/

/*-
 * Copyright (c) 1983, 1993
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

/*
 * Encode a file so it can be mailed to a remote system.
 */

#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <err.h>
#include <locale.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void encode(void);
void base64_encode(void);
static void usage(void);

FILE *output;
int mode;
char **av;

enum program_mode {
	MODE_ENCODE,
	MODE_B64ENCODE
} pmode;

int
main(int argc, char *argv[])
{
	struct stat sb;
	int base64, ch;
	char *outfile;
	extern char *__progname;
	static const char *optstr[2] = {
		"mo:",
		"o:"
	};

	base64 = 0;
	outfile = NULL;

	pmode = MODE_ENCODE;
	if (strcmp(__progname, "b64encode") == 0) {
		base64 = 1;
		pmode = MODE_B64ENCODE;
	}

	setlocale(LC_ALL, "");
	while ((ch = getopt(argc, argv, optstr[pmode])) != -1) {
		switch (ch) {
		case 'm':
			base64 = 1;
			break;
		case 'o':
			outfile = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	if (argc == 2 || outfile) {
		if (pledge("stdio rpath wpath cpath", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
	}

	switch(argc) {
	case 2:			/* optional first argument is input file */
		if (!freopen(*argv, "r", stdin) || fstat(fileno(stdin), &sb))
			err(1, "%s", *argv);
#define	RWX	(S_IRWXU|S_IRWXG|S_IRWXO)
		mode = sb.st_mode & RWX;
		++argv;
		break;
	case 1:
#define	RW	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
		mode = RW & ~umask(RW);
		break;
	case 0:
	default:
		usage();
	}

	av = argv;

	if (outfile != NULL) {
		output = fopen(outfile, "w+");
		if (output == NULL)
			err(1, "unable to open %s for output", outfile);
	} else
		output = stdout;
	if (base64)
		base64_encode();
	else
		encode();
	if (ferror(output))
		errx(1, "write error");
	exit(0);
}

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')

/*
 * Copy from in to out, encoding in base64 as you go along.
 */
void
base64_encode(void)
{
	/*
	 * Output must fit into 80 columns, chunks come in 4, leave 1.
	 */
#define	GROUPS	((80 / 4) - 1)
	unsigned char buf[3];
	char buf2[sizeof(buf) * 2 + 1];
	size_t n;
	int rv, sequence;

	sequence = 0;

	fprintf(output, "begin-base64 %o %s\n", mode, *av);
	while ((n = fread(buf, 1, sizeof(buf), stdin))) {
		++sequence;
		rv = b64_ntop(buf, n, buf2, (sizeof(buf2) / sizeof(buf2[0])));
		if (rv == -1)
			errx(1, "b64_ntop: error encoding base64");
		fprintf(output, "%s%s", buf2, (sequence % GROUPS) ? "" : "\n");
	}
	if (sequence % GROUPS)
		fprintf(output, "\n");
	fprintf(output, "====\n");
}

/*
 * Copy from in to out, encoding as you go along.
 */
void
encode(void)
{
	int ch, n;
	char *p;
	char buf[80];

	(void)fprintf(output, "begin %o %s\n", mode, *av);
	while ((n = fread(buf, 1, 45, stdin))) {
		ch = ENC(n);
		if (fputc(ch, output) == EOF)
			break;
		for (p = buf; n > 0; n -= 3, p += 3) {
			/* Pad with nulls if not a multiple of 3. */
			if (n < 3) {
				p[2] = '\0';
				if (n < 2)
					p[1] = '\0';
			}
			ch = *p >> 2;
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = ((*p << 4) & 060) | ((p[1] >> 4) & 017);
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = ((p[1] << 2) & 074) | ((p[2] >> 6) & 03);
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
			ch = p[2] & 077;
			ch = ENC(ch);
			if (fputc(ch, output) == EOF)
				break;
		}
		if (fputc('\n', output) == EOF)
			break;
	}
	if (ferror(stdin))
		errx(1, "read error");
	(void)fprintf(output, "%c\nend\n", ENC('\0'));
}

static void
usage(void)
{
	switch (pmode) {
	case MODE_ENCODE:
		(void)fprintf(stderr,
		    "usage: uuencode [-m] [-o output_file] [file] name\n");
		break;
	case MODE_B64ENCODE:
		(void)fprintf(stderr,
		    "usage: b64encode [-o output_file] [file] name\n");
		break;
	}
	exit(1);
}
