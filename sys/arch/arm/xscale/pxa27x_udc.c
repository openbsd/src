/*	$OpenBSD: pxa27x_udc.c,v 1.30 2015/05/07 01:55:43 jsg Exp $ */

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <arm/xscale/pxa27x_udc.h>

#include "usbf.h"

struct pxaudc_xfer {
	struct usbf_xfer	 xfer;
	u_int16_t		 frmlen;
};

struct pxaudc_pipe {
	struct usbf_pipe	 pipe;
//	LIST_ENTRY(pxaudc_pipe)	 list;
};

void		 pxaudc_enable(struct pxaudc_softc *);
void		 pxaudc_disable(struct pxaudc_softc *);
void		 pxaudc_read_ep0(struct pxaudc_softc *, struct usbf_xfer *);
void		 pxaudc_read_epN(struct pxaudc_softc *sc, int ep);
void		 pxaudc_write_ep0(struct pxaudc_softc *, struct usbf_xfer *);
void		 pxaudc_write(struct pxaudc_softc *, struct usbf_xfer *);
void		 pxaudc_write_epN(struct pxaudc_softc *sc, int ep);

int		 pxaudc_connect_intr(void *);
int		 pxaudc_intr(void *);
void		 pxaudc_intr1(struct pxaudc_softc *);
void		 pxaudc_ep0_intr(struct pxaudc_softc *);
void		 pxaudc_epN_intr(struct pxaudc_softc *sc, int ep, int isr);

usbf_status	 pxaudc_open(struct usbf_pipe *);
void		 pxaudc_softintr(void *);
usbf_status	 pxaudc_allocm(struct usbf_bus *, struct usb_dma *, u_int32_t);
void		 pxaudc_freem(struct usbf_bus *, struct usb_dma *);
struct usbf_xfer *pxaudc_allocx(struct usbf_bus *);
void		 pxaudc_freex(struct usbf_bus *, struct usbf_xfer *);

usbf_status	 pxaudc_ctrl_transfer(struct usbf_xfer *);
usbf_status	 pxaudc_ctrl_start(struct usbf_xfer *);
void		 pxaudc_ctrl_abort(struct usbf_xfer *);
void		 pxaudc_ctrl_done(struct usbf_xfer *);
void		 pxaudc_ctrl_close(struct usbf_pipe *);

usbf_status	 pxaudc_bulk_transfer(struct usbf_xfer *);
usbf_status	 pxaudc_bulk_start(struct usbf_xfer *);
void		 pxaudc_bulk_abort(struct usbf_xfer *);
void		 pxaudc_bulk_done(struct usbf_xfer *);
void		 pxaudc_bulk_close(struct usbf_pipe *);

struct cfdriver pxaudc_cd = {
	NULL, "pxaudc", DV_DULL
};

#if NUSBF > 0

struct usbf_bus_methods pxaudc_bus_methods = {
	pxaudc_open,
	pxaudc_softintr,
	pxaudc_allocm,
	pxaudc_freem,
	pxaudc_allocx,
	pxaudc_freex
};

struct usbf_pipe_methods pxaudc_ctrl_methods = {
	pxaudc_ctrl_transfer,
	pxaudc_ctrl_start,
	pxaudc_ctrl_abort,
	pxaudc_ctrl_done,
	pxaudc_ctrl_close
};

struct usbf_pipe_methods pxaudc_bulk_methods = {
	pxaudc_bulk_transfer,
	pxaudc_bulk_start,
	pxaudc_bulk_abort,
	pxaudc_bulk_done,
	pxaudc_bulk_close
};

#endif /* NUSBF > 0 */

#define DEVNAME(sc)	((sc)->sc_bus.bdev.dv_xname)

#define CSR_READ_4(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define CSR_WRITE_4(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_WRITE_1(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_SET_4(sc, reg, val) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) | (val))
#define CSR_CLR_4(sc, reg, val) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) & ~(val))

#ifndef PXAUDC_DEBUG
#define DPRINTF(l, x)	do {} while (0)
#else
int pxaudcdebug = 0;
#define DPRINTF(l, x)	if ((l) <= pxaudcdebug) printf x; else {}
#endif

int
pxaudc_match(void)
{
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) != CPU_ID_PXA27X)
		return (0);

	return (1);
}

