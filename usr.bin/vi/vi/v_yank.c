/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)v_yank.c	10.8 (Berkeley) 5/18/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_yank -- [buffer][count]y[count][motion]
 *	     [buffer][count]Y
 *	Yank text (or lines of text) into a cut buffer.
 *
 * !!!
 * Historic vi moved the cursor to the from MARK if it was before the current
 * cursor and on a different line, e.g., "yk" moves the cursor but "yj" and
 * "yl" do not.  Unfortunately, it's too late to change this now.  Matching
 * the historic semantics isn't easy.  The line number was always changed and
 * column movement was usually relative.  However, "y'a" moved the cursor to
 * the first non-blank of the line marked by a, while "y`a" moved the cursor
 * to the line and column marked by a.  Hopefully, the motion component code
 * got it right...   Unlike delete, we make no adjustments here.
 *
 * PUBLIC: int v_yank __P((SCR *, VICMD *));
 */
int
v_yank(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	if (cut(sp,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, &vp->m_start,
	    &vp->m_stop, F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0))
		return (1);

	/* Cursor movements reset the relative cursor position. */
	F_CLR(vp, VM_RCM_MASK);
	F_SET(vp, VM_RCM_SET);

	sp->rptlines[L_YANKED] += (vp->m_stop.lno - vp->m_start.lno) + 1;
	return (0);
}
