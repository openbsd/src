/*	$OpenBSD: hpibvar.h,v 1.3 2005/11/23 07:15:58 miod Exp $	*/
/*	$NetBSD: hpibvar.h,v 1.4 1994/10/26 07:27:24 cgd Exp $	*/

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
 *	@(#)hpibvar.h	8.1 (Berkeley) 6/10/93
 */

#define	HPIBA		32
#define	HPIBB		1
#define	HPIBC		8
#define	HPIBA_BA	21
#define	HPIBC_BA	30

#define	CSA_BA		0x1F

#define	C_DCL		20
#define	C_LAG		32
#define	C_UNL		63
#define	C_TAG		64
#define	C_UNA		94
#define	C_UNT		95
#define	C_SCG		96

struct	hpib_softc {
	char	sc_alive;
	char	sc_type;
	int	sc_ba;
	char	*sc_addr;
};

extern	struct hpib_softc hpib_softc[];

/*
 * Description structure for CS/80 devices.
 */

struct cs80_describe {
	u_int	d_iuw:16,	/* controller: installed unit word */
		d_cmaxxfr:16,	/* controller: max transfer rate (Kb) */
		d_ctype:8,	/* controller: controller type */
		d_utype:8,	/* unit: unit type */
		d_name:24,	/* unit: name (6 BCD digits) */
		d_sectsize:16,	/* unit: # of bytes per block (sector) */
		d_blkbuf:8,	/* unit: # of blocks which can be buffered */
		d_burstsize:8,	/* unit: recommended burst size */
		d_blocktime:16,	/* unit: block time (u-sec) */
		d_uavexfr:16,	/* unit: average transfer rate (Kb) */
		d_retry:16,	/* unit: optimal retry time (1/100-sec) */
		d_access:16,	/* unit: access time param (1/100-sec) */
		d_maxint:8,	/* unit: maximum interleave */
		d_fvbyte:8,	/* unit: fixed volume byte */
		d_rvbyte:8,	/* unit: removable volume byte */
		d_maxcyl:24,	/* volume: maximum cylinder */
		d_maxhead:8,	/* volume: maximum head */
		d_maxsect:16,	/* volume: maximum sector on track */
		d_maxvsecth:16,	/* volume: maximum sector on volume (MSW) */
		d_maxvsectl:32,	/* volume: maximum sector on volume (LSWs) */
		d_interleave:8;	/* volume: current interleave */
} __packed;
