/*	$OpenBSD: m8820x.c,v 1.37 2004/08/04 13:16:14 miod Exp $	*/
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
/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/simplelock.h>

#include <machine/asm_macro.h>
#include <machine/cpu_number.h>
#include <machine/locore.h>

#include <machine/cmmu.h>
#include <machine/m8820x.h>
#ifdef MVME187
#include <machine/mvme187.h>
#endif
#ifdef MVME188
#include <machine/mvme188.h>
#endif

#include <uvm/uvm_extern.h>

#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#endif

/*
 * On some versions of the 88200, page size flushes don't work. I am using
 * sledge hammer approach till I find for sure which ones are bad XXX nivas
 */
#define BROKEN_MMU_MASK

u_int max_cmmus;

void m8820x_cmmu_init(void);
void m8820x_setup_board_config(void);
void m8820x_cpu_configuration_print(int);
void m8820x_cmmu_shutdown_now(void);
void m8820x_cmmu_parity_enable(void);
unsigned m8820x_cmmu_cpu_number(void);
void m8820x_cmmu_set_sapr(unsigned, unsigned);
void m8820x_cmmu_set_uapr(unsigned);
void m8820x_cmmu_set_pair_batc_entry(unsigned, unsigned, unsigned);
void m8820x_cmmu_flush_tlb(unsigned, unsigned, vaddr_t, vsize_t);
void m8820x_cmmu_pmap_activate(unsigned, unsigned,
    u_int32_t i_batc[BATC_MAX], u_int32_t d_batc[BATC_MAX]);
void m8820x_cmmu_flush_cache(int, paddr_t, psize_t);
void m8820x_cmmu_flush_inst_cache(int, paddr_t, psize_t);
void m8820x_cmmu_flush_data_cache(int, paddr_t, psize_t);
void m8820x_dma_cachectl(vaddr_t, vsize_t, int);
void m8820x_dma_cachectl_pa(paddr_t, psize_t, int);
void m8820x_cmmu_dump_config(void);
void m8820x_cmmu_show_translation(unsigned, unsigned, unsigned, int);
void m8820x_show_apr(unsigned);

/* This is the function table for the mc8820x CMMUs */
struct cmmu_p cmmu8820x = {
	m8820x_cmmu_init,
	m8820x_setup_board_config,
	m8820x_cpu_configuration_print,
	m8820x_cmmu_shutdown_now,
	m8820x_cmmu_parity_enable,
	m8820x_cmmu_cpu_number,
	m8820x_cmmu_set_sapr,
	m8820x_cmmu_set_uapr,
	m8820x_cmmu_set_pair_batc_entry,
	m8820x_cmmu_flush_tlb,
	m8820x_cmmu_pmap_activate,
	m8820x_cmmu_flush_cache,
	m8820x_cmmu_flush_inst_cache,
	m8820x_cmmu_flush_data_cache,
	m8820x_dma_cachectl,
	m8820x_dma_cachectl_pa,
#ifdef DDB
	m8820x_cmmu_dump_config,
	m8820x_cmmu_show_translation,
#else
	NULL,
	NULL,
#endif
#ifdef DEBUG
	m8820x_show_apr,
#else
	NULL,
#endif
};

/*
 * There are 6 possible MVME188 HYPERmodule configurations:
 *  - config 0: 4 CPUs, 8 CMMUs
 *  - config 1: 2 CPUs, 8 CMMUs
 *  - config 2: 1 CPUs, 8 CMMUs
 *  - config 5: 2 CPUs, 4 CMMUs
 *  - config 6: 1 CPU,  4 CMMUs
 *  - config A: 1 CPU,  2 CMMUs
 * which can exist either with MC88200 or MC88204 CMMUs.
 *
 * Systems with more than 2 CMMUs per CPU use programmable split schemes,
 * through PCNFA (for code CMMUs) and PCNFB (for data CMMUs) configuration
 * registers.
 *
 * The following schemes are available:
 * - split on A12 address bit (A14 for 88204)
 * - split on supervisor/user access
 * - split on SRAM/non-SRAM addresses, with either supervisor-only or all
 *   access to SRAM.
 *
 * Configuration 6, with 4 CMMUs par CPU, also forces a split on A14 address
 * bit.
 *
 * Under OpenBSD, we will only split on A12 and A14 address bits, since we
 * do not want to waste CMMU resources on the SRAM, and user/supervisor
 * splits seem less efficient.
 *
 * The really nasty part of this choice is in the exception handling code,
 * when it needs to get error information from up to 4 CMMUs. See eh.S for
 * the gory details.
 */

/*
 * CMMU kernel information
 */
struct m8820x_cmmu {
	volatile u_int32_t *cmmu_regs;	/* CMMU "base" area */
	unsigned int	cmmu_cpu;	/* cpu number it is attached to */
	unsigned int	cmmu_type;
#define	NO_CMMU	0x00
#define	INST_CMMU	0x01
#define	DATA_CMMU	0x02
	vaddr_t		cmmu_addr;	/* address range */
	vaddr_t		cmmu_addr_mask;	/* address mask */
};

