/*	$OpenBSD: m8820x.c,v 1.2 2004/05/07 18:06:39 miod Exp $	*/
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
#include <machine/board.h>
#include <machine/cpu_number.h>
#include <machine/locore.h>

#include <machine/cmmu.h>
#include <machine/m8820x.h>

#include <uvm/uvm_extern.h>

#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#endif /* DDB */

/*
 * On some versions of the 88200, page size flushes don't work. I am using
 * sledge hammer approach till I find for sure which ones are bad XXX nivas
 */
#define BROKEN_MMU_MASK

#undef	SHADOW_BATC		/* don't use BATCs for now XXX nivas */

#ifdef DEBUG
unsigned int m8820x_debuglevel;
#define dprintf(_X_) \
	do { \
		if (m8820x_debuglevel != 0) { \
			unsigned int psr = disable_interrupts_return_psr(); \
			printf("%d: ", cpu_number()); \
			printf _X_;  \
			set_psr(psr); \
		} \
	} while (0)
#else
#define dprintf(_X_) do { } while (0)
#endif

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
 * CMMU kernel information
 */
struct m8820x_cmmu {
	unsigned *volatile cmmu_regs;	/* CMMU "base" area */
	unsigned int	cmmu_cpu;	/* cpu number it is attached to */
	unsigned int	cmmu_type;
#define	INST_CMMU	0
#define	DATA_CMMU	1
	unsigned int	cmmu_access;
#define	CMMU_ACS_USER	0
#define	CMMU_ACS_SUPER	1
#define	CMMU_ACS_BOTH	2
	unsigned int	cmmu_alive;
#define CMMU_DEAD	0		/* This cmmu is not there */
#define CMMU_AVAILABLE	1		/* It's there, but which cpu's? */
#define CMMU_MARRIED	2		/* Know which cpu it belongs to. */
	vaddr_t		cmmu_addr;	/* address range */
	vaddr_t		cmmu_addr_mask;	/* address mask */
	int		cmmu_addr_match;/* return value of address comparison */
#ifdef SHADOW_BATC
	unsigned batc[BATC_MAX];
#endif
};

#ifdef SHADOW_BATC
/* CMMU(cpu,data) is the cmmu struct for the named cpu's indicated cmmu.  */
#define CMMU(cpu, data) cpu_cmmu[(cpu)].pair[(data) ? DATA_CMMU : INST_CMMU]
#endif

/*
 * Structure for accessing MMUS properly
 */

struct m8820x_cmmu m8820x_cmmu[MAX_CMMUS] =
{
	/* address, cpu, mode, access, alive, addr, mask */
	{(unsigned *volatile)CMMU_I0, -1, INST_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_D0, -1, DATA_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_I1, -1, INST_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_D1, -1, DATA_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_I2, -1, INST_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_D2, -1, DATA_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_I3, -1, INST_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0},
	{(unsigned *volatile)CMMU_D3, -1, DATA_CMMU, CMMU_ACS_BOTH, CMMU_DEAD, 0, 0}
};

struct cpu_cmmu {
	struct m8820x_cmmu *pair[2];
} cpu_cmmu[MAX_CPUS];

/*
 * CMMU per CPU split strategies
 */

#define	CMMU_SPLIT_ADDRESS	0x00
#define	CMMU_SPLIT_SPV		0x01
#define	CMMU_SPLIT_SRAM_SPV	0x02
#define	CMMU_SPLIT_SRAM_ALL	0x03

#define	CMMU_SPLIT_MASK		0x03

struct cmmu_strategy {
	int inst;
	int data;
} cpu_cmmu_strategy[] = {
	/*     inst                 data */
	{ CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS},	/* CPU 0 */
	{ CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS},	/* CPU 1 */
	{ CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS},	/* CPU 2 */
	{ CMMU_SPLIT_ADDRESS,  CMMU_SPLIT_ADDRESS}	/* CPU 3 */
};

unsigned int cmmu_shift;

