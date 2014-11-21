/*	$OpenBSD: cpu.c,v 1.68 2014/11/21 07:02:11 mlarkin Exp $	*/
/* $NetBSD: cpu.c,v 1.1 2003/04/26 18:39:26 fvdl Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lapic.h"
#include "ioapic.h"

#include <sys/param.h>
#include <sys/timeout.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <dev/rndvar.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/mpbiosvar.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/gdt.h>
#include <machine/pio.h>

#if NLAPIC > 0
#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#include <dev/ic/mc146818reg.h>
#include <amd64/isa/nvram.h>
#include <dev/isa/isareg.h>

#ifdef HIBERNATE
#include <sys/hibernate.h>
#include <machine/hibernate.h>
#endif /* HIBERNATE */

int     cpu_match(struct device *, void *, void *);
void    cpu_attach(struct device *, struct device *, void *);
void	patinit(struct cpu_info *ci);

struct cpu_softc {
	struct device sc_dev;		/* device tree glue */
	struct cpu_info *sc_info;	/* pointer to CPU info */
};

#ifndef SMALL_KERNEL
void	replacesmap(void);

extern long _copyout_stac;
extern long _copyout_clac;
extern long _copyin_stac;
extern long _copyin_clac;
extern long _copy_fault_clac;
extern long _copyoutstr_stac;
extern long _copyinstr_stac;
extern long _copystr_fault_clac;
extern long _stac;
extern long _clac;

static const struct {
	void *daddr;
	void *saddr;
} ireplace[] = {
	{ &_copyout_stac, &_stac },
	{ &_copyout_clac, &_clac },
	{ &_copyin_stac, &_stac },
	{ &_copyin_clac, &_clac },
	{ &_copy_fault_clac, &_clac },
	{ &_copyoutstr_stac, &_stac },
	{ &_copyinstr_stac, &_stac },
	{ &_copystr_fault_clac, &_clac },
};

void
replacesmap(void)
{
	static int replacedone = 0;
	int i, s;
	vaddr_t nva;

	if (replacedone)
		return;
	replacedone = 1;

	s = splhigh();
	/*
	 * Create writeable aliases of memory we need
	 * to write to as kernel is mapped read-only
	 */
	nva = (vaddr_t)km_alloc(2 * PAGE_SIZE, &kv_any, &kp_none, &kd_waitok);

	for (i = 0; i < nitems(ireplace); i++) {
		paddr_t kva = trunc_page((paddr_t)ireplace[i].daddr);
		paddr_t po = (paddr_t)ireplace[i].daddr & PAGE_MASK;
		paddr_t pa1, pa2;

		pmap_extract(pmap_kernel(), kva, &pa1);
		pmap_extract(pmap_kernel(), kva + PAGE_SIZE, &pa2);
		pmap_kenter_pa(nva, pa1, PROT_READ | PROT_WRITE);
		pmap_kenter_pa(nva + PAGE_SIZE, pa2, PROT_READ | PROT_WRITE);
		pmap_update(pmap_kernel());

		/* replace 3 byte nops with stac/clac instructions */
		bcopy(ireplace[i].saddr, (void *)(nva + po), 3);
	}

	km_free((void *)nva, 2 * PAGE_SIZE, &kv_any, &kp_none);
	
	splx(s);
}
#endif /* !SMALL_KERNEL */

#ifdef MULTIPROCESSOR
int mp_cpu_start(struct cpu_info *);
void mp_cpu_start_cleanup(struct cpu_info *);
struct cpu_functions mp_cpu_funcs = { mp_cpu_start, NULL,
				      mp_cpu_start_cleanup };
#endif /* MULTIPROCESSOR */

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

/*
 * Statically-allocated CPU info for the primary CPU (or the only
 * CPU, on uniprocessors).  The CPU info list is initialized to
 * point at it.
 */
struct cpu_info cpu_info_primary = { 0, &cpu_info_primary };

struct cpu_info *cpu_info_list = &cpu_info_primary;

#ifdef MULTIPROCESSOR
/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */
struct cpu_info *cpu_info[MAXCPUS] = { &cpu_info_primary };

