/*	$OpenBSD: z8530sc.h,v 1.1 1996/05/26 19:02:11 briggs Exp $	*/
/*	$NetBSD: z8530sc.h,v 1.1 1996/05/18 18:54:30 briggs Exp $	*/

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
 * Clock source info structure
 */
struct zsclksrc {
	long	clk;	/* clock rate, in MHz, present on signal line */
	int	flags;	/* Specifies how this source can be used
			   (RTxC divided, RTxC BRG, PCLK BRG, TRxC divided)
			   and also if the source is "external" and if it
			   is changeable (by an ioctl ex.). The
			   source usage flags are used by the tty
			   child. The other bits tell zsloadchannelregs
			   if it should call an md signal source
			   changing routine. ZSC_VARIABLE says if
			   an ioctl should be able to cahnge the
			   clock rate.*/
};
#define	ZSC_PCLK	0x01
#define	ZSC_RTXBRG	0x02
#define	ZSC_RTXDIV	0x04
#define	ZSC_TRXDIV	0x08
#define	ZSC_VARIABLE	0x40
#define	ZSC_EXTERN	0x80

#define	ZSC_BRG		0x03
#define ZSC_DIV		0x0c


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

	int	cs_defspeed;		/* default baud rate (from PROM) */
	int cs_pclk_div16;		/* PCLK / 16 used only by kbd & ms kids */
	int	cs_clock_count;		/* how many signal sources available */
	struct zsclksrc cs_clocks[4];	/* info on available signal sources */

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
	long	cs_cclk_flag;		/* flag for current clock source */
	long	cs_pclk_flag;		/* flag for pending clock source */
	int	cs_csource;		/* current source # */
	int	cs_psource;		/* pending source # */

	u_char	cs_heldchange;		/* change pending (creg != preg) */
	u_char	cs_rr0;			/* last rr0 processed */
	u_char	cs_rr0_new; 	/* rr0 saved in status interrupt. */

	char	cs_softreq;		/* need soft interrupt call */
	char	cs_chip;		/* type of chip */
	char	cs__spare; 
};
#define	ZS_ENHANCED_REG	8
	/* cs_Xreg which is used to hold WR7' data; reg 8 is an alias to the
	 * data port, so we won't miss its loss. */

/*
 * Function vector - per channel
 */
typedef void	(*zsop_t)(register struct zs_chanstate *);
struct zsops {
	zsop_t	zsop_rxint;	/* receive char available */
	zsop_t	zsop_stint;	/* external/status */
	zsop_t	zsop_txint;	/* xmit buffer empty */
	zsop_t	zsop_softint;	/* process software interrupt */
};

extern struct zsops zsops_null;

struct zsc_softc {
	struct	device zsc_dev;		/* required first: base device */
	struct	zs_chanstate zsc_cs[2];	/* channel A and B soft state */
};

struct zsc_attach_args {
	int channel;	/* two serial channels per zsc */
	int hwflags;
};
#define ZS_HWFLAG_CONSOLE	1
#define ZS_HWFLAG_CONABRT	2
#define ZS_HWFLAG_RAW		4
#define ZS_HWFLAG_IGCTS		16
#define ZS_HWFLAG_IGDCD		32
/* _CONSOLE says this port is the console, _CONABRT says a Break sequence acts as
	an abort, and _RAW recomends "raw" mode defaults on a tty.
	_CONABRT is turned off if an overly-long break is received.
	_IGCTS and _IGDCD tell the tty layer to ignore CTS or DCD. Assume
	whatever's least supprising (CTS and DCD present). Used mainly for
	external clock support on mac68k. The DCD and CTS pins are used also
	for clock inputs; not good for the UNIX I/O model! */

#define ZS_CHIP_NMOS		0
#define ZS_CHIP_CMOS		1
#define ZS_CHIP_8580		2
#define ZS_CHIP_ESCC		3

void	zs_loadchannelregs __P((struct zs_chanstate *));
int	zsc_intr_soft __P((void *));
int	zsc_intr_hard __P((void *));
int	zs_checkchip __P((struct zs_chanstate *));
int	zs_break __P((struct zs_chanstate *, int));
int	zs_getspeed __P((struct zs_chanstate *));
void	zs_iflush __P((struct zs_chanstate *));

