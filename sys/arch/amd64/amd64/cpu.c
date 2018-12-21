/*	$OpenBSD: cpu.c,v 1.131 2018/12/21 12:02:55 kettenis Exp $	*/
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
#include "vmm.h"
#include "pvbus.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/timeout.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <dev/rndvar.h>
#include <sys/atomic.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/codepatch.h>
#include <machine/cpu_full.h>
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
#include <machine/vmmvar.h>

#if NLAPIC > 0
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NPVBUS > 0
#include <dev/pv/pvvar.h>
#endif

#include <dev/ic/mc146818reg.h>
#include <amd64/isa/nvram.h>
#include <dev/isa/isareg.h>

#ifdef HIBERNATE
#include <sys/hibernate.h>
#include <machine/hibernate.h>
#endif /* HIBERNATE */

/* #define CPU_DEBUG */

#ifdef CPU_DEBUG
#define DPRINTF(x...)	do { printf(x); } while(0)
#else
#define DPRINTF(x...)
#endif /* CPU_DEBUG */

int     cpu_match(struct device *, void *, void *);
void    cpu_attach(struct device *, struct device *, void *);
int     cpu_activate(struct device *, int);
void	patinit(struct cpu_info *ci);
#if NVMM > 0
void	cpu_init_vmm(struct cpu_info *ci);
#endif /* NVMM > 0 */

struct cpu_softc {
	struct device sc_dev;		/* device tree glue */
	struct cpu_info *sc_info;	/* pointer to CPU info */
};

void	replacesmap(void);
void	replacemeltdown(void);

extern long _stac;
extern long _clac;

void
replacesmap(void)
{
	static int replacedone = 0;
	int s;

	if (replacedone)
		return;
	replacedone = 1;

	s = splhigh();

	codepatch_replace(CPTAG_STAC, &_stac, 3);
	codepatch_replace(CPTAG_CLAC, &_clac, 3);

	splx(s);
}

void
replacemeltdown(void)
{
	static int replacedone = 0;
	int s;

	if (replacedone)
		return;
	replacedone = 1;

	s = splhigh();
	if (!cpu_meltdown)
		codepatch_nop(CPTAG_MELTDOWN_NOP);
	else if (pmap_use_pcid) {
		extern long _pcid_set_reuse;
		DPRINTF("%s: codepatching PCID use", __func__);
		codepatch_replace(CPTAG_PCID_SET_REUSE, &_pcid_set_reuse,
		    PCID_SET_REUSE_SIZE);
	}
	splx(s);
}

#ifdef MULTIPROCESSOR
int mp_cpu_start(struct cpu_info *);
void mp_cpu_start_cleanup(struct cpu_info *);
struct cpu_functions mp_cpu_funcs = { mp_cpu_start, NULL,
				      mp_cpu_start_cleanup };
#endif /* MULTIPROCESSOR */

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpu_match, cpu_attach, NULL, cpu_activate
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

/*
 * Statically-allocated CPU info for the primary CPU (or the only
 * CPU, on uniprocessors).  The CPU info list is initialized to
 * point at it.
 */
struct cpu_info_full cpu_info_full_primary = { .cif_cpu = { .ci_self = &cpu_info_primary } };

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


void	cpu_idle_mwait_cycle(void);
void	cpu_init_mwait(struct cpu_softc *);

u_int	cpu_mwait_size, cpu_mwait_states;

void
cpu_idle_mwait_cycle(void)
{
	struct cpu_info *ci = curcpu();

	if ((read_rflags() & PSL_I) == 0)
		panic("idle with interrupts blocked!");

	/* something already queued? */
	if (!cpu_is_idle(ci))
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
	atomic_setbits_int(&ci->ci_mwait, MWAIT_IDLING | MWAIT_ONLY);
	if (cpu_is_idle(ci)) {
		monitor(&ci->ci_mwait, 0, 0);
		if ((ci->ci_mwait & MWAIT_IDLING) == MWAIT_IDLING)
			mwait(0, 0);
	}

	/* done idling; let cpu_kick() know that an IPI is required */
	atomic_clearbits_int(&ci->ci_mwait, MWAIT_IDLING);
}

