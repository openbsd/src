/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 * Copyright (c) 1996
 *	Sven Verdoolaege. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ex_perl.c	8.6 (Berkeley) 3/18/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

#ifdef HAVE_PERL_INTERP
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

static int perl_eval(string)
	char *string;
{
	char *argv[2];

	argv[0] = string;
	argv[1] = NULL;
	perl_call_argv("_eval_", G_EVAL | G_DISCARD | G_KEEPERR, argv);
}
#else

static void
noperl(scrp)
	SCR *scrp;
{
	msgq(scrp, M_ERR, "306|Vi was not loaded with a Perl interpreter");
}
#endif

/* 
 * ex_perl -- :[line [,line]] perl [command]
 *	Run a command through the perl interpreter.
 *
 * PUBLIC: int ex_perl __P((SCR*, EXCMD *));
 */
int 
ex_perl(scrp, cmdp)
	SCR *scrp;
	EXCMD *cmdp;
{
#ifdef HAVE_PERL_INTERP
	CHAR_T *p;
	GS *gp;
	STRLEN length;
	size_t len;
	char *err, buf[64];

	/* Initialize the interpreter. */
	gp = scrp->gp;
	if (gp->perl_interp == NULL && perl_init(gp))
		return (1);

	/* Skip leading white space. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len; len > 0; --len, ++p)
			if (!isblank(*p))
				break;
	if (cmdp->argc == 0 || len == 0) {
		ex_emsg(scrp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	(void)snprintf(buf, sizeof(buf),
	    "$VI::ScreenId=%d;$VI::StartLine=%lu;$VI::StopLine=%lu",
	    scrp->id, cmdp->addr1.lno, cmdp->addr2.lno);
	perl_eval(buf);
	perl_eval(cmdp->argv[0]->bp);
	err = SvPV(GvSV(errgv),length);
	if (!length)
		return (0);

	err[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", err);
	return (1);
#else
	noperl(scrp);
	return (1);
#endif /* HAVE_PERL_INTERP */
}

/* 
 * ex_perldo -- :[line [,line]] perl [command]
 *	Run a set of lines through the perl interpreter.
 *
 * PUBLIC: int ex_perldo __P((SCR*, EXCMD *));
 */
int 
ex_perldo(scrp, cmdp)
	SCR *scrp;
	EXCMD *cmdp;
{
#ifdef HAVE_PERL_INTERP
	CHAR_T *p;
	GS *gp;
	STRLEN length;
	size_t len;
	int i;
	char *str, *argv[2];
	dSP;

	/* Initialize the interpreter. */
	gp = scrp->gp;
	if (gp->perl_interp == NULL && perl_init(gp))
		return (1);

	/* Skip leading white space. */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len; len > 0; --len, ++p)
			if (!isblank(*p))
				break;
	if (cmdp->argc == 0 || len == 0) {
		ex_emsg(scrp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}

	argv[0] = cmdp->argv[0]->bp;
	argv[1] = NULL;

	ENTER;
	SAVETMPS;
	for (i = cmdp->addr1.lno; i <= cmdp->addr2.lno; i++) {
		/*api_gline(scrp, i, argv+1, &len);*/
		api_gline(scrp, i, &str, &len);
		sv_setpvn(perl_get_sv("_", FALSE),str,len);
		perl_call_argv("_eval_", G_SCALAR | G_EVAL | G_KEEPERR, argv);
		str = SvPV(GvSV(errgv),length);
		if (length) break;
		SPAGAIN;
		if(SvTRUEx(POPs)) {
			str = SvPV(perl_get_sv("_", FALSE),len);
			api_sline(scrp, i, str, len);
		}
		PUTBACK;
	}
	FREETMPS;
	LEAVE;
	if (!length)
		return (0);

	str[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", str);
	return (1);
#else
	noperl(scrp);
	return (1);
#endif /* HAVE_PERL_INTERP */
}
