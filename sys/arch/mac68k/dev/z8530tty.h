/*	$OpenBSD: z8530tty.h,v 1.1 1996/05/26 19:02:13 briggs Exp $	*/
/*	$NetBSD: z8530tty.h,v 1.1 1996/05/18 18:54:35 briggs Exp $	*/

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
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 Dual UART driver (tty interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for plain "tty" async. serial lines.
 */


/*
 * Allow the MD var.h to override the default CFLAG so that
 * console messages during boot come out with correct parity.
 */
#ifndef	ZSTTY_DEF_CFLAG
#define	ZSTTY_DEF_CFLAG	TTYDEF_CFLAG
#endif

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#ifndef	ZSTTY_RING_SIZE
#define	ZSTTY_RING_SIZE	2048
#endif

struct zstty_stats {
	int ring_block;
	int ring_unblock;
	int tty_block;
	int tty_unblock;
};

struct zstty_softc {
	struct	device zst_dev;		/* required first: base device */
	struct  tty *zst_tty;
	struct	zs_chanstate *zst_cs;

	int zst_hwflags;	/* see z8530var.h */
	int zst_swflags;	/* TIOCFLAG_SOFTCAR, ... <ttycom.h> */

	/*
	 * Printing an overrun error message often takes long enough to
	 * cause another overrun, so we only print one per second.
	 */
	long	zst_rotime;		/* time of last ring overrun */
	long	zst_fotime;		/* time of last fifo overrun */

	/*
	 * The receive ring buffer.
	 */
	int	zst_rbget;	/* ring buffer `get' index */
	volatile int	zst_rbput;	/* ring buffer `put' index */
	int	zst_ringmask;
	int	zst_rbhiwat;

	u_short	*zst_rbuf; /* rr1, data pairs */

	/*
	 * The transmit byte count and address are used for pseudo-DMA
	 * output in the hardware interrupt code.  PDMA can be suspended
	 * to get pending changes done; heldtbc is used for this.  It can
	 * also be stopped for ^S; this sets TS_TTSTOP in tp->t_state.
	 */
	int 	zst_tbc;			/* transmit byte count */
	caddr_t	zst_tba;			/* transmit buffer address */
	int 	zst_heldtbc;		/* held tbc while xmission stopped */

	/* Flags to communicate with zstty_softint() */
	volatile char zst_rx_blocked;
	volatile char zst_rx_overrun;
	volatile char zst_tx_stopped;
	volatile char zst_tx_empty;
	volatile char zst_st_check;
	char pad[3];

	struct termios	zst_termios;	/* default values for tty flags */
#define	zst_cflag	zst_termios.c_cflag
#define	zst_iflag	zst_termios.c_iflag
#define	zst_lflag	zst_termios.c_lflag
#define	zst_oflag	zst_termios.c_oflag
#define zst_cc		zst_termios.c_cc
#define	zst_ispeed	zst_termios.c_ispeed
#define	zst_ospeed	zst_termios.c_ospeed

	char	zst_resetdef;	/* !=0 means reset tty defs. on open */
	char	zst_hwimask;	/* bits to keep low for hwiflow */

	char	zst_hwimasks[4];	/* masks for hwiflow for HFC modes */
};

#define	ZSTTY_RAW_CFLAG (CS8 | CREAD | HUPCL )
#define	ZSTTY_RAW_IFLAG (IXANY | IMAXBEL)
#define	ZSTTY_RAW_LFLAG (ECHOE|ECHOKE|ECHOCTL)
#define	ZSTTY_RAW_OFLAG (ONLCR | OXTABS)
/* Above taken from looking at a tty after a stty raw */
