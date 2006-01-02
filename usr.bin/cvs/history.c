/*	$OpenBSD: history.c,v 1.25 2006/01/02 08:11:56 xsa Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "proto.h"

#define CVS_HISTORY_MAXMOD	16

/* history flags */
#define CVS_HF_A	0x01
#define CVS_HF_C	0x02
#define CVS_HF_E	0x04
#define CVS_HF_L	0x08
#define CVS_HF_M	0x10
#define CVS_HF_O	0x20
#define CVS_HF_T	0x40
#define CVS_HF_W	0x80

#define CVS_HF_EXCL	(CVS_HF_C|CVS_HF_E|CVS_HF_M|CVS_HF_O|CVS_HF_T|CVS_HF_X)

static int	cvs_history_init(struct cvs_cmd *, int, char **, int *);
#if 0
static void	cvs_history_print(struct cvs_hent *);
#endif
static int	cvs_history_pre_exec(struct cvsroot *);

extern char *__progname;

struct cvs_cmd cvs_cmd_history = {
	CVS_OP_HISTORY, CVS_REQ_HISTORY, "history",
	{ "hi", "his" },
	"Show repository access history",
	"[-aceloTw] [-b str] [-D date] [-f file] [-m module] [-n module] "
	"[-p path] [-r rev] [-t tag] [-u user] [-x ACEFGMORTUW] [-z tz]",
	"ab:cD:ef:lm:n:op:r:Tt:u:wx:z:",
	NULL,
	0,
	cvs_history_init,
	cvs_history_pre_exec,
	NULL,
	NULL,
	NULL,
	NULL,
	CVS_CMD_SENDDIR
};

static int flags = 0;
static char *date, *rev, *user, *tag;
static char *zone = "+0000";
static u_int nbmod = 0;
static u_int rep = 0;
static char *modules[CVS_HISTORY_MAXMOD];

static int
cvs_history_init(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	date = rev = user = tag = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			flags |= CVS_HF_A;
			break;
		case 'b':
			break;
		case 'c':
			rep++;
			flags |= CVS_HF_C;
			break;
		case 'D':
			break;
		case 'e':
			rep++;
			flags |= CVS_HF_E;
			break;
		case 'f':
			break;
		case 'l':
			flags |= CVS_HF_L;
			break;
		case 'm':
			rep++;
			flags |= CVS_HF_M;
			if (nbmod == CVS_HISTORY_MAXMOD) {
				cvs_log(LP_ERR, "too many `-m' options");
				return (CVS_EX_USAGE);
			}
			modules[nbmod++] = optarg;
			break;
		case 'n':
			break;
		case 'o':
			rep++;
			flags |= CVS_HF_O;
			break;
		case 'r':
			rev = optarg;
			break;
		case 'T':
			rep++;
			flags |= CVS_HF_T;
			break;
		case 't':
			tag = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 'w':
			flags |= CVS_HF_W;
			break;
		case 'x':
			rep++;
			break;
		case 'z':
			zone = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	if (rep > 1) {
		cvs_log(LP_ERR,
		    "Only one report type allowed from: \"-Tcomxe\"");
		return (CVS_EX_USAGE);
	} else if (rep == 0)
		flags |= CVS_HF_O;    /* use -o as default */

	*arg = optind;
	return (0);
}

static int
cvs_history_pre_exec(struct cvsroot *root)
{
	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (flags & CVS_HF_A)
			cvs_sendarg(root, "-a", 0);

		if (flags & CVS_HF_C)
			cvs_sendarg(root, "-c", 0);

		if (flags & CVS_HF_O)
			cvs_sendarg(root, "-o", 0);

		if (date != NULL) {
			cvs_sendarg(root, "-D", 0);
			cvs_sendarg(root, date, 0);
		}

		if (rev != NULL) {
			cvs_sendarg(root, "-r", 0);
			cvs_sendarg(root, rev, 0);
		}

		if (tag != NULL) {
			cvs_sendarg(root, "-t", 0);
			cvs_sendarg(root, tag, 0);
		}

		/* if no user is specified, get login name of command issuer */
		if (!(flags & CVS_HF_A) && (user == NULL)) {
			if ((user = getlogin()) == NULL)
				fatal("cannot get login name");
		}

		if (!(flags & CVS_HF_A)) {
			cvs_sendarg(root, "-u", 0);
			cvs_sendarg(root, user, 0);
		}

		cvs_sendarg(root, "-z", 0);
		cvs_sendarg(root, zone, 0);
	}

	return (0);
}


#if 0
static void
cvs_history_print(struct cvs_hent *hent)
{
	struct tm etime;

	if (localtime_r(&(hent->ch_date), &etime) == NULL) {
		cvs_log(LP_ERR, "failed to convert timestamp to structure");
		return;
	}

	printf("%c %4d-%02d-%02d %02d:%02d +%04d %-16s %-16s\n",
	    hent->ch_event, etime.tm_year + 1900, etime.tm_mon + 1,
	    etime.tm_mday, etime.tm_hour, etime.tm_min,
	    0, hent->ch_user, hent->ch_repo);
}
#endif
