/*	$NetBSD: hpibvar.h,v 1.8 1996/02/14 02:44:31 thorpej Exp $	*/

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
 *	@(#)hpibvar.h	8.1 (Berkeley) 6/10/93
 */

#define	HPIB_IPL(x)	((((x) >> 4) & 0x3) + 3)

#define	HPIBA		32
#define	HPIBB		1
#define	HPIBC		8
#define	HPIBA_BA	21
#define	HPIBC_BA	30
#define	HPIBA_IPL	3

#define	CSA_BA		0x1F

#define	IDS_WDMA	0x04
#define	IDS_WRITE	0x08
#define	IDS_IR		0x40
#define	IDS_IE		0x80
#define	IDS_DMA(x)	(1 << (x))

#define	C_SDC		0x04	/* Selected device clear */
#define	C_SDC_P		0x04	/*  with odd parity */
#define	C_DCL		0x14	/* Universal device clear */
#define	C_DCL_P		0x94	/*  with odd parity */
#define	C_LAG		0x20	/* Listener address group commands */
#define	C_UNL		0x3f	/* Universal unlisten */
#define	C_UNL_P		0xbf	/*  with odd parity */
#define	C_TAG		0x40	/* Talker address group commands */
#define	C_UNA		0x5e	/* Unaddress (master talk address?) */
#define	C_UNA_P		0x5e	/*  with odd parity */
#define	C_UNT		0x5f	/* Universal untalk */
#define	C_UNT_P		0xdf	/*  with odd parity */
#define	C_SCG		0x60	/* Secondary group commands */

struct	hpib_softc {
	struct	hp_ctlr *sc_hc;
	struct	hpib_controller *sc_controller;
	char	*sc_descrip;
	int	sc_flags;
	struct	devqueue sc_dq;
	struct	devqueue sc_sq;
	int	sc_ba;
	int	sc_type;
	char	*sc_addr;
	int	sc_count;
	int	sc_curcnt;
};

/*
 * Each of the HP-IB controller drivers fills in this structure, which
 * is used by the indirect driver to call controller-specific functions.
 */
struct	hpib_controller {
	void	(*hpib_reset) __P((int));
	int	(*hpib_send) __P((int, int, int, void *, int));
	int	(*hpib_recv) __P((int, int, int, void *, int));
	int	(*hpib_ppoll) __P((int));
	void	(*hpib_ppwatch) __P((void *));
	void	(*hpib_go) __P((int, int, int, void *, int, int, int));
	void	(*hpib_done) __P((int));
	int	(*hpib_intr) __P((void *));
};

/* sc_flags */
#define	HPIBF_IO	0x1
#define	HPIBF_DONE	0x2
#define	HPIBF_PPOLL	0x4
#define	HPIBF_READ	0x8
#define	HPIBF_TIMO	0x10
#define	HPIBF_DMA16	0x8000

#ifdef _KERNEL
extern	struct hpib_softc hpib_softc[];
extern	caddr_t internalhpib;
extern	int hpibtimeout;
extern	int hpibdmathresh;

void	hpibreset __P((int));
int	hpibsend __P((int, int, int, void *, int));
int	hpibrecv __P((int, int, int, void *, int));
#endif
