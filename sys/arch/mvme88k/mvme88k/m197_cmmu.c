/*	$OpenBSD: m197_cmmu.c,v 1.2 2000/03/03 00:54:53 todd Exp $	*/
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
#ifdef MVME197

#include <sys/param.h>
#include <sys/types.h>
#include <sys/simplelock.h>
#include <machine/board.h>
#include <machine/cpus.h>
#include <machine/cpu_number.h>
#include <machine/m88110.h>

#define CMMU_DEBUG 1

#ifdef DEBUG
   #define DB_CMMU		0x4000	/* MMU debug */
unsigned int debuglevel = 0;
   #define dprintf(_L_,_X_) { if (debuglevel & (_L_)) { unsigned int psr = disable_interrupts_return_psr(); printf("%d: ", cpu_number()); printf _X_;  set_psr(psr); } }
#else
   #define dprintf(_L_,_X_)
#endif 
#undef	SHADOW_BATC		/* don't use BATCs for now XXX nivas */

/*
 * CMMU(cpu,data) Is the cmmu struct for the named cpu's indicated cmmu.
 * REGS(cpu,data) is the actual register structure.
 */

#define CMMU(cpu, data) cpu_cmmu[(cpu)].pair[(data)?DATA_CMMU:INST_CMMU]
#define REGS(cpu, data) (*CMMU(cpu, data)->cmmu_regs)

/* 
 * This lock protects the cmmu SAR and SCR's; other ports 
 * can be accessed without locking it 
 *
 * May be used from "db_interface.c".
 */

extern unsigned cache_policy;
extern unsigned cpu_sets[];
extern unsigned number_cpus;
extern unsigned master_cpu;
extern int      max_cpus, max_cmmus;
extern int      cpu_cmmu_ratio;
int init_done;

/* FORWARDS */
void m197_setup_cmmu_config(void);
void m197_setup_board_config(void);

#ifdef CMMU_DEBUG
void
m197_show_apr(unsigned value)
{
   union apr_template apr_template;
   apr_template.bits = value;

   printf("table @ 0x%x000", apr_template.field.st_base);
   if (apr_template.field.wt) printf(", writethrough");
   if (apr_template.field.g)  printf(", global");
   if (apr_template.field.ci) printf(", cache inhibit");
   if (apr_template.field.te) printf(", valid");
   else                       printf(", not valid");
   printf("\n");
}

void
m197_show_sctr(unsigned value)
{
   union {
      unsigned bits;
      struct {
         unsigned :16,
         pe: 1,
         se: 1,
         pr: 1,
         :13;
      } fields;
   } sctr; 
   sctr.bits = value;
   printf("%spe, %sse %spr]\n",
          sctr.fields.pe ? "" : "!",
          sctr.fields.se ? "" : "!",
          sctr.fields.pr ? "" : "!");
}
#endif

void 
m197_setup_board_config(void)
{
   /* dummy routine */
   m197_setup_cmmu_config();
   return;
}

void 
m197_setup_cmmu_config(void)
{
   /* we can print something here... */
   cpu_sets[0] = 1;   /* This cpu installed... */
   return;
}

void m197_cmmu_dump_config(void)
{
   /* dummy routine */
   return;
}

/* To be implemented as a macro for speedup - XXX-smurph */
static void 
m197_cmmu_store(int mmu, int reg, unsigned val)
{
}

int m197_cmmu_alive(int mmu)
{
   return 1;
}

unsigned m197_cmmu_get(int mmu, int reg)
{
   unsigned val;
   return val;
}

/*
 * This function is called by the MMU module and pokes values
 * into the CMMU's registers.
 */
void m197_cmmu_set(int reg, unsigned val, int flags,
              int num, int mode, int access, vm_offset_t addr)
{
   return;
}

#ifdef DDB
/*
 * Used by DDB for cache probe functions
 */
unsigned m197_cmmu_get_by_mode(int cpu, int mode)
{
   return 0;
}
#endif

/*
 * Should only be called after the calling cpus knows its cpu
 * number and master/slave status . Should be called first
 * by the master, before the slaves are started.
*/
void m197_cpu_configuration_print(int master)
{
   int pid = read_processor_identification_register();
   int proctype = (pid & 0xff00) >> 8;
   int procvers = (pid & 0xe) >> 1;
   int mmu, cpu = cpu_number();
   struct simplelock print_lock;

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
   return;
}

