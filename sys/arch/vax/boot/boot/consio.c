/*	$OpenBSD: consio.c,v 1.5 2002/08/09 20:26:45 jsyn Exp $ */
/*	$NetBSD: consio.c,v 1.13 2002/05/24 21:40:59 ragge Exp $ */
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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

#include "../vax/gencons.h"

#include "mtpr.h"
#include "sid.h"
#include "rpb.h"
#include "ka630.h"

#include "data.h"

void setup(void);

static void (*put_fp)(int)  = NULL;
static int (*get_fp)(void) = NULL;
static int (*test_fp)(void) = NULL;

void pr_putchar(int c);	/* putchar() using mtpr/mfpr */
int pr_getchar(void);
int pr_testchar(void);

void rom_putchar(int c);	/* putchar() using ROM routines */
int rom_getchar(void);
int rom_testchar(void);

int rom_putc;		/* ROM-address of put-routine */
int rom_getc;		/* ROM-address of get-routine */

/* Pointer to KA630 console page, initialized by ka630_consinit */
unsigned char	*ka630_conspage;

/* Function that initializes things for KA630 ROM console I/O */
void ka630_consinit(void);

/* Functions that use KA630 ROM for console I/O */
void ka630_rom_putchar(int c);
int ka630_rom_getchar(void);
int ka630_rom_testchar(void);

/* Also added such a thing for KA53 - MK-991208 */
unsigned char  *ka53_conspage;
void ka53_consinit(void);

void ka53_rom_putchar(int c);
int ka53_rom_getchar(void);
int ka53_rom_testchar(void);

void vxt_putchar(int c);
int vxt_getchar(void);
int vxt_testchar(void);

void putchar(int);
int getchar(void);
int testkey(void);
void consinit(void);
void _rtt(void);

void
putchar(int c)
{
	(*put_fp)(c);
	if (c == 10)
		(*put_fp)(13);		/* CR/LF */
}

int
getchar(void) 
{
	int c;

	do
		c = (*get_fp)() & 0177;
	while (c == 17 || c == 19);		/* ignore XON/XOFF */
	if (c < 96 && c > 64)
		c += 32;
	return c;
}

int
testkey(void)
{
	return (*test_fp)();
}

/*
 * setup() is called out of the startup files (start.s, srt0.s) and
 * initializes data which are globally used and is called before main().
 */
void 
consinit(void)
{
	put_fp = pr_putchar; /* Default */
	get_fp = pr_getchar;
	test_fp = pr_testchar;

	/*
	 * According to the vax_boardtype (vax_cputype is not specific
	 * enough to do that) we decide which method/routines to use
	 * for console I/O. 
	 * mtpr/mfpr are restricted to serial consoles, ROM-based routines
	 * support both serial and graphical consoles.
	 * We default to mtpr routines; so that we don't crash if
	 * it isn't a supported system.
	 */
	switch (vax_boardtype) {

	case VAX_BTYP_43:
	case VAX_BTYP_410:	  
	case VAX_BTYP_420:
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		test_fp = rom_testchar;
		rom_putc = 0x20040058;		/* 537133144 */
		rom_getc = 0x20040044;		/* 537133124 */
		break;

	case VAX_BTYP_VXT:
		put_fp = vxt_putchar;
		get_fp = vxt_getchar;
		test_fp = vxt_testchar;
		break;

	case VAX_BTYP_630:
		ka630_consinit();
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
	case VAX_BTYP_49:
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		test_fp = rom_testchar;
		rom_putc = 0x20040068;
		rom_getc = 0x20040054;
		break;

	case VAX_BTYP_1303:
		ka53_consinit();
		break;

#ifdef notdef
	case VAX_BTYP_630:
	case VAX_BTYP_650:
	case VAX_BTYP_9CC:
	case VAX_BTYP_60:
		put_fp = pr_putchar;
		get_fp = pr_getchar;
		break
#endif
	}
	return;
}

/*
 * putchar() using MTPR
 */
void
pr_putchar(int c)
{
	int	timeout = 1<<15;	/* don't hang the machine! */

	/*
	 * On KA88 we may get C-S/C-Q from the console.
	 * Must obey it.
	 */
	while (mfpr(PR_RXCS) & GC_DON) {
		if ((mfpr(PR_RXDB) & 0x7f) == 19) {
			while (1) {
				while ((mfpr(PR_RXCS) & GC_DON) == 0)
					;
				if ((mfpr(PR_RXDB) & 0x7f) == 17)
					break;
			}
		}
	}

	while ((mfpr(PR_TXCS) & GC_RDY) == 0)  /* Wait until xmit ready */
		if (--timeout < 0)
			break;
	mtpr(c, PR_TXDB);		/* xmit character */
}

/*
 * getchar() using MFPR
 */
int
pr_getchar(void)
{
	while ((mfpr(PR_RXCS) & GC_DON) == 0)
		;	/* wait for char */
	return (mfpr(PR_RXDB));			/* now get it */
}

int
pr_testchar(void)
{
	if (mfpr(PR_RXCS) & GC_DON)
		return mfpr(PR_RXDB);
	else
		return 0;
}

/*
 * void ka630_rom_getchar (void)  ==> initialize KA630 ROM console I/O
 */
void ka630_consinit(void)
{
	short *NVR;
	int i;

	/* Find the console page */
	NVR = (short *) KA630_NVR_ADRS;
   
	i = *NVR++ & 0xFF;
	i |= (*NVR++ & 0xFF) << 8;
	i |= (*NVR++ & 0xFF) << 16;
	i |= (*NVR++ & 0xFF) << 24;

	ka630_conspage = (char *) i;

	/* Go to last row to minimize confusion */
	ka630_conspage[KA630_ROW] = ka630_conspage[KA630_MAXROW];
	ka630_conspage[KA630_COL] = ka630_conspage[KA630_MINCOL];

	/* Use KA630 ROM console I/O routines */
	put_fp = ka630_rom_putchar;
	get_fp = ka630_rom_getchar;
	test_fp = ka630_rom_testchar;
}


/*
 * void ka53_consinit (void)  ==> initialize KA53 ROM console I/O
 */
void ka53_consinit(void)
{
	ka53_conspage = (char *) 0x2014044b;

	put_fp = ka53_rom_putchar;
	get_fp = ka53_rom_getchar;
	test_fp = ka53_rom_testchar;
}

static volatile int *vxtregs = (int *)0x200A0000;

#define	CH_SR		1
#define	CH_DAT		3
#define SR_TX_RDY	0x04
#define SR_RX_RDY	0x01

void
vxt_putchar(int c)
{
	while ((vxtregs[CH_SR] & SR_TX_RDY) == 0)
		;
	vxtregs[CH_DAT] = c;
}

int
vxt_getchar(void)
{
	while ((vxtregs[CH_SR] & SR_RX_RDY) == 0)
		;
	return vxtregs[CH_DAT];
}

int
vxt_testchar(void)
{
	if ((vxtregs[CH_SR] & SR_RX_RDY) == 0)
		return 0;
	return vxtregs[CH_DAT];
}