/*
 * Structure for accessing MMUS properly
 */

#define MAX_CMMUS	(2 * MAX_CPUS)		/* maximum cmmus on the board */

struct m8820x_cmmu m8820x_cmmu[MAX_CMMUS] = {
	/* address, cpu, mode, addr, mask */
	{ NULL, -1, INST_CMMU, 0, 0 },
	{ NULL, -1, DATA_CMMU, 0, 0 },
	{ NULL, -1, INST_CMMU, 0, 0 },
	{ NULL, -1, DATA_CMMU, 0, 0 },
	{ NULL, -1, INST_CMMU, 0, 0 },
	{ NULL, -1, DATA_CMMU, 0, 0 },
	{ NULL, -1, INST_CMMU, 0, 0 },
	{ NULL, -1, DATA_CMMU, 0, 0 }
};

unsigned int cmmu_shift;

#ifdef MVME188
/*
 * The following list describes the different MVME188 configurations
 * which are supported by this code.
 */
const struct board_config {
	int ncpus;
	int ncmmus;
} bd_config[16] = {
	{ 4, 8 },	/* 4P128 - 4P512 */
	{ 2, 8 },	/* 2P128 - 2P512 */
	{ 1, 8 },	/* 1P128 - 1P512 */
	{ 0, 0 },
	{ 0, 0 },
	{ 2, 4 },	/* 2P64  - 2P256 */
	{ 1, 4 },	/* 1P64  - 1P256 */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 1, 2 },	/* 1P32  - 1P128 */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};
#endif

/* local prototypes */
void m8820x_cmmu_set(int, unsigned, int, int, int, vaddr_t);
void m8820x_cmmu_wait(int);
void m8820x_cmmu_sync_cache(paddr_t, psize_t);
void m8820x_cmmu_sync_inval_cache(paddr_t, psize_t);
void m8820x_cmmu_inval_cache(paddr_t, psize_t);

/* Flags passed to m8820x_cmmu_set() */
#define MODE_VAL		0x01
#define ADDR_VAL		0x02

#ifdef DEBUG
void
m8820x_show_apr(unsigned value)
{
	printf("table @ 0x%x000", PG_PFNUM(value));
	if (value & CACHE_WT)
		printf(", writethrough");
	if (value & CACHE_GLOBAL)
		printf(", global");
	if (value & CACHE_INH)
		printf(", cache inhibit");
	if (value & APR_V)
		printf(", valid");
	printf("\n");
}
#endif

/*
 * This routine sets up the CPU/CMMU configuration.
 */
void
m8820x_setup_board_config()
{
	volatile unsigned *cr;
	int num, cmmu_num;
	int vme188_config;
#ifdef MVME188
	u_int32_t *volatile whoami;
#endif

	master_cpu = 0;	/* temp to get things going */
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		/* There is no WHOAMI reg on MVME187 - fake it... */
		vme188_config = 0x0a;
		m8820x_cmmu[0].cmmu_regs = (void *)SBC_CMMU_I;
		m8820x_cmmu[0].cmmu_cpu = 0;
		m8820x_cmmu[1].cmmu_regs = (void *)SBC_CMMU_D;
		m8820x_cmmu[1].cmmu_cpu = 0;
		m8820x_cmmu[2].cmmu_regs = (void *)NULL;
		m8820x_cmmu[3].cmmu_regs = (void *)NULL;
		m8820x_cmmu[4].cmmu_regs = (void *)NULL;
		m8820x_cmmu[5].cmmu_regs = (void *)NULL;
		m8820x_cmmu[6].cmmu_regs = (void *)NULL;
		m8820x_cmmu[7].cmmu_regs = (void *)NULL;
		break;
#endif /* MVME187 */
#ifdef MVME188
	case BRD_188:
		whoami = (u_int32_t *volatile)MVME188_WHOAMI;
		vme188_config = (*whoami & 0xf0) >> 4;
		m8820x_cmmu[0].cmmu_regs = (void *)VME_CMMU_I0;
		m8820x_cmmu[1].cmmu_regs = (void *)VME_CMMU_D0;
		m8820x_cmmu[2].cmmu_regs = (void *)VME_CMMU_I1;
		m8820x_cmmu[3].cmmu_regs = (void *)VME_CMMU_D1;
		m8820x_cmmu[4].cmmu_regs = (void *)VME_CMMU_I2;
		m8820x_cmmu[5].cmmu_regs = (void *)VME_CMMU_D2;
		m8820x_cmmu[6].cmmu_regs = (void *)VME_CMMU_I3;
		m8820x_cmmu[7].cmmu_regs = (void *)VME_CMMU_D3;
		break;
#endif /* MVME188 */
	}

	max_cpus = bd_config[vme188_config].ncpus;
	max_cmmus = bd_config[vme188_config].ncmmus;

	cmmu_shift = ff1(max_cmmus / max_cpus);