void
pxaudc_attach(struct pxaudc_softc *sc, void *aux)
{
	struct pxaip_attach_args	*pxa = aux;
#if NUSBF > 0
	int i;
#endif

	sc->sc_iot = pxa->pxa_iot;
	if (bus_space_map(sc->sc_iot, PXA2X0_USBDC_BASE, PXA2X0_USBDC_SIZE, 0,
	    &sc->sc_ioh)) {
		printf(": cannot map mem space\n");
		return;
	}
	sc->sc_size = PXA2X0_USBDC_SIZE;

	printf(": USB Device Controller\n");

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	/* Set up GPIO pins and disable the controller. */
	pxaudc_disable(sc);

#if NUSBF > 0
	/* Establish USB device interrupt. */
	sc->sc_ih = pxa2x0_intr_establish(PXA2X0_INT_USB, IPL_USB,
	    pxaudc_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
		return;
	}

	/* Establish device connect interrupt. */
#if 0
	sc->sc_conn_ih = pxa2x0_gpio_intr_establish(PXA_USB_DEVICE_PIN, /* XXX */
	    IST_EDGE_BOTH, IPL_USB, pxaudc_connect_intr, sc, "usbc");
#endif
	sc->sc_conn_ih = pxa2x0_gpio_intr_establish(sc->sc_gpio_detect,
	    IST_EDGE_BOTH, IPL_USB, pxaudc_connect_intr, sc, "usbc");
	if (sc->sc_conn_ih == NULL) {
		printf(": unable to establish connect interrupt\n");
		pxa2x0_intr_disestablish(sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_ioh = 0;
		sc->sc_size = 0;
		return;
	}

	/* Set up the bus struct. */
	sc->sc_bus.methods = &pxaudc_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct pxaudc_pipe);
	sc->sc_bus.ep0_maxp = PXAUDC_EP0MAXP;
	sc->sc_bus.usbrev = USBREV_1_1;
	sc->sc_bus.dmatag = pxa->pxa_dmat;
	sc->sc_npipe = 0;	/* ep0 is always there. */

	sc->sc_ep_map[0] = 0;
	/* 16 == max logical endpoints */
	for (i = 1; i < 16; i++) {
		sc->sc_ep_map[i] = -1;
	}

	/* Attach logical device and function. */
	(void)config_found((struct device *)sc, &sc->sc_bus, NULL);

	/* Enable the controller unless we're now acting as a host. */
	if (!sc->sc_is_host())
		pxaudc_enable(sc);
#endif
}

int
pxaudc_detach(struct pxaudc_softc *sc, int flags)
{
	if (sc->sc_conn_ih != NULL)
		pxa2x0_gpio_intr_disestablish(sc->sc_conn_ih);

	if (sc->sc_ih != NULL)
		pxa2x0_intr_disestablish(sc->sc_ih);

	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}

	return (0);
}

int
pxaudc_activate(struct pxaudc_softc *self, int act)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		pxaudc_disable(sc);
		break;
	case DVACT_RESUME:
		pxaudc_enable(sc);
		break;
	}
	return 0;
}

/*
 * Register manipulation
 */

#if 0
static void
pxaudc_dump_regs(struct pxaudc_softc *sc)
{
	printf("UDCCR\t%b\n", CSR_READ_4(sc, USBDC_UDCCR),
	    USBDC_UDCCR_BITS);
	printf("UDCICR0\t%b\n", CSR_READ_4(sc, USBDC_UDCICR0),
	    USBDC_UDCISR0_BITS);
	printf("UDCICR1\t%b\n", CSR_READ_4(sc, USBDC_UDCICR1),
	    USBDC_UDCISR1_BITS);
	printf("OTGICR\t%b\n", CSR_READ_4(sc, USBDC_UDCOTGICR),
	    USBDC_UDCOTGISR_BITS);
}
#endif

void
pxaudc_enable(struct pxaudc_softc *sc)
{
	int i;

	DPRINTF(10,("pxaudc_enable\n"));

	/* Start the clocks. */
	pxa2x0_clkman_config(CKEN_USBDC, 1);

#if 0
	/* Configure Port 2 for USB device. */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DMPUE |
	    USBDC_UP2OCR_DPPUE | USBDC_UP2OCR_HXOE);
#else
	/* Configure Port 2 for USB device. */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DPPUE | USBDC_UP2OCR_HXOE);