/* local prototypes */
void m8820x_cmmu_set(int, unsigned, int, int, int, int, vaddr_t);
void m8820x_cmmu_wait(int);
void m8820x_cmmu_sync_cache(paddr_t, psize_t);
void m8820x_cmmu_sync_inval_cache(paddr_t, psize_t);
void m8820x_cmmu_inval_cache(paddr_t, psize_t);

/* Flags passed to m8820x_cmmu_set() */
#define MODE_VAL		0x01
#define ACCESS_VAL		0x02
#define ADDR_VAL		0x04

#ifdef DEBUG
void
m8820x_show_apr(value)
	unsigned value;
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
	int num, cmmu_num;
	unsigned *volatile cr;

	master_cpu = 0;	/* temp to get things going */
	max_cpus = 4;
	max_cmmus = 8;

	cmmu_shift = ff1(max_cmmus / max_cpus);

	/*
	 * Probe for available MMUs
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		cr = m8820x_cmmu[cmmu_num].cmmu_regs;
		if (badwordaddr((vaddr_t)cr) == 0) {
			int type;

			type = CMMU_TYPE(cr[CMMU_IDR]);
#ifdef DIAGNOSTIC
			if (type != M88200_ID && type != M88204_ID) {
				printf("WARNING: non M8820x circuit found "
				    "at CMMU address %p\n", cr);
				continue;	/* will probably die quickly */
			}
#endif
			m8820x_cmmu[cmmu_num].cmmu_alive = CMMU_AVAILABLE;
			dprintf(("m8820x_setup_cmmu_config: CMMU %d found at %p\n",
			    cmmu_num, cr));
		}
	}

	/*
	 * Now that we know which CMMUs are there, let's report on which
	 * CPU/CMMU sets seem complete (hopefully all)
	 */
	for (num = 0; num < max_cpus; num++) {
		int i, type;

		for (i = 0; i < (1 << cmmu_shift); i++) {
			dprintf(("cmmu_init: testing CMMU %d for CPU %d\n",
			    (num << cmmu_shift) | i, num));
#ifdef DIAGNOSTIC
			if (m8820x_cmmu[(num << cmmu_shift) | i].cmmu_alive == CMMU_DEAD) {
				printf("CMMU %d attached to CPU %d is not working\n",
				    (num << cmmu_shift) | i, num);
				continue;	/* will probably die quickly */
			}
#endif
		}
		cpu_sets[num] = 1;   /* This cpu installed... */
		type = CMMU_TYPE(m8820x_cmmu[num << cmmu_shift].
		    cmmu_regs[CMMU_IDR]);

		printf("CPU%d is attached with %d MC%x CMMUs\n",
		    num, 1 << cmmu_shift, type == M88204_ID ? 0x88204 : 0x88200);
	}

	/*
	 * Calculate the CMMU<->CPU connections
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		m8820x_cmmu[cmmu_num].cmmu_cpu =
		    (cmmu_num * max_cpus) / max_cmmus;
		dprintf(("m8820x_setup_cmmu_config: CMMU %d connected with CPU %d\n",
		    cmmu_num, m8820x_cmmu[cmmu_num].cmmu_cpu));
	}

	/*
	 * Now set m8820x_cmmu[].cmmu_access and addr
	 */
	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		/*
		 * We don't set up anything for the hardwired configurations.
		 */
		m8820x_cmmu[cmmu_num].cmmu_addr = 0;
		m8820x_cmmu[cmmu_num].cmmu_addr_mask = 0;
		m8820x_cmmu[cmmu_num].cmmu_addr_match = 1;
		m8820x_cmmu[cmmu_num].cmmu_access = CMMU_ACS_BOTH;
	}
}

#ifdef DDB

const char *cmmu_strat_string[] = {
	"address split ",
	"user/spv split",
	"spv SRAM split",
	"all SRAM split"
};

