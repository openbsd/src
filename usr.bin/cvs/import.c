/*	$OpenBSD: import.c,v 1.2 2004/12/07 17:10:56 tedu Exp $	*/
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

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"

static int do_import(struct cvsroot *, char **);
static int cvs_import_dir(struct cvsroot *, char *, char *);
static int cvs_import_file(struct cvsroot *, CVSFILE *);

/*
 * cvs_import()
 *
 * Handler for the `cvs import' command.
 */
int
cvs_import(int argc, char **argv)
{
	int ch, flags;
	char *repo, *vendor, *release;
	struct cvsroot *root;

	flags = CF_IGNORE|CF_NOSYMS;

	while ((ch = getopt(argc, argv, "b:dI:k:m:")) != -1) {
		switch (ch) {
		case 'b':
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
	if (root->cr_method != CVS_METHOD_LOCAL) {
		cvs_connect(root);

		/* Do it */
		do_import(root, argv);

		cvs_disconnect(root);
	}

	return (0);
}

/*
 * Import a module using a server
 */
static int
do_import(struct cvsroot *root, char **argv)
{
	char repository[MAXPATHLEN];

	/* XXX temporary */
	if (cvs_sendarg(root, "-m testlog", 0) < 0) {
		cvs_log(LP_ERR, "failed to send temporary logmessage");
		return (-1);
	}

	/* send arguments */
	if (cvs_sendarg(root, argv[0], 0) < 0 ||
	    cvs_sendarg(root, argv[1], 0) < 0 ||
	    cvs_sendarg(root, argv[2], 0) < 0) {
		cvs_log(LP_ERR, "failed to send arguments");
		return (-1);
	}

	/* create the repository name */
	snprintf(repository, sizeof(repository), "%s/%s",
	    root->cr_dir, argv[0]);

	cvs_files = cvs_file_get(".", 0);
	if (cvs_files == NULL) {
		cvs_log(LP_ERR, "failed to obtain info on root");
		return (-1);
	}

	/* walk the root directory */
	cvs_import_dir(root, ".", repository);

	/* send import request */
	if (cvs_senddir(root, cvs_files) < 0 ||
	    cvs_sendraw(root, repository, strlen(repository) < 0 ||
	    cvs_sendraw(root, "\n", 1) < 0 ||
	    cvs_sendreq(root, CVS_REQ_IMPORT, NULL) < 0))
		cvs_log(LP_ERR, "failed to import repository %s",
		    repository);


	/* done */
	return (0);
}

static int
cvs_import_dir(struct cvsroot *root, char *dirname, char *repo)
{
	char *cwd;
	char *basedir;
	char cvsdir[MAXPATHLEN];
	CVSFILE *parent, *fp;

	if ((basedir = strrchr(dirname, '/')) != NULL)
		basedir++;
	else
		basedir = dirname;

	/* save current directory */
	if ((cwd = getcwd(NULL, MAXPATHLEN)) == NULL) {
		cvs_log(LP_ERR, "couldn't save current directory");
		return (-1);
	}

	/* Switch to the new directory */
	if (chdir(basedir) < 0) {
		cvs_log(LP_ERR, "failed to switch to directory %s", dirname);
		return (-1);
	}

	if (!strcmp(dirname, "."))
		strlcpy(cvsdir, repo, sizeof(cvsdir));
	else
		snprintf(cvsdir, sizeof(cvsdir), "%s/%s", repo, dirname);

	/* Obtain information about the directory */
	parent = cvs_file_get(".", CF_SORT|CF_RECURSE|CF_IGNORE);
	if (parent == NULL) {
		cvs_log(LP_ERR, "couldn't obtain info on %s", dirname);
		return (-1);
	}

	if (cvs_sendreq(root, CVS_REQ_DIRECTORY, dirname) < 0 ||
	    cvs_sendraw(root, cvsdir, strlen(cvsdir)) < 0 ||
	    cvs_sendraw(root, "\n", 1) < 0)
		return (-1);

	printf("Importing %s\n", dirname);

	/* Walk the directory */
	TAILQ_FOREACH(fp, &(parent->cf_ddat->cd_files), cf_list) {
		/* If we have a sub directory, skip it for now */
		if (fp->cf_type == DT_DIR)
			continue;

		/* Import the file */
		if (cvs_import_file(root, fp) < 0)
#if 0
			cvs_log(LP_ERR, "failed to import %s", fp->cf_path);
#else
			cvs_log(LP_ERR, "failed to import %s", NULL);
#endif
	}

	/* Walk the subdirectories */
	TAILQ_FOREACH(fp, &(parent->cf_ddat->cd_files), cf_list) {
		if (fp->cf_type != DT_DIR)
			continue;
		if (!strcmp(CVS_FILE_NAME(fp), ".") ||
		    !strcmp(CVS_FILE_NAME(fp), ".."))
			continue;

		if (strcmp(dirname, "."))
			snprintf(cvsdir, sizeof(cvsdir), "%s/%s",
			    dirname, CVS_FILE_NAME(fp));
		else
			strlcpy(cvsdir, CVS_FILE_NAME(fp), sizeof(cvsdir));
		if (cvs_import_dir(root, cvsdir, repo) < 0)
			cvs_log(LP_ERR, "failed to import directory %s",
			    CVS_FILE_NAME(fp));
	}

	cvs_file_free(parent);

	/* restore working directory */
	if (chdir(cwd) < 0) {
		cvs_log(LP_ERR, "failed to restore directory %s", cwd);
		return (-1);
	}

	return (0);
}

/*
 * Import a file
 */
static int
cvs_import_file(struct cvsroot *root, CVSFILE *fp)
{
	/* Send a Modified response follwed by the
	 * file's mode, length and contents
	 */
	if (cvs_sendreq(root, CVS_REQ_MODIFIED, CVS_FILE_NAME(fp)) < 0)
		return (-1);
	if (cvs_sendfile(root, CVS_FILE_NAME(fp)) < 0)
		return (-1);

	return (0);
}
