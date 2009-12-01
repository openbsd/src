/*	$OpenBSD: ce4231var.h,v 1.9 2009/12/01 23:17:11 edd Exp $	*/

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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
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

/* number of levels on the card, these relate to CSPORT_* */
#define CS4231_LVLS	7

struct ce4231_softc {
	struct	device sc_dev;		/* base device */
	struct	intrhand sc_ih;		/* interrupt vectoring */
	bus_dma_tag_t sc_dmatag;
	bus_space_tag_t	sc_bustag;	/* CS4231/DMA register tag */
	bus_space_handle_t sc_cshandle;	/* CS4231 handle */
	bus_space_handle_t sc_cdmahandle; /* capture DMA handle */
	bus_space_handle_t sc_pdmahandle; /* playback DMA handle */
	bus_space_handle_t sc_auxhandle;  /* AUX handle */
	int	sc_open;		/* already open? */
	int	sc_locked;		/* locked? */

	void	(*sc_rintr)(void *);	/* input completion intr handler */
	void	*sc_rarg;		/* arg for sc_rintr() */
	void	(*sc_pintr)(void *);	/* output completion intr handler */
	void	*sc_parg;		/* arg for sc_pintr() */

	char		sc_mute[CS4231_LVLS];	/* which devs are muted */
	u_int8_t	sc_monoout_enable; /* whether the mono dacout is on */
	u_int8_t	sc_lineout_enable; /* whether the line dacout is on */
	u_int8_t	sc_hdphout_enable; /* whether the cans dacout is on */
	u_int8_t	sc_rec_src;	   /* recording src */
	struct	cs_volume sc_volume[CS4231_LVLS];	/* software volume */

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
	u_int32_t	sc_lastaddr;
	struct cs_dma	*sc_dmas;	/* dma list */
	struct cs_dma	*sc_nowplaying;
	void *sc_pih, *sc_cih;
};
