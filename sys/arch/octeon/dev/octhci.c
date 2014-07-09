/*	$OpenBSD: octhci.c,v 1.1 2014/07/09 23:03:22 pirofti Exp $	*/

/*
 * Copyright (c) 2014 Paul Irofti <pirofti@openbsd.org>
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
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/octeonreg.h>
#include <machine/octeonvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/octhcireg.h>

#ifdef OCTHCI_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct octhci_softc {
	struct device sc_dev;
	struct usbd_bus sc_bus;		/* base device */
	struct device *sc_child;	/* /dev/usb# device */

	void *sc_ih;			/* interrupt handler */

	bus_space_tag_t sc_bust;	/* iobus space */
	bus_space_handle_t sc_regn;	/* usbn register space */
	bus_space_handle_t sc_regc;	/* usbc register space */
        bus_space_handle_t sc_dma_reg;	/* dma register space */
};

int octhci_match(struct device *, void *, void *);
void octhci_attach(struct device *, struct device *, void *);

void octhci_init(struct octhci_softc *);
void octhci_init_core(struct octhci_softc *);
void octhci_init_host(struct octhci_softc *);
int octhci_intr(void *);

const struct cfattach octhci_ca = {
	sizeof(struct octhci_softc), octhci_match, octhci_attach,
};

struct cfdriver octhci_cd = {
	NULL, "octhci", DV_DULL
};

inline void octhci_regn_set(struct octhci_softc *, bus_size_t, uint64_t);
inline void octhci_regn_clear(struct octhci_softc *, bus_size_t, uint64_t);

inline void octhci_regc_set(struct octhci_softc *, bus_size_t, uint32_t);
inline void octhci_regc_clear(struct octhci_softc *, bus_size_t, uint32_t);
inline uint32_t octhci_regc_read(struct octhci_softc *, bus_size_t);
inline void octhci_regc_write(struct octhci_softc *, bus_size_t, uint32_t);

struct octhci_soft_td {
	struct octhci_soft_td *next;
	struct octhci_soft_td *prev;
	struct octhci_soft_td *std;
	octhci_physaddr_t physaddr;
	struct usb_dma dma;             /* TD's DMA infos */
	int offs;                       /* TD's offset in struct usb_dma */
	int islot;
};

struct octhci_pipe {
	struct usbd_pipe pipe;

	struct octhci_soft_qh *sqh;
	union {
		struct octhci_soft_td *td;
	} tail;
	union {
		/* Control pipe */
		struct {
			struct usb_dma reqdma;
			u_int length;
		} ctl;
		/* Interrupt pipe */
		struct {
			u_int length;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
		} bulk;
		/* Iso pipe */
		struct {
			u_int next_frame;
			u_int cur_xfers;
		} isoc;
	} u;
};

usbd_status octhci_open(struct usbd_pipe *pipe);
void octhci_softintr(void *);
void octhci_poll(struct usbd_bus *);
struct usbd_xfer * octhci_allocx(struct usbd_bus *);
void octhci_freex(struct usbd_bus *, struct usbd_xfer *);

struct usbd_bus_methods octhci_bus_methods = {
	octhci_open,
	octhci_softintr,
	octhci_poll,
	octhci_allocx,
	octhci_freex,
};

#define OCTHCI_INTR_ENDPT 1
usbd_status octhci_root_ctrl_transfer(struct usbd_xfer *xfer);
usbd_status octhci_root_ctrl_start(struct usbd_xfer *xfer);
void octhci_root_ctrl_abort(struct usbd_xfer *xfer);
void octhci_root_ctrl_close(struct usbd_pipe *pipe);
void octhci_root_ctrl_cleartoggle(struct usbd_pipe *pipe);
void octhci_root_ctrl_done(struct usbd_xfer *xfer);

struct usbd_pipe_methods octhci_root_ctrl_methods = {
	octhci_root_ctrl_transfer,
	octhci_root_ctrl_start,
	octhci_root_ctrl_abort,
	octhci_root_ctrl_close,
	octhci_root_ctrl_cleartoggle,
	octhci_root_ctrl_done,
};


usbd_status octhci_root_intr_transfer(struct usbd_xfer *xfer);
usbd_status octhci_root_intr_start(struct usbd_xfer *xfer);
void octhci_root_intr_abort(struct usbd_xfer *xfer);
void octhci_root_intr_close(struct usbd_pipe *pipe);
void octhci_root_intr_cleartoggle(struct usbd_pipe *pipe);
void octhci_root_intr_done(struct usbd_xfer *xfer);

struct usbd_pipe_methods octhci_root_intr_methods = {
	octhci_root_intr_transfer,
	octhci_root_intr_start,
	octhci_root_intr_abort,
	octhci_root_intr_close,
	octhci_root_intr_cleartoggle,
	octhci_root_intr_done,
};


