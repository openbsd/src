/*	$OpenBSD: slivar.h,v 1.4 2007/05/19 04:10:20 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

struct sli_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_iot_slim;
	bus_space_handle_t	sc_ioh_slim;
	bus_size_t		sc_ios_slim;
	bus_space_tag_t		sc_iot_reg;
	bus_space_handle_t	sc_ioh_reg;
	bus_size_t		sc_ios_reg;
};
#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

int	sli_attach(struct sli_softc *);
int	sli_detach(struct sli_softc *, int);

int	sli_intr(void *);
