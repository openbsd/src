/* $OpenBSD: xhci.c,v 1.53 2015/01/09 20:17:05 kettenis Exp $ */

/*
 * Copyright (c) 2014 Martin Pieuchot
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/pool.h>
#include <sys/endian.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct cfdriver xhci_cd = {
	NULL, "xhci", DV_DULL
};

#ifdef XHCI_DEBUG
#define DPRINTF(x)	do { if (xhcidebug) printf x; } while(0)
#define DPRINTFN(n,x)	do { if (xhcidebug>(n)) printf x; } while (0)
int xhcidebug = 3;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define DEVNAME(sc)		((sc)->sc_bus.bdev.dv_xname)

#define TRBOFF(r, trb)	((char *)(trb) - (char *)((r).trbs))
#define DEQPTR(r)	((r).dma.paddr + (sizeof(struct xhci_trb) * (r).index))

struct pool *xhcixfer;

struct xhci_pipe {
	struct usbd_pipe	pipe;

	uint8_t			dci;
	uint8_t			slot;	/* Device slot ID */
	struct xhci_ring	ring;

	/*
	 * XXX used to pass the xfer pointer back to the
	 * interrupt routine, better way?
	 */
	struct usbd_xfer	*pending_xfers[XHCI_MAX_XFER];
	int			 halted;
	size_t			 free_trbs;
};

int	xhci_reset(struct xhci_softc *);
int	xhci_intr1(struct xhci_softc *);
void	xhci_waitintr(struct xhci_softc *, struct usbd_xfer *);
void	xhci_event_dequeue(struct xhci_softc *);
void	xhci_event_xfer(struct xhci_softc *, uint64_t, uint32_t, uint32_t);
void	xhci_event_command(struct xhci_softc *, uint64_t);
void	xhci_event_port_change(struct xhci_softc *, uint64_t, uint32_t);
int	xhci_pipe_init(struct xhci_softc *, struct usbd_pipe *);
void	xhci_context_setup(struct xhci_softc *, struct usbd_pipe *);
int	xhci_scratchpad_alloc(struct xhci_softc *, int);
void	xhci_scratchpad_free(struct xhci_softc *);
int	xhci_softdev_alloc(struct xhci_softc *, uint8_t);
void	xhci_softdev_free(struct xhci_softc *, uint8_t);
int	xhci_ring_alloc(struct xhci_softc *, struct xhci_ring *, size_t,
	    size_t);
void	xhci_ring_free(struct xhci_softc *, struct xhci_ring *);
void	xhci_ring_reset(struct xhci_softc *, struct xhci_ring *);
struct	xhci_trb *xhci_ring_dequeue(struct xhci_softc *, struct xhci_ring *,
	    int);

struct	xhci_trb *xhci_xfer_get_trb(struct xhci_softc *, struct usbd_xfer*,
	    uint8_t *, int);
void	xhci_xfer_done(struct usbd_xfer *xfer);
/* xHCI command helpers. */
int	xhci_command_submit(struct xhci_softc *, struct xhci_trb *, int);
int	xhci_command_abort(struct xhci_softc *);

void	xhci_cmd_reset_ep_async(struct xhci_softc *, uint8_t, uint8_t);
void	xhci_cmd_set_tr_deq_async(struct xhci_softc *, uint8_t, uint8_t, uint64_t);
int	xhci_cmd_configure_ep(struct xhci_softc *, uint8_t, uint64_t);
int	xhci_cmd_stop_ep(struct xhci_softc *, uint8_t, uint8_t);
int	xhci_cmd_slot_control(struct xhci_softc *, uint8_t *, int);
int	xhci_cmd_set_address(struct xhci_softc *, uint8_t,  uint64_t, uint32_t);
int	xhci_cmd_evaluate_ctx(struct xhci_softc *, uint8_t, uint64_t);
#ifdef XHCI_DEBUG
int	xhci_cmd_noop(struct xhci_softc *);
#endif

/* XXX should be part of the Bus interface. */
void	xhci_abort_xfer(struct usbd_xfer *, usbd_status);
void	xhci_pipe_close(struct usbd_pipe *);
void	xhci_noop(struct usbd_xfer *);

void 	xhci_timeout(void *);

/* USBD Bus Interface. */
usbd_status	  xhci_pipe_open(struct usbd_pipe *);
int		  xhci_setaddr(struct usbd_device *, int);
void		  xhci_softintr(void *);
void		  xhci_poll(struct usbd_bus *);
struct usbd_xfer *xhci_allocx(struct usbd_bus *);
void		  xhci_freex(struct usbd_bus *, struct usbd_xfer *);

usbd_status	  xhci_root_ctrl_transfer(struct usbd_xfer *);
usbd_status	  xhci_root_ctrl_start(struct usbd_xfer *);

usbd_status	  xhci_root_intr_transfer(struct usbd_xfer *);
usbd_status	  xhci_root_intr_start(struct usbd_xfer *);
void		  xhci_root_intr_abort(struct usbd_xfer *);
void		  xhci_root_intr_done(struct usbd_xfer *);

usbd_status	  xhci_device_ctrl_transfer(struct usbd_xfer *);
usbd_status	  xhci_device_ctrl_start(struct usbd_xfer *);
void		  xhci_device_ctrl_abort(struct usbd_xfer *);

usbd_status	  xhci_device_generic_transfer(struct usbd_xfer *);
usbd_status	  xhci_device_generic_start(struct usbd_xfer *);
void		  xhci_device_generic_abort(struct usbd_xfer *);
void		  xhci_device_generic_done(struct usbd_xfer *);

#define XHCI_INTR_ENDPT 1

struct usbd_bus_methods xhci_bus_methods = {
	.open_pipe = xhci_pipe_open,
	.dev_setaddr = xhci_setaddr,
	.soft_intr = xhci_softintr,
	.do_poll = xhci_poll,
	.allocx = xhci_allocx,
	.freex = xhci_freex,
};

struct usbd_pipe_methods xhci_root_ctrl_methods = {
	.transfer = xhci_root_ctrl_transfer,
	.start = xhci_root_ctrl_start,
	.abort = xhci_noop,
	.close = xhci_pipe_close,
	.done = xhci_noop,
};

struct usbd_pipe_methods xhci_root_intr_methods = {
	.transfer = xhci_root_intr_transfer,
	.start = xhci_root_intr_start,
	.abort = xhci_root_intr_abort,
	.close = xhci_pipe_close,
	.done = xhci_root_intr_done,
};

struct usbd_pipe_methods xhci_device_ctrl_methods = {
	.transfer = xhci_device_ctrl_transfer,
	.start = xhci_device_ctrl_start,
	.abort = xhci_device_ctrl_abort,
	.close = xhci_pipe_close,
	.done = xhci_noop,
};

#if notyet
struct usbd_pipe_methods xhci_device_isoc_methods = {
};
#endif

struct usbd_pipe_methods xhci_device_bulk_methods = {
	.transfer = xhci_device_generic_transfer,
	.start = xhci_device_generic_start,
	.abort = xhci_device_generic_abort,
	.close = xhci_pipe_close,
	.done = xhci_device_generic_done,
};

struct usbd_pipe_methods xhci_device_generic_methods = {
	.transfer = xhci_device_generic_transfer,
	.start = xhci_device_generic_start,
	.abort = xhci_device_generic_abort,
	.close = xhci_pipe_close,
	.done = xhci_device_generic_done,
};

#ifdef XHCI_DEBUG
static void
xhci_dump_trb(struct xhci_trb *trb)
{
	printf("trb=%p (0x%016llx 0x%08x 0x%b)\n", trb,
	    (long long)letoh64(trb->trb_paddr), letoh32(trb->trb_status),
	    (int)letoh32(trb->trb_flags), XHCI_TRB_FLAGS_BITMASK);
}
#endif

int	usbd_dma_contig_alloc(struct usbd_bus *, struct usbd_dma_info *,
	    void **, bus_size_t, bus_size_t, bus_size_t);
void	usbd_dma_contig_free(struct usbd_bus *, struct usbd_dma_info *);

