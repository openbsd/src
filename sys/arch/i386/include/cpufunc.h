/*	$OpenBSD: cpufunc.h,v 1.5 1999/11/20 11:18:59 matthieu Exp $	*/
/*	$NetBSD: cpufunc.h,v 1.8 1994/10/27 04:15:59 cgd Exp $	*/

/*
 * Copyright (c) 1993 Charles Hannum.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_CPUFUNC_H_
#define	_I386_CPUFUNC_H_

/*
 * Functions to provide access to i386-specific instructions.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

static __inline void invlpg __P((u_int));
static __inline void lidt __P((void *));
static __inline void lldt __P((u_short));
static __inline void ltr __P((u_short));
static __inline void lcr0 __P((u_int));
static __inline u_int rcr0 __P((void));
static __inline u_int rcr2 __P((void));
static __inline void lcr3 __P((u_int));
static __inline u_int rcr3 __P((void));
static __inline void lcr4 __P((u_int));
static __inline u_int rcr4 __P((void));
static __inline void tlbflush __P((void));
static __inline void disable_intr __P((void));
static __inline void enable_intr __P((void));

static __inline void 
#ifdef __cplusplus
invlpg(u_int addr)
#else
invlpg(addr)
	u_int addr;
#endif
{ 
        __asm __volatile("invlpg (%0)" : : "r" (addr) : "memory");
}  

static __inline void
#ifdef __cplusplus
lidt(void *p)
#else
lidt(p)
	void *p;
#endif
{
	__asm __volatile("lidt (%0)" : : "r" (p));
}

static __inline void
#ifdef __cplusplus
lldt(u_short sel)
#else
lldt(sel)
	u_short sel;
#endif
{
	__asm __volatile("lldt %0" : : "r" (sel));
}

static __inline void
#ifdef __cplusplus
ltr(u_short sel)
#else
ltr(sel)
	u_short sel;
#endif
{
	__asm __volatile("ltr %0" : : "r" (sel));
}

static __inline void
#ifdef __cplusplus
lcr0(u_int val)
#else
lcr0(val)
	u_int val;
#endif
{
	__asm __volatile("movl %0,%%cr0" : : "r" (val));
}

static __inline u_int
#ifdef __cplusplus
rcr0(void)
#else
rcr0()
#endif
{
	u_int val;
	__asm __volatile("movl %%cr0,%0" : "=r" (val));
	return val;
}

static __inline u_int
#ifdef __cplusplus
rcr2(void)
#else
rcr2()
#endif
{
	u_int val;
	__asm __volatile("movl %%cr2,%0" : "=r" (val));
	return val;
}

static __inline void
#ifdef __cplusplus
lcr3(u_int val)
#else
lcr3(val)
	u_int val;
#endif
{
	__asm __volatile("movl %0,%%cr3" : : "r" (val));
}

static __inline u_int
#ifdef __cplusplus
rcr3(void)
#else
rcr3()
#endif
{
	u_int val;
	__asm __volatile("movl %%cr3,%0" : "=r" (val));
	return val;
}

static __inline void
#ifdef __cplusplus
lcr4(u_int val)
#else
lcr4(val)
	u_int val;
#endif
{
	__asm __volatile("movl %0,%%cr4" : : "r" (val));
}

static __inline u_int
#ifdef __cplusplus
rcr4(void)
#else
rcr4()
#endif
{
	u_int val;
	__asm __volatile("movl %%cr4,%0" : "=r" (val));
	return val;
}

static __inline void
#ifdef __cplusplus
tlbflush(void)
#else
tlbflush()
#endif
{
	u_int val;
	__asm __volatile("movl %%cr3,%0" : "=r" (val));
	__asm __volatile("movl %0,%%cr3" : : "r" (val));
}

#ifdef notyet
void	setidt	__P((int idx, /*XXX*/caddr_t func, int typ, int dpl));
#endif


/* XXXX ought to be in psl.h with spl() functions */

static __inline void
#ifdef __cplusplus
disable_intr(void)
#else
disable_intr()
#endif
{
	__asm __volatile("cli");
}

static __inline void
#ifdef __cplusplus
enable_intr(void)
#else
enable_intr()
#endif
{
	__asm __volatile("sti");
}

static __inline void
wbinvd(void)
{
        __asm __volatile("wbinvd");
}


static __inline void
wrmsr(u_int msr, u_int64_t newval)
{
        __asm __volatile(".byte 0x0f, 0x30" : : "A" (newval), "c" (msr));
}

static __inline u_int64_t
rdmsr(u_int msr)
{
        u_int64_t rv;

        __asm __volatile(".byte 0x0f, 0x32" : "=A" (rv) : "c" (msr));
        return (rv);
}

#endif /* !_I386_CPUFUNC_H_ */
