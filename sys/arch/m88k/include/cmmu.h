/*	$OpenBSD: cmmu.h,v 1.22 2009/02/16 23:03:31 miod Exp $ */
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

/* machine dependent cmmu function pointer structure */
struct cmmu_p {
	cpuid_t (*init)(void);
	void (*setup_board_config)(void);
	void (*cpu_configuration_print)(int);
	void (*shutdown)(void);
	cpuid_t (*cpu_number)(void);
	void (*set_sapr)(apr_t);
	void (*set_uapr)(apr_t);
	void (*flush_tlb)(cpuid_t, u_int, vaddr_t, u_int);
	void (*flush_cache)(cpuid_t, paddr_t, psize_t);
	void (*flush_inst_cache)(cpuid_t, paddr_t, psize_t);
	void (*dma_cachectl)(paddr_t, psize_t, int);
#ifdef MULTIPROCESSOR
	void (*dma_cachectl_local)(paddr_t, psize_t, int);
	void (*initialize_cpu)(cpuid_t);
#endif
};

extern struct cmmu_p *cmmu;

#ifdef MULTIPROCESSOR
/*
 * On 8820x-based systems, this lock protects the CMMU SAR and SCR registers;
 * other registers may be accessed without locking it.
 * On 88410-based systems, this lock protects accesses to the BusSwitch GCSR
 * register, which masks or unmasks the 88410 control addresses.
 */
extern __cpu_simple_lock_t cmmu_cpu_lock;
#define CMMU_LOCK   __cpu_simple_lock(&cmmu_cpu_lock)
#define CMMU_UNLOCK __cpu_simple_unlock(&cmmu_cpu_lock)
#else
#define	CMMU_LOCK	do { /* nothing */ } while (0)
#define	CMMU_UNLOCK	do { /* nothing */ } while (0)
#endif	/* MULTIPROCESSOR */

#define cmmu_init			(cmmu->init)
#define setup_board_config		(cmmu->setup_board_config)
#define	cpu_configuration_print(a)	(cmmu->cpu_configuration_print)(a)
#define	cmmu_shutdown			(cmmu->shutdown)
#define	cmmu_cpu_number			(cmmu->cpu_number)
#define	cmmu_set_sapr(a)		(cmmu->set_sapr)(a)
#define	cmmu_set_uapr(a)		(cmmu->set_uapr)(a)
#define	cmmu_flush_tlb(a, b, c, d) 	(cmmu->flush_tlb)(a, b, c, d)
#define	cmmu_flush_cache(a, b, c)	(cmmu->flush_cache)(a, b, c)
#define	cmmu_flush_inst_cache(a, b, c)	(cmmu->flush_inst_cache)(a, b, c)
#define	dma_cachectl(a, b, c)		(cmmu->dma_cachectl)(a, b, c)
#define	dma_cachectl_local(a, b, c)	(cmmu->dma_cachectl_local)(a, b, c)
#define	cmmu_initialize_cpu(a)		(cmmu->initialize_cpu)(a)

/*
 * dma_cachectl{,_local}() modes
 */
#define DMA_CACHE_SYNC		0x00
#define DMA_CACHE_SYNC_INVAL	0x01
#define DMA_CACHE_INV		0x02

#define	DMA_CACHE_MASK		0x03

#endif	/* _KERNEL && !_LOCORE */

#endif	/* _M88K_CMMU_H_ */