int
usbd_dma_contig_alloc(struct usbd_bus *bus, struct usbd_dma_info *dma,
    void **kvap, bus_size_t size, bus_size_t alignment, bus_size_t boundary)
{
	int error;

	dma->tag = bus->dmatag;
	dma->size = size;

	error = bus_dmamap_create(dma->tag, size, 1, size, boundary,
	    BUS_DMA_NOWAIT, &dma->map);
	if (error != 0)
		return (error);;

	error = bus_dmamem_alloc(dma->tag, size, alignment, boundary, &dma->seg,
	    1, &dma->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0)
		goto destroy;

	error = bus_dmamem_map(dma->tag, &dma->seg, 1, size, &dma->vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0)
		goto free;

	error = bus_dmamap_load_raw(dma->tag, dma->map, &dma->seg, 1, size,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto unmap;

	bus_dmamap_sync(dma->tag, dma->map, 0, size, BUS_DMASYNC_PREWRITE);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	if (kvap != NULL)
		*kvap = dma->vaddr;

	return (0);

unmap:
	bus_dmamem_unmap(dma->tag, dma->vaddr, size);
free:
	bus_dmamem_free(dma->tag, &dma->seg, 1);
destroy:
	bus_dmamap_destroy(dma->tag, dma->map);
	return (error);
}

void
usbd_dma_contig_free(struct usbd_bus *bus, struct usbd_dma_info *dma)
{
	if (dma->map != NULL) {
		bus_dmamap_sync(bus->dmatag, dma->map, 0, dma->size,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(bus->dmatag, dma->map);
		bus_dmamem_unmap(bus->dmatag, dma->vaddr, dma->size);
		bus_dmamem_free(bus->dmatag, &dma->seg, 1);
		bus_dmamap_destroy(bus->dmatag, dma->map);
		dma->map = NULL;
	}
}

int
xhci_init(struct xhci_softc *sc)
{
	uint32_t hcr;
	int npage, error;

#ifdef XHCI_DEBUG
	uint16_t vers;

	vers = XREAD2(sc, XHCI_HCIVERSION);
	printf("%s: xHCI version %x.%x\n", DEVNAME(sc), vers >> 8, vers & 0xff);
#endif
	sc->sc_bus.usbrev = USBREV_3_0;
	sc->sc_bus.methods = &xhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct xhci_pipe);

	sc->sc_oper_off = XREAD1(sc, XHCI_CAPLENGTH);
	sc->sc_door_off = XREAD4(sc, XHCI_DBOFF);
	sc->sc_runt_off = XREAD4(sc, XHCI_RTSOFF);

#ifdef XHCI_DEBUG
	printf("%s: CAPLENGTH=%#lx\n", DEVNAME(sc), sc->sc_oper_off);
	printf("%s: DOORBELL=%#lx\n", DEVNAME(sc), sc->sc_door_off);
	printf("%s: RUNTIME=%#lx\n", DEVNAME(sc), sc->sc_runt_off);
#endif

	error = xhci_reset(sc);
	if (error)
		return (error);

	if (xhcixfer == NULL) {
		xhcixfer = malloc(sizeof(struct pool), M_DEVBUF, M_NOWAIT);
		if (xhcixfer == NULL) {
			printf("%s: unable to allocate pool descriptor\n",
			    DEVNAME(sc));
			return (ENOMEM);
		}
		pool_init(xhcixfer, sizeof(struct xhci_xfer), 0, 0, 0,
		    "xhcixfer", NULL);
		pool_setipl(xhcixfer, IPL_SOFTUSB);
	}

	hcr = XREAD4(sc, XHCI_HCCPARAMS);
	sc->sc_ctxsize = XHCI_HCC_CSZ(hcr) ? 64 : 32;
	DPRINTF(("%s: %d bytes context\n", DEVNAME(sc), sc->sc_ctxsize));

#ifdef XHCI_DEBUG
	hcr = XOREAD4(sc, XHCI_PAGESIZE);
	printf("%s: supported page size 0x%08x\n", DEVNAME(sc), hcr);
#endif
	/* Use 4K for the moment since it's easier. */
	sc->sc_pagesize = 4096;

	/* Get port and device slot numbers. */
	hcr = XREAD4(sc, XHCI_HCSPARAMS1);
	sc->sc_noport = XHCI_HCS1_N_PORTS(hcr);
	sc->sc_noslot = XHCI_HCS1_DEVSLOT_MAX(hcr);
	DPRINTF(("%s: %d ports and %d slots\n", DEVNAME(sc), sc->sc_noport,
	    sc->sc_noslot));

	/* Setup Device Context Base Address Array. */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sc->sc_dcbaa.dma,
	    (void **)&sc->sc_dcbaa.segs, (sc->sc_noslot + 1) * sizeof(uint64_t),
	    XHCI_DCBAA_ALIGN, sc->sc_pagesize);
	if (error)
		return (ENOMEM);

	/* Setup command ring. */
	error = xhci_ring_alloc(sc, &sc->sc_cmd_ring, XHCI_MAX_CMDS,
	    XHCI_CMDS_RING_ALIGN);
	if (error) {
		printf("%s: could not allocate command ring.\n", DEVNAME(sc));
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (error);
	}

	/* Setup one event ring and its segment table (ERST). */
	error = xhci_ring_alloc(sc, &sc->sc_evt_ring, XHCI_MAX_EVTS,
	    XHCI_EVTS_RING_ALIGN);
	if (error) {
		printf("%s: could not allocate event ring.\n", DEVNAME(sc));
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (error);
	}

	/* Allocate the required entry for the segment table. */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sc->sc_erst.dma,
	    (void **)&sc->sc_erst.segs, sizeof(struct xhci_erseg),
	    XHCI_ERST_ALIGN, XHCI_ERST_BOUNDARY);
	if (error) {
		printf("%s: could not allocate segment table.\n", DEVNAME(sc));
		xhci_ring_free(sc, &sc->sc_evt_ring);
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (ENOMEM);
	}

	/* Set our ring address and size in its corresponding segment. */
	sc->sc_erst.segs[0].er_addr = htole64(sc->sc_evt_ring.dma.paddr);
	sc->sc_erst.segs[0].er_size = htole32(XHCI_MAX_EVTS);
	sc->sc_erst.segs[0].er_rsvd = 0;
	bus_dmamap_sync(sc->sc_erst.dma.tag, sc->sc_erst.dma.map, 0,
	    sc->sc_erst.dma.size, BUS_DMASYNC_PREWRITE);

	/* Get the number of scratch pages and configure them if necessary. */
	hcr = XREAD4(sc, XHCI_HCSPARAMS2);
	npage = XHCI_HCS2_SPB_MAX(hcr);
	DPRINTF(("%s: %d scratch pages\n", DEVNAME(sc), npage));

	if (npage > 0 && xhci_scratchpad_alloc(sc, npage)) {
		printf("%s: could not allocate scratchpad.\n", DEVNAME(sc));
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_erst.dma);
		xhci_ring_free(sc, &sc->sc_evt_ring);
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (ENOMEM);
	}


	return (0);
}

void
xhci_config(struct xhci_softc *sc)
{
	uint64_t paddr;
	uint32_t hcr;

	/* Make sure to program a number of device slots we can handle. */
	if (sc->sc_noslot > USB_MAX_DEVICES)
		sc->sc_noslot = USB_MAX_DEVICES;
	hcr = XOREAD4(sc, XHCI_CONFIG) & ~XHCI_CONFIG_SLOTS_MASK;
	XOWRITE4(sc, XHCI_CONFIG, hcr | sc->sc_noslot);

	/* Set the device context base array address. */
	paddr = (uint64_t)sc->sc_dcbaa.dma.paddr;
	XOWRITE4(sc, XHCI_DCBAAP_LO, (uint32_t)paddr);
	XOWRITE4(sc, XHCI_DCBAAP_HI, (uint32_t)(paddr >> 32));

	DPRINTF(("%s: DCBAAP=%#x%#x\n", DEVNAME(sc),
	    XOREAD4(sc, XHCI_DCBAAP_HI), XOREAD4(sc, XHCI_DCBAAP_LO)));

	/* Set the command ring address. */
	paddr = (uint64_t)sc->sc_cmd_ring.dma.paddr;
	XOWRITE4(sc, XHCI_CRCR_LO, ((uint32_t)paddr) | XHCI_CRCR_LO_RCS);
	XOWRITE4(sc, XHCI_CRCR_HI, (uint32_t)(paddr >> 32));

	DPRINTF(("%s: CRCR=%#x%#x (%016llx)\n", DEVNAME(sc),
	    XOREAD4(sc, XHCI_CRCR_HI), XOREAD4(sc, XHCI_CRCR_LO), paddr));

	/* Set the ERST count number to 1, since we use only one event ring. */
	XRWRITE4(sc, XHCI_ERSTSZ(0), XHCI_ERSTS_SET(1));

	/* Set the segment table address. */
	paddr = (uint64_t)sc->sc_erst.dma.paddr;
	XRWRITE4(sc, XHCI_ERSTBA_LO(0), (uint32_t)paddr);
	XRWRITE4(sc, XHCI_ERSTBA_HI(0), (uint32_t)(paddr >> 32));

	DPRINTF(("%s: ERSTBA=%#x%#x\n", DEVNAME(sc),
	    XRREAD4(sc, XHCI_ERSTBA_HI(0)), XRREAD4(sc, XHCI_ERSTBA_LO(0))));

	/* Set the ring dequeue address. */
	paddr = (uint64_t)sc->sc_evt_ring.dma.paddr;
	XRWRITE4(sc, XHCI_ERDP_LO(0), (uint32_t)paddr);
	XRWRITE4(sc, XHCI_ERDP_HI(0), (uint32_t)(paddr >> 32));

	DPRINTF(("%s: ERDP=%#x%#x\n", DEVNAME(sc),
	    XRREAD4(sc, XHCI_ERDP_HI(0)), XRREAD4(sc, XHCI_ERDP_LO(0))));

	/* Enable interrupts. */
	hcr = XRREAD4(sc, XHCI_IMAN(0));
	XRWRITE4(sc, XHCI_IMAN(0), hcr | XHCI_IMAN_INTR_ENA);

	/* Set default interrupt moderation. */
	XRWRITE4(sc, XHCI_IMOD(0), XHCI_IMOD_DEFAULT);

	/* Allow event interrupt and start the controller. */
	XOWRITE4(sc, XHCI_USBCMD, XHCI_CMD_INTE|XHCI_CMD_RS);

	DPRINTF(("%s: USBCMD=%#x\n", DEVNAME(sc), XOREAD4(sc, XHCI_USBCMD)));
	DPRINTF(("%s: IMAN=%#x\n", DEVNAME(sc), XRREAD4(sc, XHCI_IMAN(0))));
}

