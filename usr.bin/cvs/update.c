/*	$OpenBSD: update.c,v 1.6 2004/08/12 17:48:18 jfb Exp $	*/
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
	int ch, i, flags;
	struct cvs_file *cf;

	cf = NULL;
	flags = CF_SORT|CF_RECURSE|CF_IGNORE|CF_KNOWN;

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

	if (argc == 0) {
		cf = cvs_file_get(".", flags);
	}
	else {
		/* don't perform ignore on explicitly listed files */
		flags &= ~CF_IGNORE;

		for (i = 0; i < argc; i++) {
			cf = cvs_file_get(argv[i], flags);
		}
	}

	cvs_file_examine(cf, cvs_update_file, NULL);

	cvs_senddir(cf->cf_ddat->cd_root, cf);
	cvs_sendreq(cf->cf_ddat->cd_root, CVS_REQ_UPDATE, NULL);

	return (0);
}


/*
 * cvs_update_file()
 *
 * Diff a single file.
 */

int
cvs_update_file(CVSFILE *cf, void *arg)
{
	char *dir, *repo, rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	struct cvs_ent *entp;

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
			root = cf->cf_parent->cf_ddat->cd_root;
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		}
		else {
			root = cf->cf_ddat->cd_root;
			if ((cf->cf_parent == NULL) ||
			    (root != cf->cf_parent->cf_ddat->cd_root)) {
				cvs_connect(root);
			}

			cvs_senddir(root, cf);
		}

		return (0);
	}
	else
		root = cf->cf_parent->cf_ddat->cd_root;

	rf = NULL;
	if (cf->cf_parent != NULL) {
		dir = cf->cf_parent->cf_path;
		repo = cf->cf_parent->cf_ddat->cd_repo;
	}
	else {
		dir = ".";
		repo = NULL;
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (root->cr_method == CVS_METHOD_LOCAL)
			cvs_printf("? %s\n", cf->cf_path);
		else
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		return (0);
	}

	entp = cvs_ent_getent(cf->cf_path);
	if (entp == NULL)
		return (-1);

	if ((root->cr_method != CVS_METHOD_LOCAL) &&
	    (cvs_sendentry(root, entp) < 0)) {
		cvs_ent_free(entp);
		return (-1);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		switch (cf->cf_cvstat) {
		case CVS_FST_UPTODATE:
			cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
			break;
		case CVS_FST_ADDED:
		case CVS_FST_MODIFIED:
			cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);
			cvs_sendfile(root, cf->cf_path);
			break;
		default:
			return (-1);
		}

		cvs_ent_free(entp);
		return (0);
	}

	snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
	    root->cr_dir, repo, cf->cf_path, RCS_FILE_EXT);

	rf = rcs_open(rcspath, RCS_MODE_READ);
	if (rf == NULL) {
		cvs_ent_free(entp);
		return (-1);
	}

	rcs_close(rf);
	cvs_ent_free(entp);
	return (0);
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
