/*	$OpenBSD: release.c,v 1.4 2005/05/19 15:31:35 xsa Exp $	*/
/*
 * Copyright (c) 2005 Xavier Santolaria <xsa@openbsd.org>
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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"

#define UPDCMD_FLAGS	"-n -q -d"

extern char *__progname;

static int cvs_release_options(char *, int, char **, int *);
static int cvs_release_sendflags(struct cvsroot *);
static int cvs_release_yesno(void);
static int cvs_release_dir(CVSFILE *, void *);

struct cvs_cmd_info cvs_release = {
	cvs_release_options,
	cvs_release_sendflags,
	cvs_release_dir,
	NULL, NULL,
	CF_IGNORE | CF_KNOWN | CF_NOFILES | CF_RECURSE,
	CVS_REQ_RELEASE,
	CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2 | CVS_CMD_ALLOWSPEC
};

static int	dflag;	/* -d option */

static int
cvs_release_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;
	*arg = optind;

	if (argc == 0)
		return (CVS_EX_USAGE);

	return (0);
}

static int
cvs_release_sendflags(struct cvsroot *root)
{
	if (dflag && cvs_sendarg(root, "-d", 0) < 0)
		return (CVS_EX_PROTO);

	return (0);
}

/*
 * cvs_release_yesno()
 *
 * Read from standart input for `y' or `Y' character.
 * Returns 0 on success, or -1 on failure.
 */
static int
cvs_release_yesno(void)
{
	int c, ret;

	ret = 0;
	
	fflush (stderr);
	fflush (stdout);

	if ((c = getchar()) != 'y' && c != 'Y')
		ret = -1;
	else
		while (c != EOF && c != '\n')
			c = getchar();

	return (ret);
}

/*
 * cvs_release_dir()
 *
 * Release specified directorie(s).
 * Returns 0 on success, or -1 on failure.
 */
static int
cvs_release_dir(CVSFILE *cdir, void *arg)
{
	FILE *fp;
	int j, l;
	char *wdir, cwd[MAXPATHLEN];
	char buf[256], cdpath[MAXPATHLEN], dpath[MAXPATHLEN];
	char updcmd[MAXPATHLEN];	/* XXX find a better size; malloc()?? */
	struct stat st;
	struct cvsroot *root;

	j = 0;		/* number of altered files in the working copy */

	root = CVS_DIR_ROOT(cdir);

	cvs_file_getpath(cdir, dpath, sizeof(dpath));

	l = snprintf(cdpath, sizeof(cdpath), "%s/" CVS_PATH_CVSDIR, dpath);
	if (l == -1 || l >= (int)sizeof(cdpath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", cdpath);
		return (-1);
	}

	if (cdir->cf_type == DT_DIR) {
		if (!strcmp(CVS_FILE_NAME(cdir), "."))
			return (0);
		else {
			/* test if dir has CVS/ directory */
			if (stat(cdpath, &st) == -1) {
				cvs_log(LP_ERR,
				    "no repository directory: %s", dpath);
				return (0);
			}
		}

		if (root->cr_method != CVS_METHOD_LOCAL) {
			/* XXX kept for compat reason of `cvs update' output */
			/* save current working directory for further use */
			if ((wdir = getcwd(cwd, sizeof(cwd))) == NULL)
                		cvs_log(LP_ERRNO, "cannot get current dir");

			/* change dir before running the `cvs update' command */
			if (chdir(dpath) == -1) {
				cvs_log(LP_ERRNO, "cannot change to dir `%s'",
				    dpath);
				return (-1);
			}

			/* construct `cvs update' command */
			l = snprintf(updcmd, sizeof(updcmd), "%s %s %s update",
			    __progname, UPDCMD_FLAGS, root->cr_str);
			if (l == -1 || l >= (int)sizeof(updcmd))
				return (-1);

			/* XXX we should try to avoid a new connection ... */
			if ((fp = popen(updcmd, "r")) == NULL) {
				cvs_log(LP_ERROR, "cannot run command `%s'",
				    updcmd);
				return (-1);
			}

			while (fgets(buf, sizeof(buf), fp) != NULL) {
				if (strchr("ACMPRU", buf[0]))
					j++;
				(void)fputs(buf, stdout);
			}

			if (pclose(fp) != 0) {
				cvs_log(LP_ERROR, "unable to release `%s'",
				    dpath);
				return (-1);
			}

			printf("You have [%d] altered file%s in this "
			    "repository.\n", j, j > 1 ? "s" : "");

			printf("Are you sure you want to release "
			    "%sdirectory `%s': ",
			    dflag ? "(and delete) " : "", dpath);

			if (cvs_release_yesno() == -1) {	/* No */
				(void)fprintf(stderr,
			    	    "** `%s' aborted by user choice.\n",
				    cvs_command);
				return (-1);
			}

			/* change back to original working dir */
			if (chdir(wdir) == -1) {
				cvs_log(LP_ERRNO, "cannot change to original "
				    "working dir `%s'", wdir);
				return (-1);
			}

			if (dflag == 1) {
				if (cvs_remove_dir(dpath) != CVS_EX_OK) {
					cvs_log(LP_ERRNO,
				   	    "deletion of directory `%s' failed",
					    dpath);
					return (-1);
				}
			}
		}
	} else {
		cvs_log(LP_ERR, "no such directory: %s", dpath);
		return (-1);
	}

	return (0);
}
