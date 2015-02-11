/*	$OpenBSD: octhci.c,v 1.17 2015/02/11 01:40:57 uebayasi Exp $	*/

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
#include <sys/malloc.h>
#include <sys/pool.h>

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
#define DPRINTF(x)	do { if (octhcidebug) printf x; } while(0)
#define DPRINTFN(n,x)	do { if (octhcidebug>(n)) printf x; } while (0)
int octhcidebug = 3;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define DEVNAME(sc)		((sc)->sc_bus.bdev.dv_xname)

struct octhci_softc {
	struct usbd_bus sc_bus;		/* base device */

	void *sc_ih;			/* interrupt handler */

	bus_space_tag_t sc_bust;	/* iobus space */
	bus_space_handle_t sc_regn;	/* usbn register space */
	bus_space_handle_t sc_regc;	/* usbc register space */
        bus_space_handle_t sc_dma_reg;	/* dma register space */

	int sc_noport;			/* Maximum number of ports */

	u_int8_t sc_conf;		/* Device configuration */
	struct usbd_xfer *sc_intrxfer;	/* Root HUB interrupt xfer */

	char sc_vendor[16];		/* Vendor string for root hub */
	int sc_id_vendor;		/* Vendor ID for root hub */

	int sc_port_connect;		/* Device connected to port */
	int sc_port_change;		/* Connect status changed */
	int sc_port_reset;		/* Port reset finished */
};

struct pool *octhcixfer;

struct octhci_xfer {
	struct usbd_xfer	 xfer;
};

int octhci_match(struct device *, void *, void *);
void octhci_attach(struct device *, struct device *, void *);

int octhci_init(struct octhci_softc *);
void octhci_init_core(struct octhci_softc *);
void octhci_init_host(struct octhci_softc *);
int octhci_intr(void *);

int octhci_intr1(struct octhci_softc *);
int octhci_intr_host_port(struct octhci_softc *);
int octhci_intr_host_chan(struct octhci_softc *);
int octhci_intr_host_chan_n(struct octhci_softc *, int);

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

usbd_status octhci_open(struct usbd_pipe *pipe);
void octhci_softintr(void *);
void octhci_poll(struct usbd_bus *);
struct usbd_xfer * octhci_allocx(struct usbd_bus *);
void octhci_freex(struct usbd_bus *, struct usbd_xfer *);

struct usbd_bus_methods octhci_bus_methods = {
	.open_pipe = octhci_open,
	.soft_intr = octhci_softintr,
	.do_poll = octhci_poll,
	.allocx = octhci_allocx,
	.freex = octhci_freex,
};

#define OCTHCI_INTR_ENDPT 1
usbd_status octhci_root_ctrl_transfer(struct usbd_xfer *xfer);
usbd_status octhci_root_ctrl_start(struct usbd_xfer *xfer);
void octhci_root_ctrl_abort(struct usbd_xfer *xfer);
void octhci_root_ctrl_close(struct usbd_pipe *pipe);
void octhci_root_ctrl_done(struct usbd_xfer *xfer);

struct usbd_pipe_methods octhci_root_ctrl_methods = {
	.transfer = octhci_root_ctrl_transfer,
	.start = octhci_root_ctrl_start,
	.abort = octhci_root_ctrl_abort,
	.close = octhci_root_ctrl_close,
	.done = octhci_root_ctrl_done,
};


usbd_status octhci_root_intr_transfer(struct usbd_xfer *xfer);
usbd_status octhci_root_intr_start(struct usbd_xfer *xfer);
void octhci_root_intr_abort(struct usbd_xfer *xfer);
void octhci_root_intr_close(struct usbd_pipe *pipe);
void octhci_root_intr_done(struct usbd_xfer *xfer);

struct usbd_pipe_methods octhci_root_intr_methods = {
	.transfer = octhci_root_intr_transfer,
	.start = octhci_root_intr_start,
	.abort = octhci_root_intr_abort,
	.close = octhci_root_intr_close,
	.done = octhci_root_intr_done,
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
	sc->sc_noport = 1;

