/*	$OpenBSD: update.c,v 1.15 2005/02/04 17:50:20 jfb Exp $	*/
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


int  cvs_update_file  (CVSFILE *, void *);
int  cvs_update_prune (CVSFILE *, void *);


/*
 * cvs_update()
 *
 * Handle the `cvs update' command.
 * Returns 0 on success, or the appropriate exit code on error.
 */
int
cvs_update(int argc, char **argv)
{
	int i, ch, flags;
	struct cvsroot *root;

	flags = CF_SORT|CF_RECURSE|CF_IGNORE|CF_KNOWN|CF_NOSYMS;

	while ((ch = getopt(argc, argv, "ACD:dflPpQqRr:")) != -1) {
		switch (ch) {
		case 'A':
		case 'C':
		case 'D':
		case 'd':
		case 'f':
			break;
		case 'l':
			flags &= ~CF_RECURSE;
			break;
		case 'P':
		case 'p':
		case 'Q':
		case 'q':
			break;
		case 'R':
			flags |= CF_RECURSE;
			break;
		case 'r':
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		cvs_files = cvs_file_get(".", flags);
	else {
		/* don't perform ignore on explicitly listed files */
		flags &= ~(CF_IGNORE | CF_RECURSE | CF_SORT);
		cvs_files = cvs_file_getspec(argv, argc, flags);
	}
	if (cvs_files == NULL)
		return (EX_DATAERR);

	root = CVS_DIR_ROOT(cvs_files);
	if (root == NULL) {
		cvs_log(LP_ERR,
		    "No CVSROOT specified!  Please use the `-d' option");
		cvs_log(LP_ERR,
		    "or set the CVSROOT environment variable.");
		return (EX_USAGE);
	}
	if ((root->cr_method != CVS_METHOD_LOCAL) && (cvs_connect(root) < 0))
		return (EX_PROTOCOL);

	cvs_file_examine(cvs_files, cvs_update_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0)
			return (EX_PROTOCOL);

		for (i = 0; i < argc; i++)
			if (cvs_sendarg(root, argv[i], 0) < 0)
				return (EX_PROTOCOL);
		if (cvs_sendreq(root, CVS_REQ_UPDATE, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}


/*
 * cvs_update_file()
 *
 * Update a single file.  In the case where we act as client, send any
 * pertinent information about that file to the server.
 */
int
cvs_update_file(CVSFILE *cf, void *arg)
{
	int ret;
	char *fname, *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	struct cvs_ent *entp;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);
	fname = CVS_FILE_NAME(cf);

	if (cf->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cf->cf_cvstat == CVS_FST_UNKNOWN)
				ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
				    CVS_FILE_NAME(cf));
			else
				ret = cvs_senddir(root, cf);
		}

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	entp = cvs_ent_getent(fpath);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if ((entp != NULL) && (cvs_sendentry(root, entp) < 0)) {
			cvs_ent_free(entp);
			return (-1);
		}

		switch (cf->cf_cvstat) {
		case CVS_FST_UNKNOWN:
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, fname);
			break;
		case CVS_FST_UPTODATE:
			ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, fname);
			break;
		case CVS_FST_ADDED:
		case CVS_FST_MODIFIED:
			ret = cvs_sendreq(root, CVS_REQ_MODIFIED, fname);
			if (ret == 0)
				ret = cvs_sendfile(root, fpath);
			break;
		default:
			break;
		}
	} else {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_printf("? %s\n", fpath);
			return (0);
		}

		snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, fpath, RCS_FILE_EXT);

		rf = rcs_open(rcspath, RCS_MODE_READ);
		if (rf == NULL) {
			cvs_ent_free(entp);
			return (-1);
		}

		rcs_close(rf);
	}

	if (entp != NULL)
		cvs_ent_free(entp);
	return (ret);
}


/*
 * cvs_update_prune()
 *
 * Prune all directories which contain no more files known to CVS.
 */
int
cvs_update_prune(CVSFILE *cf, void *arg)
{

	return (0);
}
