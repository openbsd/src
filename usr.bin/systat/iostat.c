/*	$OpenBSD: iostat.c,v 1.45 2015/01/16 00:03:37 deraadt Exp $	*/
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

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <paths.h>
#include "systat.h"

#include "dkstats.h"
extern struct _disk	cur, last;
struct bcachestats	bclast, bccur;

static double etime;

void showtotal(void);
void showdrive(int);
void print_io(void);
int read_io(void);
int select_io(void);
void showbcache(void);

#define ATIME(x,y) ((double)x[y].tv_sec + \
        ((double)x[y].tv_usec / (double)1000000))


field_def fields_io[] = {
	{"DEVICE", 8, 16, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"READ", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"WRITE", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"RTPS", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"WTPS", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"SEC", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 8, 19, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"STATS", 12, 15, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0}
};

#define FLD_IO_DEVICE	FIELD_ADDR(fields_io,0)
#define FLD_IO_READ	FIELD_ADDR(fields_io,1)
#define FLD_IO_WRITE	FIELD_ADDR(fields_io,2)
#define FLD_IO_RTPS	FIELD_ADDR(fields_io,3)
#define FLD_IO_WTPS	FIELD_ADDR(fields_io,4)
#define FLD_IO_SEC	FIELD_ADDR(fields_io,5)

/* This is a hack that stuffs bcache statistics to the last two columns! */
#define FLD_IO_SVAL	FIELD_ADDR(fields_io,6)
#define FLD_IO_SSTR	FIELD_ADDR(fields_io,7)

/* Define views */
field_def *view_io_0[] = {
	FLD_IO_DEVICE, FLD_IO_READ, FLD_IO_WRITE, FLD_IO_RTPS,
	FLD_IO_WTPS, FLD_IO_SEC, FLD_IO_SVAL, FLD_IO_SSTR, NULL
};


/* Define view managers */
struct view_manager iostat_mgr = {
	"Iostat", select_io, read_io, NULL, print_header,
	print_io, keyboard_callback, NULL, NULL
};


field_view views_io[] = {
	{view_io_0, "iostat", '2', &iostat_mgr},
	{NULL, NULL, 0, NULL}
};


int
select_io(void)
{
	num_disp = cur.dk_ndrive + 1;
	return (0);
}

int
read_io(void)
{
	int mib[3];
	size_t size;

	dkreadstats();
	dkswap();
	num_disp = cur.dk_ndrive + 1;

	bclast = bccur;
	mib[0] = CTL_VFS;
	mib[1] = VFS_GENERIC;
	mib[2] = VFS_BCACHESTAT;
	size = sizeof(bccur);

	if (sysctl(mib, 3, &bccur, &size, NULL, 0) < 0)
		error("cannot get vfs.bcachestat");

	if (bclast.numbufs == 0)
		bclast = bccur;

	return 0;
}


void
print_io(void)
{
	int n, count = 0;

	int curr;
	etime = naptime;

	/* XXX engine internals: save and restore curr_line for bcache */
	curr = curr_line;

	for (n = dispstart; n < num_disp - 1; n++) {
		showdrive(n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}


	if (maxprint == 0 || count < maxprint)
		showtotal();

	curr_line = curr;
	showbcache();
}

int
initiostat(void)
{
	field_view *v;

	dkinit(1);
	dkreadstats();

	bzero(&bccur, sizeof(bccur));

	for (v = views_io; v->name != NULL; v++)
		add_view(v);

	return(1);
}

void
showtotal(void)
{
	double rsum, wsum, rtsum, wtsum, mssum;
	int dn;

	rsum = wsum = rtsum = wtsum = mssum = 0.0;

	for (dn = 0; dn < cur.dk_ndrive; dn++) {
		rsum += cur.dk_rbytes[dn] / etime;
		wsum += cur.dk_wbytes[dn] / etime;
		rtsum += cur.dk_rxfer[dn] / etime;
		wtsum += cur.dk_wxfer[dn] / etime;
		mssum += ATIME(cur.dk_time, dn) / etime;
	}

	print_fld_str(FLD_IO_DEVICE, "Totals");
	print_fld_size(FLD_IO_READ, rsum);
	print_fld_size(FLD_IO_WRITE, wsum);
	print_fld_size(FLD_IO_RTPS, rtsum);
	print_fld_size(FLD_IO_WTPS, wtsum);
	print_fld_float(FLD_IO_SEC, mssum, 1);

	end_line();
}

void
showdrive(int dn)
{
	print_fld_str(FLD_IO_DEVICE, cur.dk_name[dn]);
	print_fld_size(FLD_IO_READ, cur.dk_rbytes[dn]/etime);
	print_fld_size(FLD_IO_WRITE, cur.dk_wbytes[dn]/ etime);
	print_fld_size(FLD_IO_RTPS, cur.dk_rxfer[dn] / etime);
	print_fld_size(FLD_IO_WTPS, cur.dk_wxfer[dn] / etime);
	print_fld_float(FLD_IO_SEC, ATIME(cur.dk_time, dn) / etime, 1);

	end_line();
}

void
showbcache(void)
{
	print_fld_str(FLD_IO_SSTR, "total pages");
	print_fld_ssize(FLD_IO_SVAL, bccur.numbufpages);
	end_line();

	print_fld_str(FLD_IO_SSTR, "dirty pages");
	print_fld_ssize(FLD_IO_SVAL, bccur.numdirtypages);
	end_line();

	print_fld_str(FLD_IO_SSTR, "delwri bufs");
	print_fld_ssize(FLD_IO_SVAL, bccur.delwribufs);
	end_line();

	print_fld_str(FLD_IO_SSTR, "busymap bufs");
	print_fld_ssize(FLD_IO_SVAL, bccur.busymapped);
	end_line();

	print_fld_str(FLD_IO_SSTR, "avail kvaslots");
	print_fld_ssize(FLD_IO_SVAL, bccur.kvaslots_avail);
	end_line();

	print_fld_str(FLD_IO_SSTR, "kvaslots");
	print_fld_ssize(FLD_IO_SVAL, bccur.kvaslots);
	end_line();

	print_fld_str(FLD_IO_SSTR, "pending writes");
	print_fld_ssize(FLD_IO_SVAL, bccur.pendingwrites);
	end_line();

	print_fld_str(FLD_IO_SSTR, "pending reads");
	print_fld_ssize(FLD_IO_SVAL, bccur.pendingreads);
	end_line();

	print_fld_str(FLD_IO_SSTR, "cache hits");
	print_fld_ssize(FLD_IO_SVAL, bccur.cachehits - bclast.cachehits);
	end_line();
}
