/*	$OpenBSD: swapgeneric.c,v 1.6 1998/05/11 21:35:31 niklas Exp $	*/
/*	$NetBSD: swapgeneric.c,v 1.13 1996/10/13 03:36:01 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)swapgeneric.c	7.11 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <dev/cons.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>

#include "hp.h"
#include "ra.h"
#include "hdc.h"
#include "sd.h"
#include "st.h"

void	gets __P((char *));

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	argdev = NODEV;
dev_t	dumpdev = NODEV;
int	nswap;
struct	swdevt swdevt[] = {
	{ -1,	1,	0 },
	{ -1,	1,	0 },
	{ -1,	1,	0 },
	{ 0,	0,	0 },
};
long	dumplo;
int	dmmin, dmmax, dmtext;

int (*mountroot) __P((void)) = dk_mountroot;

extern	struct cfdriver hp_cd;
extern	struct cfdriver ra_cd;
extern	struct cfdriver hd_cd;
extern	struct cfdriver sd_cd;
extern	struct cfdriver st_cd;

struct	ngcconf {
	struct	cfdriver *ng_cf;
	dev_t	ng_root;
} ngcconf[] = {
#if NHP > 0
	{ &hp_cd,	makedev(0, 0), },
#endif
#if NRA > 0
	{ &ra_cd,	makedev(9, 0), },
#endif
#if NHDC > 0
	{ &hd_cd,	makedev(19, 0), },
#endif
#if NSD > 0
	{ &sd_cd,	makedev(20, 0), },
#endif
#if NST > 0
	{ &st_cd,	makedev(21, 0), },
#endif
	{ 0 },
};

void
setconf()
{
	struct	ngcconf *nc;
	register char *cp, *gp;
	int unit, swaponroot = 0, i;
	char name[128];

	if (rootdev != NODEV)
		goto doswap;
	unit = 0;
	/*
	 * First try new config devices.
	 */
	if (boothowto & RB_ASKNAME) {
nretry:
		swaponroot = 0;
		printf("root device? ");
		gets(name);
		if (name[strlen(name) - 1] == '*')
			name[strlen(name) - 1] = swaponroot++;
		for (nc = ngcconf; nc->ng_cf; nc++)
			for (i = 0; i < nc->ng_cf->cd_ndevs; i++)
				if (nc->ng_cf->cd_devs[i] &&
				    strcmp(name, ((struct device *)
				    (nc->ng_cf->cd_devs[i]))->dv_xname) == 0)
					goto ngotit;

		printf("Use one of ");
		for (nc = ngcconf; nc->ng_cf; nc++)
			printf("%s%%d ", nc->ng_cf->cd_name);
		printf("\n");

		goto nretry;
ngotit:
		rootdev = makedev(major(nc->ng_root), i * 8);
		goto doswap;

	} else {
		for (nc = ngcconf; nc->ng_cf; nc++)
			for (i = 0; i < nc->ng_cf->cd_ndevs; i++)
				if (nc->ng_cf->cd_devs[i]) {
					printf("root on %s%d\n",
					    nc->ng_cf->cd_name, i);
					rootdev = makedev(major(nc->ng_root),
					    i * 8);
					goto doswap;
				}

	}

	printf("no suitable root\n");
	asm("halt");

doswap:
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev)+1);
	/* swap size and dumplo set during autoconfigure */
	if (swaponroot)
		rootdev = dumpdev;
}

void
gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		cnputc(c = (cngetc()&0x7f));
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '#':
		case '\177':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u'&037:
			lp = cp;
			cnputc('\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