/*
 * CMMU initialization routine
 */
void m197_load_patc(int entry, vm_offset_t vaddr, vm_offset_t paddr, int kernel);

void
m197_cmmu_init(void)
{
   int i;
   unsigned tmp;
   extern void *kernel_sdt;
   unsigned lba, pba, value;
   init_done = 0;

   /* clear BATCs */
   for (i=0; i<8; i++) {
      m197_cmmu_set_pair_batc_entry(0, i, 0);
   }
   /* clear PATCs */
   for (i=0; i<32; i++) {
      m197_load_patc(i, 0, 0, 0);
   }
   set_ictl(CMMU_ICTL_DID         /* Double instruction disable */
            | CMMU_ICTL_MEN
            | CMMU_ICTL_HTEN);
            

   set_dctl(CMMU_DCTL_MEN
            | CMMU_DCTL_HTEN);      

   set_icmd(CMMU_ICMD_INV_ITIC);  /* clear instruction cache */
   set_dcmd(CMMU_DCMD_INV_ALL);   /* clear data cache */

   tmp = (0x00000 << 12) | /* segment table base address */
      AREA_D_WT |       /* write through */
      AREA_D_G  |       /* global */
      ! AREA_D_TE ;     /* not translation enable */
   
   set_isap(tmp);
   set_dsap(tmp);

   set_isr(0);
   set_ilar(0);
   set_ipar(0);
   set_dsr(0);
   set_dlar(0);
   set_dpar(0);
   
   lba = pba = (unsigned)&kernel_sdt;
   lba &= ~0x7FFFF;
   pba = pba >> 13;
   pba &= ~0x3F;
   value = lba | pba | 0x20 | 0x01;
   
   m197_cmmu_set_pair_batc_entry(0, 0, value);
 
}


/*
 * Just before poweroff or reset....
 */
void
m197_cmmu_shutdown_now(void)
{
   unsigned tmp;
   unsigned cmmu_num;

}

/*
 * enable parity
 */
void m197_cmmu_parity_enable(void)
{
#ifdef	PARITY_ENABLE
#endif  /* PARITY_ENABLE */
}

/*
 * Find out the CPU number from accessing CMMU
 * Better be at splhigh, or even better, with interrupts
 * disabled.
 */
#define ILLADDRESS	U(0x0F000000) 	/* any faulty address */

unsigned m197_cmmu_cpu_number(void)
{
   return 0; /* to make compiler happy */
}

/**
 **	Funcitons that actually modify CMMU registers.
 **/
#if !DDB
static
#endif
void
m197_cmmu_remote_set(unsigned cpu, unsigned r, unsigned data, unsigned x)
{
   panic("m197_cmmu_remote_set() called!\n");
}

/*
 * cmmu_cpu_lock should be held when called if read
 * the CMMU_SCR or CMMU_SAR.
 */
#if !DDB
static
#endif
unsigned
m197_cmmu_remote_get(unsigned cpu, unsigned r, unsigned data)
{
   panic("m197_cmmu_remote_get() called!\n");
   return 0;
}

/* Needs no locking - read only registers */
unsigned
m197_cmmu_get_idr(unsigned data)
{
   return 0; /* todo */
}

