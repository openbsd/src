/*	$OpenBSD: import.c,v 1.3 2005/01/06 19:56:38 jfb Exp $	*/
/*
 * Copyright (c) 2004 Joris Vink <amni@pandora.be>
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
#include <sys/queue.h>

#include <err.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "log.h"
#include "file.h"
#include "cvs.h"
#include "proto.h"


#define CVS_IMPORT_DEFBRANCH    "1.1.1"



int cvs_import_file(CVSFILE *, void *);
char repo[MAXPATHLEN];

/*
 * cvs_import()
 *
 * Handler for the `cvs import' command.
 */
int
cvs_import(int argc, char **argv)
{
	int ch, flags;
	char *branch, *ep;
	struct cvsroot *root;
	RCSNUM *bnum;

	branch = CVS_IMPORT_DEFBRANCH;
	flags = CF_RECURSE | CF_IGNORE | CF_NOSYMS;

	while ((ch = getopt(argc, argv, "b:dI:k:m:")) != -1) {
		switch (ch) {
		case 'b':
			branch = optarg;
			if ((bnum = rcsnum_alloc()) == NULL)
				return (-1);
			if ((rcsnum_aton(branch, &ep, bnum) < 0) ||
			    (*ep != '\0')) {
				cvs_log(LP_ERR, "%s is not a numeric branch",
				    branch);
				return (EX_USAGE);
			}
			break;
		case 'd':
			break;
		case 'I':
			if (cvs_file_ignore(optarg) < 0) {
				cvs_log(LP_ERR, "failed to add `%s' to list "
				    "of ignore patterns", optarg);
				return (EX_USAGE);
			}
			break;
		case 'k':
			break;
		case 'm':
			cvs_msg = optarg;
			break;
		default:
			return (EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc > 4)
		return (EX_USAGE);

	cvs_files = cvs_file_get(".", flags);
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

	if ((cvs_msg == NULL) &&
	    (cvs_msg = cvs_logmsg_get(NULL, NULL, NULL, NULL)) == NULL)
		return (-1);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if ((cvs_connect(root) < 0) ||
		    (cvs_sendarg(root, "-b", 0) < 0) ||
		    (cvs_sendarg(root, branch, 0) < 0) ||
		    (cvs_logmsg_send(root, cvs_msg) < 0) ||
		    (cvs_sendarg(root, argv[0], 0) < 0) ||
		    (cvs_sendarg(root, argv[1], 0) < 0) ||
		    (cvs_sendarg(root, argv[2], 0) < 0))
			return (EX_PROTOCOL);
	}

	snprintf(repo, sizeof(repo), "%s/%s", root->cr_dir, argv[0]);
	cvs_file_examine(cvs_files, cvs_import_file, NULL);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_senddir(root, cvs_files) < 0 ||
		    cvs_sendreq(root, CVS_REQ_IMPORT, NULL) < 0)
			return (EX_PROTOCOL);
	}

	return (0);
}

/*
 * cvs_import_file()
 *
 * Perform the import of a single file or directory.
 */
int
cvs_import_file(CVSFILE *cfp, void *arg)
{
	int ret;
	struct cvsroot *root;
	char fpath[MAXPATHLEN], repodir[MAXPATHLEN];

	root = CVS_DIR_ROOT(cfp);

	cvs_file_getpath(cfp, fpath, sizeof(fpath));
	printf("Importing %s\n", fpath);

	if (cfp->cf_type == DT_DIR) {
		if (!strcmp(CVS_FILE_NAME(cfp), "."))
			strlcpy(repodir, repo, sizeof(repodir));
		else
			snprintf(repodir, sizeof(repodir), "%s/%s", repo, fpath);
		if (root->cr_method != CVS_METHOD_LOCAL) {
			ret = cvs_sendreq(root, CVS_REQ_DIRECTORY, fpath);
			if (ret == 0)
				ret = cvs_sendln(root, repodir);
		} else {
			/* create the directory */
		}

		return (0);
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_sendreq(root, CVS_REQ_MODIFIED, CVS_FILE_NAME(cfp)) < 0)
			return (-1);
		if (cvs_sendfile(root, fpath) < 0)
			return (-1);
	} else {
		/* local import */
	}

	return (0);
}