int
octhci_match(struct device *parent, void *match, void *aux)
{
	struct iobus_attach_args *aa = aux;
	struct cfdata *cf = match;

	/* XXX: check for board type */
	if (aa->aa_name == NULL ||
	    strcmp(aa->aa_name, cf->cf_driver->cd_name) != 0)
		return (0);

	return (1);
}

void
octhci_attach(struct device *parent, struct device *self, void *aux)
{
	struct octhci_softc *sc = (struct octhci_softc *)self;
	struct iobus_attach_args *aa = aux;

	int rc;

	sc->sc_bust = aa->aa_bust;
	rc = bus_space_map(aa->aa_bust, USBN_BASE, USBN_SIZE,
	    0, &sc->sc_regn);
	if (rc != 0)
		panic(": can't map registers");

	/* dma registers */
	rc = bus_space_map(aa->aa_bust, USBN_2_BASE, USBN_2_SIZE,
	    0, &sc->sc_dma_reg);
	if (rc != 0)
		panic(": can't map dma registers");

	/* control registers */
	rc = bus_space_map(aa->aa_bust, USBC_BASE, USBC_SIZE,
	    0, &sc->sc_regc);
	if (rc != 0)
		panic(": can't map control registers");

	/* 
	 * XXX: assume only one USB port is available.
	 * Should write a get_nports routine to get the actual number based
	 * on the model.
	 */

	sc->sc_bus.usbrev = USBREV_2_0;

	octhci_init(sc);
	octhci_init_core(sc);
	octhci_init_host(sc);
#if 0
	sc->sc_ih = octeon_intr_establish(CIU_INT_USB, IPL_USB, octhci_intr,
	    (void *)sc, sc->sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL)
		panic(": can't interrupt establish failed\n");
#endif

	/*
	 * No usb methods yet.
	 * sc->sc_bus.usbrev = USBREV_2_0;
	 */
	sc->sc_bus.methods = &octhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct octhci_pipe);
	sc->sc_bus.dmatag = aa->aa_dmat;
	sc->sc_child = config_found((void *)sc, &sc->sc_bus, usbctlprint);
}

void
octhci_init(struct octhci_softc *sc)
{
	uint64_t clk;

	/*
	 * Clock setup.
	 */
	clk = bus_space_read_8(sc->sc_bust, sc->sc_regn, USBN_CLK_CTL_OFFSET);
	clk |= USBN_CLK_CTL_POR;
	clk &= ~(USBN_CLK_CTL_HRST | USBN_CLK_CTL_PRST | USBN_CLK_CTL_HCLK_RST |
	    USBN_CLK_CTL_ENABLE | USBN_CLK_CTL_P_C_SEL | USBN_CLK_CTL_P_RTYPE);
	clk |= SET_USBN_CLK_CTL_DIVIDE(0x4ULL)
	    | SET_USBN_CLK_CTL_DIVIDE2(0x0ULL);

	bus_space_write_8(sc->sc_bust, sc->sc_regn, USBN_CLK_CTL_OFFSET, clk);
	bus_space_read_8(sc->sc_bust, sc->sc_regn, USBN_CLK_CTL_OFFSET);

	/* 
	 * Reset HCLK and wait for it to stabilize.
	 */
	octhci_regn_set(sc, USBN_CLK_CTL_OFFSET, USBN_CLK_CTL_HCLK_RST);
	delay(64);

	octhci_regn_clear(sc, USBN_CLK_CTL_OFFSET, USBN_CLK_CTL_POR);

	/*
	 * Wait for the PHY clock to start.
	 */
	delay(1000);

	octhci_regn_set(sc, USBN_USBP_CTL_STATUS_OFFSET,
	    USBN_USBP_CTL_STATUS_ATE_RESET);
	delay(10);

	octhci_regn_clear(sc, USBN_USBP_CTL_STATUS_OFFSET,
			USBN_USBP_CTL_STATUS_ATE_RESET);
	octhci_regn_set(sc, USBN_CLK_CTL_OFFSET, USBN_CLK_CTL_PRST);

	/*
	 * Select host mode.
	 */
	octhci_regn_clear(sc, USBN_USBP_CTL_STATUS_OFFSET,
	    USBN_USBP_CTL_STATUS_HST_MODE);
	delay(1);

	octhci_regn_set(sc, USBN_CLK_CTL_OFFSET, USBN_CLK_CTL_HRST);

	/*
	 * Enable clock.
	 */
	octhci_regn_set(sc, USBN_CLK_CTL_OFFSET, USBN_CLK_CTL_ENABLE);
	delay(1);
}

