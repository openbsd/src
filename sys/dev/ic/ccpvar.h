/*	$OpenBSD: ccpvar.h,v 1.5 2024/09/03 00:23:05 jsg Exp $ */

/*
 * Copyright (c) 2018 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2023, 2024 Hans-Joerg Hoexer <hshoexer@genua.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/timeout.h>
#include <sys/rwlock.h>

struct ccp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct timeout		sc_tick;

	int			sc_psp_attached;

	bus_dma_tag_t		sc_dmat;
	uint32_t		sc_capabilities;
	int			(*sc_sev_intr)(struct ccp_softc *, uint32_t);
	void *			sc_ih;

	bus_dmamap_t		sc_cmd_map;
	bus_dma_segment_t	sc_cmd_seg;
	size_t			sc_cmd_size;
	caddr_t			sc_cmd_kva;

	bus_dmamap_t		sc_tmr_map;
	bus_dma_segment_t	sc_tmr_seg;
	size_t			sc_tmr_size;
	caddr_t			sc_tmr_kva;

	struct rwlock		sc_lock;
};

void	ccp_attach(struct ccp_softc *);
