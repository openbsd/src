/*	$NetBSD: gtscreg.h,v 1.2 1994/10/26 02:03:39 cgd Exp $	*/

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)gvp11_dmareg.h
 */
#ifndef _AMIGA_DEV_AHSCREG_H_
#define _AMIGA_DEV_AHSCREG_H_

/*
 * Hardware layout of the GVP Series II SDMAC. This also contains the
 * registers for the sbic chip, but in favor of separating DMA and
 * scsi, the scsi-driver doesn't make use of this dependency
 */

#define	v_int		volatile int
#define vu_char		volatile u_char
#define vu_short	volatile u_short
#define vu_int		volatile u_int

struct sdmac {
  u_short	pad0[32];
  vu_short CNTR;
  u_short	pad1[15];
  u_char	pad2;
  vu_char  SASR;
  u_char	pad3;
  vu_char  SCMD;
  u_short	pad4[2];
  vu_short bank;
  u_short	pad5[3];
  vu_int   ACR;
  vu_short	secret1;      /* Initially store 0  here */
  vu_short ST_DMA;       /* strobe */
  vu_short SP_DMA;       /* strobe */
  vu_short	secret2;      /* Initially store 1  here */
  vu_short	secret3;      /*         "       15  "   */
};

#define GVP_CNTR_BUSY           (1<<0)
#define GVP_CNTR_INT_P          (1<<1)
#define GVP_CNTR_INTEN          (1<<3)
#define GVP_CNTR_DDIR           (1<<4)

#endif /* _AMIGA_DEV_AHSCREG_H_ */