#endif

	CSR_SET_4(sc, USBDC_UDCCR, 0);
	sc->sc_icr0 = 0;
	sc->sc_icr1 = 0;

	for (i = 1; i < PXAUDC_NEP; i++) 
		CSR_WRITE_4(sc, USBDC_UDCECR(i), 0); /* disable endpoints */

	for (i = 1; i < sc->sc_npipe; i++) {
		if (sc->sc_pipe[i] != NULL)  {
			struct usbf_endpoint *ep =
			    sc->sc_pipe[i]->pipe.endpoint;
			u_int32_t cr;
			int dir = usbf_endpoint_dir(ep);
			usb_endpoint_descriptor_t *ed = ep->edesc;

			if (i < 16)
				sc->sc_icr0 |= USBDC_UDCICR0_IE(i);
			else
				sc->sc_icr1 |= USBDC_UDCICR1_IE(i-16);

			cr = USBDC_UDCECR_EE | USBDC_UDCECR_DE;
			cr |= USBDC_UDCECR_ENs(
			    UE_GET_ADDR(ed->bEndpointAddress));
			cr |= USBDC_UDCECR_MPSs(UGETW(ed->wMaxPacketSize));
			cr |= USBDC_UDCECR_ETs(ed->bmAttributes & UE_XFERTYPE);
			if (dir == UE_DIR_IN)
				cr |= USBDC_UDCECR_ED;

			/* XXX - until pipe has cn/in/ain */
			cr |=   USBDC_UDCECR_AISNs(0) | USBDC_UDCECR_INs(0) |
			    USBDC_UDCECR_CNs(1);

			CSR_WRITE_4(sc, USBDC_UDCECR(i), cr);

			/* clear old status */
			CSR_WRITE_4(sc, USBDC_UDCCSR(1), 
			    USBDC_UDCCSR_PC | USBDC_UDCCSR_TRN |
			    USBDC_UDCCSR_SST | USBDC_UDCCSR_FEF);
		}
	}

	CSR_WRITE_4(sc, USBDC_UDCISR0, 0xffffffff); /* clear all */
	CSR_WRITE_4(sc, USBDC_UDCISR1, 0xffffffff); /* clear all */
	CSR_SET_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_ACM);


	/* Enable interrupts for configured endpoints. */
	CSR_WRITE_4(sc, USBDC_UDCICR0, USBDC_UDCICR0_IE(0) |
	    sc->sc_icr0);

	CSR_WRITE_4(sc, USBDC_UDCICR1, USBDC_UDCICR1_IERS |
	    USBDC_UDCICR1_IESU | USBDC_UDCICR1_IERU |
	    USBDC_UDCICR1_IECC | sc->sc_icr1);

	/* Enable the controller. */
	CSR_CLR_4(sc, USBDC_UDCCR, USBDC_UDCCR_EMCE);
	CSR_SET_4(sc, USBDC_UDCCR, USBDC_UDCCR_UDE);

	/* Enable USB client on port 2. */
	pxa2x0_gpio_clear_bit(37); /* USB_P2_8 */
}

void
pxaudc_disable(struct pxaudc_softc *sc)
{
	DPRINTF(10,("pxaudc_disable\n"));

	/* Disable the controller. */
	CSR_CLR_4(sc, USBDC_UDCCR, USBDC_UDCCR_UDE);

	/* Disable all interrupts. */
	CSR_WRITE_4(sc, USBDC_UDCICR0, 0);
	CSR_WRITE_4(sc, USBDC_UDCICR1, 0);
	CSR_WRITE_4(sc, USBDC_UDCOTGICR, 0);

	/* Set Port 2 output to "Non-OTG Host with Differential Port". */
	CSR_WRITE_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_HXS | USBDC_UP2OCR_HXOE);

	/* Set "Host Port 2 Transceiver D­ Pull Down Enable". */
	CSR_SET_4(sc, USBDC_UP2OCR, USBDC_UP2OCR_DMPDE);

	/* Stop the clocks. */
	pxa2x0_clkman_config(CKEN_USBDC, 0);

	/* Enable USB host on port 2. */
	pxa2x0_gpio_set_bit(37); /* USB_P2_8 */
}

#if NUSBF > 0

/*
 * Endpoint FIFO handling
 */

void
pxaudc_read_ep0(struct pxaudc_softc *sc, struct usbf_xfer *xfer)
{
	size_t len;
	u_int8_t *p;

	xfer->actlen = CSR_READ_4(sc, USBDC_UDCBCR(0));
	len = MIN(xfer->actlen, xfer->length);
	p = xfer->buffer;

	while (CSR_READ_4(sc, USBDC_UDCCSR0) & USBDC_UDCCSR0_RNE) {
		u_int32_t v = CSR_READ_4(sc, USBDC_UDCDR(0));

		if (len > 0) {
			if (((unsigned)p & 0x3) == 0)
				*(u_int32_t *)p = v;
			else {
				*(p+0) = v & 0xff;
				*(p+1) = (v >> 8) & 0xff;
				*(p+2) = (v >> 16) & 0xff;
				*(p+3) = (v >> 24) & 0xff;
			}
			p += 4;
			len -= 4;
		}
	}

	CSR_WRITE_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_SA | USBDC_UDCCSR0_OPC);

	xfer->status = USBF_NORMAL_COMPLETION;
	usbf_transfer_complete(xfer);
}