int
xhci_detach(struct device *self, int flags)
{
	struct xhci_softc *sc = (struct xhci_softc *)self;
	int rv;

	rv = config_detach_children(self, flags);
	if (rv != 0) {
		printf("%s: error while detaching %d\n", DEVNAME(sc), rv);
		return (rv);
	}

	/* Since the hardware might already be gone, ignore the errors. */
	xhci_command_abort(sc);

	xhci_reset(sc);

	/* Disable interrupts. */
	XRWRITE4(sc, XHCI_IMOD(0), 0);
	XRWRITE4(sc, XHCI_IMAN(0), 0);

	/* Clear the event ring address. */
	XRWRITE4(sc, XHCI_ERDP_LO(0), 0);
	XRWRITE4(sc, XHCI_ERDP_HI(0), 0);

	XRWRITE4(sc, XHCI_ERSTBA_LO(0), 0);
	XRWRITE4(sc, XHCI_ERSTBA_HI(0), 0);

	XRWRITE4(sc, XHCI_ERSTSZ(0), 0);

	/* Clear the command ring address. */
	XOWRITE4(sc, XHCI_CRCR_LO, 0);
	XOWRITE4(sc, XHCI_CRCR_HI, 0);

	XOWRITE4(sc, XHCI_DCBAAP_LO, 0);
	XOWRITE4(sc, XHCI_DCBAAP_HI, 0);

	if (sc->sc_spad.npage > 0)
		xhci_scratchpad_free(sc);

	usbd_dma_contig_free(&sc->sc_bus, &sc->sc_erst.dma);
	xhci_ring_free(sc, &sc->sc_evt_ring);
	xhci_ring_free(sc, &sc->sc_cmd_ring);
	usbd_dma_contig_free(&sc->sc_bus, &sc->sc_dcbaa.dma);

	return (0);
}

int
xhci_activate(struct device *self, int act)
{
	struct xhci_softc *sc = (struct xhci_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_RESUME:
		sc->sc_bus.use_polling++;

		xhci_reset(sc);
		xhci_ring_reset(sc, &sc->sc_cmd_ring);
		xhci_ring_reset(sc, &sc->sc_evt_ring);

		/* Renesas controllers, at least, need more time to resume. */
		usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

		xhci_config(sc);

		sc->sc_bus.use_polling--;
		rv = config_activate_children(self, act);
		break;
	case DVACT_POWERDOWN:
		rv = config_activate_children(self, act);
		xhci_reset(sc);
		break;
	default:
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

int
xhci_reset(struct xhci_softc *sc)
{
	uint32_t hcr;
	int i;

	XOWRITE4(sc, XHCI_USBCMD, 0);	/* Halt controller */
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = XOREAD4(sc, XHCI_USBSTS) & XHCI_STS_HCH;
		if (hcr)
			break;
	}

	if (!hcr)
		printf("%s: halt timeout\n", DEVNAME(sc));

	XOWRITE4(sc, XHCI_USBCMD, XHCI_CMD_HCRST);
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = XOREAD4(sc, XHCI_USBCMD) & XHCI_STS_CNR;
		if (!hcr)
			break;
	}

	if (hcr) {
		printf("%s: reset timeout\n", DEVNAME(sc));
		return (EIO);
	}

	return (0);
}


int
xhci_intr(void *v)
{
	struct xhci_softc *sc = v;

	if (sc == NULL || sc->sc_bus.dying)
		return (0);

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
		DPRINTFN(16, ("xhci_intr: ignored interrupt while polling\n"));
		return (0);
	}

	return (xhci_intr1(sc));
}

int
xhci_intr1(struct xhci_softc *sc)
{
	uint32_t intrs;

	intrs = XOREAD4(sc, XHCI_USBSTS);
	if (intrs == 0xffffffff) {
		sc->sc_bus.dying = 1;
		return (0);
	}

	if ((intrs & XHCI_STS_EINT) == 0)
		return (0);

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;

	if (intrs & XHCI_STS_HSE) {
		printf("%s: host system error\n", DEVNAME(sc));
		sc->sc_bus.dying = 1;
		sc->sc_bus.intr_context--;
		return (1);
	}

	XOWRITE4(sc, XHCI_USBSTS, intrs); /* Acknowledge */
	usb_schedsoftintr(&sc->sc_bus);

	/* Acknowledge PCI interrupt */
	intrs = XRREAD4(sc, XHCI_IMAN(0));
	XRWRITE4(sc, XHCI_IMAN(0), intrs | XHCI_IMAN_INTR_PEND);

	sc->sc_bus.intr_context--;

	return (1);
}

void
xhci_poll(struct usbd_bus *bus)
{
	struct xhci_softc *sc = (struct xhci_softc *)bus;

	if (XOREAD4(sc, XHCI_USBSTS))
		xhci_intr1(sc);
}

void
xhci_waitintr(struct xhci_softc *sc, struct usbd_xfer *xfer)
{
	int timo;

	for (timo = xfer->timeout; timo >= 0; timo--) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_bus.dying)
			break;

		if (xfer->status != USBD_IN_PROGRESS)
			return;

		xhci_intr1(sc);
	}

	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
}

void
xhci_softintr(void *v)
{
	struct xhci_softc *sc = v;

	if (sc->sc_bus.dying)
		return;

	sc->sc_bus.intr_context++;
	xhci_event_dequeue(sc);
	sc->sc_bus.intr_context--;
}

void
xhci_event_dequeue(struct xhci_softc *sc)
{
	struct xhci_trb *trb;
	uint64_t paddr;
	uint32_t status, flags;

	while ((trb = xhci_ring_dequeue(sc, &sc->sc_evt_ring, 1)) != NULL) {
		paddr = letoh64(trb->trb_paddr);
		status = letoh32(trb->trb_status);
		flags = letoh32(trb->trb_flags);

		switch (flags & XHCI_TRB_TYPE_MASK) {
		case XHCI_EVT_XFER:
			xhci_event_xfer(sc, paddr, status, flags);
			break;
		case XHCI_EVT_CMD_COMPLETE:
			memcpy(&sc->sc_result_trb, trb, sizeof(*trb));
			xhci_event_command(sc, paddr);
			break;
		case XHCI_EVT_PORT_CHANGE:
			xhci_event_port_change(sc, paddr, status);
			break;
		default:
#ifdef XHCI_DEBUG
			printf("event (%d): ", XHCI_TRB_TYPE(flags));
			xhci_dump_trb(trb);
#endif
			break;
		}

	}

	paddr = (uint64_t)DEQPTR(sc->sc_evt_ring);
	XRWRITE4(sc, XHCI_ERDP_LO(0), ((uint32_t)paddr) | XHCI_ERDP_LO_BUSY);
	XRWRITE4(sc, XHCI_ERDP_HI(0), (uint32_t)(paddr >> 32));
}

void
xhci_event_xfer(struct xhci_softc *sc, uint64_t paddr, uint32_t status,
    uint32_t flags)
{
	struct xhci_pipe *xp;
	struct usbd_xfer *xfer;
	struct xhci_xfer *xx;
	uint8_t dci, slot, code, remain;
	int trb_idx;

	slot = XHCI_TRB_GET_SLOT(flags);
	dci = XHCI_TRB_GET_EP(flags);
	if (slot > sc->sc_noslot) {
		DPRINTF(("%s: incorrect slot (%u)\n", DEVNAME(sc), slot));
		return;
	}

	xp = sc->sc_sdevs[slot].pipes[dci - 1];
	if (xp == NULL)
		return;

	code = XHCI_TRB_GET_CODE(status);
	remain = XHCI_TRB_REMAIN(status);

	trb_idx = (paddr - xp->ring.dma.paddr) / sizeof(struct xhci_trb);
	if (trb_idx < 0 || trb_idx >= xp->ring.ntrb) {
		printf("%s: wrong trb index (%d) max is %zu\n", DEVNAME(sc),
		    trb_idx, xp->ring.ntrb - 1);
		return;
	}

	xfer = xp->pending_xfers[trb_idx];
	if (xfer == NULL) {
#if 1
		DPRINTF(("%s: dev %d dci=%d paddr=0x%016llx idx=%d remain=%u"
		    " code=%u\n", DEVNAME(sc), slot, dci, (long long)paddr,
		    trb_idx, remain, code));
#endif
		printf("%s: NULL xfer pointer\n", DEVNAME(sc));
		return;
	}

	switch (code) {
	case XHCI_CODE_SUCCESS:
		/*
		 * This might be the last TRB of a TD that ended up
		 * with a Short Transfer condition, see below.
		 */
		if (xfer->actlen == 0)
			xfer->actlen = xfer->length - remain;

		xfer->status = USBD_NORMAL_COMPLETION;
		break;
	case XHCI_CODE_SHORT_XFER:
		xfer->actlen = xfer->length - remain;

		/*
		 * If this is not the last TRB of a transfer, we should
		 * theoretically clear the IOC at the end of the chain
		 * but the HC might have already processed it before we
		 * had a change to schedule the softinterrupt.
		 */
		xx = (struct xhci_xfer *)xfer;
		if (xx->index != trb_idx)
			return;

		xfer->status = USBD_NORMAL_COMPLETION;
		break;
	case XHCI_CODE_TXERR:
	case XHCI_CODE_SPLITERR:
		xfer->status = USBD_IOERROR;
		break;
	case XHCI_CODE_STALL:
		/* We need to report this condition for umass(4). */
		xfer->status = USBD_STALLED;

		/* FALLTHROUGH */
	case XHCI_CODE_BABBLE:
		/*
		 * Since the stack might try to start a new transfer as
		 * soon as a pending one finishes, make sure the endpoint
		 * is fully reset before calling usb_transfer_complete().
		 */
		xp->halted = 1;
		xhci_cmd_reset_ep_async(sc, slot, dci);
		return;
	default:
#if 1
		DPRINTF(("%s: dev %d dci=%d paddr=0x%016llx idx=%d remain=%u"
		    " code=%u\n", DEVNAME(sc), slot, dci, (long long)paddr,
		    trb_idx, remain, code));
#endif
		DPRINTF(("%s: unhandled code %d\n", DEVNAME(sc), code));
		xfer->status = USBD_IOERROR;
		xp->halted = 1;
		break;
	}

	xhci_xfer_done(xfer);
}

