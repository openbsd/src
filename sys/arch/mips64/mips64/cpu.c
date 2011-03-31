/*	$OpenBSD: cpu.c,v 1.37 2011/03/31 20:37:44 miod Exp $ */

/*
 * Copyright (c) 1997-2004 Opsycon AB (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

int	cpumatch(struct device *, void *, void *);
void	cpuattach(struct device *, struct device *, void *);

struct cpu_info cpu_info_primary;
struct cpu_info *cpu_info_list = &cpu_info_primary;
struct cpu_info *cpu_info_secondaries;
#ifdef MULTIPROCESSOR
struct cpuset cpus_running;
#endif

vaddr_t	CpuCacheAliasMask;

int cpu_is_rm7k = 0;

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};
struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL, NULL, 0
};

int
cpumatch(struct device *parent, void *match, void *aux)
{
	struct cpu_attach_args *caa = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(caa->caa_maa.maa_name, cpu_cd.cd_name) != 0)
		return 0;

	return 20;	/* Make CPU probe first */
}

void
cpuattach(struct device *parent, struct device *dev, void *aux)
{
	struct cpu_attach_args *caa = aux;
	struct cpu_hwinfo *ch = caa->caa_hw;
	struct cpu_info *ci;
	int cpuno = dev->dv_unit;
	int isr16k = 0;
	int fptype, vers_maj, vers_min;
	int displayver;

	if (cpuno == 0) {
		ci = &cpu_info_primary;
#ifdef MULTIPROCESSOR
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
		cpuset_add(&cpus_running, ci);
		cpu_info_secondaries = (struct cpu_info *)alloc_contiguous_pages(
			sizeof(struct cpu_info) * ncpusfound - 1);
		if (cpu_info_secondaries == NULL)
			panic("unable to allocate cpu_info");
#endif
	}
#ifdef MULTIPROCESSOR
	else {
		ci = &cpu_info_secondaries[cpuno - 1];
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ci->ci_flags |= CPUF_PRESENT;
	}
#endif
	ci->ci_self = ci;
	ci->ci_cpuid = cpuno;
	ci->ci_dev = dev;
	bcopy(ch, &ci->ci_hw, sizeof(struct cpu_hwinfo));
#ifdef MULTIPROCESSOR
	if (!ISSET(ci->ci_flags, CPUF_PRIMARY))
		hw_cpu_init_secondary(ci);
#endif

	printf(": ");

	displayver = 1;
	vers_maj = (ch->c0prid >> 4) & 0x0f;
	vers_min = ch->c0prid & 0x0f;
	switch (ch->type) {
	case MIPS_R4000:
		if (ci->ci_l1instcachesize == 16384)
			printf("MIPS R4400 CPU");
		else
			printf("MIPS R4000 CPU");
		break;
	case MIPS_R5000:
		printf("MIPS R5000 CPU");
		break;
	case MIPS_R10000:
		printf("MIPS R10000 CPU");
		break;
	case MIPS_R12000:
		printf("MIPS R12000 CPU");
		break;
	case MIPS_R14000:
		if (vers_maj > 2) {
			vers_maj -= 2;
			isr16k = 1;
		}
		printf("R1%d000 CPU", isr16k ? 6 : 4);
		break;
	case MIPS_R4200:
		printf("NEC VR4200 CPU (ICE)");
		break;
	case MIPS_R4300:
		printf("NEC VR4300 CPU");
		break;
	case MIPS_R4100:
		printf("NEC VR41xx CPU");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion CPU");
		break;
	case MIPS_R4700:
		printf("QED R4700 Orion CPU");
		break;
	case MIPS_RM52X0:
		printf("PMC-Sierra RM52X0 CPU");
		break;
	case MIPS_RM7000:
		if (vers_maj < 2)
			printf("PMC-Sierra RM7000 CPU");
		else
			printf("PMC-Sierra RM7000A CPU");
		cpu_is_rm7k++;
		break;
	case MIPS_RM9000:
		printf("PMC-Sierra RM9000 CPU");
		break;
	case MIPS_LOONGSON2:
		switch (ch->c0prid & 0xff) {
		case 0x00:
		case 0x02:
		case 0x03:
			printf("STC Loongson2%c CPU", 'C' + vers_min);
			break;
		case 0x05:
			printf("STC Loongson3%c CPU", 'A' + vers_min - 5);
			break;
		default:
			printf("Unknown STC Loongson CPU type (%02x)",
			    ch->c0prid & 0xff);
			break;
		}
		displayver = 0;
		break;
	case MIPS_OCTEON:
		printf("Cavium OCTEON CPU");
		break;
	default:
		printf("Unknown CPU type (0x%x)", ch->type);
		break;
	}
	if (displayver != 0)
		printf(" rev %d.%d", vers_maj, vers_min);
	printf(" %d MHz, ", ch->clock / 1000000);

	displayver = 1;
	if (ch->type == MIPS_OCTEON)
		fptype = MIPS_SOFT;
	else {
		fptype = (ch->c1prid >> 8) & 0xff;
		vers_maj = (ch->c1prid >> 4) & 0x0f;
		vers_min = ch->c1prid & 0x0f;
	}
	switch (fptype) {
	case MIPS_SOFT:
		printf("Software FP emulation");
		displayver = 0;
		break;
	case MIPS_R4000:
		printf("R4010 FPC");
		break;
	case MIPS_R10000:
		printf("R10000 FPU");
		break;
	case MIPS_R12000:
		printf("R12000 FPU");
		break;
	case MIPS_R14000:
		if (isr16k) {
			if (ch->c0prid == ch->c1prid)
				vers_maj -= 2;
			printf("R16000 FPU");
		} else
			printf("R14000 FPU");
		break;
	case MIPS_R4200:
		printf("VR4200 FPC (ICE)");
		break;
	case MIPS_R4600:
		printf("R4600 Orion FPC");
		break;
	case MIPS_R4700:
		printf("R4700 Orion FPC");
		break;
	case MIPS_R5000:
		printf("R5000 based FPC");
		break;
	case MIPS_RM52X0:
		printf("RM52X0 FPC");
		break;
	case MIPS_RM7000:
		printf("RM7000 FPC");
		break;
	case MIPS_RM9000:
		printf("RM9000 FPC");
		break;
	case MIPS_LOONGSON2:
		printf("STC Loongson2%c FPU", 'C' + vers_min);
		displayver = 0;
		break;
	default:
		printf("Unknown FPU type (0x%x)", fptype);
		break;
	}
	if (displayver != 0)
		printf(" rev %d.%d", vers_maj, vers_min);
	printf("\n");

	printf("cpu%d: cache L1-I %dKB D %dKB ", cpuno,
	    ci->ci_l1instcachesize / 1024, ci->ci_l1datacachesize / 1024);

	switch (ci->ci_cacheways) {
	case 2:
		printf("2 way");
		break;
	case 4:
		printf("4 way");
		break;
	default:
		printf("1 way");
		break;
	}

	if (ci->ci_l2size != 0) {
		switch (ch->type) {
		case MIPS_R10000:
		case MIPS_R12000:
		case MIPS_R14000:
			printf(", L2 %dKB 2 way", ci->ci_l2size / 1024);
			break;
		case MIPS_RM7000:
		case MIPS_RM9000:
		case MIPS_LOONGSON2:
			printf(", L2 %dKB 4 way", ci->ci_l2size / 1024);
			break;
		default:
			printf(", L2 %dKB direct", ci->ci_l2size / 1024);
			break;
		}
	}
	if (ci->ci_l3size != 0)
		printf(", L3 %dKB direct", ci->ci_l3size / 1024);
	printf("\n");

#ifdef DEBUG
	printf("cpu%d: Setsize %d:%d\n", cpuno,
	    ci->ci_l1instcacheset, ci->ci_l1datacacheset);
	printf("cpu%d: Alias mask %p\n", cpuno, CpuCacheAliasMask);
	printf("cpu%d: Config Register %08x\n", cpuno, cp0_get_config());
	printf("cpu%d: Cache configuration %x\n",
	    cpuno, ci->ci_cacheconfiguration);
	if (ch->type == MIPS_RM7000) {
		uint32_t tmp = cp0_get_config();

		printf("cpu%d: ", cpuno);
		printf("K0 = %1d  ", 0x7 & tmp);
		printf("SE = %1d  ", 0x1 & (tmp>>3));
		printf("DB = %1d  ", 0x1 & (tmp>>4));
		printf("IB = %1d\n", 0x1 & (tmp>>5));
		printf("cpu%d: ", cpuno);
		printf("DC = %1d  ", 0x7 & (tmp>>6));
		printf("IC = %1d  ", 0x7 & (tmp>>9));
		printf("TE = %1d  ", 0x1 & (tmp>>12));
		printf("EB = %1d\n", 0x1 & (tmp>>13));
		printf("cpu%d: ", cpuno);
		printf("EM = %1d  ", 0x1 & (tmp>>14));
		printf("BE = %1d  ", 0x1 & (tmp>>15));
		printf("TC = %1d  ", 0x1 & (tmp>>17));
		printf("EW = %1d\n", 0x3 & (tmp>>18));
		printf("cpu%d: ", cpuno);
		printf("TS = %1d  ", 0x3 & (tmp>>20));
		printf("EP = %1d  ", 0xf & (tmp>>24));
		printf("EC = %1d  ", 0x7 & (tmp>>28));
		printf("SC = %1d\n", 0x1 & (tmp>>31));
	}
	printf("cpu%d: Status Register %08x\n", cpuno, getsr());
#endif
}

