/*	$OpenBSD: m88110.c,v 1.4 2001/12/22 09:49:39 smurph Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
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
#ifdef M88110

#include <sys/param.h>
#include <sys/types.h>
#include <sys/simplelock.h>
#include <machine/board.h>
#include <machine/cpus.h>
#include <machine/cpu_number.h>
#include <machine/cmmu.h>
#include <machine/locore.h>

#define CMMU_DEBUG 1

#ifdef DEBUG
#define DB_CMMU		0x4000	/* MMU debug */
unsigned int debuglevel = 0;
#define dprintf(_L_,_X_) { if (debuglevel & (_L_)) { unsigned int psr = disable_interrupts_return_psr(); printf("%d: ", cpu_number()); printf _X_;  set_psr(psr); } }
#else
#define dprintf(_L_,_X_)
#endif 

#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#define DEBUG_MSG db_printf
#define STATIC
#else
#define DEBUG_MSG printf
#define STATIC	static
#endif /* DDB */

/* kernel copy of PATC entries */
unsigned patc_data_u[32];
unsigned patc_data_l[32];
unsigned patc_inst_u[32];
unsigned patc_inst_l[32];

#define INST	0
#define DATA	1
#define BOTH    2
#define KERN	1
#define USER	0

/* FORWARDS */
unsigned batc_val __P((unsigned phys, unsigned virt, unsigned prot));
void patc_insert __P((unsigned upper, unsigned lower, int which));
void patc_clear __P((void));
void patc_sync __P((int which));
void patc_load __P((int index, unsigned upper, unsigned lower, int which));
void m88110_cmmu_sync_cache __P((vm_offset_t physaddr, int size));
void m88110_cmmu_sync_inval_cache __P((vm_offset_t physaddr, int size));
void m88110_cmmu_inval_cache __P((vm_offset_t physaddr, int size));
int probe_mmu __P((vm_offset_t va, int data));

/* This is the function table for the mc88110 built-in CMMUs */
struct cmmu_p cmmu88110 = {
        m88110_cmmu_init,
        m88110_show_apr,
	m88110_setup_board_config,
	m88110_setup_cmmu_config,
	m88110_cmmu_dump_config,
	m88110_cpu_configuration_print,
	m88110_cmmu_shutdown_now,
	m88110_cmmu_parity_enable,
	m88110_cmmu_cpu_number,
	m88110_cmmu_get_idr,
	m88110_cmmu_set_sapr,
	m88110_cmmu_remote_set_sapr,
	m88110_cmmu_set_uapr,
	m88110_cmmu_set_batc_entry,
	m88110_cmmu_set_pair_batc_entry,
	m88110_cmmu_flush_remote_tlb,
	m88110_cmmu_flush_tlb,
	m88110_cmmu_pmap_activate,
	m88110_cmmu_flush_remote_cache,
	m88110_cmmu_flush_cache,
	m88110_cmmu_flush_remote_inst_cache,
	m88110_cmmu_flush_inst_cache,
	m88110_cmmu_flush_remote_data_cache,
	m88110_cmmu_flush_data_cache,
	m88110_dma_cachectl,
#ifdef DDB
	m88110_cmmu_get_by_mode,
	m88110_cmmu_show_translation,
	m88110_cmmu_cache_state,
	m88110_show_cmmu_info,
#endif /* end if DDB */
};

void
patc_load(int index, unsigned upper, unsigned lower, int which)
{
	/* sanity check!!! */
	if (index > 31) {
		panic("invalid PATC index %d!", index);
	}
	index = index << 5;
	switch (which) {
	case INST:
		set_iir(index);
		set_ippu(upper);
		set_ippl(lower);
		break;
	case DATA:
		set_dir(index);
		set_dppu(upper);
		set_dppl(lower);
		break;
	default:
		panic("invalid PATC! Choose DATA or INST...");
	}
}

void
patc_sync(int which)
{
	int i;
	switch (which) {
	case BOTH:
		for (i=0; i<32; i++) {
			patc_load(i, patc_data_u[i], patc_data_l[i], DATA);
			patc_load(i, patc_inst_u[i], patc_inst_l[i], INST);
		}
		break;
	case INST:
		for (i=0; i<32; i++) {
			patc_load(i, patc_inst_u[i], patc_inst_l[i], INST);
		}
		break;
	case DATA:
		for (i=0; i<32; i++) {
			patc_load(i, patc_data_u[i], patc_data_l[i], DATA);
		}
		break;
	}
}

