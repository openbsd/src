/*	$OpenBSD: checkout.c,v 1.40 2005/09/15 17:01:10 xsa Exp $	*/
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
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


#define CVS_LISTMOD	1
#define CVS_STATMOD	2

static int	cvs_checkout_init(struct cvs_cmd *, int, char **, int *);
static int	cvs_checkout_pre_exec(struct cvsroot *);

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, CVS_REQ_CO, "checkout",
	{ "co", "get" },
	"Checkout sources for editing",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPRr:st:",
	NULL,
	0,
	cvs_checkout_init,
	cvs_checkout_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

struct cvs_cmd cvs_cmd_export = {
	CVS_OP_EXPORT, CVS_REQ_EXPORT, "export",
	{ "ex", "exp" },
	"Extract copy of a module without management directories",
	"[-flNnR] [-d dir] [-k mode] -D date | -r tag module ...",
	"D:d:fk:lNnRr:",
	NULL,
	0,
	cvs_checkout_init,
	cvs_checkout_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR
};

static char *date, *tag, *koptstr, *tgtdir, *rcsid;
static int statmod = 0;
static int shorten = 0;
static int usehead = 0;
static int kflag = RCS_KWEXP_DEFAULT;

/* modules */
static char **co_mods;
static int    co_nmod;

static int
cvs_checkout_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;
	RCSNUM *rcs;

	date = tag = koptstr = tgtdir = rcsid = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			break;
		case 'c':
			statmod = CVS_LISTMOD;
			break;
		case 'D':
			date = optarg;
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 'd':
			tgtdir = optarg;
			shorten = 1;
			break;
		case 'f':
			usehead = 1;
			break;
		case 'j':
			break;
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
		case 'P':
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 'N':
			shorten = 0;
			break;
		case 'p':
			cvs_noexec = 1;	/* no locks will be created */
			break;
		case 'r':
			tag = optarg;
			cmd->cmd_flags |= CVS_CMD_PRUNEDIRS;
			break;
		case 's':
			statmod = CVS_STATMOD;
			break;
		case 't':
			rcsid = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	co_mods = argv;
	co_nmod = argc;

	if ((statmod == 0) && (argc == 0)) {
		cvs_log(LP_ABORT,
		    "must specify at least one module or directory");
		return (-1);
	}

	if (statmod && (argc > 0)) {
		cvs_log(LP_ABORT,  "-c and -s must not get any arguments");
		return (-1);
	}

	/* `export' command exceptions */
	if (cvs_cmdop == CVS_OP_EXPORT) {
		if (!tag && !date) {
			cvs_log(LP_ABORT, "must specify a tag or date");
			return (-1);
		}

		/* we don't want numerical revisions here */
		if (tag && (rcs = rcsnum_parse(tag)) != NULL) {
			cvs_log(LP_ABORT, "tag `%s' must be a symbolic tag",
			    tag);
			rcsnum_free(rcs);
			return (-1);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_checkout_pre_exec(struct cvsroot *root)
{
	int i;
	char *sp;

	for (i = 0; i < co_nmod; i++) {
		if ((sp = strchr(co_mods[i], '/')) != NULL)
			*sp = '\0';

		if ((mkdir(co_mods[i], 0755) == -1) && (errno != EEXIST)) {
			cvs_log(LP_ERRNO, "can't create base directory '%s'",
			    co_mods[i]);
			return (CVS_EX_DATA);
		}

		if (cvs_mkadmin(co_mods[i], root->cr_str, co_mods[i],
		    NULL, NULL, 0) < 0) {
			cvs_log(LP_ERR, "can't create base directory '%s'",
			    co_mods[i]);
			return (CVS_EX_DATA);
		}

		if (sp != NULL)
			*sp = '/';
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		/*
		 * These arguments are for the expand-modules
		 * command that we send to the server before requesting
		 * a checkout.
		 */
		for (i = 0; i < co_nmod; i++)
			if (cvs_sendarg(root, co_mods[i], 0) < 0)
				return (CVS_EX_PROTO);
		if (cvs_sendreq(root, CVS_REQ_DIRECTORY, ".") < 0)
			return (CVS_EX_PROTO);
		if (cvs_sendln(root, root->cr_dir) < 0)
			return (CVS_EX_PROTO);

		if (cvs_sendreq(root, CVS_REQ_XPANDMOD, NULL) < 0)
			cvs_log(LP_ERR, "failed to expand module");

		if ((usehead == 1) && (cvs_sendarg(root, "-f", 0) < 0))
			return (CVS_EX_PROTO);

		if ((tgtdir != NULL) &&
		    ((cvs_sendarg(root, "-d", 0) < 0) ||
		    (cvs_sendarg(root, tgtdir, 0) < 0)))
			return (CVS_EX_PROTO);

		if ((shorten == 0) && cvs_sendarg(root, "-N", 0) < 0)
			return (CVS_EX_PROTO);

		if ((cvs_cmd_checkout.cmd_flags & CVS_CMD_PRUNEDIRS) &&
		    (cvs_sendarg(root, "-P", 0) < 0))
			return (CVS_EX_PROTO);

		for (i = 0; i < co_nmod; i++)
			if (cvs_sendarg(root, co_mods[i], 0) < 0)
				return (CVS_EX_PROTO);

		if ((statmod == CVS_LISTMOD) &&
		    (cvs_sendarg(root, "-c", 0) < 0))
			return (CVS_EX_PROTO);
		else if ((statmod == CVS_STATMOD) &&
		    (cvs_sendarg(root, "-s", 0) < 0))
			return (CVS_EX_PROTO);

		if ((tag != NULL) && ((cvs_sendarg(root, "-r", 0) < 0) ||
		    (cvs_sendarg(root, tag, 0) < 0)))
			return (CVS_EX_PROTO);

		if ((date != NULL) && ((cvs_sendarg(root, "-D", 0) < 0) ||
		    (cvs_sendarg(root, date, 0) < 0)))
			return (CVS_EX_PROTO);
	}
	return (0);
}
