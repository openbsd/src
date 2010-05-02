/*	$OpenBSD: m8820x.c,v 1.9 2010/05/02 22:01:43 miod Exp $	*/
/*
 * Copyright (c) 2004, 2006, 2010 Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/avcommon.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/m8820x.h>
#include <machine/pmap.h>
#include <machine/prom.h>

extern	u_int32_t pfsr_straight[];
extern	u_int32_t pfsr_double[];
extern	u_int32_t pfsr_six[];

/*
 * This routine sets up the CPU/CMMU configuration.
 */
void
m8820x_setup_board_config()
{
	extern u_int32_t pfsr_save[];
	struct m8820x_cmmu *cmmu;
	struct scm_cpuconfig scc;
	int type, cpu_num, cpu_cmmu_num, cmmu_num, cmmu_per_cpu;
	volatile u_int *cr;
	u_int32_t whoami;
	u_int32_t *m8820x_pfsr;

	/*
	 * First, find if any CPU0 CMMU is a 88204. If so, we can
	 * issue the CPUCONFIG system call to get the configuration
	 * details.
	 * NOTE that this relies upon [0] and [1] to always have
	 * valid CMMU addresses - thankfully this is always the case
	 * on model 530 regardless of the CMMU configuration.
	 */
	if (badaddr((vaddr_t)m8820x_cmmu[0].cmmu_regs, 4) != 0 ||
	    badaddr((vaddr_t)m8820x_cmmu[1].cmmu_regs, 4) != 0) {
		printf("CPU0: missing CMMUs ???\n");
		scm_halt();
		/* NOTREACHED */
	}

	cr = (void *)m8820x_cmmu[0].cmmu_regs;
	type = CMMU_TYPE(cr[CMMU_IDR]);

	if (type != M88204_ID && type != M88200_ID) {
		printf("CPU0: unrecognized CMMU type %d\n", type);
		scm_halt();
		/* NOTREACHED */
	}

	/*
	 * Try and use the CPUCONFIG system call to get all the information
	 * we need. This is theoretically only available on 88204-based
	 * machines, but it can't hurt to give it a try.
	 */
	if (scm_cpuconfig(&scc) == 0 && scc.version == SCM_CPUCONFIG_VERSION)
		goto knowledge;

	/*
	 * XXX Instead of deciding on the CMMU type, we should decide on
	 * XXX the board type instead. But then, I am not sure not all
	 * XXX 88204-based designs have the WHOAMI register... -- miod
	 */
	switch (type) {
	case M88204_ID:
		/*
		 * Probe CMMU addresses to discover which CPU slots are
		 * populated. Actually, we'll simply check how many upper
		 * slots we can ignore, and keep using badaddr() to cope
		 * with unpopulated slots.
		 */
hardprobe:
		/*
		 * First, we'll assume we are in a 2:1 configuration, thus no
		 * CMMU split scheme in use.
		 */
		scc.igang = scc.dgang = 1;
		scc.isplit = scc.dsplit = 0;

		/*
		 * Probe CMMU addresses to discover which CPU slots are
		 * populated. Actually, we'll simply check how many upper
		 * slots we can ignore, and keep using badaddr() to cope
		 * with unpopulated slots.
		 */
		cmmu = m8820x_cmmu + MAX_CMMUS - 1;
		for (max_cmmus = MAX_CMMUS - 1; max_cmmus != 0;
		    max_cmmus--, cmmu--) {
			if (cmmu->cmmu_regs == NULL)
				continue;
			if (badaddr((vaddr_t)cmmu->cmmu_regs, 4) == 0)
				break;
		}
		scc.cpucount = (1 + max_cmmus) >> 1;
		break;

	case M88200_ID:
		/*
		 * Deduce our configuration from the WHOAMI register.
		 */
		whoami = (*(volatile u_int32_t *)AV_WHOAMI & 0xf0) >> 4;
		switch (whoami) {
		case 0:		/* 4 CPUs, 8 CMMUs */
			scc.cpucount = 4;
			break;
		case 5:		/* 2 CPUs, 4 CMMUs */
			scc.cpucount = 2;
			break;
		case 0x0a:	/* 1 CPU, 2 CMMU */
			scc.cpucount = 1;
			break;
		case 3:		/* 2 CPUs, 12 CMMUs */
		case 7:		/* 1 CPU, 6 CMMU */
			/*
			 * Regular logic can't cope with asymmetrical
			 * designs. Report a 4:1 ratio with two missing
			 * data CMMUs.
			 */
			ncpusfound = whoami == 7 ? 1 : 2;
			cmmu_per_cpu = 6;
			cmmu_shift = 3;
			max_cmmus = ncpusfound << cmmu_shift;
			scc.isplit = scc.dsplit = 0;	/* XXX unknown */
			m8820x_pfsr = pfsr_six;

			/*
			 * We can't use writeback userland mappings until
			 * the CMMU split scheme is known, as the current
			 * pessimistic behaviour is not good enough to
			 * prevent out-of-sync cache lines from occuring.
			 */
			default_apr |= CACHE_WT;

			goto done;
			break;
		default:
			printf("unrecognized CMMU configuration, whoami %x\n",
			    whoami);
#if 0
			scm_halt();
#else
			goto hardprobe;
#endif
		}
		/*
		 * Oh, and we are in a 2:1 configuration, thus no
		 * CMMU split scheme in use.
		 */
		scc.igang = scc.dgang = 1;
		scc.isplit = scc.dsplit = 0;

		break;
	}

knowledge:
	if (scc.igang != scc.dgang ||
	    scc.igang == 0 || scc.igang > 2) {
		printf("Unsupported CMMU to CPU ratio (%dI/%dD)\n",
		    scc.igang, scc.dgang);
		scm_halt();
		/* NOTREACHED */
	}

	ncpusfound = scc.cpucount;
	if (scc.igang == 1) {
		cmmu_shift = 1;
		m8820x_pfsr = pfsr_straight;
	} else {
		cmmu_shift = 2;
		m8820x_pfsr = pfsr_double;
	}
	max_cmmus = ncpusfound << cmmu_shift;
	cmmu_per_cpu = 1 << cmmu_shift;

done:
	/*
	 * Now that we know which CMMUs are there, report every association
	 */
	for (cpu_num = 0; cpu_num < ncpusfound; cpu_num++) {
		cmmu_num = cpu_num << cmmu_shift;
		cr = m8820x_cmmu[cmmu_num].cmmu_regs;
		if (badaddr((vaddr_t)cr, 4) == 0) {
			type = CMMU_TYPE(m8820x_cmmu[cmmu_num].
			    cmmu_regs[CMMU_IDR]);

			printf("CPU%d is associated to %d MC8820%c CMMUs\n",
			    cpu_num, cmmu_per_cpu,
			    type == M88204_ID ? '4' : '0');
		}
	}


	/*
	 * Now set up addressing limits
	 */
	if (cmmu_shift > 1) {
		for (cmmu_num = 0, cmmu = m8820x_cmmu; cmmu_num < max_cmmus;
		    cmmu_num++, cmmu++) {
			cpu_cmmu_num = cmmu_num >> 1; /* CPU view of the CMMU */

			if (CMMU_MODE(cmmu_num) == INST_CMMU) {
				/* I0, I1 */
				cmmu->cmmu_addr =
				    cpu_cmmu_num < 2 ? 0 : scc.isplit;
				cmmu->cmmu_addr_mask = scc.isplit;
			} else {
				/* D0, D1 */
				cmmu->cmmu_addr =
				    cpu_cmmu_num < 2 ? 0 : scc.dsplit;
				cmmu->cmmu_addr_mask = scc.dsplit;
			}
		}
	}

	/*
	 * Patch the exception handling code to invoke the correct pfsr
	 * analysis chunk.
	 */
	pfsr_save[0] = 0xc4000000 |
	    (((vaddr_t)m8820x_pfsr + 4 - (vaddr_t)pfsr_save) >> 2);
	pfsr_save[1] = m8820x_pfsr[0];
}

/*
 * Find out the CPU number from accessing CMMU.
 * We access the WHOAMI register, which is in data space;
 * its value will let us know which CPU has been used to perform the read.
 */
cpuid_t
m8820x_cpu_number()
{
	u_int32_t whoami;
	cpuid_t cpu;

	whoami = *(volatile u_int32_t *)AV_WHOAMI;
	switch ((whoami & 0xf0) >> 4) {
	case 0:
	case 3:
	case 5:
		for (cpu = 0; cpu < 4; cpu++)
			if (whoami & (1 << cpu))
				return (cpu);
		break;
	case 7:
	case 0x0a:
		/* for single processors, this field of whoami is undefined */
		return (0);
	}

	panic("m8820x_cpu_number: could not determine my cpu number, whoami %x",
	    whoami);
}