#ifdef MVME188
	if (bd_config[vme188_config].ncpus > 0) {
		/* 187 has a fixed configuration, no need to print it */
		if (brdtyp == BRD_188) {
			printf("MVME188 board configuration #%X "
			    "(%d CPUs %d CMMUs)\n",
			    vme188_config, max_cpus, max_cmmus);
		}
	} else {
		panic("unrecognized MVME%x board configuration #%X",
		    brdtyp, vme188_config);
	}
#endif

#ifdef DIAGNOSTIC
	/*
	 * Probe for available MMUs
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		cr = m8820x_cmmu[cmmu_num].cmmu_regs;
		if (badwordaddr((vaddr_t)cr) == 0) {
			int type;

			type = CMMU_TYPE(cr[CMMU_IDR]);
			if (type != M88200_ID && type != M88204_ID) {
				printf("WARNING: non M8820x circuit found "
				    "at CMMU address %p\n", cr);
				continue;	/* will probably die quickly */
			}
		} else
			m8820x_cmmu[cmmu_num].cmmu_type = NO_CMMU;
	}
#endif

	/*
	 * Now that we know which CMMUs are there, let's report on which
	 * CPU/CMMU sets seem complete (hopefully all)
	 */
	for (num = 0; num < max_cpus; num++) {
		int type;

		cpu_sets[num] = 1;   /* This cpu installed... */
		type = CMMU_TYPE(m8820x_cmmu[num << cmmu_shift].
		    cmmu_regs[CMMU_IDR]);

		printf("CPU%d is attached with %d MC8820%c CMMUs\n",
		    num, 1 << cmmu_shift, type == M88204_ID ? '4' : '0');
	}


#ifdef MVME188
	/*
	 * Configure CPU/CMMU strategy into PCNFA and PCNFB board registers.
	 * We should theoretically only set these on configurations 1, 2
	 * and 6, since the other ones do not have P bus decoders.
	 * However, is it safe to write them anyways - the values will be
	 * discarded. Just don't do this on a 187...
	 */
	if (brdtyp == BRD_188) {
		*(volatile unsigned long *)MVME188_PCNFA = 0;
		*(volatile unsigned long *)MVME188_PCNFB = 0;
	}
#endif

	/*
	 * Calculate the CMMU<->CPU connections
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		m8820x_cmmu[cmmu_num].cmmu_cpu =
		    (cmmu_num * max_cpus) / max_cmmus;
	}

	/*
	 * Now set up addressing limits
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		num = cmmu_num >> 1;	/* CPU view of the CMMU */

		switch (cmmu_shift) {
		case 3:
			/*
			 * A14 split (configuration 2 only).
			 * CMMU numbers 0 and 1 match on A14 set,
			 *              2 and 3 on A14 clear
			 */
			m8820x_cmmu[cmmu_num].cmmu_addr |=
			    (num < 2 ? CMMU_A14_MASK : 0);
			m8820x_cmmu[cmmu_num].cmmu_addr_mask |= CMMU_A14_MASK;
			/* FALLTHROUGH */

		case 2:
			/*
			 * A12 split.
			 * CMMU numbers 0 and 2 match on A12 set,
			 *              1 and 3 on A12 clear.
			 */
			m8820x_cmmu[cmmu_num].cmmu_addr |=
			    (num & 1 ? 0 : CMMU_A12_MASK);
			m8820x_cmmu[cmmu_num].cmmu_addr_mask |= CMMU_A12_MASK;
			break;

		case 1:
			/*
			 * We don't need to set up anything for the hardwired
			 * configurations.
			 */
			m8820x_cmmu[cmmu_num].cmmu_addr = 0;
			m8820x_cmmu[cmmu_num].cmmu_addr_mask = 0;
			break;
		}

		/*
		 * If these CMMUs are 88204, these splitting address lines
		 * need to be shifted two bits.
		 */
		if (CMMU_TYPE(m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_IDR]) ==
		    M88204_ID) {
			m8820x_cmmu[cmmu_num].cmmu_addr <<= 2;
			m8820x_cmmu[cmmu_num].cmmu_addr_mask <<= 2;
		}
	}
}

#ifdef DDB
void
m8820x_cmmu_dump_config()
{
#ifdef MVME188
	int cmmu_num;

#ifdef MVME187
	if (brdtyp != BRD_188)
		return;
#endif

	db_printf("Current CPU/CMMU configuration:\n");
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		db_printf("CMMU #%d: %s CMMU for CPU %d:\n addr 0x%08lx mask 0x%08lx\n",
		  cmmu_num,
		  (m8820x_cmmu[cmmu_num].cmmu_type == INST_CMMU) ? "inst" : "data",
		  m8820x_cmmu[cmmu_num].cmmu_cpu,
		  m8820x_cmmu[cmmu_num].cmmu_addr,
		  m8820x_cmmu[cmmu_num].cmmu_addr_mask);
	}