void
octhci_init_core(struct octhci_softc *sc)
{
	uint32_t value, version, minor, major;
	uint8_t chan;

	value = octhci_regc_read(sc, USBC_GSNPSID_OFFSET);
	/* 0x4F54XXXA */
	minor = (value >> 4) & 0xf;
	major = ((value >> 8) & 0xf) / 2;
	version = (value >> 12) & 0xf;
	printf(" core version %d pass %d.%d\n", version, major, minor);

	value = 0;
	value |= (USBC_GAHBCFG_DMAEN | USBC_GAHBCFG_NPTXFEMPLVL |
	    USBC_GAHBCFG_PTXFEMPLVL | USBC_GAHBCFG_GLBLINTRMSK);
	octhci_regc_write(sc, USBC_GAHBCFG_OFFSET, value);

	/* XXX: CN5XXX: usb->idle_hardware_channels = 0xf7 */

	value = octhci_regc_read(sc, USBC_GUSBCFG_OFFSET);
	value &= ~USBC_GUSBCFG_TOUTCAL;
	value &= ~USBC_GUSBCFG_DDRSEL;
	value &= ~USBC_GUSBCFG_USBTRDTIM;
	value |= (5U << USBC_GUSBCFG_USBTRDTIM_OFFSET);
	value &= ~USBC_GUSBCFG_PHYLPWRCLKSEL;
	octhci_regc_write(sc, USBC_GUSBCFG_OFFSET, value);

	value = octhci_regc_read(sc, USBC_GINTMSK_OFFSET);
	value |= USBC_GINTMSK_OTGINTMSK | USBC_GINTMSK_MODEMISMSK |
	    USBC_GINTMSK_HCHINTMSK;
	value &= ~USBC_GINTMSK_SOFMSK;
	octhci_regc_write(sc, USBC_GINTMSK_OFFSET, value);

	for (chan = 0; chan < 8; chan++) {
		octhci_regc_write(sc, USBC_HCINTMSK0_OFFSET + chan * 32, 0);
	}
}

void
octhci_init_host(struct octhci_softc *sc)
{
	uint32_t value;

	octhci_regc_set(sc, USBC_GINTMSK_OFFSET, USBC_GINTSTS_PRTINT);
	octhci_regc_set(sc, USBC_GINTMSK_OFFSET, USBC_GINTSTS_DISCONNINT);

	/* Clear USBC_HCFG_FSLSSUPP and USBC_HCFG_FSLSPCLKSEL */
	value = octhci_regc_read(sc, USBC_HCFG_OFFSET);
	value &= USBC_HCFG_XXX_31_3;
	octhci_regc_write(sc, USBC_HCFG_OFFSET, value);

	octhci_regc_set(sc, USBC_HPRT_OFFSET, USBC_HPRT_PRTPWR);
}

int
octhci_intr(void *arg)
{
	struct octhci_softc *sc = (struct octhci_softc *)arg;
	uint32_t intsts;

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;

	intsts = octhci_regc_read(sc, USBC_GINTSTS_OFFSET);
	intsts &= octhci_regc_read(sc, USBC_GINTMSK_OFFSET) |
	    USBC_GINTSTS_CURMOD;
	intsts |= octhci_regc_read(sc, USBC_GINTSTS_OFFSET);
	octhci_regc_write(sc, USBC_GINTSTS_OFFSET, intsts);

	if (intsts & USBC_GINTSTS_RXFLVL)
		/* Failed assumption: no DMA */
		panic("octhci_intr: Packets pending to be read from RxFIFO\n");
	if ((intsts & USBC_GINTSTS_PTXFEMP) || (intsts & USBC_GINTSTS_NPTXFEMP))
		/* Failed assumption: no DMA */
		panic("octhci_intr: Packets pending to be written on TxFIFO\n");
	if ((intsts & USBC_GINTSTS_DISCONNINT) ||
	    (intsts & USBC_GINTSTS_PRTINT)) {
		/* Device disconnected */
		uint32_t hprt;

		/* XXX: callback */

		hprt = octhci_regc_read(sc, USBC_HPRT_OFFSET);
		hprt &= ~(USBC_HPRT_PRTENA);
		octhci_regc_write(sc, USBC_HPRT_OFFSET, hprt);
	}
	if (intsts & USBC_GINTSTS_HCHINT) {
		/* Host Channel Interrupt */
		uint32_t haint;
		int chan;

		/* XXX: Assume single USB port */
		for (haint = octhci_regc_read(sc, USBC_HAINT_OFFSET);
		    haint != 0; haint ^= (1 << chan)) {
			chan = ffs32(haint) - 1;
			/* XXX: implement octhci_poll_chan(sc, chan); */
		}
	}

	sc->sc_bus.intr_context--;
	return 1;
}

