/*	$OpenBSD: cmmu.h,v 1.1 1999/09/27 20:46:17 smurph Exp $ */
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


#ifndef	__MACHINE_CMMU_H__
#define	__MACHINE_CMMU_H__

#ifndef ASSEMBLER
#include <machine/mmu.h>		 /* batc_template_t */
#endif

#include <machine/board.h>

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
 * Prototypes and stuff for cmmu.c.
 */
extern unsigned cpu_sets[MAX_CPUS];
extern int cpu_cmmu_ratio;
extern unsigned ncpus;
extern unsigned cache_policy;

#ifdef CMMU_DEBUG
 void show_apr(unsigned value);
 void show_sctr(unsigned value);
#endif

/*
 * Prototypes from "mvme88k/mvme88k/cmmu.c"
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

#endif	/* __MACHINE_CMMU_H__ */
