/*	$OpenBSD: m882xx.h,v 1.4 1999/02/09 06:36:26 smurph Exp $ */
/* 
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 * HISTORY
 * 
 */


#ifndef	__MACHINE_M882XX_H__
#define	__MACHINE_M882XX_H__

#ifndef ASSEMBLER
# include <machine/mmu.h>		 /* batc_template_t */
#endif

#include <machine/board.h>

/*
 *	88200 CMMU definitions
 */
#define CMMU_IDR	0x000	/* CMMU id register */
#define CMMU_SCR	0x004	/* system command register */
#define CMMU_SSR	0x008	/* system status register */
#define CMMU_SAR	0x00C	/* system address register */
#define CMMU_SCTR	0x104	/* system control register */
#define CMMU_PFSR	0x108	/* P bus fault status register */
#define CMMU_PFAR	0x10C	/* P bus fault address register */
#define CMMU_SAPR	0x200	/* supervisor area pointer register */
#define CMMU_UAPR	0x204	/* user area pointer register */
#define CMMU_BWP0	0x400	/* block ATC writer port 0 */
#define CMMU_BWP1	0x404	/* block ATC writer port 1 */
#define CMMU_BWP2	0x408	/* block ATC writer port 2 */
#define CMMU_BWP3	0x40C	/* block ATC writer port 3 */
#define CMMU_BWP4	0x410	/* block ATC writer port 4 */
#define CMMU_BWP5	0x414	/* block ATC writer port 5 */
#define CMMU_BWP6	0x418	/* block ATC writer port 6 */
#define CMMU_BWP7	0x41C	/* block ATC writer port 7 */
#define CMMU_CDP0	0x800	/* cache data port 0 */
#define CMMU_CDP1	0x804	/* cache data port 1 */
#define CMMU_CDP2	0x808	/* cache data port 2 */
#define CMMU_CDP3	0x80C	/* cache data port 3 */
#define CMMU_CTP0	0x840	/* cache tag port 0 */
#define CMMU_CTP1	0x844	/* cache tag port 1 */
#define CMMU_CTP2	0x848	/* cache tag port 2 */
#define CMMU_CTP3	0x84C	/* cache tag port 3 */
#define CMMU_CSSP	0x880	/* cache set status register */

/* 88204 CMMU definitions  */
#define CMMU_CSSP0	0x880	/* cache set status register */
#define CMMU_CSSP1	0x890	/* cache set status register */
#define CMMU_CSSP2	0x8A0	/* cache set status register */
#define CMMU_CSSP3	0x8B0	/* cache set status register */

/* CMMU systerm commands */
#define CMMU_FLUSH_USER_LINE		0x30	/* flush PATC */
#define CMMU_FLUSH_USER_PAGE		0x31
#define CMMU_FLUSH_USER_SEGMENT		0x32
#define CMMU_FLUSH_USER_ALL		0x33
#define CMMU_FLUSH_SUPER_LINE		0x34
#define CMMU_FLUSH_SUPER_PAGE		0x35
#define CMMU_FLUSH_SUPER_SEGMENT	0x36
#define CMMU_FLUSH_SUPER_ALL		0x37
#define CMMU_PROBE_USER			0x20	/* probe user address */
#define CMMU_PROBE_SUPER		0x24	/* probe supervisor address */
#define CMMU_FLUSH_CACHE_INV_LINE	0x14	/* data cache invalidate */
#define CMMU_FLUSH_CACHE_INV_PAGE	0x15
#define CMMU_FLUSH_CACHE_INV_SEGMENT	0x16
#define CMMU_FLUSH_CACHE_INV_ALL	0x17
#define CMMU_FLUSH_CACHE_CB_LINE	0x18	/* data cache copyback */
#define CMMU_FLUSH_CACHE_CB_PAGE	0x19
#define CMMU_FLUSH_CACHE_CB_SEGMENT	0x1A
#define CMMU_FLUSH_CACHE_CB_ALL		0x1B
#define CMMU_FLUSH_CACHE_CBI_LINE	0x1C	/* copyback and invalidate */
#define CMMU_FLUSH_CACHE_CBI_PAGE	0x1D
#define CMMU_FLUSH_CACHE_CBI_SEGMENT	0x1E
#define CMMU_FLUSH_CACHE_CBI_ALL	0x1F

/* CMMU system control command */
#define CMMU_SCTR_PE	0x00008000	/* parity enable */
#define CMMU_SCTR_SE	0x00004000	/* snoop enable */
#define CMMU_SCTR_PR	0x00002000	/* priority arbitration */

/* CMMU P bus fault status */
#define CMMU_PFSR_SUCCESS	0	/* no fault */
#define CMMU_PFSR_BERROR	3	/* bus error */
#define CMMU_PFSR_SFAULT	4	/* segment fault */
#define CMMU_PFSR_PFAULT	5	/* page fault */
#define CMMU_PFSR_SUPER		6	/* supervisor violation */
#define CMMU_PFSR_WRITE		7	/* writer violation */

