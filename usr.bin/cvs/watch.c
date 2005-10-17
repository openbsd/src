/*	$OpenBSD: watch.c,v 1.7 2005/10/17 16:16:00 moritz Exp $	*/
/*
 * Copyright (c) 2005 Xavier Santolaria <xsa@openbsd.org>
 * Copyright (c) 2005 Moritz Jodeit <moritz@openbsd.org>
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
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"

static int	cvs_watch_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_watch_pre_exec(struct cvsroot *);
static int	cvs_watch_remote(CVSFILE *, void*);
static int	cvs_watch_local(CVSFILE *, void*);

static int	cvs_watchers_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_watchers_local(CVSFILE *, void*);


struct cvs_cmd cvs_cmd_watch = {
	CVS_OP_WATCH, CVS_REQ_NOOP, "watch",
	{},
	"Set watches",
	"on | off | add | remove [-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	cvs_watch_init,
	cvs_watch_pre_exec,
	cvs_watch_remote,
	cvs_watch_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

struct cvs_cmd cvs_cmd_watchers = {
	CVS_OP_WATCHERS, CVS_REQ_WATCHERS, "watchers",
	{},
	"See who is watching a file",
	"[-lR] [file ...]",
	"lR",
	NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	cvs_watchers_init,
	NULL,
	cvs_watch_remote,
	cvs_watchers_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};


static char *aoptstr = NULL;
static int watchreq = 0;

static int
cvs_watch_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	if (argc < 2)
		return (CVS_EX_USAGE);

	if (strcmp(argv[1], "on") == 0)
		watchreq = CVS_REQ_WATCH_ON;
	else if (strcmp(argv[1], "off") == 0)
		watchreq = CVS_REQ_WATCH_OFF;
	else if (strcmp(argv[1], "add") == 0)
		watchreq = CVS_REQ_WATCH_ADD;
	else if (strcmp(argv[1], "remove") == 0)
		watchreq = CVS_REQ_WATCH_REMOVE;
	else
		return (CVS_EX_USAGE);

	cmd->cmd_req = watchreq;
	optind = 2;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			/*
			 * Only `watch add | remove' support the -a option.
			 * Check which command has been issued.
			 */
			if (watchreq != CVS_REQ_WATCH_ADD &&
			    watchreq != CVS_REQ_WATCH_REMOVE)
				return (CVS_EX_USAGE);
			if (strcmp(optarg, "commit") != 0 &&
			    strcmp(optarg, "edit") != 0 &&
			    strcmp(optarg, "unedit") != 0 &&
			    strcmp(optarg, "all") != 0 &&
			    strcmp(optarg, "none") != 0)
				return (CVS_EX_USAGE);
			if ((aoptstr = strdup(optarg)) == NULL) {
				cvs_log(LP_ERRNO, "failed to copy action");
				return (CVS_EX_DATA);
			}
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
 * cvs_watch_pre_exec()
 *
 */
static int
cvs_watch_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (watchreq != CVS_REQ_WATCH_ADD &&
		    watchreq != CVS_REQ_WATCH_REMOVE)
			return (CVS_EX_OK);

		if (aoptstr == NULL || strcmp(aoptstr, "all") == 0) {
			/* Defaults to: edit, unedit, commit */
			if ((cvs_sendarg(root, "-a", 0) < 0) ||
			    (cvs_sendarg(root, "edit", 0) < 0) ||
			    (cvs_sendarg(root, "-a", 0) < 0) ||
			    (cvs_sendarg(root, "unedit", 0) < 0) ||
			    (cvs_sendarg(root, "-a", 0) < 0) ||
			    (cvs_sendarg(root, "commit", 0) < 0)) {
				free(aoptstr);
				return (CVS_EX_PROTO);
			}
		} else {
			if ((cvs_sendarg(root, "-a", 0) < 0) ||
			    (cvs_sendarg(root, aoptstr, 0) < 0)) {
				free(aoptstr);
				return (CVS_EX_PROTO);
			}
		}
	}
	free(aoptstr);

	return (CVS_EX_OK);
}


/*
 * cvs_watch_local()
 *
 */
static int
cvs_watch_local(CVSFILE *cf, void *arg)
{
	return (CVS_EX_OK);
}


/*
 * cvs_watch_remote()
 *
 */
static int
cvs_watch_remote(CVSFILE *cf, void *arg)
{
	int ret;
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    cf->cf_name);
		else
			ret = cvs_senddir(root, cf);

		if (ret == -1)
			ret = CVS_EX_PROTO;

		return (ret);
	}

	if (cvs_sendentry(root, cf) < 0)
		return (CVS_EX_PROTO);

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);
		break;
	default:
		break;
	}

	if (ret == -1)
		ret = CVS_EX_PROTO;

	return (ret);
}


/*
 * cvs_watchers_init()
 *
 */
static int
cvs_watchers_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
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
 * cvs_watchers_local()
 *
 */
static int
cvs_watchers_local(CVSFILE *cf, void *arg)
{
	return (CVS_EX_OK);
}