void
xhci_event_command(struct xhci_softc *sc, uint64_t paddr)
{
	struct xhci_trb *trb;
	struct usbd_xfer *xfer;
	struct xhci_pipe *xp;
	uint32_t flags;
	uint8_t dci, slot;
	int i, trb_idx;

	trb_idx = (paddr - sc->sc_cmd_ring.dma.paddr) / sizeof(*trb);
	if (trb_idx < 0 || trb_idx >= sc->sc_cmd_ring.ntrb) {
		printf("%s: wrong trb index (%d) max is %zu\n", DEVNAME(sc),
		    trb_idx, sc->sc_cmd_ring.ntrb - 1);
		return;
	}

	trb = &sc->sc_cmd_ring.trbs[trb_idx];

	flags = letoh32(trb->trb_flags);

	slot = XHCI_TRB_GET_SLOT(flags);
	dci = XHCI_TRB_GET_EP(flags);

	switch (flags & XHCI_TRB_TYPE_MASK) {
	case XHCI_CMD_RESET_EP:
		xp = sc->sc_sdevs[slot].pipes[dci - 1];
		if (xp == NULL)
			break;

		/* Update the dequeue pointer past the last TRB. */
		xhci_cmd_set_tr_deq_async(sc, xp->slot, xp->dci,
		    DEQPTR(xp->ring) | xp->ring.toggle);
		break;
	case XHCI_CMD_SET_TR_DEQ:
		xp = sc->sc_sdevs[slot].pipes[dci - 1];
		if (xp == NULL)
			break;

		xp->halted = 0;

		/* Complete all pending transfers. */
		for (i = 0; i < XHCI_MAX_XFER; i++) {
			xfer = xp->pending_xfers[i];
			if (xfer != NULL && xfer->done == 0) {
				if (xfer->status != USBD_STALLED)
					xfer->status = USBD_IOERROR;
				xhci_xfer_done(xfer);
			}
		}
		break;
	default:
		/* All other commands are synchronous. */
		KASSERT(sc->sc_cmd_trb == trb);
		sc->sc_cmd_trb = NULL;
		wakeup(&sc->sc_cmd_trb);
		break;
	}
}

void
xhci_event_port_change(struct xhci_softc *sc, uint64_t paddr, uint32_t status)
{
	struct usbd_xfer *xfer = sc->sc_intrxfer;
	uint32_t port = XHCI_TRB_PORTID(paddr);
	uint8_t *p;

	if (XHCI_TRB_GET_CODE(status) != XHCI_CODE_SUCCESS) {
		DPRINTF(("%s: failed port status event\n", DEVNAME(sc)));
		return;
	}

	if (xfer == NULL)
		return;

	p = KERNADDR(&xfer->dmabuf, 0);
	memset(p, 0, xfer->length);

	p[port/8] |= 1 << (port%8);
	DPRINTF(("%s: port=%d change=0x%02x\n", DEVNAME(sc), port, *p));

	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
xhci_xfer_done(struct usbd_xfer *xfer)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_xfer *xx = (struct xhci_xfer *)xfer;
	int ntrb, i;

#ifdef XHCI_DEBUG
	if (xx->index < 0 || xp->pending_xfers[xx->index] == NULL) {
		printf("%s: xfer=%p done (index=%d, ntrb=%zd)\n", __func__,
		    xfer, xx->index, xx->ntrb);
	}
#endif

	for (ntrb = 0, i = xx->index; ntrb < xx->ntrb; ntrb++, i--) {
		xp->pending_xfers[i] = NULL;
		if (i == 0)
			i = (xp->ring.ntrb - 1);
	}
	xp->free_trbs += xx->ntrb;
	xx->index = -1;
	xx->ntrb = 0;

	timeout_del(&xfer->timeout_handle);
	usb_transfer_complete(xfer);
}

/*
 * Calculate the Device Context Index (DCI) for endpoints as stated
 * in section 4.5.1 of xHCI specification r1.1.
 */
static inline uint8_t
xhci_ed2dci(usb_endpoint_descriptor_t *ed)
{
	uint8_t dir;

	if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL)
		return (UE_GET_ADDR(ed->bEndpointAddress) * 2 + 1);

	if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
		dir = 1;
	else
		dir = 0;

	return (UE_GET_ADDR(ed->bEndpointAddress) * 2 + dir);
}

usbd_status
xhci_pipe_open(struct usbd_pipe *pipe)
{
	struct xhci_softc *sc = (struct xhci_softc *)pipe->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint8_t slot = 0, xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	int error;

	KASSERT(xp->slot == 0);

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	/* Root Hub */
	if (pipe->device->depth == 0) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &xhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | XHCI_INTR_ENDPT:
			pipe->methods = &xhci_root_intr_methods;
			break;
		default:
			pipe->methods = NULL;
			return (USBD_INVAL);
		}
		return (USBD_NORMAL_COMPLETION);
	}

#if 0
	/* Issue a noop to check if the command ring is correctly configured. */
	xhci_cmd_noop(sc);
#endif

	switch (xfertype) {
	case UE_CONTROL:
		pipe->methods = &xhci_device_ctrl_methods;

		/*
		 * Get a slot and init the device's contexts.
		 *
		 * Since the control enpoint, represented as the default
		 * pipe, is always opened first we are dealing with a
		 * new device.  Put a new slot in the ENABLED state.
		 *
		 */
		error = xhci_cmd_slot_control(sc, &slot, 1);
		if (error || slot == 0 || slot > sc->sc_noslot)
			return (USBD_INVAL);

		if (xhci_softdev_alloc(sc, slot)) {
			xhci_cmd_slot_control(sc, &slot, 0);
			return (USBD_NOMEM);
		}

		break;
	case UE_ISOCHRONOUS:
#if notyet
		pipe->methods = &xhci_device_isoc_methods;
		break;
#else
		DPRINTF(("%s: isochronous xfer not supported \n", __func__));
		return (USBD_INVAL);
#endif
	case UE_BULK:
		pipe->methods = &xhci_device_bulk_methods;
		break;
	case UE_INTERRUPT:
		pipe->methods = &xhci_device_generic_methods;
		break;
	default:
		return (USBD_INVAL);
	}

	/*
	 * Our USBD Bus Interface is pipe-oriented but for most of the
	 * operations we need to access a device context, so keep trace
	 * of the slot ID in every pipe.
	 */
	if (slot == 0)
		slot = ((struct xhci_pipe *)pipe->device->default_pipe)->slot;

	xp->slot = slot;
	xp->dci = xhci_ed2dci(ed);

	if (xhci_pipe_init(sc, pipe)) {
		xhci_cmd_slot_control(sc, &slot, 0);
		return (USBD_IOERROR);
	}

	return (USBD_NORMAL_COMPLETION);
}

/*
 * Set the maximum Endpoint Service Interface Time (ESIT) payload and
 * the average TRB buffer length for an endpoint.
 */
static inline uint32_t
xhci_get_txinfo(struct xhci_softc *sc, struct usbd_pipe *pipe)
{
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint32_t mep, atl, mps = UGETW(ed->wMaxPacketSize);

	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_CONTROL:
		mep = 0;
		atl = 8;
		break;
	case UE_INTERRUPT:
	case UE_ISOCHRONOUS:
		if (pipe->device->speed == USB_SPEED_SUPER) {
			/*  XXX Read the companion descriptor */
		}

		mep = (UE_GET_TRANS(mps) | 0x1) * UE_GET_SIZE(mps);
		atl = min(sc->sc_pagesize, mep);
		break;
	case UE_BULK:
	default:
		mep = 0;
		atl = 0;
	}

	return (XHCI_EPCTX_MAX_ESIT_PAYLOAD(mep) | XHCI_EPCTX_AVG_TRB_LEN(atl));
}

