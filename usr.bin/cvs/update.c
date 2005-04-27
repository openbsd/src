/*	$OpenBSD: update.c,v 1.26 2005/04/27 04:42:40 jfb Exp $	*/
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

#include "cvs.h"
#include "rcs.h"
#include "log.h"
#include "proto.h"


static int cvs_update_remote    (CVSFILE *, void *);
static int cvs_update_local     (CVSFILE *, void *);
static int cvs_update_options   (char *, int, char **, int *);
static int cvs_update_sendflags (struct cvsroot *);

struct cvs_cmd_info cvs_update = {
	cvs_update_options,
	cvs_update_sendflags,
	cvs_update_remote,
	NULL, NULL,
	CF_SORT | CF_RECURSE | CF_IGNORE | CF_KNOWN | CF_NOSYMS,
	CVS_REQ_UPDATE,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2 | CVS_CMD_SENDDIR
};

static int Pflag, dflag, Aflag;

static int
cvs_update_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	Pflag = dflag = Aflag = 0;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'C':
		case 'D':
		case 'd':
			dflag = 1;
			break;
		case 'f':
			break;
		case 'l':
			cvs_update.file_flags &= ~CF_RECURSE;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'p':
		case 'Q':
		case 'q':
			break;
		case 'R':
			cvs_update.file_flags |= CF_RECURSE;
			break;
		case 'r':
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_update_sendflags(struct cvsroot *root)
{
	if (Pflag && cvs_sendarg(root, "-P", 0) < 0)
		return (CVS_EX_PROTO);
	if (Aflag && cvs_sendarg(root, "-A", 0) < 0)
		return (CVS_EX_PROTO);
	if (dflag && cvs_sendarg(root, "-d", 0) < 0)
		return (CVS_EX_PROTO);
	return (0);
}

/*
 * cvs_update_remote()
 *
 * Update a single file.  In the case where we act as client, send any
 * pertinent information about that file to the server.
 */
static int
cvs_update_remote(CVSFILE *cf, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN];
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    cf->cf_name);
		else
			ret = cvs_senddir(root, cf);

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cvs_sendentry(root, cf) < 0)
		return (CVS_EX_PROTO);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);
		if (ret == 0)
			ret = cvs_sendfile(root, fpath);
		break;
	default:
		break;
	}

	return (ret);
}

/*
 * cvs_update_local()
 */
static int
cvs_update_local(CVSFILE *cf, void *arg)
{
	int ret, l;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (cf->cf_type == DT_DIR) {
		return (CVS_EX_OK);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		cvs_printf("? %s\n", fpath);
		return (0);
	}

	l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, cf->cf_name, RCS_FILE_EXT);
	if (l == -1 || l >= (int)sizeof(rcspath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcspath);
		return (-1);
	}

	rf = rcs_open(rcspath, RCS_RDWR);
	if (rf == NULL) {
		return (CVS_EX_DATA);
	}

	rcs_close(rf);

	return (ret);
}