void
pxaudc_read_epN(struct pxaudc_softc *sc, int ep)
{
	size_t len, tlen;
	u_int8_t *p;
	struct pxaudc_pipe *ppipe;
	struct usbf_pipe *pipe = NULL;
	struct usbf_xfer *xfer = NULL;
	int count;
	u_int32_t csr;

	ppipe = sc->sc_pipe[ep];

	if (ppipe == NULL) {
		return;
	}
	pipe = &ppipe->pipe;
again:
	xfer = SIMPLEQ_FIRST(&pipe->queue);

	if (xfer == NULL) {
		printf("pxaudc_read_epN: ep %d, no xfer\n", ep);
		return;
	}

	count = CSR_READ_4(sc, USBDC_UDCBCR(ep));
	tlen = len = MIN(count, xfer->length - xfer->actlen);
	p = xfer->buffer + xfer->actlen;
	csr = CSR_READ_4(sc, USBDC_UDCCSR(ep));

	if ((csr & USBDC_UDCCSR_PC) && count == 0)
	{
#ifdef DEBUG_RX
	        printf("trans1 complete\n");
#endif
		xfer->status = USBF_NORMAL_COMPLETION;
		usbf_transfer_complete(xfer);
		CSR_SET_4(sc, USBDC_UDCCSR(ep), USBDC_UDCCSR_PC);
		return;
	}

#ifdef DEBUG_RX
	printf("reading data from endpoint %x, len %x csr %x",
		ep, count, csr);
#endif

	while (CSR_READ_4(sc, USBDC_UDCCSR(ep)) & USBDC_UDCCSR_BNE) {
		u_int32_t v = CSR_READ_4(sc, USBDC_UDCDR(ep));

		/* double buffering? */
		if (len > 0) {
			if (((unsigned)p & 0x3) == 0)
				*(u_int32_t *)p = v;
			else {
				*(p+0) = v & 0xff;
				*(p+1) = (v >> 8) & 0xff;
				*(p+2) = (v >> 16) & 0xff;
				*(p+3) = (v >> 24) & 0xff;
			}
			p += 4;
			len -= 4;
			xfer->actlen += 4;
		}
		count -= 4;
	}
	CSR_SET_4(sc, USBDC_UDCCSR(ep), USBDC_UDCCSR_PC);


	if (xfer->length == xfer->actlen || (tlen == 0 && xfer->actlen != 0) ||
	    csr & USBDC_UDCCSR_SP) {
#ifdef DEBUG_RX
		printf("trans2 complete\n");
#endif
		xfer->status = USBF_NORMAL_COMPLETION;
		usbf_transfer_complete(xfer);
	}
	csr = CSR_READ_4(sc, USBDC_UDCCSR(ep));
#ifdef DEBUG_RX
	printf("csr now %x len %x\n",
	    csr, CSR_READ_4(sc, USBDC_UDCBCR(ep)));
#endif
	if (csr & USBDC_UDCCSR_PC)
		goto again;
}

void
pxaudc_write_ep0(struct pxaudc_softc *sc, struct usbf_xfer *xfer)
{
	struct pxaudc_xfer *lxfer = (struct pxaudc_xfer *)xfer;
	u_int32_t len;
	u_int8_t *p;

	if (lxfer->frmlen > 0) {
		xfer->actlen += lxfer->frmlen;
		lxfer->frmlen = 0;
	}

	DPRINTF(11,("%s: ep0 ctrl-in, xfer=%p, len=%u, actlen=%u\n",
	    DEVNAME(sc), xfer, xfer->length, xfer->actlen));

	if (xfer->actlen >= xfer->length) {
		sc->sc_ep0state = EP0_SETUP;
		usbf_transfer_complete(xfer);
		return;
	}

	sc->sc_ep0state = EP0_IN;

	p = (u_char *)xfer->buffer + xfer->actlen;
	len = xfer->length - xfer->actlen;
	len = MIN(len, PXAUDC_EP0MAXP);
	lxfer->frmlen = len;

	while (len >= 4) {
		u_int32_t v;

		if (((unsigned)p & 0x3) == 0)
			v = *(u_int32_t *)p;
		else {
			v = *(p+0);
			v |= *(p+1) << 8;
			v |= *(p+2) << 16;
			v |= *(p+3) << 24;
		}

		CSR_WRITE_4(sc, USBDC_UDCDR(0), v);
		len -= 4;
		p += 4;
	}

	while (len > 0) {
		CSR_WRITE_1(sc, USBDC_UDCDR(0), *p);
		len--;
		p++;
	}

	/* (12.6.7) Set IPR only for short packets. */
	if (lxfer->frmlen < PXAUDC_EP0MAXP)
		CSR_SET_4(sc, USBDC_UDCCSR0, USBDC_UDCCSR0_IPR);
}

