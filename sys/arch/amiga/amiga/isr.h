/*	$OpenBSD: isr.h,v 1.3 1996/05/02 06:43:18 niklas Exp $	*/
/*	$NetBSD: isr.h,v 1.8 1996/04/21 21:07:02 veego Exp $	*/

/*
 * Copyright (c) 1982 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)isr.h	7.1 (Berkeley) 5/8/90
 */

struct isr {
	struct	isr *isr_forw;
	struct	isr *isr_back;
	int	(*isr_intr) __P((void *));
	void	*isr_arg;
	int	isr_ipl;
#if defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
	int	isr_mapped_ipl;
#ifdef IPL_REMAP_2
	void	(*isr_ackintr)();
	int	isr_status;
#endif
#endif
};

#define	NISR		3
#define	ISRIPL(x)	((x) - 3)

#ifdef _KERNEL
void add_isr __P((struct isr *));
void remove_isr __P((struct isr *));
typedef void (*sifunc_t) __P((void *, void *));
void alloc_sicallback __P((void));
void add_sicallback __P((sifunc_t, void *, void *));
void rem_sicallback __P((sifunc_t));
#endif

#ifdef IPL_REMAP2
#define ISR_IDLE	0
#define ISR_WAITING	1
#define ISR_BUSY	2
#endif
