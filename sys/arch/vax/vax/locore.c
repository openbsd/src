/*	$NetBSD: locore.c,v 1.17 1996/08/20 14:13:54 ragge Exp $	*/
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
		

#include <sys/param.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/nexus.h>

void	start __P((void));
void	main __P((void));

u_int	proc0paddr;
int	*Sysmap, boothowto;
char	*esym;
extern	int bootdev;

/* 
 * We set up some information about the machine we're
 * running on and thus initializes/uses vax_cputype and vax_boardtype.
 * There should be no need to change/reinitialize these variables
 * outside of this routine, they should be read only!
 */
int vax_cputype;	/* highest byte of SID register */
int vax_bustype;	/* holds/defines all busses on this machine */
int vax_boardtype;	/* machine dependend, combination of SID and SIE */
int vax_systype;	/* machine dependend identification of the system */
 
int vax_cpudata;	/* contents of the SID register */
int vax_siedata;	/* contents of the SIE register */
int vax_confdata;	/* machine dependend, configuration/setup data */

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
	register tmpptr;

	mtpr(0x1f, PR_IPL); /* No interrupts before istack is ok, please */

	/*
	 * We can be running either in system or user space when
	 * getting here. Need to figure out which and take care
	 * of it. We also save all registers if panic gets called.
	 */
	asm("
	bisl2	$0x80000000, r9
	movl	r9, _esym
	movl	r10, _bootdev
	movl	r11, _boothowto
	jsb	ett
ett:	cmpl	(sp)+, $0x80000000
	bleq	tvo	# New boot
	pushl	$0x001f0000
	pushl	$tokmem
	rei
tvo:	movl	(sp)+,_boothowto
	movl	(sp)+,_bootdev
tokmem: movw	$0xfff, _panic
	");

	/*
	 * FIRST we must set up kernel stack, directly after end.
	 * This is the only thing we have to setup here, rest in pmap.
	 */
	PAGE_SIZE = NBPG * 2; /* Set logical page size */
#ifdef DDB
	if ((boothowto & RB_KDB) != 0)
		proc0paddr = ROUND_PAGE(esym) | 0x80000000;
	else
#endif
		proc0paddr = ROUND_PAGE(&end);

	tmpptr = proc0paddr & 0x7fffffff;
	mtpr(tmpptr, PR_PCBB); /* must be set before ksp for some cpus */
	mtpr(proc0paddr + UPAGES * NBPG, PR_KSP); /* new kernel stack */

	/*
	 * Set logical page size and put Sysmap on its place.
	 */
	Sysmap = (u_int *)ROUND_PAGE(mfpr(PR_KSP));

	/* Be sure some important internal registers have safe values */
	((struct pcb *)proc0paddr)->P0LR = 0;
	((struct pcb *)proc0paddr)->P0BR = (void *)0x80000000;
	((struct pcb *)proc0paddr)->P1LR = 0;
	((struct pcb *)proc0paddr)->P1BR = (void *)0x80000000;
	((struct pcb *)proc0paddr)->iftrap = NULL;
	mtpr(0, PR_P0LR);
	mtpr(0x80000000, PR_P0BR);
	mtpr(0, PR_P1LR);
	mtpr(0x80000000, PR_P1BR);

	mtpr(0, PR_SCBB); /* SCB at physical addr  */
	mtpr(0, PR_ESP); /* Must be zero, used in page fault routine */
	mtpr(AST_NO, PR_ASTLVL);

	/* Count up memory etc... early machine dependent routines */
	vax_cputype = ((vax_cpudata = mfpr(PR_SID)) >> 24);

	switch (vax_cputype) {
#if VAX780
	case VAX_TYP_780:
		vax_bustype = VAX_SBIBUS | VAX_CPUBUS;
		vax_boardtype = VAX_BTYP_780;
		break;
#endif
#if VAX750
	case VAX_TYP_750:
		vax_bustype = VAX_CMIBUS | VAX_CPUBUS;
		vax_boardtype = VAX_BTYP_750;
		break;
#endif
#if VAX8600
	case VAX_TYP_790:
		vax_bustype = VAX_CPUBUS | VAX_MEMBUS;
		vax_boardtype = VAX_BTYP_790;
		break;
#endif
#if VAX630 || VAX650 || VAX410 || VAX43
	case VAX_TYP_UV2:
	case VAX_TYP_CVAX:
	case VAX_TYP_RIGEL:
		vax_siedata = *(int *)(0x20040004);	/* SIE address */
		vax_boardtype = (vax_cputype<<24) | ((vax_siedata>>24)&0xFF);

		switch (vax_boardtype) {
		case VAX_BTYP_410:
		case VAX_BTYP_43:
			vax_confdata = *(int *)(0x20020000);
			vax_bustype = VAX_VSBUS | VAX_CPUBUS;
			break;

		case VAX_BTYP_630:
		case VAX_BTYP_650:
			vax_bustype = VAX_UNIBUS | VAX_CPUBUS;
			break;

		default:
			break;
		}
		break;
#endif
#if VAX8200
	case VAX_TYP_8SS:
		vax_boardtype = VAX_BTYP_8000;
		vax_bustype = VAX_BIBUS;
		mastercpu = mfpr(PR_BINID);
		break;
#endif
	default:
		/* CPU not supported, just give up */
		asm("halt");
	}

	/*
	 * before doing anything else, we need to setup the console
	 * so that output (eg. debug and error messages) are visible.
	 * They way console-output is done is different for different
	 * VAXen, thus vax_cputype and vax_boardtype are setup/used.
	 */
	cninit();

	pmap_bootstrap();

	((struct pcb *)proc0paddr)->framep = scratch;

	/*
	 * Change mode down to userspace is done by faking a stack
	 * frame that is setup in cpu_set_kpc(). Not done by returning
	 * from main anymore.
	 */
	main();
	/* NOTREACHED */
}
