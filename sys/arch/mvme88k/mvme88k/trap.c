/*	$OpenBSD: trap.c,v 1.8 1999/09/27 19:13:24 smurph Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>			/* kernel_map */

#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <machine/cpu.h>		/* DMT_VALID, etc. */
#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/m88100.h>		/* DMT_VALID, etc. */
#ifdef MVME197
#include <machine/m88110.h>		/* DMT_VALID, etc. */
#endif
#include <machine/trap.h>
#include <machine/psl.h>		/* FIP_E, etc. */
#include <machine/pcb.h>		/* FIP_E, etc. */

#include <sys/systm.h>

#if (DDB)
   #include <machine/db_machdep.h>
#else 
   #define PC_REGS(regs) ((regs->sxip & 2) ?  regs->sxip & ~3 : \
	(regs->snip & 2 ? regs->snip & ~3 : regs->sfip & ~3))
   #define inst_return(I) (((I)&0xfffffbffU) == 0xf400c001U ? TRUE : FALSE)
   #define inst_call(I) ({ unsigned i = (I); \
	   ((((i) & 0xf8000000U) == 0xc8000000U || /*bsr*/ \
      ((i) & 0xfffffbe0U) == 0xf400c800U)   /*jsr*/ \
	   ? TRUE : FALSE) \
      ;})

#endif /* DDB */
#define SSBREAKPOINT (0xF000D1F8U) /* Single Step Breakpoint */

#define TRAPTRACE
#if defined(TRAPTRACE)
unsigned traptrace = 0;
#endif

#if DDB
   #define DEBUG_MSG db_printf
#else
   #define DEBUG_MSG printf
#endif /* DDB */

#define USERMODE(PSR)   (((struct psr*)&(PSR))->psr_mode == 0)
#define SYSTEMMODE(PSR) (((struct psr*)&(PSR))->psr_mode != 0)

/* XXX MAJOR CLEANUP REQUIRED TO PORT TO BSD */

char  *trap_type[] = {
   "Reset",
   "Interrupt Exception",
   "Instruction Access",
   "Data Access Exception",
   "Misaligned Access",
   "Unimplemented Opcode",
   "Privileg Violation",
   "Bounds Check Violation",
   "Illegal Integer Divide",
   "Integer Overflow",
   "Error Exception",
};

char  *pbus_exception_type[] = {
   "Success (No Fault)",
   "unknown 1",
   "unknown 2",
   "Bus Error",
   "Segment Fault",
   "Page Fault",
   "Supervisor Violation",
   "Write Violation",
};
extern ret_addr;
#define NSIR	8
void (*sir_routines[NSIR])();
void *sir_args[NSIR];
u_char next_sir;

int   trap_types = sizeof trap_type / sizeof trap_type[0];

static inline void
userret(struct proc *p, struct m88100_saved_state *frame, u_quad_t oticks)
{
   int sig;
   int s;

   /* take pending signals */
   while ((sig = CURSIG(p)) != 0)
      postsig(sig);
   p->p_priority = p->p_usrpri;

   if (want_resched) {
      /*
       * Since we are curproc, clock will normally just change
       * our priority without moving us from one queue to another
       * (since the running process is not on a queue.)
       * If that happened after we put ourselves on the run queue
       * but before we switched, we might not be on the queue
       * indicated by our priority.
       */
      s = splstatclock();
      setrunqueue(p);
      p->p_stats->p_ru.ru_nivcsw++;
      mi_switch();
      (void) splx(s);
      while ((sig = CURSIG(p)) != 0)
         postsig(sig);
   }

   /*
    * If profiling, charge recent system time to the trapped pc.
    */
   if (p->p_flag & P_PROFIL)
      addupc_task(p, frame->sxip & ~3,
                  (int)(p->p_sticks - oticks));

   curpriority = p->p_priority;
}

void
panictrap(int type, struct m88100_saved_state *frame)
{
   static int panicing = 0;

   if (panicing++ == 0) {
      if (type == 2) {  /* instruction exception */
         DEBUG_MSG("\nInstr access fault (%s) v = %x, frame %x\n",
                pbus_exception_type[(frame->ipfsr >> 16) & 0x7],
                frame->sxip & ~3, frame);
      } else if (type == 3) { /* data access exception */
         DEBUG_MSG("\nData access fault (%s) v = %x, frame %x\n",
                pbus_exception_type[(frame->dpfsr >> 16) & 0x7],
                frame->sxip & ~3, frame);
      } else
         DEBUG_MSG("\ntrap type %d, v = %x, frame %x\n", type, frame->sxip & ~3, frame);
      regdump(frame);
   }
   if ((u_int)type < trap_types)
      panic(trap_type[type]);
   panic("trap");
   /*NOTREACHED*/
}


