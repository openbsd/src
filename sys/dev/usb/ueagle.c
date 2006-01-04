/*	$OpenBSD: ueagle.c,v 1.8 2006/01/04 06:04:41 canacar Exp $	*/

/*-
 * Copyright (c) 2003-2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

/*-
 * Analog Devices Eagle chipset driver
 * http://www.analog.com/
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kthread.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_atm.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_atm.h>
#include <netinet/if_ether.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/ezload.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ueaglereg.h>
#include <dev/usb/ueaglevar.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (ueagledebug > 0) logprintf x; } while (0)
#define DPRINTFN(n, x)	do { if (ueagledebug >= (n)) logprintf x; } while (0)
int ueagledebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* various supported device vendors/products */
static const struct ueagle_type {
	struct usb_devno	dev;
	const char		*fw;
} ueagle_devs[] = {
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEI },      NULL },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEI_NF },   "ueagleI" },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEII },     NULL },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEII_NF },  "ueagleII" },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEIIC },    NULL },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEIIC_NF }, "ueagleII" },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEIII },    NULL },
  { { USB_VENDOR_ANALOG, USB_PRODUCT_ANALOG_EAGLEIII_NF }, "ueagleIII" },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_HEINEKEN_A },     NULL },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_HEINEKEN_A_NF },  "ueagleI" },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_HEINEKEN_B },     NULL },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_HEINEKEN_B_NF },  "ueagleI" },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_MILLER_A },       NULL },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_MILLER_A_NF },    "ueagleI" },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_MILLER_B },       NULL },
  { { USB_VENDOR_USR,    USB_PRODUCT_USR_MILLER_B_NF },    "ueagleI" }
};
#define ueagle_lookup(v, p)	\
	((struct ueagle_type *)usb_lookup(ueagle_devs, v, p))

Static void	ueagle_attachhook(void *);
Static int	ueagle_getesi(struct ueagle_softc *, uint8_t *);
Static void	ueagle_loadpage(void *);
Static void	ueagle_request(struct ueagle_softc *, uint16_t, uint16_t,
		    void *, int);
#ifdef USB_DEBUG
Static void	ueagle_dump_cmv(struct ueagle_softc *, struct ueagle_cmv *);
#endif
Static int	ueagle_cr(struct ueagle_softc *, uint32_t, uint16_t,
		    uint32_t *);
