/*	$OpenBSD: diff.c,v 1.21 2003/07/04 17:50:24 millert Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"
#include "pathnames.h"

#if 0
static char const sccsid[] = "@(#)diff.c 4.7 5/11/89";
#endif

/*
 * diff - driver and subroutines
 */
int	opt;
int	aflag;			/* treat all files as text */
int	tflag;			/* expand tabs on output */
/* Algorithm related options. */
int	bflag;			/* ignore blanks in comparisons */
int	wflag;			/* totally ignore blanks in comparisons */
int	iflag;			/* ignore case in comparisons */
/* Options on hierarchical diffs. */
int	rflag;			/* recursively trace directories */
int	sflag;			/* announce files which are same */
char	*start;			/* do file only if name >= this */
/* Variable for -D D_IFDEF option. */
char	*ifdefname;		/* What we will print for #ifdef/#endif */
/* Variables for -c and -u context option. */
int	context;		/* lines of context to be printed */
/* State for exit status. */
int	status;
int	anychange;
/* Variables for diffdir. */
char	**diffargv;		/* option list to pass to recursive diffs */

/*
 * Input file names.
 * With diffdir, file1 and file2 are allocated MAXPATHLEN space,
 * and padded with a '/', and then efile1 and efile2 point after
 * the '/'.
 */
char	*file1, *file2, *efile1, *efile2;
struct	stat stb1, stb2;

__dead void usage(void);

int
main(int argc, char **argv)
{
	int ch;

	status = 2;
	diffargv = argv;

	while ((ch = getopt(argc, argv, "abC:cD:efinrS:stU:uw")) != -1) {
		switch (ch) {
		case 'a':
			aflag++;
			break;
		case 'b':
			bflag++;
			break;
		case 'C':
			opt = D_CONTEXT;
			if (!isdigit(*optarg))
				usage();
			context = atoi(optarg);	/* XXX - use strtol */
			break;
		case 'c':
			opt = D_CONTEXT;
			context = 3;
			break;
		case 'D':
			opt = D_IFDEF;
			ifdefname = optarg;
			break;
		case 'e':
			opt = D_EDIT;
			break;
		case 'f':
			opt = D_REVERSE;
			break;
		case 'i':
			iflag++;
			break;
		case 'n':
			opt = D_NREVERSE;
			break;
		case 'r':
			rflag++;
			break;
		case 'S':
			start = optarg;
			break;
		case 's':
			sflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'U':
			opt = D_UNIFIED;
			if (!isdigit(*optarg))
				usage();
			context = atoi(optarg);	/* XXX - use strtol */
			break;
		case 'u':
			opt = D_UNIFIED;
			context = 3;
			break;
		case 'w':
			wflag++;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		errorx("two filename arguments required");
	file1 = argv[0];
	file2 = argv[1];
	if (!strcmp(file1, "-"))
		stb1.st_mode = S_IFREG;
	else if (stat(file1, &stb1) < 0)
		error("%s", file1);
	if (!strcmp(file2, "-"))
		stb2.st_mode = S_IFREG;
	else if (stat(file2, &stb2) < 0)
		error("%s", file2);
	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode))
		diffdir(argv);
	else
		diffreg();
	done(0);
}

int
min(int a, int b)
{

	return (a < b ? a : b);
}

int
max(int a, int b)
{

	return (a > b ? a : b);
}

__dead void
done(int sig)
{
	if (tempfiles[0] != NULL)
		unlink(tempfiles[0]);
	if (tempfiles[1] != NULL)
		unlink(tempfiles[1]);
	if (sig)
		_exit(status);
	exit(status);
}

void *
emalloc(size_t n)
{
	void *p;

	if ((p = malloc(n)) == NULL)
		error("files too big, try -h");
	return (p);
}

void *
erealloc(void *p, size_t n)
{
	void *q;

	if ((q = realloc(p, n)) == NULL)
		error("files too big, try -h");
	return (q);
}

__dead void
error(const char *fmt, ...)
{
	va_list ap;
	int sverrno = errno;

	if (tempfiles[0] != NULL)
		unlink(tempfiles[0]);
	if (tempfiles[1] != NULL)
		unlink(tempfiles[1]);
	errno = sverrno;
	va_start(ap, fmt);
	verr(status, fmt, ap);
	va_end(ap);
}

__dead void
errorx(const char *fmt, ...)
{
	va_list ap;

	if (tempfiles[0] != NULL)
		unlink(tempfiles[0]);
	if (tempfiles[1] != NULL)
		unlink(tempfiles[1]);
	va_start(ap, fmt);
	verrx(status, fmt, ap);
	va_end(ap);
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: diff [-bitw] [-c | -e | -f | -n | -u ] file1 file2\n"
	    "       diff [-bitw] -C number file1 file2\n"
	    "       diff [-bitw] -D string file1 file2\n"
	    "       diff [-bitw] -U number file1 file2\n"
	    "       diff [-biwt] [-c | -e | -f | -n | -u ] "
	    "[-r] [-s] [-S name]\n            dir1 dir2\n");

	exit(2);
}
