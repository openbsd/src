/*	$OpenBSD: consio.c,v 1.3 2002/03/14 01:26:47 millert Exp $ */
/*	$NetBSD: consio.c,v 1.11 2000/07/19 00:58:24 matt Exp $ */
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

#include "data.h"

void setup(void);

unsigned       *bootregs;
extern struct rpb     *rpb;
struct bqo     *bqo;

static int (*put_fp)(int)  = NULL;
static int (*get_fp)(void) = NULL;
static int (*test_fp)(void) = NULL;

/* Also added such a thing for KA53 - MK-991208 */
unsigned char  *ka53_conspage;
void ka53_consinit(void);

int ka53_rom_putchar(int c);
int ka53_rom_getchar(void);
int ka53_rom_testchar(void);

int pr_putchar(int c);	/* putchar() using mtpr/mfpr */
int pr_getchar(void);
int pr_testchar(void);

int rom_putchar(int c);	/* putchar() using ROM routines */
int rom_getchar(void);
int rom_testchar(void);

static int rom_putc;		/* ROM-address of put-routine */
static int rom_getc;		/* ROM-address of get-routine */

/* Location of address of KA630 console page */
#define NVR_ADRS        0x200B8024
/* Definitions for various locations in the KA630 console page */
#define KA630_PUTC_POLL 0x20
#define KA630_PUTC      0x24
#define KA630_GETC      0x1C
#define KA630_ROW	0x4C
#define KA630_MINROW	0x4D
#define KA630_MAXROW	0x4E
#define KA630_COL	0x50
#define KA630_MINCOL    0x51
#define KA630_MAXCOL	0x52
/* Pointer to KA630 console page, initialized by ka630_consinit */
unsigned char  *ka630_conspage; 
/* Function that initializes things for KA630 ROM console I/O */
void ka630_consinit(void);
/* Functions that use KA630 ROM for console I/O */
int ka630_rom_putchar(int c);
int ka630_rom_getchar(void);
int ka630_rom_testchar(void);

putchar(c)
	int c;
{
	(*put_fp)(c);
	if (c == 10)
		(*put_fp)(13);		/* CR/LF */
}

getchar() 
{
	int c;

	do
		c = (*get_fp)() & 0177;
	while (c == 17 || c == 19);		/* ignore XON/XOFF */
	if (c < 96 && c > 64)
		c += 32;
	return c;
}

testkey()
{
	return (*test_fp)();
}

/*
 * setup() is called out of the startup files (start.s, srt0.s) and
 * initializes data which are globally used and is called before main().
 */
void 
consinit()
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

	case VAX_BTYP_690:
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		test_fp = rom_testchar;
		rom_putc = 0x20040058;		/* 537133144 */
		rom_getc = 0x20040008;		/* 537133064 */
		break;

	case VAX_BTYP_43:
	case VAX_BTYP_410:	  
	case VAX_BTYP_420:
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		test_fp = rom_testchar;
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
pr_putchar(c)
        int     c;
{
	int     timeout = 1<<15;	/* don't hang the machine! */
        while ((mfpr(PR_TXCS) & GC_RDY) == 0)  /* Wait until xmit ready */
		if (--timeout < 0)
			break;
        mtpr(c, PR_TXDB);		/* xmit character */
}

/*
 * getchar() using MFPR
 */
pr_getchar()
{
	while ((mfpr(PR_RXCS) & GC_DON) == 0);	/* wait for char */
	return (mfpr(PR_RXDB));			/* now get it */
}

pr_testchar()
{
	if (mfpr(PR_RXCS) & GC_DON)
		return mfpr(PR_RXDB);
	else
		return 0;
}
/*
 * int rom_putchar (int c)	==> putchar() using ROM-routines
 */
asm("
	.globl _rom_putchar
	_rom_putchar:
		.word 0x04		# save-mask: R2
		movl	4(ap), r2	# move argument to R2
		jsb	*_rom_putc	# write it
		ret			# that's all
");


/*
 * int rom_getchar (void)	==> getchar() using ROM-routines
 */
asm("
	.globl _rom_getchar
	_rom_getchar:
		.word 0x02		# save-mask: R1
	loop:				# do {
		jsb	*_rom_getc	#   call the getc-routine
		tstl	r0		#   check if char ready
		beql	loop		# } while (R0 == 0)
		movl	r1, r0		# R1 holds char
		ret			# we're done

	_rom_testchar:
		.word	0
		mnegl	$1,r0
		jsb	*_rom_getc
		tstl	r0
		beql	1f
		movl	r1,r0
	1:	ret
");

_rtt()
{
	asm("halt");
}



/*
 * void ka630_rom_getchar (void)  ==> initialize KA630 ROM console I/O
 */
void ka630_consinit()
{
        register short *NVR;
        register int i;

        /* Find the console page */
        NVR = (short *) NVR_ADRS;
   
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
 * int ka630_rom_getchar (void)	==> getchar() using ROM-routines on KA630
 */
asm("
	.globl _ka630_rom_getchar
	_ka630_rom_getchar:
		.word 0x802		# save-mask: R1, R11
		movl    _ka630_conspage,r11  # load location of console page
        loop630g:		       	# do {
		jsb	*0x1C(r11)	#   call the getc-routine (KA630_GETC)
	        blbc    r0, loop630g    # } while (R0 == 0)
		movl	r1, r0		# R1 holds char
		ret			# we're done

	_ka630_rom_testchar:
		.word	0
		movl	_ka630_conspage,r3
		jsb	*0x1C(r3)
		blbc	r0,1f
		movl	r1,r0
	1:	ret
");

/*
 * int ka630_rom_putchar (int c) ==> putchar() using ROM-routines on KA630
 */
asm("
	.globl _ka630_rom_putchar
	_ka630_rom_putchar:
		.word 0x802		# save-mask: R1, R11
		movl    _ka630_conspage,r11  # load location of console page
        loop630p:		       	# do {
		jsb	*0x20(r11)	#   is rom ready? (KA630_PUTC_POLL)
	        blbc    r0, loop630p    # } while (R0 == 0)
		movl	4(ap), r1	# R1 holds char
		jsb     *0x24(r11)      # output character (KA630_PUTC)
		ret			# we're done
");
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
 * int ka53_rom_getchar (void)	==> getchar() using ROM-routines
 */

asm("
	.globl _ka53_rom_getchar
	_ka53_rom_getchar:
		.word 0x0802	# r1, r11
		movl _ka53_conspage, r11	 # load location of console page
1:		jsb *0x64(r11) 	# test for char
		blbc r0, 1b		# while r0 is 0
		jsb *0x6c(r11)	# get char
		ret
");

asm("
	.globl _ka53_rom_testchar
	_ka53_rom_testchar:
		.word	0x8		# r3
		movl	_ka53_conspage, r3
		jsb *0x64(r3)
		blbc r0, 1f
		jsb *0x6c(r3)	# get it
1:		ret
");
		
/*
 * int ka53_rom_putchar (int c) ==> putchar() using ROM-routines 
 */

asm("
	.globl _ka53_rom_putchar
	_ka53_rom_putchar:
		.word 0x0802	# r1, r11
		movl _ka53_conspage, r11
1:		jsb *0x20(r11)	# ready to write?
		blbc r0, 1b		# keep going if r0 == 0
		movl 4(ap), r1	# char is in r1
		jsb *0x24(r11)	# output char
		ret
");