void
xhci_context_setup(struct xhci_softc *sc, struct usbd_pipe *pipe)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)pipe;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	uint8_t ival, speed, cerr = 0;
	uint32_t mps, route = 0, rhport = 0;
	struct usbd_device *hub;

	/*
	 * Calculate the Route String.  Assume that there is no hub with
	 * more than 15 ports and that they all have a detph < 6.  See
	 * section 8.9 of USB 3.1 Specification for more details.
	 */
	for (hub = pipe->device; hub->myhub->depth; hub = hub->myhub) {
		uint32_t port = hub->powersrc->portno;
		uint32_t depth = hub->myhub->depth;

		route |= port << (4 * (depth - 1));
	}

	/* Get Root Hub port */
	rhport = hub->powersrc->portno;

	switch (pipe->device->speed) {
	case USB_SPEED_LOW:
		ival= 3;
		speed = XHCI_SPEED_LOW;
		mps = 8;
		break;
	case USB_SPEED_FULL:
		ival = 3;
		speed = XHCI_SPEED_FULL;
		mps = 8;
		break;
	case USB_SPEED_HIGH:
		ival = min(3, ed->bInterval);
		speed = XHCI_SPEED_HIGH;
		mps = 64;
		break;
	case USB_SPEED_SUPER:
		ival = min(3, ed->bInterval);
		speed = XHCI_SPEED_SUPER;
		mps = 512;
		break;
	default:
		return;
	}

	/* XXX Until we fix wMaxPacketSize for ctrl ep depending on the speed */
	mps = max(mps, UE_GET_SIZE(UGETW(ed->wMaxPacketSize)));

	if (pipe->interval != USBD_DEFAULT_INTERVAL)
		ival = min(ival, pipe->interval);

	/* Setup the endpoint context */
	if (xfertype != UE_ISOCHRONOUS)
		cerr = 3;

	if (xfertype == UE_CONTROL || xfertype == UE_BULK)
		ival = 0;

	if ((ed->bEndpointAddress & UE_DIR_IN) || (xfertype == UE_CONTROL))
		xfertype |= 0x4;

	sdev->ep_ctx[xp->dci-1]->info_lo = htole32(XHCI_EPCTX_SET_IVAL(ival));
	sdev->ep_ctx[xp->dci-1]->info_hi = htole32(
	    XHCI_EPCTX_SET_MPS(mps) | XHCI_EPCTX_SET_EPTYPE(xfertype) |
	    XHCI_EPCTX_SET_CERR(cerr) | XHCI_EPCTX_SET_MAXB(0)
	);
	sdev->ep_ctx[xp->dci-1]->txinfo = htole32(xhci_get_txinfo(sc, pipe));
	sdev->ep_ctx[xp->dci-1]->deqp = htole64(
	    DEQPTR(xp->ring) | xp->ring.toggle
	);

	/* Unmask the new endoint */
	sdev->input_ctx->drop_flags = 0;
	sdev->input_ctx->add_flags = htole32(XHCI_INCTX_MASK_DCI(xp->dci));

	/* Setup the slot context */
	sdev->slot_ctx->info_lo = htole32(
	    XHCI_SCTX_DCI(xp->dci) | XHCI_SCTX_SPEED(speed) |
	    XHCI_SCTX_ROUTE(route)
	);
	sdev->slot_ctx->info_hi = htole32(XHCI_SCTX_RHPORT(rhport));
	sdev->slot_ctx->tt = 0;
	sdev->slot_ctx->state = 0;

/* XXX */
#define UHUB_IS_MTT(dev) (dev->ddesc.bDeviceProtocol == UDPROTO_HSHUBMTT)
	/*
	 * If we are opening the interrupt pipe of a hub, update its
	 * context before putting it in the CONFIGURED state.
	 */
	if (pipe->device->hub != NULL) {
		int nports = pipe->device->hub->nports;

		sdev->slot_ctx->info_lo |= htole32(XHCI_SCTX_HUB(1));
		sdev->slot_ctx->info_hi |= htole32(XHCI_SCTX_NPORTS(nports));

		if (UHUB_IS_MTT(pipe->device))
			sdev->slot_ctx->info_lo |= htole32(XHCI_SCTX_MTT(1));

		sdev->slot_ctx->tt |= htole32(
		    XHCI_SCTX_TT_THINK_TIME(pipe->device->hub->ttthink)
		);
	}

	/*
	 * If this is a Low or Full Speed device below an external High
	 * Speed hub, it needs some TT love.
	 */
	if (speed < XHCI_SPEED_HIGH && pipe->device->myhsport != NULL) {
		struct usbd_device *hshub = pipe->device->myhsport->parent;
		uint8_t slot = ((struct xhci_pipe *)hshub->default_pipe)->slot;

		if (UHUB_IS_MTT(hshub))
			sdev->slot_ctx->info_lo |= htole32(XHCI_SCTX_MTT(1));

		sdev->slot_ctx->tt |= htole32(
		    XHCI_SCTX_TT_HUB_SID(slot) |
		    XHCI_SCTX_TT_PORT_NUM(pipe->device->myhsport->portno)
		);
	}
#undef UHUB_IS_MTT

	/* Unmask the slot context */
	sdev->input_ctx->add_flags |= htole32(XHCI_INCTX_MASK_DCI(0));

	bus_dmamap_sync(sdev->ictx_dma.tag, sdev->ictx_dma.map, 0,
	    sc->sc_pagesize, BUS_DMASYNC_PREWRITE);
}

int
xhci_pipe_init(struct xhci_softc *sc, struct usbd_pipe *pipe)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)pipe;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	int error;

#ifdef XHCI_DEBUG
	struct usbd_device *dev = pipe->device;
	printf("%s: pipe=%p addr=%d depth=%d port=%d speed=%d dev %d dci %u"
	    " (epAddr=0x%x)\n", __func__, pipe, dev->address, dev->depth,
	    dev->powersrc->portno, dev->speed, xp->slot, xp->dci,
	    pipe->endpoint->edesc->bEndpointAddress);
#endif

	if (xhci_ring_alloc(sc, &xp->ring, XHCI_MAX_XFER, XHCI_XFER_RING_ALIGN))
		return (ENOMEM);

	xp->free_trbs = xp->ring.ntrb;
	xp->halted = 0;

	sdev->pipes[xp->dci - 1] = xp;

	xhci_context_setup(sc, pipe);

	if (xp->dci == 1) {
		/*
		 * If we are opening the default pipe, the Slot should
		 * be in the ENABLED state.  Issue an "Address Device"
		 * with BSR=1 to put the device in the DEFAULT state.
		 * We cannot jump directly to the ADDRESSED state with
		 * BSR=0 because some Low/Full speed devices wont accept
		 * a SET_ADDRESS command before we've read their device
		 * descriptor.
		 */
		error = xhci_cmd_set_address(sc, xp->slot,
		    sdev->ictx_dma.paddr, XHCI_TRB_BSR);
	} else {
		error = xhci_cmd_configure_ep(sc, xp->slot,
		    sdev->ictx_dma.paddr);
	}

	if (error) {
		xhci_ring_free(sc, &xp->ring);
		return (EIO);
	}

	return (0);
}

void
xhci_pipe_close(struct usbd_pipe *pipe)
{
	struct xhci_softc *sc = (struct xhci_softc *)pipe->device->bus;
	struct xhci_pipe *lxp, *xp = (struct xhci_pipe *)pipe;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	int i;

	/* Root Hub */
	if (pipe->device->depth == 0)
		return;

	if (!xp->halted || xhci_cmd_stop_ep(sc, xp->slot, xp->dci))
		DPRINTF(("%s: error stopping ep (%d)\n", DEVNAME(sc), xp->dci));

	/* Mask the endpoint */
	sdev->input_ctx->drop_flags = htole32(XHCI_INCTX_MASK_DCI(xp->dci));
	sdev->input_ctx->add_flags = 0;

	/* Update last valid Endpoint Context */
	for (i = 30; i >= 0; i--) {
		lxp = sdev->pipes[i];
		if (lxp != NULL && lxp != xp)
			break;
	}
	sdev->slot_ctx->info_lo = htole32(XHCI_SCTX_DCI(lxp->dci));

	/* Clear the Endpoint Context */
	memset(sdev->ep_ctx[xp->dci - 1], 0, sizeof(struct xhci_epctx));

	bus_dmamap_sync(sdev->ictx_dma.tag, sdev->ictx_dma.map, 0,
	    sc->sc_pagesize, BUS_DMASYNC_PREWRITE);

	if (xhci_cmd_configure_ep(sc, xp->slot, sdev->ictx_dma.paddr))
		DPRINTF(("%s: error clearing ep (%d)\n", DEVNAME(sc), xp->dci));

	xhci_ring_free(sc, &xp->ring);
	sdev->pipes[xp->dci - 1] = NULL;

	/*
	 * If we are closing the default pipe, the device is probably
	 * gone, so put its slot in the DISABLED state.
	 */
	if (xp->dci == 1) {
		xhci_cmd_slot_control(sc, &xp->slot, 0);
		xhci_softdev_free(sc, xp->slot);
	}
}

/*
 * Transition a device from DEFAULT to ADDRESSED Slot state, this hook
 * is needed for Low/Full speed devices.
 *
 * See section 4.5.3 of USB 3.1 Specification for more details.
 */
int
xhci_setaddr(struct usbd_device *dev, int addr)
{
	struct xhci_softc *sc = (struct xhci_softc *)dev->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)dev->default_pipe;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	int error;

	/* Root Hub */
	if (dev->depth == 0)
		return (0);

	KASSERT(xp->dci == 1);

	xhci_context_setup(sc, dev->default_pipe);

	error = xhci_cmd_set_address(sc, xp->slot, sdev->ictx_dma.paddr, 0);

#ifdef XHCI_DEBUG
	if (error == 0) {
		struct xhci_sctx *sctx;
		uint8_t addr;

		bus_dmamap_sync(sdev->octx_dma.tag, sdev->octx_dma.map, 0,
		    sc->sc_pagesize, BUS_DMASYNC_POSTREAD);

		/* Get output slot context. */
		sctx = (struct xhci_sctx *)sdev->octx_dma.vaddr;
		addr = XHCI_SCTX_DEV_ADDR(letoh32(sctx->state));
		error = (addr == 0);

		printf("%s: dev %d addr %d\n", DEVNAME(sc), xp->slot, addr);
	}
#endif

	return (error);
}

