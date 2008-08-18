/*	$OpenBSD: consio.c,v 1.10 2008/08/18 23:20:44 miod Exp $ */
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
		
#include <sys/param.h>

#include "../vax/gencons.h"

#include "mtpr.h"
#include "sid.h"
#include "rpb.h"
#include "ka630.h"

#include "data.h"

#include <machine/cvax.h>
#include <arch/vax/mbus/mbusreg.h>

void setup(void);

static void (*put_fp)(int)  = NULL;
static int (*get_fp)(void) = NULL;
static int (*test_fp)(void) = NULL;

/*
 * I/O using mtpr/mfpr
 */

void	pr_putchar(int c);
int	pr_getchar(void);
int	pr_testchar(void);

/*
 * I/O using ROM routines
 */

void	rom_putchar(int c);
int	rom_getchar(void);
int	rom_testchar(void);

int	rom_putc;		/* ROM-address of put-routine */
int	rom_getc;		/* ROM-address of get-routine */

/*
 * I/O using the KA630 ROM console routines
 */

/* Pointer to KA630 console page, initialized by ka630_consinit */
unsigned char	*ka630_conspage;
void	ka630_consinit(void);

void	ka630_rom_putchar(int c);
int	ka630_rom_getchar(void);
int	ka630_rom_testchar(void);

/*
 * I/O using the KA53 ROM console routines
 */

unsigned char  *ka53_conspage;
void	ka53_consinit(void);

void	ka53_rom_putchar(int c);
int	ka53_rom_getchar(void);
int	ka53_rom_testchar(void);

/*
 * I/O using the VXT2000 serial ports
 */

void	vxt_putchar(int c);
int	vxt_getchar(void);
int	vxt_testchar(void);

/*
 * I/O using the KA60 ROM console routines
 */

unsigned char  *ka60_conspage;
void	ka60_consinit(void);

void	ka60_rom_putchar(int c);
int	ka60_rom_getchar(void);
int	ka60_rom_testchar(void);

void	putchar(int);
int	getchar(void);
int	testkey(void);
void	consinit(void);
void	_rtt(void);

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
		c += 32;			/* force lowercase */
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
		put_fp = rom_putchar;
		get_fp = vxt_getchar;
		test_fp = vxt_testchar;
		rom_putc = 0x20040058;		/* 537133144 */
		rom_getc = 0x20040044;		/* 537133124 */
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

	case VAX_BTYP_60:
		ka60_consinit();
		break;

#ifdef notdef
	case VAX_BTYP_630:
	case VAX_BTYP_650:
	case VAX_BTYP_9CC:
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
 * void ka630_consinit (void)  ==> initialize KA630 ROM console I/O
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

/*
 * VXT2000 console routines.
 *
 * While we can use the rom putchar routine, the rom getchar routine
 * will happily return the last key pressed, even if it is not pressed
 * anymore.
 *
 * To guard against this, we monitor the keyboard serial port and will
 * only invoke the rom function (which will do the keyboard layout
 * translation for us) if there is indeed a new keyboard event (we still
 * need to guard against dead keys, hence the while() condition in
 * vxt_getchar). This still unfortunately causes phantom characters to
 * appear when playing with the shift keys, but nothing backspace can't
 * erase, so this will be a minor annoyance.
 *
 * If console is on the serial port, we do not use the prom routines at
 * all.
 */
static volatile int *vxtregs = (int *)0x200A0000;

#define	CH_SRA		0x01
#define	CH_CRA		0x02
#define	CH_DATA		0x03
#define	CH_SRC		0x11
#define	CH_CRC		0x12
#define	CH_DATC		0x13

#define	CR_RX_ENA	0x01
#define	CR_TX_ENA	0x04
#define SR_RX_RDY	0x01
#define SR_TX_RDY	0x04

int
vxt_getchar(void)
{
	if (vax_confdata & 2) {
		vxtregs[CH_CRC] = CR_RX_ENA;
		while ((vxtregs[CH_SRC] & SR_RX_RDY) == 0 ||
		    rom_testchar() == 0)
			;
		return rom_getchar();
	} else {
		vxtregs[CH_CRA] = CR_RX_ENA;
		while ((vxtregs[CH_SRA] & SR_RX_RDY) == 0)
			;
		return vxtregs[CH_DATA];
	}
}

int
vxt_testchar(void)
{
	if (vax_confdata & 2) {
		vxtregs[CH_CRC] = CR_RX_ENA;
		if ((vxtregs[CH_SRC] & SR_RX_RDY) == 0)
			return 0;
		return rom_testchar();
	} else {
		vxtregs[CH_CRA] = CR_RX_ENA;
		if ((vxtregs[CH_SRA] & SR_RX_RDY) == 0)
			return 0;
		return vxtregs[CH_DATA];
	}
}

int ka60_ioslot = -1;

/*
 * void ka60_consinit (void)  ==> initialize KA60 ROM console I/O
 */
void ka60_consinit(void)
{
	extern int jbuf[10];
	extern int mcheck_silent;
	extern int setjmp(int *);

	int mid, fbicaddr;
	uint32_t modtype, fbicrange;

	for (mid = 0; mid < MBUS_SLOT_MAX; mid++) {
		fbicaddr = MBUS_SLOT_BASE(mid) + FBIC_BASE;

		if (setjmp(jbuf)) {
			/* this slot is empty */
			continue;
		}
		mcheck_silent = 1;
		modtype = *(uint32_t *)(fbicaddr + FBIC_MODTYPE);
		mcheck_silent = 0;

		if ((modtype & MODTYPE_CLASS_MASK) >> MODTYPE_CLASS_SHIFT !=
		    CLASS_IO)
			continue;

		ka60_ioslot = mid;

		fbicrange = *(uint32_t *)(fbicaddr + FBIC_RANGE);
		if (fbicrange ==
		    ((HOST_TO_MBUS(CVAX_SSC) & RANGE_MATCH) | RANGE_ENABLE))
			break;
	}
	mcheck_silent = 0;

	if (ka60_ioslot < 0) {
		/*
		 * This shouldn't happen. Try mid #5 (enclosure slot #4) as a
		 * supposedly sane default.
		 */
		ka60_ioslot = 5;
	}

	ka60_conspage = (char *) *(unsigned int *)0x20140514;
	put_fp = ka60_rom_putchar;
	get_fp = ka60_rom_getchar;
	test_fp = ka60_rom_testchar;
}
