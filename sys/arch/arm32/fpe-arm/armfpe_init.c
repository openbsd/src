/* $NetBSD: armfpe_init.c,v 1.6 1996/03/18 19:55:01 mark Exp $ */

/*
 * Copyright (C) 1996 Mark Brinicombe
 * Copyright (C) 1995 Neil A Carson.
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
 * This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * arm_fpe.c
 *
 * Stuff needed to interface the ARM floating point emulator module to RiscBSD.
 *
 * Created      : 22/10/95
 */

/*#define DEBUG*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/acct.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/cpus.h>
#include <machine/katelib.h>
#include <machine/frame.h>

#include "armfpe.h"		/* Prototypes for things */

extern int want_resched;
extern u_int fpe_nexthandler;

void undefinedinstruction_bounce __P(());
void arm_fpe_exception_glue __P((int exception));
void arm_fpe_panic __P(());
void undefined_entry __P(());
void arm_fpe_post_proc_glue __P(());

/*
 * A module header, pointing into the module
 */

extern u_int arm_fpe_mod[]; 
extern u_int undefined_handler_address;

/*
 * Error messages for the various exceptions, numbered 0-5
 */
 
static char *exception_errors[] = {
	"Floating point invalid operation",
	"Floating point division by zero (0)",
	"Floating point overflow",
	"Floating point underflow",
	"Floating point operation inexact",
	"Floating point major faliure... core fault trapped... not good!"
};

/*
 * Relocate the FPE.
 */

void
arm_fpe_mod_reloc(void)
{
	int cnt;
	arm_fpe_mod_hdr_t *arm_fpe_mod_hdr = (arm_fpe_mod_hdr_t *)arm_fpe_mod;

	/* Go through the module header, and convert all offsets into absolute
	 * addresses. Careful here - the last two fields of the header do _NOT_
	 * want to be relocated!
	 */

	for (cnt = 0; cnt < (sizeof(arm_fpe_mod_hdr_t) >> 2) - 2; cnt ++) {
#ifdef DEBUG
		printf("FPE: entry %02x = %08x ", cnt, arm_fpe_mod[cnt]);
#endif
		arm_fpe_mod[cnt] += (u_int) arm_fpe_mod;
#ifdef DEBUG
		printf(" reloc=%08x\n", arm_fpe_mod[cnt]);
#endif
	}
	/* Print a startup message, and a couple of variables needed, these may need
	 * checking to make *sure* they are OK!
	 */

#ifdef DEBUG
	printf("FPE: global workspace size = %d bytes, context size = %d bytes\n",
	    arm_fpe_mod_hdr->WorkspaceLength, arm_fpe_mod_hdr->ContextLength);
	printf("FPE: base=%08x\n", (u_int)arm_fpe_mod);
#endif
}

/*
 * Initialisation point. The kernel calls this during the configuration of the cpu
 * in order to install the FPE.
 * The FPE specification needs to be filled in the specified cpu_t structure
 * and the FPE needs to be installed on the CPU undefined instruction vector.
 */

int
initialise_arm_fpe(cpu)
	cpu_t *cpu;
{
	int error;

	cpu->fpu_class = FPU_CLASS_FPE;
	cpu->fpu_type = FPU_TYPE_ARMLTD_FPE;
	strcpy(cpu->fpu_model, "Advanced RISC Machines floating point emulator");
	error = arm_fpe_boot();
	if (error != 0) {
		strcat(cpu->fpu_model, " - boot failed");
		return(1);
	}

/* Return with start failure so the old FPE is installed */

/*	strcat(cpu->fpu_model, " - boot aborted");*/

	return(0);
}

/*
 * The actual FPE boot routine.
 * This has to do a number of things :
 * 1. Relocate the FPE - Note this requires write access to the kernel text area
 * 2. Allocated memory for the FPE
 * 3. Initialise the FPE
 */

