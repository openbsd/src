/*	$OpenBSD: remove.c,v 1.4 2005/03/30 17:43:04 joris Exp $	*/
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


int cvs_remove_file(CVSFILE *, void *);
int cvs_remove_options(char *, int, char **, int *);

static int	force_remove = 0;	/* -f option */

struct cvs_cmd_info cvs_remove = {
	cvs_remove_options,
	NULL,
	cvs_remove_file,
	NULL, NULL,
	0,
	CVS_REQ_REMOVE,
	CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2 | CVS_CMD_ALLOWSPEC
};

int
cvs_remove_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'f':
			force_remove = 1;
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

	*arg = optind;
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
		/* if -f option is used, physically remove the file */
		if (force_remove == 1) {
			if((unlink(fpath) == -1) && (errno != ENOENT)) {
				cvs_log(LP_ERRNO, "failed to unlink `%s'",
				    fpath);
				return (-1);
			}
		}

		cvs_log(LP_INFO, "scheduling file `%s' for removal",
		    CVS_FILE_NAME(cf));
		cvs_log(LP_INFO,
		    "use `%s commit' to remove this file permanently",
		    __progname);
	}

	cvs_ent_close(entfile);

	return (ret);
}
