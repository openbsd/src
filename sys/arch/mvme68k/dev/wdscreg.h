/*	$OpenBSD: wdscreg.h,v 1.2 2003/06/02 23:27:50 millert Exp $ */

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *  @(#)dmareg.h
 */
#ifndef _MVME68K_DEV_WDSCREG_H_
#define _MVME68K_DEV_WDSCREG_H_

#define DMAC_CSR_ENABLE     (1 << 0)    /* Enable the DMAC */
#define DMAC_CSR_TABLE      (1 << 1)    /* Select Table Mode */
#define DMAC_CSR_WRITE      (1 << 2)    /* Write data from RAM to SCSI */
#define DMAC_CSR_TBUSERR    (1 << 3)    /* Bus error during table walk */
#define DMAC_CSR_DBUSERR    (1 << 4)    /* Bus error during data xfer */
#define DMAC_CSR_TSIZE      (1 << 5)    /* Table addr. not in 32 bits */
#define DMAC_CSR_8BITS      (1 << 6)    /* Non-8 bit handshake */
#define DMAC_CSR_DONE       (1 << 7)    /* Transfer complete, or error */

#define DMAC_SR_HOLDING     0x0f        /* Data holding state */
#define DMAC_SR_INCREMENT   0xf0        /* Increment value */

#endif