struct usbd_xfer *
xhci_allocx(struct usbd_bus *bus)
{
	return (pool_get(xhcixfer, PR_NOWAIT | PR_ZERO));
}

void
xhci_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
	pool_put(xhcixfer, xfer);
}

int
xhci_scratchpad_alloc(struct xhci_softc *sc, int npage)
{
	uint64_t *pte;
	int error, i;

	/* Allocate the required entry for the table. */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sc->sc_spad.table_dma,
	    (void **)&pte, npage * sizeof(uint64_t), XHCI_SPAD_TABLE_ALIGN,
	    sc->sc_pagesize);
	if (error)
		return (ENOMEM);

	/* Allocate pages. XXX does not need to be contiguous. */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sc->sc_spad.pages_dma,
	    NULL, npage * sc->sc_pagesize, sc->sc_pagesize, 0);
	if (error) {
		usbd_dma_contig_free(&sc->sc_bus, &sc->sc_spad.table_dma);
		return (ENOMEM);
	}

	for (i = 0; i < npage; i++) {
		pte[i] = htole64(
		    sc->sc_spad.pages_dma.paddr + (i * sc->sc_pagesize)
		);
	}

	bus_dmamap_sync(sc->sc_spad.table_dma.tag, sc->sc_spad.table_dma.map, 0,
	    npage * sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	/*  Entry 0 points to the table of scratchpad pointers. */
	sc->sc_dcbaa.segs[0] = htole64(sc->sc_spad.table_dma.paddr);
	bus_dmamap_sync(sc->sc_dcbaa.dma.tag, sc->sc_dcbaa.dma.map, 0,
	    sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	sc->sc_spad.npage = npage;

	return (0);
}

void
xhci_scratchpad_free(struct xhci_softc *sc)
{
	sc->sc_dcbaa.segs[0] = 0;
	bus_dmamap_sync(sc->sc_dcbaa.dma.tag, sc->sc_dcbaa.dma.map, 0,
	    sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	usbd_dma_contig_free(&sc->sc_bus, &sc->sc_spad.pages_dma);
	usbd_dma_contig_free(&sc->sc_bus, &sc->sc_spad.table_dma);
}

int
xhci_ring_alloc(struct xhci_softc *sc, struct xhci_ring *ring, size_t ntrb,
    size_t alignment)
{
	size_t size;
	int error;

	size = ntrb * sizeof(struct xhci_trb);

	error = usbd_dma_contig_alloc(&sc->sc_bus, &ring->dma,
	    (void **)&ring->trbs, size, alignment, XHCI_RING_BOUNDARY);
	if (error)
		return (error);

	ring->ntrb = ntrb;

	xhci_ring_reset(sc, ring);

	return (0);
}

void
xhci_ring_free(struct xhci_softc *sc, struct xhci_ring *ring)
{
	usbd_dma_contig_free(&sc->sc_bus, &ring->dma);
}

void
xhci_ring_reset(struct xhci_softc *sc, struct xhci_ring *ring)
{
	size_t size;

	size = ring->ntrb * sizeof(struct xhci_trb);

	memset(ring->trbs, 0, size);

	ring->index = 0;
	ring->toggle = XHCI_TRB_CYCLE;

	/*
	 * Since all our rings use only one segment, at least for
	 * the moment, link their tail to their head.
	 */
	if (ring != &sc->sc_evt_ring) {
		struct xhci_trb *trb = &ring->trbs[ring->ntrb - 1];

		trb->trb_paddr = htole64(ring->dma.paddr);
		trb->trb_flags = htole32(XHCI_TRB_TYPE_LINK | XHCI_TRB_LINKSEG);
	}
	bus_dmamap_sync(ring->dma.tag, ring->dma.map, 0, size,
	    BUS_DMASYNC_PREWRITE);
}

struct xhci_trb*
xhci_ring_dequeue(struct xhci_softc *sc, struct xhci_ring *ring, int cons)
{
	struct xhci_trb *trb;
	uint32_t idx = ring->index;

	KASSERT(idx < ring->ntrb);

	bus_dmamap_sync(ring->dma.tag, ring->dma.map, idx * sizeof(*trb),
	    sizeof(*trb), BUS_DMASYNC_POSTREAD);

	trb = &ring->trbs[idx];

	/* Make sure this TRB can be consumed. */
	if (cons && ring->toggle != (letoh32(trb->trb_flags) & XHCI_TRB_CYCLE))
		return (NULL);
	idx++;

	if (idx < (ring->ntrb - 1)) {
		ring->index = idx;
	} else {
		if (ring->toggle)
			ring->trbs[idx].trb_flags |= htole32(XHCI_TRB_CYCLE);
		else
			ring->trbs[idx].trb_flags &= ~htole32(XHCI_TRB_CYCLE);

		bus_dmamap_sync(ring->dma.tag, ring->dma.map,
		    sizeof(struct xhci_trb) * idx, sizeof(struct xhci_trb),
		    BUS_DMASYNC_PREWRITE);

		ring->index = 0;
		ring->toggle ^= 1;
	}

	return (trb);
}

struct xhci_trb *
xhci_xfer_get_trb(struct xhci_softc *sc, struct usbd_xfer* xfer,
    uint8_t *togglep, int last)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_xfer *xx = (struct xhci_xfer *)xfer;

	KASSERT(xp->free_trbs >= 1);

	/* Associate this TRB to our xfer. */
	xp->pending_xfers[xp->ring.index] = xfer;
	xp->free_trbs--;

	xx->index = (last) ? xp->ring.index : -2;
	xx->ntrb += 1;

	*togglep = xp->ring.toggle;
	return (xhci_ring_dequeue(sc, &xp->ring, 0));
}

int
xhci_command_submit(struct xhci_softc *sc, struct xhci_trb *trb0, int timeout)
{
	struct xhci_trb *trb;
	int s, error = 0;

	KASSERT(timeout == 0 || sc->sc_cmd_trb == NULL);

	trb0->trb_flags |= htole32(sc->sc_cmd_ring.toggle);

	trb = xhci_ring_dequeue(sc, &sc->sc_cmd_ring, 0);
	if (trb == NULL)
		return (EAGAIN);
	memcpy(trb, trb0, sizeof(struct xhci_trb));
	bus_dmamap_sync(sc->sc_cmd_ring.dma.tag, sc->sc_cmd_ring.dma.map,
	    TRBOFF(sc->sc_cmd_ring, trb), sizeof(struct xhci_trb),
	    BUS_DMASYNC_PREWRITE);


	if (timeout == 0) {
		XDWRITE4(sc, XHCI_DOORBELL(0), 0);
		return (0);
	}

	assertwaitok();

	s = splusb();
	sc->sc_cmd_trb = trb;
	XDWRITE4(sc, XHCI_DOORBELL(0), 0);
	error = tsleep(&sc->sc_cmd_trb, PZERO, "xhcicmd",
	    (timeout*hz+999)/ 1000 + 1);
	if (error) {
#ifdef XHCI_DEBUG
		printf("%s: tsleep() = %d\n", __func__, error);
		printf("cmd = %d ", XHCI_TRB_TYPE(letoh32(trb->trb_flags)));
		xhci_dump_trb(trb);
#endif
		KASSERT(sc->sc_cmd_trb == trb);
		sc->sc_cmd_trb = NULL;
		splx(s);
		return (error);
	}
	splx(s);

	memcpy(trb0, &sc->sc_result_trb, sizeof(struct xhci_trb));

	if (XHCI_TRB_GET_CODE(letoh32(trb0->trb_status)) != XHCI_CODE_SUCCESS) {
		printf("%s: event error code=%d\n", DEVNAME(sc),
		    XHCI_TRB_GET_CODE(letoh32(trb0->trb_status)));
		error = EIO;
	}

#ifdef XHCI_DEBUG
	if (error) {
		printf("result = %d ", XHCI_TRB_TYPE(letoh32(trb0->trb_flags)));
		xhci_dump_trb(trb0);
	}
#endif
	return (error);
}

int
xhci_command_abort(struct xhci_softc *sc)
{
	uint32_t reg;
	int i;

	reg = XOREAD4(sc, XHCI_CRCR_LO);
	if ((reg & XHCI_CRCR_LO_CRR) == 0)
		return (0);

	XOWRITE4(sc, XHCI_CRCR_LO, reg | XHCI_CRCR_LO_CA);
	XOWRITE4(sc, XHCI_CRCR_HI, 0);

	for (i = 0; i < 250; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		reg = XOREAD4(sc, XHCI_CRCR_LO) & XHCI_CRCR_LO_CRR;
		if (!reg)
			break;
	}

	if (reg) {
		printf("%s: command ring abort timeout\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
xhci_cmd_configure_ep(struct xhci_softc *sc, uint8_t slot, uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s dev %u\n", DEVNAME(sc), __func__, slot));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_CMD_CONFIG_EP
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

int
xhci_cmd_stop_ep(struct xhci_softc *sc, uint8_t slot, uint8_t dci)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s dev %u dci %u\n", DEVNAME(sc), __func__, slot, dci));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_STOP_EP
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

void
xhci_cmd_reset_ep_async(struct xhci_softc *sc, uint8_t slot, uint8_t dci)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s dev %u dci %u\n", DEVNAME(sc), __func__, slot, dci));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_RESET_EP
	);

	xhci_command_submit(sc, &trb, 0);
}

void
xhci_cmd_set_tr_deq_async(struct xhci_softc *sc, uint8_t slot, uint8_t dci,
   uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s dev %u dci %u\n", DEVNAME(sc), __func__, slot, dci));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_SET_TR_DEQ
	);

	xhci_command_submit(sc, &trb, 0);
}

int
xhci_cmd_slot_control(struct xhci_softc *sc, uint8_t *slotp, int enable)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	if (enable)
		trb.trb_flags = htole32(XHCI_CMD_ENABLE_SLOT);
	else
		trb.trb_flags = htole32(
			XHCI_TRB_SET_SLOT(*slotp) | XHCI_CMD_DISABLE_SLOT
		);

	if (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT))
		return (EIO);

	if (enable)
		*slotp = XHCI_TRB_GET_SLOT(letoh32(trb.trb_flags));

	return (0);
}