#if defined(MVME187) || defined(MVME188)
unsigned last_trap[4] = {0,0,0,0};
/*ARGSUSED*/
void
trap(unsigned type, struct m88100_saved_state *frame)
{
   struct proc *p;
   u_quad_t sticks = 0;
   vm_map_t map;
   vm_offset_t va;
   vm_prot_t ftype;
   int fault_type;
   u_long fault_code;
   unsigned nss, fault_addr;
   struct vmspace *vm;
   union sigval sv;
   int su = 0;
   int result;
   int sig = 0;
   unsigned pc = PC_REGS(frame);  /* get program counter (sxip) */

   extern vm_map_t kernel_map;
   extern int fubail(), subail();
   extern unsigned guarded_access_start;
   extern unsigned guarded_access_end;
   extern unsigned guarded_access_bad;

   if (type != last_trap[3]) {
      last_trap[0] = last_trap[1];
      last_trap[1] = last_trap[2];
      last_trap[2] = last_trap[3];
      last_trap[3] = type;
   }
   cnt.v_trap++;
   if ((p = curproc) == NULL)
      p = &proc0;

   if (USERMODE(frame->epsr)) {
      sticks = p->p_sticks;
      type += T_USER;
      p->p_md.md_tf = frame;  /* for ptrace/signals */
      fault_type = 0;
      fault_code = 0;
   }
/*   printf("trap 0x%x ", type); */
   switch (type) {
      default:
         panictrap(frame->vector, frame);
         /*NOTREACHED*/

#if defined(DDB)
      case T_KDB_BREAK:
         /*FALLTHRU*/
      case T_KDB_BREAK+T_USER:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_break_trap(T_KDB_BREAK,(db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }
      case T_KDB_ENTRY:
         /*FALLTHRU*/
      case T_KDB_ENTRY+T_USER:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_entry_trap(T_KDB_ENTRY,(db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }

   #if 0
      case T_ILLFLT:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_error_trap(type == T_ILLFLT ? "unimplemented opcode" :
                           "error fault", (db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }
   #endif /* 0 */
#endif /* DDB */
      case T_INT:
      case T_INT+T_USER:
         /* This function pointer is set in machdep.c 
            It calls m188_ext_int or sbc_ext_int depending
            on the value of cputyp - smurph */
         (*mdfp.interrupt_func)(T_INT, frame);
         return;

      case T_MISALGNFLT:
         DEBUG_MSG("kernel misalgined "
                   "access exception @ 0x%08x\n", frame->sxip);
         panictrap(frame->vector, frame);
         break;

      case T_INSTFLT:
         /* kernel mode instruction access fault.
          * Should never, never happen for a non-paged kernel.
          */
         DEBUG_MSG("kernel mode instruction "
                   "page fault @ 0x%08x\n", frame->sxip);
         panictrap(frame->vector, frame);
         break;

      case T_DATAFLT:
         /* kernel mode data fault */
         /*
          * If the faulting address is in user space, handle it in
          * the context of the user process. Else, use kernel map.
          */

         if (type == T_DATAFLT) {
            fault_addr = frame->dma0;
            if (frame->dmt0 & (DMT_WRITE|DMT_LOCKBAR)) {
               ftype = VM_PROT_READ|VM_PROT_WRITE;
               fault_code = VM_PROT_WRITE;
            } else {
               ftype = VM_PROT_READ;
               fault_code = VM_PROT_READ;
            }
         } else {
            fault_addr = frame->sxip & XIP_ADDR;
            ftype = VM_PROT_READ;
            fault_code = VM_PROT_READ;
         }

         va = trunc_page((vm_offset_t)fault_addr);

         vm = p->p_vmspace;
         map = &vm->vm_map;

         /* data fault on a kernel address... */
         if (frame->dmt0 & DMT_DAS)
            map = kernel_map;

         /* 
          * We don't want to call vm_fault() if it is fuwintr() or
          * suwintr(). These routines are for copying from interrupt
          * context and vm_fault() can potentially sleep. You may
          * wonder if it isn't bad karma for an interrupt handler to	
          * touch the current process. Indeed it is, but clock interrupt
          * does it while doing profiling. It is OK in that context.
          */

         if (p->p_addr->u_pcb.pcb_onfault == (int)fubail ||
             p->p_addr->u_pcb.pcb_onfault == (int)subail)
            goto outtahere;

         /* data fault on the user address */
         if (type == T_DATAFLT && (frame->dmt0 & DMT_DAS) == 0) {
            type = T_DATAFLT + T_USER;
            goto user_fault;
         }

         /*
          *	If it is a guarded access, bus error is OK.
          */

         if ((frame->dpfsr >> 16 & 0x7) == 0x3 &&     /* bus error */
             (frame->sxip & ~3) >= (unsigned)&guarded_access_start &&
             (frame->sxip & ~3) <= (unsigned)&guarded_access_end) {

            frame->snip = ((unsigned)&guarded_access_bad    ) | FIP_V;
            frame->sfip = ((unsigned)&guarded_access_bad + 4) | FIP_V;
            frame->sxip = 0;
            frame->dmt0 = 0;/* XXX what about other trans. in data unit */
            frame->dpfsr = 0;
            return;
         }

         /*
          *	On a segment or a page fault, call vm_fault() to resolve
          *	the fault.
          */
         if ((frame->dpfsr >> 16 & 0x7) == 0x4        /* seg fault  */
             || (frame->dpfsr >> 16 & 0x7) == 0x5) { /* page fault */
            result = vm_fault(map, va, ftype, FALSE); 
            /*
            printf("vm_fault(map 0x%x, va 0x%x, ftype 0x%x, FALSE) -> %d (%s)\n", 
                   map, va, ftype, result,
                   result ? "KERN_INVALID_ADDRESS" : "KERN_SUCCESS");
            */                   
            if (result == KERN_SUCCESS) {
               /*
                * We could resolve the fault. Call
                * data_access_emulation to drain the data unit pipe
                * line and reset dmt0 so that trap won't get called
                * again. For inst faults, back up the pipe line.
                */
               if (type == T_DATAFLT) {
                  /*
                  printf("calling data_access_emulation()\n");
                  */
                  data_access_emulation(frame);
                  frame->dmt0 = 0;
                  frame->dpfsr = 0;
               } else {
                  frame->sfip = frame->snip & ~FIP_E;
                  frame->snip = frame->sxip & ~NIP_E;
               }
               return;
            }
         }
         /*
         printf ("PBUS Fault %d (%s) va = 0x%x\n", ((frame->dpfsr >> 16) & 0x7), 
                 pbus_exception_type[(frame->dpfsr >> 16) & 0x7], va);
         */
         /*
          * if still the fault is not resolved ...
          */
         if (!p->p_addr->u_pcb.pcb_onfault)
            panictrap(frame->vector, frame);

         outtahere:
         frame->snip = ((unsigned)p->p_addr->u_pcb.pcb_onfault    ) | FIP_V;
         frame->sfip = ((unsigned)p->p_addr->u_pcb.pcb_onfault + 4) | FIP_V;
         frame->sxip = 0;
         frame->dmt0 = 0;  /* XXX what about other trans. in data unit */
         frame->dpfsr = 0;
         return;
      case T_INSTFLT+T_USER:
         /* User mode instruction access fault */
         /*FALLTHRU*/
      case T_DATAFLT+T_USER:
         user_fault:
/*         printf("\nUser Data access fault (%s) v = %x, frame %x\n",
                pbus_exception_type[(frame->dpfsr >> 16) & 0x7],
                frame->sxip & ~3, frame);
*/

         if (type == T_INSTFLT+T_USER) {
            fault_addr = frame->sxip & XIP_ADDR;
         } else {
            fault_addr = frame->dma0;
         }

         if (frame->dmt0 & (DMT_WRITE|DMT_LOCKBAR)) {
            ftype = VM_PROT_READ|VM_PROT_WRITE;
            fault_code = VM_PROT_WRITE;
         } else {
            ftype = VM_PROT_READ;
            fault_code = VM_PROT_READ;
         }

         va = trunc_page((vm_offset_t)fault_addr);

         vm = p->p_vmspace;
         map = &vm->vm_map;

         /* Call vm_fault() to resolve non-bus error faults */
         if ((frame->ipfsr >> 16 & 0x7) != 0x3 &&
             (frame->dpfsr >> 16 & 0x7) != 0x3) {

            result = vm_fault(map, va, ftype, FALSE); 
            frame->ipfsr = frame->dpfsr = 0;
            /*
            printf("vm_fault(map 0x%x, va 0x%x, ftype 0x%x, FALSE) -> %d (%s)\n", 
                   map, va, ftype, result,
                   result ? "KERN_INVALID_ADDRESS" : "KERN_SUCCESS");
            */
         }

         if ((caddr_t)va >= vm->vm_maxsaddr) {
            if (result == KERN_SUCCESS) {
               nss = clrnd(btoc(USRSTACK - va));/* XXX check this */
               if (nss > vm->vm_ssize)
                  vm->vm_ssize = nss;
            } else if (result == KERN_PROTECTION_FAILURE)
               result = KERN_INVALID_ADDRESS;
         }

         if (result == KERN_SUCCESS) {
            if (type == T_DATAFLT+T_USER) {
               /*
               printf("calling data_access_emulation()\n");
               */
               /*
                * We could resolve the fault. Call
                * data_access_emulation to drain the data unit
                * pipe line and reset dmt0 so that trap won't
                * get called again.
                */
               data_access_emulation(frame);
               frame->dmt0 = 0;
               frame->dpfsr = 0;
            } else {
               /* back up SXIP, SNIP clearing the the Error bit */
               frame->sfip = frame->snip & ~FIP_E;
               frame->snip = frame->sxip & ~NIP_E;
            }
         } else {
            sig = result == KERN_PROTECTION_FAILURE ? SIGBUS : SIGSEGV;
            fault_type = result == KERN_PROTECTION_FAILURE ? BUS_ADRERR
                         : SEGV_MAPERR;
         }
         /*
         printf("sig == %d, fault_type == %d\n", sig, fault_type);
         */
         break;

      case T_MISALGNFLT+T_USER:
/*	DEBUG_MSG("T_MISALGNFLT\n");*/
         sig = SIGBUS;
         fault_type = BUS_ADRALN;
/*	panictrap(fault_type, frame);*/
         break;

      case T_PRIVINFLT+T_USER:
      case T_ILLFLT+T_USER:
         sig = SIGILL;
         break;

      case T_BNDFLT+T_USER:
         sig = SIGFPE;
         break;
      case T_ZERODIV+T_USER:
         sig = SIGFPE;
         fault_type = FPE_INTDIV;
         break;
      case T_OVFFLT+T_USER:
         sig = SIGFPE;
         fault_type = FPE_INTOVF;
         break;

      case T_FPEPFLT+T_USER:
      case T_FPEIFLT+T_USER:
         sig = SIGFPE;
         break;

      case T_SIGTRAP+T_USER:
         sig = SIGTRAP;
         fault_type = TRAP_TRACE;
         break;

      case T_STEPBPT+T_USER:
         /*
          * This trap is used by the kernel to support single-step
          * debugging (although any user could generate this trap
          * which should probably be handled differently). When a
          * process is continued by a debugger with the PT_STEP
          * function of ptrace (single step), the kernel inserts
          * one or two breakpoints in the user process so that only
          * one instruction (or two in the case of a delayed branch)
          * is executed.  When this breakpoint is hit, we get the
          * T_STEPBPT trap.
          */
         
         {
            register unsigned va;
            unsigned instr;
            struct uio uio;
            struct iovec iov;

            /* compute address of break instruction */
            va = pc;

            /* read break instruction */
            instr = fuiword((caddr_t)pc);
#if 0
            printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
                   p->p_comm, p->p_pid, instr, pc,
                   p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
            /* check and see if we got here by accident */
            if ((p->p_md.md_ss_addr != pc && 
                p->p_md.md_ss_taken_addr != pc) ||
                instr != SSBREAKPOINT) {
               sig = SIGTRAP;
               fault_type = TRAP_TRACE;
               break;
            }
            /* restore original instruction and clear BP  */
            instr = p->p_md.md_ss_instr;
            va = p->p_md.md_ss_addr;
            if (va != 0) {
               iov.iov_base = (caddr_t)&instr;
               iov.iov_len = sizeof(int); 
               uio.uio_iov = &iov;
               uio.uio_iovcnt = 1; 
               uio.uio_offset = (off_t)va;
               uio.uio_resid = sizeof(int);
               uio.uio_segflg = UIO_SYSSPACE;
               uio.uio_rw = UIO_WRITE;
               uio.uio_procp = curproc;
               procfs_domem(p, p, NULL, &uio);
            }
            
            /* branch taken instruction */
            instr = p->p_md.md_ss_taken_instr;
            va = p->p_md.md_ss_taken_addr;
            if (instr != 0) {
               iov.iov_base = (caddr_t)&instr;
               iov.iov_len = sizeof(int); 
               uio.uio_iov = &iov;
               uio.uio_iovcnt = 1; 
               uio.uio_offset = (off_t)va;
               uio.uio_resid = sizeof(int);
               uio.uio_segflg = UIO_SYSSPACE;
               uio.uio_rw = UIO_WRITE;
               uio.uio_procp = curproc;
               procfs_domem(p, p, NULL, &uio);
            }
#if 1
            frame->sfip = frame->snip;    /* set up next FIP */
            frame->snip = pc;    /* set up next NIP */
            frame->snip |= 2;         /* set valid bit   */
#endif
            p->p_md.md_ss_addr = 0;
            p->p_md.md_ss_instr = 0;
            p->p_md.md_ss_taken_addr = 0;
            p->p_md.md_ss_taken_instr = 0;
            sig = SIGTRAP;
            fault_type = TRAP_BRKPT;
         }
         break;

      case T_USERBPT+T_USER:
         /*
          * This trap is meant to be used by debuggers to implement
          * breakpoint debugging.  When we get this trap, we just
          * return a signal which gets caught by the debugger.
          */
         frame->sfip = frame->snip;    /* set up the next FIP */
         frame->snip = frame->sxip;    /* set up the next NIP */
         sig = SIGTRAP;
         fault_type = TRAP_BRKPT;
         break;

      case T_ASTFLT+T_USER:
         want_ast = 0;
         if (p->p_flag & P_OWEUPC) {
            p->p_flag &= ~P_OWEUPC;
            ADDUPROF(p);
         }
         break;
   }

   /*
    * If trap from supervisor mode, just return
    */
   if (SYSTEMMODE(frame->epsr))
      return;

   if (sig) {
      sv.sival_int = fault_addr;
      trapsignal(p, sig, fault_code, fault_type, sv);
      /*		
       * don't want multiple faults - we are going to
       * deliver signal.
       */
      frame->dmt0 = 0;
      frame->dpfsr = 0;
   }

   userret(p, frame, sticks);
}
#endif /* defined(MVME187) || defined(MVME188) */
/*ARGSUSED*/
#ifdef MVME197
void
trap2(unsigned type, struct m88100_saved_state *frame)
{
   struct proc *p;
   u_quad_t sticks = 0;
   vm_map_t map;
   vm_offset_t va;
   vm_prot_t ftype;
   int fault_type;
   u_long fault_code;
   unsigned nss, fault_addr;
   struct vmspace *vm;
   union sigval sv;
   int su = 0;
   int result;
   int sig = 0;
   unsigned pc = PC_REGS(frame);  /* get program counter (sxip) */
   unsigned dsr, isr, user = 0, write = 0, data = 0;

   extern vm_map_t kernel_map;
   extern int fubail(), subail();
   extern unsigned guarded_access_start;
   extern unsigned guarded_access_end;
   extern unsigned guarded_access_bad;

   cnt.v_trap++;
   if ((p = curproc) == NULL)
      p = &proc0;

   if (USERMODE(frame->epsr)) {
      sticks = p->p_sticks;
      type += T_USER;
      p->p_md.md_tf = frame;  /* for ptrace/signals */
      fault_type = 0;
      fault_code = 0;
   }
   printf("m197_trap 0x%x ", type);
   switch (type) {
      default:
         panictrap(frame->vector, frame);
         /*NOTREACHED*/
      case T_197_READ+T_USER:
         user = 1;
      case T_197_READ:
         va = (vm_offset_t) frame->dlar;
         /* if it was a user read, handle in context of the user */
         if ((frame->dsr & CMMU_DSR_SU) && !user) {
            map = kernel_map;
         } else {
            vm = p->p_vmspace;
            map = &vm->vm_map;
         }
         result = m197_table_search(map->pmap, va, CMMU_READ, user, CMMU_DATA);
         if (result) {
            switch (result) {
               case 4: /* Seg Fault */
                  frame->dsr |= CMMU_DSR_SI | CMMU_DSR_RW;
                  break;
               case 5: /* Page Fault */
                  frame->dsr |= CMMU_DSR_PI | CMMU_DSR_RW;
                  break;
               case 6: /* Supervisor Violation */
                  frame->dsr |= CMMU_DSR_SP | CMMU_DSR_RW;
                  break;
            }
            /* table search failed and we are going to report a data fault */
            if (user) {
               type = T_DATAFLT+T_USER;
               goto m197_user_fault;
            } else {
               type = T_DATAFLT;
               goto m197_data_fault;
            }
         } else {
            return; /* PATC sucessfully loaded */
         }
         break;      
      case T_197_WRITE+T_USER:
         user = 1;
      case T_197_WRITE:
         /* if it was a user read, handle in context of the user */
         if ((frame->dsr & CMMU_DSR_SU) && !user) {
            map = kernel_map;
         } else {
            vm = p->p_vmspace;
            map = &vm->vm_map;
         }
         va = (vm_offset_t) frame->dlar;
         result = m197_table_search(map->pmap, va, CMMU_WRITE, user, CMMU_DATA);
         if (result) {
            switch (result) {
               case 4: /* Seg Fault */
                  frame->dsr |= CMMU_DSR_SI;
                  break;
               case 5: /* Page Fault */
                  frame->dsr |= CMMU_DSR_PI;
                  break;
               case 6: /* Supervisor Violation */
                  frame->dsr |= CMMU_DSR_SP;
                  break;
               case 7: /* Write Violation */
                  frame->dsr |= CMMU_DSR_WE;
                  break;
            }
            /* table search failed and we are going to report a data fault */
            if (user) {
               type = T_DATAFLT+T_USER;
               goto m197_user_fault;
            } else {
               type = T_DATAFLT;
               goto m197_data_fault;
            }
         } else {
            return; /* PATC sucessfully loaded */
         }
         break;      
      case T_197_INST+T_USER:
         user = 1;
      case T_197_INST:
         /* if it was a user read, handle in context of the user */
         if ((frame->isr & CMMU_ISR_SU) && !user) {
            map = kernel_map;
         } else {
            vm = p->p_vmspace;
            map = &vm->vm_map;
         }
         va = (vm_offset_t) frame->sxip;
         result = m197_table_search(map->pmap, va, CMMU_READ, user, CMMU_INST);
         if (result) {
            switch (result) {
               case 4: /* Seg Fault */
                  frame->isr |= CMMU_ISR_SI;
                  break;
               case 5: /* Page Fault */
                  frame->isr |= CMMU_ISR_PI;
                  break;
               case 6: /* Supervisor Violation */
                  frame->isr |= CMMU_ISR_SP;
                  break;
            }
            /* table search failed and we are going to report a data fault */
            if (user) {
               type = T_INSTFLT+T_USER;
               goto m197_user_fault;
            } else {
               type = T_INSTFLT;
               goto m197_inst_fault;
            }
         } else {
            return; /* PATC sucessfully loaded */
         }
         break;      
   #if defined(DDB)
      case T_KDB_BREAK:
         /*FALLTHRU*/
      case T_KDB_BREAK+T_USER:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_break_trap(T_KDB_BREAK,(db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }
      case T_KDB_ENTRY:
         /*FALLTHRU*/
      case T_KDB_ENTRY+T_USER:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_entry_trap(T_KDB_ENTRY,(db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }

      #if 0
      case T_ILLFLT:
         {
            int s = db_splhigh();
            db_enable_interrupt();
            ddb_error_trap(type == T_ILLFLT ? "unimplemented opcode" :
                           "error fault", (db_regs_t*)frame);
            db_disable_interrupt();
            db_splx(s);
            return;
         }
      #endif /* 0 */
   #endif /* DDB */
      case T_ILLFLT:
         DEBUG_MSG("test trap "
                   "page fault @ 0x%08x\n", frame->sxip);
         panictrap(frame->vector, frame);

      case T_MISALGNFLT:
         DEBUG_MSG("kernel misalgined "
                   "access exception @ 0x%08x\n", frame->sxip);
         panictrap(frame->vector, frame);
         break;

      case T_INSTFLT:
m197_inst_fault:
         /* kernel mode instruction access fault.
          * Should never, never happen for a non-paged kernel.
          */
         DEBUG_MSG("kernel mode instruction "
                   "page fault @ 0x%08x\n", frame->sxip);
         panictrap(frame->vector, frame);
         break;

      case T_DATAFLT:
         /* kernel mode data fault */
         /*
          * If the faulting address is in user space, handle it in
          * the context of the user process. Else, use kernel map.
          */
m197_data_fault:
         if (type == T_DATAFLT) {
            fault_addr = frame->dlar;
            if (frame->dsr & CMMU_DSR_RW) {
               ftype = VM_PROT_READ;
               fault_code = VM_PROT_READ;
            } else {
               ftype = VM_PROT_READ|VM_PROT_WRITE;
               fault_code = VM_PROT_WRITE;
               write = 1;
            }
            data = 1;
         } else {
            fault_addr = frame->sxip & XIP_ADDR;
            ftype = VM_PROT_READ;
            fault_code = VM_PROT_READ;
         }

         va = trunc_page((vm_offset_t)fault_addr);
         vm = p->p_vmspace;
         map = &vm->vm_map;

         /* data fault on a kernel address... */
         if (type == T_DATAFLT) {
            if (frame->dsr & CMMU_DSR_SU) {
               map = kernel_map;
            }
         }

         /* 
          * We don't want to call vm_fault() if it is fuwintr() or
          * suwintr(). These routines are for copying from interrupt
          * context and vm_fault() can potentially sleep. You may
          * wonder if it isn't bad karma for an interrupt handler to	
          * touch the current process. Indeed it is, but clock interrupt
          * does it while doing profiling. It is OK in that context.
          */

         if (p->p_addr->u_pcb.pcb_onfault == (int)fubail ||
             p->p_addr->u_pcb.pcb_onfault == (int)subail)
            goto m197_outtahere;

         /* data fault on the user address */
         if (type == T_DATAFLT && (frame->dsr & CMMU_DSR_SU) == 0) {
            type = T_DATAFLT + T_USER;
            goto m197_user_fault;
         }

         /*
          *	If it is a guarded access, bus error is OK.
          */

         if ((frame->dsr & CMMU_DSR_BE) &&     /* bus error */
             (frame->sxip & ~3) >= (unsigned)&guarded_access_start &&
             (frame->sxip & ~3) <= (unsigned)&guarded_access_end) {
            return;
         }

         /*
          *	On a segment or a page fault, call vm_fault() to resolve
          *	the fault.
          */
         result = m197_table_search(map->pmap, va, write, 1, data);
/* todo         
         switch (result) {
         case :
         }
*/         
         if (type == T_DATAFLT) {
            if ((frame->dsr & CMMU_DSR_SI)        /* seg fault  */
                || (frame->dsr & CMMU_DSR_PI)) { /* page fault */
               result = vm_fault(map, va, ftype, FALSE); 
               if (result == KERN_SUCCESS) {
                  return;
               }
            }
         } else {
            if ((frame->isr & CMMU_ISR_SI)        /* seg fault  */
                || (frame->isr & CMMU_ISR_PI)) { /* page fault */
               result = vm_fault(map, va, ftype, FALSE); 
               if (result == KERN_SUCCESS) {
                  return;
               }
            }
         }

         /*
         printf ("PBUS Fault %d (%s) va = 0x%x\n", ((frame->dpfsr >> 16) & 0x7), 
                 pbus_exception_type[(frame->dpfsr >> 16) & 0x7], va);
         */
         /*
          * if still the fault is not resolved ...
          */
         if (!p->p_addr->u_pcb.pcb_onfault)
            panictrap(frame->vector, frame);

m197_outtahere:
         frame->sxip = ((unsigned)p->p_addr->u_pcb.pcb_onfault);
         return;
      case T_INSTFLT+T_USER:
         /* User mode instruction access fault */
         /*FALLTHRU*/
      case T_DATAFLT+T_USER:
m197_user_fault:
/*         printf("\nUser Data access fault (%s) v = %x, frame %x\n",
                pbus_exception_type[(frame->dpfsr >> 16) & 0x7],
                frame->sxip & ~3, frame);
*/

         if (type == T_INSTFLT+T_USER) {
            fault_addr = frame->sxip & XIP_ADDR;
            ftype = VM_PROT_READ;
            fault_code = VM_PROT_READ;
         } else {
            fault_addr = frame->dlar;
            if (frame->dsr & CMMU_DSR_RW) {
               ftype = VM_PROT_READ;
               fault_code = VM_PROT_READ;
            } else {
               ftype = VM_PROT_READ|VM_PROT_WRITE;
               fault_code = VM_PROT_WRITE;
            }
         }

         va = trunc_page((vm_offset_t)fault_addr);

         vm = p->p_vmspace;
         map = &vm->vm_map;

         /* Call vm_fault() to resolve non-bus error faults */
         if (type == T_DATAFLT+T_USER) {
            if ((frame->dsr & CMMU_DSR_SI)        /* seg fault  */
                || (frame->dsr & CMMU_DSR_PI)) { /* page fault */
               result = vm_fault(map, va, ftype, FALSE); 
               if (result == KERN_SUCCESS) {
                  return;
               }
            }
         } else {
            if ((frame->isr & CMMU_ISR_SI)        /* seg fault  */
                || (frame->isr & CMMU_ISR_PI)) { /* page fault */
               result = vm_fault(map, va, ftype, FALSE); 
               if (result == KERN_SUCCESS) {
                  return;
               }
            }
         }

         if ((caddr_t)va >= vm->vm_maxsaddr) {
            if (result == KERN_SUCCESS) {
               nss = clrnd(btoc(USRSTACK - va));/* XXX check this */
               if (nss > vm->vm_ssize)
                  vm->vm_ssize = nss;
            } else if (result == KERN_PROTECTION_FAILURE)
               result = KERN_INVALID_ADDRESS;
         }

         if (result != KERN_SUCCESS) {
            sig = result == KERN_PROTECTION_FAILURE ? SIGBUS : SIGSEGV;
            fault_type = result == KERN_PROTECTION_FAILURE ? BUS_ADRERR
                         : SEGV_MAPERR;
         } else {
            return;
         }
         /*
         printf("sig == %d, fault_type == %d\n", sig, fault_type);
         */
         break;

      case T_MISALGNFLT+T_USER:
/*	DEBUG_MSG("T_MISALGNFLT\n");*/
         sig = SIGBUS;
         fault_type = BUS_ADRALN;
/*	panictrap(fault_type, frame);*/
         break;

      case T_PRIVINFLT+T_USER:
      case T_ILLFLT+T_USER:
         sig = SIGILL;
         break;

      case T_BNDFLT+T_USER:
         sig = SIGFPE;
         break;
      case T_ZERODIV+T_USER:
         sig = SIGFPE;
         fault_type = FPE_INTDIV;
         break;
      case T_OVFFLT+T_USER:
         sig = SIGFPE;
         fault_type = FPE_INTOVF;
         break;

      case T_FPEPFLT+T_USER:
      case T_FPEIFLT+T_USER:
         sig = SIGFPE;
         break;

      case T_SIGTRAP+T_USER:
         sig = SIGTRAP;
         fault_type = TRAP_TRACE;
         break;

      case T_STEPBPT+T_USER:
         /*
          * This trap is used by the kernel to support single-step
          * debugging (although any user could generate this trap
          * which should probably be handled differently). When a
          * process is continued by a debugger with the PT_STEP
          * function of ptrace (single step), the kernel inserts
          * one or two breakpoints in the user process so that only
          * one instruction (or two in the case of a delayed branch)
          * is executed.  When this breakpoint is hit, we get the
          * T_STEPBPT trap.
          */
   #if 0
         frame->sfip = frame->snip;    /* set up next FIP */
         frame->snip = frame->sxip;    /* set up next NIP */
         break;
   #endif
         {
            register unsigned va;
            unsigned instr;
            struct uio uio;
            struct iovec iov;

            /* compute address of break instruction */
            va = pc;

            /* read break instruction */
            instr = fuiword((caddr_t)pc);
   #if 1
            printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
                   p->p_comm, p->p_pid, instr, pc,
                   p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
   #endif
            /* check and see if we got here by accident */
/*
   if (p->p_md.md_ss_addr != pc || instr != SSBREAKPOINT) {
      sig = SIGTRAP;
      fault_type = TRAP_TRACE;
      break;
   }
*/
            /* restore original instruction and clear BP  */
            /*sig = suiword((caddr_t)pc, p->p_md.md_ss_instr);*/
            instr = p->p_md.md_ss_instr;
            if (instr == 0) {
               printf("Warning: can't restore instruction at %x: %x\n",
                      p->p_md.md_ss_addr, p->p_md.md_ss_instr);
            } else {
               iov.iov_base = (caddr_t)&instr;
               iov.iov_len = sizeof(int); 
               uio.uio_iov = &iov;
               uio.uio_iovcnt = 1; 
               uio.uio_offset = (off_t)pc;
               uio.uio_resid = sizeof(int);
               uio.uio_segflg = UIO_SYSSPACE;
               uio.uio_rw = UIO_WRITE;
               uio.uio_procp = curproc;
            }

            frame->sfip = frame->snip;    /* set up next FIP */
            frame->snip = frame->sxip;    /* set up next NIP */
            frame->snip |= 2;         /* set valid bit   */
            p->p_md.md_ss_addr = 0;
            sig = SIGTRAP;
            fault_type = TRAP_BRKPT;
            break;
         }

      case T_USERBPT+T_USER:
         /*
          * This trap is meant to be used by debuggers to implement
          * breakpoint debugging.  When we get this trap, we just
          * return a signal which gets caught by the debugger.
          */
         frame->sfip = frame->snip;    /* set up the next FIP */
         frame->snip = frame->sxip;    /* set up the next NIP */
         sig = SIGTRAP;
         fault_type = TRAP_BRKPT;
         break;

      case T_ASTFLT+T_USER:
         want_ast = 0;
         if (p->p_flag & P_OWEUPC) {
            p->p_flag &= ~P_OWEUPC;
            ADDUPROF(p);
         }
         break;
   }

   /*
    * If trap from supervisor mode, just return
    */
   if (SYSTEMMODE(frame->epsr))
      return;

   if (sig) {
      sv.sival_int = fault_addr;
      trapsignal(p, sig, fault_code, fault_type, sv);
      /*		
       * don't want multiple faults - we are going to
       * deliver signal.
       */
      frame->dsr = 0;
   }
   userret(p, frame, sticks);
}
#endif /* MVME197 */
void
test_trap2(int num, int m197)
{
   DEBUG_MSG("\n[test_trap (Good News[tm]) m197 = %d, vec = %d]\n", m197, num);
   bugreturn();
}

void
test_trap(struct m88100_saved_state *frame)
{
   DEBUG_MSG("\n[test_trap (Good News[tm]) frame 0x%08x]\n", frame);
   regdump((struct trapframe*)frame);
   bugreturn();
}
void
error_fault(struct m88100_saved_state *frame)
{
   DEBUG_MSG("\n[ERROR EXCEPTION (Bad News[tm]) frame 0x%08x]\n", frame);
   regdump((struct trapframe*)frame);
   DEBUG_MSG("trap trace %x -> %x -> %x -> %x\n", last_trap[0], last_trap[1], last_trap[2], last_trap[3]);
#if DDB 
   gimmeabreak();
   DEBUG_MSG("[you really can't restart after an error exception.]\n");
   gimmeabreak();
#endif /* DDB */
   bugreturn();  /* This gets us to Bug instead of a loop forever */
}

void
error_reset(struct m88100_saved_state *frame) 
{
   DEBUG_MSG("\n[RESET EXCEPTION (Really Bad News[tm]) frame 0x%08x]\n", frame);
   DEBUG_MSG("This is usually caused by a branch to a NULL function pointer.\n");
   DEBUG_MSG("Use the debugger trace command to track it down.\n");

#if DDB 
   gimmeabreak();
   DEBUG_MSG("[It's useless to restart after a reset exception. You might as well reboot.]\n");
   gimmeabreak();
#endif /* DDB */
   bugreturn();  /* This gets us to Bug instead of a loop forever */
}

syscall(register_t code, struct m88100_saved_state *tf)
{
   register int i, nsys, *ap, nap;
   register struct sysent *callp;
   register struct proc *p;
   int error, new;
   struct args {
      int i[8];
   } args;
   int rval[2];
   u_quad_t sticks;
   extern struct pcb *curpcb;

   cnt.v_syscall++;

   p = curproc;

   callp = p->p_emul->e_sysent;
   nsys  = p->p_emul->e_nsysent;

#ifdef DIAGNOSTIC
   if (USERMODE(tf->epsr) == 0)
      panic("syscall");
   if (curpcb != &p->p_addr->u_pcb)
      panic("syscall curpcb/ppcb");
   if (tf != (struct trapframe *)&curpcb->user_state)
      panic("syscall trapframe");
#endif

   sticks = p->p_sticks;
   p->p_md.md_tf = tf;

   /*
    * For 88k, all the arguments are passed in the registers (r2-r12)
    * For syscall (and __syscall), r2 (and r3) has the actual code.
    * __syscall  takes a quad syscall number, so that other
    * arguments are at their natural alignments.
    */
   ap = &tf->r[2];
   nap = 6;

   switch (code) {
      case SYS_syscall:
         code = *ap++;
         nap--;
         break;
      case SYS___syscall:
         if (callp != sysent)
            break;
         code = ap[_QUAD_LOWWORD];
         ap += 2;
         nap -= 2;
         break;
   }

   /* Callp currently points to syscall, which returns ENOSYS. */

   if (code < 0 || code >= nsys)
      callp += p->p_emul->e_nosys;
   else {
      callp += code;
      i = callp->sy_argsize / sizeof(register_t);
      if (i > 8)
         panic("syscall nargs");
      /*
       * just copy them; syscall stub made sure all the
       * args are moved from user stack to registers.
       */
      bcopy((caddr_t)ap, (caddr_t)args.i, i * sizeof(register_t));
   }
#ifdef SYSCALL_DEBUG
   scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
   if (KTRPOINT(p, KTR_SYSCALL))
      ktrsyscall(p->p_tracep, code, callp->sy_argsize, args.i);
#endif
   rval[0] = 0;
   rval[1] = 0;
   error = (*callp->sy_call)(p, &args, rval);
   /*
    * system call will look like:
    *	 ld r10, r31, 32; r10,r11,r12 might be garbage.
    *	 ld r11, r31, 36
    *	 ld r12, r31, 40
    *	 or r13, r0, <code>
    *       tb0 0, r0, <128> <- xip
    *	 br err 	  <- nip
    *       jmp r1 	  <- fip
    *  err: or.u r3, r0, hi16(errno)
    *	 st r2, r3, lo16(errno)
    *	 subu r2, r0, 1 
    *	 jmp r1
    *
    * So, when we take syscall trap, sxip/snip/sfip will be as
    * shown above.
    * Given this,
    * 1. If the system call returned 0, need to skip nip.
    *	nip = fip, fip += 4
    *    (doesn't matter what fip + 4 will be but we will never
    *    execute this since jmp r1 at nip will change the execution flow.)
    * 2. If the system call returned an errno > 0, plug the value
    *    in r2, and leave nip and fip unchanged. This will have us
    *    executing "br err" on return to user space.
    * 3. If the system call code returned ERESTART,
    *    we need to rexecute the trap instruction. Back up the pipe
    *    line.
    *     fip = nip, nip = xip
    * 4. If the system call returned EJUSTRETURN, don't need to adjust
    *    any pointers.
    */

   if (error == 0) {
      /*
       * If fork succeeded and we are the child, our stack
       * has moved and the pointer tf is no longer valid,
       * and p is wrong.  Compute the new trapframe pointer.
       * (The trap frame invariably resides at the
       * tippity-top of the u. area.)
       */
      p = curproc;
      tf = USER_REGS(p);
      tf->r[2] = rval[0];
      tf->r[3] = rval[1];
      tf->epsr &= ~PSR_C;
      tf->snip = tf->sfip & ~FIP_E;
      tf->sfip = tf->snip + 4;
   } else if (error > 0) {
      /* error != ERESTART && error != EJUSTRETURN*/
      tf->r[2] = error;
      tf->epsr |= PSR_C;   /* fail */
      tf->snip = tf->snip & ~NIP_E;
      tf->sfip = tf->sfip & ~FIP_E;
   } else if (error == ERESTART) {
      /*
       * If (error == ERESTART), back up the pipe line. This
       * will end up reexecuting the trap.
       */
      tf->epsr &= ~PSR_C;
      tf->sfip = tf->snip & ~NIP_E;
      tf->snip = tf->sxip & ~NIP_E;
   } else {
      /* if (error == EJUSTRETURN), leave the ip's alone */
      tf->epsr &= ~PSR_C;
   }
#ifdef SYSCALL_DEBUG
   scdebug_ret(p, code, error, rval);
#endif
   userret(p, tf, sticks);
#ifdef KTRACE
   if (KTRPOINT(p, KTR_SYSRET))
      ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/* Instruction pointers opperate differently on mc88110 */
m197_syscall(register_t code, struct m88100_saved_state *tf)
{
   register int i, nsys, *ap, nap;
   register struct sysent *callp;
   register struct proc *p;
   int error, new;
   struct args {
      int i[8];
   } args;
   int rval[2];
   u_quad_t sticks;
   extern struct pcb *curpcb;

   cnt.v_syscall++;

   p = curproc;

   callp = p->p_emul->e_sysent;
   nsys  = p->p_emul->e_nsysent;

#ifdef DIAGNOSTIC
   if (USERMODE(tf->epsr) == 0)
      panic("syscall");
   if (curpcb != &p->p_addr->u_pcb)
      panic("syscall curpcb/ppcb");
   if (tf != (struct trapframe *)&curpcb->user_state)
      panic("syscall trapframe");
#endif

   sticks = p->p_sticks;
   p->p_md.md_tf = tf;

   /*
    * For 88k, all the arguments are passed in the registers (r2-r12)
    * For syscall (and __syscall), r2 (and r3) has the actual code.
    * __syscall  takes a quad syscall number, so that other
    * arguments are at their natural alignments.
    */
   ap = &tf->r[2];
   nap = 6;

   switch (code) {
      case SYS_syscall:
         code = *ap++;
         nap--;
         break;
      case SYS___syscall:
         if (callp != sysent)
            break;
         code = ap[_QUAD_LOWWORD];
         ap += 2;
         nap -= 2;
         break;
   }

   /* Callp currently points to syscall, which returns ENOSYS. */

   if (code < 0 || code >= nsys)
      callp += p->p_emul->e_nosys;
   else {
      callp += code;
      i = callp->sy_argsize / sizeof(register_t);
      if (i > 8)
         panic("syscall nargs");
      /*
       * just copy them; syscall stub made sure all the
       * args are moved from user stack to registers.
       */
      bcopy((caddr_t)ap, (caddr_t)args.i, i * sizeof(register_t));
   }
#ifdef SYSCALL_DEBUG
   scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
   if (KTRPOINT(p, KTR_SYSCALL))
      ktrsyscall(p->p_tracep, code, callp->sy_argsize, args.i);
#endif
   rval[0] = 0;
   rval[1] = 0;
   error = (*callp->sy_call)(p, &args, rval);
   /*
    * system call will look like:
    *	 ld r10, r31, 32; r10,r11,r12 might be garbage.
    *	 ld r11, r31, 36
    *	 ld r12, r31, 40
    *	 or r13, r0, <code>
    *       tb0 0, r0, <128> <- sxip
    *	 br err 	  <- snip
    *       jmp r1
    *  err: or.u r3, r0, hi16(errno)
    *	 st r2, r3, lo16(errno)
    *	 subu r2, r0, 1 
    *	 jmp r1
    *
    * So, when we take syscall trap, sxip/snip will be as
    * shown above.
    * Given this,
    * 1. If the system call returned 0, need to jmp r1.
    *	   sxip += 8
    * 2. If the system call returned an errno > 0, increment 
    *    sxip += 4 and plug the value in r2. This will have us
    *    executing "br err" on return to user space.
    * 3. If the system call code returned ERESTART,
    *    we need to rexecute the trap instruction. leave xip as is.
    * 4. If the system call returned EJUSTRETURN, just return.
    *    sxip += 8
    */

   if (error == 0) {
      /*
       * If fork succeeded and we are the child, our stack
       * has moved and the pointer tf is no longer valid,
       * and p is wrong.  Compute the new trapframe pointer.
       * (The trap frame invariably resides at the
       * tippity-top of the u. area.)
       */
      p = curproc;
      tf = USER_REGS(p);
      tf->r[2] = rval[0];
      tf->r[3] = rval[1];
      tf->epsr &= ~PSR_C;
      tf->sxip += 8;
      tf->sxip &= ~3;
   } else if (error > 0) {
      /* error != ERESTART && error != EJUSTRETURN*/
      tf->r[2] = error;
      tf->epsr |= PSR_C;   /* fail */
      tf->sxip += 4;
      tf->sxip &= ~3;
   } else if (error == ERESTART) {
      /*
       * If (error == ERESTART), back up the pipe line. This
       * will end up reexecuting the trap.
       */
      tf->epsr &= ~PSR_C;
   } else {
      /* if (error == EJUSTRETURN) */
      tf->epsr &= ~PSR_C;
      tf->sxip += 8; 
      tf->sxip &= ~3;
   }
#ifdef SYSCALL_DEBUG
   scdebug_ret(p, code, error, rval);
#endif
   userret(p, tf, sticks);
#ifdef KTRACE
   if (KTRPOINT(p, KTR_SYSRET))
      ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/*
 * Set up return-value registers as fork() libc stub expects,
 * and do normal return-to-user-mode stuff.
 */
void
child_return(struct proc *p)
{
   struct trapframe *tf;

   tf = USER_REGS(p);
   tf->r[2] = 0;
   tf->r[3] = 0;
   tf->epsr &= ~PSR_C;
   if (cputyp != CPU_197) {
      tf->snip = tf->sfip & ~3;
      tf->sfip = tf->snip + 4;
   } else {
      tf->sxip += 8;
      tf->sxip &= ~3;
   }

   userret(p, tf, p->p_sticks);
#ifdef KTRACE
   if (KTRPOINT(p, KTR_SYSRET))
      ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}

/*
 * Allocation routines for software interrupts.
 */
u_long
allocate_sir(proc, arg)
void (*proc)();
void *arg;
{
   int bit;

   if (next_sir >= NSIR)
      panic("allocate_sir: none left");
   bit = next_sir++;
   sir_routines[bit] = proc;
   sir_args[bit] = arg;
   return (1 << bit);
}

void
init_sir()
{
   extern void netintr();

   sir_routines[0] = netintr;
   sir_routines[1] = softclock;
   next_sir = 2;
}


/************************************\
* User Single Step Debugging Support *
\************************************/

unsigned 
ss_get_value(struct proc *p, unsigned addr, int size)
{
   struct uio uio;
   struct iovec iov;
   unsigned value;

   iov.iov_base = (caddr_t)&value;
   iov.iov_len = size; 
   uio.uio_iov = &iov;
   uio.uio_iovcnt = 1; 
   uio.uio_offset = (off_t)addr;
   uio.uio_resid = size;
   uio.uio_segflg = UIO_SYSSPACE;
   uio.uio_rw = UIO_READ;
   uio.uio_procp = curproc;
   procfs_domem(curproc, p, NULL, &uio);
   return value;
}

int 
ss_put_value(struct proc *p, unsigned addr, unsigned value, int size)
{
   struct uio uio;
   struct iovec iov;
   int i;

   iov.iov_base = (caddr_t)&value;
   iov.iov_len = size; 
   uio.uio_iov = &iov;
   uio.uio_iovcnt = 1; 
   uio.uio_offset = (off_t)addr;
   uio.uio_resid = size;
   uio.uio_segflg = UIO_SYSSPACE;
   uio.uio_rw = UIO_WRITE;
   uio.uio_procp = curproc;
   i = procfs_domem(curproc, p, NULL, &uio);
   return i;
}

/*
 * ss_branch_taken(instruction, program counter, func, func_data)
 *
 * instruction will be a control flow instruction location at address pc.
 * Branch taken is supposed to return the address to which the instruction
 * would jump if the branch is taken. Func can be used to get the current
 * register values when invoked with a register number and func_data as
 * arguments.
 *
 * If the instruction is not a control flow instruction, panic.
 */
unsigned
ss_branch_taken(
               unsigned inst,
               unsigned pc,
               unsigned (*func)(unsigned int, struct trapframe *),
               struct trapframe *func_data)  /* 'opaque' */
{

   /* check if br/bsr */
   if ((inst & 0xf0000000U) == 0xc0000000U) {
      /* signed 26 bit pc relative displacement, shift left two bits */
      inst = (inst & 0x03ffffffU)<<2;
      /* check if sign extension is needed */
      if (inst & 0x08000000U)
         inst |= 0xf0000000U;
      return pc + inst;
   }

   /* check if bb0/bb1/bcnd case */
   switch ((inst & 0xf8000000U)) {
      case 0xd0000000U: /* bb0 */
      case 0xd8000000U: /* bb1 */
      case 0xe8000000U: /* bcnd */
         /* signed 16 bit pc relative displacement, shift left two bits */
         inst = (inst & 0x0000ffffU)<<2;
         /* check if sign extension is needed */
         if (inst & 0x00020000U)
            inst |= 0xfffc0000U;
         return pc + inst;
   }

   /* check jmp/jsr case */
   /* check bits 5-31, skipping 10 & 11 */
   if ((inst & 0xfffff3e0U) == 0xf400c000U)
      return (*func)(inst & 0x1f, func_data);  /* the register value */

   return 0; /* keeps compiler happy */
}

/*
 * ss_getreg_val - handed a register number and an exception frame.
 *              Returns the value of the register in the specified
 *              frame. Only makes sense for general registers.
 */
unsigned
ss_getreg_val(unsigned regno, struct trapframe *tf)
{
   if (regno == 0)
      return 0;
   else if (regno < 31)
      return tf->r[regno];
   else {
      panic("bad register number to ss_getreg_val.");
      return 0;/*to make compiler happy */
   }
}

boolean_t
ss_inst_branch(unsigned ins)
{
  /* check high five bits */
 
  switch (ins >> (32-5))
    {
    case 0x18: /* br */
    case 0x1a: /* bb0 */
    case 0x1b: /* bb1 */
    case 0x1d: /* bcnd */
      return TRUE;
      break;
    case 0x1e: /* could be jmp */
      if ((ins & 0xfffffbe0U) == 0xf400c000U)
	return TRUE;
    }

  return FALSE;
}


/* ss_inst_delayed - this instruction is followed by a delay slot. Could be
   br.n, bsr.n bb0.n, bb1.n, bcnd.n or jmp.n or jsr.n */

boolean_t
ss_inst_delayed(unsigned ins)
{
  /* check the br, bsr, bb0, bb1, bcnd cases */
  switch ((ins & 0xfc000000U)>>(32-6))
    {
    case 0x31: /* br */
    case 0x33: /* bsr */
    case 0x35: /* bb0 */
    case 0x37: /* bb1 */
    case 0x3b: /* bcnd */
      return TRUE;
    }

 /* check the jmp, jsr cases */
 /* mask out bits 0-4, bit 11 */
  return ((ins & 0xfffff7e0U) == 0xf400c400U) ? TRUE : FALSE;
}

unsigned
ss_next_instr_address(struct proc *p, unsigned pc, unsigned delay_slot)
{
    if (delay_slot == 0)
	return pc + 4;
    else
    {
	if (ss_inst_delayed(ss_get_value(p, pc, sizeof(int))))
	   return pc + 4;
	else
	   return pc;
    }
}

int
cpu_singlestep(p)
register struct proc *p;
{
   register unsigned va;
   struct trapframe *sstf = USER_REGS(p); /*p->p_md.md_tf;*/
   unsigned pc, brpc;
   int i;
   int bpinstr = SSBREAKPOINT;
   unsigned curinstr;
   unsigned inst;
   struct uio uio;
   struct iovec iov;

   pc = PC_REGS(sstf);
   /*
    * User was stopped at pc, e.g. the instruction
    * at pc was not executed.
    * Fetch what's at the current location.
    */
   curinstr = ss_get_value(p, pc, sizeof(int));

   /* compute next address after current location */
   if (curinstr != 0) {
   	if (ss_inst_branch(curinstr) || inst_call(curinstr) || inst_return(curinstr)) {
   	    brpc = ss_branch_taken(curinstr, pc, ss_getreg_val, sstf);
   	    if (brpc != pc) {	/* self-branches are hopeless */
#if 0
             printf("SS %s (%d): next taken breakpoint set at %x\n", 
                    p->p_comm, p->p_pid, brpc);
#endif 
   		     p->p_md.md_ss_taken_addr = brpc;
   		     p->p_md.md_ss_taken_instr = ss_get_value(p, brpc, sizeof(int));
              /* Store breakpoint instruction at the "next" location now. */
              i = ss_put_value(p, brpc, bpinstr, sizeof(int));
              if (i < 0) return (EFAULT);
   	    }
   	}
   	pc = ss_next_instr_address(p, pc, 0);
#if 0
      printf("SS %s (%d): next breakpoint set at %x\n", 
             p->p_comm, p->p_pid, pc);
#endif 
   } else {
      pc = PC_REGS(sstf) + 4;
#if 0
      printf("SS %s (%d): next breakpoint set at %x\n", 
             p->p_comm, p->p_pid, pc);
#endif 
   }
   
   if (p->p_md.md_ss_addr) {
      printf("SS %s (%d): breakpoint already set at %x (va %x)\n",
             p->p_comm, p->p_pid, p->p_md.md_ss_addr, pc); /* XXX */
      return (EFAULT);
   }

   p->p_md.md_ss_addr = pc;

   /* Fetch what's at the "next" location. */
   p->p_md.md_ss_instr = ss_get_value(p, pc, sizeof(int));

   /* Store breakpoint instruction at the "next" location now. */
   i = ss_put_value(p, pc, bpinstr, sizeof(int));

   if (i < 0) return (EFAULT);
   return (0);
}

