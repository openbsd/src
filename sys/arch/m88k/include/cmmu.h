/*	$OpenBSD: cmmu.h,v 1.14 2005/12/04 12:20:19 miod Exp $ */
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
	void (*init)(void);
	void (*setup_board_config)(void);
	void (*cpu_configuration_print)(int);
	void (*shutdown_now)(void);
	cpuid_t (*cpu_number)(void);
	void (*set_sapr)(cpuid_t, apr_t);
	void (*set_uapr)(apr_t);
	void (*flush_tlb)(cpuid_t, u_int, vaddr_t, u_int);
	void (*flush_cache)(cpuid_t, paddr_t, psize_t);
	void (*flush_inst_cache)(cpuid_t, paddr_t, psize_t);
	void (*flush_data_cache)(cpuid_t, paddr_t, psize_t);
	int (*dma_cachectl)(pmap_t, vaddr_t, vsize_t, int);
	int (*dma_cachectl_pa)(paddr_t, psize_t, int);
};

/* THE pointer! */
extern struct cmmu_p *cmmu;

/* The macros... */
#define cmmu_init			(cmmu->init)
#define setup_board_config		(cmmu->setup_board_config)
#define	cpu_configuration_print(a)	(cmmu->cpu_configuration_print)(a)
#define	cmmu_shutdown_now		(cmmu->shutdown_now)
#define	cmmu_cpu_number			(cmmu->cpu_number)
#define	cmmu_set_sapr(a, b)		(cmmu->set_sapr)(a, b)
#define	cmmu_set_uapr(a)		(cmmu->set_uapr)(a)
#define	cmmu_flush_tlb(a, b, c, d) 	(cmmu->flush_tlb)(a, b, c, d)
#define	cmmu_flush_cache(a, b, c)	(cmmu->flush_cache)(a, b, c)
#define	cmmu_flush_inst_cache(a, b, c)	(cmmu->flush_inst_cache)(a, b, c)
#define	cmmu_flush_data_cache(a, b, c)	(cmmu->flush_data_cache)(a, b, c)
#define	dma_cachectl(a, b, c, d)	(cmmu->dma_cachectl)(a, b, c, d)
#define	dma_cachectl_pa(a, b, c)	(cmmu->dma_cachectl_pa)(a, b, c)
#define	cmmu_dump_config		(cmmu->dump_config)
#define	cmmu_show_translation(a, b, c, d) \
	(cmmu->show_translation)(a, b, c, d)
#define show_apr(ap)			(cmmu->show_apr)(ap)

/*
 * dma_cachectl() modes
 */
#define DMA_CACHE_SYNC		0
#define DMA_CACHE_SYNC_INVAL	1
#define DMA_CACHE_INV		2

#endif	/* _KERNEL && !_LOCORE */

#endif	/* _M88K_CMMU_H_ */
