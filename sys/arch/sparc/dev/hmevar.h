/*	$OpenBSD: hmevar.h,v 1.2 1998/07/17 21:33:10 jason Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

	/*
	 * Register sets
	 */
	struct	hme_gr *sc_gr;		/* global registers */
	struct	hme_txr *sc_txr;	/* transmitter regs */
	struct	hme_rxr *sc_rxr;	/* receiver registers */
	struct	hme_cr *sc_cr;		/* configuration registers */
	struct	hme_tcvr *sc_tcvr;	/* MIF registers */

	struct	hme_swr sc_sw;		/* software copy registers */
	int	sc_burst;		/* DMA burst size in effect */
	int	sc_rev;			/* Card revision */

	u_int32_t	sc_flags;	/* status flags	*/
	u_int32_t	sc_promisc;	/* are we promiscuous? */
	u_int32_t	sc_phyaddr;	/* PHY addr */
	int		sc_an_state;	/* state of negotiation */
	int		sc_an_ticks;	/* how long has passed? */
	int		sc_tcvr_type;	/* transceiver type */

	/*
	 * RX/TX ring buffers, descriptors, and counters
	 */
	struct	stp_base sc_stp;
};
