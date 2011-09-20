/*	$OpenBSD: cpufunc.c,v 1.14 2011/09/20 22:02:10 miod Exp $	*/
/*	$NetBSD: cpufunc.c,v 1.65 2003/11/05 12:53:15 scw Exp $	*/

/*
 * arm7tdmi support code Copyright (c) 2001 John Fremlin
 * arm8 support code Copyright (c) 1997 ARM Limited
 * arm8 support code Copyright (c) 1997 Causality Limited
 * arm9 support code Copyright (C) 2001 ARM Ltd
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpufuncs.c
 *
 * C functions for supporting CPU / MMU / TLB specific operations.
 *
 * Created      : 30/01/97
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm.h>

#include <arm/cpuconf.h>
#include <arm/cpufunc.h>

#ifdef CPU_XSCALE_80200
#include <arm/xscale/i80200reg.h>
#include <arm/xscale/i80200var.h>
#endif

#ifdef CPU_XSCALE_80321
#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>
#endif

#ifdef CPU_XSCALE_IXP425
#include <arm/xscale/ixp425reg.h>
#include <arm/xscale/ixp425var.h>
#endif

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321)
#include <arm/xscale/xscalereg.h>
#endif

#if defined(PERFCTRS)
struct arm_pmc_funcs *arm_pmc;
#endif

/* PRIMARY CACHE VARIABLES */
int	arm_picache_size;
int	arm_picache_line_size;
int	arm_picache_ways;

int	arm_pdcache_size;	/* and unified */
int	arm_pdcache_line_size;
int	arm_pdcache_ways;

int	arm_pcache_type;
int	arm_pcache_unified;

int	arm_dcache_align;
int	arm_dcache_align_mask;

/* 1 == use cpu_sleep(), 0 == don't */
int cpu_do_powersave;

#ifdef CPU_ARM8
struct cpu_functions arm8_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	arm8_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	arm8_tlb_flushID,		/* tlb_flushID		*/
	arm8_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	arm8_tlb_flushID,		/* tlb_flushI		*/
	arm8_tlb_flushID_SE,		/* tlb_flushI_SE	*/
	arm8_tlb_flushID,		/* tlb_flushD		*/
	arm8_tlb_flushID_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	cpufunc_nullop,			/* icache_sync_all	*/
	(void *)cpufunc_nullop,		/* icache_sync_range	*/

	arm8_cache_purgeID,		/* dcache_wbinv_all	*/
	(void *)arm8_cache_purgeID,	/* dcache_wbinv_range	*/
/*XXX*/	(void *)arm8_cache_purgeID,	/* dcache_inv_range	*/
	(void *)arm8_cache_cleanID,	/* dcache_wb_range	*/

	arm8_cache_purgeID,		/* idcache_wbinv_all	*/
	(void *)arm8_cache_purgeID,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	cpufunc_nullop,			/* drain_writebuf	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	arm8_context_switch,		/* context_switch	*/
	arm8_setup			/* cpu setup		*/
};
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
struct cpu_functions arm9_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	arm9_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm9_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	arm9_icache_sync_all,		/* icache_sync_all	*/
	arm9_icache_sync_range,		/* icache_sync_range	*/

		/* ...cache in write-though mode... */
	arm9_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	arm9_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	arm9_dcache_wbinv_range,	/* dcache_inv_range	*/
	arm9_dcache_wb_range,		/* dcache_wb_range	*/

	arm9_idcache_wbinv_all,		/* idcache_wbinv_all	*/
	arm9_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	arm9_context_switch,		/* context_switch	*/
	arm9_setup			/* cpu setup		*/
};
#endif /* CPU_ARM9 */

#if defined(CPU_ARM9E) || defined(CPU_ARM10)
struct cpu_functions armv5_ec_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	armv5_ec_setttb,		/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm10_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	arm10_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	armv5_ec_icache_sync_all,	/* icache_sync_all	*/
	armv5_ec_icache_sync_range,	/* icache_sync_range	*/

		/* ...cache in write-though mode... */
	armv5_ec_dcache_wbinv_all,	/* dcache_wbinv_all	*/
	armv5_ec_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	armv5_ec_dcache_wbinv_range,	/* dcache_inv_range	*/
	armv5_ec_dcache_wb_range,	/* dcache_wb_range	*/

	armv5_ec_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv5_ec_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	arm10_context_switch,		/* context_switch	*/
	arm10_setup			/* cpu setup		*/
};
#endif /* CPU_ARM9E || CPU_ARM10 */


