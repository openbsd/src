/*	$OpenBSD: pcb.h,v 1.7 2011/03/23 16:54:37 pirofti Exp $	*/
/*	$NetBSD: pcb.h,v 1.10 1996/02/02 18:08:26 mycroft Exp $	*/

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

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/reg.h>
#include <machine/pte.h>

struct pcb {

  	/* Hardware registers, based on VAX special instructions */

	long	KSP;		/*  Kernel Stack Pointer      */
	long	ESP;		/*  Executive Stack Pointer   */
	long	SSP;		/*  Supervisor Stack Pointer  */
	long	USP;		/*  User Stack Pointer        */
	long	R[12];		/*  Register 0-11             */
	long	AP;		/*  Argument Pointer          */
	long	FP;		/*  Frame Pointer             */
	long	PC;		/*  Program Counter           */
	long	PSL;		/*  Program Status Longword   */
	pt_entry_t *P0BR;	/*  Page 0 Base Register      */
	long	P0LR;		/*  Page 0 Length Register    */
	pt_entry_t *P1BR;	/*  Page 1 Base Register      */
	long	P1LR;		/*  Page 1 Length Register    */

	/* Software registers, only used by kernel software */
	void   *framep;		/* Pointer to syscall frame */
	void   *iftrap;		/* Tells whether fault copy */
};

#define	AST_MASK 0x07000000
#define	AST_PCB	 0x04000000

/* machine-specific core dump; save regs */
struct	md_coredump {
	struct reg md_reg;
};

#endif /* _MACHINE_PCB_H_ */