void 
patc_clear(void)
{
	int i;
	for (i=0; i<32; i++) {
		patc_data_u[i] = 0;
		patc_data_l[i] = 0;
		patc_inst_u[i] = 0;
		patc_inst_l[i] = 0;
	}
	patc_sync(BOTH);
}

/* implement a FIFO on the PATC entries */
void 
patc_insert(unsigned upper, unsigned lower, int which)
{
	int i;
	switch(which){
	case INST:
		for (i=31; i>0; i--) {
			patc_inst_u[i] = patc_inst_u[i-1];
			patc_inst_l[i] = patc_inst_l[i-1];
		}
		patc_inst_u[0] = upper;
		patc_inst_l[0] = lower;
		patc_sync(INST);
		break;
	case DATA:
		for (i=31; i>0; i--) {
			patc_data_u[i] = patc_data_u[i-1];
			patc_data_l[i] = patc_data_l[i-1];
		}
		patc_data_u[0] = upper;
		patc_data_l[0] = lower;
		patc_sync(DATA);
		break;
	case BOTH:
		panic("patc_insert(): can't insert both INST and DATA.");
	}
}

unsigned 
batc_val(unsigned phys, unsigned virt, unsigned prot)
{
	unsigned val = 0;
	virt = (virt >> BATC_ADDR_SHIFT);
	val |= (virt << BATC_LBA_SHIFT);
	phys = (phys >> BATC_ADDR_SHIFT);
	val |= (phys << BATC_PBA_SHIFT);
	val |= prot;
	return(val);
}


void
m88110_show_apr(unsigned value)
{
	union apr_template apr_template;
	apr_template.bits = value;

	printf("table @ 0x%x000", apr_template.field.st_base);
	if (apr_template.field.wt) printf(", writethrough");
	if (apr_template.field.g)  printf(", global");
	if (apr_template.field.ci) printf(", cache inhibit");
	if (apr_template.field.te) printf(", valid");
	else			   printf(", not valid");
	printf("\n");
}

void 
m88110_setup_board_config(void)
{
	/* dummy routine */
	m88110_setup_cmmu_config();
	return;
}

void 
m88110_setup_cmmu_config(void)
{
	/* we can print something here... */
	cpu_sets[0] = 1;   /* This cpu installed... */
	return;
}

void m88110_cmmu_dump_config(void)
{
	/* dummy routine */
   return;
}

#ifdef DDB
/*
 * Used by DDB for cache probe functions
 */
unsigned m88110_cmmu_get_by_mode(int cpu, int mode)
{
	CMMU_LOCK;
	return 0;
	CMMU_UNLOCK;
}
#endif

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void 
m88110_cpu_configuration_print(int master)
{
	int pid = read_processor_identification_register();
	int proctype = (pid & 0xff00) >> 8;
	int procvers = (pid & 0xe) >> 1;
	int cpu = cpu_number();
	struct simplelock print_lock;

	CMMU_LOCK;
	if (master)
		simple_lock_init(&print_lock);

	simple_lock(&print_lock);

	printf("Processor %d: ", cpu);
	if (proctype)
		printf("Architectural Revision 0x%x UNKNOWN CPU TYPE Version 0x%x\n",
		       proctype, procvers);
	else
		printf("M88110 Version 0x%x\n", procvers);

	simple_unlock(&print_lock);
        CMMU_UNLOCK;
	return;
}

/*
 * CMMU initialization routine
 */
void m88110_load_patc(int entry, vm_offset_t vaddr, vm_offset_t paddr, int kernel);

void
m88110_cmmu_init(void)
{
	int i;

	/* clear BATCs */
	for (i=0; i<8; i++) {
		m88110_cmmu_set_pair_batc_entry(0, i, 0);
	}
	/* clear PATCs */
	patc_clear();
	
	set_ictl(BATC_32M 
		 | CMMU_ICTL_DID	/* Double instruction disable */
		 | CMMU_ICTL_MEN
		 | CMMU_ICTL_CEN 
		 | CMMU_ICTL_BEN
		 | CMMU_ICTL_HTEN);

	set_dctl(BATC_32M 
                 | CMMU_DCTL_MEN
		 | CMMU_DCTL_CEN 
		 | CMMU_DCTL_SEN
		 | CMMU_DCTL_ADS
		 | CMMU_DCTL_HTEN);      


	mc88110_inval_inst();		/* clear instruction cache & TIC */
	mc88110_inval_data();		/* clear data cache */
	mc88410_inval();		/* clear external data cache */

	set_dcmd(CMMU_DCMD_INV_SATC);	/* invalidate ATCs */

	set_isr(0);
	set_ilar(0);
	set_ipar(0);
	set_dsr(0);
	set_dlar(0);
	set_dpar(0);
}