void
cpu_init_mwait(struct cpu_softc *sc)
{
	unsigned int smallest, largest, extensions, c_substates;

	if ((cpu_ecxfeature & CPUIDECX_MWAIT) == 0 || cpuid_level < 0x5)
		return;

	/* get the monitor granularity */
	CPUID(0x5, smallest, largest, extensions, cpu_mwait_states);
	smallest &= 0xffff;
	largest  &= 0xffff;

	printf("%s: mwait min=%u, max=%u", sc->sc_dev.dv_xname,
	    smallest, largest);
	if (extensions & 0x1) {
		if (cpu_mwait_states > 0) {
			c_substates = cpu_mwait_states;
			printf(", C-substates=%u", 0xf & c_substates);
			while ((c_substates >>= 4) > 0)
				printf(".%u", 0xf & c_substates);
		}
		if (extensions & 0x2)
			printf(", IBE");
	} else {
		/* substates not supported, forge the default: just C1 */
		cpu_mwait_states = 1 << 4;
	}

	/* paranoia: check the values */
	if (smallest < sizeof(int) || largest < smallest ||
	    (largest & (sizeof(int)-1)))
		printf(" (bogus)");
	else
		cpu_mwait_size = largest;
	printf("\n");

	/* enable use of mwait; may be overriden by acpicpu later */
	if (cpu_mwait_size > 0)
		cpu_idle_cycle_fcn = &cpu_idle_mwait_cycle;
}

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
		struct cpu_info_full *cif;
		
		cif = km_alloc(sizeof *cif, &kv_any, &kp_zero, &kd_waitok);
		ci = &cif->cif_cpu;
#if defined(MULTIPROCESSOR)
		ci->ci_tss = &cif->cif_tss;
		ci->ci_gdt = &cif->cif_gdt;
		memcpy(ci->ci_gdt, cpu_info_primary.ci_gdt, GDT_SIZE);
		cpu_enter_pages(cif);
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
		if (caa->cpu_apicid != lapic_cpu_number()) {
			panic("%s: running cpu is at apic %d"
			    " instead of at expected %d",
			    sc->sc_dev.dv_xname, lapic_cpu_number(), caa->cpu_apicid);
		}
#endif
	}

	ci->ci_self = ci;
	sc->sc_info = ci;

	ci->ci_dev = self;
	ci->ci_apicid = caa->cpu_apicid;
	ci->ci_acpi_proc_id = caa->cpu_acpi_proc_id;
#ifdef MULTIPROCESSOR
	ci->ci_cpuid = cpunum;
#else
	ci->ci_cpuid = 0;	/* False for APs, but they're not used anyway */
#endif
	ci->ci_func = caa->cpu_func;
	ci->ci_handled_intr_level = IPL_NONE;

#if defined(MULTIPROCESSOR)
	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack.
	 */
	kstack = (vaddr_t)km_alloc(USPACE, &kv_any, &kp_dirty, &kd_nowait);
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
	pcb->pcb_cr3 = pcb->pcb_pmap->pm_pdirpa;