#ifdef CPU_ARM10
struct cpu_functions arm10_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	armv5_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm10_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	arm10_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	armv5_icache_sync_all,		/* icache_sync_all	*/
	armv5_icache_sync_range,	/* icache_sync_range	*/

	armv5_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	armv5_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	armv5_dcache_inv_range,		/* dcache_inv_range	*/
	armv5_dcache_wb_range,		/* dcache_wb_range	*/

	armv5_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv5_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	arm10_context_switch,		/* context_switch	*/
	arm10_setup			/* cpu setup		*/
};
#endif /* CPU_ARM10 */

#ifdef CPU_ARM11
struct cpu_functions arm11_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id				*/
	cpufunc_nullop,			/* cpwait			*/

	/* MMU functions */

	cpufunc_control,		/* control			*/
	cpufunc_domains,		/* Domain			*/
	arm11_setttb,			/* Setttb			*/
	cpufunc_faultstatus,		/* Faultstatus			*/
	cpufunc_faultaddress,		/* Faultaddress			*/

	/* TLB functions */

	arm11_tlb_flushID,		/* tlb_flushID			*/
	arm11_tlb_flushID_SE,		/* tlb_flushID_SE		*/
	arm11_tlb_flushI,		/* tlb_flushI			*/
	arm11_tlb_flushI_SE,		/* tlb_flushI_SE		*/
	arm11_tlb_flushD,		/* tlb_flushD			*/
	arm11_tlb_flushD_SE,		/* tlb_flushD_SE		*/

	/* Cache operations */

	armv5_icache_sync_all,		/* icache_sync_all	*/
	armv5_icache_sync_range,	/* icache_sync_range	*/

	armv5_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	armv5_dcache_wbinv_range,	/* dcache_wbinv_range	*/
/*XXX*/	armv5_dcache_wbinv_range,	/* dcache_inv_range	*/
	armv5_dcache_wb_range,		/* dcache_wb_range	*/

	armv5_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv5_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	arm11_drain_writebuf,		/* drain_writebuf	*/

	arm11_cpu_sleep,		/* sleep (wait for interrupt) */

	/* Soft functions */
	arm11_context_switch,		/* context_switch	*/
	arm11_setup			/* cpu setup		*/
};
#endif /* CPU_ARM11 */

#ifdef CPU_ARMv7
struct cpu_functions armv7_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	armv7_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv7_tlb_flushID,		/* tlb_flushID		*/
	armv7_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv7_tlb_flushI,		/* tlb_flushI		*/
	armv7_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv7_tlb_flushD,		/* tlb_flushD		*/
	armv7_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	armv7_icache_sync_all,		/* icache_sync_all	*/
	armv7_icache_sync_range,	/* icache_sync_range	*/

	armv7_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	armv7_dcache_wbinv_range,	/* dcache_wbinv_range	*/
/*XXX*/	armv7_dcache_wbinv_range,	/* dcache_inv_range	*/
	armv7_dcache_wb_range,		/* dcache_wb_range	*/

	armv7_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv7_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv7_drain_writebuf,		/* drain_writebuf	*/

	armv7_cpu_sleep,		/* sleep (wait for interrupt) */

	/* Soft functions */
	armv7_context_switch,		/* context_switch	*/
	armv7_setup			/* cpu setup		*/
};
#endif /* CPU_ARMv7 */


#if defined(CPU_SA1100) || defined(CPU_SA1110)
struct cpu_functions sa11x0_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	sa1_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	sa1_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	sa1_cache_syncI,		/* icache_sync_all	*/
	sa1_cache_syncI_rng,		/* icache_sync_range	*/

	sa1_cache_purgeD,		/* dcache_wbinv_all	*/
	sa1_cache_purgeD_rng,		/* dcache_wbinv_range	*/
/*XXX*/	sa1_cache_purgeD_rng,		/* dcache_inv_range	*/
	sa1_cache_cleanD_rng,		/* dcache_wb_range	*/

	sa1_cache_purgeID,		/* idcache_wbinv_all	*/
	sa1_cache_purgeID_rng,		/* idcache_wbinv_range	*/

	/* Other functions */

	sa11x0_drain_readbuf,		/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	sa11x0_cpu_sleep,		/* sleep		*/

	/* Soft functions */
	sa11x0_context_switch,		/* context_switch	*/
	sa11x0_setup			/* cpu setup		*/
};
#endif	/* CPU_SA1100 || CPU_SA1110 */

