/*	$OpenBSD: add.c,v 1.15 2005/03/30 17:43:04 joris Exp $	*/
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


int cvs_add_file(CVSFILE *, void *);
int cvs_add_options(char *, int, char **, int *);
int cvs_add_sendflags(struct cvsroot *);

struct cvs_cmd_info cvs_add = {
	cvs_add_options,
	cvs_add_sendflags,
	cvs_add_file,
	NULL, NULL,
	0,
	CVS_REQ_ADD,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static int kflag = RCS_KWEXP_DEFAULT;
static char *koptstr;

int
cvs_add_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	cvs_msg = NULL;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (EX_USAGE);
			}
			break;
		case 'm':
			if ((cvs_msg = strdup(optarg)) == NULL) {
				cvs_log(LP_ERRNO, "failed to copy message");
				return (EX_DATAERR);
			}
			break;
		default:
			return (EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

int
cvs_add_sendflags(struct cvsroot *root)
{
	char buf[16];

	if (kflag != RCS_KWEXP_DEFAULT) {
		strlcpy(buf, "-k", sizeof(buf));
		strlcat(buf, koptstr, sizeof(buf));
		if (cvs_sendarg(root, buf, 0) < 0)
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
