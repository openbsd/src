/*	$OpenBSD: diff.c,v 1.7 2003/06/25 19:56:57 millert Exp $	*/

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

#include <stdlib.h>
#include <unistd.h>

#include "diff.h"
#include "pathnames.h"

#if 0
static char const sccsid[] = "@(#)diff.c 4.7 5/11/89";
#endif

/*
 * diff - driver and subroutines
 */

char diff[] = _PATH_DIFF;
char diffh[] = _PATH_DIFFH;
char pr[] = _PATH_PR;

static void noroom(void);
__dead void usage(void);

int
main(int argc, char **argv)
{
	int ch;

	ifdef1 = "FILE1";
	ifdef2 = "FILE2";
	status = 2;
	diffargv = argv;

	while ((ch = getopt(argc, argv, "bC:cD:efhilnrS:stw")) != -1) {
		switch (ch) {
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
			/* -Dfoo = -E -1 -2foo */
			opt = D_IFDEF;
			ifdef1 = "";
			ifdef2 = optarg;
			wantelses++;
			break;
		case 'e':
			opt = D_EDIT;
			break;
		case 'f':
			opt = D_REVERSE;
			break;
		case 'h':
			hflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'l':
			lflag++;
			break;
		case 'n':
			opt = D_NREVERSE;
			break;
		case 'r':
			opt = D_REVERSE;
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
		errx(1, "two filename arguments required");
	file1 = argv[0];
	file2 = argv[1];
	if (hflag && opt)
		errx(1, "-h doesn't support -D, -c, -C, -e, -f, -I or -n");
	if (!strcmp(file1, "-"))
		stb1.st_mode = S_IFREG;
	else if (stat(file1, &stb1) < 0)
		err(1, "%s", file1);
	if (!strcmp(file2, "-"))
		stb2.st_mode = S_IFREG;
	else if (stat(file2, &stb2) < 0)
		err(1, "%s", file2);
	if ((stb1.st_mode & S_IFMT) == S_IFDIR &&
	    (stb2.st_mode & S_IFMT) == S_IFDIR) {
		diffdir(argv);
	} else
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
	if (tempfile)
		unlink(tempfile);
	if (sig)
		_exit(status);
	exit(status);
}

void *
talloc(size_t n)
{
	void *p;

	if ((p = malloc(n)) == NULL)
		noroom();
	return (p);
}

void *
ralloc(void *p, size_t n)
{
	void *q;

	if ((q = realloc(p, n)) == NULL)
		noroom();
	return (q);
}

static void
noroom(void)
{
	warn("files too big, try -h");
	done(0);
}

__dead void
usage(void)
{
	(void)fprintf(stderr, "usage: diff [-c | -C lines | -e | -f | -h | -n ] [-biwt] file1 file2\n"
	    "usage: diff [-Dstring] [-biw] file1 file2\n"
	    "usage: diff [-l] [-r] [-s] [-c | -C lines | -e | -f | -h | -n ] [-biwt]\n            [-Sname] dir1 dir2\n");

	exit(1);
}
