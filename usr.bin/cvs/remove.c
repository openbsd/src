/*	$OpenBSD: remove.c,v 1.39 2005/12/30 02:03:28 joris Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2004, 2005 Xavier Santolaria <xsa@openbsd.org>
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


extern char *__progname;


static int	cvs_remove_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_remove_remote(CVSFILE *, void *);
static int	cvs_remove_local(CVSFILE *, void *);
static int	cvs_remove_file(const char *);

static int	force_remove = 0;	/* -f option */
static int	nuked = 0;

struct cvs_cmd cvs_cmd_remove = {
	CVS_OP_REMOVE, CVS_REQ_REMOVE, "remove",
	{ "rm", "delete" },
	"Remove an entry from the repository",
	"[-flR] [file ...]",
	"flR",
	NULL,
	CF_IGNORE | CF_RECURSE,
	cvs_remove_init,
	NULL,
	cvs_remove_remote,
	cvs_remove_local,
	NULL,
	NULL,
	CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2 | CVS_CMD_ALLOWSPEC
};

static int
cvs_remove_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'f':
			force_remove = 1;
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

	argc -= optind;
	argv += optind;

	*arg = optind;
	return (0);
}


static int
cvs_remove_remote(CVSFILE *cf, void *arg)
{
	int ret;
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		else
			cvs_senddir(root, cf);
		return (0);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cvs_remove_file(fpath) < 0)
		return (CVS_EX_FILE);

	cvs_sendentry(root, cf);

	if (cf->cf_cvstat != CVS_FST_LOST && force_remove != 1) {
		if (cf->cf_cvstat != CVS_FST_ADDED)
			cvs_sendreq(root, CVS_REQ_MODIFIED, cf->cf_name);

		if (cf->cf_flags & CVS_FILE_ONDISK)
			cvs_sendfile(root, fpath);
	}

	return (0);
}

static int
cvs_remove_local(CVSFILE *cf, void *arg)
{
	int existing, l, removed;
	char buf[MAXPATHLEN], fpath[MAXPATHLEN];
	CVSENTRIES *entf;
	struct cvs_ent *ent;

	existing = removed = 0;
	entf = (CVSENTRIES *)cf->cf_entry;

	if (cf->cf_type == DT_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Removing %s", cf->cf_name);
		return (0);
	}

	if (cvs_cmdop != CVS_OP_SERVER) {
		cvs_file_getpath(cf, fpath, sizeof(fpath));

		if (cvs_remove_file(fpath) < 0)
			return (CVS_EX_FILE);
	}

	if (nuked == 0) {
		existing++;
		if (verbosity > 1)
			cvs_log(LP_WARN, "file `%s' still in working directory",
			    cf->cf_name);
	} else if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
		if (verbosity > 1)
			cvs_log(LP_WARN, "nothing known about `%s'",
			    cf->cf_name);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_ADDED) {
		if (cvs_ent_remove(entf, cf->cf_name, 0) == -1)
			return (CVS_EX_FILE);

		l = snprintf(buf, sizeof(buf), "%s/%s%s",
		    CVS_PATH_CVSDIR, cf->cf_name, CVS_DESCR_FILE_EXT);
		if (l == -1 || l >= (int)sizeof(buf)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", buf);
			return (CVS_EX_DATA);
		}

		if (cvs_unlink(buf) == -1)
			return (CVS_EX_FILE);

		if (verbosity > 1)
			cvs_log(LP_NOTICE, "removed `%s'", cf->cf_name);
		return (0);
	} else if (cf->cf_cvstat == CVS_FST_REMOVED) {
		if (verbosity > 1 )
			cvs_log(LP_WARN,
			    "file `%s' already scheduled for removal",
			    cf->cf_name);
		return (0);
	} else {
		if ((ent = cvs_ent_get(entf, cf->cf_name)) == NULL)
			return (CVS_EX_DATA);

		/* Prefix revision with `-' */
		ent->ce_status = CVS_ENT_REMOVED;
		entf->cef_flags &= ~CVS_ENTF_SYNC;

		if (verbosity > 1)
			cvs_log(LP_NOTICE, "scheduling file `%s' for removal",
			    cf->cf_name);
		removed++;
	}

	if (removed != 0) {
		if (verbosity > 0)
			cvs_log(LP_NOTICE, "use '%s commit' to remove %s "
			    "permanently", __progname,
			    (removed == 1) ? "this file" : "these files");
		return (0);
	}

	if (existing != 0) {
		cvs_log(LP_WARN, ((existing == 1) ?
		    "%d file exists; remove it first" :
		    "%d files exist; remove them first"), existing);
		return (0);
	}

	return (0);
}

/*
 * cvs_remove_file()
 *
 * Physically remove the file.
 * Used by both remote and local handlers.
 * Returns 0 on success, -1 on failure.
 */
static int
cvs_remove_file(const char *fpath)
{
	struct stat st;

	/* if -f option is used, physically remove the file */
	if (force_remove == 1) {
		if (cvs_unlink(fpath) == -1)
			return (-1);
		nuked++;
	} else {
		if ((stat(fpath, &st) == -1) && (errno == ENOENT))
			nuked++;
	}

	return (0);
}