void
m8820x_cmmu_dump_config()
{
	int cmmu_num;

	db_printf("Current CPU/CMMU configuration:\n");

	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		db_printf("CMMU #%d: %s CMMU for CPU %d:\n Strategy: %s\n %s access addr 0x%08lx mask 0x%08lx match %s\n",
		  cmmu_num,
		  (m8820x_cmmu[cmmu_num].cmmu_type == INST_CMMU) ? "inst" : "data",
		  m8820x_cmmu[cmmu_num].cmmu_cpu,
		  cmmu_strat_string[(m8820x_cmmu[cmmu_num].cmmu_type == INST_CMMU) ?
		    cpu_cmmu_strategy[m8820x_cmmu[cmmu_num].cmmu_cpu].inst :
		    cpu_cmmu_strategy[m8820x_cmmu[cmmu_num].cmmu_cpu].data],
		  (m8820x_cmmu[cmmu_num].cmmu_access == CMMU_ACS_BOTH) ?   "User and spv" :
		  ((m8820x_cmmu[cmmu_num].cmmu_access == CMMU_ACS_USER) ? "User        " :
		   "Supervisor  "),
		  m8820x_cmmu[cmmu_num].cmmu_addr,
		  m8820x_cmmu[cmmu_num].cmmu_addr_mask,
		  m8820x_cmmu[cmmu_num].cmmu_addr_match ? "TRUE" : "FALSE");
	}
}
#endif	/* DDB */

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void
m8820x_cmmu_set(reg, val, flags, num, mode, access, addr)
	int reg;
	unsigned val;
	int flags, num, mode, access;
	vaddr_t addr;
{
	int mmu;

	/*
	 * We scan all CMMUs to find the matching ones and store the
	 * values there.
	 */
	for (mmu = num << cmmu_shift;
	    mmu < (num + 1) << cmmu_shift; mmu++) {
		if (((flags & MODE_VAL)) &&
		    (m8820x_cmmu[mmu].cmmu_type != mode))
			continue;
		if (((flags & ACCESS_VAL)) &&
		    (m8820x_cmmu[mmu].cmmu_access != access) &&
		    (m8820x_cmmu[mmu].cmmu_access != CMMU_ACS_BOTH))
			continue;
		if (flags & ADDR_VAL) {
			if (((addr & m8820x_cmmu[mmu].cmmu_addr_mask) == m8820x_cmmu[mmu].cmmu_addr)
			    != m8820x_cmmu[mmu].cmmu_addr_match) {
				continue;
			}
		}
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
m8820x_cpu_configuration_print(master)
	int master;
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
		int access = m8820x_cmmu[mmu].cmmu_access;

		if (mmu % 2 == 0)
			printf("\ncpu%d: ", cpu);
		else
			printf(", ");

		if (mmutypes[mmuid][0] == 'U')
			printf("unknown model id 0x%x", mmuid);
		else
			printf("%s", mmutypes[mmuid]);
		printf(" rev 0x%x, %s %scache",
		    CMMU_VERSION(idr),
		    access == CMMU_ACS_BOTH ? "global" :
		    (access == CMMU_ACS_USER ? "user" : "sup"),
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
	unsigned *volatile cr;

	for (cpu = 0; cpu < max_cpus; cpu++) {
		cpu_cmmu[cpu].pair[INST_CMMU] = 0;
		cpu_cmmu[cpu].pair[DATA_CMMU] = 0;
	}

	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++) {
		if (m8820x_cmmu[cmmu_num].cmmu_alive != CMMU_DEAD) {
			cr = m8820x_cmmu[cmmu_num].cmmu_regs;
			type = CMMU_TYPE(cr[CMMU_IDR]);

			cpu_cmmu[m8820x_cmmu[cmmu_num].cmmu_cpu].
			  pair[m8820x_cmmu[cmmu_num].cmmu_type] =
			    &m8820x_cmmu[cmmu_num];

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

#ifdef SHADOW_BATC
			m8820x_cmmu[cmmu_num].batc[0] =
			m8820x_cmmu[cmmu_num].batc[1] =
			m8820x_cmmu[cmmu_num].batc[2] =
			m8820x_cmmu[cmmu_num].batc[3] =
			m8820x_cmmu[cmmu_num].batc[4] =
			m8820x_cmmu[cmmu_num].batc[5] =
			m8820x_cmmu[cmmu_num].batc[6] =
			m8820x_cmmu[cmmu_num].batc[7] = 0;
#endif
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

	for (cpu = 0; cpu < max_cpus; cpu++) {
		if (cpu_sets[cpu] == 0)
			continue;

		m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
		    DATA_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCTR, CMMU_SCTR_SE, MODE_VAL, cpu,
		    INST_CMMU, 0, 0);

		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
		    ACCESS_VAL, cpu, 0, CMMU_ACS_SUPER, 0);
		m8820x_cmmu_wait(cpu);
		/* Icache gets flushed just below */
	}

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

		m8820x_cmmu_set(CMMU_SAPR, apr, MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_SUPER_ALL,
		    ACCESS_VAL, cpu, 0, CMMU_ACS_SUPER, 0);
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
	unsigned *volatile cr;

	CMMU_LOCK;
	for (cmmu_num = 0; cmmu_num < MAX_CMMUS; cmmu_num++)
		if (m8820x_cmmu[cmmu_num].cmmu_alive != CMMU_DEAD) {
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
	unsigned *volatile cr;

	CMMU_LOCK;

	for (cmmu_num = 0; cmmu_num < max_cmmus; cmmu_num++)
		if (m8820x_cmmu[cmmu_num].cmmu_alive != CMMU_DEAD) {
			cr = m8820x_cmmu[cmmu_num].cmmu_regs;
			cr[CMMU_SCTR] |= CMMU_SCTR_PE;
		}

	CMMU_UNLOCK;
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */
#define ILLADDRESS	0x3ffffff0 	/* any faulty address for luna88k2 */

unsigned
m8820x_cmmu_cpu_number()
{
	unsigned cmmu_no;
	int i, cpu;

	CMMU_LOCK;

	for (i = 0; i < 10; i++) {
		/* clear CMMU p-bus status registers */
		for (cmmu_no = 0; cmmu_no < MAX_CMMUS; cmmu_no++) {
			if (m8820x_cmmu[cmmu_no].cmmu_alive == CMMU_AVAILABLE &&
			    m8820x_cmmu[cmmu_no].cmmu_type == DATA_CMMU)
				m8820x_cmmu[cmmu_no].cmmu_regs[CMMU_PFSR] = 0;
		}

		/* access faulting address */
		badwordaddr((vaddr_t)ILLADDRESS);

		/* check which CMMU reporting the fault  */
		for (cmmu_no = 0; cmmu_no < MAX_CMMUS; cmmu_no++) {
			if (m8820x_cmmu[cmmu_no].cmmu_alive == CMMU_AVAILABLE &&
			    m8820x_cmmu[cmmu_no].cmmu_type == DATA_CMMU &&
			    CMMU_PFSR_FAULT(m8820x_cmmu[cmmu_no].
			      cmmu_regs[CMMU_PFSR]) != CMMU_PFSR_SUCCESS) {
				/* clean register, just in case... */
				m8820x_cmmu[cmmu_no].cmmu_regs[CMMU_PFSR] = 0;
				m8820x_cmmu[cmmu_no].cmmu_alive = CMMU_MARRIED;
				cpu = m8820x_cmmu[cmmu_no].cmmu_cpu;
				CMMU_UNLOCK;
				return cpu;
			}
		}
	}
	CMMU_UNLOCK;

	panic("m8820x_cmmu_cpu_number: could not determine my cpu number");
}

void
m8820x_cmmu_set_sapr(cpu, ap)
	unsigned cpu, ap;
{
	CMMU_LOCK;
	m8820x_cmmu_set(CMMU_SAPR, ap, ACCESS_VAL, cpu, 0, CMMU_ACS_SUPER, 0);
	CMMU_UNLOCK;
}

void
m8820x_cmmu_set_uapr(ap)
	unsigned ap;
{
	int s = splhigh();
	int cpu = cpu_number();

	CMMU_LOCK;
	/* this functionality also mimiced in m8820x_cmmu_pmap_activate() */
	m8820x_cmmu_set(CMMU_UAPR, ap, ACCESS_VAL, cpu, 0, CMMU_ACS_USER, 0);
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
m8820x_cmmu_set_pair_batc_entry(cpu, entry_no, value)
	unsigned cpu, entry_no;
	unsigned value;	/* the value to stuff into the batc */
{
	CMMU_LOCK;

	m8820x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL | ACCESS_VAL,
	    cpu, DATA_CMMU, CMMU_ACS_USER, 0);
#ifdef SHADOW_BATC
	CMMU(cpu,DATA_CMMU)->batc[entry_no] = value;
#endif
	m8820x_cmmu_set(CMMU_BWP(entry_no), value, MODE_VAL | ACCESS_VAL,
	    cpu, INST_CMMU, CMMU_ACS_USER, 0);
#ifdef SHADOW_BATC
	CMMU(cpu,INST_CMMU)->batc[entry_no] = value;
#endif

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
		    ADDR_VAL | ACCESS_VAL, cpu, 0,
		    kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL | ACCESS_VAL, cpu, 0,
		    kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);
	} else if (size > 3 * PAGE_SIZE) {
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_ALL : CMMU_FLUSH_USER_ALL,
		    ACCESS_VAL, cpu, 0,
		    kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, 0);
	} else
	while (size != 0) {
		m8820x_cmmu_set(CMMU_SAR, vaddr,
		    ADDR_VAL | ACCESS_VAL, cpu, 0,
		    kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);
		m8820x_cmmu_set(CMMU_SCR,
		    kernel ? CMMU_FLUSH_SUPER_PAGE : CMMU_FLUSH_USER_PAGE,
		    ADDR_VAL | ACCESS_VAL, cpu, 0,
		    kernel ? CMMU_ACS_SUPER : CMMU_ACS_USER, vaddr);

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
m8820x_cmmu_pmap_activate(cpu, uapr, i_batc, d_batc)
	unsigned cpu, uapr;
	u_int32_t i_batc[BATC_MAX];
	u_int32_t d_batc[BATC_MAX];
{
	int entry_no;

	CMMU_LOCK;

	/* the following is from m8820x_cmmu_set_uapr */
	m8820x_cmmu_set(CMMU_UAPR, uapr, ACCESS_VAL,
		      cpu, 0, CMMU_ACS_USER, 0);

	for (entry_no = 0; entry_no < BATC_MAX; entry_no++) {
		m8820x_cmmu_set(CMMU_BWP(entry_no), i_batc[entry_no],
		    MODE_VAL | ACCESS_VAL, cpu, INST_CMMU, CMMU_ACS_USER, 0);
		m8820x_cmmu_set(CMMU_BWP(entry_no), d_batc[entry_no],
		    MODE_VAL | ACCESS_VAL, cpu, DATA_CMMU, CMMU_ACS_USER, 0);
#ifdef SHADOW_BATC
		CMMU(cpu,INST_CMMU)->batc[entry_no] = i_batc[entry_no];
		CMMU(cpu,DATA_CMMU)->batc[entry_no] = d_batc[entry_no];
#endif
	}

	/*
	 * Flush the user TLB.
	 * IF THE KERNEL WILL EVER CARE ABOUT THE BATC ENTRIES,
	 * THE SUPERVISOR TLBs SHOULD BE FLUSHED AS WELL.
	 */
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_USER_ALL, ACCESS_VAL,
	    cpu, 0, CMMU_ACS_USER, 0);

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
		    cpu, 0, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
		    cpu, 0, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE, ADDR_VAL,
		    cpu, 0, 0, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, ADDR_VAL,
		    cpu, 0, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE, ADDR_VAL,
		    cpu, 0, 0, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr, 0,
		    cpu, 0, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT, 0,
		    cpu, 0, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, 0,
	    cpu, 0, 0, 0);
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
		    cpu, INST_CMMU, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0, 0);
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
		    cpu, DATA_CMMU, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0, 0);
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
		    cpu, DATA_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CB_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CB_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0, 0);
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
		    cpu, DATA_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_CBI_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_CBI_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0, 0);
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
		    cpu, DATA_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
		    cpu, INST_CMMU, 0, 0);
	} else if (size <= MC88200_CACHE_LINE) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_LINE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else if (size <= NBPG) {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_PAGE,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	} else {
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, INST_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL, cpu, INST_CMMU, 0, 0);
		m8820x_cmmu_set(CMMU_SAR, (unsigned)physaddr,
		    MODE_VAL | ADDR_VAL, cpu, DATA_CMMU, 0, (unsigned)physaddr);
		m8820x_cmmu_set(CMMU_SAR, CMMU_FLUSH_CACHE_INV_SEGMENT,
		    MODE_VAL, cpu, DATA_CMMU, 0, 0);
	}
