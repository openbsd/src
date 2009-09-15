/*	$OpenBSD: cpu.h,v 1.2 2009/09/15 04:54:31 syuu Exp $ */

/* Use Mips generic include file */

#ifdef _KERNEL
#ifdef MULTIPROCESSOR
#if defined(TGT_OCTANE)
#define HW_CPU_NUMBER 0x900000000ff50000/* HEART_PRID */
#else
#error MULTIPROCESSOR kernel not supported on this configuration
#endif
#define hw_cpu_number() (*(uint64_t *)HW_CPU_NUMBER)
#else/* MULTIPROCESSOR */
#define hw_cpu_number() 0
#endif/* MULTIPROCESSOR */
#endif/* _KERNEL */

#include <mips64/cpu.h>