#endif /* MVME188 */
}
#endif	/* DDB */

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void
m8820x_cmmu_set(int reg, unsigned val, int flags, int cpu, int mode,
    vaddr_t addr)
{
	int mmu;

	/*
	 * We scan all CMMUs to find the matching ones and store the
	 * values there.
	 */
	for (mmu = cpu << cmmu_shift;
	    mmu < (cpu + 1) << cmmu_shift; mmu++) {
		if ((flags & MODE_VAL) != 0) {
			if (m8820x_cmmu[mmu].cmmu_type != mode)
				continue;
		}
#ifdef MVME188
		if ((flags & ADDR_VAL) != 0) {
			if (m8820x_cmmu[mmu].cmmu_addr_mask != 0 &&
			    (addr & m8820x_cmmu[mmu].cmmu_addr_mask) !=
			     m8820x_cmmu[mmu].cmmu_addr)
				continue;
		}
#endif
		m8820x_cmmu[mmu].cmmu_regs[reg] = val;
	}
}

/*
 * Force a read from the CMMU status register, thereby forcing execution to
 * stop until all pending CMMU operations are finished.
 * This is used by the various cache invalidation functions.
 */
void
m8820x_cmmu_wait(int cpu)
{
	int mmu;

	/*
	 * We scan all related CMMUs and read their status register.
	 */
	for (mmu = cpu << cmmu_shift;
	    mmu < (cpu + 1) << cmmu_shift; mmu++) {
#ifdef DEBUG
		if (m8820x_cmmu[mmu].cmmu_regs[CMMU_SSR] & CMMU_SSR_BE) {
			panic("cache flush failed!");
		}
#else
		/* force the read access, but do not issue this statement... */
		__asm__ __volatile__ ("|or r0, r0, %0" ::
		    "r" (m8820x_cmmu[mmu].cmmu_regs[CMMU_SSR]));
#endif
	}
}

const char *mmutypes[8] = {
	"Unknown (0)",
	"Unknown (1)",
	"Unknown (2)",
	"Unknown (3)",
	"Unknown (4)",
	"M88200 (16K)",
	"M88204 (64K)",
	"Unknown (7)"
};

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void
m8820x_cpu_configuration_print(int master)
{
	int pid = read_processor_identification_register();
	int proctype = (pid & 0xff00) >> 8;
	int procvers = (pid & 0xe) >> 1;
	int mmu, cpu = cpu_number();
	struct simplelock print_lock;

	if (master)
		simple_lock_init(&print_lock);

	simple_lock(&print_lock);

	printf("cpu%d: ", cpu);
	if (proctype != 0) {
		printf("unknown model arch 0x%x rev 0x%x\n",
		    proctype, procvers);
		simple_unlock(&print_lock);
		return;
	}

	printf("M88100 rev 0x%x", procvers);
#if 0	/* not useful yet */
#ifdef MVME188
	if (brdtyp == BRD_188)
		printf(", %s", master ? "master" : "slave");
#endif
#endif
	printf(", %d CMMU", 1 << cmmu_shift);

	for (mmu = cpu << cmmu_shift; mmu < (cpu + 1) << cmmu_shift;
	    mmu++) {
		int idr = m8820x_cmmu[mmu].cmmu_regs[CMMU_IDR];
		int mmuid = CMMU_TYPE(idr);

		if (mmu % 2 == 0)
			printf("\ncpu%d: ", cpu);
		else
			printf(", ");

		if (mmutypes[mmuid][0] == 'U')
			printf("unknown model id 0x%x", mmuid);
		else
			printf("%s", mmutypes[mmuid]);
		printf(" rev 0x%x, %scache",
		    CMMU_VERSION(idr),
		    m8820x_cmmu[mmu].cmmu_type == INST_CMMU ? "I" : "D");
	}
	printf("\n");

#ifndef ERRATA__XXX_USR
	{
		static int errata_warn = 0;

		if (proctype != 0 && procvers < 2) {
			if (!errata_warn++)
				printf("WARNING: M88100 bug workaround code "
				    "not enabled.\nPlease recompile the kernel "
				    "with option ERRATA__XXX_USR !\n");
		}
	}
#endif

	simple_unlock(&print_lock);
}

/*
 * CMMU initialization routine
 */