extern void cpu_switchto_asm(struct proc *, struct proc *);
extern void MipsSaveCurFPState(struct proc *);
extern void MipsSaveCurFPState16(struct proc *);
extern void MipsSwitchFPState(struct proc *, struct trap_frame *);
extern void MipsSwitchFPState16(struct proc *, struct trap_frame *);

void
cpu_switchto(struct proc *oldproc, struct proc *newproc)
{
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
	if (ci->ci_fpuproc)
		save_fpu();
#endif

	cpu_switchto_asm(oldproc, newproc);
}

void
enable_fpu(struct proc *p)
{
#ifndef FPUEMUL
	struct cpu_info *ci = curcpu();

	if (p->p_md.md_regs->sr & SR_FR_32)
		MipsSwitchFPState(ci->ci_fpuproc, p->p_md.md_regs);
	else
		MipsSwitchFPState16(ci->ci_fpuproc, p->p_md.md_regs);

	ci->ci_fpuproc = p;
	p->p_md.md_regs->sr |= SR_COP_1_BIT;
	p->p_md.md_flags |= MDP_FPUSED;
#endif
}

void
save_fpu(void)
{
#ifndef FPUEMUL
	struct cpu_info *ci = curcpu();
	struct proc *p;

	KASSERT(ci->ci_fpuproc);
	p = ci->ci_fpuproc;
	if (p->p_md.md_regs->sr & SR_FR_32)
		MipsSaveCurFPState(p);
	else
		MipsSaveCurFPState16(p);
#endif
}