/*
 * Just before poweroff or reset....
 */
void
m88110_cmmu_shutdown_now(void)
{
	CMMU_LOCK;
        CMMU_UNLOCK;
}

/*
 * enable parity
 */
void 
m88110_cmmu_parity_enable(void)
{
#ifdef	PARITY_ENABLE
	CMMU_LOCK;
        CMMU_UNLOCK;
#endif  /* PARITY_ENABLE */
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */
#define ILLADDRESS	U(0x0F000000) 	/* any faulty address */

unsigned 
m88110_cmmu_cpu_number(void)
{
	return 0; /* to make compiler happy */
}

/* Needs no locking - read only registers */
unsigned
m88110_cmmu_get_idr(unsigned data)
{
	return 0; /* todo */
}

int 
probe_mmu(vm_offset_t va, int data)
{
	unsigned result;
	if (data) {
		CMMU_LOCK;
		set_dsar((unsigned)va);
		set_dcmd(CMMU_DCMD_PRB_SUPR);
		result = get_dsr();
		CMMU_UNLOCK;
		if (result & CMMU_DSR_BH)
			return 2;
		else if (result & CMMU_DSR_PH)
			return 1;
		else
			return 0;
	} else {
		CMMU_LOCK;
		set_isar((unsigned)va);
		set_icmd(CMMU_ICMD_PRB_SUPR);
		result = get_isr();
		CMMU_UNLOCK;
		if (result & CMMU_ISR_BH)
			return 2;
		else if (result & CMMU_ISR_PH)
			return 1;
		else
			return 0;
	}
	return 0;
}

void
m88110_cmmu_set_sapr(unsigned ap)
{
#if 0
	int result;
#endif 
	unsigned ictl, dctl;
	CMMU_LOCK;

	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_SATC);

	ictl = get_ictl();
	dctl = get_dctl();
	/* disabel translation */
	set_ictl((ictl &~ CMMU_ICTL_MEN));
	set_dctl((dctl &~ CMMU_DCTL_MEN));

	set_isap(ap);
	set_dsap(ap);
	
	patc_clear();
	
	set_icmd(CMMU_ICMD_INV_UATC);
	set_icmd(CMMU_ICMD_INV_SATC);
	set_dcmd(CMMU_DCMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_SATC);
	
	/* restore MMU settings */
	set_ictl(ictl);
	set_dctl(dctl);
	
	CMMU_UNLOCK;
	return;
}

void
m88110_cmmu_remote_set_sapr(unsigned cpu, unsigned ap)
{
	m88110_cmmu_set_sapr(ap);
}

void
m88110_cmmu_set_uapr(unsigned ap)
{
	CMMU_LOCK;
	set_iuap(ap);
	set_duap(ap);
	set_icmd(CMMU_ICMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_UATC);
	mc88110_inval_inst();
	CMMU_UNLOCK;
}

/*
 * Set batc entry number entry_no to value in 
 * the data or instruction cache depending on data.
 *
 * Except for the cmmu_init, this function, m88110_cmmu_set_pair_batc_entry,
 * and m88110_cmmu_pmap_activate are the only functions which may set the
 * batc values.
 */
void
m88110_cmmu_set_batc_entry(
			unsigned cpu,
			unsigned entry_no,
			unsigned data,	 /* 1 = data, 0 = instruction */
			unsigned value)	 /* the value to stuff */
{
	CMMU_LOCK;
	if (data) {
		set_dir(entry_no);
		set_dbp(value);
	} else {
		set_iir(entry_no);
		set_ibp(value);
	}
	CMMU_UNLOCK;
}

/*
 * Set batc entry number entry_no to value in 
 * the data and instruction cache for the named CPU.
 */
void
m88110_cmmu_set_pair_batc_entry(unsigned cpu, unsigned entry_no, unsigned value)
/* the value to stuff into the batc */
{
	m88110_cmmu_set_batc_entry(cpu, entry_no, 1, value);
	m88110_cmmu_set_batc_entry(cpu, entry_no, 0, value);
}

