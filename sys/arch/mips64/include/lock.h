/*	$OpenBSD: lock.h,v 1.5 2013/05/21 20:05:30 tedu Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

#include <mips64/atomic.h>

#define rw_cas __cpu_cas
static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	int success, scratch0, scratch1;

        __asm volatile(
		".set noreorder\n"
		"1:\n"
		"lld	%0, (%5)\n"
		"bne	%0, %3, 2f\n"
		"move	%1, %4\n"
		"scd	%1, (%5)\n"
		"beqz	%1, 1b\n"
		"move   %2, $0\n"
		"j	3f\n"
		"nop\n"
		"2:\n"
		"daddi   %2, $0, 1\n"
		"3:\n"
		".set reorder\n"
		: "=&r"(scratch0), "=&r"(scratch1), "=&r"(success)
		: "r"(old), "r"(new), "r"(addr)
		: "memory");

	return success;
}

#endif	/* _MIPS64_LOCK_H_ */