void
m8820x_cmmu_init()
{
	unsigned int line, cmmu_num;
	int cssp, cpu, type;
	u_int32_t apr;
	volatile unsigned *cr;

	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		if (m8820x_cmmu[cmmu_num].cmmu_type != NO_CMMU) {
			cr = m8820x_cmmu[cmmu_num].cmmu_regs;
			type = CMMU_TYPE(cr[CMMU_IDR]);

			/*
			 * Reset cache
			 */
			for (cssp = type == M88204_ID ? 3 : 0;
			    cssp >= 0; cssp--)
				for (line = 0; line <= 255; line++) {
					cr[CMMU_SAR] =
					    line << MC88200_CACHE_SHIFT;
					cr[CMMU_CSSP(cssp)] =
					    CMMU_CSSP_L5 | CMMU_CSSP_L4 |
					    CMMU_CSSP_L3 | CMMU_CSSP_L2 |
					    CMMU_CSSP_L1 | CMMU_CSSP_L0 |
					    CMMU_CSSP_VV(3, CMMU_VV_INVALID) |
					    CMMU_CSSP_VV(2, CMMU_VV_INVALID) |
					    CMMU_CSSP_VV(1, CMMU_VV_INVALID) |
					    CMMU_CSSP_VV(0, CMMU_VV_INVALID);
				}

			/*
			 * Set the SCTR, SAPR, and UAPR to some known state
			 */
			cr[CMMU_SCTR] &=
			    ~(CMMU_SCTR_PE | CMMU_SCTR_SE | CMMU_SCTR_PR);
			cr[CMMU_SAPR] = cr[CMMU_UAPR] =
			    ((0x00000 << PG_BITS) | CACHE_WT | CACHE_GLOBAL |
			    CACHE_INH) & ~APR_V;

			cr[CMMU_BWP0] = cr[CMMU_BWP1] =
			cr[CMMU_BWP2] = cr[CMMU_BWP3] =
			cr[CMMU_BWP4] = cr[CMMU_BWP5] =
			cr[CMMU_BWP6] = cr[CMMU_BWP7] = 0;
			cr[CMMU_SCR] = CMMU_FLUSH_CACHE_INV_ALL;
			__asm__ __volatile__ ("|or r0, r0, %0" ::
			    "r" (cr[CMMU_SSR]));
			cr[CMMU_SCR] = CMMU_FLUSH_SUPER_ALL;
			cr[CMMU_SCR] = CMMU_FLUSH_USER_ALL;
		}
	}

	/*
	 * Enable snooping on MVME188 only.
	 * Snooping is enabled for instruction cmmus as well so that
	 * we can share breakpoints.
	 */
#ifdef MVME188
	if (brdtyp == BRD_188) {
		for (cpu = 0; cpu < max_cpus; cpu++) {
			if (cpu_sets[cpu] == 0)
				continue;

			m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
			    DATA_CMMU, 0);
			m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
			    INST_CMMU, 0);

			m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
			    0, cpu, 0, 0);
			m8820x_cmmu_wait(cpu);
			/* Icache gets flushed just below */
		}
	}
#endif

	/*
	 * Enable instruction cache.
	 * Data cache can not be enabled at this point, because some device
	 * addresses can never be cached, and the no-caching zones are not
	 * set up yet.
	 */
	for (cpu = 0; cpu < max_cpus; cpu++) {
		if (cpu_sets[cpu] == 0)
			continue;

		apr = ((0x00000 << PG_BITS) | CACHE_WT | CACHE_GLOBAL)
		    & ~(CACHE_INH | APR_V);

		m8820x_cmmu_set(CMMU_SAPR, apr, MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
		    0, cpu, 0, 0);
		m8820x_cmmu_wait(cpu);
	}
}

/*
 * Just before poweroff or reset....
 */
void
m8820x_cmmu_shutdown_now()
{
	unsigned cmmu_num;
	volatile unsigned *cr;

	CMMU_LOCK;
	for (cmmu_num = 0; cmmu_num < MAX_CMMUS; cmmu_num++)
		if (m8820x_cmmu[cmmu_num].cmmu_type != NO_CMMU) {
			cr = m8820x_cmmu[cmmu_num].cmmu_regs;

			cr[CMMU_SCTR] &=
			    ~(CMMU_SCTR_PE | CMMU_SCTR_SE | CMMU_SCTR_PR);
			cr[CMMU_SAPR] = cr[CMMU_UAPR] =
			    ((0x00000 << PG_BITS) | CACHE_INH) &
			    ~(CACHE_WT | CACHE_GLOBAL | APR_V);
		}
	CMMU_UNLOCK;
}

/*
 * enable parity
 */
void
m8820x_cmmu_parity_enable()
{
	unsigned cmmu_num;
	volatile unsigned *cr;

	CMMU_LOCK;

	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++)
		if (m8820x_cmmu[cmmu_num].cmmu_type != NO_CMMU) {
			cr = m8820x_cmmu[cmmu_num].cmmu_regs;
			cr[CMMU_SCTR] |= CMMU_SCTR_PE;
		}

	CMMU_UNLOCK;
}

/*
 * Find out the CPU number from accessing CMMU.
 * On MVME187, there is only one CPU, so this is trivial.
 * On MVME188, we access the WHOAMI register, which is in data space;
 * its value will let us know which data CMMU has been used to perform
 * the read, and we can reliably compute the CPU number from it.
 */
unsigned
m8820x_cmmu_cpu_number()
{
#ifdef MVME188
	u_int32_t whoami;
	unsigned int cpu;
#endif

#ifdef MVME187
	if (brdtyp != BRD_188)
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

void
m8820x_cmmu_set_sapr(unsigned cpu, unsigned ap)
{
	CMMU_LOCK;
	m8820x_cmmu_set(CMMU_SAPR, ap, 0, cpu, 0, 0);
	CMMU_UNLOCK;
}

void
m8820x_cmmu_set_uapr(unsigned ap)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;
	/* this functionality also mimiced in m8820x_cmmu_pmap_activate() */
	m8820x_cmmu_set(CMMU_UAPR, ap, 0, cpu, 0, 0);
	CMMU_UNLOCK;
	splx(s);
}