/* Area Description */
#define AREA_D_WT	0x00000200	/* write through */
#define AREA_D_G	0x00000080	/* global */
#define AREA_D_CI	0x00000040	/* cache inhibit */
#define AREA_D_TE	0x00000001	/* translation enable */

/* Segment Description */
#define SEG_D_WT	0x00000200	/* write through */
#define SEG_D_SP	0x00000100	/* supervisor protection */
#define SEG_D_G		0x00000080	/* global */
#define SEG_D_CI	0x00000040	/* cache inhibit */
#define SEG_D_WP	0x00000004	/* write protect */
#define SEG_D_V		0x00000001	/* valid */

/*
 * Flags for cmmu_flush_tlb
 */
#define FLUSH_KERNEL    1
#define FLUSH_USER      0
#define FLUSH_ALL       ((vm_offset_t)~0)


#ifndef	ASSEMBLER
/*
 * This file defines the data structures for the mmu.
 * One major data structure, the page descriptor, is not defined here
 * but rather in pte.h as struct pte.
 */

struct area_d {		/* area descriptor */
    unsigned
	ad_addr:20,	/* segment table base address */
	       : 2,
	ad_wt  : 1,	/* write through */
	       : 1,
	ad_g   : 1,	/* global */
	ad_ci  : 1,	/* cache inhibit */
	       : 5,
	ad_te  : 1;	/* translation enable */
};

struct segment_d {	/* segment descriptor */
    unsigned
	sd_addr:20,	/* page table base address */
	       : 2,
	sd_wt  : 1,	/* write through */ 
	sd_sp  : 1,	/* supervisor protection */
	sd_g   : 1,	/* global */
	sd_ci  : 1,	/* cache inhibit */
	       : 3,
	sd_wp  : 1,	/* write protect */
	       : 1,
	sd_v   : 1;	/* valid */
};

typedef	struct segment_d segment_d_t;

struct pfsr {		/* P bus fault status register */
    unsigned
	       :13,
	pfsr_fc: 3,	/* falut code */
	       :16;
};

struct batc {		/* block address translation register */
    unsigned
	batc_lba:13,	/* logical block address */
	batc_pba:13,	/* physical block address */
	batc_s  : 1,	/* supervisor */
	batc_wt : 4,	/* write through */
	batc_g  : 1,	/* global */
	batc_ci : 1,	/* cache inhibit */
	batc_wp : 1,	/* write protect */
	batc_v  : 1;	/* valid */
};

/*
 * Prototypes and stuff for cmmu.c.
 */
extern unsigned cpu_sets[MAX_CPUS];
extern unsigned ncpus;
extern unsigned cache_policy;

#ifdef CMMU_DEBUG
 void show_apr(unsigned value);
 void show_sctr(unsigned value);
#endif

/*
 * Prototypes from "motorola/m88k/m88100/cmmu.c"
 */
unsigned cmmu_cpu_number(void);
#if !DDB
static
#endif /* !DDB */
unsigned cmmu_remote_get(unsigned cpu, unsigned r, unsigned data);
unsigned cmmu_get_idr(unsigned data);
void cmmu_init(void);
void cmmu_shutdown_now(void);
void cmmu_parity_enable(void);
#if !DDB
static
#endif /* !DDB */
void cmmu_remote_set(unsigned cpu, unsigned r, unsigned data, unsigned x);
void cmmu_set_sapr(unsigned ap);
void cmmu_remote_set_sapr(unsigned cpu, unsigned ap);
void cmmu_set_uapr(unsigned ap);
void cmmu_flush_tlb(unsigned kernel, vm_offset_t vaddr, int size);
void cmmu_flush_remote_cache(int cpu, vm_offset_t physaddr, int size);
void cmmu_flush_cache(vm_offset_t physaddr, int size);
void cmmu_flush_remote_inst_cache(int cpu, vm_offset_t physaddr, int size);
void cmmu_flush_inst_cache(vm_offset_t physaddr, int size);
void cmmu_flush_remote_data_cache(int cpu, vm_offset_t physaddr, int size);
void cmmu_flush_data_cache(vm_offset_t physaddr, int size);

void cmmu_pmap_activate(
    unsigned cpu,
    unsigned uapr,
    batc_template_t i_batc[BATC_MAX],
    batc_template_t d_batc[BATC_MAX]);

void cmmu_flush_remote_tlb(
	unsigned cpu,
	unsigned kernel,
	vm_offset_t vaddr,
	int size);

void cmmu_set_batc_entry(
     unsigned cpu,
     unsigned entry_no,
     unsigned data,   /* 1 = data, 0 = instruction */
     unsigned value);  /* the value to stuff into the batc */

void cmmu_set_pair_batc_entry(
     unsigned cpu,
     unsigned entry_no,
     unsigned value);  /* the value to stuff into the batc */

#endif	/* ASSEMBLER */

#define INST_CMMU 0
#define DATA_CMMU 1

#define NBSG    (4*1024*1024) /* segment size */

#endif	/* __MACHINE_M882XX_H__ */
