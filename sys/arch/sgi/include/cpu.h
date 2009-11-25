/*	$OpenBSD: cpu.h,v 1.5 2009/11/25 17:39:51 syuu Exp $ */

#ifndef _SGI_CPU_H_
#define _SGI_CPU_H_

#ifdef _KERNEL
#ifdef MULTIPROCESSOR
#if defined(TGT_OCTANE)
#define HW_CPU_NUMBER_REG 0x900000000ff50000 /* HEART_PRID */
#else /* TGT_OCTANE */
#error MULTIPROCESSOR kernel not supported on this configuration
#endif /* TGT_OCTANE */
#define hw_cpu_number() (*(uint64_t *)HW_CPU_NUMBER_REG)
#else/* MULTIPROCESSOR */
#define hw_cpu_number() 0
#endif/* MULTIPROCESSOR */
#endif/* _KERNEL */

#include <mips64/cpu.h>

#if defined(_KERNEL) && defined(MULTIPROCESSOR) && !defined(_LOCORE)
void hw_cpu_boot_secondary(struct cpu_info *);
void hw_cpu_hatch(struct cpu_info *);
void hw_cpu_spinup_trampoline(struct cpu_info *);
int  hw_ipi_intr_establish(int (*)(void *), u_long);
void hw_ipi_intr_set(u_long);
void hw_ipi_intr_clear(u_long);
#endif/* _KERNEL && MULTIPROCESSOR && !_LOCORE */
#endif /* !_SGI_CPU_H_ */