/**
 **	Functions that invalidate TLB entries.
 **/

/*
 *	flush any tlb
 *	Some functionality mimiced in m88110_cmmu_pmap_activate.
 */
void
m88110_cmmu_flush_remote_tlb(unsigned cpu, unsigned kernel, vm_offset_t vaddr, int size)
{
	register int s = splhigh();
	
	CMMU_LOCK;
	if (kernel) {
		set_icmd(CMMU_ICMD_INV_SATC);
		set_dcmd(CMMU_DCMD_INV_SATC);
	} else {
		set_icmd(CMMU_ICMD_INV_UATC);
		set_dcmd(CMMU_DCMD_INV_UATC);
	}
	CMMU_UNLOCK;
	
	splx(s);
}

/*
 *	flush my personal tlb
 */
void
m88110_cmmu_flush_tlb(unsigned kernel, vm_offset_t vaddr, int size)
{
	int cpu;
	cpu = cpu_number();
	m88110_cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
}

/*
 * New fast stuff for pmap_activate.
 * Does what a few calls used to do.
 * Only called from pmap.c's _pmap_activate().
 */
void
m88110_cmmu_pmap_activate(
		       unsigned cpu,
		       unsigned uapr,
		       batc_template_t i_batc[BATC_MAX],
		       batc_template_t d_batc[BATC_MAX])
{
	m88110_cmmu_set_uapr(uapr);

	/*
	for (entry_no = 0; entry_no < 8; entry_no++) {
	   m88110_cmmu_set_batc_entry(cpu, entry_no, 0, i_batc[entry_no].bits);
	   m88110_cmmu_set_batc_entry(cpu, entry_no, 1, d_batc[entry_no].bits);
	}
	*/
	/*
	 * Flush the user TLB.
	 * IF THE KERNEL WILL EVER CARE ABOUT THE BATC ENTRIES,
	 * THE SUPERVISOR TLBs SHOULB EE FLUSHED AS WELL.
	 */
	set_icmd(CMMU_ICMD_INV_UATC);
	set_dcmd(CMMU_DCMD_INV_UATC);
}

/**
 **	Functions that invalidate caches.
 **
 ** Cache invalidates require physical addresses.  Care must be exercised when
 ** using segment invalidates.  This implies that the starting physical address
 ** plus the segment length should be invalidated.  A typical mistake is to
 ** extract the first physical page of a segment from a virtual address, and
 ** then expecting to invalidate when the pages are not physically contiguous.
 **
 ** We don't push Instruction Caches prior to invalidate because they are not
 ** snooped and never modified (I guess it doesn't matter then which form
 ** of the command we use then).
 **/

/* 
 * Care must be taken to avoid flushing the data cache when 
 * the data cache is not on!  From the 0F92L Errata Documentation
 * Package, Version 1.1
 */

/*
 *	flush both Instruction and Data caches
 */
void
m88110_cmmu_flush_remote_cache(int cpu, vm_offset_t physaddr, int size)
{
	register int s = splhigh();
	
	mc88110_inval_inst();
	mc88110_flush_data();
	mc88410_flush();
	splx(s);
}

/*
 *	flush both Instruction and Data caches
 */
void
m88110_cmmu_flush_cache(vm_offset_t physaddr, int size)
{
	int cpu = cpu_number();
	
	m88110_cmmu_flush_remote_cache(cpu, physaddr, size);
}

/*
 *	flush Instruction caches
 */
void
m88110_cmmu_flush_remote_inst_cache(int cpu, vm_offset_t physaddr, int size)
{
	register int s = splhigh();

	mc88110_inval_inst();
	splx(s);
}

/*
 *	flush Instruction caches
 */
void
m88110_cmmu_flush_inst_cache(vm_offset_t physaddr, int size)
{
	int cpu;
	
	cpu = cpu_number();
	m88110_cmmu_flush_remote_inst_cache(cpu, physaddr, size);
}

/*
 * flush data cache
 */ 
void
m88110_cmmu_flush_remote_data_cache(int cpu, vm_offset_t physaddr, int size)
{ 
	register int s = splhigh();

	mc88110_flush_data();
	mc88410_flush();
	splx(s);
}

/*
 * flush data cache
 */ 
