/*	$OpenBSD: profile.h,v 1.1 2004/04/29 06:25:34 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)profile.h	8.1 (Berkeley) 6/11/93
 */
#ifndef __MACHINE_PROFILE_H__
#define __MACHINE_PROFILE_H__
#define	_MCOUNT_DECL static inline void _mcount

#define	MCOUNT \
extern void mcount() __asm__ ("mcount");					\
void									\
mcount()								\
{									\
	int selfret;							\
	int callerret;							\
	/*								\
	 * find the return address for mcount,				\
	 * and the return address for mcount's caller.			\
	 *								\
	 * selfret = ret pushed by mcount call				\
	 */								\
	__asm__ __volatile__ ("or %0,r1,0" : "=r" (selfret));		\
	/*								\
	 * callerret = ret pushed by call into self.			\
	 */								\
	/*								\
	 * This may not be right. It all depends on where the		\
	 * caller stores the return address. XXX			\
	 */								\
	__asm__ __volatile__("addu	 r10,r31,48");			\
	__asm__ __volatile__("ld %0,r10,36" : "=r" (callerret));	\
	_mcount(callerret, selfret);					\
}

#ifdef _KERNEL
/*
 * Note that we assume splhigh() and splx() cannot call mcount()
 * recursively.
 */
#define	MCOUNT_ENTER	s = splhigh()
#define	MCOUNT_EXIT	splx(s)
#endif /* _KERNEL */
#endif /* __MACHINE_PROFILE_H__ */
