/*	$OpenBSD: swapgeneric.c,v 1.8 2001/04/09 00:59:31 hugh Exp $	*/
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

int (*mountroot) __P((void)) = NULL;

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