int 
probe_mmu(vm_offset_t va, int data)
{
   unsigned result;
   if (data) {
      set_dsar((unsigned)va);
      set_dcmd(CMMU_DCMD_PRB_SUPR);
      result = get_dsr();
      if (result & CMMU_DSR_PH) 
         return 1;
      else
         return 0;
   } else {
      set_isar((unsigned)va);
      set_icmd(CMMU_ICMD_PRB_SUPR);
      result = get_isr();
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
m197_cmmu_set_sapr(unsigned ap)
{
   int result;
   set_icmd(CMMU_ICMD_INV_SATC);
   set_dcmd(CMMU_DCMD_INV_SATC);
   /* load an entry pointing to seg table into PATC */
   /* Don't forget to set it valid */
   
   m197_load_patc(0, (vm_offset_t)ap, (vm_offset_t)(ap | 0x1), 1);
   if(!(result = probe_mmu((vm_offset_t) ap, 1))){
      printf("Didn't make it!!!!\n");
      return;
   } else {
      if (result == 2)
         printf("area pointer is in BATC.\n");
      if (result == 1)
         printf("area pointer is in PATC.\n");
   }
   
   set_isap(ap);
   set_dsap(ap);
}

void
m197_cmmu_remote_set_sapr(unsigned cpu, unsigned ap)
{
   m197_cmmu_set_sapr(ap);
}

void
m197_cmmu_set_uapr(unsigned ap)
{
   set_iuap(ap);
   set_duap(ap);
}

/*
 * Set batc entry number entry_no to value in 
 * the data or instruction cache depending on data.
 *
 * Except for the cmmu_init, this function, m197_cmmu_set_pair_batc_entry,
 * and m197_cmmu_pmap_activate are the only functions which may set the
 * batc values.
 */
void
m197_cmmu_set_batc_entry(
                   unsigned cpu,
                   unsigned entry_no,
                   unsigned data,   /* 1 = data, 0 = instruction */
                   unsigned value)  /* the value to stuff */
{
   if (data) {
      set_dir(entry_no);
      set_dbp(value);
   } else {
      set_iir(entry_no);
      set_ibp(value);
   }
}

/*
 * Set batc entry number entry_no to value in 
 * the data and instruction cache for the named CPU.
 */
void
m197_cmmu_set_pair_batc_entry(unsigned cpu, unsigned entry_no, unsigned value)
/* the value to stuff into the batc */
{
   m197_cmmu_set_batc_entry(cpu, entry_no, 1, value);
   m197_cmmu_set_batc_entry(cpu, entry_no, 0, value);
}

/**
 **	Functions that invalidate TLB entries.
 **/

/*
 *	flush any tlb
 *	Some functionality mimiced in m197_cmmu_pmap_activate.
 */
void
m197_cmmu_flush_remote_tlb(unsigned cpu, unsigned kernel, vm_offset_t vaddr, int size)
{
   register s = splhigh();
   if (kernel) {
      set_icmd(CMMU_ICMD_INV_SATC);
      set_dcmd(CMMU_DCMD_INV_SATC);
   } else {
      set_icmd(CMMU_ICMD_INV_UATC);
      set_dcmd(CMMU_DCMD_INV_UATC);
   }
   splx(s);
}

/*
 *	flush my personal tlb
 */
void
m197_cmmu_flush_tlb(unsigned kernel, vm_offset_t vaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   m197_cmmu_flush_remote_tlb(cpu, kernel, vaddr, size);
}

/*
 * New fast stuff for pmap_activate.
 * Does what a few calls used to do.
 * Only called from pmap.c's _pmap_activate().
 */
void
m197_cmmu_pmap_activate(
                  unsigned cpu,
                  unsigned uapr,
                  batc_template_t i_batc[BATC_MAX],
                  batc_template_t d_batc[BATC_MAX])
{
   int entry_no;

   m197_cmmu_set_uapr(uapr);

   /*
   for (entry_no = 0; entry_no < 8; entry_no++) {
      m197_cmmu_set_batc_entry(cpu, entry_no, 0, i_batc[entry_no].bits);
      m197_cmmu_set_batc_entry(cpu, entry_no, 1, d_batc[entry_no].bits);
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
 *	flush both Instruction and Data caches
 */
void
m197_cmmu_flush_remote_cache(int cpu, vm_offset_t physaddr, int size)
{
   register s = splhigh();
   set_icmd(CMMU_ICMD_INV_ITIC);
   set_dcmd(CMMU_DCMD_FLUSH_ALL_INV);
   splx(s);
}

/*
 *	flush both Instruction and Data caches
 */
void
m197_cmmu_flush_cache(vm_offset_t physaddr, int size)
{
   int cpu = cpu_number();
   m197_cmmu_flush_remote_cache(cpu, physaddr, size);
}

/*
 *	flush Instruction caches
 */
void
m197_cmmu_flush_remote_inst_cache(int cpu, vm_offset_t physaddr, int size)
{
   register s = splhigh();
   
   set_icmd(CMMU_ICMD_INV_ITIC);

   splx(s);
}

/*
 *	flush Instruction caches
 */
void
m197_cmmu_flush_inst_cache(vm_offset_t physaddr, int size)
{
   int cpu;
   cpu = cpu_number();
   m197_cmmu_flush_remote_inst_cache(cpu, physaddr, size);
}

/*
 * flush data cache
 */ 
void
m197_cmmu_flush_remote_data_cache(int cpu, vm_offset_t physaddr, int size)
{ 
   register s = splhigh();
   set_dcmd(CMMU_DCMD_FLUSH_ALL_INV);
   splx(s);
}

/*
 * flush data cache
 */ 
void
m197_cmmu_flush_data_cache(vm_offset_t physaddr, int size)
{ 
   int cpu;
   cpu = cpu_number();
   m197_cmmu_flush_remote_data_cache(cpu, physaddr, size);
}

/*
 * sync dcache (and icache too)
 */
void
m197_cmmu_sync_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();
   /* set_mmureg(CMMU_ICTL, CMMU_ICMD_INV_TIC); */ 
   set_dcmd(CMMU_DCMD_FLUSH_ALL);

   splx(s);
}

void
m197_cmmu_sync_inval_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();

   set_dcmd(CMMU_DCMD_FLUSH_ALL_INV);
   splx(s);
}

void
m197_cmmu_inval_cache(vm_offset_t physaddr, int size)
{
   register s = splhigh();
   int cpu;
   cpu = cpu_number();
   set_icmd(CMMU_ICMD_INV_ITIC);
   set_dcmd(CMMU_DCMD_INV_ALL);
   splx(s);
}

void
m197_dma_cachectl(vm_offset_t va, int size, int op)
{
   int count;
   if (op == DMA_CACHE_SYNC)
      m197_cmmu_sync_cache(kvtop(va), size);
   else if (op == DMA_CACHE_SYNC_INVAL)
      m197_cmmu_sync_inval_cache(kvtop(va), size);
   else
      m197_cmmu_inval_cache(kvtop(va), size);
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
m197_cmmu_show_translation(
                     unsigned address,
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

   if (verbose_flag)
      db_printf("-------------------------------------------\n");

}


void
m197_cmmu_cache_state(unsigned addr, unsigned supervisor_flag)
{
   static char *vv_name[4] =
   {"exclu-unmod", "exclu-mod", "shared-unmod", "invalid"};
   int cmmu_num;
}

void
m197_show_cmmu_info(unsigned addr)
{
   int cmmu_num;
   m197_cmmu_cache_state(addr, 1);
}
#endif /* end if DDB */

#define MSDTENT(addr, va)	((sdt_entry_t *)(addr + SDTIDX(va)))
#define MPDTENT(addr, va)	((sdt_entry_t *)(addr + PDTIDX(va)))
void
m197_load_patc(int entry, vm_offset_t vaddr, vm_offset_t paddr, int kernel)
{
   unsigned lpa, pfa, i;
   
   lpa = (unsigned)vaddr & 0xFFFFF000;
   if (kernel) {
      lpa |= 0x01;
   }
   pfa = (unsigned)paddr;
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
m197_table_search(pmap_t map, vm_offset_t virt, int write, int kernel, int data)
{
   sdt_entry_t *sdt;
   pt_entry_t  *pte;
   unsigned lpa, pfa, i;
   static entry_num = 0;

   if (map == (pmap_t)0)
      panic("m197_table_search: pmap is NULL");

   sdt = SDTENT(map, virt);

   /*
    * Check whether page table exist or not.
    */
   if (!SDT_VALID(sdt))
      return (4); /* seg fault */
   
   /* OK, it's valid.  Now check permissions. */
   if (!kernel)
      if (SDT_SUP(sdt))
         return (6); /* Supervisor Violation */
   if (write)
      if (SDT_WP(sdt))
         return (7); /* Write Violation */
   
   else
   pte = (pt_entry_t *)(((sdt + SDT_ENTRIES)->table_addr)<<PDT_SHIFT) + PDTIDX(virt);
   /*
    * Check whether page frame exist or not.
    */
   if (!PDT_VALID(pte))
      return (5); /* Page Fault */
   
   /* OK, it's valid.  Now check permissions. */
   if (!kernel)
      if (PDT_SUP(sdt))
         return (6); /* Supervisor Violation */
   if (write)
      if (PDT_WP(sdt))
         return (7); /* Write Violation */
   /* If we get here, load the PATC. */
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

#endif /* MVME197 */


