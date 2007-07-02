/*	$OpenBSD: ahcivar.h,v 1.1 2007/07/02 00:52:25 dlg Exp $ */

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

struct ahci_port;

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	int			sc_flags;
#define AHCI_F_NO_NCQ			(1<<0)

	u_int			sc_ncmds;

#define AHCI_MAX_PORTS          32
	struct ahci_port	*sc_ports[AHCI_MAX_PORTS];

	struct atascsi		*sc_atascsi;

#ifdef AHCI_COALESCE
	u_int32_t		sc_ccc_mask;
	u_int32_t		sc_ccc_ports;
	u_int32_t		sc_ccc_ports_cur;
#endif
};

int	ahci_attach(struct ahci_softc *, struct pci_attach_args *,
	    pci_intr_handle_t);