#endif

	/* further PCB init done later. */

	printf(": ");

	switch (caa->cpu_role) {
	case CPU_ROLE_SP:
		printf("(uniprocessor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_SP | CPUF_PRIMARY;
		cpu_intr_init(ci);
#ifndef SMALL_KERNEL
		cpu_ucode_apply(ci);
#endif
		identifycpu(ci);
#ifdef MTRR
		mem_range_attach();
#endif /* MTRR */
		cpu_init(ci);
		cpu_init_mwait(sc);
		break;

	case CPU_ROLE_BP:
		printf("apid %d (boot processor)\n", caa->cpu_apicid);
		ci->ci_flags |= CPUF_PRESENT | CPUF_BSP | CPUF_PRIMARY;
		cpu_intr_init(ci);
#ifndef SMALL_KERNEL
		cpu_ucode_apply(ci);
#endif
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
		ioapic_bsp_id = caa->cpu_apicid;
#endif
		cpu_init_mwait(sc);
		break;

	case CPU_ROLE_AP:
		/*
		 * report on an AP
		 */
		printf("apid %d (application processor)\n", caa->cpu_apicid);

#if defined(MULTIPROCESSOR)
		cpu_intr_init(ci);
		cpu_start_secondary(ci);
		sched_init_cpu(ci);
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
#if NVMM > 0
	cpu_init_vmm(ci);
#endif /* NVMM > 0 */
}

static void
replacexsave(void)
{
	extern long _xrstor, _xsave, _xsaveopt;
	u_int32_t eax, ebx, ecx, edx;
	static int replacedone = 0;
	int s;

	if (replacedone)
		return;
	replacedone = 1;

	/* find out whether xsaveopt is supported */
	CPUID_LEAF(0xd, 1, eax, ebx, ecx, edx);
	s = splhigh();
	codepatch_replace(CPTAG_XRSTOR, &_xrstor, 4);
	codepatch_replace(CPTAG_XSAVE,
	    (eax & XSAVE_XSAVEOPT) ? &_xsaveopt : &_xsave, 4);
	splx(s);
}


/*
 * Initialize the processor appropriately.
 */

void
cpu_init(struct cpu_info *ci)
{
	struct savefpu *sfp;
	u_int cr4;

	/* configure the CPU if needed */
	if (ci->cpu_setup != NULL)
		(*ci->cpu_setup)(ci);

	cr4 = rcr4() | CR4_DEFAULT;
	if (ci->ci_feature_sefflags_ebx & SEFF0EBX_SMEP)
		cr4 |= CR4_SMEP;
	if (ci->ci_feature_sefflags_ebx & SEFF0EBX_SMAP)
		cr4 |= CR4_SMAP;
	if (ci->ci_feature_sefflags_ecx & SEFF0ECX_UMIP)
		cr4 |= CR4_UMIP;
	if ((cpu_ecxfeature & CPUIDECX_XSAVE) && cpuid_level >= 0xd)
		cr4 |= CR4_OSXSAVE;
	if (pmap_use_pcid)
		cr4 |= CR4_PCIDE;
	lcr4(cr4);

	if ((cpu_ecxfeature & CPUIDECX_XSAVE) && cpuid_level >= 0xd) {
		u_int32_t eax, ebx, ecx, edx;

		xsave_mask = XCR0_X87 | XCR0_SSE;
		CPUID_LEAF(0xd, 0, eax, ebx, ecx, edx);
		if (eax & XCR0_AVX)
			xsave_mask |= XCR0_AVX;
		xsetbv(0, xsave_mask);
		CPUID_LEAF(0xd, 0, eax, ebx, ecx, edx);
		if (CPU_IS_PRIMARY(ci)) {
			fpu_save_len = ebx;
			KASSERT(fpu_save_len <= sizeof(struct savefpu));
		} else {
			KASSERT(ebx == fpu_save_len);
		}

		replacexsave();
	}

	/* Give proc0 a clean FPU save area */
	sfp = &proc0.p_addr->u_pcb.pcb_savefpu;
	memset(sfp, 0, fpu_save_len);
	sfp->fp_fxsave.fx_fcw = __INITIAL_NPXCW__;
	sfp->fp_fxsave.fx_mxcsr = __INITIAL_MXCSR__;
	fpureset();
	if (xsave_mask) {
		/* must not use xsaveopt here */
		xsave(sfp, xsave_mask);
	} else
		fxsave(sfp);

#if NVMM > 0
	/* Re-enable VMM if needed */
	if (ci->ci_flags & CPUF_VMM)
		start_vmm_on_cpu(ci);
#endif /* NVMM > 0 */

#ifdef MULTIPROCESSOR
	ci->ci_flags |= CPUF_RUNNING;
	/*
	 * Big hammer: flush all TLB entries, including ones from PTEs
	 * with the G bit set.  This should only be necessary if TLB
	 * shootdown falls far behind.
	 */
	cr4 = rcr4();
	lcr4(cr4 & ~CR4_PGE);
	lcr4(cr4);
#endif
}

#if NVMM > 0
/*
 * cpu_init_vmm
 *
 * Initializes per-cpu VMM state
 *
 * Parameters:
 *  ci: the cpu for which state is being initialized
 */
void
cpu_init_vmm(struct cpu_info *ci)
{
	/*
	 * Allocate a per-cpu VMXON region for VMX CPUs
	 */
	if (ci->ci_vmm_flags & CI_VMM_VMX) {
		ci->ci_vmxon_region = (struct vmxon_region *)malloc(PAGE_SIZE,
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (!pmap_extract(pmap_kernel(), (vaddr_t)ci->ci_vmxon_region,
		    &ci->ci_vmxon_region_pa))
			panic("Can't locate VMXON region in phys mem\n");
	}
}
#endif /* NVMM > 0 */

#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors(void)
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
		if (ci->ci_flags & (CPUF_BSP | CPUF_SP | CPUF_PRIMARY))
			continue;
		ci->ci_randseed = (arc4random() & 0x7fffffff) + 1;
		cpu_boot_secondary(ci);
	}
}

