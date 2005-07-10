/*	$OpenBSD: add.c,v 1.23 2005/07/10 21:55:30 joris Exp $	*/
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
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


extern char *__progname;


static int cvs_add_remote(CVSFILE *, void *);
static int cvs_add_local(CVSFILE *, void *);
static int cvs_add_init(struct cvs_cmd *, int, char **, int *);
static int cvs_add_pre_exec(struct cvsroot *);

struct cvs_cmd cvs_cmd_add = {
	CVS_OP_ADD, CVS_REQ_ADD, "add",
	{ "ad", "new" },
	"Add a new file/directory to the repository",
	"[-k mode] [-m msg] file ...",
	"k:m:",
	NULL,
	0,
	cvs_add_init,
	cvs_add_pre_exec,
	cvs_add_remote,
	cvs_add_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static int kflag = RCS_KWEXP_DEFAULT;
static char *koptstr;

static int
cvs_add_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	cvs_msg = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				rcs_kflag_usage();
				return (CVS_EX_USAGE);
			}
			break;
		case 'm':
			if ((cvs_msg = strdup(optarg)) == NULL) {
				cvs_log(LP_ERRNO, "failed to copy message");
				return (CVS_EX_DATA);
			}
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_add_pre_exec(struct cvsroot *root)
{
	char buf[16];

	if ((root->cr_method != CVS_METHOD_LOCAL) &&
	    (kflag != RCS_KWEXP_DEFAULT)) {
		strlcpy(buf, "-k", sizeof(buf));
		strlcat(buf, koptstr, sizeof(buf));
		if (cvs_sendarg(root, buf, 0) < 0)
			return (CVS_EX_PROTO);
	}

	return (0);
}

static int
cvs_add_remote(CVSFILE *cf, void *arg)
{
	int ret;
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		ret = cvs_senddir(root, cf);
		if (ret == -1)
			ret = CVS_EX_PROTO;
		return (ret);
	}

	if (cf->cf_cvstat == CVS_FST_UNKNOWN)
		ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED,
		    cf->cf_name);

	if (ret == -1)
		ret = CVS_EX_PROTO;

	return (ret);
}

static
int cvs_add_local(CVSFILE *cf, void *arg)
{
	cvs_log(LP_INFO, "scheduling file `%s' for addition", cf->cf_name);
	cvs_log(LP_INFO, "use `%s commit' to add this file permanently",
	    __progname);

	return (0);
}
