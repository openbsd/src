/*	$OpenBSD: m8820x.c,v 1.51 2011/10/09 17:01:34 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
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
#include <machine/cpu.h>

#include <machine/cmmu.h>
#include <machine/m8820x.h>
#ifdef MVME187
#include <machine/mvme187.h>
#endif
#ifdef MVME188
#include <machine/mvme188.h>
#endif

#ifdef MVME188
/*
 * There are 6 possible MVME188 HYPERmodule configurations:
 *  - config 0: 4 CPUs, 8 CMMUs
 *  - config 1: 2 CPUs, 8 CMMUs
 *  - config 2: 1 CPUs, 8 CMMUs
 *  - config 5: 2 CPUs, 4 CMMUs
 *  - config 6: 1 CPU,  4 CMMUs
 *  - config A: 1 CPU,  2 CMMUs (similar in operation to MVME187)
 * which can exist either with MC88200 or MC88204 CMMUs.
 */
const struct board_config {
	int ncpus;
	int ncmmus;
	u_int32_t *pfsr;
} bd_config[16] = {
	{ 4, 8, pfsr_save_188_straight },	/* 4P128 - 4P512 */
	{ 2, 8, pfsr_save_188_double },		/* 2P128 - 2P512 */
	{ 1, 8, pfsr_save_188_quad },		/* 1P128 - 1P512 */
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 2, 4, pfsr_save_188_straight },	/* 2P64  - 2P256 */
	{ 1, 4, pfsr_save_188_double },		/* 1P64  - 1P256 */
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 1, 2, pfsr_save_188_straight },	/* 1P32  - 1P128 */
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 0, 0, NULL },
	{ 0, 0, NULL }
};
#endif

/*
 * This routine sets up the CPU/CMMU configuration.
 */
void
m8820x_setup_board_config()
{
	extern u_int32_t pfsr_save[];
	int num;
	u_int32_t *m8820x_pfsr;
#ifdef MVME188
	u_int32_t whoami;
	int vme188_config;
	struct m8820x_cmmu *cmmu;
	int cmmu_num;
#endif

	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
#ifdef MVME188
		/* There is no WHOAMI reg on MVME187 - fake it... */
		vme188_config = 0x0a;
#endif
		m8820x_cmmu[0].cmmu_regs = (void *)SBC_CMMU_I;
		m8820x_cmmu[1].cmmu_regs = (void *)SBC_CMMU_D;
		ncpusfound = 1;
		max_cmmus = 2;
		cmmu_shift = 1;
		m8820x_pfsr = pfsr_save_187;
		break;
#endif /* MVME187 */
#ifdef MVME188
	case BRD_188:
		whoami = *(volatile u_int32_t *)MVME188_WHOAMI;
		vme188_config = (whoami & 0xf0) >> 4;
		m8820x_cmmu[0].cmmu_regs = (void *)VME_CMMU_I0;
		m8820x_cmmu[1].cmmu_regs = (void *)VME_CMMU_D0;
		m8820x_cmmu[2].cmmu_regs = (void *)VME_CMMU_I1;
		m8820x_cmmu[3].cmmu_regs = (void *)VME_CMMU_D1;
		m8820x_cmmu[4].cmmu_regs = (void *)VME_CMMU_I2;
		m8820x_cmmu[5].cmmu_regs = (void *)VME_CMMU_D2;
		m8820x_cmmu[6].cmmu_regs = (void *)VME_CMMU_I3;
		m8820x_cmmu[7].cmmu_regs = (void *)VME_CMMU_D3;
		ncpusfound = bd_config[vme188_config].ncpus;
		max_cmmus = bd_config[vme188_config].ncmmus;
		m8820x_pfsr = bd_config[vme188_config].pfsr;
		cmmu_shift = ff1(max_cmmus / ncpusfound);
		break;
#endif /* MVME188 */
	}

#ifdef MVME188
	if (bd_config[vme188_config].ncpus != 0) {
		/* 187 has a fixed configuration, no need to print it */
		if (brdtyp == BRD_188) {
			printf("MVME188 board configuration #%X "
			    "(%d CPUs %d CMMUs)\n",
			    vme188_config, ncpusfound, max_cmmus);
		}
	} else {
		panic("unrecognized MVME%x board configuration #%X",
		    brdtyp, vme188_config);
	}
#endif

	/*
	 * Patch the exception handling code to invoke the correct pfsr
	 * analysis chunk.
	 */
	pfsr_save[0] = 0xc4000000 |
	    (((vaddr_t)m8820x_pfsr + 4 - (vaddr_t)pfsr_save) >> 2);
	pfsr_save[1] = m8820x_pfsr[0];

