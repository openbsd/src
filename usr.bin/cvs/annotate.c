/*	$OpenBSD: annotate.c,v 1.1 2004/12/09 20:03:26 jfb Exp $	*/
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


int  cvs_annotate_file  (CVSFILE *, void *);
int  cvs_annotate_prune (CVSFILE *, void *);


/*
 * cvs_annotate()
 *
 * Handle the `cvs annotate' command.
 * Returns 0 on success, or the appropriate exit code on error.
 */
int
cvs_annotate(int argc, char **argv)
{
	int i, ch, flags;
	char *date, *rev;
	struct cvsroot *root;

	date = NULL;
	rev = NULL;
	flags = CF_SORT|CF_RECURSE|CF_IGNORE|CF_NOSYMS;

	while ((ch = getopt(argc, argv, "D:FflRr:")) != -1) {
		switch (ch) {
		case 'D':
			date = optarg;
			break;
		case 'l':
			flags &= ~CF_RECURSE;
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

	if (argc == 0) {
		cvs_files = cvs_file_get(".", flags);
	} else {
		/* don't perform ignore on explicitly listed files */
		flags &= ~(CF_IGNORE | CF_RECURSE | CF_SORT);
		cvs_files = cvs_file_getspec(argv, argc, flags);
	}
	if (cvs_files == NULL)
		return (EX_DATAERR);

	root = CVS_DIR_ROOT(cvs_files);
	if (root->cr_method != NULL) {
		cvs_connect(root);
		if (rev != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, rev, 0);
		}
		if (date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, date, 0); 
		}
	}

	cvs_file_examine(cvs_files, cvs_annotate_file, NULL);


	if (root->cr_method != CVS_METHOD_LOCAL) {
		cvs_senddir(root, cvs_files);
		for (i = 0; i < argc; i++)
			cvs_sendarg(root, argv[i], 0);
		cvs_sendreq(root, CVS_REQ_ANNOTATE, NULL);
	}

	return (0);
}


/*
 * cvs_annotate_file()
 *
 * Annotate a single file.
 */
int
cvs_annotate_file(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN];
	struct cvsroot *root;
	struct cvs_ent *entp;

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
			root = cf->cf_parent->cf_ddat->cd_root;
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cf));
		} else {
			root = cf->cf_ddat->cd_root;
			if ((cf->cf_parent == NULL) ||
			    (root != cf->cf_parent->cf_ddat->cd_root)) {
				cvs_connect(root);
			}

			cvs_senddir(root, cf);
		}

		return (0);
	} else
		root = cf->cf_parent->cf_ddat->cd_root;

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (root->cr_method == CVS_METHOD_LOCAL)
			cvs_printf("? %s\n", fpath);
		else
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cf));
		return (0);
	}

	entp = cvs_ent_getent(fpath);
	if ((entp != NULL) && (root->cr_method != CVS_METHOD_LOCAL) &&
	    (cvs_sendentry(root, entp) < 0)) {
		cvs_ent_free(entp);
		return (-1);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		switch (cf->cf_cvstat) {
		case CVS_FST_UPTODATE:
			cvs_sendreq(root, CVS_REQ_UNCHANGED, CVS_FILE_NAME(cf));
			break;
		case CVS_FST_ADDED:
		case CVS_FST_MODIFIED:
			cvs_sendreq(root, CVS_REQ_ISMODIFIED,
			    CVS_FILE_NAME(cf));
			break;
		default:
			return (-1);
		}

	}

	if (entp != NULL)
		cvs_ent_free(entp);
	return (0);
}
