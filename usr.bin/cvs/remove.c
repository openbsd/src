/*	$OpenBSD: remove.c,v 1.2 2005/01/31 16:49:28 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2004 Xavier Santolaria <xsa@openbsd.org>
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sysexits.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


extern char *__progname;


int  cvs_remove_file (CVSFILE *, void *);


/*
 * cvs_remove()
 *
 * Handler for the `cvs remove' command.
 * Returns 0 on success, or one of the known system exit codes on failure.
 */
int
cvs_remove(int argc, char **argv)
{
	int i, ch;
	struct cvsroot *root;

	while ((ch = getopt(argc, argv, "flR")) != -1) {
		switch (ch) {
		case 'f':
			break;
		case 'l':
			break;
		case 'R':
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return (EX_USAGE);

	cvs_files = cvs_file_getspec(argv, argc, 0);
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

	cvs_file_examine(cvs_files, cvs_remove_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0)
			return (EX_PROTOCOL);

		for (i = 0; i < argc; i++)
			if (cvs_sendarg(root, argv[i], 0) < 0)
				return (EX_PROTOCOL);

		if (cvs_sendreq(root, CVS_REQ_REMOVE, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}


int
cvs_remove_file(CVSFILE *cf, void *arg)
{
	int ret;
	char fpath[MAXPATHLEN];
	struct cvsroot *root;
	CVSENTRIES *entfile;
	struct cvs_ent *ent;

	ret = 0;
	ent = NULL;
	root = CVS_DIR_ROOT(cf);

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

	entfile = cvs_ent_open(dirname(fpath), O_RDWR);
	if (entfile == NULL) {
		cvs_log(LP_ERR, "failed to remove `%s'", fpath);
		return (-1);
	}

	ent = cvs_ent_get(entfile, CVS_FILE_NAME(cf));

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (ent != NULL)
			ret = cvs_sendentry(root, ent);
	} else {
		cvs_log(LP_INFO, "scheduling file `%s' for removal",
		    CVS_FILE_NAME(cf));
		cvs_log(LP_INFO,
		    "use `%s commit' to remove this file permanently",
		    __progname);
	}

	cvs_ent_close(entfile);

	return (ret);
}
