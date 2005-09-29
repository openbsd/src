/*	$OpenBSD: co.c,v 1.2 2005/09/29 15:21:57 joris Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

extern char *__progname;

int
checkout_main(int argc, char **argv)
{
	int i, ch;
	RCSNUM *rev;
	RCSFILE *file;
	BUF *bp;
	char buf[16];
	char *s, fpath[MAXPATHLEN];

	rev = RCS_HEAD_REV;
	while ((ch = getopt(argc, argv, "r:")) != -1) {
		switch (ch) {
		case 'r':
			if ((rev = rcsnum_parse(optarg)) == NULL) {
				cvs_log(LP_ERR, "bad revision number");
				exit(1);
			}
			break;
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit (1);
	}
	
	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if ((file = rcs_open(fpath, RCS_READ)) == NULL)
			continue;

		if ((s = strrchr(fpath, ',')) != NULL)
			*s = '\0';

		if ((bp = rcs_getrev(file, rev)) == NULL) {
			cvs_log(LP_ERR, "cannot find '%s' in %s",
			    rcsnum_tostr(rev, buf, sizeof(buf)), fpath);
			rcs_close(file);
			continue;
		}

		if (cvs_buf_write(bp, fpath, 0644) < 0)
			cvs_log(LP_ERR, "failed to write revision to file");

		cvs_buf_free(bp);
		rcs_close(file);
	}

	if (rev != RCS_HEAD_REV)
		rcsnum_free(rev);

	return (0);
}

void
checkout_usage(void)
{
	fprintf(stderr, "usage %s [-r rev] file ...\n", __progname);
}
