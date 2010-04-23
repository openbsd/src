/* $OpenBSD: kvm86.c,v 1.4 2010/04/23 21:34:40 deraadt Exp $ */
/* $NetBSD: kvm86.c,v 1.10 2005/12/26 19:23:59 perry Exp $ */
/*
 * Copyright (c) 2002
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/lock.h>
#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/kvm86.h>
#include <machine/cpu.h>

/* assembler functions in kvm86call.s */
extern int kvm86_call(struct trapframe *);
extern void kvm86_ret(struct trapframe *, int);

#define PGTABLE_SIZE	((1024 + 64) * 1024 / PAGE_SIZE)

struct kvm86_data {
	pt_entry_t pgtbl[PGTABLE_SIZE];

	struct segment_descriptor sd;

	struct pcb pcb;
	u_long iomap[0x10000/32];
};

void kvm86_map(struct kvm86_data *, paddr_t, uint32_t);
void kvm86_mapbios(struct kvm86_data *);
void kvm86_prepare(struct kvm86_data *vmd);
/*
 * global VM for BIOS calls
 */
struct kvm86_data *bioscallvmd;
/* page for trampoline and stack */
void *bioscallscratchpage;
/* where this page is mapped in the vm86 */
#define BIOSCALLSCRATCHPAGE_VMVA 0x1000
/* a virtual page to map in vm86 memory temporarily */
vaddr_t bioscalltmpva;

struct mutex kvm86_mp_mutex;

#define KVM86_IOPL3 /* not strictly necessary, saves a lot of traps */

void
kvm86_init()
{
	size_t vmdsize;
	char *buf;
	struct kvm86_data *vmd;
	struct pcb *pcb;
	paddr_t pa;
	int i;

	vmdsize = round_page(sizeof(struct kvm86_data)) + PAGE_SIZE;

	if ((buf = (char *)uvm_km_zalloc(kernel_map, vmdsize)) == NULL)
		return;
	
	/* first page is stack */
	vmd = (struct kvm86_data *)(buf + PAGE_SIZE);
	pcb = &vmd->pcb;

	/*
	 * derive pcb and TSS from proc0
	 * we want to access all IO ports, so we need a full-size
	 *  permission bitmap
	 * XXX do we really need the pcb or just the TSS?
	 */
	memcpy(pcb, &proc0.p_addr->u_pcb, sizeof(struct pcb));
	pcb->pcb_tss.tss_esp0 = (int)vmd;
	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	for (i = 0; i < sizeof(vmd->iomap) / 4; i++)
		vmd->iomap[i] = 0;
	pcb->pcb_tss.tss_ioopt =
	    ((caddr_t)vmd->iomap - (caddr_t)&pcb->pcb_tss) << 16;

	/* setup TSS descriptor (including our iomap) */
	setsegment(&vmd->sd, &pcb->pcb_tss,
	    sizeof(struct pcb) + sizeof(vmd->iomap) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);

	/* prepare VM for BIOS calls */
	kvm86_mapbios(vmd);
	if ((bioscallscratchpage = (void *)uvm_km_alloc(kernel_map, PAGE_SIZE))
	    == 0)
		return;

	pmap_extract(pmap_kernel(), (vaddr_t)bioscallscratchpage, &pa);
	kvm86_map(vmd, pa, BIOSCALLSCRATCHPAGE_VMVA);
	bioscallvmd = vmd;
	bioscalltmpva = uvm_km_alloc(kernel_map, PAGE_SIZE);
	mtx_init(&kvm86_mp_mutex, IPL_IPI);
}

/*
 * XXX pass some stuff to the assembler code
 * XXX this should be done cleanly (in call argument to kvm86_call())
 */

volatile struct pcb *vm86pcb;
volatile int vm86tssd0, vm86tssd1;
volatile paddr_t vm86newptd;
volatile struct trapframe *vm86frame;
volatile pt_entry_t *vm86pgtableva;

void
kvm86_prepare(struct kvm86_data *vmd)
{
	vm86newptd = vtophys((vaddr_t)vmd) | PG_V | PG_RW | PG_U | PG_u;
	vm86pgtableva = vmd->pgtbl;
	vm86frame = (struct trapframe *)vmd - 1;
	vm86pcb = &vmd->pcb;
	vm86tssd0 = *(int*)&vmd->sd;
	vm86tssd1 = *((int*)&vmd->sd + 1);
}

void
kvm86_map(struct kvm86_data *vmd, paddr_t pa, uint32_t vmva)
{

	vmd->pgtbl[vmva >> 12] = pa | PG_V | PG_RW | PG_U | PG_u;
}

void
kvm86_mapbios(struct kvm86_data *vmd)
{
	paddr_t pa;

	/* map first physical page (vector table, BIOS data) */
	kvm86_map(vmd, 0, 0);

	/* map ISA hole */
	for (pa = 0xa0000; pa < 0x100000; pa += PAGE_SIZE)
		kvm86_map(vmd, pa, pa);
}

void *
kvm86_bios_addpage(uint32_t vmva)
{
	void *mem;
	paddr_t pa;

	if (bioscallvmd->pgtbl[vmva >> 12]) /* allocated? */
		return (NULL);

	if ((mem = (void *)uvm_km_alloc(kernel_map, PAGE_SIZE)) == NULL)
		return (NULL);
	
	pmap_extract(pmap_kernel(), (vaddr_t)mem, &pa);	
	kvm86_map(bioscallvmd, pa, vmva);

	return (mem);
}

void
kvm86_bios_delpage(uint32_t vmva, void *kva)
{

	bioscallvmd->pgtbl[vmva >> 12] = 0;
	uvm_km_free(kernel_map, (vaddr_t)kva, PAGE_SIZE);
}

