/*	$OpenBSD: cmmu.c,v 1.6 2000/03/03 00:54:53 todd Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
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
#include <sys/types.h>
#include <sys/simplelock.h>
#include <machine/board.h>
#include <machine/cpus.h>
#include <machine/cpu_number.h>
#include <machine/cmmu.h>
#if defined(MVME187) || defined(MVME188)
#include <machine/m882xx.h>
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
#include <machine/m88110.h>
#endif /* MVME197 */

/* 
 * This lock protects the cmmu SAR and SCR's; other ports 
 * can be accessed without locking it 
 *
 * May be used from "db_interface.c".
 */
struct simplelock cmmu_cpu_lock;

#define CMMU_LOCK   simple_lock(&cmmu_cpu_lock)
#define CMMU_UNLOCK simple_unlock(&cmmu_cpu_lock)

unsigned cache_policy = /*CACHE_INH*/ 0;
unsigned cpu_sets[MAX_CPUS];
unsigned number_cpus = 0;
unsigned master_cpu = 0;
int      vme188_config;
int      max_cpus, max_cmmus;
int      cpu_cmmu_ratio;

void
show_apr(unsigned value)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_show_apr(value);
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_show_apr(value);
      break;
#endif 
   }
}

void
show_sctr(unsigned value)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_show_sctr(value);
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_show_sctr(value);
      break;
#endif 
   }
}

void 
setup_board_config(void)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_setup_board_config();
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_setup_board_config();
      break;
#endif 
   }
}

void 
setup_cmmu_config(void)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_setup_cmmu_config();
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_setup_cmmu_config();
      break;
#endif 
   }
   return;
}

void 
cmmu_dump_config(void)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_dump_config();
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_cmmu_dump_config();
      break;
#endif 
   }
   return;
}

#ifdef DDB
/*
 * Used by DDB for cache probe functions
 */
unsigned 
cmmu_get_by_mode(int cpu, int mode)
{
   unsigned retval;
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      retval = m18x_cmmu_get_by_mode(cpu, mode);
      break;
#endif
#ifdef MVME197
   case CPU_197:
      retval = m197_cmmu_get_by_mode(cpu, mode);
      break;
#endif 
   }
   CMMU_UNLOCK;
   return retval;
}
#endif

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void 
cpu_configuration_print(int master)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cpu_configuration_print(master);
      break;
#endif
#ifdef MVME197
   case CPU_197:
      m197_cpu_configuration_print(master);
      break;
#endif 
   }
   CMMU_UNLOCK;
   return;
}

/*
 * CMMU initialization routine
 */
void 
cmmu_init(void)
{
   /* init the lock */
   simple_lock_init(&cmmu_cpu_lock);

   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_init();
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_init();
      break;
#endif /* MVME197 */
   }
   return;
}

/*
 * Just before poweroff or reset....
 */
void
cmmu_shutdown_now(void)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_shutdown_now();
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_shutdown_now();
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

#define PARITY_ENABLE

/*
 * enable parity
 */
void 
cmmu_parity_enable(void)
{
#ifdef	PARITY_ENABLE
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_parity_enable();
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_parity_enable();
      break;
#endif /* MVME197 */
   }
#endif  /* PARITY_ENABLE */
   CMMU_UNLOCK;
   return;
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */
unsigned 
cmmu_cpu_number(void)
{
   unsigned retval;
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      retval = m18x_cmmu_cpu_number();
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      retval = m197_cmmu_cpu_number();
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return retval;
}

/**
 **	Funcitons that actually modify CMMU registers.
 **/

#if !DDB
static
#endif
void
cmmu_remote_set(unsigned cpu, unsigned r, unsigned data, unsigned x)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_remote_set(cpu, r, data, x);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_remote_set(cpu, r, data, x);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 * cmmu_cpu_lock should be held when called if read
 * the CMMU_SCR or CMMU_SAR.
 */
#if !DDB
static
#endif
unsigned
cmmu_remote_get(unsigned cpu, unsigned r, unsigned data)
{
   unsigned retval;
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      retval = m18x_cmmu_remote_get(cpu, r, data);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      retval = m197_cmmu_remote_get(cpu, r, data);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return retval;
}

/* Needs no locking - read only registers */
unsigned
cmmu_get_idr(unsigned data)
{
   unsigned retval;
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      retval = m18x_cmmu_get_idr(data);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      retval = m197_cmmu_get_idr(data);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return retval;
}

void
cmmu_set_sapr(unsigned ap)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_set_sapr(ap);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_set_sapr(ap);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

void
cmmu_remote_set_sapr(unsigned cpu, unsigned ap)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_remote_set_sapr(cpu, ap);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_remote_set_sapr(cpu, ap);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

void
cmmu_set_uapr(unsigned ap)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_set_uapr(ap);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_set_uapr(ap);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 * Set batc entry number entry_no to value in 
 * the data or instruction cache depending on data.
 *
 * Except for the cmmu_init, this function, cmmu_set_pair_batc_entry,
 * and cmmu_pmap_activate are the only functions which may set the
 * batc values.
 */