/*
 * Set batc entry number entry_no to value in
 * the data and instruction cache for the named CPU.
 *
 * Except for the cmmu_init, this function and m8820x_cmmu_pmap_activate
 * are the only functions which may set the batc values.
 */
void
m8820x_cmmu_set_pair_batc_entry(unsigned cpu, unsigned entry_no, unsigned value)
{
	CMMU_LOCK;

	m8820x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL, cpu, DATA_CMMU, 0);
	m8820x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL, cpu, INST_CMMU, 0);

	CMMU_UNLOCK;
}

/*
 * Functions that invalidate TLB entries.
 */

/*
 *	flush any tlb
 *	Some functionality mimiced in m8820x_cmmu_pmap_activate.
 */
void
m8820x_cmmu_flush_tlb(unsigned cpu, unsigned kernel, vaddr_t vaddr,
    vsize_t size)
{
	int s = splhigh();

	CMMU_LOCK;

	/*
	 * Since segment operations are horribly expensive, don't
	 * do any here. Invalidations of up to three pages are performed
	 * as page invalidations, otherwise the entire tlb is flushed.
	 *
	 * Note that this code relies upon size being a multiple of
	 * a page and vaddr being page-aligned.
	 */
	if (size == PAGE_SIZE) {	/* most frequent situation */
		m8820x_cmmu_set(CMMU_SAR, vaddr,
		    ADDR_VAL, cpu, 0, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL, cpu, 0, vaddr);
	} else if (size > 3 * PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_ALL : CMMU_FLUSH_USER_ALL,
		    0, cpu, 0, 0);
	} else
	while (size != 0) {
		m8820x_cmmu_set(CMMU_SAR, vaddr,
		    ADDR_VAL, cpu, 0, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL, cpu, 0, vaddr);

		size -= PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}

	CMMU_UNLOCK;
	splx(s);
}

/*
 * New fast stuff for pmap_activate.
 * Does what a few calls used to do.
 * Only called from pmap_activate().
 */
void
m8820x_cmmu_pmap_activate(unsigned cpu, unsigned uapr, u_int32_t i_batc[],
    u_int32_t d_batc[])
{
	int entry_no;

	CMMU_LOCK;

	/* the following is from m8820x_cmmu_set_uapr */
	m8820x_cmmu_set(CMMU_UAPR, uapr, 0, cpu, 0, 0);

	for (entry_no = 0; entry_no < BATC_MAX; entry_no++) {
		m8820x_cmmu_set(CMMU_BWP(entry_no), i_batc[entry_no],
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_BWP(entry_no), d_batc[entry_no],
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}

	/*
	 * Flush the user TLB.
	 * IF THE KERNEL WILL EVER CARE ABOUT THE BATC ENTRIES,
	 * THE SUPERVISOR TLBs SHOULD BE FLUSHED AS WELL.
	 */
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_USER_ALL, 0, cpu, 0, 0);

	CMMU_UNLOCK;
}

/*
 * Functions that invalidate caches.
 *
 * Cache invalidates require physical addresses.  Care must be exercised when
 * using segment invalidates.  This implies that the starting physical address
 * plus the segment length should be invalidated.  A typical mistake is to
 * extract the first physical page of a segment from a virtual address, and
 * then expecting to invalidate when the pages are not physically contiguous.
 *
 * We don't push Instruction Caches prior to invalidate because they are not
 * snooped and never modified (I guess it doesn't matter then which form
 * of the command we use then).
 */

/*
 *	flush both Instruction and Data caches
 */
void
m8820x_cmmu_flush_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0,
		    cpu, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
		    cpu, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE, ADDR_VAL,
		    cpu, 0, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
		    cpu, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE, ADDR_VAL,
		    cpu, 0, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, 0,
		    cpu, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, 0,
		    cpu, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0, cpu, 0, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 *	flush Instruction caches
 */
void
m8820x_cmmu_flush_inst_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_flush_data_cache(int cpu, paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL, cpu, DATA_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

/*
 * sync dcache (and icache too)
 */
void
m8820x_cmmu_sync_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_sync_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_cmmu_inval_cache(paddr_t physaddr, psize_t size)
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;

#if !defined(BROKEN_MMU_MASK)
	if (size > NBSG) {
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
		    cpu, DATA_CMMU, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0);
#endif /* !BROKEN_MMU_MASK */

	m8820x_cmmu_wait(cpu);

	CMMU_UNLOCK;
	splx(s);
}

