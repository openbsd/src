/*	$OpenBSD: edit.c,v 1.10 2005/12/30 02:03:28 joris Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"



static int	cvs_edit_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_edit_remote(CVSFILE *, void *);
static int	cvs_edit_local(CVSFILE *, void *);

static int	cvs_editors_remote(CVSFILE *, void *);


struct cvs_cmd cvs_cmd_edit = {
	CVS_OP_EDIT, CVS_REQ_NOOP, "edit",
	{ },
	"Mark a file as being edited",
	"[-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	CF_SORT | CF_RECURSE,
	cvs_edit_init,
	NULL,
	cvs_edit_remote,
	cvs_edit_local,
	NULL,
	NULL,
	0
};

struct cvs_cmd cvs_cmd_editors = {
	CVS_OP_EDITORS, CVS_REQ_EDITORS, "editors",
	{ },
	"List editors on a file",
	"[-lR] [file ...]",
	"lR",
	NULL,
	CF_SORT | CF_RECURSE,
	cvs_edit_init,
	NULL,
	cvs_editors_remote,
	NULL,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};


struct cvs_cmd cvs_cmd_unedit = {
	CVS_OP_UNEDIT, CVS_REQ_NOOP, "unedit",
	{ },
	"Undo an edit command",
	"[-lR] [file ...]",
	"lR",
	NULL,
	CF_SORT | CF_RECURSE,
	cvs_edit_init,
	NULL,
	cvs_edit_remote,
	cvs_edit_local,
	NULL,
	NULL,
	0
};



static int
cvs_edit_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch, dflag, mod_count;

	dflag = 0;
	mod_count = 0;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			/*
			 * The `editors' and `unedit' commands do not have
			 * the -a option. Check which command has been issued.
			 */
			if (cvs_cmdop != CVS_OP_EDIT)
				return (CVS_EX_USAGE);
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (CVS_EX_OK);
}


/*
 * cvs_edit_remote()
 *
 */
static int
cvs_edit_remote(CVSFILE *cf, void *arg)
{
	int *mod_count;

	mod_count = (int *)arg;

	return (CVS_EX_OK);
}


/*
 * cvs_edit_local()
 *
 */
static int
cvs_edit_local(CVSFILE *cf, void *arg)
{
	int *mod_count;

	mod_count = (int *)arg;

	return (CVS_EX_OK);
}


/*
 * cvs_editors_remote()
 *
 */
static int
cvs_editors_remote(CVSFILE *cf, void *arg)
{
	struct cvsroot *root;

	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

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