void    	cpu_hatch(void *);
void    	cpu_boot_secondary(struct cpu_info *ci);
void    	cpu_start_secondary(struct cpu_info *ci);
void		cpu_copy_trampoline(void);

/*
 * Runs once per boot once multiprocessor goo has been detected and
 * the local APIC on the boot processor has been mapped.
 *
 * Called from lapic_boot_init() (from mpbios_scan()).
 */
void
cpu_init_first(void)
{
	cpu_copy_trampoline();
}
#endif

int
cpu_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct cpu_attach_args *caa = aux;

	if (strcmp(caa->caa_name, cf->cf_driver->cd_name) != 0)
		return 0;

	if (cf->cf_unit >= MAXCPUS)
		return 0;

	return 1;
}

static void
cpu_vm_init(struct cpu_info *ci)
{
	int ncolors = 2, i;

	for (i = CAI_ICACHE; i <= CAI_L2CACHE; i++) {
		struct x86_cache_info *cai;
		int tcolors;

		cai = &ci->ci_cinfo[i];

		tcolors = atop(cai->cai_totalsize);
		switch(cai->cai_associativity) {
		case 0xff:
			tcolors = 1; /* fully associative */
			break;
		case 0:
		case 1:
			break;
		default:
			tcolors /= cai->cai_associativity;
		}
		ncolors = max(ncolors, tcolors);
	}

#ifdef notyet
	/*
	 * Knowing the size of the largest cache on this CPU, re-color
	 * our pages.
	 */
	if (ncolors <= uvmexp.ncolors)
		return;
	printf("%s: %d page colors\n", ci->ci_dev->dv_xname, ncolors);
	uvm_page_recolor(ncolors);
#endif
}

#if defined(MULTIPROCESSOR)
void	cpu_idle_mwait_cycle(void);
void	cpu_init_mwait(struct cpu_softc *);
void	cpu_enable_mwait(void);

void
cpu_idle_mwait_cycle(void)
{
	struct cpu_info *ci = curcpu();
	volatile int *state = &ci->ci_mwait[0];

	if ((read_rflags() & PSL_I) == 0)
		panic("idle with interrupts blocked!");

	/* something already queued? */
	if (ci->ci_schedstate.spc_whichqs != 0)
		return;

	/*
	 * About to idle; setting the MWAIT_IN_IDLE bit tells
	 * cpu_unidle() that it can't be a no-op and tells cpu_kick()
	 * that it doesn't need to use an IPI.  We also set the
	 * MWAIT_KEEP_IDLING bit: those routines clear it to stop
	 * the mwait.  Once they're set, we do a final check of the
	 * queue, in case another cpu called setrunqueue() and added
	 * something to the queue and called cpu_unidle() between
	 * the check in sched_idle() and here.
	 */
	atomic_setbits_int(state, MWAIT_IDLING);
	if (ci->ci_schedstate.spc_whichqs == 0) {
		monitor(state, 0, 0);
		if ((*state & MWAIT_IDLING) == MWAIT_IDLING)
			mwait(0, 0);
	}

	/* done idling; let cpu_kick() know that an IPI is required */
	atomic_clearbits_int(state, MWAIT_IDLING);
}

unsigned int mwait_size;

void
cpu_init_mwait(struct cpu_softc *sc)
{
	unsigned int smallest, largest, extensions, c_substates;

	if ((cpu_ecxfeature & CPUIDECX_MWAIT) == 0)
		return;

	return;

	/* get the monitor granularity */
	CPUID(0x5, smallest, largest, extensions, c_substates);
	smallest &= 0xffff;
	largest  &= 0xffff;

	printf("%s: mwait min=%u, max=%u", sc->sc_dev.dv_xname,
	    smallest, largest);
	if (extensions & 0x1) {
		printf(", C-substates=%u.%u.%u.%u.%u",
		    0xf & (c_substates),
		    0xf & (c_substates >> 4),
		    0xf & (c_substates >> 8),
		    0xf & (c_substates >> 12),
		    0xf & (c_substates >> 16));
		if (extensions & 0x2)
			printf(", IBE");
	}

	/* paranoia: check the values */
	if (smallest < sizeof(int) || largest < smallest ||
	    (largest & (sizeof(int)-1)))
		printf(" (bogus)");
	else
		mwait_size = largest;
	printf("\n");
}

