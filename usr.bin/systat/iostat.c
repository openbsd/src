/*	$OpenBSD: iostat.c,v 1.28 2007/05/30 05:20:58 otto Exp $	*/
/*	$NetBSD: iostat.c,v 1.5 1996/05/10 23:16:35 thorpej Exp $	*/

/*
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)iostat.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: iostat.c,v 1.28 2007/05/30 05:20:58 otto Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

#include "dkstats.h"
extern struct _disk	cur, last;

static double etime;

static void numlabels(void);

#define ATIME(x,y) ((double)x[y].tv_sec + \
        ((double)x[y].tv_usec / (double)1000000))

#define NFMT "%-8.8s  %14.0f %14.0f  %10.0f %10.0f  %10.1f"
#define SFMT "%-8.8s  %14s %14s  %10s %10s  %10s"

WINDOW *
openiostat(void)
{
	return (subwin(stdscr, LINES-1-1, 0, 1, 0));
}

void
closeiostat(WINDOW *w)
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

int
initiostat(void)
{
	dkinit(1);
	dkreadstats();
	return (1);
}

void
fetchiostat(void)
{
	if (cur.dk_ndrive == 0)
		return;
	dkreadstats();
}

void
labeliostat(void)
{
	mvwprintw(wnd, 1, 0, SFMT, "Device", "rKBytes", "wKBytes", "rtps",
	    "wtps", "sec");
}

void
showiostat(void)
{
	int i;

	dkswap();

	etime = 0.0;
	for (i = 0; i < CPUSTATES; i++) {
		etime += cur.cp_time[i];
	}

	if (etime == 0.0)
		etime = 1.0;

	etime /= (float) hz;

	if (last.dk_ndrive != cur.dk_ndrive)
		labeliostat();

	if (cur.dk_ndrive == 0)
		return;

	numlabels();
}

void
numlabels(void)
{
	double rsum, wsum, rtsum, wtsum, mssum;
	int row, dn;

	row = 2;
	wmove(wnd, 0, 0);
	wclrtoeol(wnd);

	if (cur.dk_ndrive == 0) {
		mvwaddstr(wnd, row, 0, "No drives attached.");
		return;
	}

	rsum = wsum = rtsum = wtsum = mssum = 0.0;

	for (dn = 0; dn < cur.dk_ndrive; dn++) {
		rsum += cur.dk_rbytes[dn] / etime;
		wsum += cur.dk_wbytes[dn] / etime;
		rtsum += cur.dk_rxfer[dn] / etime;
		wtsum += cur.dk_wxfer[dn] / etime;
		mssum += ATIME(cur.dk_time, dn) / etime;
		mvwprintw(wnd, row++, 0, NFMT,
		    cur.dk_name[dn],
		    cur.dk_rbytes[dn] / 1024.0 / etime,
		    cur.dk_wbytes[dn] / 1024.0 / etime,
		    cur.dk_rxfer[dn] / etime,
		    cur.dk_wxfer[dn] / etime,
		    ATIME(cur.dk_time, dn) / etime);
	}
	mvwprintw(wnd, row++, 0, NFMT,
	    "Totals", rsum / 1024.0, wsum / 1024.0, rtsum, wtsum, mssum);
}

int
cmdiostat(char *cmd, char *args)
{
	wclear(wnd);
	labeliostat();
	refresh();
	return (1);
}
