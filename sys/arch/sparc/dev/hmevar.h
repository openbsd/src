/*	$OpenBSD: hmevar.h,v 1.11 2004/09/28 00:21:23 brad Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
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

struct hme_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	intrhand sc_ih;		/* interrupt vectoring */
	int	sc_node;		/* which sbus node */

	mii_data_t	sc_mii;		/* mii bus */

	struct	arpcom sc_arpcom;	/* ethernet common */

	/*
	 * Register sets
	 */
	struct	hme_gr *sc_gr;		/* global registers */
	struct	hme_txr *sc_txr;	/* transmitter regs */
	struct	hme_rxr *sc_rxr;	/* receiver registers */
	struct	hme_cr *sc_cr;		/* configuration registers */
	struct	hme_tcvr *sc_tcvr;	/* MIF registers */

	int	sc_burst;		/* DMA burst size in effect */
	int	sc_rev;			/* Card revision */

	u_int32_t	sc_flags;	/* status flags	*/

	short		sc_if_flags;

	/*
	 * RX/TX ring buffers, descriptors, and counters
	 */
	struct	hme_desc *sc_desc, *sc_desc_dva;	/* ring descriptors */
	struct	hme_bufs *sc_bufs, *sc_bufs_dva;	/* packet buffers */
	int	sc_first_td, sc_last_td, sc_no_td;	/* tx counters */
	int	sc_last_rd;				/* rx counters */
};