void
cpu_enable_mwait(void)
{
	unsigned long area;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	if (mwait_size == 0)
		return;

	/*
	 * Allocate the area, with a bit extra so that we can align
	 * to a multiple of mwait_size
	 */
	area = (unsigned long)malloc((ncpus * mwait_size) + mwait_size
	    - sizeof(int), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (area == 0) {
		printf("cpu0: mwait failed\n");
	} else {
		/* round to a multiple of mwait_size  */
		area = ((area + mwait_size - sizeof(int)) / mwait_size)
		    * mwait_size;
		CPU_INFO_FOREACH(cii, ci) {
			ci->ci_mwait = (int *)area;
			area += mwait_size;
		}
		cpu_idle_cycle_fcn = &cpu_idle_mwait_cycle;
	}
}
#endif /* MULTIPROCESSOR */

void
cpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct cpu_softc *sc = (void *) self;
	struct cpu_attach_args *caa = aux;
	struct cpu_info *ci;
#if defined(MULTIPROCESSOR)
	int cpunum = sc->sc_dev.dv_unit;
	vaddr_t kstack;
	struct pcb *pcb;
#endif

	/*
	 * If we're an Application Processor, allocate a cpu_info
	 * structure, otherwise use the primary's.
	 */
	if (caa->cpu_role == CPU_ROLE_AP) {
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK|M_ZERO);
#if defined(MULTIPROCESSOR)
		if (cpu_info[cpunum] != NULL)
			panic("cpu at apic id %d already attached?", cpunum);
		cpu_info[cpunum] = ci;
#endif
#ifdef TRAPLOG
		ci->ci_tlog_base = malloc(sizeof(struct tlog),
		    M_DEVBUF, M_WAITOK);
#endif
	} else {
		ci = &cpu_info_primary;
#if defined(MULTIPROCESSOR)
		if (caa->cpu_number != lapic_cpu_number()) {
			panic("%s: running cpu is at apic %d"
			    " instead of at expected %d",
			    sc->sc_dev.dv_xname, lapic_cpu_number(), caa->cpu_number);
		}
#endif
	}

	ci->ci_self = ci;
	sc->sc_info = ci;

	ci->ci_dev = self;
	ci->ci_apicid = caa->cpu_number;
#ifdef MULTIPROCESSOR
	ci->ci_cpuid = cpunum;
#else
	ci->ci_cpuid = 0;	/* False for APs, but they're not used anyway */
#endif
	ci->ci_func = caa->cpu_func;

	simple_lock_init(&ci->ci_slock);