#ifdef DEBUG
	/*
	 * Check CMMU type
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		volatile u_int32_t *cr = m8820x_cmmu[cmmu_num].cmmu_regs;
		if (badaddr((vaddr_t)cr, 4) == 0) {
			int type;

			type = CMMU_TYPE(cr[CMMU_IDR]);
			if (type != M88200_ID && type != M88204_ID) {
				printf("WARNING: non M8820x circuit found "
				    "at CMMU address %p\n", cr);
				continue;	/* will probably die quickly */
			}
		}
	}
#endif

	/*
	 * Now that we know which CMMUs are there, report every association
	 */
	for (num = 0; num < ncpusfound; num++) {
		int type;

		type = CMMU_TYPE(m8820x_cmmu[num << cmmu_shift].
		    cmmu_regs[CMMU_IDR]);

		printf("CPU%d is associated to %d MC8820%c CMMUs\n",
		    num, 1 << cmmu_shift, type == M88204_ID ? '4' : '0');
	}


#ifdef MVME188
	/*
	 * Systems with more than 2 CMMUs per CPU use programmable split
	 * schemes, through PCNFA (for code CMMUs) and PCNFB (for data CMMUs)
	 * configuration registers.
	 *
	 * The following schemes are available:
	 * - split on A12 address bit (A14 for 88204)
	 * - split on supervisor/user access
	 * - split on SRAM/non-SRAM addresses, with either supervisor-only or
	 *   all access to SRAM.
	 *
	 * Configuration 6, with 4 CMMUs par CPU, also allows a split on A14
	 * address bit (A16 for 88204).
	 *
	 * Setup the default A12/A14 scheme here. We should theoretically only
	 * set the PCNFA and PCNFB on configurations 1, 2 and 6, since the
	 * other ones do not have P bus decoders.
	 * However, is it safe to write them anyways - the values will be
	 * discarded. Just don't do this on a 187...
	 */
	if (brdtyp == BRD_188) {
		*(volatile u_int32_t *)MVME188_PCNFA = 0;
		*(volatile u_int32_t *)MVME188_PCNFB = 0;
	}

	/*
	 * Now set up addressing limits
	 */
	for (cmmu_num = 0, cmmu = m8820x_cmmu; cmmu_num < max_cmmus;
	    cmmu_num++, cmmu++) {
		num = cmmu_num >> 1;	/* CPU view of the CMMU */

		switch (cmmu_shift) {
		case 3:
			/*
			 * A14 split (configuration 2 only).
			 * CMMU numbers 0 and 1 match on A14 set,
			 *              2 and 3 on A14 clear
			 */
			cmmu->cmmu_addr |= (num < 2 ? CMMU_A14_MASK : 0);
			cmmu->cmmu_addr_mask |= CMMU_A14_MASK;
			/* FALLTHROUGH */

		case 2:
			/*
			 * A12 split.
			 * CMMU numbers 0 and 2 match on A12 set,
			 *              1 and 3 on A12 clear.
			 */
			cmmu->cmmu_addr |= (num & 1 ? 0 : CMMU_A12_MASK);
			cmmu->cmmu_addr_mask |= CMMU_A12_MASK;
			break;

		case 1:
			/*
			 * We don't need to set up anything for the hardwired
			 * configurations.
			 */
			cmmu->cmmu_addr = 0;
			cmmu->cmmu_addr_mask = 0;
			break;
		}

		/*
		 * If these CMMUs are 88204, these splitting address lines
		 * need to be shifted two bits.
		 */
		if (CMMU_TYPE(cmmu->cmmu_regs[CMMU_IDR]) == M88204_ID) {
			cmmu->cmmu_addr <<= 2;
			cmmu->cmmu_addr_mask <<= 2;
		}
	}
#endif
}

/*
 * Find out the CPU number from accessing CMMU.
 * On MVME187, there is only one CPU, so this is trivial.
 * On MVME188, we access the WHOAMI register, which is in data space;
 * its value will let us know which data CMMU has been used to perform
 * the read, and we can reliably compute the CPU number from it.
 */
cpuid_t
m8820x_cpu_number()
{
#ifdef MVME188
	u_int32_t whoami;
	cpuid_t cpu;
#endif

#ifdef MVME187
#ifdef MVME188
	if (brdtyp != BRD_188)
#endif
		return 0;
#endif

#ifdef MVME188
	whoami = *(volatile u_int32_t *)MVME188_WHOAMI;
	switch ((whoami & 0xf0) >> 4) {
	/* 2 CMMU per CPU multiprocessor modules */
	case 0:
	case 5:
		for (cpu = 0; cpu < 4; cpu++)
			if (whoami & (1 << cpu))
				return cpu;
		break;
	/* 4 CMMU per CPU dual processor modules */
	case 1:
		for (cpu = 0; cpu < 4; cpu++)
			if (whoami & (1 << cpu))
				return cpu >> 1;
		break;
	/* single processor modules */
	case 2:
	case 6:
	case 0x0a:
		return 0;
	}
	panic("can't figure out cpu number from whoami register %x", whoami);
#endif
}
