/*	$OpenBSD: history.c,v 1.6 2005/01/31 21:46:43 jfb Exp $	*/
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
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "rcs.h"
#include "log.h"
#include "proto.h"

#define CVS_HISTORY_MAXMOD    16

/* history flags */
#define CVS_HF_A     0x01
#define CVS_HF_C     0x02
#define CVS_HF_E     0x04
#define CVS_HF_L     0x08
#define CVS_HF_M     0x10
#define CVS_HF_O     0x20
#define CVS_HF_T     0x40
#define CVS_HF_W     0x80

#define CVS_HF_EXCL (CVS_HF_C|CVS_HF_E|CVS_HF_M|CVS_HF_O|CVS_HF_T|CVS_HF_X)

static void  cvs_history_print  (struct cvs_hent *);


extern char *__progname;


/*
 * cvs_history()
 *
 * Handle the `cvs history' command.
 */
int
cvs_history(int argc, char **argv)
{
	int ch, flags;
	u_int nbmod, rep;
	char *user, *zone, *tag, *cp;
	char *modules[CVS_HISTORY_MAXMOD], histpath[MAXPATHLEN];
	struct cvsroot *root;
	struct cvs_hent *hent;
	CVSHIST *hp;

	tag = NULL;
	user = NULL;
	zone = "+0000";
	nbmod = 0;
	flags = 0;
	rep = 0;

	while ((ch = getopt(argc, argv, "acelm:oTt:u:wx:z:")) != -1) {
		switch (ch) {
		case 'a':
			flags |= CVS_HF_A;
			break;
		case 'c':
			rep++;
			flags |= CVS_HF_C;
			break;
		case 'e':
			rep++;
			flags |= CVS_HF_E;
			break;
		case 'l':
			flags |= CVS_HF_L;
			break;
		case 'm':
			rep++;
			flags |= CVS_HF_M;
			if (nbmod == CVS_HISTORY_MAXMOD) {
				cvs_log(LP_ERR, "too many `-m' options");
				return (EX_USAGE);
			}
			modules[nbmod++] = optarg;
			break;
		case 'o':
			rep++;
			flags |= CVS_HF_O;
			break;
		case 'T':
			rep++;
			flags |= CVS_HF_T;
			break;
		case 't':
			tag = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 'w':
			flags |= CVS_HF_W;
			break;
		case 'x':
			rep++;
			for (cp = optarg; *cp != '\0'; cp++) {
			}
			break;
		case 'z':
			zone = optarg;
			break;
		default:
			return (EX_USAGE);
		}
	}

	if (rep > 1) {
		cvs_log(LP_ERR,
		    "Only one report type allowed from: \"-Tcomxe\"");
		return (EX_USAGE);
	} else if (rep == 0)
		flags |= CVS_HF_O;    /* use -o as default */

	root = cvsroot_get(".");
	if (root == NULL) {
		cvs_log(LP_ERR,
		    "No CVSROOT specified!  Please use the `-d' option");
		cvs_log(LP_ERR,
		    "or set the CVSROOT environment variable.");
		return (EX_USAGE);
	}
	if (root->cr_method == CVS_METHOD_LOCAL) {
		snprintf(histpath, sizeof(histpath), "%s/%s", root->cr_dir,
		    CVS_PATH_HISTORY);
		hp = cvs_hist_open(histpath);
		if (hp == NULL) {
			return (EX_UNAVAILABLE);
		}

		while ((hent = cvs_hist_getnext(hp)) != NULL) {
			cvs_history_print(hent);
		}
		cvs_hist_close(hp);
	} else {
		if (flags & CVS_HF_C)
			cvs_sendarg(root, "-c", 0);

		if (flags & CVS_HF_O)
			cvs_sendarg(root, "-o", 0);

		if (tag != NULL) {
			cvs_sendarg(root, "-t", 0);
			cvs_sendarg(root, tag, 0);
		}
		if (user != NULL) {
			cvs_sendarg(root, "-u", 0);
			cvs_sendarg(root, user, 0);
		}

		cvs_sendarg(root, "-z", 0);
		cvs_sendarg(root, zone, 0);

		cvs_sendreq(root, CVS_REQ_HISTORY, NULL);
	}

	return (0);
}


static void
cvs_history_print(struct cvs_hent *hent)
{
	struct tm etime;

	if (localtime_r(&(hent->ch_date), &etime) == NULL) {
		cvs_log(LP_ERROR, "failed to convert timestamp to structure");
		return;
	}

	printf("%c %4d-%02d-%02d %02d:%02d +%04d %-16s %-16s\n",
	    hent->ch_event, etime.tm_year + 1900, etime.tm_mon + 1,
	    etime.tm_mday, etime.tm_hour, etime.tm_min,
	    0, hent->ch_user, hent->ch_repo);
}