#ifdef CPU_IXP12X0
struct cpu_functions ixp12x0_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	sa1_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	sa1_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	sa1_cache_syncI,		/* icache_sync_all	*/
	sa1_cache_syncI_rng,		/* icache_sync_range	*/

	sa1_cache_purgeD,		/* dcache_wbinv_all	*/
	sa1_cache_purgeD_rng,		/* dcache_wbinv_range	*/
/*XXX*/	sa1_cache_purgeD_rng,		/* dcache_inv_range	*/
	sa1_cache_cleanD_rng,		/* dcache_wb_range	*/

	sa1_cache_purgeID,		/* idcache_wbinv_all	*/
	sa1_cache_purgeID_rng,		/* idcache_wbinv_range	*/

	/* Other functions */

	ixp12x0_drain_readbuf,		/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */
	ixp12x0_context_switch,		/* context_switch	*/
	ixp12x0_setup			/* cpu setup		*/
};
#endif	/* CPU_IXP12X0 */

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425)
struct cpu_functions xscale_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	xscale_cpwait,			/* cpwait		*/

	/* MMU functions */

	xscale_control,			/* control		*/
	cpufunc_domains,		/* domain		*/
	xscale_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	xscale_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	xscale_cache_syncI,		/* icache_sync_all	*/
	xscale_cache_syncI_rng,		/* icache_sync_range	*/

	xscale_cache_purgeD,		/* dcache_wbinv_all	*/
	xscale_cache_purgeD_rng,	/* dcache_wbinv_range	*/
	xscale_cache_flushD_rng,	/* dcache_inv_range	*/
	xscale_cache_cleanD_rng,	/* dcache_wb_range	*/

	xscale_cache_purgeID,		/* idcache_wbinv_all	*/
	xscale_cache_purgeID_rng,	/* idcache_wbinv_range	*/

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/

	xscale_cpu_sleep,		/* sleep		*/

	/* Soft functions */
	xscale_context_switch,		/* context_switch	*/
	xscale_setup			/* cpu setup		*/
};
#endif
/* CPU_XSCALE_80200 || CPU_XSCALE_80321 || CPU_XSCALE_PXA2X0 || CPU_XSCALE_IXP425 */

/*
 * Global constants also used by locore.s
 */

struct cpu_functions cpufuncs;
u_int cputype;
u_int cpu_reset_needs_v4_MMU_disable;	/* flag used in locore.s */

#if defined(CPU_ARM8) || defined(CPU_ARM9) || \
    defined(CPU_ARM9E) || defined(CPU_ARM10) || defined(CPU_ARM11) || \
    defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425)
static void get_cachetype_cp15 (void);

/* Additional cache information local to this file.  Log2 of some of the
   above numbers.  */
static int	arm_dcache_l2_nsets;
static int	arm_dcache_l2_assoc;
static int	arm_dcache_l2_linesize;