void
pxaudc_write(struct pxaudc_softc *sc, struct usbf_xfer *xfer)
{
	u_int8_t *p;
	int ep = sc->sc_ep_map[usbf_endpoint_index(xfer->pipe->endpoint)];
	int tlen = 0;
	int maxp = UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize);
	u_int32_t csr, csr_o;

#ifdef DEBUG_TX_PKT
	if (xfer->actlen == 0)
		printf("new packet len %x\n", xfer->length);
#endif

#ifdef DEBUG_TX
	printf("writing data to endpoint %x, xlen %x xact %x\n",
		ep, xfer->length, xfer->actlen);
#endif


	if (xfer->actlen == xfer->length) {
		/*
		 * If the packet size is wMaxPacketSize byte multiple
		 * send a zero packet to indicate termiation.
		 */
		if ((xfer->actlen % maxp) == 0 &&
		    xfer->status != USBF_NORMAL_COMPLETION &&
		    xfer->flags & USBD_FORCE_SHORT_XFER) {
			if (CSR_READ_4(sc, USBDC_UDCCSR(ep))
			    & USBDC_UDCCSR_BNF) {
				CSR_SET_4(sc, USBDC_UDCCSR(ep),
				    USBDC_UDCCSR_SP);
				/*
				 * if we send a zero packet, we are 'done', but
				 * dont to usbf_transfer_complete() just yet
				 * because the short packet will cause another
				 * interrupt.
				 */
				xfer->status = USBF_NORMAL_COMPLETION;
				return;
			} else  {
				printf("fifo full when trying to set short packet\n");
			}
		}
		xfer->status = USBF_NORMAL_COMPLETION;
#ifdef DEBUG_TX_PKT
		printf("packet complete %x\n", xfer->actlen);
#endif
		usbf_transfer_complete(xfer);
		return;
	}

	p = xfer->buffer + xfer->actlen;



	csr_o = 0;
	csr = CSR_READ_4(sc, USBDC_UDCCSR(ep));
	if (csr & USBDC_UDCCSR_PC)
		csr_o |= USBDC_UDCCSR_PC;
	if (csr & USBDC_UDCCSR_TRN)
		csr_o |= USBDC_UDCCSR_TRN;
	if (csr & USBDC_UDCCSR_SST)
		csr_o |= USBDC_UDCCSR_SST;
	if (csr_o != 0)
		CSR_WRITE_4(sc, USBDC_UDCCSR(ep), csr_o);


	while (CSR_READ_4(sc, USBDC_UDCCSR(ep)) & USBDC_UDCCSR_BNF) {
		u_int32_t v;

		if (xfer->length - xfer->actlen < 4) {
			while (xfer->actlen < xfer->length) {
				CSR_WRITE_1(sc, USBDC_UDCDR(ep), *p);
				p++;
				xfer->actlen++;
				tlen++;
			}
			break;
		}
		if (((unsigned)p & 0x3) == 0)
			v = *(u_int32_t *)p;
		else {
			v = *(p+0);
			v |= *(p+1) << 8;
			v |= *(p+2) << 16;
			v |= *(p+3) << 24;
		}
		CSR_WRITE_4(sc, USBDC_UDCDR(ep), v);

		p += 4;
		xfer->actlen += 4;

		tlen += 4;
	}
#ifdef DEBUG_TX
	printf(" wrote tlen %x %x\n", tlen, xfer->actlen);
	if (xfer->actlen == 0) {
		printf("whoa, write_ep called, but no free space\n");
	}
#endif
	if (xfer->actlen == xfer->length) {
		if ((xfer->actlen % maxp) != 0) {
			if (xfer->flags & USBD_FORCE_SHORT_XFER) {
				CSR_SET_4(sc, USBDC_UDCCSR(ep), USBDC_UDCCSR_SP);
#ifdef DEBUG_TX
				printf("setting short packet on %x csr\n", ep,
				    CSR_READ_4(sc, USBDC_UDCCSR(ep)));
#endif
			} else {
				/* fill buffer to maxpacket size??? */
			}
		}
	}
}

/*
 * Interrupt handling
 */

int
pxaudc_connect_intr(void *v)
{
	struct pxaudc_softc *sc = v;

	DPRINTF(10,("pxaudc_connect_intr: connect=%d\n",
	    pxa2x0_gpio_get_bit(sc->sc_gpio_detect)));

	/* XXX only set a flag here */
	if (sc->sc_is_host()) {
#if 0
		printf("%s:switching to host\n", sc->sc_bus.bdev.dv_xname);
#endif
		pxaudc_disable(sc);
	} else {
#if 0
		printf("%s:switching to client\n", sc->sc_bus.bdev.dv_xname);
#endif
		pxaudc_enable(sc);
	}

	/* Claim this interrupt. */
	return 1;
}

