/*	$OpenBSD: getlog.c,v 1.3 2004/07/30 01:49:23 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <paths.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "rcs.h"
#include "sock.h"
#include "proto.h"


#define CVS_GLOG_RFONLY    0x01
#define CVS_GLOG_HDONLY    0x02


#define CVS_GETLOG_REVSEP   "----------------------------"
#define CVS_GETLOG_REVEND \
 "============================================================================="

static void cvs_getlog_print   (const char *, RCSFILE *, u_int);




/*
 * cvs_getlog()
 *
 * Implement the `cvs log' command.
 */

int
cvs_getlog(int argc, char **argv)
{
	int i, rfonly, honly, recurse;
	u_int flags;
	char rcspath[MAXPATHLEN];
	RCSFILE *rfp;

	rfonly = 0;
	honly = 0;
	recurse = 1;

	while ((i = getopt(argc, argv, "d:hlRr:")) != -1) {
		switch (i) {
		case 'd':
			break;
		case 'h':
			honly = 1;
			break;
		case 'l':
			recurse = 0;
			break;
		case 'R':
			rfonly = 1;
			break;
		case 'r':
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; i++) {
		rfp = NULL;
#if 0
		if (cvs_root->cr_method == CVS_METHOD_LOCCAL) {
			snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
			    cvs_root->cr_dir, argv[i], RCS_FILE_EXT);

			rfp = rcs_open(rcspath, RCS_MODE_READ);
			if (rfp == NULL)
				continue;
		}
#endif

		cvs_getlog_print(argv[i], rfp, 0);
	}

	return (0);
}


static void
cvs_getlog_print(const char *file, RCSFILE *rfp, u_int flags)
{
	char numbuf[64], datebuf[64], *sp;
	struct rcs_delta *rdp;

	printf("RCS file: %s\nWorking file: %s\n",
	    rfp->rf_path, file);
	printf("Working file: %s\n", NULL);
	printf("head: %s\nbranch:\nlocks:\naccess list:\n");
	printf("symbolic names:\nkeyword substitutions:\n");
	printf("total revisions: %u;\tselected revisions: %u\n", 1, 1);

	printf("description:\n");

	for (;;) {
		printf(CVS_GETLOG_REVSEP "\n");
		rcsnum_tostr(&(rdp->rd_num), numbuf, sizeof(numbuf));
		printf("revision %s\n", numbuf);
		printf("date: %d/%02d/%d %02d:%02d:%02d;  author: %s;"
		    "  state: %s;  lines:",
		    rdp->rd_date.tm_year, rdp->rd_date.tm_mon + 1,
		    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
		    rdp->rd_date.tm_min, rdp->rd_date.tm_sec,
		    rdp->rd_author, rdp->rd_state);
	}

	printf(CVS_GETLOG_REVEND "\n");

}