#ifdef MULTIPROCESSOR
struct cpu_info *
get_cpu_info(int cpuno)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci->ci_cpuid == cpuno)
			return ci;
	}
	return NULL;
}

void
cpu_boot_secondary_processors(void)
{
       struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	CPU_INFO_FOREACH(cii, ci) {
               if ((ci->ci_flags & CPUF_PRESENT) == 0)
                       continue;
               if (ci->ci_flags & CPUF_PRIMARY)
                       continue;

               sched_init_cpu(ci);
               ci->ci_randseed = random();
               cpu_boot_secondary(ci);
	}

       /* This must called after xheart0 has initialized, so here is 
	* the best place to do so.
	*/
       mips64_ipi_init();
}

void
cpu_unidle(struct cpu_info *ci)
{
	if (ci != curcpu())
		mips64_send_ipi(ci->ci_cpuid, MIPS64_IPI_NOP);
}

vaddr_t 
alloc_contiguous_pages(size_t size)
{
	struct pglist mlist;
	struct vm_page *m;
	int error;
	paddr_t pa;

	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(roundup(size, USPACE), 0, 0xffffffff, 0, 0,
		&mlist, 1, UVM_PLA_NOWAIT | UVM_PLA_ZERO);
	if (error)
		return NULL;
	m = TAILQ_FIRST(&mlist);
	pa = VM_PAGE_TO_PHYS(m);

	return PHYS_TO_XKPHYS(pa, CCA_CACHED);
}
#endif