	sc->sc_bus.usbrev = USBREV_2_0;

	if (octhci_init(sc))
		return;

	octhci_init_core(sc);
	octhci_init_host(sc);

	sc->sc_ih = octeon_intr_establish(CIU_INT_USB, IPL_USB, octhci_intr,
	    (void *)sc, sc->sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL)
		panic(": interrupt establish failed");

	sc->sc_bus.methods = &octhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct usbd_pipe);
	sc->sc_bus.dmatag = aa->aa_dmat;

	config_found((void *)sc, &sc->sc_bus, usbctlprint);
}

int
octhci_init(struct octhci_softc *sc)
{
	uint64_t clk;

	if (octhcixfer == NULL) {
		octhcixfer = malloc(sizeof(struct pool), M_DEVBUF, M_NOWAIT);
		if (octhcixfer == NULL) {
			printf("%s: unable to allocate pool descriptor\n",
			    DEVNAME(sc));
			return (ENOMEM);
		}
		pool_init(octhcixfer, sizeof(struct octhci_xfer), 0, 0, 0,
		    "octhcixfer", NULL);
	}

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

	return (0);
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
	printf(": core version %d pass %d.%d\n", version, major, minor);

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
octhci_intr(void *v)
{
	struct octhci_softc *sc = v;

	if (sc == NULL || sc->sc_bus.dying) {
		DPRINTFN(16, ("octhci_intr: dying\n"));
		return (0);
	}

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
		DPRINTFN(16, ("octhci_intr: ignore interrupt while polling\n"));
		return (0);
	}

	return (octhci_intr1(sc));
}

int
octhci_intr1(struct octhci_softc *sc)
{
	uint32_t intsts, intmsk;

	intsts = octhci_regc_read(sc, USBC_GINTSTS_OFFSET);
	intmsk = octhci_regc_read(sc, USBC_GINTMSK_OFFSET);

	if ((intsts & intmsk) == 0) {
		DPRINTFN(16, ("%s: masked interrupt\n", DEVNAME(sc)));
		return (0);
	}

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;

	octhci_regc_write(sc, USBC_GINTSTS_OFFSET, intsts); /* Acknowledge */

	if (intsts & USBC_GINTSTS_WKUPINT) {
		DPRINTFN(16, ("%s: wake-up interrupt\n", DEVNAME(sc)));
		octhci_regc_set(sc, USBC_GINTSTS_OFFSET,
		    USBC_GINTSTS_WKUPINT);
	}
	if (intsts & USBC_GINTSTS_SESSREQINT) {
		DPRINTFN(16, ("%s: session request interrupt\n", DEVNAME(sc)));
		octhci_regc_set(sc, USBC_GINTSTS_OFFSET,
		    USBC_GINTSTS_SESSREQINT);
	}
	if (intsts & USBC_GINTSTS_DISCONNINT) {
		DPRINTFN(16, ("%s: disco interrupt\n", DEVNAME(sc)));
		sc->sc_port_connect = 0;
		octhci_regc_set(sc, USBC_GINTSTS_OFFSET,
		    USBC_GINTSTS_DISCONNINT);
	}
	if (intsts & USBC_GINTSTS_CONIDSTSCHNG) {
		DPRINTFN(16, ("%s: connector ID interrupt\n", DEVNAME(sc)));
		octhci_regc_set(sc, USBC_GINTSTS_OFFSET,
		    USBC_GINTSTS_CONIDSTSCHNG);
	}
	if (intsts & USBC_GINTSTS_HCHINT) {
		DPRINTFN(16, ("%s: host channel interrupt\n", DEVNAME(sc)));
		octhci_intr_host_chan(sc);
	}
	if (intsts & USBC_GINTSTS_PRTINT) {
		DPRINTFN(16, ("%s: host port interrupt\n", DEVNAME(sc)));
		octhci_intr_host_port(sc);
	}
	if (intsts & USBC_GINTSTS_MODEMIS) {
		DPRINTFN(16, ("%s: mode missmatch\n", DEVNAME(sc)));
	}
	if (intsts & USBC_GINTSTS_OTGINT) {
		DPRINTFN(16, ("%s: OTG interrupt\n", DEVNAME(sc)));
	}

	usb_schedsoftintr(&sc->sc_bus);

	sc->sc_bus.intr_context--;
	return (1);
}

