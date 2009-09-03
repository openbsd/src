/*	$OpenBSD: pxammcvar.h,v 1.2 2009/09/03 21:40:29 marex Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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

#include <machine/bus.h>

struct pxammc_tag {
	void *cookie;
	u_int32_t (*get_ocr)(void *);
	int (*set_power)(void *, u_int32_t);
};

struct pxammc_softc {
	struct device sc_dev;		/* base device */
	struct pxammc_tag tag;		/* attachment driver functions */
	bus_space_tag_t sc_iot;		/* register space tag */
	bus_space_handle_t sc_ioh;	/* register space handle */
	struct device *sc_sdmmc;	/* generic sdmmc bus device */
	void *sc_card_ih;		/* card interrupt handle */
	void *sc_ih;			/* MMC interrupt handle */
	int sc_flags;			/* driver state flags */
#define PMF_CARD_INITED	0x0001		/* card init sequence sent */
	int sc_clkdiv;			/* current clock divider */
	struct sdmmc_command * volatile sc_cmd;	/* command in progress */
	int sc_gpio_detect;		/* card detect GPIO */
};

int	pxammc_match(void);
void	pxammc_attach(struct pxammc_softc *, void *);