static void
get_cachetype_cp15()
{
	u_int ctype, isize, dsize;
	u_int multiplier;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 1"
		: "=r" (ctype));

	/*
	 * ...and thus spake the ARM ARM:
	 *
	 * If an <opcode2> value corresponding to an unimplemented or
	 * reserved ID register is encountered, the System Control
	 * processor returns the value of the main ID register.
	 */
	if (ctype == cpufunc_id())
		goto out;

	if ((ctype & CPU_CT_S) == 0)
		arm_pcache_unified = 1;

	/*
	 * If you want to know how this code works, go read the ARM ARM.
	 */

	arm_pcache_type = CPU_CT_CTYPE(ctype);

	if (arm_pcache_unified == 0) {
		isize = CPU_CT_ISIZE(ctype);
		multiplier = (isize & CPU_CT_xSIZE_M) ? 3 : 2;
		arm_picache_line_size = 1U << (CPU_CT_xSIZE_LEN(isize) + 3);
		if (CPU_CT_xSIZE_ASSOC(isize) == 0) {
			if (isize & CPU_CT_xSIZE_M)
				arm_picache_line_size = 0; /* not present */
			else
				arm_picache_ways = 1;
		} else {
			arm_picache_ways = multiplier <<
			    (CPU_CT_xSIZE_ASSOC(isize) - 1);
		}
		arm_picache_size = multiplier << (CPU_CT_xSIZE_SIZE(isize) + 8);
	}

	dsize = CPU_CT_DSIZE(ctype);
	multiplier = (dsize & CPU_CT_xSIZE_M) ? 3 : 2;
	arm_pdcache_line_size = 1U << (CPU_CT_xSIZE_LEN(dsize) + 3);
	if (CPU_CT_xSIZE_ASSOC(dsize) == 0) {
		if (dsize & CPU_CT_xSIZE_M)
			arm_pdcache_line_size = 0; /* not present */
		else
			arm_pdcache_ways = 1;
	} else {
		arm_pdcache_ways = multiplier <<
		    (CPU_CT_xSIZE_ASSOC(dsize) - 1);
	}
	arm_pdcache_size = multiplier << (CPU_CT_xSIZE_SIZE(dsize) + 8);

	arm_dcache_align = arm_pdcache_line_size;

	arm_dcache_l2_assoc = CPU_CT_xSIZE_ASSOC(dsize) + multiplier - 2;
	arm_dcache_l2_linesize = CPU_CT_xSIZE_LEN(dsize) + 3;
	arm_dcache_l2_nsets = 6 + CPU_CT_xSIZE_SIZE(dsize) -
	    CPU_CT_xSIZE_ASSOC(dsize) - CPU_CT_xSIZE_LEN(dsize);

 out:
	arm_dcache_align_mask = arm_dcache_align - 1;
}
#endif /* ARM7TDMI || ARM8 || ARM9 || XSCALE */

#if defined(CPU_SA1100) || defined(CPU_SA1110) || defined(CPU_IXP12X0)
/* Cache information for CPUs without cache type registers. */
struct cachetab {
	u_int32_t ct_cpuid;
	int	ct_pcache_type;
	int	ct_pcache_unified;
	int	ct_pdcache_size;
	int	ct_pdcache_line_size;
	int	ct_pdcache_ways;
	int	ct_picache_size;
	int	ct_picache_line_size;
	int	ct_picache_ways;
};

struct cachetab cachetab[] = {
    /* cpuid,		cache type,	  u,  dsiz, ls, wy,  isiz, ls, wy */
    /* XXX is this type right for SA-1? */
    { CPU_ID_SA1100,	CPU_CT_CTYPE_WB1, 0,  8192, 32, 32, 16384, 32, 32 },
    { CPU_ID_SA1110,	CPU_CT_CTYPE_WB1, 0,  8192, 32, 32, 16384, 32, 32 },
    { CPU_ID_IXP1200,	CPU_CT_CTYPE_WB1, 0, 16384, 32, 32, 16384, 32, 32 }, /* XXX */
    { 0, 0, 0, 0, 0, 0, 0, 0}
};

static void get_cachetype_table (void);

static void
get_cachetype_table()
{
	int i;
	u_int32_t cpuid = cpufunc_id();

	for (i = 0; cachetab[i].ct_cpuid != 0; i++) {
		if (cachetab[i].ct_cpuid == (cpuid & CPU_ID_CPU_MASK)) {
			arm_pcache_type = cachetab[i].ct_pcache_type;
			arm_pcache_unified = cachetab[i].ct_pcache_unified;
			arm_pdcache_size = cachetab[i].ct_pdcache_size;
			arm_pdcache_line_size =
			    cachetab[i].ct_pdcache_line_size;
			arm_pdcache_ways = cachetab[i].ct_pdcache_ways;
			arm_picache_size = cachetab[i].ct_picache_size;
			arm_picache_line_size =
			    cachetab[i].ct_picache_line_size;
			arm_picache_ways = cachetab[i].ct_picache_ways;
		}
	}
	arm_dcache_align = arm_pdcache_line_size;

	arm_dcache_align_mask = arm_dcache_align - 1;
}

#endif /* SA110 || SA1100 || SA1111 || IXP12X0 */

#ifdef CPU_ARMv7
void arm_get_cachetype_cp15v7 (void);
int	arm_dcache_l2_nsets;
int	arm_dcache_l2_assoc;
int	arm_dcache_l2_linesize;

static int
log2(int size)
{
	int i = 0;
	while (size != 0)
	for (i = 0; size != 0; i++)
		size >>= 1;
	return i;
}

