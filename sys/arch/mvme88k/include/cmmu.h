/*	$OpenBSD: cmmu.h,v 1.7 2001/12/13 08:55:51 smurph Exp $ */
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

#ifndef	_MACHINE_CMMU_H_
#define	_MACHINE_CMMU_H_

#include <machine/mmu.h>

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


#ifndef	_LOCORE
/*
 * Prototypes and stuff for cmmu.c.
 */
extern unsigned cpu_sets[MAX_CPUS];
extern int cpu_cmmu_ratio;
extern unsigned number_cpus, master_cpu;
extern unsigned cache_policy;
extern unsigned number_cpus;
extern unsigned master_cpu;
extern int      max_cpus, max_cmmus;

/* 
 * This lock protects the cmmu SAR and SCR's; other ports 
 * can be accessed without locking it 
 *
 * May be used from "db_interface.c".
 */
extern struct simplelock cmmu_cpu_lock;

#define CMMU_LOCK   simple_lock(&cmmu_cpu_lock)
#define CMMU_UNLOCK simple_unlock(&cmmu_cpu_lock)

/* machine dependant cmmu function pointer structure */
struct cmmu_p {
	void (*cmmu_init_func) __P((void));
	void (*show_apr_func) __P((unsigned));
	void (*setup_board_config_func) __P((void));
	void (*setup_cmmu_config_func) __P((void));
	void (*cmmu_dump_config_func) __P((void));
	void (*cpu_configuration_print_func) __P((int));
	void (*cmmu_shutdown_now_func) __P((void));
	void (*cmmu_parity_enable_func) __P((void));
	unsigned (*cmmu_cpu_number_func) __P((void));
	void (*cmmu_remote_set_func) __P((unsigned, unsigned, unsigned, unsigned));
	unsigned (*cmmu_remote_get_func) __P((unsigned, unsigned, unsigned));
	unsigned (*cmmu_get_idr_func) __P((unsigned));
	void (*cmmu_set_sapr_func) __P((unsigned));
	void (*cmmu_remote_set_sapr_func) __P((unsigned, unsigned));
	void (*cmmu_set_uapr_func) __P((unsigned));
	void (*cmmu_set_batc_entry_func) __P((unsigned, unsigned, unsigned, unsigned));
	void (*cmmu_set_pair_batc_entry_func) __P((unsigned, unsigned, unsigned));
	void (*cmmu_flush_remote_tlb_func) __P((unsigned, unsigned, vm_offset_t, int));
	void (*cmmu_flush_tlb_func) __P((unsigned, vm_offset_t, int));
	void (*cmmu_pmap_activate_func) __P((unsigned, unsigned,
					     batc_template_t i_batc[BATC_MAX],
					     batc_template_t d_batc[BATC_MAX]));
	void (*cmmu_flush_remote_cache_func) __P((int, vm_offset_t, int));
	void (*cmmu_flush_cache_func) __P((vm_offset_t, int));
	void (*cmmu_flush_remote_inst_cache_func) __P((int, vm_offset_t, int));
	void (*cmmu_flush_inst_cache_func) __P((vm_offset_t, int));
	void (*cmmu_flush_remote_data_cache_func) __P((int, vm_offset_t, int));
	void (*cmmu_flush_data_cache_func) __P((vm_offset_t, int));
	void (*dma_cachectl_func) __P((vm_offset_t, int, int));
#ifdef DDB
	unsigned (*cmmu_get_by_mode_func) __P((int, int));
	void (*cmmu_show_translation_func) __P((unsigned, unsigned, unsigned, int));
	void (*cmmu_cache_state_func) __P((unsigned, unsigned));
	void (*show_cmmu_info_func) __P((unsigned));
#endif /* end if DDB */
};

/* THE pointer! */
extern struct cmmu_p *cmmu;

extern struct cmmu_p cmmu88110;
extern struct cmmu_p cmmu8820x;

/* The macros... */
#define cmmu_init (cmmu->cmmu_init_func)
#define show_apr(ap) (cmmu->show_apr_func)(ap)
#define setup_board_config	(cmmu->setup_board_config_func)
#define	setup_cmmu_config 	(cmmu->setup_cmmu_config_func)
#define	cmmu_dump_config	(cmmu->cmmu_dump_config_func)
#define	cpu_configuration_print(a)	(cmmu->cpu_configuration_print_func)(a)
#define	cmmu_shutdown_now	(cmmu->cmmu_shutdown_now_func)
#define	cmmu_parity_enable	(cmmu->cmmu_parity_enable_func)
#define	cmmu_cpu_number		(cmmu->cmmu_cpu_number_func)
#define	cmmu_remote_set(a, b, c, d)	(cmmu->cmmu_remote_set_func)(a, b, c, d)
#define	cmmu_remote_get(a, b, c)	(cmmu->cmmu_remote_get_func)(a, b, c)
#define	cmmu_get_idr(a)		(cmmu->cmmu_get_idr_func)(a)
#define	cmmu_set_sapr(a)	(cmmu->cmmu_set_sapr_func)(a)
#define	cmmu_remote_set_sapr(a, b)	(cmmu->cmmu_remote_set_sapr_func)(a, b)
#define	cmmu_set_uapr(a)	(cmmu->cmmu_set_uapr_func)(a)
#define	cmmu_set_batc_entry(a, b, c, d) 	(cmmu->cmmu_set_batc_entry_func)(a, b, c, d)
#define	cmmu_set_pair_batc_entry(a, b, c)	(cmmu->cmmu_set_pair_batc_entry_func)(a, b, c)
#define	cmmu_flush_remote_tlb(a, b, c, d) 	(cmmu->cmmu_flush_remote_tlb_func)(a, b, c, d)
#define	cmmu_flush_tlb(a, b, c)	(cmmu->cmmu_flush_tlb_func)(a, b, c)
#define	cmmu_pmap_activate(a, b, c, d) 	(cmmu->cmmu_pmap_activate_func)(a, b, c, d) 
#define	cmmu_flush_remote_cache(a, b, c)	(cmmu->cmmu_flush_remote_cache_func)(a, b, c)
#define	cmmu_flush_cache(a, b)	(cmmu->cmmu_flush_cache_func)(a, b)
#define	cmmu_flush_remote_inst_cache(a, b, c)	(cmmu->cmmu_flush_remote_inst_cache_func)(a, b, c)
#define	cmmu_flush_inst_cache(a, b)	(cmmu->cmmu_flush_inst_cache_func)(a, b)
#define	cmmu_flush_remote_data_cache(a, b, c)	(cmmu->cmmu_flush_remote_data_cache_func)(a, b, c)
#define	cmmu_flush_data_cache(a, b)	(cmmu->cmmu_flush_data_cache_func)(a, b)
#define	dma_cachectl(a, b, c)	(cmmu->dma_cachectl_func)(a, b, c)
#ifdef DDB
#define	cmmu_get_by_mode(a, b)	(cmmu->cmmu_get_by_mode_func)(a, b)
#define	cmmu_show_translation(a, b, c, d)	(cmmu->cmmu_show_translation_func)(a, b, c, d)
#define	cmmu_cache_state(a, b)	(cmmu->cmmu_cache_state_func)(a, b)
#define	show_cmmu_info(a)	(cmmu->show_cmmu_info_func)(a)
#endif /* end if DDB */

#endif	/* _LOCORE */

#ifdef M88100
#include <machine/m8820x.h>
#endif /* M88100 */
#ifdef M88110
#include <machine/m88110.h>
#include <machine/m88410.h>
#endif /* M88110 */

#endif	/* _MACHINE_CMMU_H_ */

