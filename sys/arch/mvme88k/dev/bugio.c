/*	$OpenBSD: bugio.c,v 1.19 2010/12/23 20:05:08 miod Exp $ */
/*
 * Copyright (c) 2006, 2010, Miodrag Vallat.
 * Copyright (c) 1998 Steve Murphree, Jr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/asm_macro.h>
#include <machine/bugio.h>
#include <machine/prom.h>

register_t ossr3;
register_t bugsr3;

#ifdef MULTIPROCESSOR
#include <sys/lock.h>
__cpu_simple_lock_t bug_lock = __SIMPLELOCK_UNLOCKED;
#define	BUG_LOCK()	__cpu_simple_lock(&bug_lock)
#define	BUG_UNLOCK()	__cpu_simple_unlock(&bug_lock)
#else
#define	BUG_LOCK()	do { } while (0)
#define	BUG_UNLOCK()	do { } while (0)
#endif

#define MVMEPROM_CALL(x)						\
	__asm__ __volatile__ ("or r9,r0," __STRING(x) "; tb0 0,r0,496"	\
	    :::	"r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",		\
	        "r9", "r10", "r11", "r12", "r13", "memory")

#define	BUGCTXT()							\
{									\
	BUG_LOCK();							\
	psr = get_psr();						\
	set_psr(psr | PSR_IND);			/* paranoia */		\
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (ossr3));		\
	__asm__ __volatile__ ("stcr %0, cr20" :: "r"(bugsr3));		\
}

#define	OSCTXT()							\
{									\
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (bugsr3));		\
	__asm__ __volatile__ ("stcr %0, cr20" :: "r"(ossr3));		\
	set_psr(psr);							\
	BUG_UNLOCK();							\
}

void
bugpcrlf(void)
{
	u_int psr;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_OUTCRLF);
	OSCTXT();
}

void
buginit()
{
	__asm__ __volatile__ ("ldcr %0, cr20" : "=r" (bugsr3));
}

char
buginchr(void)
{
	u_int psr;
	int ret;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_INCHR);
	__asm__ __volatile__ ("or %0,r0,r2" : "=r" (ret));
	OSCTXT();
	return ((char)ret & 0xff);
}

void
bugoutchr(int c)
{
	u_int psr;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	__asm__ __volatile__ ("or r2,r0,%0" : : "r" (c));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
	OSCTXT();
}

void
bugreturn(void)
{
	u_int psr;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_EXIT);
	OSCTXT();
}

void
bugbrdid(struct mvmeprom_brdid *id)
{
	u_int psr;
	struct mvmeprom_brdid *ptr;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	__asm__ __volatile__ ("or %0,r0,r2" : "=r" (ptr));
	OSCTXT();

	bcopy(ptr, id, sizeof(struct mvmeprom_brdid));
}

void
bugdiskrd(struct mvmeprom_dskio *dio)
{
	u_int psr;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	__asm__ __volatile__ ("or r2, r0, %0" : : "r" (dio));
	MVMEPROM_CALL(MVMEPROM_DSKRD);
	OSCTXT();
}

#ifdef MULTIPROCESSOR

/*
 * Ask the BUG to start a particular cpu at our provided address.
 */
int
spin_cpu(cpuid_t cpu, vaddr_t address)
{
	u_int psr;
	int ret;

#ifdef DIAGNOSTIC
	if (!cold)
		panic("%s: BUG calls are forbidden at this point", __func__);
#endif

	BUGCTXT();
	__asm__ __volatile__ ("or r2, r0, %0; or r3, r0, %1" ::
	    "r" (cpu), "r" (address));
	MVMEPROM_CALL(MVMEPROM_FORKMPU);
	__asm__ __volatile__ ("or %0,r0,r2" : "=r" (ret));
	OSCTXT();

	return (ret);
}

#endif	/* MULTIPROCESSOR */