size_t
kvm86_bios_read(u_int32_t vmva, char *buf, size_t len)
{
	size_t todo, now;
	paddr_t vmpa;

	todo = len;
	while (todo > 0) {
		now = min(todo, PAGE_SIZE - (vmva & (PAGE_SIZE - 1)));

		if (!bioscallvmd->pgtbl[vmva >> 12])
			break;
		vmpa = bioscallvmd->pgtbl[vmva >> 12] & ~(PAGE_SIZE - 1);
		pmap_kenter_pa(bioscalltmpva, vmpa, VM_PROT_READ);
		pmap_update(pmap_kernel());

		memcpy(buf, (void *)(bioscalltmpva + (vmva & (PAGE_SIZE - 1))),
		       now);
		buf += now;
		todo -= now;
		vmva += now;
	}
	return (len - todo);
}

int
kvm86_bioscall(int intno, struct trapframe *tf)
{
	static const unsigned char call[] = {
		0xfa, /* CLI */
		0xcd, /* INTxx */
		0,
		0xfb, /* STI */
		0xf4  /* HLT */
	};

	memcpy(bioscallscratchpage, call, sizeof(call));
	*((unsigned char *)bioscallscratchpage + 2) = intno;

	tf->tf_eip = BIOSCALLSCRATCHPAGE_VMVA;
	tf->tf_cs = 0;
	tf->tf_esp = BIOSCALLSCRATCHPAGE_VMVA + PAGE_SIZE - 2;
	tf->tf_ss = 0;
	tf->tf_eflags = PSL_USERSET | PSL_VM;
#ifdef KVM86_IOPL3
	tf->tf_eflags |= PSL_IOPL;
#endif
	tf->tf_ds = tf->tf_es = tf->tf_fs = tf->tf_gs = 0;

	kvm86_prepare(bioscallvmd); /* XXX */
	return (kvm86_call(tf));
}

int
kvm86_simplecall(int no, struct kvm86regs *regs)
{
	struct trapframe tf;
	int res;
	
	memset(&tf, 0, sizeof(struct trapframe));
	tf.tf_eax = regs->eax;
	tf.tf_ebx = regs->ebx;
	tf.tf_ecx = regs->ecx;
	tf.tf_edx = regs->edx;
	tf.tf_esi = regs->esi;
	tf.tf_edi = regs->edi;
	tf.tf_vm86_es = regs->es;
	
	mtx_enter(&kvm86_mp_mutex);	
	res = kvm86_bioscall(no, &tf);
	mtx_leave(&kvm86_mp_mutex);

	regs->eax = tf.tf_eax;
	regs->ebx = tf.tf_ebx;
	regs->ecx = tf.tf_ecx;
	regs->edx = tf.tf_edx;
	regs->esi = tf.tf_esi;
	regs->edi = tf.tf_edi;
	regs->es = tf.tf_vm86_es;
	regs->eflags = tf.tf_eflags;
	
	return (res);
}

void
kvm86_gpfault(struct trapframe *tf)
{
	unsigned char *kva, insn, trapno;
	uint16_t *sp;

	kva = (unsigned char *)((tf->tf_cs << 4) + tf->tf_eip);
	insn = *kva;
#ifdef KVM86DEBUG
	printf("kvm86_gpfault: cs=%x, eip=%x, insn=%x, eflags=%x\n",
	       tf->tf_cs, tf->tf_eip, insn, tf->tf_eflags);
#endif

	KASSERT(tf->tf_eflags & PSL_VM);

	switch (insn) {
	case 0xf4: /* HLT - normal exit */
		kvm86_ret(tf, 0);
		break;
	case 0xcd: /* INTxx */
		/* fake a return stack frame and call real mode handler */
		trapno = *(kva + 1);
		sp = (uint16_t *)((tf->tf_ss << 4) + tf->tf_esp);
		*(--sp) = tf->tf_eflags;
		*(--sp) = tf->tf_cs;
		*(--sp) = tf->tf_eip + 2;
		tf->tf_esp -= 6;
		tf->tf_cs = *(uint16_t *)(trapno * 4 + 2);
		tf->tf_eip = *(uint16_t *)(trapno * 4);
		break;
	case 0xcf: /* IRET */
		sp = (uint16_t *)((tf->tf_ss << 4) + tf->tf_esp);
		tf->tf_eip = *(sp++);
		tf->tf_cs = *(sp++);
		tf->tf_eflags = *(sp++);
		tf->tf_esp += 6;
		tf->tf_eflags |= PSL_VM; /* outside of 16bit flag reg */
		break;
#ifndef KVM86_IOPL3 /* XXX check VME? */
	case 0xfa: /* CLI */
	case 0xfb: /* STI */
		/* XXX ignore for now */
		tf->tf_eip++;
		break;
	case 0x9c: /* PUSHF */
		sp = (uint16_t *)((tf->tf_ss << 4) + tf->tf_esp);
		*(--sp) = tf->tf_eflags;
		tf->tf_esp -= 2;
		tf->tf_eip++;
		break;
	case 0x9d: /* POPF */
		sp = (uint16_t *)((tf->tf_ss << 4) + tf->tf_esp);
		tf->tf_eflags = *(sp++);
		tf->tf_esp += 2;
		tf->tf_eip++;
		tf->tf_eflags |= PSL_VM; /* outside of 16bit flag reg */
		break;
#endif
	default:
#ifdef KVM86DEBUG
		printf("kvm86_gpfault: unhandled\n");
#else
		printf("kvm86_gpfault: cs=%x, eip=%x, insn=%x, eflags=%x\n",
		       tf->tf_cs, tf->tf_eip, insn, tf->tf_eflags);
#endif
		/*
		 * signal error to caller
		 */
		kvm86_ret(tf, -1);
		break;
	}
}
