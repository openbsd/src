/*	$OpenBSD: hp98265reg.h,v 1.1 2004/08/03 21:46:56 miod Exp $	*/
/*	$NetBSD: hp98265reg.h,v 1.1 2003/08/01 01:18:45 tsutsui Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	@(#)scsireg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * HP 98265A SCSI Interface Hardware Description.
 */

#define SPC_OFFSET	32
#define SPC_SIZE	(32 * 2)	/* XXX */

#define HPSCSI_ID	0x00
#define  ID_MASK	0x1f
#define  SCSI_ID	0x07
#define  ID_WORD_DMA	0x20

#define HPSCSI_CSR	0x01
#define  CSR_IE		0x80
#define  CSR_IR		0x40
#define  SCSI_IPL(csr)	((((csr) >> 4) & 3) + 3)
#define  CSR_DMA32	0x08
#define  CSR_DMAIN	0x04
#define  CSR_DE1	0x02
#define  CSR_DE0	0x01

#define HPSCSI_WRAP	0x02
#define  WRAP_REQ	0x80
#define  WRAP_ACK	0x40
#define  WRAP_BSY	0x08
#define  WRAP_MSG	0x04
#define  WRAP_CD	0x02
#define  WRAP_IO	0x01

#define HPSCSI_HCONF	0x03
#define  HCONF_TP	0x80
#define  SCSI_SYNC_XFER(hconf) (((hconf) >> 5) & 3)
#define  HCONF_SD	0x10
#define  HCONF_PARITY	0x08