int
xhci_cmd_set_address(struct xhci_softc *sc, uint8_t slot, uint64_t addr,
    uint32_t bsr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s BSR=%u\n", DEVNAME(sc), __func__, bsr ? 1 : 0));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_CMD_ADDRESS_DEVICE | bsr
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

int
xhci_cmd_evaluate_ctx(struct xhci_softc *sc, uint8_t slot, uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s dev %u\n", DEVNAME(sc), __func__, slot));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_CMD_EVAL_CTX
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

#ifdef XHCI_DEBUG
int
xhci_cmd_noop(struct xhci_softc *sc)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(XHCI_CMD_NOOP);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}
#endif

int
xhci_softdev_alloc(struct xhci_softc *sc, uint8_t slot)
{
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[slot];
	int i, error;
	uint8_t *kva;

	/*
	 * Setup input context.  Even with 64 byte context size, it
	 * fits into the smallest supported page size, so use that.
	 */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sdev->ictx_dma,
	    (void **)&kva, sc->sc_pagesize, XHCI_ICTX_ALIGN, sc->sc_pagesize);
	if (error)
		return (ENOMEM);

	sdev->input_ctx = (struct xhci_inctx *)kva;
	sdev->slot_ctx = (struct xhci_sctx *)(kva + sc->sc_ctxsize);
	for (i = 0; i < 31; i++)
		sdev->ep_ctx[i] =
		    (struct xhci_epctx *)(kva + (i + 2) * sc->sc_ctxsize);

	DPRINTF(("%s: dev %d, input=%p slot=%p ep0=%p\n", DEVNAME(sc),
	 slot, sdev->input_ctx, sdev->slot_ctx, sdev->ep_ctx[0]));

	/* Setup output context */
	error = usbd_dma_contig_alloc(&sc->sc_bus, &sdev->octx_dma, NULL,
	    sc->sc_pagesize, XHCI_OCTX_ALIGN, sc->sc_pagesize);
	if (error) {
		usbd_dma_contig_free(&sc->sc_bus, &sdev->ictx_dma);
		return (ENOMEM);
	}

	memset(&sdev->pipes, 0, sizeof(sdev->pipes));

	DPRINTF(("%s: dev %d, setting DCBAA to 0x%016llx\n", DEVNAME(sc),
	    slot, (long long)sdev->octx_dma.paddr));

	sc->sc_dcbaa.segs[slot] = htole64(sdev->octx_dma.paddr);
	bus_dmamap_sync(sc->sc_dcbaa.dma.tag, sc->sc_dcbaa.dma.map,
	    slot * sizeof(uint64_t), sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	return (0);
}

void
xhci_softdev_free(struct xhci_softc *sc, uint8_t slot)
{
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[slot];

	sc->sc_dcbaa.segs[slot] = 0;
	bus_dmamap_sync(sc->sc_dcbaa.dma.tag, sc->sc_dcbaa.dma.map,
	    slot * sizeof(uint64_t), sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	usbd_dma_contig_free(&sc->sc_bus, &sdev->octx_dma);
	usbd_dma_contig_free(&sc->sc_bus, &sdev->ictx_dma);

	memset(sdev, 0, sizeof(struct xhci_soft_dev));
}

/* Root hub descriptors. */
usb_device_descriptor_t xhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x03},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	9,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indexes */
	1			/* # of configurations */
};

const usb_config_descriptor_t xhci_confd = {
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

const usb_interface_descriptor_t xhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,
	0
};

const usb_endpoint_descriptor_t xhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | XHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{2, 0},                 /* max 15 ports */
	255
};

const usb_endpoint_ss_comp_descriptor_t xhci_endpcd = {
	USB_ENDPOINT_SS_COMP_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT_SS_COMP,
	0,
	0,
	{0, 0}
};

const usb_hub_descriptor_t xhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_SS_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

void
xhci_abort_xfer(struct usbd_xfer *xfer, usbd_status status)
{
	splsoftassert(IPL_SOFTUSB);

	DPRINTF(("%s: xfer=%p status=%s err=%s actlen=%d len=%d index=%d\n",
	    __func__, xfer, usbd_errstr(xfer->status), usbd_errstr(status),
	    xfer->actlen, xfer->length, ((struct xhci_xfer *)xfer)->index));

	xfer->status = status;
	xhci_xfer_done(xfer);
}

void
xhci_timeout(void *addr)
{
	struct usbd_xfer *xfer = addr;
	int s;

	s = splusb();
	xhci_abort_xfer(xfer, USBD_TIMEOUT);
	splx(s);
}

