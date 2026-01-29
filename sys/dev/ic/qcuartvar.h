/*	$OpenBSD: qcuartvar.h,v 1.1 2026/01/29 11:23:35 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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

struct qcuart_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct soft_intrhand	*sc_si;
	void			*sc_ih;

	struct tty		*sc_tty;
	int			sc_conspeed;
	int			sc_floods;
	int			sc_halt;
	int			sc_cua;
	int	 		*sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
#define QCUART_IBUFSIZE		128
#define QCUART_IHIGHWATER	100
	int			sc_ibufs[2][QCUART_IBUFSIZE];
};

void	qcuart_attach_common(struct qcuart_softc *, int);
int	qcuart_intr(void *);

int	qcuartcnattach(bus_space_tag_t, bus_addr_t);