#if defined(MULTIPROCESSOR)
	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack.
	 */
	kstack = uvm_km_alloc (kernel_map, USPACE);
	if (kstack == 0) {
		if (caa->cpu_role != CPU_ROLE_AP) {
			panic("cpu_attach: unable to allocate idle stack for"
			    " primary");
		}
		printf("%s: unable to allocate idle stack\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	pcb = ci->ci_idle_pcb = (struct pcb *) kstack;
	memset(pcb, 0, USPACE);

	pcb->pcb_kstack = kstack + USPACE - 16;
	pcb->pcb_rbp = pcb->pcb_rsp = kstack + USPACE - 16;
	pcb->pcb_pmap = pmap_kernel();
	pcb->pcb_cr0 = rcr0();
	pcb->pcb_cr3 = pcb->pcb_pmap->pm_pdirpa;
#endif

	/* further PCB init done later. */

	printf(": ");

	switch (caa->cpu_role) {
	case CPU_ROLE_SP:
		printf("(uniprocessor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_SP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
#ifdef MTRR
		mem_range_attach();
#endif /* MTRR */
		cpu_init(ci);
		break;

	case CPU_ROLE_BP:
		printf("apid %d (boot processor)\n", caa->cpu_number);
		ci->ci_flags |= CPUF_PRESENT | CPUF_BSP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
#ifdef MTRR
		mem_range_attach();
#endif /* MTRR */
		cpu_init(ci);

#if NLAPIC > 0
		/*
		 * Enable local apic
		 */
		lapic_enable();
		lapic_calibrate_timer(ci);
#endif
#if NIOAPIC > 0
		ioapic_bsp_id = caa->cpu_number;
#endif
#if defined(MULTIPROCESSOR)
		cpu_init_mwait(sc);
#endif
		break;

	case CPU_ROLE_AP:
		/*
		 * report on an AP
		 */
		printf("apid %d (application processor)\n", caa->cpu_number);

#if defined(MULTIPROCESSOR)
		cpu_intr_init(ci);
		gdt_alloc_cpu(ci);
		sched_init_cpu(ci);
		cpu_start_secondary(ci);
		ncpus++;
		if (ci->ci_flags & CPUF_PRESENT) {
			ci->ci_next = cpu_info_list->ci_next;
			cpu_info_list->ci_next = ci;
		}
#else
		printf("%s: not started\n", sc->sc_dev.dv_xname);
#endif
		break;

	default:
		panic("unknown processor type??");
	}
	cpu_vm_init(ci);

#if defined(MULTIPROCESSOR)
	if (mp_verbose) {
		printf("%s: kstack at 0x%lx for %d bytes\n",
		    sc->sc_dev.dv_xname, kstack, USPACE);
		printf("%s: idle pcb at %p, idle sp at 0x%llx\n",
		    sc->sc_dev.dv_xname, pcb, pcb->pcb_rsp);
	}
#endif
}

/*
 * Initialize the processor appropriately.
 */

void
cpu_init(struct cpu_info *ci)
{
	/* configure the CPU if needed */
	if (ci->cpu_setup != NULL)
		(*ci->cpu_setup)(ci);
	/*
	 * We do this here after identifycpu() because errata may affect
	 * what we do.
	 */
	patinit(ci);

	lcr0(rcr0() | CR0_WP);
	lcr4(rcr4() | CR4_DEFAULT |
	    (ci->ci_feature_sefflags & SEFF0EBX_SMEP ? CR4_SMEP : 0));
#ifndef SMALL_KERNEL
	if (ci->ci_feature_sefflags & SEFF0EBX_SMAP)
		lcr4(rcr4() | CR4_SMAP);
#endif

#ifdef MULTIPROCESSOR
	ci->ci_flags |= CPUF_RUNNING;
	tlbflushg();
#endif
}


#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors(void)
{
	struct cpu_info *ci;
	u_long i;

	cpu_enable_mwait();

	for (i=0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		if (ci->ci_flags & (CPUF_BSP | CPUF_SP | CPUF_PRIMARY))
			continue;
		ci->ci_randseed = (arc4random() & 0x7fffffff) + 1;
		cpu_boot_secondary(ci);
	}
}

void
cpu_init_idle_pcbs(void)
{
	struct cpu_info *ci;
	u_long i;

	for (i=0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		x86_64_init_pcb_tss_ldt(ci);
	}
}

void
cpu_start_secondary(struct cpu_info *ci)
{
	int i;

	ci->ci_flags |= CPUF_AP;

	CPU_STARTUP(ci);

	/*
	 * wait for it to become ready
	 */
	for (i = 100000; (!(ci->ci_flags & CPUF_PRESENT)) && i>0;i--) {
		delay(10);
	}
	if (! (ci->ci_flags & CPUF_PRESENT)) {
		printf("%s: failed to become ready\n", ci->ci_dev->dv_xname);
#if defined(MPDEBUG) && defined(DDB)
		printf("dropping into debugger; continue from here to resume boot\n");
		Debugger();
#endif
	}

	if ((ci->ci_flags & CPUF_IDENTIFIED) == 0) {
		atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);

		/* wait for it to identify */
		for (i = 100000; (ci->ci_flags & CPUF_IDENTIFY) && i > 0; i--)
			delay(10);

		if (ci->ci_flags & CPUF_IDENTIFY)
			printf("%s: failed to identify\n",
			    ci->ci_dev->dv_xname);
	}

	CPU_START_CLEANUP(ci);
}

void
cpu_boot_secondary(struct cpu_info *ci)
{
	int i;

	atomic_setbits_int(&ci->ci_flags, CPUF_GO);

	for (i = 100000; (!(ci->ci_flags & CPUF_RUNNING)) && i>0;i--) {
		delay(10);
	}
	if (! (ci->ci_flags & CPUF_RUNNING)) {
		printf("cpu failed to start\n");
#if defined(MPDEBUG) && defined(DDB)
		printf("dropping into debugger; continue from here to resume boot\n");
		Debugger();
#endif
	}
}

/*
 * The CPU ends up here when it's ready to run
 * This is called from code in mptramp.s; at this point, we are running
 * in the idle pcb/idle stack of the new cpu.  When this function returns,
 * this processor will enter the idle loop and start looking for work.
 *
 * XXX should share some of this with init386 in machdep.c
 */
void
cpu_hatch(void *v)
{
	struct cpu_info *ci = (struct cpu_info *)v;
	int s;

	cpu_init_msrs(ci);

#ifdef DEBUG
	if (ci->ci_flags & CPUF_PRESENT)
		panic("%s: already running!?", ci->ci_dev->dv_xname);
#endif

	ci->ci_flags |= CPUF_PRESENT;

	lapic_enable();
	lapic_startclock();

	if ((ci->ci_flags & CPUF_IDENTIFIED) == 0) {
		/*
		 * We need to wait until we can identify, otherwise dmesg
		 * output will be messy.
		 */
		while ((ci->ci_flags & CPUF_IDENTIFY) == 0)
			delay(10);

		identifycpu(ci);

		/* Signal we're done */
		atomic_clearbits_int(&ci->ci_flags, CPUF_IDENTIFY);
		/* Prevent identifycpu() from running again */
		atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFIED);
	}

	while ((ci->ci_flags & CPUF_GO) == 0)
		delay(10);
#ifdef HIBERNATE
	if ((ci->ci_flags & CPUF_PARK) != 0) {
		atomic_clearbits_int(&ci->ci_flags, CPUF_PARK);
		hibernate_drop_to_real_mode();
	}
#endif /* HIBERNATE */

#ifdef DEBUG
	if (ci->ci_flags & CPUF_RUNNING)
		panic("%s: already running!?", ci->ci_dev->dv_xname);
#endif

	lcr0(ci->ci_idle_pcb->pcb_cr0);
	cpu_init_idt();
	lapic_set_lvt();
	gdt_init_cpu(ci);
	fpuinit(ci);

	lldt(0);

	cpu_init(ci);

	/* Re-initialise memory range handling on AP */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->initAP(&mem_range_softc);

	s = splhigh();
	lcr8(0);
	enable_intr();

	nanouptime(&ci->ci_schedstate.spc_runtime);
	splx(s);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

#if defined(DDB)

#include <ddb/db_output.h>
#include <machine/db_machdep.h>

/*
 * Dump cpu information from ddb.
 */
void
cpu_debug_dump(void)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	db_printf("addr		dev	id	flags	ipis	curproc		fpcurproc\n");
	CPU_INFO_FOREACH(cii, ci) {
		db_printf("%p	%s	%u	%x	%x	%10p	%10p\n",
		    ci,
		    ci->ci_dev == NULL ? "BOOT" : ci->ci_dev->dv_xname,
		    ci->ci_cpuid,
		    ci->ci_flags, ci->ci_ipis,
		    ci->ci_curproc,
		    ci->ci_fpcurproc);
	}
}
#endif