#define OCTHCI_PS_CLEAR \
    (USBC_HPRT_PRTENCHNG | USBC_HPRT_PRTOVRCURRCHNG | USBC_HPRT_PRTCONNDET)
int
octhci_intr_host_port(struct octhci_softc *sc)
{
	struct usbd_xfer *xfer = sc->sc_intrxfer;
	u_char *p;
	int i, m;
	uint32_t hprt = octhci_regc_read(sc, USBC_HPRT_OFFSET);

	if (xfer == NULL)
		goto ack;

	p = KERNADDR(&xfer->dmabuf, 0);
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		/* Pick out CHANGE bits from the status reg. */
		if (hprt & OCTHCI_PS_CLEAR)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("%s: change=0x%02x\n", __func__, *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);

ack:
	octhci_regc_write(sc, USBC_HPRT_OFFSET, hprt);	/* Acknowledge */

	return (USBD_NORMAL_COMPLETION);
}

int
octhci_intr_host_chan(struct octhci_softc *sc)
{
	int chan = 0;
	uint32_t haint;

	haint = octhci_regc_read(sc, USBC_HAINT_OFFSET);
	haint &= octhci_regc_read(sc, USBC_HAINTMSK_OFFSET);

	DPRINTFN(16, ("%s: haint %X\n", DEVNAME(sc), haint));

	for (; haint != 0; haint ^= (1 << chan)) {
		chan = ffs32(haint) - 1;
		octhci_intr_host_chan_n(sc, chan);
	}

	return (USBD_NORMAL_COMPLETION);
}

int
octhci_intr_host_chan_n(struct octhci_softc *sc, int chan)
{
	uint32_t hcintn;

	hcintn = octhci_regc_read(sc, USBC_HCINT0_OFFSET + chan * 0x20);
	hcintn &= octhci_regc_read(sc, USBC_HCINTMSK0_OFFSET + chan * 0x20);

	DPRINTFN(16, ("%s: chan %d hcintn %d\n", DEVNAME(sc), chan, hcintn));

	/* Acknowledge */
	octhci_regc_write(sc, USBC_HCINT0_OFFSET + chan * 0x20, hcintn);

	return (USBD_NORMAL_COMPLETION);
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
		return (USBD_IOERROR);

	/* Root Hub */
	if (pipe->device->depth == 0) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			DPRINTF(("Root Hub Control\n"));
			pipe->methods = &octhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | OCTHCI_INTR_ENDPT:
			DPRINTF(("Root Hub Interrupt\n"));
			pipe->methods = &octhci_root_intr_methods;
			break;
		default:
			DPRINTF(("%s: bad bEndpointAddress 0x%02x\n", __func__,
			    ed->bEndpointAddress));
			pipe->methods = NULL;
			return (USBD_INVAL);
		}
		return (USBD_NORMAL_COMPLETION);
	}

	DPRINTFN(16, ("%s: Not a Root Hub\n", DEVNAME(sc)));
	/* XXX: not supported yet */
	return (USBD_INVAL);
}

void
octhci_softintr(void *aux)
{
}

void
octhci_poll(struct usbd_bus *bus)
{
	struct octhci_softc *sc = (struct octhci_softc *)bus;

	if (octhci_regc_read(sc, USBC_GINTSTS_OFFSET))
		octhci_intr1(sc);
}

