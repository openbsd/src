/*	$OpenBSD: cmmu.h,v 1.13 2005/12/03 19:06:08 miod Exp $ */
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

#ifndef	_M88K_CMMU_H_
#define	_M88K_CMMU_H_

/*
 * Prototypes and stuff for cmmu.c.
 */
#if defined(_KERNEL) && !defined(_LOCORE)

#ifdef MULTIPROCESSOR
/*
 * This lock protects the cmmu SAR and SCR's; other ports
 * can be accessed without locking it.
 *
 * May be used from "db_interface.c".
 */
extern __cpu_simple_lock_t cmmu_cpu_lock;

#define CMMU_LOCK   __cpu_simple_lock(&cmmu_cpu_lock)
#define CMMU_UNLOCK __cpu_simple_unlock(&cmmu_cpu_lock)

#else

#define	CMMU_LOCK	do { /* nothing */ } while (0)
#define	CMMU_UNLOCK	do { /* nothing */ } while (0)

#endif	/* MULTIPROCESSOR */

/* machine dependent cmmu function pointer structure */
struct cmmu_p {
	void (*cmmu_init_func)(void);
	void (*setup_board_config_func)(void);
	void (*cpu_configuration_print_func)(int);
	void (*cmmu_shutdown_now_func)(void);
	void (*cmmu_parity_enable_func)(void);
	cpuid_t (*cmmu_cpu_number_func)(void);
	void (*cmmu_set_sapr_func)(cpuid_t, apr_t);
	void (*cmmu_set_uapr_func)(apr_t);
	void (*cmmu_flush_tlb_func)(cpuid_t, unsigned, vaddr_t, u_int);
	void (*cmmu_flush_cache_func)(cpuid_t, paddr_t, psize_t);
	void (*cmmu_flush_inst_cache_func)(cpuid_t, paddr_t, psize_t);
	void (*cmmu_flush_data_cache_func)(cpuid_t, paddr_t, psize_t);
	int (*dma_cachectl_func)(pmap_t, vaddr_t, vsize_t, int);
	int (*dma_cachectl_pa_func)(paddr_t, psize_t, int);
	/* DDB only */
	void (*cmmu_dump_config_func)(void);
	void (*cmmu_show_translation_func)(unsigned, unsigned, unsigned, int);
	/* DEBUG only */
	void (*show_apr_func)(apr_t);
};

/* THE pointer! */
extern struct cmmu_p *cmmu;

/* The macros... */
#define cmmu_init		(cmmu->cmmu_init_func)
#define setup_board_config	(cmmu->setup_board_config_func)
#define	cpu_configuration_print(a)	(cmmu->cpu_configuration_print_func)(a)
#define	cmmu_shutdown_now	(cmmu->cmmu_shutdown_now_func)
#define	cmmu_parity_enable	(cmmu->cmmu_parity_enable_func)
#define	cmmu_cpu_number		(cmmu->cmmu_cpu_number_func)
#define	cmmu_set_sapr(a, b)	(cmmu->cmmu_set_sapr_func)(a, b)
#define	cmmu_set_uapr(a)	(cmmu->cmmu_set_uapr_func)(a)
#define	cmmu_flush_tlb(a, b, c, d) 	(cmmu->cmmu_flush_tlb_func)(a, b, c, d)
#define	cmmu_flush_cache(a, b, c)	(cmmu->cmmu_flush_cache_func)(a, b, c)
#define	cmmu_flush_inst_cache(a, b, c)	(cmmu->cmmu_flush_inst_cache_func)(a, b, c)
#define	cmmu_flush_data_cache(a, b, c)	(cmmu->cmmu_flush_data_cache_func)(a, b, c)
#define	dma_cachectl(a, b, c, d)	(cmmu->dma_cachectl_func)(a, b, c, d)
#define	dma_cachectl_pa(a, b, c)	(cmmu->dma_cachectl_pa_func)(a, b, c)
#define	cmmu_dump_config	(cmmu->cmmu_dump_config_func)
#define	cmmu_show_translation(a, b, c, d)	(cmmu->cmmu_show_translation_func)(a, b, c, d)
#define show_apr(ap)		(cmmu->show_apr_func)(ap)

/*
 * dma_cachectl modes
 */
#define DMA_CACHE_SYNC		0
#define DMA_CACHE_SYNC_INVAL	1
#define DMA_CACHE_INV		2

#endif	/* _KERNEL && !_LOCORE */

#endif	/* _M88K_CMMU_H_ */