void
cpu_copy_trampoline(void)
{
	/*
	 * Copy boot code.
	 */
	extern u_char cpu_spinup_trampoline[];
	extern u_char cpu_spinup_trampoline_end[];

	extern u_int32_t mp_pdirpa;
	extern paddr_t tramp_pdirpa;

	memcpy((caddr_t)MP_TRAMPOLINE,
	    cpu_spinup_trampoline,
	    cpu_spinup_trampoline_end-cpu_spinup_trampoline);

	/*
	 * We need to patch this after we copy the trampoline,
	 * the symbol points into the copied trampoline.
	 */
	mp_pdirpa = tramp_pdirpa;
}


int
mp_cpu_start(struct cpu_info *ci)
{
#if NLAPIC > 0
	int error;
#endif
	unsigned short dwordptr[2];

	/*
	 * "The BSP must initialize CMOS shutdown code to 0Ah ..."
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_JUMP);

	/*
	 * "and the warm reset vector (DWORD based at 40:67) to point
	 * to the AP startup code ..."
	 */

	dwordptr[0] = 0;
	dwordptr[1] = MP_TRAMPOLINE >> 4;

	pmap_kenter_pa(0, 0, PROT_READ | PROT_WRITE);
	memcpy((u_int8_t *) 0x467, dwordptr, 4);
	pmap_kremove(0, PAGE_SIZE);

#if NLAPIC > 0
	/*
	 * ... prior to executing the following sequence:"
	 */

	if (ci->ci_flags & CPUF_AP) {
		if ((error = x86_ipi_init(ci->ci_apicid)) != 0)
			return error;

		delay(10000);

		if (cpu_feature & CPUID_APIC) {
			if ((error = x86_ipi(MP_TRAMPOLINE/PAGE_SIZE,
					     ci->ci_apicid,
					     LAPIC_DLMODE_STARTUP)) != 0)
				return error;
			delay(200);

			if ((error = x86_ipi(MP_TRAMPOLINE/PAGE_SIZE,
					     ci->ci_apicid,
					     LAPIC_DLMODE_STARTUP)) != 0)
				return error;
			delay(200);
		}
	}
#endif
	return 0;
}

