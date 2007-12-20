#ifndef __M88K_PROFILE_H__
#define __M88K_PROFILE_H__
/*	$OpenBSD: profile.h,v 1.4 2007/12/20 21:19:34 miod Exp $ */
/*
 * Copyright (c) 2004, Miodrag Vallat.
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

#define	_MCOUNT_DECL static inline void _mcount

/*
 * On OpenBSD, calls to the function profiler save r2-r9 on stack. The
 * monitor point is found in r1. The function's return address is taken
 * from the stack frame pointed to by r30.
 */
#define	MCOUNT \
extern void mcount(void) __asm__ ("mcount"); \
void \
mcount() \
{ \
	int	returnaddress, monpoint; \
	__asm__ __volatile__ ("or %0, r1, r0" : "=r"(returnaddress)); \
	__asm__ __volatile__ ("ld %0, r30, 4" : "=r"(monpoint)); \
	_mcount(monpoint, returnaddress); \
}

#ifdef _KERNEL
#define	MCOUNT_ENTER	s = get_psr(); set_psr(s | PSR_IND);
#define	MCOUNT_EXIT	set_psr(s)
#endif /* _KERNEL */

#endif /* __M88K_PROFILE_H__ */
