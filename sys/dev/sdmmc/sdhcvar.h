/*	$OpenBSD: sdhcvar.h,v 1.11 2018/03/19 21:40:32 kettenis Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

#ifndef _SDHCVAR_H_
#define _SDHCVAR_H_

#include <machine/bus.h>

struct sdhc_host;

struct sdhc_softc {
	struct device sc_dev;
	struct sdhc_host **sc_host;
	int sc_nhosts;
	u_int sc_flags;
	u_int sc_clkbase;

	bus_dma_tag_t sc_dmat;

	int (*sc_card_detect)(struct sdhc_softc *);
	int (*sc_signal_voltage)(struct sdhc_softc *, int);
};

/* Host controller functions called by the attachment driver. */
int	sdhc_host_found(struct sdhc_softc *, bus_space_tag_t,
	    bus_space_handle_t, bus_size_t, int, u_int32_t);
int	sdhc_activate(struct device *, int);
void	sdhc_shutdown(void *);
int	sdhc_intr(void *);

void	sdhc_needs_discover(struct sdhc_softc *);

/* flag values */
#define SDHC_F_NOPWR0		(1 << 0)
#define SDHC_F_NODDR50		(1 << 1)

#endif
