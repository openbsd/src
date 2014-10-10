/*	$OpenBSD: merge.c,v 1.9 2014/10/10 08:15:25 otto Exp $	*/
/*
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

int
merge_main(int argc, char **argv)
{
	int ch, flags, labels, status;
	const char *label[3];
	BUF *bp;

	flags = labels = 0;

	/*
	 * Using getopt(3) and not rcs_getopt() because merge(1)
	 * allows spaces between options and their arguments.
	 * Thus staying compatible with former implementation.
	 */
	while ((ch = getopt(argc, argv, "AEeL:pqV")) != -1) {
		switch(ch) {
		case 'A':
			/*
			 * kept for compatibility
			 */
			break;
		case 'E':
			flags |= MERGE_EFLAG;
			flags |= MERGE_OFLAG;
			break;
		case 'e':
			flags |= MERGE_EFLAG;
			break;
		case 'L':
			if (3 <= labels)
				errx(D_ERROR, "too many -L options");
			label[labels++] = optarg;
			break;
		case 'p':
			flags |= PIPEOUT;
			break;
		case 'q':
			flags |= QUIET;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 3) {
		warnx("%s arguments", (argc < 3) ? "not enough" : "too many");
		(usage)();
	}

	for (; labels < 3; labels++)
		label[labels] = argv[labels];

	/* XXX handle labels */
	if ((bp = merge_diff3(argv, flags)) == NULL)
		errx(D_ERROR, "failed to merge");

	if (diff3_conflicts != 0)
		status = D_OVERLAPS;
	else
		status = 0;

	if (flags & PIPEOUT)
		buf_write_fd(bp, STDOUT_FILENO);
	else {
		/* XXX */
		if (buf_write(bp, argv[0], 0644) < 0)
			warnx("buf_write failed");
	}
	buf_free(bp);

	return (status);
}

__dead void
merge_usage(void)
{
	(void)fprintf(stderr,
	    "usage: merge [-EepqV] [-L label] file1 file2 file3\n");

	exit(D_ERROR);
}