void
cpu_start_secondary(struct cpu_info *ci)
{
	int i;

	ci->ci_flags |= CPUF_AP;

	pmap_kenter_pa(MP_TRAMPOLINE, MP_TRAMPOLINE, PROT_READ | PROT_EXEC);
	pmap_kenter_pa(MP_TRAMP_DATA, MP_TRAMP_DATA, PROT_READ | PROT_WRITE);

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
		db_enter();
#endif
	}

	if ((ci->ci_flags & CPUF_IDENTIFIED) == 0) {
		atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);

		/* wait for it to identify */
		for (i = 2000000; (ci->ci_flags & CPUF_IDENTIFY) && i > 0; i--)
			delay(10);

		if (ci->ci_flags & CPUF_IDENTIFY)
			printf("%s: failed to identify\n",
			    ci->ci_dev->dv_xname);
	}

	CPU_START_CLEANUP(ci);

	pmap_kremove(MP_TRAMPOLINE, PAGE_SIZE);
	pmap_kremove(MP_TRAMP_DATA, PAGE_SIZE);
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
		db_enter();
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
	cpu_ucode_apply(ci);

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

	cpu_init_idt();
	lapic_set_lvt();
	gdt_init_cpu(ci);
	fpuinit(ci);

	lldt(0);

	cpu_init(ci);
#if NPVBUS > 0
	pvbus_init_cpu();
#endif

	/* Re-initialise memory range handling on AP */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->initAP(&mem_range_softc);

	s = splhigh();
	lcr8(0);
	intr_enable();

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

	db_printf("addr		dev	id	flags	ipis	curproc\n");
	CPU_INFO_FOREACH(cii, ci) {
		db_printf("%p	%s	%u	%x	%x	%10p\n",
		    ci,
		    ci->ci_dev == NULL ? "BOOT" : ci->ci_dev->dv_xname,
		    ci->ci_cpuid,
		    ci->ci_flags, ci->ci_ipis,
		    ci->ci_curproc);
	}
}
#endif

