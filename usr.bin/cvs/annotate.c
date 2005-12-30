/*	$OpenBSD: annotate.c,v 1.25 2005/12/30 02:03:28 joris Exp $	*/
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

static int	cvs_annotate_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_annotate_remote(CVSFILE *, void *);
static int	cvs_annotate_local(CVSFILE *, void *);
static int	cvs_annotate_pre_exec(struct cvsroot *);

struct cvs_cmd cvs_cmd_annotate = {
	CVS_OP_ANNOTATE, CVS_REQ_ANNOTATE, "annotate",
	{ "ann", "blame"  },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] ...",
	"D:flRr:",
	NULL,
	CF_SORT | CF_RECURSE | CF_IGNORE | CF_NOSYMS,
	cvs_annotate_init,
	cvs_annotate_pre_exec,
	cvs_annotate_remote,
	cvs_annotate_local,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};

static char *date, *rev;
static int usehead;

static int
cvs_annotate_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	usehead = 0;
	date = NULL;
	rev = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			date = optarg;
			break;
		case 'f':
			usehead = 1;
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
			break;
		case 'r':
			rev = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_annotate_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (usehead)
			cvs_sendarg(root, "-f", 0);

		if (rev != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, rev, 0);
		}

		if (date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, date, 0);
		}
	}

	return (0);
}

/*
 * cvs_annotate_remote()
 *
 * Annotate a single file.
 */
static int
cvs_annotate_remote(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	cvs_sendentry(root, cf);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);
		break;
	default:
		break;
	}

	return (0);
}


static int
cvs_annotate_local(CVSFILE *cf, void *arg)
{
	char fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;

	if (cf->cf_type == DT_DIR)
		return (0);

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cf->cf_cvstat == CVS_FST_UNKNOWN)
		return (0);

	cvs_rcs_getpath(cf, rcspath, sizeof(rcspath));

	rf = rcs_open(rcspath, RCS_READ);
	if (rf == NULL)
		return (CVS_EX_DATA);

	cvs_printf("Annotations for %s", cf->cf_name);
	cvs_printf("\n***************\n");

	rcs_close(rf);

	return (0);
}
