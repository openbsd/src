/*	$OpenBSD: add.c,v 1.12 2004/12/30 20:37:56 jfb Exp $	*/
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
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


extern char *__progname;


int  cvs_add_file (CVSFILE *, void *);


/*
 * cvs_add()
 *
 * Handler for the `cvs add' command.
 * Returns 0 on success, or one of the known system exit codes on failure.
 */
int
cvs_add(int argc, char **argv)
{
	int i, ch;
	char *kflag, *msg;
	struct cvsroot *root;

	kflag = msg = NULL;

	while ((ch = getopt(argc, argv, "k:m:")) != -1) {
		switch (ch) {
		case 'k':
			kflag = optarg;
			break;
		case 'm':
			msg = optarg;
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

	cvs_file_examine(cvs_files, cvs_add_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0)
			return (EX_PROTOCOL);

		for (i = 0; i < argc; i++)
			if (cvs_sendarg(root, argv[i], 0) < 0)
				return (EX_PROTOCOL);

		if (cvs_sendreq(root, CVS_REQ_ADD, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}


int
cvs_add_file(CVSFILE *cf, void *arg)
{
	int ret;
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL)
			ret = cvs_senddir(root, cf);

		return (ret);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED,
			    CVS_FILE_NAME(cf));
	} else {
		cvs_log(LP_INFO, "scheduling file `%s' for addition",
		    CVS_FILE_NAME(cf));
		cvs_log(LP_INFO, "use `%s commit' to add this file permanently",
		    __progname);
	}

	return (ret);
}
