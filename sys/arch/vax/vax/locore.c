/*	$NetBSD: locore.c,v 1.10 1995/12/13 18:50:30 ragge Exp $	*/

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
#include "sys/types.h"
#include "sys/reboot.h"

#include "vm/vm.h"

#include "machine/cpu.h"
#include "machine/sid.h"
#include "machine/uvaxII.h"
#include "machine/param.h"
#include "machine/vmparam.h"
#include "machine/pcb.h"

u_int	proc0paddr;
int	cpunumber, *Sysmap, boothowto, cpu_type;
char	*esym;
extern	int bootdev;

/*
 * Start is called from boot; the first routine that is called
 * in kernel. Kernel stack is setup somewhere in a safe place;
 * but we need to move it to a better known place. Memory
 * management is disabled, and no interrupt system is active.
 * We shall be at kernel stack when called; not interrupt stack.
 */
void
start()
{
	extern	u_int *end;
	extern	void *scratch;
	register curtop;

	mtpr(0x1f, PR_IPL); /* No interrupts before istack is ok, please */

	/*
	 * We can be running either in system or user space when
	 * getting here. Need to figure out which and take care
	 * of it.
	 */
	asm("
	movl	r9,_esym
	movl	r10,_bootdev
	movl	r11,_boothowto
	jsb	ett
ett:	cmpl	(sp)+,$0x80000000
	bleq	tvo	# New boot
	pushl	$0x001f0000
	pushl	$to_kmem
	rei
tvo:	movl	(sp)+,_boothowto
	movl	(sp)+,_bootdev
to_kmem:
	");

	/*
	 * FIRST we must set up kernel stack, directly after end.
	 * This is the only thing we have to setup here, rest in pmap.
	 */

	PAGE_SIZE = NBPG*2; /* Set logical page size */
#ifdef DDB
	if ((boothowto & RB_KDB) != 0)
		proc0paddr = ROUND_PAGE(esym) | 0x80000000;
	else
#endif
		proc0paddr = ROUND_PAGE(&end);

	mtpr(proc0paddr, PR_PCBB); /* must be set before ksp for some cpus */
	mtpr(proc0paddr + UPAGES * NBPG, PR_KSP); /* new kernel stack */

	/*
	 * Set logical page size and put Sysmap on its place.
	 */
	Sysmap = (u_int *)ROUND_PAGE(mfpr(PR_KSP));

	/* Be sure some important internal registers have safe values */
        ((struct pcb *)proc0paddr)->P0LR = 0;
        ((struct pcb *)proc0paddr)->P0BR = 0;
        ((struct pcb *)proc0paddr)->P1LR = 0;
        ((struct pcb *)proc0paddr)->P1BR = (void *)0x80000000;
        ((struct pcb *)proc0paddr)->iftrap = NULL;
	mtpr(0, PR_P0LR);
	mtpr(0, PR_P0BR);
	mtpr(0, PR_P1LR);
	mtpr(0x80000000, PR_P1BR);

	mtpr(0, PR_SCBB); /* SCB at physical addr  */
	mtpr(0, PR_ESP); /* Must be zero, used in page fault routine */
	mtpr(AST_NO, PR_ASTLVL);
	
	cninit();

	/* Count up memory etc... early machine dependent routines */
	if((cpunumber = MACHID(mfpr(PR_SID)))>VAX_MAX) cpunumber=0;
	cpu_type = mfpr(PR_SID);
	pmap_bootstrap();

	((struct pcb *)proc0paddr)->framep = scratch;

	/*
	 * change mode down to userspace is done by faking an stack
	 * frame that is setup in cpu_set_kpc(). Not done by returning
	 * from main anymore.
	 */
	main();

	/* NOTREACHED */
}