void
m8820x_dma_cachectl(vaddr_t va, vsize_t size, int op)
{
	paddr_t pa;
#if !defined(BROKEN_MMU_MASK)
	psize_t count;

	while (size != 0) {
		count = NBPG - (va & PGOFSET);

		if (size < count)
			count = size;

		if (pmap_extract(pmap_kernel(), va, &pa) != FALSE) {
			switch (op) {
			case DMA_CACHE_SYNC:
				m8820x_cmmu_sync_cache(pa, count);
				break;
			case DMA_CACHE_SYNC_INVAL:
				m8820x_cmmu_sync_inval_cache(pa, count);
				break;
			default:
				m8820x_cmmu_inval_cache(pa, count);
				break;
			}
		}

		va += count;
		size -= count;
	}
#else
	/* XXX This assumes the space is also physically contiguous */
	if (pmap_extract(pmap_kernel(), va, &pa) != FALSE) {
		switch (op) {
		case DMA_CACHE_SYNC:
			m8820x_cmmu_sync_cache(pa, size);
			break;
		case DMA_CACHE_SYNC_INVAL:
			m8820x_cmmu_sync_inval_cache(pa, size);
			break;
		default:
			m8820x_cmmu_inval_cache(pa, size);
			break;
		}
	}
#endif /* !BROKEN_MMU_MASK */
}

void
m8820x_dma_cachectl_pa(paddr_t pa, psize_t size, int op)
{
#if !defined(BROKEN_MMU_MASK)
	psize_t count;

	while (size != 0) {
		count = NBPG - (va & PGOFSET);

		if (size < count)
			count = size;

		switch (op) {
		case DMA_CACHE_SYNC:
			m8820x_cmmu_sync_cache(pa, count);
			break;
		case DMA_CACHE_SYNC_INVAL:
			m8820x_cmmu_sync_inval_cache(pa, count);
			break;
		default:
			m8820x_cmmu_inval_cache(pa, count);
			break;
		}

		pa += count;
		size -= count;
	}
#else
	switch (op) {
	case DMA_CACHE_SYNC:
		m8820x_cmmu_sync_cache(pa, size);
		break;
	case DMA_CACHE_SYNC_INVAL:
		m8820x_cmmu_sync_inval_cache(pa, size);
		break;
	default:
		m8820x_cmmu_inval_cache(pa, size);
		break;
	}
#endif /* !BROKEN_MMU_MASK */
}

#ifdef DDB
/*
 * Show (for debugging) how the current CPU translates the given ADDRESS
 * (as DATA).
 */
