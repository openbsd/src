/*
 * Copyright (c) 2009 Marek Vasut <marex@openbsd.org>
 *
 * Moved from pxa27x_udc.c:
 *
 * Copyright (c) 2007 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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

#include <arm/xscale/pxa27x_udcreg.h>

#define PXAUDC_EP0MAXP	16	/* XXX */
#define PXAUDC_NEP	24	/* total number of endpoints */

struct pxaudc_softc {
	struct usbf_bus		 sc_bus;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	bus_size_t		 sc_size;
	void			*sc_ih;
	void			*sc_conn_ih;
	SIMPLEQ_HEAD(,usbf_xfer) sc_free_xfers;	/* recycled xfers */
	u_int32_t		 sc_icr0;	/* enabled EP interrupts */
	u_int32_t		 sc_icr1;	/* enabled EP interrupts */
	enum {
		EP0_SETUP,
		EP0_IN
	}			 sc_ep0state;
	u_int32_t		 sc_isr0;	/* XXX deferred interrupts */
	u_int32_t		 sc_isr1;	/* XXX deferred interrupts */
	u_int32_t		 sc_otgisr;	/* XXX deferred interrupts */
	struct pxaudc_pipe	*sc_pipe[PXAUDC_NEP];
	int			 sc_npipe;

	int			 sc_cn;
	int			 sc_in;
	int			 sc_isn;
	int8_t			 sc_ep_map[16];

	struct device		*sc_dev;

	int			sc_gpio_detect;
	int			sc_gpio_detect_inv;

	int			sc_gpio_pullup;
	int			sc_gpio_pullup_inv;

	int			(*sc_is_host)(void);
};

int		 pxaudc_match(void);
void		 pxaudc_attach(struct pxaudc_softc *, void *);
int		 pxaudc_detach(struct pxaudc_softc *, int);
int		 pxaudc_activate(struct pxaudc_softc *, int);