void
m88110_cmmu_flush_data_cache(vm_offset_t physaddr, int size)
{ 
	int cpu;
	
	cpu = cpu_number();
	m88110_cmmu_flush_remote_data_cache(cpu, physaddr, size);
}

/*
 * sync dcache (and icache too)
 */
void
m88110_cmmu_sync_cache(vm_offset_t physaddr, int size)
{
	register int s = splhigh();

	mc88110_inval_inst();
	mc88110_flush_data();
	mc88410_flush();
	splx(s);
}

void
m88110_cmmu_sync_inval_cache(vm_offset_t physaddr, int size)
{
	register int s = splhigh();

	mc88110_sync_data();
	mc88410_sync();
	splx(s);
}

void
m88110_cmmu_inval_cache(vm_offset_t physaddr, int size)
{
	register int s = splhigh();
	
	mc88110_inval_inst();
	mc88110_inval_data();
	mc88410_inval();
	splx(s);
}

void
m88110_dma_cachectl(vm_offset_t va, int size, int op)
{
	if (op == DMA_CACHE_SYNC)
		m88110_cmmu_sync_cache(kvtop(va), size);
	else if (op == DMA_CACHE_SYNC_INVAL)
		m88110_cmmu_sync_inval_cache(kvtop(va), size);
	else
		m88110_cmmu_inval_cache(kvtop(va), size);
}

