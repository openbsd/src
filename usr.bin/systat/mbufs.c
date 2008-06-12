/*	$OpenBSD: mbufs.c,v 1.18 2008/06/12 22:26:01 canacar Exp $	*/
/*	$NetBSD: mbufs.c,v 1.2 1995/01/20 08:52:02 jtc Exp $	*/

/*-
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <paths.h>
#include "systat.h"


void print_mb(void);
int read_mb(void);
int select_mb(void);
static void showmbuf(int);

static struct mbstat mb;

char *mtnames[] = {
	"free",
	"data",
	"headers",
	"sockets",
	"pcbs",
	"routes",
	"hosts",
	"arps",
	"socknames",
	"zombies",
	"sockopts",
	"frags",
	"rights",
	"ifaddrs",
};

#define NUM_TYPES (sizeof(mb.m_mtypes) / sizeof(mb.m_mtypes[0]))
#define	NNAMES	(sizeof (mtnames) / sizeof (mtnames[0]))

int mb_index[NUM_TYPES];
int mbuf_cnt = 0;


field_def fields_mb[] = {
	{"TYPE", 6, 16, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"VALUE", 5, 8, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"", 40, 80, 1, FLD_ALIGN_BAR, -1, 0, 0, 60},
};

#define FIELD_ADDR(x) (&fields_mb[x])

#define FLD_MB_NAME	FIELD_ADDR(0)
#define FLD_MB_VALUE	FIELD_ADDR(1)
#define FLD_MB_BAR	FIELD_ADDR(2)

/* Define views */
field_def *view_mb_0[] = {
	FLD_MB_NAME, FLD_MB_VALUE, FLD_MB_BAR, NULL
};


/* Define view managers */
struct view_manager mbuf_mgr = {
	"Mbufs", select_mb, read_mb, NULL, print_header,
	print_mb, keyboard_callback, NULL, NULL
};

field_view views_mb[] = {
	{view_mb_0, "mbufs", '4', &mbuf_mgr},
	{NULL, NULL, 0, NULL}
};


int
select_mb(void)
{
	int i, w = 50;

	read_mb();
	for (i = 0; i < NUM_TYPES; i++)
		if (w < (5 * mb.m_mtypes[i] / 4))
			w = 5 * mb.m_mtypes[i] / 4;

	w -= w % 10;
	FLD_MB_BAR->arg = w;

	return (0);
}

int
read_mb(void)
{
	int mib[2], i;
	size_t size = sizeof (mb);

	mib[0] = CTL_KERN;
	mib[1] = KERN_MBSTAT;

	if (sysctl(mib, 2, &mb, &size, NULL, 0) < 0) {
		error("sysctl(KERN_MBSTAT) failed");
		return 1;
	}

	mbuf_cnt = 0;
	memset(mb_index, 0, sizeof(mb_index));

	for (i = 0; i < NUM_TYPES; i++) {
		if (mb.m_mtypes[i])
			mb_index[mbuf_cnt++] = i;
	}

	num_disp = mbuf_cnt;

	return 0;
}


void
print_mb(void)
{
	int n, count = 0;

	for (n = dispstart; n < num_disp; n++) {
		showmbuf(n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

int
initmembufs(void)
{
	field_view *v;

	for (v = views_mb; v->name != NULL; v++)
		add_view(v);

	return(1);
}


static void
showmbuf(int m)
{
	int i;

	i = mb_index[m];

	if (i < NNAMES)
		print_fld_str(FLD_MB_NAME, mtnames[i]);
	else
		print_fld_uint(FLD_MB_NAME, i);

	print_fld_uint(FLD_MB_VALUE, mb.m_mtypes[i]);
	print_fld_bar(FLD_MB_BAR, mb.m_mtypes[i]);

	end_line();
}


