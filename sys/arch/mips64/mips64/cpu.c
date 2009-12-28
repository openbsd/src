/*	$OpenBSD: cpu.c,v 1.22 2009/12/28 06:55:27 syuu Exp $ */

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
#include <sys/user.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

int	cpumatch(struct device *, void *, void *);
void	cpuattach(struct device *, struct device *, void *);

struct cpu_info cpu_info_primary;
struct cpu_info *cpu_info_list = &cpu_info_primary;
#ifdef MULTIPROCESSOR
struct cpuset cpus_running;

/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */

struct cpu_info *cpu_info[MAXCPUS] = { &cpu_info_primary };
#endif

u_int	CpuPrimaryInstCacheSize;
u_int	CpuPrimaryInstCacheLSize;
u_int	CpuPrimaryInstSetSize;
u_int	CpuPrimaryDataCacheSize;
u_int	CpuPrimaryDataCacheLSize;
u_int	CpuPrimaryDataSetSize;
u_int	CpuCacheAliasMask;
u_int	CpuSecondaryCacheSize;
u_int	CpuTertiaryCacheSize;
u_int	CpuNWayCache;
u_int	CpuCacheType;		/* R4K, R5K, RM7K */
u_int	CpuConfigRegister;
u_int	CpuStatusRegister;
u_int	CpuExternalCacheOn;	/* R5K, RM7K */
u_int	CpuOnboardCacheOn;	/* RM7K */

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
	struct cfdata *cf = match;
	struct mainbus_attach_args *maa = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(maa->maa_name, cpu_cd.cd_name) != 0)
		return 0;
	if (cf->cf_unit >= MAX_CPUS)
		return 0;

	return 20;	/* Make CPU probe first */
}