void
m8820x_cmmu_show_translation(unsigned address, unsigned supervisor_flag,
    unsigned verbose_flag, int unused __attribute__ ((unused)))
{
	int cpu = cpu_number();
	vaddr_t va = address;
	int cmmu_num;
	u_int32_t value;

	/*
	 * Find the correct data CMMU.
	 */
	for (cmmu_num = cpu << cmmu_shift;
	    cmmu_num < (cpu + 1) << cmmu_shift; cmmu_num++) {
		if (m8820x_cmmu[cmmu_num].cmmu_type != DATA_CMMU)
			continue;
		if (m8820x_cmmu[cmmu_num].cmmu_addr_mask == 0 ||
		    (va & m8820x_cmmu[cmmu_num].cmmu_addr_mask) ==
		     m8820x_cmmu[cmmu_num].cmmu_addr)
			break;
	}
	if (cmmu_num == (cpu + 1) << cmmu_shift) {
		db_printf("No matching cmmu for VA %08x\n", address);
		return;
	}

	if (verbose_flag != 0)
		db_printf("VA %08x is managed by CMMU#%d.\n",
		    address, cmmu_num);

	/*
	 * Perform some sanity checks.
	 */
	if (verbose_flag == 0) {
		if ((m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_SCTR] &
		    CMMU_SCTR_SE) == 0)
			db_printf("WARNING: snooping not enabled for CMMU#%d.\n",
			    cmmu_num);
	} else {
		int i;

		for (i = 0; i < max_cmmus; i++)
			if (m8820x_cmmu[i].cmmu_type != NO_CMMU &&
			    (verbose_flag > 1 ||
			     (m8820x_cmmu[i].cmmu_regs[CMMU_SCTR] &
			      CMMU_SCTR_SE) == 0)) {
				db_printf("CMMU#%d (cpu %d %s) snooping %s\n",
				    i, m8820x_cmmu[i].cmmu_cpu,
				    m8820x_cmmu[i].cmmu_type ? "data" : "inst",
				    (m8820x_cmmu[i].cmmu_regs[CMMU_SCTR] &
				     CMMU_SCTR_SE) ? "on":"OFF");
			}
	}

	/*
	 * Ask for a CMMU probe and report its result.
	 */
	{
		volatile unsigned *cmmu_regs = m8820x_cmmu[cmmu_num].cmmu_regs;
		u_int32_t ssr;

		cmmu_regs[CMMU_SAR] = address;
		cmmu_regs[CMMU_SCR] =
		    supervisor_flag ? CMMU_PROBE_SUPER : CMMU_PROBE_USER;
		ssr = cmmu_regs[CMMU_SSR];

		switch (verbose_flag) {
		case 2:
			db_printf("probe of 0x%08x returns ssr=0x%08x\n",
			    address, ssr);
			/* FALLTHROUGH */
		case 1:
			if (ssr & CMMU_SSR_V)
				db_printf("PROBE of 0x%08x returns phys=0x%x",
				    address, cmmu_regs[CMMU_SAR]);
			else
				db_printf("PROBE fault at 0x%x",
				    cmmu_regs[CMMU_PFAR]);
			if (ssr & CMMU_SSR_CE)
				db_printf(", copyback err");
			if (ssr & CMMU_SSR_BE)
				db_printf(", bus err");
			if (ssr & CACHE_WT)
				db_printf(", writethrough");
			if (ssr & CMMU_SSR_SO)
				db_printf(", sup prot");
			if (ssr & CACHE_GLOBAL)
				db_printf(", global");
			if (ssr & CACHE_INH)
				db_printf(", cache inhibit");
			if (ssr & CMMU_SSR_M)
				db_printf(", modified");
			if (ssr & CMMU_SSR_U)
				db_printf(", used");
			if (ssr & CMMU_SSR_PROT)
				db_printf(", write prot");
			if (ssr & CMMU_SSR_BH)
				db_printf(", BATC");
			db_printf(".\n");
			break;
		}
	}

	/*
	 * Interpret area descriptor.
	 */

	if (supervisor_flag)
		value = m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_SAPR];
	else
		value = m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_UAPR];

	switch (verbose_flag) {
	case 2:
		db_printf("CMMU#%d", cmmu_num);
		db_printf(" %cAPR is 0x%08x\n",
		    supervisor_flag ? 'S' : 'U', value);
		/* FALLTHROUGH */
	case 1:
		db_printf("CMMU#%d", cmmu_num);
		db_printf(" %cAPR: SegTbl: 0x%x000p",
		    supervisor_flag ? 'S' : 'U', PG_PFNUM(value));
		if (value & CACHE_WT)
			db_printf(", WTHRU");
		if (value & CACHE_GLOBAL)
			db_printf(", GLOBAL");
		if (value & CACHE_INH)
			db_printf(", INHIBIT");
		if (value & APR_V)
			db_printf(", VALID");
		db_printf("\n");
		break;
	}

	if ((value & APR_V) == 0) {
		db_printf("VA 0x%08x -> apr 0x%08x not valid\n", va, value);
		return;
	}

	value &= PG_FRAME;	/* now point to seg page */

	/*
	 * Walk segment and page tables to find our page.
	 */
	{
		sdt_entry_t sdt;

		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
			    SDTIDX(va), value);
		value |= SDTIDX(va) * sizeof(sdt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("VA 0x%08x -> segment table @0x%08x not accessible\n",
			    va, value);
			return;
		}

		sdt = *(sdt_entry_t *)value;
		switch (verbose_flag) {
		case 2:
			db_printf("SEG DESC @0x%x is 0x%08x\n", value, sdt);
			/* FALLTHROUGH */
		case 1:
			db_printf("SEG DESC @0x%x: PgTbl: 0x%x000",
			    value, PG_PFNUM(sdt));
			if (sdt & CACHE_WT)
				db_printf(", WTHRU");
			if (sdt & SG_SO)
				db_printf(", S-PROT");
			if (sdt & CACHE_GLOBAL)
				db_printf(", GLOBAL");
			if (sdt & CACHE_INH)
				db_printf(", $INHIBIT");
			if (sdt & SG_PROT)
				db_printf(", W-PROT");
			if (sdt & SG_V)
				db_printf(", VALID");
			db_printf(".\n");
			break;
		}

		if ((sdt & SG_V) == 0) {
			db_printf("VA 0x%08x -> segment entry 0x%8x @0x%08x not valid\n",
			    va, sdt, value);
			return;
		}

		value = ptoa(PG_PFNUM(sdt));
	}

	{
		pt_entry_t pte;

		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
			    PDTIDX(va), value);
		value |= PDTIDX(va) * sizeof(pt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("VA 0x%08x -> page table entry @0x%08x not accessible\n",
			    va, value);
			return;
		}

		pte = *(pt_entry_t *)value;
		switch (verbose_flag) {
		case 2:
			db_printf("PAGE DESC @0x%x is 0x%08x.\n", value, pte);
			/* FALLTHROUGH */
		case 1:
			db_printf("PAGE DESC @0x%x: page @%x000",
			    value, PG_PFNUM(pte));
			if (pte & PG_W)
				db_printf(", WIRE");
			if (pte & CACHE_WT)
				db_printf(", WTHRU");
			if (pte & PG_SO)
				db_printf(", S-PROT");
			if (pte & CACHE_GLOBAL)
				db_printf(", GLOBAL");
			if (pte & CACHE_INH)
				db_printf(", $INHIBIT");
			if (pte & PG_M)
				db_printf(", MOD");
			if (pte & PG_U)
				db_printf(", USED");
			if (pte & PG_PROT)
				db_printf(", W-PROT");
			if (pte & PG_V)
				db_printf(", VALID");
			db_printf(".\n");
			break;
		}

		if ((pte & PG_V) == 0) {
			db_printf("VA 0x%08x -> page table entry 0x%08x @0x%08x not valid\n",
			    va, pte, value);
			return;
		}

		value = ptoa(PG_PFNUM(pte)) | (va & PAGE_MASK);
	}

	db_printf("VA 0x%08x -> PA 0x%08x\n", va, value);
}
#endif /* DDB */