void
cmmu_set_batc_entry(
                   unsigned cpu,
                   unsigned entry_no,
                   unsigned data,   /* 1 = data, 0 = instruction */
                   unsigned value)  /* the value to stuff into the batc */
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_set_batc_entry(cpu, entry_no, data, value);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_set_batc_entry(cpu, entry_no, data, value);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 * Set batc entry number entry_no to value in 
 * the data and instruction cache for the named CPU.
 */
void
cmmu_set_pair_batc_entry(
                        unsigned cpu,
                        unsigned entry_no,
                        unsigned value)  /* the value to stuff into the batc */
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_set_pair_batc_entry(cpu, entry_no, value);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_set_pair_batc_entry(cpu, entry_no, value);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/**
 **	Functions that invalidate TLB entries.
 **/

/*
 *	flush any tlb
 *	Some functionality mimiced in cmmu_pmap_activate.
 */
void
cmmu_flush_remote_tlb(unsigned cpu, unsigned kernel, vm_offset_t vaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 *	flush my personal tlb
 */
void
cmmu_flush_tlb(unsigned kernel, vm_offset_t vaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
}

/*
 * New fast stuff for pmap_activate.
 * Does what a few calls used to do.
 * Only called from pmap.c's _pmap_activate().
 */
void
cmmu_pmap_activate(
                  unsigned cpu,
                  unsigned uapr,
                  batc_template_t i_batc[BATC_MAX],
                  batc_template_t d_batc[BATC_MAX])
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_pmap_activate(cpu, uapr, i_batc, d_batc);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_pmap_activate(cpu, uapr, i_batc, d_batc);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
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
 *	flush both Instruction and Data caches
 */
void
cmmu_flush_remote_cache(int cpu, vm_offset_t physaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_flush_remote_cache(cpu, physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_flush_remote_cache(cpu, physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 *	flush both Instruction and Data caches
 */
void
cmmu_flush_cache(vm_offset_t physaddr, int size)
{
   int cpu = cpu_number();
   cmmu_flush_remote_cache(cpu, physaddr, size);
}

/*
 *	flush Instruction caches
 */
void
cmmu_flush_remote_inst_cache(int cpu, vm_offset_t physaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_flush_remote_inst_cache(cpu, physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_flush_remote_inst_cache(cpu, physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 *	flush Instruction caches
 */
void
cmmu_flush_inst_cache(vm_offset_t physaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   cmmu_flush_remote_inst_cache(cpu, physaddr, size);
}

void
cmmu_flush_remote_data_cache(int cpu, vm_offset_t physaddr, int size)
{ 
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_flush_remote_data_cache(cpu, physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_flush_remote_data_cache(cpu, physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

/*
 * flush data cache
 */ 
void
cmmu_flush_data_cache(vm_offset_t physaddr, int size)
{ 
   int cpu;
   cpu = cpu_number();
   cmmu_flush_remote_data_cache(cpu, physaddr, size);
}

/*
 * sync dcache (and icache too)
 */
static void
cmmu_sync_cache(vm_offset_t physaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_sync_cache(physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_sync_cache(physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

static void
cmmu_sync_inval_cache(vm_offset_t physaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_sync_inval_cache(physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_sync_inval_cache(physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

static void
cmmu_inval_cache(vm_offset_t physaddr, int size)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_inval_cache(physaddr, size);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_inval_cache(physaddr, size);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

void
dma_cachectl(vm_offset_t va, int size, int op)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_dma_cachectl(va, size, op);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_dma_cachectl(va, size, op);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}

#if DDB

/*
 * Show (for debugging) how the given CMMU translates the given ADDRESS.
 * If cmmu == -1, the data cmmu for the current cpu is used.
 */
void
cmmu_show_translation(
                     unsigned address,
                     unsigned supervisor_flag,
                     unsigned verbose_flag,
                     int cmmu_num)
{
   CMMU_LOCK;
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_show_translation(address, supervisor_flag, 
                                 verbose_flag, cmmu_num);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_show_translation(address, supervisor_flag, 
                                 verbose_flag, cmmu_num);
      break;
#endif /* MVME197 */
   }
   CMMU_UNLOCK;
   return;
}


void
cmmu_cache_state(unsigned addr, unsigned supervisor_flag)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_cmmu_cache_state(addr, supervisor_flag);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_cmmu_cache_state(addr, supervisor_flag);
      break;
#endif /* MVME197 */
   }
   return;
}

void
show_cmmu_info(unsigned addr)
{
   switch (cputyp) {
#if defined(MVME187) || defined(MVME188)
   case CPU_187:
   case CPU_188:
      m18x_show_cmmu_info(addr);
      break;
#endif /* defined(MVME187) || defined(MVME188) */
#ifdef MVME197
   case CPU_197:
      m197_show_cmmu_info(addr);
      break;
#endif /* MVME197 */
   }
   return;
}
#endif /* end if DDB */
