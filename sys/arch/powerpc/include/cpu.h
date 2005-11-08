/*	$OpenBSD: cpu.h,v 1.24 2005/11/08 20:30:47 kettenis Exp $	*/
/*	$NetBSD: cpu.h,v 1.1 1996/09/30 16:34:21 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_POWERPC_CPU_H_
#define	_POWERPC_CPU_H_

#include <machine/frame.h>

#include <machine/psl.h>

#define	CLKF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	CLKF_PC(frame)		((frame)->srr0)
#define	CLKF_INTR(frame)	((frame)->depth != 0)

#define	cpu_swapout(p)
#define	cpu_wait(p)

void	delay(unsigned);
#define	DELAY(n)		delay(n)

extern volatile int want_resched;
extern volatile int astpending;

#define	need_resched(ci)	(want_resched = 1, astpending = 1)
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, astpending = 1)
#define	signotify(p)		(astpending = 1)

extern char *bootpath;

#ifndef	CACHELINESIZE
#define	CACHELINESIZE	32			/* For now		XXX */
#endif

static __inline void
syncicache(void *from, int len)
{
	int l;
	char *p = from;

	len = len + (((u_int32_t) from) & (CACHELINESIZE - 1));
	l = len;

	do {
		__asm __volatile ("dcbst 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	__asm __volatile ("sync");
	p = from;
	l = len;
	do {
		__asm __volatile ("icbi 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	__asm __volatile ("isync");
}

static __inline void
invdcache(void *from, int len)
{
	int l;
	char *p = from;

	len = len + (((u_int32_t) from) & (CACHELINESIZE - 1));
	l = len;

	do {
		__asm __volatile ("dcbi 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	__asm __volatile ("sync");
}

#define FUNC_SPR(n, name) \
static __inline u_int32_t ppc_mf ## name (void)			\
{								\
	u_int32_t ret;						\
	__asm __volatile ("mfspr %0," # n : "=r" (ret));	\
	return ret;						\
}								\
static __inline void ppc_mt ## name (u_int32_t val)		\
{								\
	__asm __volatile ("mtspr "# n ",%0" :: "r" (val));	\
}								\

FUNC_SPR(0, mq)
FUNC_SPR(1, xer)
FUNC_SPR(4, rtcu)
FUNC_SPR(5, rtcl)
FUNC_SPR(8, lr)
FUNC_SPR(9, ctr)
FUNC_SPR(18, dsisr)
FUNC_SPR(19, dar)
FUNC_SPR(22, dec)
FUNC_SPR(25, sdr1)
FUNC_SPR(26, srr0)
FUNC_SPR(27, srr1)
FUNC_SPR(256, vrsave)
FUNC_SPR(272, sprg0)
FUNC_SPR(273, sprg1)
FUNC_SPR(274, sprg2)
FUNC_SPR(275, sprg3)
FUNC_SPR(280, asr)
FUNC_SPR(282, ear)
FUNC_SPR(287, pvr)
FUNC_SPR(528, ibat0u)
FUNC_SPR(529, ibat0l)
FUNC_SPR(530, ibat1u)
FUNC_SPR(531, ibat1l)
FUNC_SPR(532, ibat2u)
FUNC_SPR(533, ibat2l)
FUNC_SPR(534, ibat3u)
FUNC_SPR(535, ibat3l)
FUNC_SPR(536, dbat0u)
FUNC_SPR(537, dbat0l)
FUNC_SPR(538, dbat1u)
FUNC_SPR(539, dbat1l)
FUNC_SPR(540, dbat2u)
FUNC_SPR(541, dbat2l)
FUNC_SPR(542, dbat3u)
FUNC_SPR(543, dbat3l)
FUNC_SPR(1008, hid0)
FUNC_SPR(1009, hid1)
FUNC_SPR(1010, iabr)
FUNC_SPR(1017, l2cr)
FUNC_SPR(1018, l3cr)
FUNC_SPR(1013, dabr)
FUNC_SPR(1023, pir)

static __inline u_int32_t
ppc_mftbl (void)
{
	int ret;
	__asm __volatile ("mftb %0" : "=r" (ret));
	return ret;
}

static __inline u_int64_t
ppc_mftb(void)
{
	u_long scratch;
	u_int64_t tb;

	__asm __volatile ("1: mftbu %0; mftb %0+1; mftbu %1;"
	    " cmpw 0,%0,%1; bne 1b" : "=r"(tb), "=r"(scratch));
	return tb;
}

static __inline u_int32_t
ppc_mfmsr (void)
{
	int ret;
        __asm __volatile ("mfmsr %0" : "=r" (ret));
	return ret;
}

static __inline void
ppc_mtmsr (u_int32_t val)
{
        __asm __volatile ("mtmsr %0" :: "r" (val));
}

static __inline void
ppc_mtsrin(u_int32_t val, u_int32_t sn_shifted)
{
	__asm __volatile ("mtsrin %0,%1" :: "r"(val), "r"(sn_shifted));
}

u_int64_t ppc64_mfscomc(void);
void ppc64_mtscomc(u_int64_t);
u_int64_t ppc64_mfscomd(void);

/*
 * General functions to enable and disable interrupts
 * without having inlined assembly code in many functions.
 */
static __inline void
ppc_intr_enable(int enable)
{
	u_int32_t msr;
	if (enable != 0) {
		msr = ppc_mfmsr();
		msr |= PSL_EE;
		ppc_mtmsr(msr);
	}
}

static __inline int
ppc_intr_disable(void)
{
	u_int32_t emsr, dmsr;
	emsr = ppc_mfmsr();
	dmsr = emsr & ~PSL_EE;
	ppc_mtmsr(dmsr);
	return (emsr & PSL_EE);
}

int ppc_cpuspeed(int *);
void ppc_check_procid(void);
extern int ppc_proc_is_64b;

/*
 * PowerPC CPU types
 */
#define	PPC_CPU_MPC601		1
#define	PPC_CPU_MPC603		3
#define	PPC_CPU_MPC604		4
#define	PPC_CPU_MPC603e		6
#define	PPC_CPU_MPC603ev	7
#define	PPC_CPU_MPC750		8
#define	PPC_CPU_MPC604ev	9
#define	PPC_CPU_MPC7400		12
#define	PPC_CPU_IBM970FX	0x003c
#define	PPC_CPU_IBM750FX	0x7000
#define	PPC_CPU_MPC7410		0x800c
#define	PPC_CPU_MPC7447A	0x8003
#define	PPC_CPU_MPC7450		0x8000
#define	PPC_CPU_MPC7455		0x8001

#endif	/* _POWERPC_CPU_H_ */