int
pxaudc_intr(void *v)
{
	struct pxaudc_softc *sc = v;
	u_int32_t isr0, isr1, otgisr;

	isr0 = CSR_READ_4(sc, USBDC_UDCISR0);
	isr1 = CSR_READ_4(sc, USBDC_UDCISR1);
	otgisr = CSR_READ_4(sc, USBDC_UDCOTGISR);

	DPRINTF(10,("pxaudc_intr: isr0=%b, isr1=%b, otgisr=%b\n",
	    isr0, USBDC_UDCISR0_BITS, isr1, USBDC_UDCISR1_BITS,
	    otgisr, USBDC_UDCOTGISR_BITS));

	if (isr0 || isr1 || otgisr) {
		sc->sc_isr0 |= isr0;
		sc->sc_isr1 |= isr1;
		sc->sc_otgisr |= otgisr;

		//usbf_schedsoftintr(&sc->sc_bus);
		pxaudc_intr1(sc); /* XXX */
	}

	CSR_WRITE_4(sc, USBDC_UDCISR0, isr0);
	CSR_WRITE_4(sc, USBDC_UDCISR1, isr1);
	CSR_WRITE_4(sc, USBDC_UDCOTGISR, otgisr);

	/* Claim this interrupt. */
	return 1;
}
u_int32_t csr1, csr2;

void
pxaudc_intr1(struct pxaudc_softc *sc)
{
	u_int32_t isr0, isr1, otgisr;
	int i;
	//int s;

	//s = splhardusb();
	isr0 = sc->sc_isr0;
	isr1 = sc->sc_isr1;
	otgisr = sc->sc_otgisr;
	sc->sc_isr0 = 0;
	sc->sc_isr1 = 0;
	sc->sc_otgisr = 0;
	//splx(s);

	sc->sc_bus.intr_context++;

	if (isr1 & USBDC_UDCISR1_IRCC) {
		u_int32_t ccr;
                CSR_SET_4(sc, USBDC_UDCCR, USBDC_UDCCR_SMAC);

		/* wait for reconfig to finish (SMAC auto clears) */
		while (CSR_READ_4(sc, USBDC_UDCCR)  & USBDC_UDCCR_SMAC)
			delay(10);

		ccr = CSR_READ_4(sc, USBDC_UDCCR);
		sc->sc_cn = USBDC_UDCCR_ACNr(ccr);
		sc->sc_in = USBDC_UDCCR_AINr(ccr);
		sc->sc_isn = USBDC_UDCCR_AAISNr(ccr);
		goto ret;
	}
#if 0
	printf("pxaudc_intr: isr0=%b, isr1=%b, otgisr=%b\n",
	    isr0, USBDC_UDCISR0_BITS, isr1, USBDC_UDCISR1_BITS,
	    otgisr, USBDC_UDCOTGISR_BITS);
#endif

	/* Handle USB RESET condition. */
	if (isr1 & USBDC_UDCISR1_IRRS) {
		sc->sc_ep0state = EP0_SETUP;
		usbf_host_reset(&sc->sc_bus);
		/* Discard all other interrupts. */
		goto ret;
	}

	/* Service control pipe interrupts. */
	if (isr0 & USBDC_UDCISR0_IR(0))
		pxaudc_ep0_intr(sc);

	for (i = 1; i < 24; i++) {
		if (i < 16) {
			if (USBDC_UDCISR0_IRs(isr0,i))
				pxaudc_epN_intr(sc, i,
				    USBDC_UDCISR0_IRs(isr0,i));
		} else {
			if (USBDC_UDCISR1_IRs(isr1,i))
				pxaudc_epN_intr(sc, i,
				    USBDC_UDCISR1_IRs(isr1,i));
		}
	}

	if (isr1 & USBDC_UDCISR1_IRSU) {
		/* suspend ?? */
	}
	if (isr1 & USBDC_UDCISR1_IRRU) {
		/* resume ?? */
	}

ret:
	sc->sc_bus.intr_context--;
}

void
pxaudc_epN_intr(struct pxaudc_softc *sc, int ep, int isr)
{
	struct pxaudc_pipe *ppipe;
	struct usbf_pipe *pipe;
	int dir;

	/* should not occur before device is configured */
	if (sc->sc_cn == 0)
		return;
	if (isr &  2)
		printf("ep%d: fifo error\n", ep); /* XXX */

	/* faster method of determining direction? */
	ppipe = sc->sc_pipe[ep];

	if (ppipe == NULL)
		return;
	pipe = &ppipe->pipe;
	dir = usbf_endpoint_dir(pipe->endpoint);

	if (dir == UE_DIR_IN) {
		pxaudc_write_epN(sc, ep);
	} else {
		pxaudc_read_epN(sc, ep);
	}

}