Static int	ueagle_cw(struct ueagle_softc *, uint32_t, uint16_t, uint32_t);
Static int	ueagle_stat(struct ueagle_softc *);
Static void	ueagle_stat_thread(void *);
Static int	ueagle_boot(struct ueagle_softc *);
Static void	ueagle_swap_intr(struct ueagle_softc *, struct ueagle_swap *);
Static void	ueagle_cmv_intr(struct ueagle_softc *, struct ueagle_cmv *);
Static void	ueagle_intr(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static uint32_t	ueagle_crc_update(uint32_t, uint8_t *, int);
Static void	ueagle_push_cell(struct ueagle_softc *, uint8_t *);
Static void	ueagle_rxeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static void	ueagle_txeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
Static int	ueagle_encap(struct ueagle_softc *, struct mbuf *);
Static void	ueagle_start(struct ifnet *);
Static int	ueagle_open_vcc(struct ueagle_softc *,
		    struct atm_pseudoioctl *);
Static int	ueagle_close_vcc(struct ueagle_softc *,
		    struct atm_pseudoioctl *);
Static int	ueagle_ioctl(struct ifnet *, u_long, caddr_t);
Static int	ueagle_open_pipes(struct ueagle_softc *);
Static void	ueagle_close_pipes(struct ueagle_softc *);
Static int	ueagle_init(struct ifnet *);
Static void	ueagle_stop(struct ifnet *, int);

USB_DECLARE_DRIVER(ueagle);

USB_MATCH(ueagle)
{
	USB_MATCH_START(ueagle, uaa);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (ueagle_lookup(uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

Static void
ueagle_attachhook(void *xsc)
{
	char *firmwares[2];
	struct ueagle_softc *sc = xsc;

	firmwares[0] = (char *)sc->fw;
	firmwares[1] = NULL;

	if (ezload_downloads_and_reset(sc->sc_udev, firmwares) != 0) {
		printf("%s: could not download firmware\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}
}

USB_ATTACH(ueagle)
{
	USB_ATTACH_START(ueagle, sc, uaa);
	struct ifnet *ifp = &sc->sc_if;
	char *devinfop;
	uint8_t addr[ETHER_ADDR_LEN];

	sc->sc_udev = uaa->device;
	USB_ATTACH_SETUP;

	/*
	 * Pre-firmware modems must be flashed and reset first.  They will
	 * automatically detach themselves from the bus and reattach later
	 * with a new product Id.
	 */
	sc->fw = ueagle_lookup(uaa->vendor, uaa->product)->fw;
	if (sc->fw != NULL) {
		if (rootvp == NULL)
			mountroothook_establish(ueagle_attachhook, sc);
		else
			ueagle_attachhook(sc);

		/* processing of pre-firmware modems ends here */
		USB_ATTACH_SUCCESS_RETURN;
	}

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	if (usbd_set_config_no(sc->sc_udev, UEAGLE_CONFIG_NO, 0) != 0) {
		printf("%s: could not set configuration no\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	if (ueagle_getesi(sc, addr) != 0) {
		printf("%s: could not read end system identifier\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	printf("%s: address: %02x:%02x:%02x:%02x:%02x:%02x\n",
	    USBDEVNAME(sc->sc_dev), addr[0], addr[1], addr[2], addr[3],
	    addr[4], addr[5]);

	usb_init_task(&sc->sc_swap_task, ueagle_loadpage, sc);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX;
	ifp->if_init = ueagle_init;
	ifp->if_ioctl = ueagle_ioctl;
	ifp->if_start = ueagle_start;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, USBDEVNAME(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	atm_ifattach(ifp);

	/* override default MTU value (9180 is too large for us) */
	ifp->if_mtu = UEAGLE_IFMTU;

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_RAW, 0);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(ueagle)
{
	USB_DETACH_START(ueagle, sc);
	struct ifnet *ifp = &sc->sc_if;

	if (sc->fw != NULL)
		return 0; /* shortcut for pre-firmware devices */

	sc->gone = 1;
	ueagle_stop(ifp, 1);

	/* wait for stat thread to exit properly */
	if (sc->stat_thread != NULL) {
		DPRINTFN(3, ("%s: waiting for stat thread to exit\n",
		    USBDEVNAME(sc->sc_dev)));

		tsleep(sc->stat_thread, PZERO, "ueaglestat", 0);

		DPRINTFN(3, ("%s: stat thread exited properly\n",
		    USBDEVNAME(sc->sc_dev)));
	}

	if_detach(ifp);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return 0;
}

/*
 * Retrieve the device End System Identifier (MAC address).
 */
Static int
ueagle_getesi(struct ueagle_softc *sc, uint8_t *addr)
{
	usb_string_descriptor_t us;
	usbd_status error;
	uint16_t c;
	int i, len;

	error = usbd_get_string_desc(sc->sc_udev, UEAGLE_ESISTR, 0, &us, &len);
	if (error != 0)
		return error;

	if (us.bLength < (6 + 1) * 2)
		return 1;

	for (i = 0; i < 6 * 2; i++) {
		if ((c = UGETW(us.bString[i])) & 0xff00)
			return 1;	/* not 8-bit clean */

		if (i & 1)
			addr[i / 2] <<= 4;
		else
			addr[i / 2] = 0;

		if (c >= '0' && c <= '9')
			addr[i / 2] |= c - '0';
		else if (c >= 'a' && c <= 'f')
			addr[i / 2] |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			addr[i / 2] |= c - 'A' + 10;
		else
			return 1;
	}

	return 0;
}

Static void
ueagle_loadpage(void *xsc)
{
	struct ueagle_softc *sc = xsc;
	usbd_xfer_handle xfer;
	struct ueagle_block_info bi;
	uint16_t pageno = sc->pageno;
	uint16_t ovl = sc->ovl;
	uint8_t pagecount, blockcount;
	uint16_t blockaddr, blocksize;
	uint32_t pageoffset;
	uint8_t *p;
	int i;

	p = sc->dsp;
	pagecount = *p++;

	if (pageno >= pagecount) {
		printf("%s: invalid page number %u requested\n",
		    USBDEVNAME(sc->sc_dev), pageno);
		return;
	}

	p += 4 * pageno;
	pageoffset = UGETDW(p);
	if (pageoffset == 0)
		return;

	p = sc->dsp + pageoffset;
	blockcount = *p++;

	DPRINTF(("%s: sending %u blocks for fw page %u\n",
	    USBDEVNAME(sc->sc_dev), blockcount, pageno));

	if ((xfer = usbd_alloc_xfer(sc->sc_udev)) == NULL) {
		printf("%s: could not allocate xfer\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}

	USETW(bi.wHdr, UEAGLE_BLOCK_INFO_HDR);
	USETW(bi.wOvl, ovl);
	USETW(bi.wOvlOffset, ovl | 0x8000);

	for (i = 0; i < blockcount; i++) {
		blockaddr = UGETW(p); p += 2;
		blocksize = UGETW(p); p += 2;

		USETW(bi.wSize, blocksize);
		USETW(bi.wAddress, blockaddr);
		USETW(bi.wLast, (i == blockcount - 1) ? 1 : 0);

		/* send block info through the IDMA pipe */
		usbd_setup_xfer(xfer, sc->pipeh_idma, sc, &bi, sizeof bi, 0,
		    UEAGLE_IDMA_TIMEOUT, NULL);
		if (usbd_sync_transfer(xfer) != 0) {
			printf("%s: could not transfer block info\n",
			    USBDEVNAME(sc->sc_dev));
			break;
		}

		/* send block data through the IDMA pipe */
		usbd_setup_xfer(xfer, sc->pipeh_idma, sc, p, blocksize, 0,
		    UEAGLE_IDMA_TIMEOUT, NULL);
		if (usbd_sync_transfer(xfer) != 0) {
			printf("%s: could not transfer block data\n",
			    USBDEVNAME(sc->sc_dev));
			break;
		}

		p += blocksize;
	}

	usbd_free_xfer(xfer);
}

Static void
ueagle_request(struct ueagle_softc *sc, uint16_t val, uint16_t index,
    void *data, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UEAGLE_REQUEST;
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, len);

	error = usbd_do_request_async(sc->sc_udev, &req, data);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS)
		printf("%s: could not send request\n", USBDEVNAME(sc->sc_dev));
}

#ifdef USB_DEBUG
Static void
ueagle_dump_cmv(struct ueagle_softc *sc, struct ueagle_cmv *cmv)
{
	printf("    Preamble:    0x%04x\n", UGETW(cmv->wPreamble));
	printf("    Destination: %s (0x%02x)\n",
	    (cmv->bDst == UEAGLE_HOST) ? "Host" : "Modem", cmv->bDst);
	printf("    Type:        %u\n", cmv->bFunction >> 4);
	printf("    Subtype:     %u\n", cmv->bFunction & 0xf);
	printf("    Index:       %u\n", UGETW(cmv->wIndex));
	printf("    Address:     %c%c%c%c.%u\n",
	    cmv->dwSymbolicAddress[1], cmv->dwSymbolicAddress[0],
	    cmv->dwSymbolicAddress[3], cmv->dwSymbolicAddress[2],
	    UGETW(cmv->wOffsetAddress));
	printf("    Data:        0x%08x\n", UGETDATA(cmv->dwData));
}
#endif

Static int
ueagle_cr(struct ueagle_softc *sc, uint32_t address, uint16_t offset,
    uint32_t *data)
{
	struct ueagle_cmv cmv;
	usbd_status error;
	int s;

	USETW(cmv.wPreamble, UEAGLE_CMV_PREAMBLE);
	cmv.bDst = UEAGLE_MODEM;
	cmv.bFunction = UEAGLE_CR;
	USETW(cmv.wIndex, sc->index);
	USETW(cmv.wOffsetAddress, offset);
	USETDW(cmv.dwSymbolicAddress, address);
	USETDATA(cmv.dwData, 0);

#ifdef USB_DEBUG
	if (ueagledebug >= 15) {
		printf("%s: reading CMV\n", USBDEVNAME(sc->sc_dev));
		ueagle_dump_cmv(sc, &cmv);
	}
#endif

	s = splusb();

	ueagle_request(sc, UEAGLE_SETBLOCK, UEAGLE_MPTXSTART, &cmv, sizeof cmv);

	/* wait at most 2 seconds for an answer */
	error = tsleep(UEAGLE_COND_CMV(sc), PZERO, "cmv", 2 * hz);
	if (error != 0) {
		printf("%s: timeout waiting for CMV ack\n",
		    USBDEVNAME(sc->sc_dev));
		splx(s);
		return error;
	}

	*data = sc->data;
	splx(s);

	return 0;
}

Static int
ueagle_cw(struct ueagle_softc *sc, uint32_t address, uint16_t offset,
    uint32_t data)
{
	struct ueagle_cmv cmv;
	usbd_status error;
	int s;

	USETW(cmv.wPreamble, UEAGLE_CMV_PREAMBLE);
	cmv.bDst = UEAGLE_MODEM;
	cmv.bFunction = UEAGLE_CW;
	USETW(cmv.wIndex, sc->index);
	USETW(cmv.wOffsetAddress, offset);
	USETDW(cmv.dwSymbolicAddress, address);
	USETDATA(cmv.dwData, data);

#ifdef USB_DEBUG
	if (ueagledebug >= 15) {
		printf("%s: writing CMV\n", USBDEVNAME(sc->sc_dev));
		ueagle_dump_cmv(sc, &cmv);
	}
#endif

	s = splusb();

	ueagle_request(sc, UEAGLE_SETBLOCK, UEAGLE_MPTXSTART, &cmv, sizeof cmv);

	/* wait at most 2 seconds for an answer */
	error = tsleep(UEAGLE_COND_CMV(sc), PZERO, "cmv", 2 * hz);
	if (error != 0) {
		printf("%s: timeout waiting for CMV ack\n",
		    USBDEVNAME(sc->sc_dev));
		splx(s);
		return error;
	}

	splx(s);

	return 0;
}

Static int
ueagle_stat(struct ueagle_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	uint32_t data;
	usbd_status error;
#define CR(sc, address, offset, data) do {				\
	if ((error = ueagle_cr(sc, address, offset, data)) != 0)	\
		return error;						\
} while (0)

	CR(sc, UEAGLE_CMV_STAT, 0, &sc->stats.phy.status);
	switch ((sc->stats.phy.status >> 8) & 0xf) {
	case 0: /* idle */
		DPRINTFN(3, ("%s: waiting for synchronization\n",
		    USBDEVNAME(sc->sc_dev)));
		return ueagle_cw(sc, UEAGLE_CMV_CNTL, 0, 2);

	case 1: /* initialization */
		DPRINTFN(3, ("%s: initializing\n", USBDEVNAME(sc->sc_dev)));
		return ueagle_cw(sc, UEAGLE_CMV_CNTL, 0, 2);

	case 2: /* operational */
		DPRINTFN(4, ("%s: operational\n", USBDEVNAME(sc->sc_dev)));
		break;

	default: /* fail ... */
		DPRINTFN(3, ("%s: synchronization failed\n",
		    USBDEVNAME(sc->sc_dev)));
		ueagle_init(ifp);
		return 1;
	}

	CR(sc, UEAGLE_CMV_DIAG, 1, &sc->stats.phy.flags);
	if (sc->stats.phy.flags & 0x10) {
		DPRINTF(("%s: delineation LOSS\n", USBDEVNAME(sc->sc_dev)));
		sc->stats.phy.status = 0;
		ueagle_init(ifp);
		return 1;
	}

	CR(sc, UEAGLE_CMV_RATE, 0, &data);
	sc->stats.phy.dsrate = ((data >> 16) & 0x1ff) * 32;
	sc->stats.phy.usrate = (data & 0xff) * 32;

	CR(sc, UEAGLE_CMV_DIAG, 23, &data);
	sc->stats.phy.attenuation = (data & 0xff) / 2;

	CR(sc, UEAGLE_CMV_DIAG,  3, &sc->stats.atm.cells_crc_errors);
	CR(sc, UEAGLE_CMV_DIAG, 22, &sc->stats.phy.dserror);
	CR(sc, UEAGLE_CMV_DIAG, 25, &sc->stats.phy.dsmargin);
	CR(sc, UEAGLE_CMV_DIAG, 46, &sc->stats.phy.userror);
	CR(sc, UEAGLE_CMV_DIAG, 49, &sc->stats.phy.usmargin);
	CR(sc, UEAGLE_CMV_DIAG, 51, &sc->stats.phy.rxflow);
	CR(sc, UEAGLE_CMV_DIAG, 52, &sc->stats.phy.txflow);
	CR(sc, UEAGLE_CMV_DIAG, 54, &sc->stats.phy.dsunc);
	CR(sc, UEAGLE_CMV_DIAG, 58, &sc->stats.phy.usunc);
	CR(sc, UEAGLE_CMV_INFO,  8, &sc->stats.phy.vidco);
	CR(sc, UEAGLE_CMV_INFO, 14, &sc->stats.phy.vidcpe);

	if (sc->pipeh_tx != NULL)
		return 0;

	return ueagle_open_pipes(sc);
#undef CR
}

Static void
ueagle_stat_thread(void *arg)
{
	struct ueagle_softc *sc = arg;

	for (;;) {
		if (ueagle_stat(sc) != 0)
			break;

		usbd_delay_ms(sc->sc_udev, 5000);
	}

	wakeup(sc->stat_thread);

	kthread_exit(0);
}

Static int
ueagle_boot(struct ueagle_softc *sc)
{
	uint16_t zero = 0; /* ;-) */
	usbd_status error;
#define CW(sc, address, offset, data) do {				\
	if ((error = ueagle_cw(sc, address, offset, data)) != 0)	\
		return error;						\
} while (0)

	ueagle_request(sc, UEAGLE_SETMODE, UEAGLE_BOOTIDMA, NULL, 0);
	ueagle_request(sc, UEAGLE_SETMODE, UEAGLE_STARTRESET, NULL, 0);

	usbd_delay_ms(sc->sc_udev, 200);

	ueagle_request(sc, UEAGLE_SETMODE, UEAGLE_ENDRESET, NULL, 0);
	ueagle_request(sc, UEAGLE_SET2183DATA, UEAGLE_MPTXMAILBOX, &zero, 2);
	ueagle_request(sc, UEAGLE_SET2183DATA, UEAGLE_MPRXMAILBOX, &zero, 2);
	ueagle_request(sc, UEAGLE_SET2183DATA, UEAGLE_SWAPMAILBOX, &zero, 2);

	usbd_delay_ms(sc->sc_udev, 1000);

	sc->pageno = 0;
	sc->ovl = 0;
	ueagle_loadpage(sc);

	/* wait until modem reaches operationnal state */
	error = tsleep(UEAGLE_COND_READY(sc), PZERO | PCATCH, "boot", 10 * hz);
	if (error != 0) {
		printf("%s: timeout waiting for operationnal state\n",
		    USBDEVNAME(sc->sc_dev));
		return error;
	}

	CW(sc, UEAGLE_CMV_CNTL, 0, 1);

	/* send configuration options */
	CW(sc, UEAGLE_CMV_OPTN, 0, UEAGLE_OPTN0);
	CW(sc, UEAGLE_CMV_OPTN, 2, UEAGLE_OPTN2);
	CW(sc, UEAGLE_CMV_OPTN, 7, UEAGLE_OPTN7);

	/* continue with synchronization */
	CW(sc, UEAGLE_CMV_CNTL, 0, 2);

	return kthread_create(ueagle_stat_thread, sc, &sc->stat_thread,
	    USBDEVNAME(sc->sc_dev));
#undef CW
}

Static void
ueagle_swap_intr(struct ueagle_softc *sc, struct ueagle_swap *swap)
{
#define rotbr(v, n)	((v) >> (n) | (v) << (8 - (n)))
	sc->pageno = swap->bPageNo;
	sc->ovl = rotbr(swap->bOvl, 4);

	usb_add_task(sc->sc_udev, &sc->sc_swap_task);
#undef rotbr
}

/*
 * This function handles spontaneous CMVs and CMV acknowledgements sent by the
 * modem on the interrupt pipe.
 */
Static void
ueagle_cmv_intr(struct ueagle_softc *sc, struct ueagle_cmv *cmv)
{
#ifdef USB_DEBUG
	if (ueagledebug >= 15) {
		printf("%s: receiving CMV\n", USBDEVNAME(sc->sc_dev));
		ueagle_dump_cmv(sc, cmv);
	}
#endif

	if (UGETW(cmv->wPreamble) != UEAGLE_CMV_PREAMBLE) {
		printf("%s: received CMV with invalid preamble\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}

	if (cmv->bDst != UEAGLE_HOST) {
		printf("%s: received CMV with bad direction\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}

	/* synchronize our current CMV index with the modem */
	sc->index = UGETW(cmv->wIndex) + 1;

	switch (cmv->bFunction) {
	case UEAGLE_MODEMREADY:
		wakeup(UEAGLE_COND_READY(sc));
		break;

	case UEAGLE_CR_ACK:
		sc->data = UGETDATA(cmv->dwData);
		/* FALLTHROUGH */
	case UEAGLE_CW_ACK:
		wakeup(UEAGLE_COND_CMV(sc));
		break;
	}
}

Static void
ueagle_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ueagle_softc *sc = priv;
	struct ueagle_intr *intr;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: abnormal interrupt status: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->pipeh_intr);

		return;
	}

	intr = (struct ueagle_intr *)sc->ibuf;
	switch (UGETW(intr->wInterrupt)) {
	case UEAGLE_INTR_SWAP:
		ueagle_swap_intr(sc, (struct ueagle_swap *)(intr + 1));
		break;

	case UEAGLE_INTR_CMV:
		ueagle_cmv_intr(sc, (struct ueagle_cmv *)(intr + 1));
		break;

	default:
		printf("%s: caught unknown interrupt\n",
		    USBDEVNAME(sc->sc_dev));
	}
}

static const uint32_t ueagle_crc32_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc,
	0x17c56b6b, 0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f,
	0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a,
	0x384fbdbd, 0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
	0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8,
	0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e,
	0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
	0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84,
	0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027,
	0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022,
	0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077,
	0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c,
	0x2e003dc5, 0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1,
	0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb,
	0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d,
	0x40d816ba, 0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
	0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f,
	0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044,
	0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689,
	0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683,
	0xd1799b34, 0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59,
	0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c,
	0x774bb0eb, 0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e,
	0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48,
	0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2,
	0xe6ea3d65, 0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601,
	0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604,
	0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6,
	0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad,
	0x81b02d74, 0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7,
	0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd,
	0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b,
	0x0fdc1bec, 0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
	0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679,
	0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12,
	0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af,
	0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5,
	0x9e7d9662, 0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06,
	0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03,
	0xb1f740b4
};

Static uint32_t
ueagle_crc_update(uint32_t crc, uint8_t *buf, int len)
{
	for (; len != 0; len--, buf++)
		crc = ueagle_crc32_table[(crc >> 24) ^ *buf] ^ (crc << 8);

	return crc;
}

/*
 * Reassembly part of the software ATM AAL5 SAR.
 */
Static void
ueagle_push_cell(struct ueagle_softc *sc, uint8_t *cell)
{
	struct ueagle_vcc *vcc = &sc->vcc;
	struct ifnet *ifp;
	struct mbuf *m;
	uint32_t crc;
	uint16_t pdulen, totlen;
	int s;

	sc->stats.atm.cells_received++;

	if (!(vcc->flags & UEAGLE_VCC_ACTIVE) ||
	    ATM_CH_GETVPI(cell) != vcc->vpi ||
	    ATM_CH_GETVCI(cell) != vcc->vci) {
		sc->stats.atm.vcc_no_conn++;
		return;
	}

	if (vcc->flags & UEAGLE_VCC_DROP) {
		if (ATM_CH_ISLASTCELL(cell)) {
			vcc->flags &= ~UEAGLE_VCC_DROP;
			sc->stats.atm.cspdus_dropped++;
		}

		sc->stats.atm.cells_dropped++;
		return;
	}

	if (vcc->m == NULL) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			vcc->flags |= UEAGLE_VCC_DROP;
			return;
		}

		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			vcc->flags |= UEAGLE_VCC_DROP;
			m_freem(m);
			return;
		}

		vcc->m = m;
		vcc->dst = mtod(m, uint8_t *);
		vcc->limit = vcc->dst + MCLBYTES - ATM_CELL_PAYLOAD_SIZE;
	}

	if (vcc->dst > vcc->limit) {
		vcc->flags |= UEAGLE_VCC_DROP;
		sc->stats.atm.cells_dropped++;
		goto fail;
	}

	memcpy(vcc->dst, cell + ATM_CELL_HEADER_SIZE, ATM_CELL_PAYLOAD_SIZE);
	vcc->dst += ATM_CELL_PAYLOAD_SIZE;

	if (!ATM_CH_ISLASTCELL(cell))
		return;

	/*
	 * Handle the last cell of the AAL5 CPCS-PDU.
	 */
	m = vcc->m;

	totlen = vcc->dst - mtod(m, uint8_t *);
	pdulen = AAL5_TR_GETPDULEN(cell);

	if (totlen < pdulen + AAL5_TRAILER_SIZE) {
		sc->stats.atm.cspdus_dropped++;
		goto fail;
	}

	if (totlen >= pdulen + ATM_CELL_PAYLOAD_SIZE + AAL5_TRAILER_SIZE) {
		sc->stats.atm.cspdus_dropped++;
		goto fail;
	}

	crc = ueagle_crc_update(CRC_INITIAL, mtod(m, uint8_t *), totlen);
	if (crc != CRC_MAGIC) {
		sc->stats.atm.cspdus_crc_errors++;
		goto fail;
	}

	/* finalize mbuf */
	ifp = &sc->sc_if;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = pdulen;

	sc->stats.atm.cspdus_received++;

	s = splnet();

#if NBPFILTER > 0
	if (ifp->if_bpf != NULL)
		bpf_mtap(ifp->if_bpf, m);
#endif

	/* send the AAL5 CPCS-PDU to the ATM layer */
	ifp->if_ipackets++;
	atm_input(ifp, &vcc->aph, m, vcc->rxhand);
	vcc->m = NULL;

	splx(s);

	return;

fail:	m_freem(vcc->m);
	vcc->m = NULL;
}

Static void
ueagle_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ueagle_isoreq *req = priv;
	struct ueagle_softc *sc = req->sc;
	uint32_t count;
	uint8_t *p;
	int i;

	if (status == USBD_CANCELLED)
		return;

	for (i = 0; i < UEAGLE_NISOFRMS; i++) {
		count = req->frlengths[i];
		p = req->offsets[i];

		while (count >= ATM_CELL_SIZE) {
			ueagle_push_cell(sc, p);
			p += ATM_CELL_SIZE;
			count -= ATM_CELL_SIZE;
		}
#ifdef DIAGNOSTIC
		if (count > 0) {
			printf("%s: truncated cell (%u bytes)\n", count,
			    USBDEVNAME(sc->sc_dev));
		}
#endif
		req->frlengths[i] = sc->isize;
	}

	usbd_setup_isoc_xfer(req->xfer, sc->pipeh_rx, req, req->frlengths,
	    UEAGLE_NISOFRMS, USBD_NO_COPY, ueagle_rxeof);
	usbd_transfer(xfer);
}

Static void
ueagle_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ueagle_txreq *req = priv;
	struct ueagle_softc *sc = req->sc;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->pipeh_tx);

		ifp->if_oerrors++;
		return;
	}

	s = splnet();

	ifp->if_opackets++;
	ifp->if_flags &= ~IFF_OACTIVE;
	ueagle_start(ifp);

	splx(s);
}