struct usbd_xfer *
octhci_allocx(struct usbd_bus *bus)
{
	struct octhci_xfer *xx;

	xx = pool_get(octhcixfer, PR_NOWAIT | PR_ZERO);
#ifdef DIAGNOSTIC
	if (xx != NULL)
		xx->xfer.busy_free = XFER_ONQU;
#endif
	return ((struct usbd_xfer *)xx);
}

void
octhci_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
	struct octhci_xfer *xx = (struct octhci_xfer*)xfer;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_ONQU) {
		printf("%s: xfer=%p not busy, 0x%08x\n", __func__, xfer,
		    xfer->busy_free);
		return;
	}
#endif
	pool_put(octhcixfer, xx);
}


/* Root hub descriptors. */
usb_device_descriptor_t octhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indexes */
	1			/* # of configurations */
};

const usb_config_descriptor_t octhci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_SELF_POWERED,
	0                      /* max power */
};

const usb_interface_descriptor_t octhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,	/* XXX */
	0
};

const usb_endpoint_descriptor_t octhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | OCTHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},                 /* max packet */
	12
};

const usb_hub_descriptor_t octhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

/*
 * USBD Root Control Pipe Methods.
 */

usbd_status
octhci_root_ctrl_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (octhci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
octhci_root_ctrl_start(struct usbd_xfer *xfer)
{
	struct octhci_softc *sc = (struct octhci_softc *)xfer->device->bus;
	usb_port_status_t ps;
	usb_device_request_t *req;
	void *buf = NULL;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	int s, len, value, index;
	int l, totlen = 0;
	int i;
	/* int port; */
	uint32_t v;

	KASSERT(xfer->rqflags & URQ_REQUEST);

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	req = &xfer->request;

	DPRINTFN(4,("%s: type=0x%02x request=%02x\n", __func__,
	    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf, 0);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(uint8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8,("octhci_root_ctrl_start: wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(octhci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &octhci_devd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_OTHER_SPEED_CONFIGURATION:
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &octhci_confd, l);
			((usb_config_descriptor_t *)buf)->bDescriptorType =
			    value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &octhci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &octhci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = usbd_str(buf, len, "\001");
				break;
			case 1: /* Vendor */
				totlen = usbd_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = usbd_str(buf, len, "octHCI root hub");
				break;
			}
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(uint8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(8, ("octhci_root_ctrl_start: UR_CLEAR_PORT_FEATURE "
		    "port=%d feature=%d\n", index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		switch (value) {
		case UHF_PORT_ENABLE:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTENA);
			break;
		case UHF_PORT_SUSPEND:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTSUSP);
			break;
		case UHF_PORT_POWER:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTPWR);
			break;
		case UHF_PORT_INDICATOR:
			break;
		case UHF_C_PORT_CONNECTION:
			break;
		case UHF_C_PORT_ENABLE:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTENCHNG);
			break;
		case UHF_C_PORT_SUSPEND:
			break;
		case UHF_C_PORT_OVER_CURRENT:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTOVRCURRCHNG);
			break;
		case UHF_C_PORT_RESET:
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTRST);
			sc->sc_port_reset = 0;
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;

	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		hubd = octhci_hubd;
		hubd.bNbrPorts = sc->sc_noport;

		/* Taken from the SDK Root Hub example */
		USETW(hubd.wHubCharacteristics, UHD_OC_INDIVIDUAL);
		hubd.bPwrOn2PwrGood = 1;
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE;

		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 16) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len);
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("octhci_root_ctrl_start: get port status i=%d\n",
		    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}

		v = octhci_regc_read(sc, USBC_HPRT_OFFSET);
		DPRINTFN(8,("octhci_root_ctrl_start: port status=0x%04x\n", v));
		switch ((v & USBC_HPRT_PRTSPD) >> USBC_HPRT_PRTSPD_OFFSET) {
		case USBC_HPRT_PRTSPD_FULL:
			i = UPS_FULL_SPEED;
			break;
		case USBC_HPRT_PRTSPD_LOW:
			i = UPS_LOW_SPEED;
			break;
		case USBC_HPRT_PRTSPD_HIGH:
		default:
			i = UPS_HIGH_SPEED;
			break;
		}
		if (v & USBC_HPRT_PRTCONNSTS) {
			i |= UPS_CURRENT_CONNECT_STATUS;
			sc->sc_port_change = (sc->sc_port_connect != 1);
			sc->sc_port_connect = 1;
		}
		if (v & USBC_HPRT_PRTENA)
			i |= UPS_PORT_ENABLED;
		if (v & USBC_HPRT_PRTOVRCURRACT)
			i |= UPS_OVERCURRENT_INDICATOR;
		if (v & USBC_HPRT_PRTRST)
			i |= UPS_RESET;
		if (v & USBC_HPRT_PRTPWR)
			i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);

		i = 0;
		if (v & USBC_HPRT_PRTCONNDET || sc->sc_port_change)
			i |= UPS_C_CONNECT_STATUS;
		if (v & USBC_HPRT_PRTENCHNG)
			i |= UPS_C_PORT_ENABLED;
		if (v & USBC_HPRT_PRTOVRCURRCHNG)
			i |= UPS_C_OVERCURRENT_INDICATOR;
		if (sc->sc_port_reset)
			i |= UPS_C_PORT_RESET;
		USETW(ps.wPortChange, i);

		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):

		i = index >> 8;
		index &= 0x00ff;

		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}

		switch (value) {
		case UHF_PORT_ENABLE:
			/* The application can't set the enable bit */
			break;
		case UHF_PORT_SUSPEND:
			DPRINTFN(6, ("suspend port %u (LPM=%u)\n", index, i));
			octhci_regc_set(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTSUSP);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(6, ("reset port %d\n", index));
			/* Start reset sequence. */
			octhci_regc_set(sc, USBC_HPRT_OFFSET, USBC_HPRT_PRTRST);
			/* Wait for reset to complete. */
			usb_delay_ms(&sc->sc_bus, USB_PORT_ROOT_RESET_DELAY);
			if (sc->sc_bus.dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			/* Terminate reset sequence. */
			octhci_regc_clear(sc, USBC_HPRT_OFFSET,
			    USBC_HPRT_PRTRST);
			sc->sc_port_reset = 1;
			break;
		case UHF_PORT_POWER:
			DPRINTFN(3, ("set port power %d\n", index));
			octhci_regc_set(sc, USBC_HPRT_OFFSET, USBC_HPRT_PRTPWR);
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(3, ("set port indicator %d\n", index));
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_CLEAR_TT_BUFFER, UT_WRITE_CLASS_OTHER):
	case C(UR_RESET_TT, UT_WRITE_CLASS_OTHER):
	case C(UR_GET_TT_STATE, UT_READ_CLASS_OTHER):
	case C(UR_STOP_TT, UT_WRITE_CLASS_OTHER):
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
ret:
	xfer->status = err;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
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
octhci_root_ctrl_done(struct usbd_xfer *xfer)
{
}

/*
 * USBD Root Interrupt Pipe Methods.
 */

usbd_status
octhci_root_intr_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (octhci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}


usbd_status
octhci_root_intr_start(struct usbd_xfer *xfer)
{
	struct octhci_softc *sc = (struct octhci_softc *)xfer->device->bus;

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

void
octhci_root_intr_abort(struct usbd_xfer *xfer)
{
	struct octhci_softc *sc = (struct octhci_softc *)xfer->device->bus;
	int s;

	sc->sc_intrxfer = NULL;

	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

void
octhci_root_intr_close(struct usbd_pipe *pipe)
{
}

void
octhci_root_intr_done(struct usbd_xfer *xfer)
{
}