#else
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
	    cpu, DATA_CMMU, 0, 0);
	m8820x_cmmu_set(CMMU_SCR, CMMU_FLUSH_CACHE_INV_ALL, MODE_VAL,
	    cpu, INST_CMMU, 0, 0);
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
union ssr {
   unsigned bits;
   struct {
      unsigned  :16,
      ce:1,
      be:1,
      :4,
      wt:1,
      sp:1,
      g:1,
      ci:1,
      :1,
      m:1,
      u:1,
      wp:1,
      bh:1,
      v:1;
   } field;
};

union cssp {
   unsigned bits;
   struct {
      unsigned   : 2,
      l: 6,
      d3: 1,
      d2: 1,
      d1: 1,
      d0: 1,
      vv3: 2,
      vv2: 2,
      vv1: 2,
      vv0: 2,
      :12;
   } field;
};

union batcu {
   unsigned bits;
   struct {              /* block address translation register */
      unsigned int
      lba:13,            /* logical block address */
      pba:13,            /* physical block address */
      s:1,               /* supervisor */
      wt:4,              /* write through */
      g:1,               /* global */
      ci:1,              /* cache inhibit */
      wp:1,              /* write protect */
      v:1;               /* valid */
   } field;
};

   #define VV_EX_UNMOD		0
   #define VV_EX_MOD		1
   #define VV_SHARED_UNMOD		2
   #define VV_INVALID		3

   #define D(UNION, LINE) \
	((LINE) == 3 ? (UNION).field.d3 : \
	 ((LINE) == 2 ? (UNION).field.d2 : \
	  ((LINE) == 1 ? (UNION).field.d1 : \
	   ((LINE) == 0 ? (UNION).field.d0 : ~0))))
   #define VV(UNION, LINE) \
	((LINE) == 3 ? (UNION).field.vv3 : \
	 ((LINE) == 2 ? (UNION).field.vv2 : \
	  ((LINE) == 1 ? (UNION).field.vv1 : \
	   ((LINE) == 0 ? (UNION).field.vv0 : ~0))))

   #undef VEQR_ADDR
   #define  VEQR_ADDR 0