void
cpuattach(struct device *parent, struct device *dev, void *aux)
{
	struct cpu_info *ci;
	int cpuno = dev->dv_unit;
	int isr16k = 0;
	int displayver;

	if (cpuno == 0) {
		ci = &cpu_info_primary;
#ifdef MULTIPROCESSOR
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
		cpuset_add(&cpus_running, ci);
#endif
	}
#ifdef MULTIPROCESSOR
	else {
		ci = (struct cpu_info *)smp_malloc(sizeof(*ci));
		if (ci == NULL)
			panic("unable to allocate cpu_info\n");
		bzero((char *)ci, sizeof(*ci));
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ci->ci_flags |= CPUF_PRESENT;
		cpu_info[cpuno] = ci;
	}
#endif
	ci->ci_self = ci;
	ci->ci_cpuid = cpuno;
	ci->ci_dev = dev;

	printf(": ");

	displayver = 1;
	switch (sys_config.cpu[cpuno].type) {
	case MIPS_R4000:
		if (CpuPrimaryInstCacheSize == 16384)
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
		if (sys_config.cpu[cpuno].vers_maj > 2) {
			sys_config.cpu[cpuno].vers_maj -= 2;
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
		if (sys_config.cpu[cpuno].vers_maj < 2)
			printf("PMC-Sierra RM7000 CPU");
		else
			printf("PMC-Sierra RM7000A CPU");
		cpu_is_rm7k++;
		break;
	case MIPS_RM9000:
		printf("PMC-Sierra RM9000 CPU");
		break;
	case MIPS_LOONGSON2:
		printf("STC Loongson2%c CPU",
		    'C' + sys_config.cpu[cpuno].vers_min);
		displayver = 0;
		break;
	default:
		printf("Unknown CPU type (0x%x)",sys_config.cpu[cpuno].type);
		break;
	}
	if (displayver != 0)
		printf(" rev %d.%d", sys_config.cpu[cpuno].vers_maj,
		    sys_config.cpu[cpuno].vers_min);
	printf(" %d MHz, ", sys_config.cpu[cpuno].clock / 1000000);

	displayver = 1;
	switch (sys_config.cpu[cpuno].fptype) {
	case MIPS_SOFT:
		printf("Software FP emulation");
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
		printf("R1%d000 FPU", isr16k ? 6 : 4);
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
		printf("STC Loongson2%c FPU",
		    'C' + sys_config.cpu[cpuno].fpvers_min);
		displayver = 0;
		break;
	default:
		printf("Unknown FPU type (0x%x)", sys_config.cpu[cpuno].fptype);
		break;
	}
	if (displayver != 0)
		printf(" rev %d.%d", sys_config.cpu[cpuno].fpvers_maj,
		    sys_config.cpu[cpuno].fpvers_min);
	printf("\n");

	printf("cpu%d: cache L1-I %dKB", cpuno, CpuPrimaryInstCacheSize / 1024);
	printf(" D %dKB ", CpuPrimaryDataCacheSize / 1024);

	switch (CpuNWayCache) {
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

	if (CpuSecondaryCacheSize != 0) {
		switch (sys_config.cpu[cpuno].type) {
		case MIPS_R10000:
		case MIPS_R12000:
		case MIPS_R14000:
			printf(", L2 %dKB 2 way", CpuSecondaryCacheSize / 1024);
			break;
		case MIPS_RM7000:
		case MIPS_RM9000:
		case MIPS_LOONGSON2:
			printf(", L2 %dKB 4 way", CpuSecondaryCacheSize / 1024);
			break;
		default:
			printf(", L2 %dKB direct", CpuSecondaryCacheSize / 1024);
			break;
		}
	}
	if (CpuTertiaryCacheSize != 0)
		printf(", L3 %dKB direct", CpuTertiaryCacheSize / 1024);
	printf("\n");

#ifdef DEBUG
	printf("cpu%d: Setsize %d:%d\n", cpuno,
	    CpuPrimaryInstSetSize, CpuPrimaryDataSetSize);
	printf("cpu%d: Alias mask 0x%x\n", cpuno, CpuCacheAliasMask);
	printf("cpu%d: Config Register %x\n", cpuno, CpuConfigRegister);
	printf("cpu%d: Cache type %x\n", cpuno, CpuCacheType);
	if (sys_config.cpu[cpuno].fptype == MIPS_RM7000) {
		u_int tmp = CpuConfigRegister;

		printf("cpu%d: ", cpuno);
		printf("K0 = %1d  ",0x7 & tmp);
		printf("SE = %1d  ",0x1 & (tmp>>3));
		printf("DB = %1d  ",0x1 & (tmp>>4));
		printf("IB = %1d\n",0x1 & (tmp>>5));
		printf("cpu%d: ", cpuno);
		printf("DC = %1d  ",0x7 & (tmp>>6));
		printf("IC = %1d  ",0x7 & (tmp>>9));
		printf("TE = %1d  ",0x1 & (tmp>>12));
		printf("EB = %1d\n",0x1 & (tmp>>13));
		printf("cpu%d: ", cpuno);
		printf("EM = %1d  ",0x1 & (tmp>>14));
		printf("BE = %1d  ",0x1 & (tmp>>15));
		printf("TC = %1d  ",0x1 & (tmp>>17));
		printf("EW = %1d\n",0x3 & (tmp>>18));
		printf("cpu%d: ", cpuno);
		printf("TS = %1d  ",0x3 & (tmp>>20));
		printf("EP = %1d  ",0xf & (tmp>>24));
		printf("EC = %1d  ",0x7 & (tmp>>28));
		printf("SC = %1d\n",0x1 & (tmp>>31));
	}
	printf("cpu%d: Status Register %x\n", cpuno, CpuStatusRegister);
#endif
}

#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors(void)
{
       struct cpu_info *ci;
       u_long i;

       for (i = 0; i < MAXCPUS; i++) {
               ci = cpu_info[i];
               if (ci == NULL)
                       continue;
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
smp_malloc(size_t size)
{
       struct pglist mlist;
       struct vm_page *m;
       int error;
       vaddr_t va;
       paddr_t pa;

       if (size < PAGE_SIZE) {
	       va = (vaddr_t)malloc(size, M_DEVBUF, M_NOWAIT);
	       if (va == NULL)
		       return NULL;
	       error = pmap_extract(pmap_kernel(), va, &pa);
	       if (error == FALSE)
		       return NULL;
       } else { 
	       TAILQ_INIT(&mlist);
	       error = uvm_pglistalloc(size, 0, -1L, 0, 0,
		   &mlist, 1, UVM_PLA_NOWAIT);
	       if (error)
		       return NULL;
	       m = TAILQ_FIRST(&mlist);
	       pa = VM_PAGE_TO_PHYS(m);
       }

       return PHYS_TO_XKPHYS(pa, CCA_CACHED);
}
#endif