/*
 * Segmentation part of the software ATM AAL5 SAR.
 */
Static int
ueagle_encap(struct ueagle_softc *sc, struct mbuf *m0)
{
	struct ueagle_vcc *vcc = &sc->vcc;
	struct ueagle_txreq *req;
	struct mbuf *m;
	uint8_t *src, *dst;
	uint32_t crc;
	int n, cellleft, mleft;
	usbd_status error;

	req = &sc->txreqs[0];

	m_adj(m0, sizeof (struct atm_pseudohdr));

	dst = req->buf;
	cellleft = 0;
	crc = CRC_INITIAL;

	for (m = m0; m != NULL; m = m->m_next) {
		src = mtod(m, uint8_t *);
		mleft = m->m_len;

		crc = ueagle_crc_update(crc, src, mleft);

		if (cellleft != 0) {
			n = min(mleft, cellleft);

			memcpy(dst, src, n);
			dst += n;
			src += n;
			cellleft -= n;
			mleft -= n;
		}

		while (mleft >= ATM_CELL_PAYLOAD_SIZE) {
			memcpy(dst, vcc->ch, ATM_CELL_HEADER_SIZE);
			dst += ATM_CELL_HEADER_SIZE;
			memcpy(dst, src, ATM_CELL_PAYLOAD_SIZE);
			dst += ATM_CELL_PAYLOAD_SIZE;
			src += ATM_CELL_PAYLOAD_SIZE;
			mleft -= ATM_CELL_PAYLOAD_SIZE;
			sc->stats.atm.cells_transmitted++;
		}

		if (mleft != 0) {
			memcpy(dst, vcc->ch, ATM_CELL_HEADER_SIZE);
			dst += ATM_CELL_HEADER_SIZE;
			memcpy(dst, src, mleft);
			dst += mleft;
			cellleft = ATM_CELL_PAYLOAD_SIZE - mleft;
			sc->stats.atm.cells_transmitted++;
		}
	}

	/*
	 * If there is not enough space to put the AAL5 trailer into this cell,
	 * pad the content of this cell with zeros and create a new cell which
	 * will contain no data except the AAL5 trailer itself.
	 */
	if (cellleft < AAL5_TRAILER_SIZE) {
		memset(dst, 0, cellleft);
		crc = ueagle_crc_update(crc, dst, cellleft);
		dst += cellleft;

		memcpy(dst, vcc->ch, ATM_CELL_HEADER_SIZE);
		dst += ATM_CELL_HEADER_SIZE;
		cellleft = ATM_CELL_PAYLOAD_SIZE;
		sc->stats.atm.cells_transmitted++;
	}

	/*
	 * Fill the AAL5 CPCS-PDU trailer.
	 */
	memset(dst, 0, cellleft - AAL5_TRAILER_SIZE);

	/* src now points to the beginning of the last cell */
	src = dst + cellleft - ATM_CELL_SIZE;
	ATM_CH_SETPTFLAGS(src, 1);

	AAL5_TR_SETCPSUU(src, 0);
	AAL5_TR_SETCPI(src, 0);
	AAL5_TR_SETPDULEN(src, m0->m_pkthdr.len);

	crc = ~ueagle_crc_update(crc, dst, cellleft - 4);
	AAL5_TR_SETCRC(src, crc);

	usbd_setup_xfer(req->xfer, sc->pipeh_tx, req, req->buf,
	    dst + cellleft - req->buf, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    UEAGLE_TX_TIMEOUT, ueagle_txeof);

	error = usbd_transfer(req->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS)
		return error;

	sc->stats.atm.cspdus_transmitted++;

	return 0;
}

