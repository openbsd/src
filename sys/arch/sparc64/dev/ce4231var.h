/*	$OpenBSD: ce4231var.h,v 1.1 2001/10/01 18:10:32 jason Exp $	*/

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

/*
 * Driver for CS4231/EBDMA based audio found in some sun4u systems
 */

/*
 * List of device memory allocations (see ce4231_malloc/ce4231_free).
 */
struct cs_dma {
	struct cs_dma *		next;
	caddr_t			addr;
	bus_dmamap_t		dmamap;
	bus_dma_segment_t	segs[1];
	int			nsegs;
	size_t			size;
};

struct cs_volume {
	u_int8_t	left;
	u_int8_t	right;
};

struct ce4231_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	intrhand sc_ih;		/* interrupt vectoring */
	bus_dma_tag_t sc_dmatag;
	bus_space_tag_t	sc_bustag;	/* CS4231/DMA register tag */
	bus_space_handle_t sc_cshandle;	/* CS4231 handle */
	bus_space_handle_t sc_cdmahandle; /* capture DMA handle */
	bus_space_handle_t sc_pdmahandle; /* playback DMA handle */
	bus_space_handle_t sc_auxhandle;  /* AUX handle */
	struct	evcnt sc_intrcnt;	/* statistics */
	int	sc_open;		/* already open? */
	int	sc_locked;		/* locked? */

	void	(*sc_rintr)(void*);	/* input completion intr handler */
	void *	sc_rarg;		/* arg for sc_rintr() */
	void	(*sc_pintr)(void*);	/* output completion intr handler */
	void *	sc_parg;		/* arg for sc_pintr() */

	char		sc_mute[9];	/* which devs are muted */
	u_int8_t	sc_out_port;	/* output port */
	struct	cs_volume sc_volume[9];	/* software volume */

	int sc_format_bits;
	int sc_speed_bits;
	int sc_precision;
	int sc_need_commit;
	int sc_channels;
	u_int sc_last_format;
	u_int32_t	sc_blksz;
	u_int32_t	sc_playcnt;
	u_int32_t	sc_playsegsz;
	u_int32_t	sc_burst;
	struct cs_dma	*sc_dmas;	/* dma list */
	struct cs_dma	*sc_nowplaying;
};
