/*	$OpenBSD: checkout.c,v 1.14 2005/03/30 17:43:04 joris Exp $	*/
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

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"


#define CVS_LISTMOD    1
#define CVS_STATMOD    2

int cvs_checkout_options(char *, int, char **, int *);
int cvs_checkout_sendflags(struct cvsroot *);

struct cvs_cmd_info cvs_checkout = {
	cvs_checkout_options,
	cvs_checkout_sendflags,
	NULL, NULL, NULL,
	0,
	CVS_REQ_CO,
	CVS_CMD_SENDDIR | CVS_CMD_SENDARGS1 | CVS_CMD_SENDARGS2
};

static char *date, *rev, *koptstr, *tgtdir, *rcsid;
static int statmod = 0;
static int kflag = RCS_KWEXP_DEFAULT;

int
cvs_checkout_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	date = rev = koptstr = tgtdir = rcsid = NULL;
	kflag = RCS_KWEXP_DEFAULT;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'A':
			break;
		case 'c':
			statmod = CVS_LISTMOD;
			break;
		case 'D':
			date = optarg;
			break;
		case 'd':
			tgtdir = optarg;
			break;
		case 'f':
			break;
		case 'j':
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (EX_USAGE);
			}
			break;
		case 'r':
			rev = optarg;
			break;
		case 's':
			statmod = CVS_STATMOD;
			break;
		case 't':
			rcsid = optarg;
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (!statmod && (argc == 0)) {
		cvs_log(LP_ERR,
		    "must specify at least one module or directory");
		return (EX_USAGE);
	}

	if (statmod && (argc > 0)) {
		cvs_log(LP_ERR,  "-c and -s must not get any arguments");
		return (EX_USAGE);
	}

	*arg = optind;
	return (0);
}

int
cvs_checkout_sendflags(struct cvsroot *root)
{
	if (cvs_senddir(root, cvs_files) < 0)
		return (EX_PROTOCOL);
	if (cvs_sendreq(root, CVS_REQ_XPANDMOD, NULL) < 0)
		cvs_log(LP_ERR, "failed to expand module");

	/* XXX not too sure why we have to send this arg */
	if (cvs_sendarg(root, "-N", 0) < 0)
		return (EX_PROTOCOL);

	if ((statmod == CVS_LISTMOD) &&
	    (cvs_sendarg(root, "-c", 0) < 0))
		return (EX_PROTOCOL);

	if ((statmod == CVS_STATMOD) &&
	    (cvs_sendarg(root, "-s", 0) < 0))
		return (EX_PROTOCOL);

	return (0);
}