/*
 * Show (for debugging) how the given CMMU translates the given ADDRESS.
 * If cmmu == -1, the data cmmu for the current cpu is used.
 */
void
m8820x_cmmu_show_translation(address, supervisor_flag, verbose_flag, cmmu_num)
	unsigned address, supervisor_flag, verbose_flag;
	int cmmu_num;
{
	/*
	 * A virtual address is split into three fields. Two are used as
	 * indicies into tables (segment and page), and one is an offset into
	 * a page of memory.
	 */
	union {
		unsigned bits;
		struct {
			unsigned segment_table_index:SDT_BITS,
			page_table_index:PDT_BITS,
			page_offset:PG_BITS;
		} field;
	} virtual_address;
	u_int32_t value;

	if (verbose_flag)
		db_printf("-------------------------------------------\n");



	/****** ACCESS PROPER CMMU or THREAD ***********/
	if (cmmu_num == -1) {
		int cpu = cpu_number();
		if (cpu_cmmu[cpu].pair[DATA_CMMU] == 0) {
			db_printf("ack! can't figure my own data cmmu number.\n");
			return;
		}
		cmmu_num = cpu_cmmu[cpu].pair[DATA_CMMU] - m8820x_cmmu;
		if (verbose_flag)
			db_printf("The data cmmu for cpu#%d is cmmu#%d.\n",
				  0, cmmu_num);
	} else if (cmmu_num < 0 || cmmu_num >= MAX_CMMUS) {
		db_printf("invalid cpu number [%d]... must be in range [0..%d]\n",
			  cmmu_num, MAX_CMMUS - 1);

		return;
	}

	if (m8820x_cmmu[cmmu_num].cmmu_alive == CMMU_DEAD) {
		db_printf("warning: cmmu %d is not alive.\n", cmmu_num);
#if 0
		return;
#endif
	}

	if (!verbose_flag) {
		if (!(m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_SCTR] & CMMU_SCTR_SE))
			db_printf("WARNING: snooping not enabled for CMMU#%d.\n",
				  cmmu_num);
	} else {
		int i;
		for (i = 0; i < MAX_CMMUS; i++)
			if ((i == cmmu_num || m8820x_cmmu[i].cmmu_alive != CMMU_DEAD) &&
			    (verbose_flag > 1 || !(m8820x_cmmu[i].cmmu_regs[CMMU_SCTR] & CMMU_SCTR_SE))) {
				db_printf("CMMU#%d (cpu %d %s) snooping %s\n", i,
					  m8820x_cmmu[i].cmmu_cpu, m8820x_cmmu[i].cmmu_type ? "data" : "inst",
					  (m8820x_cmmu[i].cmmu_regs[CMMU_SCTR] & CMMU_SCTR_SE) ? "on":"OFF");
			}
	}

	if (supervisor_flag)
		value = m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_SAPR];
	else
		value = m8820x_cmmu[cmmu_num].cmmu_regs[CMMU_UAPR];

