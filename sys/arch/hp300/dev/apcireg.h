/*	$OpenBSD: apcireg.h,v 1.1 1997/04/16 11:55:57 downsj Exp $	*/
/*	$NetBSD: apcireg.h,v 1.1 1997/04/14 20:36:11 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Michael Smith.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <hp300/dev/iotypes.h>

struct apciregs {
	vu_char		ap_data;
	u_char		pad0[3];
	vu_char		ap_ier;
	u_char		pad1[3];
	vu_char		ap_iir;
#define	ap_fifo	ap_iir
	u_char		pad2[3];
	vu_char		ap_cfcr;
	u_char		pad3[3];
	vu_char		ap_mcr;
	u_char		pad4[3];
	vu_char		ap_lsr;
	u_char		pad5[3];
	vu_char		ap_msr;
	u_char		pad6[3];
	vu_char		ap_scratch;
};

/* max number of apci ports */
#define	APCI_MAXPORT	4

/* Frodo interrupt number of lowest apci port */
#define	APCI_INTR0	12

/*
 * baudrate divisor calculations.
 *
 * The input clock frequency appears to be 8.0064MHz, giving a scale
 * factor of 500400.  (Using exactly 8MHz gives framing errors with
 * the Apollo keyboard.)
 */
#define	APCIBRD(x)	(500000 / (x))
