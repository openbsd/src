/*	$OpenBSD: trap.c,v 1.2 1999/01/11 05:11:22 millert Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define INTRDEBUG

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/iomod.h>
#include <machine/cpufunc.h>
#include <machine/reg.h>
#include <machine/db_machdep.h>
#include <machine/autoconf.h>

#define	FAULT_TYPE(op)	(VM_PROT_READ|(inst_load(op) ? 0 : VM_PROT_WRITE))

const char *trap_type[] = {
	"invalid interrupt vector",
	"high priority machine check",
	"power failure",
	"recovery counter trap",
	"external interrupt",
	"low-priority machine check",
	"instruction TLB miss fault",
	"instruction protection trap",
	"Illegal instruction trap",
	"break instruction trap",
	"privileged operation trap",
	"privileged register trap",
	"overflow trap",
	"conditional trap",
	"assist exception trap",
	"data TLB miss fault",
	"ITLB non-access miss fault",
	"DTLB non-access miss fault",
	"data protection trap/unalligned data reference trap",
	"data break trap",
	"TLB dirty bit trap",
	"page reference trap",
	"assist emulation trap",
	"higher-privelege transfer trap",
	"lower-privilege transfer trap",
	"taken branch trap",
	"data access rights trap",
	"data protection ID trap",
	"unaligned data ref trap",
	"reserved",
	"reserved 2"
};
int trap_types = sizeof(trap_type)/sizeof(trap_type[0]);

u_int32_t sir;

void pmap_hptdump __P((void));
void cpu_intr __P((struct trapframe *));

void
trap(type, frame)
	int type;
	struct trapframe *frame;
{
	struct proc *p = curproc;
	register vm_offset_t va;
	register vm_map_t map;
	register pa_space_t space;
	u_int opcode, t;
	int ret;

	va = frame->ior;
	space = (pa_space_t) frame->isr;

	if (USERMODE(frame->iioq_head)) {
		type |= T_USER;
		p->p_md.md_regs = frame;
	}

	if ((type & ~T_USER) != T_INTERRUPT)
		printf("trap: %d, %s for %x:%x at %x:%x\n",
		       type, trap_type[type & ~T_USER], space, va,
		       frame->iisq_head, frame->iioq_head);

	switch (type) {
	case T_NONEXIST:
	case T_NONEXIST|T_USER:
		/* we are screwd up by the central scrutinizer */
		panic ("trap: zombie on the bridge!!!");
		break;

	case T_RECOVERY:
	case T_RECOVERY|T_USER:
		printf ("trap: handicapped");
		break;

	case T_INTERRUPT:
	case T_INTERRUPT|T_USER:
		mfctl(CR_EIRR, t);
		t &= frame->eiem;
		/* hardcode intvl timer intr, to save for proc switching */
		if (t & INT_ITMR) {
			/* ACK it now */
			mtctl(INT_ITMR, CR_EIRR);
			/* we've got an interval timer interrupt */
			cpu_initclocks();
			hardclock(frame);
		} else {
#ifdef INTRDEBUG
			printf ("cpu_intr: 0x%08x\n", t);
#endif
			cpu_intr(frame);
		}
		return;

	case T_HPMC:
	case T_POWERFAIL:
	case T_LPMC:
		break;

	case T_IBREAK:
	case T_DBREAK:
		if (kdb_trap (type, 0, frame))
			return;
		break;

	case T_DTLBMISS:
		va = trunc_page(va);
		opcode = frame->iir;

#ifdef DDB
		Debugger();
#endif
		ret = vm_fault(map, va, FAULT_TYPE(opcode), FALSE);
		if (ret == KERN_SUCCESS)
			break;
		panic("trap: vm_fault(%p, %x, %d, %d): %d",
		      map, va, FAULT_TYPE(opcode), 0, ret);
		break;
	default:
		/* pmap_hptdump(); */
#ifdef DDB
		Debugger();
#endif
	}
}

/* all the interrupts, minus cpu clock, which is the last */
struct cpu_intr_vector {
	const char *name;
	int pri;
	int (*handler) __P((void *));
	void *arg;
} cpu_intr_vectors[CPU_NINTS - 1];
#define	ECPU_INTR_VECTORS	&cpu_intr_vectors[CPU_NINTS - 1]

int
cpu_intr_establish(pri, handler, arg, name)
	int pri;
	int (*handler) __P((void *));
	void *arg;
	const char *name;
{
	register struct cpu_intr_vector *p;

	for (p = cpu_intr_vectors; p < ECPU_INTR_VECTORS; p++)
		if (!p->handler)
			break;

	/* no more vectors, fail */
	if (p >= ECPU_INTR_VECTORS)
		return 0;

	p->name = name;
	p->pri = pri;
	p->handler = handler;
	p->arg = arg;

	return p - cpu_intr_vectors;
}

void
cpu_intr(frame)
	struct trapframe *frame;
{
	register u_int32_t t;
	register struct cpu_intr_vector *p;
	register int bit;

	do {
		mfctl(CR_EIRR, t);
		t &= frame->eiem;
		bit = ffs(t) - 1;
		if (bit >= 0) {
			mtctl(1 << bit, CR_EIRR);
#ifdef INTRDEBUG
			printf ("cpu_intr: 0x%x\n", (1 << bit));
#endif
			p = &cpu_intr_vectors[bit];
			if (p->handler) {
				register int s = splx(p->pri);
				if (!(p->handler)(p->arg))
					printf ("%s: can't handle interrupt\n",
						p->name);
				splx(s);
			} else
				printf ("cpu_intr: stray interrupt %d\n", bit);
		}
	} while (bit >= 0);
}