#ifdef SHADOW_BATC
	{
		int i;
		union batcu batc;
		for (i = 0; i < 8; i++) {
			batc.bits = m8820x_cmmu[cmmu_num].batc[i];
			if (batc.field.v == 0) {
				if (verbose_flag>1)
					db_printf("cmmu #%d batc[%d] invalid.\n", cmmu_num, i);
			} else {
				db_printf("cmmu#%d batc[%d] v%08x p%08x", cmmu_num, i,
					  batc.field.lba << 18, batc.field.pba);
				if (batc.field.s)  db_printf(", supervisor");
				if (batc.field.wt) db_printf(", wt.th");
				if (batc.field.g)  db_printf(", global");
				if (batc.field.ci) db_printf(", cache inhibit");
				if (batc.field.wp) db_printf(", write protect");
			}
		}
	}
#endif	/* SHADOW_BATC */

	/******* SEE WHAT A PROBE SAYS (if not a thread) ***********/
	{
		union ssr ssr;
		unsigned *volatile cmmu_regs = m8820x_cmmu[cmmu_num].cmmu_regs;
		cmmu_regs[CMMU_SAR] = address;
		cmmu_regs[CMMU_SCR] = supervisor_flag ? CMMU_PROBE_SUPER : CMMU_PROBE_USER;
		ssr.bits = cmmu_regs[CMMU_SSR];
		if (verbose_flag > 1)
			db_printf("probe of 0x%08x returns ssr=0x%08x\n",
				  address, ssr.bits);
		if (ssr.field.v)
			db_printf("PROBE of 0x%08x returns phys=0x%x",
				  address, cmmu_regs[CMMU_SAR]);
		else
			db_printf("PROBE fault at 0x%x", cmmu_regs[CMMU_PFAR]);
		if (ssr.field.ce) db_printf(", copyback err");
		if (ssr.field.be) db_printf(", bus err");
		if (ssr.field.wt) db_printf(", writethrough");
		if (ssr.field.sp) db_printf(", sup prot");
		if (ssr.field.g)  db_printf(", global");
		if (ssr.field.ci) db_printf(", cache inhibit");
		if (ssr.field.m)  db_printf(", modified");
		if (ssr.field.u)  db_printf(", used");
		if (ssr.field.wp) db_printf(", write prot");
		if (ssr.field.bh) db_printf(", BATC");
		db_printf(".\n");
	}

	/******* INTERPRET AREA DESCRIPTOR *********/
	{
		if (verbose_flag > 1) {
			db_printf("CMMU#%d", cmmu_num);
			db_printf(" %cAPR is 0x%08x\n",
				  supervisor_flag ? 'S' : 'U', value);
		}
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

		/* if not valid, done now */
		if ((value & APR_V) == 0) {
			db_printf("<would report an error, valid bit not set>\n");

			return;
		}

		value &= PG_FRAME;	/* now point to seg page */
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	virtual_address.bits = address;

	/****** ACCESS SEGMENT TABLE AND INTERPRET SEGMENT DESCRIPTOR  *******/
	{
		sdt_entry_t sdt;
		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.segment_table_index, value);
		value |= virtual_address.field.segment_table_index *
			 sizeof(sdt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("ERROR: unable to access page at 0x%08x.\n", value);
			return;
		}

		sdt = *(sdt_entry_t *)value;
		if (verbose_flag > 1)
			db_printf("SEG DESC @0x%x is 0x%08x\n", value, sdt);
		db_printf("SEG DESC @0x%x: PgTbl: 0x%x000",
			  value, PG_PFNUM(sdt));
		if (sdt & CACHE_WT)		    db_printf(", WTHRU");
		else				    db_printf(", !wthru");
		if (sdt & SG_SO)		    db_printf(", S-PROT");
		else				    db_printf(", UserOk");
		if (sdt & CACHE_GLOBAL)		    db_printf(", GLOBAL");
		else				    db_printf(", !global");
		if (sdt & CACHE_INH)		    db_printf(", $INHIBIT");
		else				    db_printf(", $ok");
		if (sdt & SG_PROT)		    db_printf(", W-PROT");
		else				    db_printf(", WriteOk");
		if (sdt & SG_V)			    db_printf(", VALID");
		else				    db_printf(", !valid");
		db_printf(".\n");

		/* if not valid, done now */
		if (!(sdt & SG_V)) {
			db_printf("<would report an error, STD entry not valid>\n");
			return;
		}

		value = ptoa(PG_PFNUM(sdt));
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	/******* PAGE TABLE *********/
	{
		pt_entry_t pte;
		if (verbose_flag)
			db_printf("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.page_table_index, value);
		value |= virtual_address.field.page_table_index *
			 sizeof(pt_entry_t);

		if (badwordaddr((vaddr_t)value)) {
			db_printf("error: unable to access page at 0x%08x.\n", value);

			return;
		}

		pte = *(pt_entry_t *)value;
		if (verbose_flag > 1)
			db_printf("PAGE DESC @0x%x is 0x%08x.\n", value, pte);
		db_printf("PAGE DESC @0x%x: page @%x000",
			  value, PG_PFNUM(pte));
		if (pte & PG_W)			db_printf(", WIRE");
		else				db_printf(", !wire");
		if (pte & CACHE_WT)		db_printf(", WTHRU");
		else				db_printf(", !wthru");
		if (pte & PG_SO)		db_printf(", S-PROT");
		else				db_printf(", UserOk");
		if (pte & CACHE_GLOBAL)		db_printf(", GLOBAL");
		else				db_printf(", !global");
		if (pte & CACHE_INH)		db_printf(", $INHIBIT");
		else				db_printf(", $ok");
		if (pte & PG_M)			db_printf(", MOD");
		else				db_printf(", !mod");
		if (pte & PG_U)			db_printf(", USED");
		else				db_printf(", !used");
		if (pte & PG_PROT)		db_printf(", W-PROT");
		else				db_printf(", WriteOk");
		if (pte & PG_V)			db_printf(", VALID");
		else				db_printf(", !valid");
		db_printf(".\n");

		/* if not valid, done now */
		if (!(pte & PG_V)) {
			db_printf("<would report an error, PTE entry not valid>\n");
			return;
		}

		value = ptoa(PG_PFNUM(pte));
		if (verbose_flag)
			db_printf("will follow to byte %d of page at 0x%x...\n",
				  virtual_address.field.page_offset, value);
		value |= virtual_address.field.page_offset;

		if (badwordaddr((vaddr_t)value)) {
			db_printf("error: unable to access page at 0x%08x.\n", value);
			return;
		}
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		db_printf("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	db_printf("WORD at 0x%x is 0x%08x.\n", value, *(unsigned *)value);

}
#endif /* DDB */