void
mp_cpu_start_cleanup(struct cpu_info *ci)
{
	/*
	 * Ensure the NVRAM reset byte contains something vaguely sane.
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_RST);
}
#endif	/* MULTIPROCESSOR */

typedef void (vector)(void);
extern vector Xsyscall, Xsyscall32;

void
cpu_init_msrs(struct cpu_info *ci)
{
	wrmsr(MSR_STAR,
	    ((uint64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	    ((uint64_t)GSEL(GUCODE32_SEL, SEL_UPL) << 48));
	wrmsr(MSR_LSTAR, (uint64_t)Xsyscall);
	wrmsr(MSR_CSTAR, (uint64_t)Xsyscall32);
	wrmsr(MSR_SFMASK, PSL_NT|PSL_T|PSL_I|PSL_C);

	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_GSBASE, (u_int64_t)ci);
	wrmsr(MSR_KERNELGSBASE, 0);
}

void
patinit(struct cpu_info *ci)
{
	extern int	pmap_pg_wc;
	u_int64_t	reg;

	if ((ci->ci_feature_flags & CPUID_PAT) == 0)
		return;
#define	PATENTRY(n, type)	(type << ((n) * 8))
#define	PAT_UC		0x0UL
#define	PAT_WC		0x1UL
#define	PAT_WT		0x4UL
#define	PAT_WP		0x5UL
#define	PAT_WB		0x6UL
#define	PAT_UCMINUS	0x7UL
	/* 
	 * Set up PAT bits.
	 * The default pat table is the following:
	 * WB, WT, UC- UC, WB, WT, UC-, UC
	 * We change it to:
	 * WB, WC, UC-, UC, WB, WC, UC-, UC.
	 * i.e change the WT bit to be WC.
	 */
	reg = PATENTRY(0, PAT_WB) | PATENTRY(1, PAT_WC) |
	    PATENTRY(2, PAT_UCMINUS) | PATENTRY(3, PAT_UC) |
	    PATENTRY(4, PAT_WB) | PATENTRY(5, PAT_WC) |
	    PATENTRY(6, PAT_UCMINUS) | PATENTRY(7, PAT_UC);

	wrmsr(MSR_CR_PAT, reg);
	pmap_pg_wc = PG_WC;
}

struct timeout rdrand_tmo;
void rdrand(void *);

void
rdrand(void *v)
{
	struct timeout *tmo = v;
	union {
		uint64_t u64;
		uint32_t u32[2];
	} r;
	uint64_t valid;
	int i;

	for (i = 0; i < 2; i++) {
		__asm volatile(
		    "xor	%1, %1\n\t"
		    "rdrand	%0\n\t"
		    "rcl	$1, %1\n"
		    : "=r" (r.u64), "=r" (valid) );

		if (valid) {
			add_true_randomness(r.u32[0]);
			add_true_randomness(r.u32[1]);
		}
	}

	timeout_add_msec(tmo, 10);
}
