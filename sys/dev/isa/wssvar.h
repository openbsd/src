/*	$OpenBSD: wssvar.h,v 1.1 1998/04/26 21:02:38 provos Exp $	*/
/*	$NetBSD: wssvar.h,v 1.1 1998/01/19 22:18:25 augustss Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

/*
 * Mixer devices
 */
#define WSS_MIC_IN_LVL		0
#define WSS_LINE_IN_LVL		1
#define WSS_DAC_LVL		2
#define WSS_REC_LVL		3
#define WSS_MON_LVL		4
#define WSS_MIC_IN_MUTE		5
#define WSS_LINE_IN_MUTE	6
#define WSS_DAC_MUTE		7

#define WSS_RECORD_SOURCE	8

/* Classes */
#define WSS_INPUT_CLASS		9
#define WSS_RECORD_CLASS	10
#define WSS_MONITOR_CLASS	11

struct wss_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	isa_chipset_tag_t sc_ic;

	struct  ad1848_softc sc_ad1848;
#define wss_irq    sc_ad1848.sc_irq
#define wss_drq    sc_ad1848.sc_drq
#define wss_recdrq sc_ad1848.sc_recdrq

	int 	mic_mute, cd_mute, dac_mute;

	int	mad_chip_type;		/* chip type if MAD emulation of WSS */
	int	mad_ioindex;
	bus_space_handle_t mad_ioh;	/* MAD handle */
	bus_space_handle_t mad_ioh1, mad_ioh2, mad_ioh3;
};

void	wssattach __P((struct wss_softc *));

u_int	mad_read __P((struct wss_softc *, int));
void	mad_write __P((struct wss_softc *, int, int));
void	madattach __P((struct wss_softc *));
