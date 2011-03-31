/*	$OpenBSD: esp_pcmcia.c,v 1.10 2011/03/31 13:05:27 jasper Exp $	*/
/*	$NetBSD: esp_pcmcia.c,v 1.8 2000/06/05 15:36:45 tsutsui Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

struct esp_pcmcia_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */

	int		sc_active;		/* Pseudo-DMA state vars */
	int		sc_tc;
	int		sc_datain;
	size_t		sc_dmasize;
	size_t		sc_dmatrans;
	char		**sc_dmaaddr;
	size_t		*sc_pdmalen;

	/* PCMCIA-specific goo. */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handler */
#ifdef ESP_PCMCIA_POLL
	struct callout sc_poll_ch;
#endif
	int sc_flags;
#define	ESP_PCMCIA_ATTACHED	1		/* attach completed */
#define ESP_PCMCIA_ATTACHING	2		/* attach in progress */
};

int	esp_pcmcia_match(struct device *, void *, void *); 
void	esp_pcmcia_attach(struct device *, struct device *, void *);  
void	esp_pcmcia_init(struct esp_pcmcia_softc *);
int	esp_pcmcia_detach(struct device *, int);
int	esp_pcmcia_enable(void *, int);

struct scsi_adapter esp_pcmcia_adapter = {
	ncr53c9x_scsi_cmd,	/* cmd */
	scsi_minphys,		/* scsi_minphys */
	0,			/* open */
	0,			/* close */
};

struct cfattach esp_pcmcia_ca = {
	sizeof(struct esp_pcmcia_softc), esp_pcmcia_match, esp_pcmcia_attach
};

/*
 * Functions and the switch for the MI code.
 */
#ifdef ESP_PCMCIA_POLL
void	esp_pcmcia_poll(void *);
#endif
u_char	esp_pcmcia_read_reg(struct ncr53c9x_softc *, int);
void	esp_pcmcia_write_reg(struct ncr53c9x_softc *, int, u_char);
int	esp_pcmcia_dma_isintr(struct ncr53c9x_softc *);
void	esp_pcmcia_dma_reset(struct ncr53c9x_softc *);
int	esp_pcmcia_dma_intr(struct ncr53c9x_softc *);
int	esp_pcmcia_dma_setup(struct ncr53c9x_softc *, caddr_t *,
	    size_t *, int, size_t *);
void	esp_pcmcia_dma_go(struct ncr53c9x_softc *);
void	esp_pcmcia_dma_stop(struct ncr53c9x_softc *);
int	esp_pcmcia_dma_isactive(struct ncr53c9x_softc *);

struct ncr53c9x_glue esp_pcmcia_glue = {
	esp_pcmcia_read_reg,
	esp_pcmcia_write_reg,
	esp_pcmcia_dma_isintr,
	esp_pcmcia_dma_reset,
	esp_pcmcia_dma_intr,
	esp_pcmcia_dma_setup,
	esp_pcmcia_dma_go,
	esp_pcmcia_dma_stop,
	esp_pcmcia_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

struct esp_pcmcia_product {
	u_int16_t	app_vendor;		/* PCMCIA vendor ID */
	u_int16_t	app_product;		/* PCMCIA product ID */
	int		app_expfunc;		/* expected function number */
} esp_pcmcia_prod[] = {
	{ PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KXLC002, 0 },
	{ PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KME, 0 },
	{ PCMCIA_VENDOR_NEWMEDIA2, PCMCIA_PRODUCT_NEWMEDIA2_BUSTOASTER, 0 }
};

int
esp_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;
	int i;

	for (i = 0; i < nitems(esp_pcmcia_prod); i++)
		if (pa->manufacturer == esp_pcmcia_prod[i].app_vendor &&
		    pa->product == esp_pcmcia_prod[i].app_product &&
		    pa->pf->number == esp_pcmcia_prod[i].app_expfunc)
			return (1);
	return (0);
}

