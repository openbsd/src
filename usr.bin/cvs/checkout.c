/*	$OpenBSD: checkout.c,v 1.9 2004/08/12 21:03:46 jfb Exp $	*/
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



/*
 * cvs_checkout()
 *
 * Handler for the `cvs checkout' command.
 * Returns 0 on success, or one of the known system exit codes on failure.
 */

int
cvs_checkout(int argc, char **argv)
{
	int ch;
	struct cvsroot *root;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		cvs_log(LP_ERR,
		    "must specify at least one module or directory");
		return (EX_USAGE);
	}

	cvs_files = cvs_file_get(".", 0);
	if (cvs_files == NULL)
		return (EX_DATAERR);

	root = CVS_DIR_ROOT(cvs_files);
	if (root->cr_method != CVS_METHOD_LOCAL) {
		cvs_connect(root);

		if ((cvs_sendarg(root, argv[0], 0) < 0) ||
		    (cvs_senddir(root, cvs_files) < 0) ||
		    (cvs_sendreq(root, CVS_REQ_XPANDMOD, NULL) < 0))
			cvs_log(LP_ERR, "failed to expand module");

		/* XXX not too sure why we have to send this arg */
		if ((cvs_sendarg(root, "-N", 0) < 0) ||
		    (cvs_sendarg(root, argv[0], 0) < 0) ||
		    (cvs_senddir(root, cvs_files) < 0) ||
		    (cvs_sendreq(root, CVS_REQ_CO, NULL) < 0))
			cvs_log(LP_ERR, "failed to checkout");
	}

	return (0);
}
