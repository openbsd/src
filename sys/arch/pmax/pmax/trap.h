/*	$NetBSD: trap.h,v 1.2 1997/05/25 05:01:51 jonathan Exp $	*/

/*
 * Copyright (c) 1995, Jonathan Stone
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
 *      This product includes software developed by Jonathan Stone.
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

/*
 * Functions defined in trap.c, used in machdep.c and locore
 */
extern int kn01_intr __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));
extern int kmin_intr __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));
extern int xine_intr __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));

extern int kn02_intr __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));

extern	int (*mips_hardware_intr) __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));
#ifdef DS5000_240
extern int kn03_intr __P((u_int mask, u_int pc, u_int statusReg,
		   u_int causeReg));
#endif /*DS5000_240*/

/* Return the resulting PC as if the branch was executed.  */
extern u_int MachEmulateBranch  __P((u_int* regs, u_int instPC,
				     u_int fpcCSR, int allowNonBranch));

/*
 * Called by locore to handle exceptions other than UTLBMISS
 * (which has a dedicated vector on mips) and external interrupts.
 */
extern u_int trap __P((u_int status, u_int cause, u_int vaddr,  u_int pc,
			 int args));

#ifdef DEBUG
extern int cpu_singlestep __P((register struct proc *p)); 
extern int kdbpeek __P((int addr));
#endif