void
esp_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct esp_pcmcia_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf = pa->pf;
	const char *intrstr;

	esc->sc_pf = pf;

	for (cfe = SIMPLEQ_FIRST(&pf->cfe_head); cfe != NULL;
	    cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {
		if (cfe->num_memspace != 0 ||
		    cfe->num_iospace != 1)
			continue;

		if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
		    cfe->iospace[0].length, 0, &esc->sc_pcioh) == 0)
			break;
	}

	if (cfe == 0) {
		printf(": can't alloc i/o space\n");
		goto no_config_entry;
	}

	/* Enable the card. */
	pcmcia_function_init(pf, cfe);
	if (pcmcia_function_enable(pf)) {
		printf(": function enable failed\n");
		goto enable_failed;
	}

	/* Map in the I/O space */
	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0, esc->sc_pcioh.size,
	    &esc->sc_pcioh, &esc->sc_io_window)) {
		printf(": can't map i/o space\n");
		goto iomap_failed;
	}

	printf(" port 0x%lx/%lu", esc->sc_pcioh.addr,
	    (u_long)esc->sc_pcioh.size);

	esp_pcmcia_init(esc);

	esc->sc_ih = pcmcia_intr_establish(esc->sc_pf, IPL_BIO,
	    ncr53c9x_intr, &esc->sc_ncr53c9x, sc->sc_dev.dv_xname);
	intrstr = pcmcia_intr_string(esc->sc_pf, esc->sc_ih);
	if (esc->sc_ih == NULL) {
		printf(", %s\n", intrstr);
		goto iomap_failed;
	}
	if (*intrstr)
		printf(", %s", intrstr);

	/*
	 *  Initialize nca board itself.
	 */
	esc->sc_flags |= ESP_PCMCIA_ATTACHING;
	ncr53c9x_attach(sc, &esp_pcmcia_adapter);
	esc->sc_flags &= ~ESP_PCMCIA_ATTACHING;
	esc->sc_flags |= ESP_PCMCIA_ATTACHED;
	return;

iomap_failed:
	/* Disable the device. */
	pcmcia_function_disable(esc->sc_pf);

enable_failed:
	/* Unmap our I/O space. */
	pcmcia_io_free(esc->sc_pf, &esc->sc_pcioh);

no_config_entry:
	return;
}

void
esp_pcmcia_init(esc)
	struct esp_pcmcia_softc *esc;
{
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	bus_space_tag_t iot = esc->sc_pcioh.iot;
	bus_space_handle_t ioh = esc->sc_pcioh.ioh;

	/* id 7, clock 40M, parity ON, sync OFF, fast ON, slow ON */

	sc->sc_glue = &esp_pcmcia_glue;

#ifdef ESP_PCMCIA_POLL
	callout_init(&esc->sc_poll_ch);
#endif

	sc->sc_rev = NCR_VARIANT_ESP406;
	sc->sc_id = 7;
	sc->sc_freq = 40;
	/* try -PARENB -SLOW */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB | NCRCFG1_SLOW;
	/* try +FE */
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	/* try -IDM -FSCSI -FCLK */
	sc->sc_cfg3 = NCRESPCFG3_CDB | NCRESPCFG3_FCLK | NCRESPCFG3_IDM |
	    NCRESPCFG3_FSCSI;
	sc->sc_cfg4 = NCRCFG4_ACTNEG;
	/* try +INTP */
	sc->sc_cfg5 = NCRCFG5_CRS1 | NCRCFG5_AADDR | NCRCFG5_PTRINC;
	sc->sc_minsync = 0;
	sc->sc_maxxfer = 64 * 1024;

	bus_space_write_1(iot, ioh, NCR_CFG5, sc->sc_cfg5);

	bus_space_write_1(iot, ioh, NCR_PIOI, 0);
	bus_space_write_1(iot, ioh, NCR_PSTAT, 0);
	bus_space_write_1(iot, ioh, 0x09, 0x24);

	bus_space_write_1(iot, ioh, NCR_CFG4, sc->sc_cfg4);
}

#ifdef notyet
int
esp_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct esp_pcmcia_softc *esc = (void *)self;
	int error;

	if ((esc->sc_flags & ESP_PCMCIA_ATTACHED) == 0) {
		/* Nothing to detach. */
		return (0);
	}

	error = ncr53c9x_detach(&esc->sc_ncr53c9x, flags);
	if (error)
		return (error);

	/* Unmap our i/o window and i/o space. */
	pcmcia_io_unmap(esc->sc_pf, esc->sc_io_window);
	pcmcia_io_free(esc->sc_pf, &esc->sc_pcioh);

	return (0);
}
#endif

int
esp_pcmcia_enable(arg, onoff)
	void *arg;
	int onoff;
{
	struct esp_pcmcia_softc *esc = arg;

	if (onoff) {
#ifdef ESP_PCMCIA_POLL
		callout_reset(&esc->sc_poll_ch, 1, esp_pcmcia_poll, esc);
#else
		/* Establish the interrupt handler. */
		esc->sc_ih = pcmcia_intr_establish(esc->sc_pf, IPL_BIO,
		    ncr53c9x_intr, &esc->sc_ncr53c9x,
		    esc->sc_ncr53c9x.sc_dev.dv_xname);
		if (esc->sc_ih == NULL) {
			printf("%s: couldn't establish interrupt handler\n",
			    esc->sc_ncr53c9x.sc_dev.dv_xname);
			return (EIO);
		}
#endif

		/*
		 * If attach is in progress, we know that card power is
		 * enabled and chip will be initialized later.
		 * Otherwise, enable and reset now.
		 */
		if ((esc->sc_flags & ESP_PCMCIA_ATTACHING) == 0) {
			if (pcmcia_function_enable(esc->sc_pf)) {
				printf("%s: couldn't enable PCMCIA function\n",
				    esc->sc_ncr53c9x.sc_dev.dv_xname);
				pcmcia_intr_disestablish(esc->sc_pf,
				    esc->sc_ih);
				return (EIO);
			}

			/* Initialize only chip.  */
			ncr53c9x_init(&esc->sc_ncr53c9x, 0);
		}
	} else {
		pcmcia_function_disable(esc->sc_pf);
#ifdef ESP_PCMCIA_POLL
		callout_stop(&esc->sc_poll_ch);
#else
		pcmcia_intr_disestablish(esc->sc_pf, esc->sc_ih);
#endif
	}

	return (0);
}

