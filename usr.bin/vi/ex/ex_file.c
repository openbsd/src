/*	$OpenBSD: ex_file.c,v 1.6 2009/10/27 23:59:47 deraadt Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"

/*
 * ex_file -- :f[ile] [name]
 *	Change the file's name and display the status line.
 *
 * PUBLIC: int ex_file(SCR *, EXCMD *);
 */
int
ex_file(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	CHAR_T *p;
	FREF *frp;

	NEEDFILE(sp, cmdp);

	switch (cmdp->argc) {
	case 0:
		break;
	case 1:
		frp = sp->frp;

		/* Make sure can allocate enough space. */
		if ((p = v_strdup(sp,
		    cmdp->argv[0]->bp, cmdp->argv[0]->len)) == NULL)
			return (1);

		/* If already have a file name, it becomes the alternate. */
		if (!F_ISSET(frp, FR_TMPFILE))
			set_alt_name(sp, frp->name);

		/* Free the previous name. */
		free(frp->name);
		frp->name = p;

		/*
		 * The file has a real name, it's no longer a temporary,
		 * clear the temporary file flags.
		 */
		F_CLR(frp, FR_TMPEXIT | FR_TMPFILE);

		/* Have to force a write if the file exists, next time. */
		F_SET(frp, FR_NAMECHANGE);

		/* Notify the screen. */
		(void)sp->gp->scr_rename(sp, sp->frp->name, 1);
		break;
	default:
		abort();
	}
	msgq_status(sp, sp->lno, MSTAT_SHOWLAST);
	return (0);
}