void
arm_get_cachetype_cp15v7(void)
{
	extern int pmap_cachevivt;
	uint32_t cachereg;
	uint32_t cache_level_id;
	uint32_t line_size, ways, sets, size;
	uint32_t sel;
	uint32_t ctr;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 1"
		: "=r" (ctr) :);

	switch ((ctr >> 14) & 3) {
	case 2:
		pmap_cachevivt = 0;
	#if 0
		pmap_alias_dist = 0x4000;
		pmap_alias_bits = 0x3000;
	#endif
		break;
	case 3:
		pmap_cachevivt = 0;
		break;
	default:
		break;
	}

	__asm __volatile("mrc p15, 1, %0, c0, c0, 1"
		: "=r" (cache_level_id) :);

	/* dcache L1 */
	sel = 0;
	__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
		:: "r" (sel));
	__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
		: "=r" (cachereg) :);
	line_size = 1 << ((cachereg & 7)+4);
	ways = ((0x00000ff8 & cachereg) >> 3) + 1;
	sets = ((0x0ffff000 & cachereg) >> 13) + 1;
	arm_pcache_unified = (cache_level_id & 0x7) == 2;
	arm_pdcache_line_size = line_size;
	arm_pdcache_ways = ways;
	size = line_size * ways * sets;
	arm_pdcache_size = size;

	switch (cachereg & 0xc0000000) {
	case 0x00000000:
		arm_pcache_type = 0;
		break;
	case 0x40000000:
		arm_pcache_type = CPU_CT_CTYPE_WT;
		break;
	case 0x80000000:
	case 0xc0000000:
		arm_pcache_type = CPU_CT_CTYPE_WB1;
	}

	/* icache L1 */
	sel = 1;
	__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
		:: "r" (sel));
	__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
		: "=r" (cachereg) :);
	line_size = 1 << ((cachereg & 7)+4);
	ways = ((0x00000ff8 & cachereg) >> 3) + 1;
	sets = ((0x0ffff000 & cachereg) >> 13) + 1;
	arm_picache_line_size = line_size;
	size = line_size * ways * sets;
	arm_picache_size = size;
	arm_picache_ways = ways;

	arm_dcache_align = arm_pdcache_line_size;

	arm_dcache_align_mask = arm_dcache_align - 1;

	/* ucache L2 */
	sel = 1;
	__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
		:: "r" (sel));
	__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
		: "=r" (cachereg) :);
	line_size = 1 << ((cachereg & 7)+4);
	ways = ((0x00000ff8 & cachereg) >> 3) + 1;
	sets = ((0x0ffff000 & cachereg) >> 13) + 1;
	arm_dcache_l2_nsets = log2(sets);
	arm_dcache_l2_assoc = log2(ways);
	arm_dcache_l2_linesize = log2(line_size);
}

/* 
 */
void
armv7_idcache_wbinv_all()
{
	uint32_t arg;
	arg = 0;
	__asm __volatile("mcr	p15, 0, r0, c7, c5, 0" :: "r" (arg));
	armv7_dcache_wbinv_all();
}
/* brute force cache flushing */
void
armv7_dcache_wbinv_all()
{
	int sets, ways, lvl;
	int nincr, nsets, nways;
	uint32_t wayincr, setincr;
	uint32_t wayval, setval;
	uint32_t word;

	nsets = arm_picache_size/arm_picache_ways/arm_picache_line_size;
	nways = arm_picache_ways;
	nincr = arm_picache_line_size;

	wayincr = 1 << (32 - arm_picache_ways);
	setincr = arm_picache_line_size;

#if 0
	printf("l1 nsets %d nways %d nincr %d wayincr %x setincr %x\n",
	    nsets, nways, nincr, wayincr, setincr);
#endif
	
	lvl = 0; /* L1 */
	setval = 0;
	for (sets = 0; sets < nsets; sets++)  {
		wayval = 0;
		for (ways = 0; ways < nways; ways++) {
			word = wayval | setval | lvl;

			/* Clean D cache SE with Set/Index */
			__asm __volatile("mcr	p15, 0, %0, c7, c10, 2"
			    : : "r" (word));
			wayval += nincr;
		}
		setval += setincr;
	}
	/* drain the write buffer */
	__asm __volatile("mcr	p15, 0, %0, c7, c10, 4" : : "r" (0));

	/* L2 */
	nsets = 1 << arm_dcache_l2_nsets;
	nways = 1 << arm_dcache_l2_assoc;
	nincr = 1 << arm_dcache_l2_linesize;

	wayincr = 1 << (32 - arm_picache_ways);
	setincr = arm_picache_line_size;

#if 0
	printf("l2 nsets %d nways %d nincr %d wayincr %x setincr %x\n",
	    nsets, nways, nincr, wayincr, setincr);
#endif
	
	lvl = 1 << 1; /* L2 */
	setval = 0;
	for (sets = 0; sets < nsets; sets++)  {
		wayval = 0;
		for (ways = 0; ways < nways; ways++) {
			word = wayval | setval | lvl;

			/* Clean D cache SE with Set/Index */
			__asm __volatile("mcr	p15, 0, %0, c7, c10, 2"
			    : : "r" (word));
			wayval += nincr;
		}
		setval += setincr;
	}
	/* drain the write buffer */
	__asm __volatile("mcr	p15, 0, %0, c7, c10, 4" : : "r" (0));

}
#endif /* CPU_ARMv7 */


