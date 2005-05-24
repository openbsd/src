/*	$OpenBSD: checkout.c,v 1.23 2005/05/24 20:04:43 joris Exp $	*/
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
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"


#define CVS_LISTMOD    1
#define CVS_STATMOD    2

static int cvs_checkout_init     (struct cvs_cmd *, int, char **, int *);
static int cvs_checkout_pre_exec (struct cvsroot *);

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, CVS_REQ_CO, "checkout",
	{ "co", "get" },
	"Checkout sources for editing",
	"[-AcflNnPpRs] [-D date | -r rev] [-d dir] [-j rev] [-k mode] "
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

static char *date, *rev, *koptstr, *tgtdir, *rcsid;
static int statmod = 0;
static int shorten = 1;
static int usehead = 0;
static int kflag = RCS_KWEXP_DEFAULT;

/* modules */
static char **co_mods;
static int    co_nmod;

static int
cvs_checkout_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	date = rev = koptstr = tgtdir = rcsid = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			break;
		case 'c':
			statmod = CVS_LISTMOD;
			break;
		case 'D':
			date = optarg;
			break;
		case 'd':
			tgtdir = optarg;
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
		case 'N':
			shorten = 0;
			break;
		case 'p':
			cvs_noexec = 1;	/* no locks will be created */
			break;
		case 'r':
			rev = optarg;
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

	if (!statmod && (argc == 0)) {
		cvs_log(LP_ERR,
		    "must specify at least one module or directory");
		return (CVS_EX_USAGE);
	}

	if (statmod && (argc > 0)) {
		cvs_log(LP_ERR,  "-c and -s must not get any arguments");
		return (CVS_EX_USAGE);
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

		if (cvs_mkadmin(co_mods[i], root->cr_str, co_mods[i]) < 0) {
			cvs_log(LP_ERROR, "can't create base directory '%s'",
			    co_mods[i]);
			return (CVS_EX_DATA);
		}

		if (sp != NULL)
			*sp = '/';
	}

	if (root->cr_method != CVS_METHOD_LOCAL) {
		for (i = 0; i < co_nmod; i++)
			if (cvs_sendarg(root, co_mods[i], 0) < 0)
				return (CVS_EX_PROTO);
		if (cvs_sendreq(root, CVS_REQ_DIRECTORY, ".") < 0)
			return (CVS_EX_PROTO);
		if (cvs_sendln(root, root->cr_dir) < 0)
			return (CVS_EX_PROTO);

		if (cvs_sendreq(root, CVS_REQ_XPANDMOD, NULL) < 0)
			cvs_log(LP_ERR, "failed to expand module");

		if (usehead && (cvs_sendarg(root, "-f", 0) < 0))
			return (CVS_EX_PROTO);

		if ((tgtdir != NULL) &&
		    ((cvs_sendarg(root, "-d", 0) < 0) ||
		    (cvs_sendarg(root, tgtdir, 0) < 0)))
			return (CVS_EX_PROTO);

		if (!shorten && cvs_sendarg(root, "-N", 0) < 0)
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
	}
	return (0);
}