void
pxaudc_write_epN(struct pxaudc_softc *sc, int ep)
{
	struct pxaudc_pipe *ppipe;
	struct usbf_pipe *pipe = NULL;
	struct usbf_xfer *xfer = NULL;

	ppipe = sc->sc_pipe[ep];

	if (ppipe == NULL) {
		return;
	}
	pipe = &ppipe->pipe;
	xfer = SIMPLEQ_FIRST(&pipe->queue);
	if (xfer != NULL)
		pxaudc_write(sc, xfer);
}
void
pxaudc_ep0_intr(struct pxaudc_softc *sc)
{
	struct pxaudc_pipe *ppipe;
	struct usbf_pipe *pipe = NULL;
	struct usbf_xfer *xfer = NULL;
	u_int32_t csr0;

	csr0 = CSR_READ_4(sc, USBDC_UDCCSR0);
	DPRINTF(10,("pxaudc_ep0_intr: csr0=%b\n", csr0, USBDC_UDCCSR0_BITS));
	delay (25);

	ppipe = sc->sc_pipe[0];
	if (ppipe != NULL) {
		pipe = &ppipe->pipe;
		xfer = SIMPLEQ_FIRST(&pipe->queue);
	}

	if (sc->sc_ep0state == EP0_SETUP && (csr0 & USBDC_UDCCSR0_OPC)) {
		if (pipe == NULL) {
			DPRINTF(10,("pxaudc_ep0_intr: no control pipe\n"));
			return;
		}

		if (xfer == NULL) {
			DPRINTF(10,("pxaudc_ep0_intr: no xfer\n"));
			return;
		}

		pxaudc_read_ep0(sc, xfer);
	} else if (sc->sc_ep0state == EP0_IN &&
	    (csr0 & USBDC_UDCCSR0_IPR) == 0 && xfer) {
		pxaudc_write_ep0(sc, xfer);
	}
}

/*
 * Bus methods
 */

usbf_status
pxaudc_open(struct usbf_pipe *pipe)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	struct pxaudc_pipe *ppipe = (struct pxaudc_pipe *)pipe;
	int ep_idx;
	int s;

	ep_idx = usbf_endpoint_index(pipe->endpoint);
	if (ep_idx >= PXAUDC_NEP)
		return USBF_BAD_ADDRESS;

	DPRINTF(10,("pxaudc_open\n"));
	s = splhardusb();

	switch (usbf_endpoint_type(pipe->endpoint)) {
	case UE_CONTROL:
		pipe->methods = &pxaudc_ctrl_methods;
		break;

	case UE_BULK:
		pipe->methods = &pxaudc_bulk_methods;
		break;

	case UE_ISOCHRONOUS:
	case UE_INTERRUPT:
	default:
		/* XXX */
		splx(s);
		return USBF_BAD_ADDRESS;
	}
	
	if (ep_idx != 0 && sc->sc_ep_map[ep_idx] != -1) {
		printf("endpoint %d already used by %c",
		    ep_idx, '@'+ sc->sc_ep_map[0]);
		return USBF_BAD_ADDRESS;
	}
	sc->sc_ep_map[ep_idx] = sc->sc_npipe;

	sc->sc_pipe[sc->sc_npipe] = ppipe;
	sc->sc_npipe++;

	splx(s);
	return USBF_NORMAL_COMPLETION;
}

void
pxaudc_softintr(void *v)
{
	struct pxaudc_softc *sc = v;

	pxaudc_intr1(sc);
}

usbf_status
pxaudc_allocm(struct usbf_bus *bus, struct usb_dma *dmap, u_int32_t size)
{
	return usbf_allocmem(bus, size, 0, dmap);
}

void
pxaudc_freem(struct usbf_bus *bus, struct usb_dma *dmap)
{
	usbf_freemem(bus, dmap);
}

struct usbf_xfer *
pxaudc_allocx(struct usbf_bus *bus)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)bus;
	struct usbf_xfer *xfer;

	xfer = SIMPLEQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
	else
		xfer = malloc(sizeof(struct pxaudc_xfer), M_USB, M_NOWAIT);
	if (xfer != NULL)
		bzero(xfer, sizeof(struct pxaudc_xfer));
	return xfer;
}

void
pxaudc_freex(struct usbf_bus *bus, struct usbf_xfer *xfer)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)bus;

	SIMPLEQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

