/* $NetBSD: postmortem.c,v 1.4 1996/03/13 21:26:52 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * postmortem.c
 *
 * Postmortem routines
 *
 * Created      : 17/09/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <machine/frame.h>
#include <machine/katelib.h>

#ifdef POSTMORTEM

typedef struct {
	vm_offset_t physical;
	vm_offset_t virtual;
} pv_addr_t;

extern pv_addr_t irqstack;
extern pv_addr_t undstack;
extern pv_addr_t abtstack;
extern struct proc *proc1;

int usertraceback = 0;

/* dumpb - dumps memory in bytes */

void
pm_dumpb(addr, count)
	u_char *addr;
	int count;
{
	u_int byte;
	int loop;

	for (; count > 0; count -= 16) {
		printf("%08x: ", (int)addr);

		for (loop = 0; loop < 16; ++loop) {
			byte = addr[loop];
			printf("%02x ", byte);
		}

		printf(" ");

		for (loop = 0; loop < 16; ++loop) {
			byte = addr[loop];
			if (byte < 0x20)
				printf("\x1b[31m%c\x1b[0m", byte + '@');
			else if (byte == 0x7f)
				printf("\x1b[31m?\x1b[0m");
			else if (byte < 0x80)
				printf("%c", byte);
			else if (byte < 0xa0)
				printf("\x1b[32m%c\x1b[0m", byte - '@');
			else if (byte == 0xff)
				printf("\x1b[32m?\x1b[0m");
			else
				printf("%c", byte & 0x7f);
		}

		printf("\r\n");
		addr += 16;
	}
}


/* dumpw - dumps memory in bytes */

void
pm_dumpw(addr, count)
	u_char *addr;
	int count;
{
	u_int byte;
	int loop;

	for (; count > 0; count -= 32) {
	        printf("%08x: ", (int)addr);

		for (loop = 0; loop < 8; ++loop) {
			byte = ((u_int *)addr)[loop];
			printf("%08x ", byte);
		}

		printf(" ");

		for (loop = 0; loop < 32; ++loop) {
			byte = addr[loop];
			if (byte < 0x20)
				printf("\x1b[31m%c\x1b[0m", byte + '@');
			else if (byte == 0x7f)
				printf("\x1b[31m?\x1b[0m");
			else if (byte < 0x80)
				printf("%c", byte);
			else if (byte < 0xa0)
				printf("\x1b[32m%c\x1b[0m", byte - '@');
			else if (byte == 0xff)
				printf("\x1b[32m?\x1b[0m");
			else
				printf("%c", byte & 0x7f);
		}

		printf("\r\n");
		addr += 32;
	}
}


/* Dump a trap frame */

void
dumpframe(frame)
	trapframe_t *frame;
{
	u_int s;
    
	s = splhigh();
	printf("frame address = %08x  ", (u_int)frame);
	printf("spsr =%08x\n", frame->tf_spsr);
	printf("r0 =%08x r1 =%08x r2 =%08x r3 =%08x\n", frame->tf_r0, frame->tf_r1, frame->tf_r2, frame->tf_r3);
	printf("r4 =%08x r5 =%08x r6 =%08x r7 =%08x\n", frame->tf_r4, frame->tf_r5, frame->tf_r6, frame->tf_r7);
	printf("r8 =%08x r9 =%08x r10=%08x r11=%08x\n", frame->tf_r8, frame->tf_r9, frame->tf_r10, frame->tf_r11);
	printf("r12=%08x r13=%08x r14=%08x r15=%08x\n", frame->tf_r12, frame->tf_usr_sp, frame->tf_usr_lr, frame->tf_pc);
	printf("slr=%08x\n", frame->tf_svc_lr);

	(void)splx(s);
}


/* Dump kstack information */
/*
void kstack_stuff(p)
	struct proc *p;
{
	struct pmap *pmap;

	if (p == 0) return;
	if (p->p_addr == 0) return;
	if (p->p_vmspace == 0) return;
	
	printf("proc=%08x comm=%s pid=%d\n", (u_int)p, p->p_comm, p->p_pid);
	
	pmap = &p->p_vmspace->vm_pmap;

	printf("pmap=%08x\n", (u_int)pmap);

	printf("p->p_addr=%08x pa=%08x,%d:%08x,%d\n",
	    (u_int)p->p_addr,
	    (u_int)pmap_extract(pmap, (vm_offset_t)p->p_addr),
	    pmap_page_index(pmap_extract(pmap, (vm_offset_t)p->p_addr)),
	    (u_int)pmap_extract(pmap, (vm_offset_t)p->p_addr + NBPG),
	    pmap_page_index(pmap_extract(pmap, (vm_offset_t)p->p_addr + NBPG)));
}
*/