#ifdef ESP_PCMCIA_POLL
void
esp_pcmcia_poll(arg)
	void *arg;
{
	struct esp_pcmcia_softc *esc = arg;

	(void) ncr53c9x_intr(&esc->sc_ncr53c9x);
	callout_reset(&esc->sc_poll_ch, 1, esp_pcmcia_poll, esc);
}
#endif

/*
 * Glue functions.
 */
u_char
esp_pcmcia_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;
	u_char v;

	v = bus_space_read_1(esc->sc_pcioh.iot, esc->sc_pcioh.ioh, reg);
	return v;
}

void
esp_pcmcia_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;
	u_char v = val;

	if (reg == NCR_CMD && v == (NCRCMD_TRANS|NCRCMD_DMA))
		v = NCRCMD_TRANS;
	bus_space_write_1(esc->sc_pcioh.iot, esc->sc_pcioh.ioh, reg, v);
}

int
esp_pcmcia_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{

	return NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT;
}

void
esp_pcmcia_dma_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;

	esc->sc_active = 0;
	esc->sc_tc = 0;
}

int
esp_pcmcia_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;
	u_char	*p;
	u_int	espphase, espstat, espintr;
	int	cnt;

	if (esc->sc_active == 0) {
		printf("%s: dma_intr--inactive DMA\n", sc->sc_dev.dv_xname);
		return -1;
	}

	if ((sc->sc_espintr & NCRINTR_BS) == 0) {
		esc->sc_active = 0;
		return 0;
	}

	cnt = *esc->sc_pdmalen;
	if (*esc->sc_pdmalen == 0) {
		printf("%s: data interrupt, but no count left\n",
		    sc->sc_dev.dv_xname);
	}

	p = *esc->sc_dmaaddr;
	espphase = sc->sc_phase;
	espstat = (u_int) sc->sc_espstat;
	espintr = (u_int) sc->sc_espintr;
	do {
		if (esc->sc_datain) {
			*p++ = NCR_READ_REG(sc, NCR_FIFO);
			cnt--;
			if (espphase == DATA_IN_PHASE)
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			else
				esc->sc_active = 0;
	 	} else {
			if (espphase == DATA_OUT_PHASE ||
			    espphase == MESSAGE_OUT_PHASE) {
				NCR_WRITE_REG(sc, NCR_FIFO, *p++);
				cnt--;
				NCR_WRITE_REG(sc, NCR_CMD, NCRCMD_TRANS);
			} else
				esc->sc_active = 0;
		}

		if (esc->sc_active) {
			while (!(NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT));
			espstat = NCR_READ_REG(sc, NCR_STAT);
			espintr = NCR_READ_REG(sc, NCR_INTR);
			espphase = (espintr & NCRINTR_DIS)
				    ? /* Disconnected */ BUSFREE_PHASE
				    : espstat & PHASE_MASK;
		}
	} while (esc->sc_active && espintr);
	sc->sc_phase = espphase;
	sc->sc_espstat = (u_char) espstat;
	sc->sc_espintr = (u_char) espintr;
	*esc->sc_dmaaddr = p;
	*esc->sc_pdmalen = cnt;

	if (*esc->sc_pdmalen == 0)
		esc->sc_tc = NCRSTAT_TC;
	sc->sc_espstat |= esc->sc_tc;
	return 0;
}

int
esp_pcmcia_dma_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;

	esc->sc_dmaaddr = addr;
	esc->sc_pdmalen = len;
	esc->sc_datain = datain;
	esc->sc_dmasize = *dmasize;
	esc->sc_tc = 0;

	return 0;
}

void
esp_pcmcia_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;

	esc->sc_active = 1;
}

void
esp_pcmcia_dma_stop(sc)
	struct ncr53c9x_softc *sc;
{
}

int
esp_pcmcia_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_pcmcia_softc *esc = (struct esp_pcmcia_softc *)sc;

	return (esc->sc_active);
}