#ifdef DDB

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
m88110_cmmu_show_translation(unsigned address,
		unsigned supervisor_flag,
		unsigned verbose_flag,
		int cmmu_num)
{
	/*
	 * A virtual address is split into three fields. Two are used as
	 * indicies into tables (segment and page), and one is an offset into
	 * a page of memory.
	 */
	union {
		unsigned bits;
		struct {
			unsigned segment_table_index:10,
			page_table_index:10,
			page_offset:12;
		} field;
	} virtual_address;
	unsigned value;
	unsigned result;
	unsigned probeaddr;

	if (verbose_flag)
		db_printf("-------------------------------------------\n");

	if (supervisor_flag)
		value = get_dsap();
	else
		value = get_duap();

	/******* SEE WHAT A PROBE SAYS (if not a thread) ***********/

	set_dsar(address);
	if (supervisor_flag) {
		set_dcmd(CMMU_DCMD_PRB_SUPR);
	} else {
		set_dcmd(CMMU_DCMD_PRB_USER);
	}
	result = get_dsr();
	probeaddr = get_dsar();
	if (verbose_flag > 1)
		DEBUG_MSG("probe of 0x%08x returns dsr=0x%08x\n",
			  address, result);
	if (result & CMMU_DSR_PH || result & CMMU_DSR_BH) {
		DEBUG_MSG("probe of 0x%08x returns phys=0x%x",
			  address, probeaddr);
		if (result & CMMU_DSR_CP) DEBUG_MSG(", copyback err");
		if (result & CMMU_DSR_BE) DEBUG_MSG(", bus err");
		if (result & CMMU_DSR_TBE) DEBUG_MSG(", table search bus error");
		if (result & CMMU_DSR_SU) DEBUG_MSG(", sup prot");
		if (result & CMMU_DSR_WE) DEBUG_MSG(", write prot");
		if (result & CMMU_DSR_PH) DEBUG_MSG(", PATC");
		if (result & CMMU_DSR_BH) DEBUG_MSG(", BATC");
	} else {
		DEBUG_MSG("probe of 0x%08x missed the ATCs");
}
	DEBUG_MSG(".\n");

	/******* INTERPRET AREA DESCRIPTOR *********/
	{
		union apr_template apr_template;
		apr_template.bits = value;
		if (verbose_flag > 1) {
			DEBUG_MSG(" %cAPR is 0x%08x\n",
				  supervisor_flag ? 'S' : 'U', apr_template.bits);
		}
		DEBUG_MSG(" %cAPR: SegTbl: 0x%x000p",
			  supervisor_flag ? 'S' : 'U', apr_template.field.st_base);
		if (apr_template.field.wt) DEBUG_MSG(", WTHRU");
		else			   DEBUG_MSG(", !wthru");
		if (apr_template.field.g)  DEBUG_MSG(", GLOBAL");
		else			   DEBUG_MSG(", !global");
		if (apr_template.field.ci) DEBUG_MSG(", $INHIBIT");
		else			   DEBUG_MSG(", $ok");
		if (apr_template.field.te) DEBUG_MSG(", VALID");
		else			   DEBUG_MSG(", !valid");
		DEBUG_MSG(".\n");

		/* if not valid, done now */
		if (apr_template.field.te == 0) {
			DEBUG_MSG("<would report an error, valid bit not set>\n");
			return;
		}
		value = apr_template.field.st_base << 12; /* now point to seg page */
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	virtual_address.bits = address;

	/****** ACCESS SEGMENT TABLE AND INTERPRET SEGMENT DESCRIPTOR  *******/
	{
		union sdt_entry_template std_template;
		if (verbose_flag)
			DEBUG_MSG("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.segment_table_index, value);
		value |= virtual_address.field.segment_table_index *
			 sizeof(struct sdt_entry);

		if (badwordaddr((vm_offset_t)value)) {
			DEBUG_MSG("ERROR: unable to access page at 0x%08x.\n", value);
			return;
		}

		std_template.bits = *(unsigned *)value;
		if (verbose_flag > 1)
			DEBUG_MSG("SEG DESC @0x%x is 0x%08x\n", value, std_template.bits);
		DEBUG_MSG("SEG DESC @0x%x: PgTbl: 0x%x000",
			  value, std_template.sdt_desc.table_addr);
		if (std_template.sdt_desc.wt)	    DEBUG_MSG(", WTHRU");
		else				    DEBUG_MSG(", !wthru");
		if (std_template.sdt_desc.sup)	    DEBUG_MSG(", S-PROT");
		else				    DEBUG_MSG(", UserOk");
		if (std_template.sdt_desc.g)	    DEBUG_MSG(", GLOBAL");
		else				    DEBUG_MSG(", !global");
		if (std_template.sdt_desc.no_cache) DEBUG_MSG(", $INHIBIT");
		else				    DEBUG_MSG(", $ok");
		if (std_template.sdt_desc.prot)	    DEBUG_MSG(", W-PROT");
		else				    DEBUG_MSG(", WriteOk");
		if (std_template.sdt_desc.dtype)    DEBUG_MSG(", VALID");
		else				    DEBUG_MSG(", !valid");
		DEBUG_MSG(".\n");

		/* if not valid, done now */
		if (std_template.sdt_desc.dtype == 0) {
			DEBUG_MSG("<would report an error, STD entry not valid>\n");
			return;
		}
		value = std_template.sdt_desc.table_addr << 12;
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	/******* PAGE TABLE *********/
	{
		union pte_template pte_template;
		if (verbose_flag)
			DEBUG_MSG("will follow to entry %d of page at 0x%x...\n",
				  virtual_address.field.page_table_index, value);
		value |= virtual_address.field.page_table_index *
			 sizeof(struct pt_entry);

		if (badwordaddr((vm_offset_t)value)) {
			DEBUG_MSG("error: unable to access page at 0x%08x.\n", value);

			return;
		}

		pte_template.bits = *(unsigned *)value;
		if (verbose_flag > 1)
			DEBUG_MSG("PAGE DESC @0x%x is 0x%08x.\n", value, pte_template.bits);
		DEBUG_MSG("PAGE DESC @0x%x: page @%x000",
			  value, pte_template.pte.pfn);
		if (pte_template.pte.wired)    DEBUG_MSG(", WIRE");
		else			       DEBUG_MSG(", !wire");
		if (pte_template.pte.wt)       DEBUG_MSG(", WTHRU");
		else			       DEBUG_MSG(", !wthru");
		if (pte_template.pte.sup)      DEBUG_MSG(", S-PROT");
		else			       DEBUG_MSG(", UserOk");
		if (pte_template.pte.g)	       DEBUG_MSG(", GLOBAL");
		else			       DEBUG_MSG(", !global");
		if (pte_template.pte.ci)       DEBUG_MSG(", $INHIBIT");
		else			       DEBUG_MSG(", $ok");
		if (pte_template.pte.modified) DEBUG_MSG(", MOD");
		else			       DEBUG_MSG(", !mod");
		if (pte_template.pte.pg_used)  DEBUG_MSG(", USED");
		else			       DEBUG_MSG(", !used");
		if (pte_template.pte.prot)     DEBUG_MSG(", W-PROT");
		else			       DEBUG_MSG(", WriteOk");
		if (pte_template.pte.dtype)    DEBUG_MSG(", VALID");
		else			       DEBUG_MSG(", !valid");
		DEBUG_MSG(".\n");

		/* if not valid, done now */
		if (pte_template.pte.dtype == 0) {
			DEBUG_MSG("<would report an error, PTE entry not valid>\n");
			return;
		}

		value = pte_template.pte.pfn << 12;
		if (verbose_flag)
			DEBUG_MSG("will follow to byte %d of page at 0x%x...\n",
				  virtual_address.field.page_offset, value);
		value |= virtual_address.field.page_offset;

		if (badwordaddr((vm_offset_t)value)) {
			DEBUG_MSG("error: unable to access page at 0x%08x.\n", value);
			return;
		}
	}

	/* translate value from physical to virtual */
	if (verbose_flag)
		DEBUG_MSG("[%x physical is %x virtual]\n", value, value + VEQR_ADDR);
	value += VEQR_ADDR;

	DEBUG_MSG("WORD at 0x%x is 0x%08x.\n", value, *(unsigned *)value);
}


void
m88110_cmmu_cache_state(unsigned addr, unsigned supervisor_flag)
{
#ifdef not_yet
	static char *vv_name[4] =
	{"exclu-unmod", "exclu-mod", "shared-unmod", "invalid"};
	int cmmu_num;
#endif 
}

void
m88110_show_cmmu_info(unsigned addr)
{
	m88110_cmmu_cache_state(addr, 1);
}
#endif /* end if DDB */

#define MSDTENT(addr, va)	((sdt_entry_t *)(addr + SDTIDX(va)))
#define MPDTENT(addr, va)	((sdt_entry_t *)(addr + PDTIDX(va)))
void
m88110_load_patc(int entry, vm_offset_t vaddr, vm_offset_t paddr, int kernel)
{
	unsigned long lpa, pfa, i;

	lpa = (unsigned)vaddr & 0xFFFFF000;
	if (kernel) {
		lpa |= 0x01;
	}
	pfa = (unsigned)paddr & 0xFFFFF000;
	pfa |= 0x01;
	i = entry << 5;
	set_iir(i);
	set_ippu(lpa);
	set_ippl(pfa);
	set_dir(i);
	set_dppu(lpa);
	set_dppl(lpa);
}

#define SDT_WP(sd_ptr)  ((sd_ptr)->prot != 0)
#define SDT_SUP(sd_ptr)  ((sd_ptr)->sup != 0)
#define PDT_WP(pte_ptr)  ((pte_ptr)->prot != 0)
#define PDT_SUP(pte_ptr)  ((pte_ptr)->sup != 0)

int 
m88110_table_search(pmap_t map, vm_offset_t virt, int write, int kernel, int data)
{
	sdt_entry_t *sdt;
	pt_entry_t  *pte;
	unsigned long lpa, i;
	static unsigned int entry_num = 0;

	if (map == (pmap_t)0)
		panic("m88110_table_search: pmap is NULL");

	sdt = SDTENT(map, virt);

	/*
	 * Check whether page table exist or not.
	 */
	if (!SDT_VALID(sdt))
		return (4); /* seg fault */

	/* OK, it's valid.  Now check permissions. */
	if (!kernel && SDT_SUP(sdt))
			return (6); /* Supervisor Violation */
	if (write && SDT_WP(sdt))
			return (7); /* Write Violation */

	pte = (pt_entry_t *)(((sdt + SDT_ENTRIES)->table_addr)<<PDT_SHIFT) + PDTIDX(virt);
	/*
	 * Check whether page frame exist or not.
	 */
	if (!PDT_VALID(pte))
		return (5); /* Page Fault */

	/* OK, it's valid.  Now check permissions. */
	if (!kernel && PDT_SUP(pte))
			return (6); /* Supervisor Violation */
	if (write && PDT_WP(pte))
			return (7); /* Write Violation */
	/* If we get here, load the PATC. */
	entry_num++;
	if (entry_num > 32)
		entry_num = 0;
	lpa = (unsigned)virt & 0xFFFFF000;
	if (kernel)
		lpa |= 0x01;
	i = entry_num << 5;
	if (data) {
		set_dir(i); /* set PATC index */
		set_dppu(lpa); /* set logical address */
		set_dppl((unsigned)pte); /* set page fram address */
	} else {
		set_iir(i);
		set_ippu(lpa);
		set_ippl((unsigned)pte);
	}
	return 0;
}

#endif /* M88110 */


