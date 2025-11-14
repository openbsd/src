/* $OpenBSD: ispivar.h,v 1.1 2025/11/14 01:55:07 jcs Exp $ */
/*
 * Intel LPSS SPI controller
 *
 * Copyright (c) 2015-2018 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include "acpi.h"
#if NACPI > 0
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>
#endif

#include <dev/pci/pcivar.h>

#include <dev/spi/spivar.h>

/* #define ISPI_DEBUG */

#ifdef ISPI_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct ispi_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	void			*sc_ih;
	struct pci_attach_args	sc_paa;
	long			sc_ssp_clk;
	int			sc_lpss_reg_offset;
	int			sc_reg_cs_ctrl;
	int			sc_rx_threshold;
	int			sc_tx_threshold;
	int			sc_tx_threshold_hi;

	struct spi_controller	sc_spi_tag;
	struct rwlock		sc_buslock;
	struct spi_config	sc_spi_conf;

	int			 sc_ridx;
	int			 sc_widx;

#if NACPI > 0
	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
#endif
	u_int32_t		sc_caps;
};

void		ispi_init(struct ispi_softc *sc);

int		ispi_match(struct device *, void *, void *);
void		ispi_attach(struct device *, struct device *, void *);
int		ispi_activate(struct device *, int);
int		ispi_spi_print(void *aux, const char *pnp);

void		ispi_write(struct ispi_softc *sc, int reg, uint32_t val);
uint32_t	ispi_read(struct ispi_softc *sc, int reg);

#if NACPI > 0
int		ispi_acpi_found_hid(struct aml_node *node, void *arg);
#endif

void		ispi_config(void *, struct spi_config *);
int		ispi_acquire_bus(void *, int);
void		ispi_release_bus(void *, int);
int		ispi_transfer(void *, char *, char *, int, int);
void		ispi_start(struct ispi_softc *);
void		ispi_send(struct ispi_softc *);
void		ispi_recv(struct ispi_softc *);

int		ispi_intr(void *);
int		ispi_status(struct ispi_softc *);
int		ispi_flush(struct ispi_softc *);
void		ispi_clear_status(struct ispi_softc *);
int		ispi_rx_fifo_empty(struct ispi_softc *);
int		ispi_rx_fifo_overrun(struct ispi_softc *);