void
check_stacks(p)
	struct proc *p;
{
	u_char *ptr;
	int loop;

	if (p) {
		ptr = ((u_char *)p->p_addr) + USPACE_UNDEF_STACK_BOTTOM;
		for (loop = 0; loop < (USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM) && *ptr == 0xdd; ++loop, ++ptr) ;
		printf("%d bytes of undefined stack fill pattern out of %d bytes\n", loop,
		    USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM);
		ptr = ((u_char *)p->p_addr) + USPACE_SVC_STACK_BOTTOM;
		for (loop = 0; loop < (USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM) && *ptr == 0xdd; ++loop, ++ptr) ;
		printf("%d bytes of svc stack fill pattern out of %d bytes\n", loop,
		    USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM);
	}
}


/* Perform a postmortem */

void
postmortem(frame)
 	trapframe_t *frame;
{
	u_int s;
	struct proc *p = curproc;
	int loop;
	u_int addr;
			
	s = splhigh();

/* Check the stack for a known pattern */

	check_stacks(p);

#ifdef ROTTEN_INARDS
	addr = traceback();

	dumpframe(frame);

	printf("curproc=%08x paddr=%08x pcb=%08x curpcb=%08x\n",
	    (u_int) curproc, (u_int) curproc->p_addr,
	    (u_int) &curproc->p_addr->u_pcb, (u_int) curpcb);
	printf("CPSR=%08x ", GetCPSR());

	printf("Process = %08x ", (u_int)curproc);
	printf("pid = %d ", curproc->p_pid); 
	printf("comm = %s\n", curproc->p_comm); 

	pm_dumpw(irqstack.virtual + NBPG - 0x100, 0x100);
	pm_dumpw(undstack.virtual + NBPG - 0x20, 0x20);
	pm_dumpw(abtstack.virtual + NBPG - 0x20, 0x20);

	printf("abt_sp=%08x irq_sp=%08x und_sp=%08x svc_sp=%08x\n",
	    get_stackptr(PSR_ABT32_MODE),
	    get_stackptr(PSR_IRQ32_MODE),
	    get_stackptr(PSR_UND32_MODE),
	    get_stackptr(PSR_SVC32_MODE));

	if (curpcb)
		printf("curpcb=%08x pcb_sp=%08x pcb_und_sp=%08x\n", curpcb, curpcb->pcb_sp, curpcb->pcb_und_sp);

	printf("proc0=%08x paddr=%08x pcb=%08x\n", (u_int)&proc0,
	    (u_int)proc0.p_addr, (u_int) &proc0.p_addr->u_pcb);

/*
	kstack_stuff(&proc0);
	kstack_stuff(curproc);
*/
#else
	printf("Process = %08x ", (u_int)curproc);
	printf("pid = %d ", curproc->p_pid); 
	printf("comm = %s\n", curproc->p_comm); 
	printf("CPSR=%08x ", GetCPSR());

	printf("Traceback info\n");
	addr = simpletraceback();
	printf("Trapframe PC = %08x\n", frame->tf_pc);
	printf("Trapframe SPSR = %08x\n", frame->tf_spsr);

	if (usertraceback) {
		printf("Attempting user trackback\n");
		user_traceback(frame->tf_r11);
	}

#endif
	if ((frame->tf_spsr & PSR_MODE) == PSR_IRQ32_MODE
	    && addr >= irqstack.virtual && addr < (irqstack.virtual + NBPG)) {
		printf("Trap occurred in IRQ\n");
		printf("IRQ Traceback info\n");
		irqtraceback(addr, irqstack.virtual);
	}
	(void)splx(s);
}


void
buried_alive(p)
	struct proc *p;
{
	printf("Ok major screw up detected on kernel stack\n");
	printf("Putting the process down to minimise further trashing\n");
	printf("Process was %08x pid=%d comm=%s\n", (u_int) p, p->p_pid, p->p_comm);

}
#else
void
postmortem(frame)
	trapframe_t *frame;
{
	printf("No postmortem support compiled in\n");	
}

void
buried_alive(p)
	struct proc *p;
{
}
#endif

void
traceback_sym(lr, pc)
	u_int lr;
	u_int pc;
{
#ifdef DDB
	printf("fp->lr=%08x fp->pc=%08x\n", lr, pc);
/*	printf("fp->lr=");
	db_printsym((db_addr_t)(lr), DB_STGY_ANY);
	printf(" fp->pc=");
	db_printsym((db_addr_t)(pc), DB_STGY_ANY);	
	printf("\n");*/
#else
	printf("fp->lr=%08x fp->pc=%08x\n", lr, pc);
#endif
}

/* End of postmortem.c */
