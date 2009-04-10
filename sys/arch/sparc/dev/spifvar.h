/*	$OpenBSD: spifvar.h,v 1.5 2009/04/10 20:53:51 miod Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	SPIF_MAX_SERIAL	8
#define SPIF_MAX_PARALLEL 1

struct stty_port {
	struct tty *sp_tty;		/* tty device */
	struct spif_softc *sp_sc;	/* pointer back to registers */
	int sp_channel;			/* channel number */
	u_char *sp_rbuf;		/* ring buffer start */
	u_char *sp_rend;		/* ring buffer end */
	u_char *sp_rget;		/* ring buffer read pointer */
	u_char *sp_rput;		/* ring buffer write pointer */
	u_char *sp_txp;			/* transmit character pointer */
	int sp_txc;			/* transmit character counter */

	int sp_openflags;		/* open flags */
	int sp_carrier;			/* software carrier status */
	int sp_flags;			/* software state */
	char sp_dtr;			/* software dtr status */
};

struct stty_softc {
	struct	device sc_dev;		/* base device */
	int	sc_nports;		/* number of serial ports */
	struct	stty_port sc_port[SPIF_MAX_SERIAL];
};

struct sbpp_softc {
	struct	device sc_dev;		/* base device */
	int	sc_nports;		/* number of parallel ports */
};

struct spif_softc {
	struct	device sc_dev;		/* base device */
	struct	intrhand sc_stcih;	/* stc interrupt vectoring */
	struct	intrhand sc_ppcih;	/* ppc interrupt vectoring */
	void	*sc_softih;		/* soft interrupt vectoring */
	int	sc_rev;			/* revision level */
	int	sc_osc;			/* oscillator speed (hz) */
	int	sc_node;		/* which sbus node */
	int	sc_nser;		/* number of serial ports */
	int	sc_npar;		/* number of parallel ports */
	int	sc_rev2;		/* cd180 chip revision */
	struct	spifregs *sc_regs;	/* registers */
	struct	stty_softc *sc_ttys;	/* our ttys */
	struct	sbpp_softc *sc_bpps;	/* our ttys */
};