usbd_status
xhci_root_ctrl_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_root_ctrl_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	usb_port_status_t ps;
	usb_device_request_t *req;
	void *buf = NULL;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	int s, len, value, index;
	int l, totlen = 0;
	int port, i;
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
		DPRINTFN(8,("xhci_root_ctrl_start: wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(xhci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &xhci_devd, l);
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
			memcpy(buf, &xhci_confd, l);
			((usb_config_descriptor_t *)buf)->bDescriptorType =
			    value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_endpd, l);
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
				totlen = usbd_str(buf, len, "xHCI root hub");
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
		DPRINTFN(8, ("xhci_root_ctrl_start: UR_CLEAR_PORT_FEATURE "
		    "port=%d feature=%d\n", index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = XHCI_PORTSC(index);
		v = XOREAD4(sc, port) & ~XHCI_PS_CLEAR;
		switch (value) {
		case UHF_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			/* TODO */
			break;
		case UHF_PORT_POWER:
			XOWRITE4(sc, port, v & ~XHCI_PS_PP);
			break;
		case UHF_PORT_INDICATOR:
			XOWRITE4(sc, port, v & ~XHCI_PS_SET_PIC(3));
			break;
		case UHF_C_PORT_CONNECTION:
			XOWRITE4(sc, port, v | XHCI_PS_CSC);
			break;
		case UHF_C_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PEC);
			break;
		case UHF_C_PORT_SUSPEND:
			XOWRITE4(sc, port, v | XHCI_PS_PLC);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			XOWRITE4(sc, port, v | XHCI_PS_OCC);
			break;
		case UHF_C_PORT_RESET:
			XOWRITE4(sc, port, v | XHCI_PS_PRC);
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
		v = XREAD4(sc, XHCI_HCCPARAMS);
		hubd = xhci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		USETW(hubd.wHubCharacteristics,
		    (XHCI_HCC_PPC(v) ? UHD_PWR_INDIVIDUAL : UHD_PWR_GANGED) |
		    (XHCI_HCC_PIND(v) ? UHD_PORT_IND : 0));
		hubd.bPwrOn2PwrGood = 10; /* xHCI section 5.4.9 */
		for (i = 1; i <= sc->sc_noport; i++) {
			v = XOREAD4(sc, XHCI_PORTSC(i));
			if (v & XHCI_PS_DR)
				hubd.DeviceRemovable[i / 8] |= 1U << (i % 8);
		}
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
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
		DPRINTFN(8,("xhci_root_ctrl_start: get port status i=%d\n",
		    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = XOREAD4(sc, XHCI_PORTSC(index));
		DPRINTFN(8,("xhci_root_ctrl_start: port status=0x%04x\n", v));
		switch (XHCI_PS_SPEED(v)) {
		case XHCI_SPEED_FULL:
			i = UPS_FULL_SPEED;
			break;
		case XHCI_SPEED_LOW:
			i = UPS_LOW_SPEED;
			break;
		case XHCI_SPEED_HIGH:
			i = UPS_HIGH_SPEED;
			break;
		case XHCI_SPEED_SUPER:
		default:
			i = UPS_SUPER_SPEED;
			break;
		}
		if (v & XHCI_PS_CCS)	i |= UPS_CURRENT_CONNECT_STATUS;
		if (v & XHCI_PS_PED)	i |= UPS_PORT_ENABLED;
		if (v & XHCI_PS_OCA)	i |= UPS_OVERCURRENT_INDICATOR;
		if (v & XHCI_PS_PR)	i |= UPS_RESET;
		if (v & XHCI_PS_PP)	i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);
		i = 0;
		if (v & XHCI_PS_CSC)    i |= UPS_C_CONNECT_STATUS;
		if (v & XHCI_PS_PEC)    i |= UPS_C_PORT_ENABLED;
		if (v & XHCI_PS_OCC)    i |= UPS_C_OVERCURRENT_INDICATOR;
		if (v & XHCI_PS_PRC)	i |= UPS_C_PORT_RESET;
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
		port = XHCI_PORTSC(index);
		v = XOREAD4(sc, port) & ~XHCI_PS_CLEAR;

		switch (value) {
		case UHF_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			DPRINTFN(6, ("suspend port %u (LPM=%u)\n", index, i));
			if (XHCI_PS_SPEED(v) == XHCI_SPEED_SUPER) {
				err = USBD_IOERROR;
				goto ret;
			}
			XOWRITE4(sc, port, v |
			    XHCI_PS_SET_PLS(i ? 2 /* LPM */ : 3) | XHCI_PS_LWS);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(6, ("reset port %d\n", index));
			XOWRITE4(sc, port, v | XHCI_PS_PR);
			break;
		case UHF_PORT_POWER:
			DPRINTFN(3, ("set port power %d\n", index));
			XOWRITE4(sc, port, v | XHCI_PS_PP);
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(3, ("set port indicator %d\n", index));

			v &= ~XHCI_PS_SET_PIC(3);
			v |= XHCI_PS_SET_PIC(1);

			XOWRITE4(sc, port, v);
			break;
		case UHF_C_PORT_RESET:
			XOWRITE4(sc, port, v | XHCI_PS_PRC);
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
xhci_noop(struct usbd_xfer *xfer)
{
}


usbd_status
xhci_root_intr_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_root_intr_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

void
xhci_root_intr_abort(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	int s;

	sc->sc_intrxfer = NULL;

	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

void
xhci_root_intr_done(struct usbd_xfer *xfer)
{
}

/* Number of packets remaining in the TD after the corresponding TRB. */
static inline uint32_t
xhci_xfer_tdsize(struct usbd_xfer *xfer, uint32_t remain, uint32_t len)
{
	uint32_t npkt, mps = UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize);

	if (len == 0)
		return XHCI_TRB_TDREM(0);

	npkt = (remain - len) / mps;
	if (npkt > 31)
		npkt = 31;

	return XHCI_TRB_TDREM(npkt);
}

usbd_status
xhci_device_ctrl_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_device_ctrl_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_trb *trb0, *trb;
	uint32_t flags, len = UGETW(xfer->request.wLength);
	uint8_t toggle0, toggle;
	int s;

	KASSERT(xfer->rqflags & URQ_REQUEST);

	if (sc->sc_bus.dying || xp->halted)
		return (USBD_IOERROR);

	if (xp->free_trbs < 3)
		return (USBD_NOMEM);

	/* We'll do the setup TRB once we're finished with the other stages. */
	trb0 = xhci_xfer_get_trb(sc, xfer, &toggle0, 0);

	/* Data TRB */
	if (len != 0) {
		trb = xhci_xfer_get_trb(sc, xfer, &toggle, 0);

		flags = XHCI_TRB_TYPE_DATA | toggle;
		if (usbd_xfer_isread(xfer))
			flags |= XHCI_TRB_DIR_IN | XHCI_TRB_ISP;

		trb->trb_paddr = htole64(DMAADDR(&xfer->dmabuf, 0));
		trb->trb_status = htole32(
		    XHCI_TRB_INTR(0) | XHCI_TRB_LEN(len) |
		    xhci_xfer_tdsize(xfer, len, len)
		);
		trb->trb_flags = htole32(flags);

	}

	/* Status TRB */
	trb = xhci_xfer_get_trb(sc, xfer, &toggle, 1);

	flags = XHCI_TRB_TYPE_STATUS | XHCI_TRB_IOC | toggle;
	if (len == 0 || !usbd_xfer_isread(xfer))
		flags |= XHCI_TRB_DIR_IN;

	trb->trb_paddr = 0;
	trb->trb_status = htole32(XHCI_TRB_INTR(0));
	trb->trb_flags = htole32(flags);

	/* Setup TRB */
	flags = XHCI_TRB_TYPE_SETUP | XHCI_TRB_IDT | toggle0;
	if (len != 0) {
		if (usbd_xfer_isread(xfer))
			flags |= XHCI_TRB_TRT_IN;
		else
			flags |= XHCI_TRB_TRT_OUT;
	}

	trb0->trb_paddr = (uint64_t)*((uint64_t *)&xfer->request);
	trb0->trb_status = htole32(XHCI_TRB_INTR(0) | XHCI_TRB_LEN(8));
	trb0->trb_flags = htole32(flags);

	bus_dmamap_sync(xp->ring.dma.tag, xp->ring.dma.map,
	    TRBOFF(xp->ring, trb0), 3 * sizeof(struct xhci_trb),
	    BUS_DMASYNC_PREWRITE);

	s = splusb();
	XDWRITE4(sc, XHCI_DOORBELL(xp->slot), xp->dci);

	xfer->status = USBD_IN_PROGRESS;

	if (sc->sc_bus.use_polling)
		xhci_waitintr(sc, xfer);
	else if (xfer->timeout) {
		timeout_del(&xfer->timeout_handle);
		timeout_set(&xfer->timeout_handle, xhci_timeout, xfer);
		timeout_add_msec(&xfer->timeout_handle, xfer->timeout);
	}
	splx(s);

	return (USBD_IN_PROGRESS);
}

void
xhci_device_ctrl_abort(struct usbd_xfer *xfer)
{
	xhci_abort_xfer(xfer, USBD_CANCELLED);
}

usbd_status
xhci_device_generic_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_device_generic_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_device_generic_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_trb *trb0, *trb;
	uint32_t len, remain, flags;
	uint32_t len0, mps;
	uint64_t paddr = DMAADDR(&xfer->dmabuf, 0);
	uint8_t toggle0, toggle;
	int s, i, ntrb;

	KASSERT(!(xfer->rqflags & URQ_REQUEST));

	if (sc->sc_bus.dying || xp->halted)
		return (USBD_IOERROR);

	/* How many TRBs do we need for this transfer? */
	ntrb = (xfer->length + XHCI_TRB_MAXSIZE - 1) / XHCI_TRB_MAXSIZE;

	/* If the buffer crosses a 64k boundary, we need one more. */
	len0 = XHCI_TRB_MAXSIZE - (paddr & (XHCI_TRB_MAXSIZE - 1));
	if (len0 < xfer->length)
		ntrb++;
	else
		len0 = xfer->length;

	/* If we need to append a zero length packet, we need one more. */
	mps = UGETW(xfer->pipe->endpoint->edesc->wMaxPacketSize);
	if ((xfer->flags & USBD_FORCE_SHORT_XFER || xfer->length == 0) &&
	    (xfer->length % mps == 0))
		ntrb++;

	if (xp->free_trbs < ntrb)
		return (USBD_NOMEM);

	/* We'll do the first TRB once we're finished with the chain. */
	trb0 = xhci_xfer_get_trb(sc, xfer, &toggle0, (ntrb == 1));

	remain = xfer->length - len0;
	paddr += len0;
	len = min(remain, XHCI_TRB_MAXSIZE);

	/* Chain more TRBs if needed. */
	for (i = ntrb - 1; i > 0; i--) {
		/* Next (or Last) TRB. */
		trb = xhci_xfer_get_trb(sc, xfer, &toggle, (i == 1));
		flags = XHCI_TRB_TYPE_NORMAL | toggle;
		if (usbd_xfer_isread(xfer))
			flags |= XHCI_TRB_ISP;
		flags |= (i == 1) ? XHCI_TRB_IOC : XHCI_TRB_CHAIN;

		trb->trb_paddr = htole64(paddr);
		trb->trb_status = htole32(
		    XHCI_TRB_INTR(0) | XHCI_TRB_LEN(len) |
		    xhci_xfer_tdsize(xfer, remain, len)
		);
		trb->trb_flags = htole32(flags);

		remain -= len;
		paddr += len;
		len = min(remain, XHCI_TRB_MAXSIZE);
	}

	/* First TRB. */
	flags = XHCI_TRB_TYPE_NORMAL | toggle0;
	if (usbd_xfer_isread(xfer))
		flags |= XHCI_TRB_ISP;
	flags |= (ntrb == 1) ? XHCI_TRB_IOC : XHCI_TRB_CHAIN;

	trb0->trb_paddr = htole64(DMAADDR(&xfer->dmabuf, 0));
	trb0->trb_status = htole32(
	    XHCI_TRB_INTR(0) | XHCI_TRB_LEN(len0) |
	    xhci_xfer_tdsize(xfer, xfer->length, len0)
 	);
	trb0->trb_flags = htole32(flags);

	bus_dmamap_sync(xp->ring.dma.tag, xp->ring.dma.map,
	    TRBOFF(xp->ring, trb0), sizeof(struct xhci_trb) * ntrb,
	    BUS_DMASYNC_PREWRITE);

	s = splusb();
	XDWRITE4(sc, XHCI_DOORBELL(xp->slot), xp->dci);

	xfer->status = USBD_IN_PROGRESS;

	if (sc->sc_bus.use_polling)
		xhci_waitintr(sc, xfer);
	else if (xfer->timeout) {
		timeout_del(&xfer->timeout_handle);
		timeout_set(&xfer->timeout_handle, xhci_timeout, xfer);
		timeout_add_msec(&xfer->timeout_handle, xfer->timeout);
	}
	splx(s);

	return (USBD_IN_PROGRESS);
}

void
xhci_device_generic_done(struct usbd_xfer *xfer)
{
	usb_syncmem(&xfer->dmabuf, 0, xfer->length, usbd_xfer_isread(xfer) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	/* Only happens with interrupt transfers. */
	if (xfer->pipe->repeat) {
		xfer->actlen = 0;
		xhci_device_generic_start(xfer);
	}
}

void
xhci_device_generic_abort(struct usbd_xfer *xfer)
{
	KASSERT(!xfer->pipe->repeat || xfer->pipe->intrxfer == xfer);

	xhci_abort_xfer(xfer, USBD_CANCELLED);
}