Static void
ueagle_start(struct ifnet *ifp)
{
	struct ueagle_softc *sc = ifp->if_softc;
	struct mbuf *m0;

	/* nothing goes out until modem is synchronized and VCC is opened */
	if (!(sc->vcc.flags & UEAGLE_VCC_ACTIVE))
		return;

	if (sc->pipeh_tx == NULL)
		return;

	IFQ_POLL(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	if (ueagle_encap(sc, m0) != 0) {
		m_freem(m0);
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m0);

#if NBPFILTER > 0
	if (ifp->if_bpf != NULL)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	m_freem(m0);

	ifp->if_flags |= IFF_OACTIVE;
}

Static int
ueagle_open_vcc(struct ueagle_softc *sc, struct atm_pseudoioctl *api)
{
	struct ueagle_vcc *vcc = &sc->vcc;

	DPRINTF(("%s: opening ATM VCC\n", USBDEVNAME(sc->sc_dev)));

	vcc->vpi = ATM_PH_VPI(&api->aph);
	vcc->vci = ATM_PH_VCI(&api->aph);
	vcc->rxhand = api->rxhand;
	vcc->m = NULL;
	vcc->aph = api->aph;
	vcc->flags = UEAGLE_VCC_ACTIVE;

	/* pre-calculate cell headers (HEC field is set by hardware) */
	ATM_CH_FILL(vcc->ch, 0, vcc->vpi, vcc->vci, 0, 0, 0);

	return 0;
}

Static int
ueagle_close_vcc(struct ueagle_softc *sc, struct atm_pseudoioctl *api)
{
	DPRINTF(("%s: closing ATM VCC\n", USBDEVNAME(sc->sc_dev)));

	sc->vcc.flags &= ~UEAGLE_VCC_ACTIVE;

	return 0;
}

Static int
ueagle_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ueagle_softc *sc = ifp->if_softc;
	struct atm_pseudoioctl *api;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;

		ueagle_init(ifp);
#ifdef INET
		ifa->ifa_rtrequest = atm_rtrequest;
#endif
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				ueagle_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ueagle_stop(ifp, 1);
		}
		break;

	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;

		if (ifr->ifr_mtu > UEAGLE_IFMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCATMENA:
		api = (struct atm_pseudoioctl *)data;
		error = ueagle_open_vcc(sc, api);
		break;

	case SIOCATMDIS:
		api = (struct atm_pseudoioctl *)data;
		error = ueagle_close_vcc(sc, api);
		break;

	default:
		error = EINVAL;
	}

	splx(s);

	return error;
}