/*
 * Cannot panic here as we may not have a console yet ...
 */

int
set_cpufuncs()
{
	cputype = cpufunc_id();
	cputype &= CPU_ID_CPU_MASK;

	/*
	 * NOTE: cpu_do_powersave defaults to off.  If we encounter a
	 * CPU type where we want to use it by default, then we set it.
	 */

#ifdef CPU_ARM8
	if ((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD &&
	    (cputype & 0x0000f000) == 0x00008000) {
		cpufuncs = arm8_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 0;	/* XXX correct? */
		get_cachetype_cp15();
		pmap_pte_init_arm8();
		return 0;
	}
#endif	/* CPU_ARM8 */
#ifdef CPU_ARM9
	if (((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD ||
	     (cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_TI) &&
	    (cputype & 0x0000f000) == 0x00009000) {
		cpufuncs = arm9_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype_cp15();
		arm9_dcache_sets_inc = 1U << arm_dcache_l2_linesize;
		arm9_dcache_sets_max =
		    (1U << (arm_dcache_l2_linesize + arm_dcache_l2_nsets)) -
		    arm9_dcache_sets_inc;
		arm9_dcache_index_inc = 1U << (32 - arm_dcache_l2_assoc);
		arm9_dcache_index_max = 0U - arm9_dcache_index_inc;
		pmap_pte_init_arm9();
		return 0;
	}
#endif /* CPU_ARM9 */
#if defined(CPU_ARM9E) || defined(CPU_ARM10)
	if (cputype == CPU_ID_ARM926EJS || cputype == CPU_ID_ARM1026EJS) {
		cpufuncs = armv5_ec_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype_cp15();
		pmap_pte_init_generic();
		return 0;
	}
#endif /* CPU_ARM9E || CPU_ARM10 */
#ifdef CPU_ARM10
	if (/* cputype == CPU_ID_ARM1020T || */
	    cputype == CPU_ID_ARM1020E) {
		/*
		 * Select write-through cacheing (this isn't really an
		 * option on ARM1020T).
		 */
		cpufuncs = arm10_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype_cp15();
		armv5_dcache_sets_inc = 1U << arm_dcache_l2_linesize;
		armv5_dcache_sets_max =
		    (1U << (arm_dcache_l2_linesize + arm_dcache_l2_nsets)) -
		    armv5_dcache_sets_inc;
		armv5_dcache_index_inc = 1U << (32 - arm_dcache_l2_assoc);
		armv5_dcache_index_max = 0U - armv5_dcache_index_inc;
		pmap_pte_init_generic();
		return 0;
	}
#endif /* CPU_ARM10 */
#ifdef CPU_ARM11
	if (cputype == CPU_ID_ARM1136JS ||
	    cputype == CPU_ID_ARM1136JSR1 || 1) {
		cpufuncs = arm11_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype_cp15();
		arm11_dcache_sets_inc = 1U << arm_dcache_l2_linesize;
		arm11_dcache_sets_max =
		    (1U << (arm_dcache_l2_linesize + arm_dcache_l2_nsets)) -
		    arm11_dcache_sets_inc;
		arm11_dcache_index_inc = 1U << (32 - arm_dcache_l2_assoc);
		arm11_dcache_index_max = 0U - arm11_dcache_index_inc;
		pmap_pte_init_arm11();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;
		return 0;
	}
#endif /* CPU_ARM11 */
#ifdef CPU_ARMv7
	if ((cputype & CPU_ID_CORTEX_A8_MASK) == CPU_ID_CORTEX_A8) {
		cpufuncs = armv7_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		arm_get_cachetype_cp15v7();
		armv7_dcache_sets_inc = 1U << arm_dcache_l2_linesize;
		armv7_dcache_sets_max =
		    (1U << (arm_dcache_l2_linesize + arm_dcache_l2_nsets)) -
		    armv7_dcache_sets_inc;
		armv7_dcache_index_inc = 1U << (32 - arm_dcache_l2_assoc);
		armv7_dcache_index_max = 0U - armv7_dcache_index_inc;
		pmap_pte_init_armv7();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;
		return 0;
	}
#endif /* CPU_ARMv7 */
#ifdef CPU_SA1100
	if (cputype == CPU_ID_SA1100) {
		cpufuncs = sa11x0_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* SA needs it	*/
		get_cachetype_table();
		pmap_pte_init_sa1();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif	/* CPU_SA1100 */
#ifdef CPU_SA1110
	if (cputype == CPU_ID_SA1110) {
		cpufuncs = sa11x0_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* SA needs it	*/
		get_cachetype_table();
		pmap_pte_init_sa1();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif	/* CPU_SA1110 */
#ifdef CPU_IXP12X0
	if (cputype == CPU_ID_IXP1200) {
		cpufuncs = ixp12x0_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;
		get_cachetype_table();
		pmap_pte_init_sa1();
		return 0;
	}
#endif  /* CPU_IXP12X0 */
#ifdef CPU_XSCALE_80200
	if (cputype == CPU_ID_80200) {
		int rev = cpufunc_id() & CPU_ID_REVISION_MASK;

		i80200_icu_init();

#ifdef PERFCTRS
		/*
		 * Reset the Performance Monitoring Unit to a
		 * pristine state:
		 *	- CCNT, PMN0, PMN1 reset to 0
		 *	- overflow indications cleared
		 *	- all counters disabled
		 */
		__asm __volatile("mcr p14, 0, %0, c0, c0, 0"
			:
			: "r" (PMNC_P|PMNC_C|PMNC_PMN0_IF|PMNC_PMN1_IF|
			       PMNC_CC_IF));
#endif /* PERFCTRS */

#if defined(XSCALE_CCLKCFG)
		/*
		 * Crank CCLKCFG to maximum legal value.
		 */
		__asm __volatile ("mcr p14, 0, %0, c6, c0, 0"
			:
			: "r" (XSCALE_CCLKCFG));
#endif

		/*
		 * XXX Disable ECC in the Bus Controller Unit; we
		 * don't really support it, yet.  Clear any pending
		 * error indications.
		 */
		__asm __volatile("mcr p13, 0, %0, c0, c1, 0"
			:
			: "r" (BCUCTL_E0|BCUCTL_E1|BCUCTL_EV));

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		/*
		 * i80200 errata: Step-A0 and A1 have a bug where
		 * D$ dirty bits are not cleared on "invalidate by
		 * address".
		 *
		 * Workaround: Clean cache line before invalidating.
		 */
		if (rev == 0 || rev == 1)
			cpufuncs.cf_dcache_inv_range = xscale_cache_purgeD_rng;

		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();
		return 0;
	}
#endif /* CPU_XSCALE_80200 */
#ifdef CPU_XSCALE_80321
	if (cputype == CPU_ID_80321_400 || cputype == CPU_ID_80321_600 ||
	    cputype == CPU_ID_80321_400_B0 || cputype == CPU_ID_80321_600_B0 ||
	    cputype == CPU_ID_80219_400 || cputype == CPU_ID_80219_600) {
		i80321intc_init();

#ifdef PERFCTRS
		/*
		 * Reset the Performance Monitoring Unit to a
		 * pristine state:
		 *	- CCNT, PMN0, PMN1 reset to 0
		 *	- overflow indications cleared
		 *	- all counters disabled
		 */
		__asm __volatile("mcr p14, 0, %0, c0, c0, 0"
			:
			: "r" (PMNC_P|PMNC_C|PMNC_PMN0_IF|PMNC_PMN1_IF|
			       PMNC_CC_IF));
#endif /* PERFCTRS */

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();
		return 0;
	}
#endif /* CPU_XSCALE_80321 */
#ifdef CPU_XSCALE_PXA2X0
	/* ignore core revision to test PXA2xx CPUs */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA250 ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA210) {

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();

		/* Use powersave on this CPU. */
		cpu_do_powersave = 1;

		return 0;
	}
#endif /* CPU_XSCALE_PXA2X0 */
#ifdef CPU_XSCALE_IXP425
	if (cputype == CPU_ID_IXP425_533 || cputype == CPU_ID_IXP425_400 ||
	    cputype == CPU_ID_IXP425_266) {
		ixp425_icu_init();

		cpufuncs = xscale_cpufuncs;
#if defined(PERFCTRS)
		xscale_pmu_init();
#endif

		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();

		return 0;
	}
#endif /* CPU_XSCALE_IXP425 */
	/*
	 * Bzzzz. And the answer was ...
	 */
	panic("No support for this CPU type (%08x) in kernel", cputype);
	return(ARCHITECTURE_NOT_PRESENT);
}

/*
 * CPU Setup code
 */

#ifdef CPU_ARM8
void
arm8_setup()
{
	int integer;
	int cpuctrl, cpuctrlmask;
	int clocktest;
	int setclock = 0;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IDC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Get clock configuration */
	clocktest = arm8_clock_config(0, 0) & 0x0f;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* Set the clock/test register */
	if (setclock)
		arm8_clock_config(0x7f, clocktest);
}
#endif	/* CPU_ARM8 */

#ifdef CPU_ARM9
void
arm9_setup()
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
	    | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_VECRELOC
		 | CPU_CONTROL_ROUNDROBIN;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(cpuctrlmask, cpuctrl);

}
#endif	/* CPU_ARM9 */

#if defined(CPU_ARM9E) || defined(CPU_ARM10)
void
arm10_setup()
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm __volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Allow detection code to find the VFP if it's fitted.  */
	__asm __volatile ("mcr\tp15, 0, %0, c1, c0, 2" : : "r" (0x0fffffff));

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM9E || CPU_ARM10 */

#ifdef CPU_ARM11
void
arm11_setup()
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_AFLT_ENABLE /* | CPU_CONTROL_BPRD_ENABLE */;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM11 */

#ifdef CPU_ARMv7
void
armv7_setup()
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_BPRD_ENABLE | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK
	    | CPU_CONTROL_VECRELOC | CPU_CONTROL_FI | CPU_CONTROL_VE;

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	/* XXX */
	/*
	asm volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );
	*/

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARMv7 */

#if defined(CPU_SA1100) || defined(CPU_SA1110)
void
sa11x0_setup()
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_SA1100 || CPU_SA1110 */

#if defined(CPU_IXP12X0)
void
ixp12x0_setup()
{
	int cpuctrl, cpuctrlmask;


	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_DC_ENABLE | CPU_CONTROL_WBUF_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_ROM_ENABLE | CPU_CONTROL_IC_ENABLE
		 | CPU_CONTROL_VECRELOC;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	curcpu()->ci_ctrl = cpuctrl;
	/* cpu_control(0xffffffff, cpuctrl); */
	cpu_control(cpuctrlmask, cpuctrl);
}
#endif /* CPU_IXP12X0 */

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425)
void
xscale_setup()
{
	uint32_t auxctl;
	int cpuctrl, cpuctrlmask;

	/*
	 * The XScale Write Buffer is always enabled.  Our option
	 * is to enable/disable coalescing.  Note that bits 6:3
	 * must always be enabled.
	 */

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE | CPU_CONTROL_AFLT_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/*
	 * Set the control register.  Note that bits 6:3 must always
	 * be set to 1.
	 */
	curcpu()->ci_ctrl = cpuctrl;
/*	cpu_control(cpuctrlmask, cpuctrl);*/
	cpu_control(0xffffffff, cpuctrl);

	/* Make sure write coalescing is turned on */
	__asm __volatile("mrc p15, 0, %0, c1, c0, 1"
		: "=r" (auxctl));
#ifdef XSCALE_NO_COALESCE_WRITES
	auxctl |= XSCALE_AUXCTL_K;
#else
	auxctl &= ~XSCALE_AUXCTL_K;
#endif
	__asm __volatile("mcr p15, 0, %0, c1, c0, 1"
		: : "r" (auxctl));
}
#endif	/* CPU_XSCALE_80200 || CPU_XSCALE_80321 || CPU_XSCALE_PXA2X0 || CPU_XSCALE_IXP425 */
