/*	$OpenBSD: reg.h,v 1.2 2005/05/16 11:47:14 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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

#ifndef _M88K_REG_H_
#define _M88K_REG_H_

struct reg {
	unsigned int	r[32];
	unsigned int	epsr;
	unsigned int	fpsr;
	unsigned int	fpcr;
	unsigned int	sxip;
#define exip sxip	/* mc88110 */
	unsigned int	snip;
#define enip snip	/* mc88110 */
	unsigned int	sfip;
	unsigned int	ssbr;
#define duap ssbr	/* mc88110 */
	unsigned int	dmt0;
#define dsr dmt0	/* mc88110 */
	unsigned int	dmd0;
#define dlar dmd0	/* mc88110 */
	unsigned int	dma0;
#define dpar dma0	/* mc88110 */
	unsigned int	dmt1;
#define isr dmt1	/* mc88110 */
	unsigned int	dmd1;
#define ilar dmd1	/* mc88110 */
	unsigned int	dma1;
#define ipar dma1	/* mc88110 */
	unsigned int	dmt2;
#define isap dmt2	/* mc88110 */
	unsigned int	dmd2;
#define dsap dmd2	/* mc88110 */
	unsigned int	dma2;
#define iuap dma2	/* mc88110 */
	unsigned int	fpecr;
	unsigned int	fphs1;
	unsigned int	fpls1;
	unsigned int	fphs2;
	unsigned int	fpls2;
	unsigned int	fppt;
	unsigned int	fprh;
	unsigned int	fprl;
	unsigned int	fpit;
};

#endif /* _M88K_REG_H_ */
