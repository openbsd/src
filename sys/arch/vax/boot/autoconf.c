/*	$NetBSD: autoconf.c,v 1.5 1996/03/07 23:27:06 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */
		


#include "sys/param.h"
#include "../include/mtpr.h"
#include "../include/sid.h"
#include "vaxstand.h"

int	nmba=0, nuba=0, nbi=0,nsbi=0,nuda=0;
int	*mbaaddr, *ubaaddr;
int	*udaaddr, *uioaddr, tmsaddr;

static int mba750[]={0xf28000,0xf2a000,0xf2c000};
static int uba750[]={0xf30000,0xf32000};
static int uio750[]={0xfc0000,0xf80000};
static int uda750[]={0772150};

/* 11/780's only have 4, 8600 have 8 of these. */
static int mba780[]={0x20010000,0x20012000,0x20014000,0x20016000,
	0x22010000,0x22012000,0x22014000,0x22016000};
static int uba780[]={0x20006000,0x20008000,0x2000a000,0x2000c000,
	0x22006000,0x22008000,0x2200a000,0x2200c000};
static int uio780[]={0x20100000,0x20140000,0x20180000,0x201c0000,
	0x22100000,0x22140000,0x22180000,0x221c0000};

static int uba630[]={0x20087800};
static int uio630[]={0x30000000};
#define qbdev(csr) (((csr) & 017777)-0x10000000)
static int uda630[]={qbdev(0772150),qbdev(0760334)};
/*
 * Autoconf routine is really stupid; but it actually don't
 * need any intelligence. We just assume that all possible
 * devices exists on each cpu. Fast & easy.
 */

autoconf()
{
	int i = MACHID(mfpr(PR_SID));

	switch (i) {

	default:
		printf("CPU type %d not supported by boot\n",i);
		asm("halt");

	case VAX_8600:
		nmba = 8;
		nuba = 8;
		nuda = 1;
		mbaaddr = mba780;
		ubaaddr = uba780;
		udaaddr = uda750;
		uioaddr = uio780;
		tmsaddr = 0774500;
		break;

	case VAX_780:
		nmba = 4;
		nuba = 4;
		nuda = 1;
		mbaaddr = mba780;
		ubaaddr = uba780;
		udaaddr = uda750;
		uioaddr = uio780;
		tmsaddr = 0774500;
		break;

	case VAX_750:
		nmba = 3;
		nuba = 2;
		nuda = 1;
		mbaaddr = mba750;
		ubaaddr = uba750;
		udaaddr = uda750;
		uioaddr = uio750;
		tmsaddr = 0774500;
		break;

	case VAX_650:	/* the same for uvaxIII */
	case VAX_78032:
		nuba = 1;
		nuda = 2;
		ubaaddr = uba630;
		udaaddr = uda630;
		uioaddr = uio630;
		tmsaddr = qbdev(0774500);
		break;
	}
}

