/*	$OpenBSD: cpufunc.h,v 1.31 2018/10/04 05:00:40 guenther Exp $	*/
/*	$NetBSD: cpufunc.h,v 1.3 2003/05/08 10:27:43 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

/*
 * Functions to provide access to i386-specific instructions.
 */

#include <sys/types.h>

#include <machine/specialreg.h>

#if defined(_KERNEL) && !defined (_STANDALONE)

extern int cpu_feature;

static __inline void 
invlpg(u_int64_t addr)
{ 
        __asm volatile("invlpg (%0)" : : "r" (addr) : "memory");
}  

static __inline void
lidt(void *p)
{
	__asm volatile("lidt (%0)" : : "r" (p) : "memory");
}

static __inline void
lldt(u_short sel)
{
	__asm volatile("lldt %0" : : "r" (sel));
}

static __inline void
ltr(u_short sel)
{
	__asm volatile("ltr %0" : : "r" (sel));
}

static __inline void
lcr8(u_int val)
{
	u_int64_t val64 = val;
	__asm volatile("movq %0,%%cr8" : : "r" (val64));
}

/*
 * Upper 32 bits are reserved anyway, so just keep this 32bits.
 */
static __inline void
lcr0(u_int val)
{
	u_int64_t val64 = val;
	__asm volatile("movq %0,%%cr0" : : "r" (val64));
}

static __inline u_int
rcr0(void)
{
	u_int64_t val64;
	u_int val;
	__asm volatile("movq %%cr0,%0" : "=r" (val64));
	val = val64;
	return val;
}

static __inline u_int64_t
rcr2(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr2,%0" : "=r" (val));
	return val;
}

static __inline void
lcr3(u_int64_t val)
{
	__asm volatile("movq %0,%%cr3" : : "r" (val));
}

static __inline u_int64_t
rcr3(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr3,%0" : "=r" (val));
	return val;
}

/*
 * Same as for cr0. Don't touch upper 32 bits.
 */
static __inline void
lcr4(u_int val)
{
	u_int64_t val64 = val;

	__asm volatile("movq %0,%%cr4" : : "r" (val64));
}

static __inline u_int
rcr4(void)
{
	u_int64_t val64;
	__asm volatile("movq %%cr4,%0" : "=r" (val64));
	return (u_int) val64;
}

static __inline void
tlbflush(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr3,%0" : "=r" (val));
	__asm volatile("movq %0,%%cr3" : : "r" (val));
}

static inline void
invpcid(uint64_t type, paddr_t pcid, paddr_t addr)
{
	uint64_t desc[2] = { pcid, addr };
	asm volatile("invpcid %0,%1" : : "m"(desc[0]), "r"(type));
}
#define INVPCID_ADDR		0
#define INVPCID_PCID		1
#define INVPCID_ALL		2
#define INVPCID_NON_GLOBAL	3

#ifdef notyet
void	setidt(int idx, /*XXX*/caddr_t func, int typ, int dpl);
#endif


/* XXXX ought to be in psl.h with spl() functions */

static __inline u_long
read_rflags(void)
{
	u_long	ef;

	__asm volatile("pushfq; popq %0" : "=r" (ef));
	return (ef);
}

static __inline void
write_rflags(u_long ef)
{
	__asm volatile("pushq %0; popfq" : : "r" (ef));
}

static __inline void
intr_enable(void)
{
	__asm volatile("sti");
}

static __inline u_long
intr_disable(void)
{
	u_long ef;

	ef = read_rflags();
	__asm volatile("cli");
	return (ef);
}

static __inline void
intr_restore(u_long ef)
{
	write_rflags(ef);
}

static __inline u_int64_t
rdmsr(u_int msr)
{
	uint32_t hi, lo;
	__asm volatile("rdmsr" : "=d" (hi), "=a" (lo) : "c" (msr));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline void
wrmsr(u_int msr, u_int64_t newval)
{
	__asm volatile("wrmsr" :
	    : "a" (newval & 0xffffffff), "d" (newval >> 32), "c" (msr));
}

/* 
 * Some of the undocumented AMD64 MSRs need a 'passcode' to access.
 *
 * See LinuxBIOSv2: src/cpu/amd/model_fxx/model_fxx_init.c
 */

#define	OPTERON_MSR_PASSCODE	0x9c5a203a
 
static __inline u_int64_t
rdmsr_locked(u_int msr, u_int code)
{
	uint32_t hi, lo;
	__asm volatile("rdmsr"
	    : "=d" (hi), "=a" (lo)
	    : "c" (msr), "D" (code));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline void
wrmsr_locked(u_int msr, u_int code, u_int64_t newval)
{
	__asm volatile("wrmsr" :
	    : "a" (newval & 0xffffffff), "d" (newval >> 32), "c" (msr), "D" (code));
}

static __inline void
wbinvd(void)
{
	__asm volatile("wbinvd");
}

static __inline void
clflush(u_int64_t addr)
{
	__asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
}

static __inline void
mfence(void)
{
	__asm volatile("mfence" : : : "memory");
}

static __inline u_int64_t
rdtsc(void)
{
	uint32_t hi, lo;

	__asm volatile("rdtsc" : "=d" (hi), "=a" (lo));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline u_int64_t
rdpmc(u_int pmc)
{
	uint32_t hi, lo;

	__asm volatile("rdpmc" : "=d" (hi), "=a" (lo) : "c" (pmc));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline void
monitor(const volatile void *addr, u_long extensions, u_int hints)
{

	__asm volatile("monitor"
	    : : "a" (addr), "c" (extensions), "d" (hints));
}

static __inline void
mwait(u_long extensions, u_int hints)
{

	__asm volatile(
		"	mwait			;"
		"	mov	$8,%%rcx	;"
		"	.align	16,0x90		;"
		"3:	call	5f		;"
		"4:	pause			;"
		"	lfence			;"
		"	call	4b		;"
		"	.align	16,0xcc		;"
		"5:	call	7f		;"
		"6:	pause			;"
		"	lfence			;"
		"	call	6b		;"
		"	.align	16,0xcc		;"
		"7:	loop	3b		;"
		"	add	$(16*8),%%rsp"
	    : "+c" (extensions) : "a" (hints));
}

static __inline void
xsetbv(uint32_t reg, uint64_t mask)
{
	uint32_t lo, hi;

	lo = mask;
	hi = mask >> 32;
	__asm volatile("xsetbv" :: "c" (reg), "a" (lo), "d" (hi) : "memory");
}

static __inline uint64_t
xgetbv(uint32_t reg)
{
	uint32_t lo, hi;

	__asm volatile("xgetbv" : "=a" (lo), "=d" (hi) : "c" (reg));

	return (((uint64_t)hi << 32) | (uint64_t)lo);
}

static __inline void
stgi(void)
{
	__asm volatile("stgi");
}

static __inline void
clgi(void)
{
	__asm volatile("clgi");
}

/* Break into DDB. */
static __inline void
breakpoint(void)
{
	__asm volatile("int $3");
}

void amd64_errata(struct cpu_info *);
void cpu_ucode_setup(void);
void cpu_ucode_apply(struct cpu_info *);

struct cpu_info_full;
void cpu_enter_pages(struct cpu_info_full *);

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