Static int
ueagle_open_pipes(struct ueagle_softc *sc)
{
	usb_endpoint_descriptor_t *edesc;
	usbd_interface_handle iface;
	struct ueagle_txreq *txreq;
	struct ueagle_isoreq *isoreq;
	usbd_status error;
	uint8_t *buf;
	int i, j;

	error = usbd_device2interface_handle(sc->sc_udev, UEAGLE_US_IFACE_NO,
	    &iface);
	if (error != 0) {
		printf("%s: could not get tx interface handle\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = usbd_open_pipe(iface, UEAGLE_TX_PIPE, USBD_EXCLUSIVE_USE,
	    &sc->pipeh_tx);
	if (error != 0) {
		printf("%s: could not open tx pipe\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	for (i = 0; i < UEAGLE_TX_LIST_CNT; i++) {
		txreq = &sc->txreqs[i];

		txreq->sc = sc;

		txreq->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (txreq->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		txreq->buf = usbd_alloc_buffer(txreq->xfer, UEAGLE_TXBUFLEN);
		if (txreq->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
	}

	error = usbd_device2interface_handle(sc->sc_udev, UEAGLE_DS_IFACE_NO,
	    &iface);
	if (error != 0) {
		printf("%s: could not get rx interface handle\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/* XXX: alternative interface number sould depend on downrate */
	error = usbd_set_interface(iface, 8);
	if (error != 0) {
		printf("%s: could not set rx alternative interface\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	edesc = usbd_get_endpoint_descriptor(iface, UEAGLE_RX_PIPE);
	if (edesc == NULL) {
		printf("%s: could not get rx endpoint descriptor\n",
		    USBDEVNAME(sc->sc_dev));
		error = EIO;
		goto fail;
	}

	sc->isize = UGETW(edesc->wMaxPacketSize);

	error = usbd_open_pipe(iface, UEAGLE_RX_PIPE, USBD_EXCLUSIVE_USE,
	    &sc->pipeh_rx);
	if (error != 0) {
		printf("%s: could not open rx pipe\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	for (i = 0; i < UEAGLE_NISOREQS; i++) {
		isoreq = &sc->isoreqs[i];

		isoreq->sc = sc;

		isoreq->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (isoreq->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		buf = usbd_alloc_buffer(isoreq->xfer,
		    sc->isize * UEAGLE_NISOFRMS);
		if (buf == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		for (j = 0; j < UEAGLE_NISOFRMS; j++) {
			isoreq->frlengths[j] = sc->isize;
			isoreq->offsets[j] = buf + j * sc->isize;
		}

		usbd_setup_isoc_xfer(isoreq->xfer, sc->pipeh_rx, isoreq,
		    isoreq->frlengths, UEAGLE_NISOFRMS, USBD_NO_COPY,
		    ueagle_rxeof);
		usbd_transfer(isoreq->xfer);
	}

	ueagle_request(sc, UEAGLE_SETMODE, UEAGLE_LOOPBACKOFF, NULL, 0);

	return 0;

fail:	ueagle_close_pipes(sc);
	return error;
}

Static void
ueagle_close_pipes(struct ueagle_softc *sc)
{
	int i;

	ueagle_request(sc, UEAGLE_SETMODE, UEAGLE_LOOPBACKON, NULL, 0);

	/* free Tx resources */
	if (sc->pipeh_tx != NULL) {
		usbd_abort_pipe(sc->pipeh_tx);
		usbd_close_pipe(sc->pipeh_tx);
		sc->pipeh_tx = NULL;
	}

	for (i = 0; i < UEAGLE_TX_LIST_CNT; i++) {
		if (sc->txreqs[i].xfer != NULL) {
			usbd_free_xfer(sc->txreqs[i].xfer);
			sc->txreqs[i].xfer = NULL;
		}
	}

	/* free Rx resources */
	if (sc->pipeh_rx != NULL) {
		usbd_abort_pipe(sc->pipeh_rx);
		usbd_close_pipe(sc->pipeh_rx);
		sc->pipeh_rx = NULL;
	}

	for (i = 0; i < UEAGLE_NISOREQS; i++) {
		if (sc->isoreqs[i].xfer != NULL) {
			usbd_free_xfer(sc->isoreqs[i].xfer);
			sc->isoreqs[i].xfer = NULL;
		}
	}
}

Static int
ueagle_init(struct ifnet *ifp)
{
	struct ueagle_softc *sc = ifp->if_softc;
	usbd_interface_handle iface;
	usbd_status error;
	size_t len;

	ueagle_stop(ifp, 0);

	error = usbd_device2interface_handle(sc->sc_udev, UEAGLE_US_IFACE_NO,
	    &iface);
	if (error != 0) {
		printf("%s: could not get idma interface handle\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = usbd_open_pipe(iface, UEAGLE_IDMA_PIPE, USBD_EXCLUSIVE_USE,
	    &sc->pipeh_idma);
	if (error != 0) {
		printf("%s: could not open idma pipe\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = usbd_device2interface_handle(sc->sc_udev, UEAGLE_INTR_IFACE_NO,
	    &iface);
	if (error != 0) {
		printf("%s: could not get interrupt interface handle\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = loadfirmware("ueagle-dsp", &sc->dsp, &len);
	if (error != 0) {
		printf("%s: could not load firmware\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = usbd_open_pipe_intr(iface, UEAGLE_INTR_PIPE, USBD_SHORT_XFER_OK,
	    &sc->pipeh_intr, sc, sc->ibuf, UEAGLE_INTR_MAXSIZE, ueagle_intr,
	    UEAGLE_INTR_INTERVAL);
	if (error != 0) {
		printf("%s: could not open interrupt pipe\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = ueagle_boot(sc);
	if (error != 0) {
		printf("%s: could not boot modem\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/*
	 * Opening of tx and rx pipes if deferred after synchronization is
	 * established.
	 */

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return 0;

fail:	ueagle_stop(ifp, 1);
	return error;
}

Static void
ueagle_stop(struct ifnet *ifp, int disable)
{
	struct ueagle_softc *sc = ifp->if_softc;

	/* stop any pending task */
	usb_rem_task(sc->sc_udev, &sc->sc_swap_task);

	/* free Tx and Rx resources */
	ueagle_close_pipes(sc);

	/* free firmware */
	if (sc->dsp != NULL) {
		free(sc->dsp, M_DEVBUF);
		sc->dsp = NULL;
	}

	/* free interrupt resources */
	if (sc->pipeh_intr != NULL) {
		usbd_abort_pipe(sc->pipeh_intr);
		usbd_close_pipe(sc->pipeh_intr);
		sc->pipeh_intr = NULL;
	}

	/* free IDMA resources */
	if (sc->pipeh_idma != NULL) {
		usbd_abort_pipe(sc->pipeh_idma);
		usbd_close_pipe(sc->pipeh_idma);
		sc->pipeh_idma = NULL;
	}

	/* reset statistics */
	memset(&sc->stats, 0, sizeof (struct ueagle_stats));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

Static int
ueagle_activate(device_ptr_t self, enum devact act)
{
	struct ueagle_softc *sc = (struct ueagle_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return EOPNOTSUPP;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		sc->gone = 1;
		break;
	}

	return 0;
}
