/*	$OpenBSD: ubsecreg.h,v 1.1 2000/05/18 01:25:19 jason Exp $	*/

/*
 * Copyright (c) 2000 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define UB_MCR			0x00		/* MCR */

#define UB_DMACTRL		0x04		/* DMA control */
#define UB_DMAC_MCR2IEN		0x40000000	/* Enable MCR2 completion interrupt */
#define UB_DMAC_MCRIEN		0x20000000	/* Enable MCR completion interrupt */
#define UB_DMAC_FRAGMODE	0x10000000
#define UB_DMAC_LE32		0x08000000
#define UB_DMAC_LE64		0x04000000
#define UB_DMAC_DMAERR		0x02000000

#define UB_DMASTAT		0x08
#define UB_DMAS_MCRBUSY		0x80000000
#define UB_DMAS_MCRFULL		0x40000000
#define UB_DMAS_MCRDONE		0x20000000
#define UB_DMAS_DMAERR		0x10000000
#define UB_DMAS_MCR2FULL	0x08000000
#define UB_DMAS_MCR2DONE	0x04000000

#define UB_DMAADDR		0x0c
#define UB_DMAA_ADDMASK		0xfffffffc
#define UB_DMAA_READ		0x00000002

#define UB_MCR2			0x10		/* MCR2 */