int
arm_fpe_boot(void)
{
	u_int workspace;
	int id;
	arm_fpe_mod_hdr_t *arm_fpe_mod_hdr = (arm_fpe_mod_hdr_t *)arm_fpe_mod;
	
	/* First things first ... Relocate the FPE pointers */

	arm_fpe_mod_reloc();

	/* Now we must do some memory allocation */

	workspace = (u_int)malloc(arm_fpe_mod_hdr->WorkspaceLength, M_DEVBUF, M_NOWAIT);
#ifdef DEBUG
	printf("Gloabl workspace at 0x%08x\n", workspace);
#endif

	if (!workspace)
		return(ENOMEM);

	*arm_fpe_mod_hdr->main_ws_ptr_addr = workspace;

	*arm_fpe_mod_hdr->local_handler_ptr_addr = (u_int)&undefined_handler_address;
	*arm_fpe_mod_hdr->old_handler_ptr_addr = undefined_handler_address;

	/* Initialise out gloable workspace */

#ifdef DEBUG
	printf("Initing workspace ");
#endif

	id = arm_fpe_core_initws(workspace, (u_int)&fpe_nexthandler, (u_int)&fpe_nexthandler);

#ifdef DEBUG
	printf("id=%08x\n", id);
#endif

	/* Set up an exception handler */

	*arm_fpe_mod_hdr->exc_handler_ptr_addr = (u_int)arm_fpe_exception_glue;

	/* Set up post instruction handler */
#if defined(CPU_ARM6) || defined(CPU_ARM7)
	*arm_fpe_mod_hdr->fp_post_proc_addr = (((((u_int)arm_fpe_post_proc_glue -
	    (u_int)arm_fpe_mod_hdr->fp_post_proc_addr - 8)>>2) & 0x00ffffff) | 0xea000000);
#ifdef DEBUG
	printf("arm_fpe_mod_hdr->fp_post_proc_addr = %08x (%08x)",
	    arm_fpe_mod_hdr->fp_post_proc_addr,
	    *arm_fpe_mod_hdr->fp_post_proc_addr);
#endif
#else
#error ARMFPE currently only supports ARM6 and ARM7
#endif

#ifdef DEBUG
	printf("Initialising proc0 FPE context\n");
#endif

	/* Initialise proc0's FPE context */

	arm_fpe_core_initcontext(FP_CONTEXT(&proc0));
	arm_fpe_core_changecontext(FP_CONTEXT(&proc0));

	return(0);
}


/*
 * Callback routine from the FPE when instruction emulation completes
 */

void
arm_fpe_postproc(fpframe, frame)
	u_int fpframe;
	struct trapframe *frame;
{
	register u_int s;
	register int sig;
	register struct proc *p;

	p = curproc;
	p->p_md.md_regs = frame;

/* take pending signals */

	while ((sig = (CURSIG(p))) != 0) {
		postsig(sig);
	}

	p->p_priority = p->p_usrpri;

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

/* Profiling. */

	if (p->p_flag & P_PROFIL) {
		extern int psratio;
		u_int pc;

		pc = ReadWord(fpframe + 15*4);

		if (pc <0x1000 || pc > 0xefc00000)
			printf("armfpe_postproc: pc=%08x\n", pc);

/*		addupc_task(p, pc, (int)(p->p_sticks - sticks) * psratio);*/
		addupc_task(p, pc, (int)(p->p_sticks - p->p_sticks) * psratio);
	}

	curpriority = p->p_priority;
}


/*
 * Callback routine from the FPE when an exception occurs.
 */

void
arm_fpe_exception(exception, pc, fpframe)
	int exception;
	u_int pc;
	u_int fpframe;
{
	if (exception >= 0 && exception < 6)
		printf("fpe exception: %d - %s\n", exception, exception_errors[exception]);
	else
		printf("fpe exception: %d - unknown\n", exception);

	trapsignal(curproc, SIGFPE, exception);

	printf("PC=%08x\n", ReadWord(fpframe + 60));

	userret(curproc, pc, curproc->p_sticks);
}


void
arm_fpe_copycontext(c1, c2)
	u_int c1;
	u_int c2;
{
	fp_context_frame_t fpcontext;

	arm_fpe_core_savecontext(c1, (int *)&fpcontext, 0);
	arm_fpe_core_loadcontext(c2, (int *)&fpcontext);
}

/* End of armfpe_init.c */