int
mp_cpu_start(struct cpu_info *ci)
{
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
		x86_ipi_init(ci->ci_apicid);

		delay(10000);

		if (cpu_feature & CPUID_APIC) {
			x86_ipi(MP_TRAMPOLINE/PAGE_SIZE, ci->ci_apicid,
			    LAPIC_DLMODE_STARTUP);
			delay(200);

			x86_ipi(MP_TRAMPOLINE/PAGE_SIZE, ci->ci_apicid,
			    LAPIC_DLMODE_STARTUP);
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
extern vector Xsyscall_meltdown, Xsyscall, Xsyscall32;

void
cpu_init_msrs(struct cpu_info *ci)
{
	wrmsr(MSR_STAR,
	    ((uint64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	    ((uint64_t)GSEL(GUCODE32_SEL, SEL_UPL) << 48));
	wrmsr(MSR_LSTAR, cpu_meltdown ? (uint64_t)Xsyscall_meltdown :
	    (uint64_t)Xsyscall);
	wrmsr(MSR_CSTAR, (uint64_t)Xsyscall32);
	wrmsr(MSR_SFMASK, PSL_NT|PSL_T|PSL_I|PSL_C|PSL_D|PSL_AC);

	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_GSBASE, (u_int64_t)ci);
	wrmsr(MSR_KERNELGSBASE, 0);

	patinit(ci);
}

void
patinit(struct cpu_info *ci)
{
	extern int	pmap_pg_wc;
	u_int64_t	reg;

	if ((cpu_feature & CPUID_PAT) == 0)
		return;
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
	extern int	has_rdrand, has_rdseed;
	union {
		uint64_t u64;
		uint32_t u32[2];
	} r, t;
	uint8_t valid = 0;

	if (has_rdseed)
		__asm volatile(
		    "rdseed	%0\n\t"
		    "setc	%1\n"
		    : "=r" (r.u64), "=qm" (valid) );
	if (has_rdrand && (has_rdseed == 0 || valid == 0))
		__asm volatile(
		    "rdrand	%0\n\t"
		    "setc	%1\n"
		    : "=r" (r.u64), "=qm" (valid) );

	t.u64 = rdtsc();

	if (valid)
		t.u64 ^= r.u64;
	enqueue_randomness(t.u32[0]);
	enqueue_randomness(t.u32[1]);

	if (tmo)
		timeout_add_msec(tmo, 10);
}

int
cpu_activate(struct device *self, int act)
{
	struct cpu_softc *sc = (struct cpu_softc *)self;

	switch (act) {
	case DVACT_RESUME:
		if (sc->sc_info->ci_cpuid == 0)
			rdrand(NULL);
		break;
	}

	return (0);
}

/*
 * cpu_enter_pages
 *
 * Requests mapping of various special pages required in the Intel Meltdown
 * case (to be entered into the U-K page table):
 *
 *  1 tss+gdt page for each CPU
 *  1 trampoline stack page for each CPU
 *
 * The cpu_info_full struct for each CPU straddles these pages. The offset into
 * 'cif' is calculated below, for each page. For more information, consult
 * the definition of struct cpu_info_full in cpu_full.h
 *
 * On CPUs unaffected by Meltdown, this function still configures 'cif' but
 * the calls to pmap_enter_special become no-ops.
 *
 * Parameters:
 *  cif : the cpu_info_full structure describing a CPU whose pages are to be
 *    entered into the special meltdown U-K page table.
 */ 
void
cpu_enter_pages(struct cpu_info_full *cif)
{
	vaddr_t va;
	paddr_t pa;

	/* The TSS+GDT need to be readable */
	va = (vaddr_t)cif;
	pmap_extract(pmap_kernel(), va, &pa);
	pmap_enter_special(va, pa, PROT_READ);
	DPRINTF("%s: entered tss+gdt page at va 0x%llx pa 0x%llx\n", __func__,
	   (uint64_t)va, (uint64_t)pa);

	/* The trampoline stack page needs to be read/write */
	va = (vaddr_t)&cif->cif_tramp_stack;
	pmap_extract(pmap_kernel(), va, &pa);
	pmap_enter_special(va, pa, PROT_READ | PROT_WRITE);
	DPRINTF("%s: entered t.stack page at va 0x%llx pa 0x%llx\n", __func__,
	   (uint64_t)va, (uint64_t)pa);

	cif->cif_tss.tss_rsp0 = va + sizeof(cif->cif_tramp_stack) - 16;
	DPRINTF("%s: cif_tss.tss_rsp0 = 0x%llx\n" ,__func__,
	    (uint64_t)cif->cif_tss.tss_rsp0);
	cif->cif_cpu.ci_intr_rsp = cif->cif_tss.tss_rsp0 -
	    sizeof(struct iretq_frame);

#define	SETUP_IST_SPECIAL_STACK(ist, cif, member) do {			\
	(cif)->cif_tss.tss_ist[(ist)] = (vaddr_t)&(cif)->member +	\
	    sizeof((cif)->member) - 16;					\
	(cif)->member[nitems((cif)->member) - 2] = (int64_t)&(cif)->cif_cpu; \
} while (0)

	SETUP_IST_SPECIAL_STACK(0, cif, cif_dblflt_stack);
	SETUP_IST_SPECIAL_STACK(1, cif, cif_nmi_stack);

	/* an empty iomap, by setting its offset to the TSS limit */
	cif->cif_tss.tss_iobase = sizeof(cif->cif_tss);
}