inline void
octhci_regn_set(struct octhci_softc *sc, bus_size_t offset, uint64_t bits)
{
	uint64_t value;

	value = bus_space_read_8(sc->sc_bust, sc->sc_regn, offset);
	value |= bits;

	bus_space_write_8(sc->sc_bust, sc->sc_regn, offset, value);
	/* guarantee completion of the store operation on RSL registers*/
	bus_space_read_8(sc->sc_bust, sc->sc_regn, offset);
}

inline void
octhci_regn_clear(struct octhci_softc *sc, bus_size_t offset, uint64_t bits)
{
	uint64_t value;

	value = bus_space_read_8(sc->sc_bust, sc->sc_regn, offset);
	value &= ~bits;

	bus_space_write_8(sc->sc_bust, sc->sc_regn, offset, value);
	/* guarantee completion of the store operation on RSL registers*/
	bus_space_read_8(sc->sc_bust, sc->sc_regn, offset);
}

inline void
octhci_regc_set(struct octhci_softc *sc, bus_size_t offset, uint32_t bits)
{
	uint32_t value;

	value = octhci_regc_read(sc, offset);
	value |= bits;
	octhci_regc_write(sc, offset, value);
}

inline void
octhci_regc_clear(struct octhci_softc *sc, bus_size_t offset, uint32_t bits)
{
	uint32_t value;

	value = octhci_regc_read(sc, offset);
	value &= ~bits;
	octhci_regc_write(sc, offset, value);
}

/* XXX: fix the endianess in the include and ditch these methods */
inline void
octhci_regc_write(struct octhci_softc *sc, bus_size_t offset, uint32_t value)
{
	/* 
	 * In the Cavium document, CSR addresses are written in little-endian
	 * format. so the address should be swapped on the core running in
	 * big-endian.
	 */
	bus_space_write_4(sc->sc_bust, sc->sc_regc, (offset^4), value);
}

inline uint32_t
octhci_regc_read(struct octhci_softc *sc, bus_size_t offset)
{
	/* 
	 * In the Cavium document, CSR addresses are written in little-endian
	 * format. so the address should be swapped on the core running in
	 * big-endian.
	 */
	return bus_space_read_4(sc->sc_bust, sc->sc_regc, (offset^4));
}


/*
 * USBD Bus Methods.
 */

usbd_status
octhci_open(struct usbd_pipe *pipe)
{
	struct octhci_softc *sc = (struct octhci_softc *)pipe->device->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;

	if (sc->sc_bus.dying)
		return USBD_IOERROR;

	/* Root Hub */
	if (pipe->device->depth == 0) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			printf("Root Hub Control\n");
			pipe->methods = &octhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | OCTHCI_INTR_ENDPT:
			printf("Root Hub Interrupt\n");
			pipe->methods = &octhci_root_intr_methods;
			break;
		default:
			pipe->methods = NULL;
			return USBD_INVAL;
		}
		return USBD_NORMAL_COMPLETION;
	}

	/* XXX: not supported yet */
	return USBD_INVAL;
}

void
octhci_softintr(void *aux)
{
}

void
octhci_poll(struct usbd_bus *bus)
{
}

struct usbd_xfer *
octhci_allocx(struct usbd_bus *bus)
{
	return NULL;
}

void
octhci_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
}

/*
 * USBD Root Control Pipe Methods.
 */

usbd_status
octhci_root_ctrl_transfer(struct usbd_xfer *xfer)
{
	return USBD_NORMAL_COMPLETION;
}

usbd_status
octhci_root_ctrl_start(struct usbd_xfer *xfer)
{
	return USBD_NORMAL_COMPLETION;
}

void
octhci_root_ctrl_abort(struct usbd_xfer *xfer)
{
}

void
octhci_root_ctrl_close(struct usbd_pipe *pipe)
{
}

void
octhci_root_ctrl_cleartoggle(struct usbd_pipe *pipe)
{
}

void
octhci_root_ctrl_done(struct usbd_xfer *xfer)
{
}

/*
 * USBD Root Interrupt Pipe Methods.
 */

usbd_status
octhci_root_intr_transfer(struct usbd_xfer *xfer)
{
	return USBD_NORMAL_COMPLETION;
}

usbd_status
octhci_root_intr_start(struct usbd_xfer *xfer)
{
	return USBD_NORMAL_COMPLETION;
}

void
octhci_root_intr_abort(struct usbd_xfer *xfer)
{
}

void
octhci_root_intr_close(struct usbd_pipe *pipe)
{
}

void
octhci_root_intr_cleartoggle(struct usbd_pipe *pipe)
{
}

void
octhci_root_intr_done(struct usbd_xfer *xfer)
{
}

