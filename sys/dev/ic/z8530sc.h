/*	$NetBSD: z8530sc.h,v 1.1 1996/01/24 01:07:24 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)zsvar.h	8.1 (Berkeley) 6/11/93
 */


/*
 * Function vector - per channel
 */
struct zsops {
	int	(*zsop_rxint)();	/* receive char available */
	int	(*zsop_stint)();	/* external/status */
	int	(*zsop_txint)();	/* xmit buffer empty */
	int	(*zsop_softint)();	/* process software interrupt */
};

extern struct zsops zsops_null;


/*
 * Software state, per zs channel.
 */
struct zs_chanstate {

	/* Pointers to the device registers. */
	volatile u_char	*cs_reg_csr; 	/* ctrl, status, and reg. number. */
	volatile u_char	*cs_reg_data;	/* data or numbered register */

	int	cs_channel;		/* sub-unit number */
	void *cs_private;	/* sub-driver data pointer */
	struct zsops *cs_ops;

	int cs_pclk_div16;		/* PCLK / 16 */
	int	cs_defspeed;		/* default baud rate (from PROM) */

	/*
	 * We must keep a copy of the write registers as they are
	 * mostly write-only and we sometimes need to set and clear
	 * individual bits (e.g., in WR3).  Not all of these are
	 * needed but 16 bytes is cheap and this makes the addressing
	 * simpler.  Unfortunately, we can only write to some registers
	 * when the chip is not actually transmitting, so whenever
	 * we are expecting a `transmit done' interrupt the preg array
	 * is allowed to `get ahead' of the current values.  In a
	 * few places we must change the current value of a register,
	 * rather than (or in addition to) the pending value; for these
	 * cs_creg[] contains the current value.
	 */
	u_char	cs_creg[16];		/* current values */
	u_char	cs_preg[16];		/* pending values */

	u_char	cs_heldchange;		/* change pending (creg != preg) */
	u_char	cs_rr0;			/* last rr0 processed */

	char	cs_softreq;		/* need soft interrupt call */
	char	cs__spare;
};

struct zsc_softc {
	struct	device zsc_dev;		/* required first: base device */
	struct	zs_chanstate zsc_cs[2];	/* channel A and B soft state */
};

struct zsc_attach_args {
	int channel;	/* two serial channels per zsc */
	int hwflags;
};
#define ZS_HWFLAG_CONSOLE 1

