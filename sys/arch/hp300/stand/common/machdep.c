/*	$OpenBSD: machdep.c,v 1.3 1998/05/10 11:31:19 downsj Exp $	*/
/*	$NetBSD: machdep.c,v 1.4 1997/06/28 07:20:25 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: machdep.c 1.10 92/06/18
 *
 *	@(#)machdep.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>

#include "samachdep.h"

char *
getmachineid()
{
	extern int machineid, mmuid;
	char *cp;

	switch (machineid) {
	case HP_320:
		cp = "320"; break;
	case HP_330:
		cp = "318/319/330"; break;
	case HP_340:
		cp = "340"; break;
	case HP_345:
		cp = "345"; break;
	case HP_350:
		cp = "350"; break;
	case HP_360:
		cp = "360"; break;
	case HP_370:
		cp = "370"; break;
	case HP_375:
		cp = "375"; break;
	case HP_380:
		cp = "380"; break;
	case HP_385:
		cp = "385"; break;
	case HP_400:
		cp = "400"; break;
	case HP_425:
		switch (mmuid) {
		case MMUID_425_T:
			cp = "425t"; break;
		case MMUID_425_S:
			cp = "425s"; break;
		case MMUID_425_E:
			cp = "425e"; break;
		default:
			cp = "425"; break;
		}
		break;
	case HP_433:
		switch (mmuid) {
		case MMUID_433_T:
			cp = "433t"; break;
		case MMUID_433_S:
			cp = "433s"; break;
		default:
			cp = "433"; break;
		}
		break;
	default:
		cp = "???"; break;
	}
	return(cp);
}

int userom;

struct trapframe {
	int dregs[8];
	int aregs[8];
	int whoknows;
	short sr;
	int pc;
	short frame;
};

trap(fp)
	struct trapframe *fp;
{
	static int intrap = 0;

	if (intrap)
		return(0);
	intrap = 1;

#if 0
	userom = 1;
#endif

	printf("Got unexpected trap: format=%x vector=%x ps=%x pc=%x\n",
		  (fp->frame>>12)&0xF, fp->frame&0xFFF, fp->sr, fp->pc);
	printf("dregs: %x %x %x %x %x %x %x %x\n",
	       fp->dregs[0], fp->dregs[1], fp->dregs[2], fp->dregs[3], 
	       fp->dregs[4], fp->dregs[5], fp->dregs[6], fp->dregs[7]);
	printf("aregs: %x %x %x %x %x %x %x %x\n",
	       fp->aregs[0], fp->aregs[1], fp->aregs[2], fp->aregs[3], 
	       fp->aregs[4], fp->aregs[5], fp->aregs[6], fp->aregs[7]);

#if 0
	userom = 0;
#endif

	intrap = 0;
	return(0);
}

#define ROWS	24
#define COLS	80

void
romputchar(c)
	int c;
{
	static char buf[COLS];
	static int col = 0, row = 0;
	int i;

	switch (c) {
	case '\0':
		break;
	case '\r':
		break;	/* ignore */
	case '\n':
		for (i = col; i < COLS-1; i++)
			buf[i] = ' ';
		buf[i] = '\0';
		romout(row, buf);
		col = 0;
		if (++row == ROWS)
			row = 0;
		break;

	case '\t':
		do {
			romputchar(' ');
		} while (col & 7);
		break;

	default:
		buf[col] = c;
		if (++col == COLS-1)
			romputchar('\n');
		break;
	}
}

void
machdep_start(entry, howto, loadaddr, ssym, esym)
	char *entry;
	int howto; 
	char *loadaddr;
	char *ssym, *esym; 
{
	/* Fix what we were passed in from exec() */
	entry = loadaddr;
#define	round_to_size(x) (((int)(x) + sizeof(int) - 1) & ~(sizeof(int) - 1))
	esym = (char *)round_to_size(esym - (char *)loadaddr);
#undef round_to_size

	__asm __volatile ("movl %0,d7" : : "m" (howto));
	__asm __volatile ("movl %0,d6" : : "m" (opendev));
	__asm __volatile ("movl %0,d5" : : "m" (cons_scode));
	__asm __volatile ("movl %0,a5" : : "a" (loadaddr));
	__asm __volatile ("movl %0,a4" : : "a" (esym));
	(*((int (*)())entry))();
}
