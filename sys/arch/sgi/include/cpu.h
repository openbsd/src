/*	$OpenBSD: cpu.h,v 1.3 2009/09/30 06:22:00 syuu Exp $ */

/* Use Mips generic include file */

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
