/*	$NetBSD: consio.c,v 1.4 1996/08/02 11:22:00 ragge Exp $ */
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

#include "../vax/gencons.h"

#include "../include/mtpr.h"
#include "../include/sid.h"
#include "../include/rpb.h"

#include "data.h"

void setup __P((void));

int	vax_cputype;
int	vax_boardtype;

int	is_750;
int	is_mvax;

unsigned       *bootregs;
struct rpb     *rpb;
struct bqo     *bqo;

static int (*put_fp) __P((int))  = NULL;
static int (*get_fp) __P((void)) = NULL;

int pr_putchar __P((int c));	/* putchar() using mtpr/mfpr */
int pr_getchar __P((void));

int rom_putchar __P((int c));	/* putchar() using ROM routines */
int rom_getchar __P((void));

static int rom_putc;		/* ROM-address of put-routine */
static int rom_getc;		/* ROM-address of get-routine */

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
	return c;
}


/*
 * setup() is called out of the startup files (start.s, srt0.s) and
 * initializes data which are globally used and is called before main().
 */
void 
setup()
{
	vax_cputype = (mfpr(PR_SID) >> 24) & 0xFF;

	put_fp = pr_putchar;
	get_fp = pr_getchar;
	/*
	 * according to vax_cputype we initialize vax_boardtype.
	 */
        switch (vax_cputype) {

	case VAX_650:
	case VAX_78032:
		is_mvax = 1;
		vax_boardtype = (vax_cputype << 24) |
		    ((*(int*)0x20040004 >> 24) & 0377);
		rpb = (struct rpb *)bootregs[11];	/* bertram: ??? */
		break;
        }

	/*
	 * According to the vax_boardtype (vax_cputype is not specific
	 * enough to do that) we decide which method/routines to use
	 * for console I/O. 
	 * mtpr/mfpr are restricted to serial consoles, ROM-based routines
	 * support both serial and graphical consoles, thus we use that
	 * as fallthrough/default.
	 */
	switch (vax_boardtype) {	/* ROM-based is default !!! */

	case VAX_BTYP_650:
	case VAX_BTYP_660:
	case VAX_BTYP_670:
	case VAX_BTYP_690:
	case VAX_BTYP_1303:
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		rom_putc = 0x20040058;		/* 537133144 */
		rom_getc = 0x20040008;		/* 537133064 */
		break;

	case VAX_BTYP_43:
	case VAX_BTYP_46:
	case VAX_BTYP_49:
	case VAX_BTYP_410:	  
		put_fp = rom_putchar;
		get_fp = rom_getchar;
		rom_putc = 0x20040058;		/* 537133144 */
		rom_getc = 0x20040044;		/* 537133124 */
		break;

	default:
		break;
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
");