/*
 * Control pipe methods
 */

usbf_status
pxaudc_ctrl_transfer(struct usbf_xfer *xfer)
{
	usbf_status err;

	/* Insert last in queue. */
	err = usbf_insert_transfer(xfer);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBF_IN_PROGRESS),
	 * so start first.
	 */
	return pxaudc_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

usbf_status
pxaudc_ctrl_start(struct usbf_xfer *xfer)
{
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int iswrite = !(xfer->rqflags & URQ_REQUEST);
	int s;

	s = splusb();
	xfer->status = USBF_IN_PROGRESS;
	if (iswrite)
		pxaudc_write_ep0(sc, xfer);
	else {
		/* XXX boring message, this case is normally reached if
		 * XXX the xfer for a device request is being queued. */
		DPRINTF(10,("%s: ep[%x] ctrl-out, xfer=%p, len=%u, "
		    "actlen=%u\n", DEVNAME(sc),
		    usbf_endpoint_address(xfer->pipe->endpoint),
		    xfer, xfer->length,
		    xfer->actlen));
	}
	splx(s);
	return USBF_IN_PROGRESS;
}

/* (also used by bulk pipes) */
void
pxaudc_ctrl_abort(struct usbf_xfer *xfer)
{
	int s;
#ifdef PXAUDC_DEBUG
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int index = usbf_endpoint_index(pipe->endpoint);
	int dir = usbf_endpoint_dir(pipe->endpoint);
	int type = usbf_endpoint_type(pipe->endpoint);
#endif

	DPRINTF(10,("%s: ep%d %s-%s abort, xfer=%p\n", DEVNAME(sc), index,
	    type == UE_CONTROL ? "ctrl" : "bulk", dir == UE_DIR_IN ?
	    "in" : "out", xfer));

	/*
	 * Step 1: Make soft interrupt routine and hardware ignore the xfer.
	 */
	s = splusb();
	xfer->status = USBF_CANCELLED;
	timeout_del(&xfer->timeout_handle);
	splx(s);

	/*
	 * Step 2: Make sure hardware has finished any possible use of the
	 * xfer and the soft interrupt routine has run.
	 */
	s = splusb();
	/* XXX this does not seem right, what if there
	 * XXX are two xfers in the FIFO and we only want to
	 * XXX ignore one? */
#ifdef notyet
	pxaudc_flush(sc, usbf_endpoint_address(pipe->endpoint));
#endif
	/* XXX we're not doing DMA and the soft interrupt routine does not
	   XXX need to clean up anything. */
	splx(s);

	/*
	 * Step 3: Execute callback.
	 */
	s = splusb();
	usbf_transfer_complete(xfer);
	splx(s);
}

void
pxaudc_ctrl_done(struct usbf_xfer *xfer)
{
}

void
pxaudc_ctrl_close(struct usbf_pipe *pipe)
{
	/* XXX */
}

/*
 * Bulk pipe methods
 */

usbf_status
pxaudc_bulk_transfer(struct usbf_xfer *xfer)
{
	usbf_status err;

	/* Insert last in queue. */
	err = usbf_insert_transfer(xfer);
	if (err)
		return err;

	/*
	 * Pipe isn't running (otherwise err would be USBF_IN_PROGRESS),
	 * so start first.
	 */
	return pxaudc_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

usbf_status
pxaudc_bulk_start(struct usbf_xfer *xfer)
{
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;
	int iswrite = (usbf_endpoint_dir(pipe->endpoint) == UE_DIR_IN);
	int s;

	DPRINTF(0,("%s: ep%d bulk-%s start, xfer=%p, len=%u\n", DEVNAME(sc),
	    usbf_endpoint_index(pipe->endpoint), iswrite ? "in" : "out",
	    xfer, xfer->length));

	s = splusb();
	xfer->status = USBF_IN_PROGRESS;
	if (iswrite)
		pxaudc_write(sc, xfer);
	else {
		/* enable interrupt */
	}
	splx(s);
	return USBF_IN_PROGRESS;
}

void
pxaudc_bulk_abort(struct usbf_xfer *xfer)
{
	pxaudc_ctrl_abort(xfer);
}

void
pxaudc_bulk_done(struct usbf_xfer *xfer)
{
#if 0
	int ep = usbf_endpoint_address(xfer->pipe->endpoint);
	struct usbf_pipe *pipe = xfer->pipe;
	struct pxaudc_softc *sc = (struct pxaudc_softc *)pipe->device->bus;

#endif
}

void
pxaudc_bulk_close(struct usbf_pipe *pipe)
{
	/* XXX */
}

#endif /* NUSBF > 0 */
