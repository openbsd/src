/*	$OpenBSD: m88410.h,v 1.12 2005/12/12 20:36:32 miod Exp $ */
/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 *      This product includes software developed by Steve Murphree.
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
 *
 */

#ifndef	__M88410_H__
#define	__M88410_H__

#ifdef _KERNEL

/*
 *	MC88410 External Cache Controller definitions.
 *	This is only available on MVME197 SP, DP and QP models.
 */

#include <mvme88k/dev/busswreg.h>

void	mc88410_flush_page(paddr_t);
void	mc88410_flush(void);
void	mc88410_inval(void);

static __inline__ void
mc88410_sync(void)
{
	mc88410_flush();
	mc88410_inval();
}

static __inline__ int
mc88410_present(void)
{
	return (*(volatile u_int16_t *)(BS_BASE + BS_GCSR)) & BS_GCSR_B410;
}

#endif	/* _KERNEL */

#endif	/* __M88410_H__ */
