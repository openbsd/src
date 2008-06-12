/*	$OpenBSD: iostat.c,v 1.29 2008/06/12 17:53:49 beck Exp $	*/
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
static char rcsid[] = "$OpenBSD: iostat.c,v 1.29 2008/06/12 17:53:49 beck Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/mount.h>

#include <string.h>
#include <stdlib.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

#include "dkstats.h"
extern struct _disk	cur, last;
struct bcachestats	bclast, bccur;

static double etime;

static void numlabels(void);

#define ATIME(x,y) ((double)x[y].tv_sec + \
        ((double)x[y].tv_usec / (double)1000000))

#define NFMT "%-6.6s  %8.0f %8.0f  %6.0f %6.0f  %4.1f"
#define SFMT "%-6.6s  %8s %8s  %6s %6s  %4s"
#define	BCSCOL	50

WINDOW *
openiostat(void)
{
	bzero(&bccur, sizeof(bccur));
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
	int mib[3];
	size_t size;

	if (cur.dk_ndrive == 0)
		return;
	dkreadstats();

	bclast = bccur;
	mib[0] = CTL_VFS;
	mib[1] = VFS_GENERIC;
	mib[2] = VFS_BCACHESTAT;
	size = sizeof(bccur);
	if (sysctl(mib, 3, &bccur, &size, NULL, 0) < 0)
		mvwaddstr(wnd, 20, 0, "cannot get vfs.bcachestat");
	if (bclast.numbufs == 0)
		bclast = bccur;
}

void
labeliostat(void)
{
	mvwprintw(wnd, 1, 0, SFMT, "Device", "rKBytes", "wKBytes", "rtps",
	    "wtps", "sec");
	mvwprintw(wnd, 1, BCSCOL + 16, "numbufs");
	mvwprintw(wnd, 2, BCSCOL + 16, "freebufs");
	mvwprintw(wnd, 3, BCSCOL + 16, "numbufpages");
	mvwprintw(wnd, 4, BCSCOL + 16, "numfreepages");
	mvwprintw(wnd, 5, BCSCOL + 16, "numdirtypages");
	mvwprintw(wnd, 6, BCSCOL + 16, "numcleanpages");
	mvwprintw(wnd, 7, BCSCOL + 16, "pendingwrites");
	mvwprintw(wnd, 8, BCSCOL + 16, "pendingreads");
	mvwprintw(wnd, 9, BCSCOL + 16, "numwrites");
	mvwprintw(wnd, 10, BCSCOL + 16, "numreads");
	mvwprintw(wnd, 11, BCSCOL + 16, "cachehits");
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

	mvwprintw(wnd, 1, BCSCOL, "%15ld", bccur.numbufs);
	mvwprintw(wnd, 2, BCSCOL, "%15ld", bccur.freebufs);
	mvwprintw(wnd, 3, BCSCOL, "%15ld", bccur.numbufpages);
	if (bccur.numfreepages)
		mvwprintw(wnd, 4, BCSCOL, "%15ld", bccur.numfreepages);
	else
		mvwprintw(wnd, 4, BCSCOL, "%15s", "");
	if (bccur.numdirtypages)
		mvwprintw(wnd, 5, BCSCOL, "%15ld", bccur.numdirtypages);
	else
		mvwprintw(wnd, 5, BCSCOL, "%15s", "");
	if (bccur.numcleanpages)
		mvwprintw(wnd, 6, BCSCOL, "%15ld", bccur.numcleanpages);
	else
		mvwprintw(wnd, 6, BCSCOL, "%15s", "");
	if (bccur.pendingwrites)
		mvwprintw(wnd, 7, BCSCOL, "%15ld", bccur.pendingwrites);
	else
		mvwprintw(wnd, 7, BCSCOL, "%15s", "");
	if (bccur.pendingreads)
		mvwprintw(wnd, 8, BCSCOL, "%15ld", bccur.pendingreads);
	else
		mvwprintw(wnd, 8, BCSCOL, "%15s", "");
	if (bccur.numwrites - bclast.numwrites)
		mvwprintw(wnd, 9, BCSCOL, "%15ld",
		    bccur.numwrites - bclast.numwrites);
	else
		mvwprintw(wnd, 9, BCSCOL, "%15s", "");
	if (bccur.numreads - bclast.numreads)
		mvwprintw(wnd, 10, BCSCOL, "%15ld",
		    bccur.numreads - bclast.numreads);
	else
		mvwprintw(wnd, 10, BCSCOL, "%15s", "");
	if (bccur.cachehits - bclast.cachehits)
		mvwprintw(wnd, 11, BCSCOL, "%15ld",
		    bccur.cachehits - bclast.cachehits);
	else
		mvwprintw(wnd, 11, BCSCOL, "%15s", "");
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
