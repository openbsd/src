/* $NetBSD: undefined.c,v 1.2 1996/03/08 20:54:25 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * undefined.c
 *
 * Fault handler
 *
 * Created      : 06/01/95
 */

#define CONTINUE_AFTER_RESET_BUG
#define FAST_FPE

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#ifdef FAST_FPE
#include <sys/acct.h>
#endif

#include <machine/cpu.h>
#include <machine/katelib.h>
#include <machine/frame.h>
#include <machine/undefined.h>
#include <machine/irqhandler.h>

#ifdef FAST_FPE
extern int want_resched;
#endif

undef_handler_t undefined_handlers[MAX_COPROCS];

u_int disassemble __P((u_int));


int
default_undefined_handler(address, instruction, frame)
	u_int address;
	u_int instruction;
	trapframe_t *frame;
{
	struct proc *p;

	p = curproc;
	if (p == NULL)
		p = &proc0;
	log(LOG_ERR, "Undefined instruction 0x%08x @ 0x%08x in process %s (pid %d)\n",
	    instruction, address, p->p_comm, p->p_pid);
	return(1);
}


int
install_coproc_handler(coproc, handler)
	int coproc;
	undef_handler_t handler;
{
	if (coproc < 0 || coproc > MAX_COPROCS)
		return(EINVAL);
	if (handler == (undef_handler_t)0)
		handler = default_undefined_handler;
      
	undefined_handlers[coproc] = handler;
	return(0);
}


void
undefined_init()
{
	int loop;

	for (loop = 0; loop < MAX_COPROCS; ++loop)
		undefined_handlers[loop] = default_undefined_handler;
}


void
undefinedinstruction(frame)
	trapframe_t *frame;
{
	struct proc *p;
/*	struct pcb *pcb;*/
	u_int fault_pc;
	int fault_instruction;
	int s;
	int fault_code;
	u_quad_t sticks;
	int coprocessor;

#ifndef BLOCK_IRQS
	if (!(frame->tf_spsr & I32_bit))
		enable_interrupts(I32_bit);
#endif
    
/* Update vmmeter statistics */
    
	cnt.v_trap++;
         
	fault_pc = frame->tf_pc - 4;

/* Should use fuword() here .. but in the interests of squeezing every bit
 * of speed we will just use ReadWord(). We know the instruction can be
 * read as was just executed so this will never fail unless the kernel
 * is screwed up in which case it does not really matter does it ?
 */

	fault_instruction = ReadWord(fault_pc);

/* Check for coprocessor instruction */

/*
 * According to the datasheets you only need to look at bit 27 of the instruction
 * to tell the difference between and undefined instruction and a
 * coprocessor instruction.
 */

	if ((fault_instruction & (1 << 27)) != 0)
		coprocessor = (fault_instruction >> 8) & 0x0f;
	else {
		coprocessor = 0;
		s = splhigh();
		disassemble(fault_pc);
		(void)splx(s);
	}
		
/* Get the current proc structure or proc0 if there is none */

	if ((p = curproc) == 0)
		p = &proc0;

/*	printf("fault in process %08x %d\n", p, p->p_pid);*/

	if ((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE) {
/*		printf("USR32 mode : %08x\n", frame->tf_spsr);*/
		sticks = p->p_sticks;
                  
/* Modify the fault_code to reflect the USR/SVC state at time of fault */

		fault_code = FAULT_USER;
		p->p_md.md_regs = frame;
	} else
		fault_code = 0;

#if 0
/* can't use curpcb, as it might be NULL; and we have p in a register anyway */

    pcb = &p->p_addr->u_pcb;
    if (pcb == 0)
      {
        panic("no pcb ... we're toast !\n");
      }
#endif

/* OK this is were we do something about the instruction */

/* Check for coprocessor instruction */

/*
	s = splhigh();
    	printf("Coprocessor number %d instruction fault\n", coprocessor);
	(void)splx(s);
*/

	if ((undefined_handlers[coprocessor](fault_pc, fault_instruction,
	    frame)) != 0) {
		s = splhigh();

		if ((fault_instruction & 0x0f000010) == 0x0e000000) {
			printf("CDP\n");
			disassemble(fault_pc);
		}
		else if ((fault_instruction & 0x0e000000) == 0x0c000000) {
			printf("LDC/STC\n");
			disassemble(fault_pc);
		}
		else if ((fault_instruction & 0x0f000010) == 0x0e000010) {
			printf("MRC/MCR\n");
			disassemble(fault_pc);
		}
		else {
			printf("Undefined instruction\n");
			disassemble(fault_pc);
		}

		(void)splx(s);
        
		if ((fault_code & FAULT_USER) == 0) {
			printf("Undefined instruction in kernel: Heavy man !\n");
			postmortem(frame);
		}

		trapsignal(p, SIGILL, fault_instruction);
	}

	if ((fault_code & FAULT_USER) == 0)
		return;

#ifdef FAST_FPE
/* Optimised exit code */

	{
		int sig;

/* take pending signals */

		while ((sig = (CURSIG(p))) != 0) {
			postsig(sig);
		}

		p->p_priority = p->p_usrpri;

/*
 * Check for reschedule request, at the moment there is only
 * 1 ast so this code should always be run
 */

		if (want_resched) {
        /*
         * Since we are curproc, a clock interrupt could
         * change our priority without changing run queues
         * (the running process is not kept on a run queue).
         * If this happened after we setrunqueue ourselves but
         * before we switch()'ed, we might not be on the queue
         * indicated by our priority
         */
	
		        s = splstatclock();
			setrunqueue(p);
			p->p_stats->p_ru.ru_nivcsw++;

			mi_switch();

			(void)splx(s);
			while ((sig = (CURSIG(p))) != 0) {
				postsig(sig);
			}
		}

/*
 * The profiling bit is commented out at the moment. This can be reinstated
 * later on. Currently addupc_task is not written.
 */

		if (p->p_flag & P_PROFIL) {
			extern int psratio;
			addupc_task(p, frame->tf_pc, (int)(p->p_sticks - sticks) * psratio );
		}

		curpriority = p->p_priority;
	}

#else
#ifdef VALIDATE_TRAPFRAME
	validate_trapframe(frame, 5);
#endif

	userret(p, frame->tf_pc, sticks);

#ifdef VALIDATE_TRAPFRAME
	validate_trapframe(frame, 5);
#endif
#endif
}


void
resethandler(frame)
	trapframe_t *frame;
{
	postmortem(frame);

#ifdef CONTINUE_AFTER_RESET_BUG
	printf("Branch throuh zero\n");
	printf("The system should now be considered very unstable :-)\n");
	sigexit(curproc, SIGILL);

#ifdef VALIDATE_TRAPFRAME
	validate_trapframe(frame, 4);
#endif
#else
	panic("Branch through zero..... were dead\n");
#endif
}

/* End of undefined.c */
