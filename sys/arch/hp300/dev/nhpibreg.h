/*	$OpenBSD: nhpibreg.h,v 1.3 2003/06/02 23:27:45 millert Exp $	*/
/*	$NetBSD: nhpibreg.h,v 1.5 1995/01/07 10:30:15 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
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
 *	@(#)nhpibreg.h	8.1 (Berkeley) 6/10/93
 */

#include <hp300/dev/iotypes.h>	/* XXX */

struct	nhpibdevice {
	u_char	hpib_pad0;
	vu_char	hpib_cid;
	u_char	hpib_pad1;
#define	hpib_ie		hpib_ids
	vu_char	hpib_ids;
	u_char	hpib_pad2;
	vu_char	hpib_csa;
	u_char	hpib_pad3[11];
#define	hpib_mim	hpib_mis
	vu_char	hpib_mis;
	u_char	hpib_pad4;
#define	hpib_lim	hpib_lis
	vu_char	hpib_lis;
	u_char	hpib_pad5;
	vu_char	hpib_is;
	u_char	hpib_pad6;
#define	hpib_acr	hpib_cls
	vu_char	hpib_cls;
	u_char	hpib_pad7;
	vu_char	hpib_ar;
	u_char	hpib_pad8;
	vu_char	hpib_sprb;
	u_char	hpib_pad9;
#define	hpib_ppr	hpib_cpt
	vu_char	hpib_cpt;
	u_char	hpib_pad10;
	vu_char	hpib_data;
};

/*
 * Bits in hpib_lis (and hpib_lim).
 */
#define	LIS_IFC		0x01
#define	LIS_SRQ		0x02
#define	LIS_MA		0x04
#define	LIS_DCAS	0x08
#define	LIS_APT		0x10
#define	LIS_UCG		0x20
#define	LIS_ERR		0x40
#define	LIS_GET		0x80

#define	MIS_END		0x08
#define	MIS_BO		0x10
#define	MIS_BI		0x20

#define	IS_TADS		0x02
#define	IS_LADS		0x04

/*
 * ti9914 "Auxiliary Commands" - Some are Set/Clear, others pulse.
 */
#define	AUX_CSWRST	0x00	/* End software reset */
#define	AUX_RHDF	0x02	/* release RFD (ready for data) holdoff */
#define	AUX_CHDFA	0x03	/* Clear holdoff on all data */
#define	AUX_CHDFE	0x04	/* Clear holdoff on EOI data only */
#define	AUX_EOI		0x08	/* Pulse EOI (with data) */
#define	AUX_CLON	0x09	/* Clear listen only */
#define	AUX_CTON	0x0a	/* Clear talk only */
#define	AUX_GTS		0x0b	/* Go to standby (clears ATN line) */
#define	AUX_TCA		0x0c	/* Take control (async) */
#define	AUX_TCS		0x0d	/* Take control (sync) */
#define	AUX_CPP		0x0e	/* Clear parallel poll */
#define	AUX_CSIC	0x0f	/* Clear IFC (interface clear) line */
#define	AUX_CSRE	0x10	/* Clear REN (remote enable) line */
#define	AUX_CDAI	0x13	/* Clear interrupt disable */
#define	AUX_CSTD1	0x15	/* Clear 1200ns T1 delay */
#define	AUX_CSHDW	0x16	/* Clear shadow handshake */
#define	AUX_CVSTD1	0x17	/* Clear 600ns T1 delay */

#define	AUX_SSWRST	0x80	/* Start software reset */
#define	AUX_SHDFA	0x83	/* Set holdoff on all data */
#define	AUX_SHDFE	0x84	/* Set holdoff on EOI data only */
#define	AUX_SLON	0x89	/* Set listen only */
#define	AUX_STON	0x8a	/* Set talk only */
#define	AUX_SPP		0x8e	/* Set parallel poll */
#define	AUX_SSIC	0x8f	/* Set IFC line */
#define	AUX_SSRE	0x90	/* Set REN line */
#define	AUX_SDAI	0x93	/* Disable all interrupts */
#define	AUX_SSTD1	0x95	/* Set T1 delay to 1200ns */
#define	AUX_SSHDW	0x96	/* Set shadow handshake */
#define	AUX_SVSTD1	0x97	/* Set T1 delay to 600ns */
