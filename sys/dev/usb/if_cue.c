/*	$OpenBSD: if_cue.c,v 1.2 2000/03/30 16:19:32 aaron Exp $ */
/*	$NetBSD: if_cue.c,v 1.20 2000/03/30 08:53:30 augustss Exp $	*/
/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/if_cue.c,v 1.4 2000/01/16 22:45:06 wpaul Exp $
 */

/*
 * CATC USB-EL1210A USB to ethernet driver. Used in the CATC Netmate
 * adapters and others.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The CATC USB-EL1210A provides USB ethernet support at 10Mbps. The
 * RX filter uses a 512-bit multicast hash table, single perfect entry
 * for the station address, and promiscuous mode. Unlike the ADMtek
 * and KLSI chips, the CATC ASIC supports read and write combining
 * mode where multiple packets can be transfered using a single bulk
 * transaction, which helps performance a great deal.
 */

/*
 * Ported to NetBSD and somewhat rewritten by Lennart Augustsson.
 */

/*
 * TODO:
 * proper cleanup on errors
 */
#if defined(__NetBSD__)
#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h"
#elif defined(__OpenBSD__)
#include "bpfilter.h"
#endif /* defined(__OpenBSD__) */

#include <sys/param.h>
#include <sys/systm.h>
#if !defined(__OpenBSD__)
#include <sys/callout.h>
#endif
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#if defined(__FreeBSD__)

#include <net/ethernet.h>
#include <machine/clock.h>	/* for DELAY */
#include <sys/bus.h>

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#endif

#include <net/if.h>
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <net/if_arp.h>
#endif
#include <net/if_dl.h>

#if defined(__NetBSD__) || defined(__OpenBSD__)
#define BPF_MTAP(ifp, m) bpf_mtap((ifp)->if_bpf, (m))
#else
#define BPF_MTAP(ifp, m) bpf_mtap((ifp), (m))
#endif

#if defined(__FreeBSD__) || NBPFILTER > 0
#include <net/bpf.h>
#endif

#if defined(__NetBSD__)
#include <net/if_ether.h>
#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_inarp.h>
#endif
#endif /* defined(__NetBSD__) */

#if defined(__OpenBSD__)
#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#endif /* defined(__OpenBSD__) */

#if defined(__NetBSD__) || defined(__OpenBSD__)
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#ifdef __FreeBSD__
#include <dev/usb/usb_ethersubr.h>
#endif

#include <dev/usb/if_cuereg.h>

#ifdef CUE_DEBUG
#define DPRINTF(x)	if (cuedebug) logprintf x
#define DPRINTFN(n,x)	if (cuedebug >= (n)) logprintf x
int	cuedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static struct cue_type cue_devs[] = {
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE },
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE2 },
	/* Belkin F5U111 adapter covered by NETMATE entry */
	{ 0, 0 }
};

USB_DECLARE_DRIVER(cue);

Static int cue_open_pipes	__P((struct cue_softc *));
Static int cue_tx_list_init	__P((struct cue_softc *));
Static int cue_rx_list_init	__P((struct cue_softc *));
Static int cue_newbuf		__P((struct cue_softc *, struct cue_chain *,
				    struct mbuf *));
Static int cue_send		__P((struct cue_softc *, struct mbuf *, int));
Static void cue_rxeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void cue_txeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void cue_tick		__P((void *));
Static void cue_start		__P((struct ifnet *));
Static int cue_ioctl		__P((struct ifnet *, u_long, caddr_t));
Static void cue_init		__P((void *));
Static void cue_stop		__P((struct cue_softc *));
Static void cue_watchdog		__P((struct ifnet *));

Static void cue_setmulti	__P((struct cue_softc *));
Static u_int32_t cue_crc	__P((caddr_t));
Static void cue_reset		__P((struct cue_softc *));

Static int cue_csr_read_1	__P((struct cue_softc *, int));
Static int cue_csr_write_1	__P((struct cue_softc *, int, int));
Static int cue_csr_read_2	__P((struct cue_softc *, int));
#ifdef notdef
Static int cue_csr_write_2	__P((struct cue_softc *, int, int));
#endif
Static int cue_mem		__P((struct cue_softc *, int,
				    int, void *, int));
Static int cue_getmac		__P((struct cue_softc *, void *));

#ifdef __FreeBSD__
#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/usb/if_cue.c,v 1.4 2000/01/16 22:45:06 wpaul Exp $";
#endif

Static void cue_rxstart		__P((struct ifnet *));
Static void cue_shutdown	__P((device_t));

Static struct usb_qdat cue_qdat;

Static device_method_t cue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cue_match),
	DEVMETHOD(device_attach,	cue_attach),
	DEVMETHOD(device_detach,	cue_detach),
	DEVMETHOD(device_shutdown,	cue_shutdown),

	{ 0, 0 }
};

Static driver_t cue_driver = {
	"cue",
	cue_methods,
	sizeof(struct cue_softc)
};

Static devclass_t cue_devclass;

DRIVER_MODULE(if_cue, uhub, cue_driver, cue_devclass, usbd_driver_load, 0);

#endif /* defined(__FreeBSD__) */

#define CUE_DO_REQUEST(dev, req, data)			\
	usbd_do_request_flags(dev, req, data, USBD_NO_TSLEEP, NULL)

#define CUE_SETBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) | (x))

#define CUE_CLRBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) & ~(x))

Static int
cue_csr_read_1(sc, reg)
	struct cue_softc	*sc;
	int			reg;
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int8_t		val = 0;
	int			s;

	if (sc->cue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, &val);
	splx(s);

	if (err) {
		DPRINTF(("%s: cue_csr_read_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	DPRINTFN(10,("%s: cue_csr_read_1 reg=0x%x val=0x%x\n", 
		     USBDEVNAME(sc->cue_dev), reg, val));

	return (val);
}

Static int
cue_csr_read_2(sc, reg)
	struct cue_softc	*sc;
	int			reg;
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;
	int			s;

	if (sc->cue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, &val);
	splx(s);

	DPRINTFN(10,("%s: cue_csr_read_2 reg=0x%x val=0x%x\n", 
		     USBDEVNAME(sc->cue_dev), reg, UGETW(val)));

	if (err) {
		DPRINTF(("%s: cue_csr_read_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (UGETW(val));
}

Static int
cue_csr_write_1(sc, reg, val)
	struct cue_softc	*sc;
	int			reg, val;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	if (sc->cue_dying)
		return (0);

	DPRINTFN(10,("%s: cue_csr_write_1 reg=0x%x val=0x%x\n", 
		     USBDEVNAME(sc->cue_dev), reg, val));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, NULL);
	splx(s);

	if (err) {
		DPRINTF(("%s: cue_csr_write_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	DPRINTFN(20,("%s: cue_csr_write_1, after reg=0x%x val=0x%x\n", 
		     USBDEVNAME(sc->cue_dev), reg, cue_csr_read_1(sc, reg)));

	return (0);
}

#ifdef notdef
Static int
cue_csr_write_2(sc, reg, val)
	struct cue_softc	*sc;
	int			reg, aval;
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;
	int			s;

	if (sc->cue_dying)
		return (0);

	DPRINTFN(10,("%s: cue_csr_write_2 reg=0x%x val=0x%x\n", 
		     USBDEVNAME(sc->cue_dev), reg, aval));

	USETW(val, aval);
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, NULL);
	splx(s);

	if (err) {
		DPRINTF(("%s: cue_csr_write_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}
#endif

Static int
cue_mem(sc, cmd, addr, buf, len)
	struct cue_softc	*sc;
	int			cmd;
	int			addr;
	void			*buf;
	int			len;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	DPRINTFN(10,("%s: cue_mem cmd=0x%x addr=0x%x len=%d\n",
		     USBDEVNAME(sc->cue_dev), cmd, addr, len));

	if (cmd == CUE_CMD_READSRAM)
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, buf);
	splx(s);

	if (err) {
		DPRINTF(("%s: cue_csr_mem: addr=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), addr, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

Static int
cue_getmac(sc, buf)
	struct cue_softc	*sc;
	void			*buf;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	DPRINTFN(10,("%s: cue_getmac\n", USBDEVNAME(sc->cue_dev)));

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_GET_MACADDR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, ETHER_ADDR_LEN);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, buf);
	splx(s);

	if (err) {
		printf("%s: read MAC address failed\n", USBDEVNAME(sc->cue_dev));
		return (-1);
	}

	return (0);
}

#define CUE_POLY	0xEDB88320
#define CUE_BITS	9

Static u_int32_t
cue_crc(addr)
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? CUE_POLY : 0);
	}

	return (crc & ((1 << CUE_BITS) - 1));
}

Static void
cue_setmulti(sc)
	struct cue_softc	*sc;
{
	struct ifnet		*ifp;
#if defined(__FreeBSD__)
	struct ifmultiaddr	*ifma;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct ether_multi	*enm;
	struct ether_multistep	step;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
	u_int32_t		h, i;

	ifp = GET_IFP(sc);

	DPRINTFN(2,("%s: cue_setmulti if_flags=0x%x\n", 
		    USBDEVNAME(sc->cue_dev), ifp->if_flags));

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
			sc->cue_mctab[i] = 0xFF;
		cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
		    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);
		return;
	}

	/* first, zot all the existing hash bits */
	for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
		sc->cue_mctab[i] = 0;

	/* now program new ones */
#if defined(__FreeBSD__)
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = cue_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);		
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->cue_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
#if 0
		if (memcmp(enm->enm_addrlo,
			   enm->enm_addrhi, ETHER_ADDR_LEN) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			/* XXX what now? */
			return;
		}
#endif
		h = cue_crc(enm->enm_addrlo);
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);		
		ETHER_NEXT_MULTI(step, enm);
	}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	/*
	 * Also include the broadcast address in the filter
	 * so we can receive broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		h = cue_crc(etherbroadcastaddr);
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);		
	}

	cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
	    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);
}

Static void
cue_reset(sc)
	struct cue_softc	*sc;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	DPRINTFN(2,("%s: cue_reset\n", USBDEVNAME(sc->cue_dev)));

	if (sc->cue_dying)
		return;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	s = splusb();
	err = CUE_DO_REQUEST(sc->cue_udev, &req, NULL);
	splx(s);

	if (err)
		printf("%s: reset failed\n", USBDEVNAME(sc->cue_dev));

	/* Wait a little while for the chip to get its brains in order. */
	delay(1000);		/* XXX */
}

/*
 * Probe for a CATC chip.
 */
USB_MATCH(cue)
{
	USB_MATCH_START(cue, uaa);
	struct cue_type			*t;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (t = cue_devs; t->cue_vid != 0; t++)
		if (uaa->vendor == t->cue_vid && uaa->product == t->cue_did)
			return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(cue)
{
	USB_ATTACH_START(cue, sc, uaa);
	char			devinfo[1024];
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

#ifdef __FreeBSD__
	bzero(sc, sizeof(struct cue_softc));
#endif

	DPRINTFN(5,(" : cue_attach: sc=%p, dev=%p", sc, dev));

	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->cue_dev), devinfo);

	err = usbd_set_config_no(dev, CUE_CONFIG_NO, 0);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->cue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->cue_udev = dev;
	sc->cue_product = uaa->product;
	sc->cue_vendor = uaa->vendor;

	err = usbd_device2interface_handle(dev, CUE_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->cue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->cue_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->cue_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->cue_ed[CUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

#if 0
	/* Reset the adapter. */
	cue_reset(sc);
#endif
	/*
	 * Get station address.
	 */
	cue_getmac(sc, &eaddr);

	s = splimp();

	/*
	 * A CATC chip was detected. Inform the world.
	 */
#if defined(__FreeBSD__)
	printf("%s: Ethernet address: %6D\n", USBDEVNAME(sc->cue_dev), eaddr, ":");

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = USBDEVNAME(sc->cue_dev);
	ifp->if_name = "cue";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cue_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = cue_start;
	ifp->if_watchdog = cue_watchdog;
	ifp->if_init = cue_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	cue_qdat.ifp = ifp;
	cue_qdat.if_rxstart = cue_rxstart;

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
	usb_register_netisr();

#elif defined(__NetBSD__) || defined(__OpenBSD__)

	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->cue_dev),
	    ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cue_ioctl;
	ifp->if_start = cue_start;
	ifp->if_watchdog = cue_watchdog;
#if defined(__OpenBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
	strncpy(ifp->if_xname, USBDEVNAME(sc->cue_dev), IFNAMSIZ);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->cue_dev),
	    RND_TYPE_NET, 0);
#endif

#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	usb_callout_init(sc->cue_stat_ch);

	sc->cue_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->cue_udev,
	    USBDEV(sc->cue_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(cue)
{
	USB_DETACH_START(cue, sc);
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __FUNCTION__));

	s = splusb();

	usb_uncallout(sc->cue_stat_ch, cue_tick, sc);

	if (!sc->cue_attached) {
		/* Detached before attached finished, so just bail out. */
		splx(s);
		return (0);
	}

	if (ifp->if_flags & IFF_RUNNING)
		cue_stop(sc);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ether_ifdetach(ifp);
#endif /* __NetBSD__ */

	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->cue_ep[CUE_ENDPT_TX] != NULL ||
	    sc->cue_ep[CUE_ENDPT_RX] != NULL ||
	    sc->cue_ep[CUE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->cue_dev));
#endif

	sc->cue_attached = 0;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->cue_udev,
	    USBDEV(sc->cue_dev));

	return (0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
cue_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct cue_softc *sc = (struct cue_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __FUNCTION__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* Deactivate the interface. */
		if_deactivate(&sc->cue_ec.ec_if);
		sc->cue_dying = 1;
		break;
	}
	return (0);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
cue_newbuf(sc, c, m)
	struct cue_softc	*sc;
	struct cue_chain	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->cue_dev));
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->cue_dev));
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);
	c->cue_mbuf = m_new;

	return (0);
}

Static int
cue_rx_list_init(sc)
	struct cue_softc	*sc;
{
	struct cue_cdata	*cd;
	struct cue_chain	*c;
	int			i;

	cd = &sc->cue_cdata;
	for (i = 0; i < CUE_RX_LIST_CNT; i++) {
		c = &cd->cue_rx_chain[i];
		c->cue_sc = sc;
		c->cue_idx = i;
		if (cue_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->cue_xfer == NULL) {
			c->cue_xfer = usbd_alloc_xfer(sc->cue_udev);
			if (c->cue_xfer == NULL)
				return (ENOBUFS);
			c->cue_buf = usbd_alloc_buffer(c->cue_xfer, CUE_BUFSZ);
			if (c->cue_buf == NULL) {
				usbd_free_xfer(c->cue_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static int
cue_tx_list_init(sc)
	struct cue_softc	*sc;
{
	struct cue_cdata	*cd;
	struct cue_chain	*c;
	int			i;

	cd = &sc->cue_cdata;
	for (i = 0; i < CUE_TX_LIST_CNT; i++) {
		c = &cd->cue_tx_chain[i];
		c->cue_sc = sc;
		c->cue_idx = i;
		c->cue_mbuf = NULL;
		if (c->cue_xfer == NULL) {
			c->cue_xfer = usbd_alloc_xfer(sc->cue_udev);
			if (c->cue_xfer == NULL)
				return (ENOBUFS);
			c->cue_buf = usbd_alloc_buffer(c->cue_xfer, CUE_BUFSZ);
			if (c->cue_buf == NULL) {
				usbd_free_xfer(c->cue_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

#ifdef __FreeBSD__
Static void
cue_rxstart(ifp)
	struct ifnet		*ifp;
{
	struct cue_softc	*sc;
	struct cue_chain	*c;

	sc = ifp->if_softc;
	c = &sc->cue_cdata.cue_rx_chain[sc->cue_cdata.cue_rx_prod];

	if (cue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->cue_xfer, sc->cue_ep[CUE_ENDPT_RX],
	    c, c->cue_buf, CUE_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, cue_rxeof);
	usbd_transfer(c->cue_xfer);
}
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
cue_rxeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct cue_chain	*c = priv;
	struct cue_softc	*sc = c->cue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m;
	int			total_len = 0;
	u_int16_t		len;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int			s;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->cue_dev),
		     __FUNCTION__, status));

	if (sc->cue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->cue_rx_errs++;
		if (usbd_ratecheck(&sc->cue_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			    USBDEVNAME(sc->cue_dev), sc->cue_rx_errs,
			    usbd_errstr(status));
			sc->cue_rx_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->cue_ep[CUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	memcpy(mtod(c->cue_mbuf, char *), c->cue_buf, total_len);

	m = c->cue_mbuf;
	len = UGETW(mtod(m, u_int8_t *));

	/* No errors; receive the packet. */
	total_len = len;

	if (len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	m_adj(m, sizeof(u_int16_t));
	m->m_pkthdr.len = m->m_len = total_len;

#if defined(__FreeBSD__)
	m->m_pkthdr.rcvif = (struct ifnet *)&cue_qdat;
	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);

	return;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m->m_pkthdr.rcvif = ifp;

	s = splimp();

	/* XXX ugly */
	if (cue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

#if NBPFILTER > 0
	/*
	 * Handle BPF listeners. Let the BPF user see the packet, but
	 * don't pass it up to the ether_input() layer unless it's
	 * a broadcast packet, multicast packet, matches our ethernet
	 * address or the interface is in promiscuous mode.
	 */
	if (ifp->if_bpf) {
#if defined(__NetBSD__)
		struct ether_header *eh = mtod(m, struct ether_header *);
		BPF_MTAP(ifp, m);
		if ((ifp->if_flags & IFF_PROMISC) &&
		    memcmp(eh->ether_dhost, LLADDR(ifp->if_sadl),
			   ETHER_ADDR_LEN) &&
		    !(eh->ether_dhost[0] & 1)) {
			m_freem(m);
			goto done1;
		}
#else
		BPF_MTAP(ifp, m);
#endif
	}
#endif

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->cue_dev),
		    __FUNCTION__, m->m_len));
	IF_INPUT(ifp, m);
 done1:
	splx(s);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

done:
	/* Setup new transfer. */
	usbd_setup_xfer(c->cue_xfer, sc->cue_ep[CUE_ENDPT_RX],
	    c, c->cue_buf, CUE_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, cue_rxeof);
	usbd_transfer(c->cue_xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->cue_dev),
		    __FUNCTION__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
Static void
cue_txeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct cue_chain	*c = priv;
	struct cue_softc	*sc = c->cue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	if (sc->cue_dying)
		return;

	s = splimp();

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->cue_dev),
		    __FUNCTION__, status));

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->cue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->cue_ep[CUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

#if defined(__FreeBSD__)
	c->cue_mbuf->m_pkthdr.rcvif = ifp;
	usb_tx_done(c->cue_mbuf);
	c->cue_mbuf = NULL;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m_freem(c->cue_mbuf);
	c->cue_mbuf = NULL;

	if (ifp->if_snd.ifq_head != NULL)
		cue_start(ifp);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	splx(s);
}

Static void
cue_tick(xsc)
	void			*xsc;
{
	struct cue_softc	*sc = xsc;
	struct ifnet		*ifp;
	int			s;

	if (sc == NULL)
		return;

	if (sc->cue_dying)
		return;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __FUNCTION__));

	s = splimp();

	ifp = GET_IFP(sc);

	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_SINGLECOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_MULTICOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_EXCESSCOLL);

	if (cue_csr_read_2(sc, CUE_RX_FRAMEERR))
		ifp->if_ierrors++;

	usb_callout(sc->cue_stat_ch, hz, cue_tick, sc);

	splx(s);
}

Static int
cue_send(sc, m, idx)
	struct cue_softc	*sc;
	struct mbuf		*m;
	int			idx;
{
	int			total_len;
	struct cue_chain	*c;
	usbd_status		err;

	c = &sc->cue_cdata.cue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->cue_buf + 2);
	c->cue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;

	DPRINTFN(10,("%s: %s: total_len=%d\n",
		     USBDEVNAME(sc->cue_dev), __FUNCTION__, total_len));

	/* The first two bytes are the frame length */
	c->cue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->cue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	/* XXX 10000 */
	usbd_setup_xfer(c->cue_xfer, sc->cue_ep[CUE_ENDPT_TX],
	    c, c->cue_buf, total_len, USBD_NO_COPY, 10000, cue_txeof);

	/* Transmit */
	err = usbd_transfer(c->cue_xfer);
	if (err != USBD_IN_PROGRESS) {
		cue_stop(sc);
		return (EIO);
	}

	sc->cue_cdata.cue_tx_cnt++;

	return (0);
}

Static void
cue_start(ifp)
	struct ifnet		*ifp;
{
	struct cue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	if (sc->cue_dying)
		return;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__FUNCTION__));

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (cue_send(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

#if NBPFILTER > 0
	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m_head);
#endif

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

Static void
cue_init(xsc)
	void			*xsc;
{
	struct cue_softc	*sc = xsc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			i, s, ctl;
	u_char			*eaddr;

	if (sc->cue_dying)
		return;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__FUNCTION__));

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
#if 1
	cue_reset(sc);
#endif

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x03); /* 1 wait state */

#if defined(__FreeBSD__) || defined(__OpenBSD__)
	eaddr = sc->arpcom.ac_enaddr;
#elif defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		cue_csr_write_1(sc, CUE_PAR0 - i, eaddr[i]);

	/* Enable RX logic. */
	ctl = CUE_ETHCTL_RX_ON | CUE_ETHCTL_MCAST_ON;
	if (ifp->if_flags & IFF_PROMISC)
		ctl |= CUE_ETHCTL_PROMISC;
	cue_csr_write_1(sc, CUE_ETHCTL, ctl);

	/* Init TX ring. */
	if (cue_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->cue_dev));
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (cue_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->cue_dev));
		splx(s);
		return;
	}

	/* Load the multicast filter. */
	cue_setmulti(sc);

	/*
	 * Set the number of RX and TX buffers that we want
	 * to reserve inside the ASIC.
	 */
	cue_csr_write_1(sc, CUE_RX_BUFPKTS, CUE_RX_FRAMES);
	cue_csr_write_1(sc, CUE_TX_BUFPKTS, CUE_TX_FRAMES);

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x01); /* 1 wait state */

	/* Program the LED operation. */
	cue_csr_write_1(sc, CUE_LEDCTL, CUE_LEDCTL_FOLLOW_LINK);

	if (sc->cue_ep[CUE_ENDPT_RX] == NULL) {
		if (cue_open_pipes(sc)) {
			splx(s);
			return;
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout(sc->cue_stat_ch, hz, cue_tick, sc);
}

Static int
cue_open_pipes(sc)
	struct cue_softc	*sc;
{
	struct cue_chain	*c;
	usbd_status		err;
	int			i;

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		return (EIO);
	}

	/* Start up the receive pipe. */
	for (i = 0; i < CUE_RX_LIST_CNT; i++) {
		c = &sc->cue_cdata.cue_rx_chain[i];
		usbd_setup_xfer(c->cue_xfer, sc->cue_ep[CUE_ENDPT_RX],
		    c, c->cue_buf, CUE_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    cue_rxeof);
		usbd_transfer(c->cue_xfer);
	}

	return (0);
}

Static int
cue_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct cue_softc	*sc = ifp->if_softc;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
#endif
	int			s, error = 0;

	if (sc->cue_dying)
		return (EIO);

	s = splimp();

	switch(command) {
#if defined(__FreeBSD__)
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		cue_init(sc);

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
#if defined(__NetBSD__)
			arp_ifinit(ifp, ifa);
#else
			arp_ifinit(&sc->arpcom, ifa);
#endif
			break;
#endif /* INET */
#ifdef NS
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
					LLADDR(ifp->if_sadl);
			else
				memcpy(LLADDR(ifp->if_sadl),
				       ina->x_host.c_host,
				       ifp->if_addrlen);
			break;
		    }
#endif /* NS */
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->cue_if_flags & IFF_PROMISC)) {
				CUE_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->cue_if_flags & IFF_PROMISC) {
				CUE_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				cue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				cue_stop(sc);
		}
		sc->cue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		cue_setmulti(sc);
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return (error);
}

Static void
cue_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct cue_softc	*sc = ifp->if_softc;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__FUNCTION__));

	if (sc->cue_dying)
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->cue_dev));

	/*
	 * The polling business is a kludge to avoid allowing the
	 * USB code to call tsleep() in usbd_delay_ms(), which will
	 * kill us since the watchdog routine is invoked from
	 * interrupt context.
	 */
	usbd_set_polling(sc->cue_udev, 1);
	cue_stop(sc);
	cue_init(sc);
	usbd_set_polling(sc->cue_udev, 0);

	if (ifp->if_snd.ifq_head != NULL)
		cue_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
cue_stop(sc)
	struct cue_softc	*sc;
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__FUNCTION__));

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	cue_csr_write_1(sc, CUE_ETHCTL, 0);
	cue_reset(sc);
	usb_uncallout(sc->cue_stat_ch, cue_tick, sc);

	/* Stop transfers. */
	if (sc->cue_ep[CUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_RX] = NULL;
	}

	if (sc->cue_ep[CUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_TX] = NULL;
	}

	if (sc->cue_ep[CUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < CUE_RX_LIST_CNT; i++) {
		if (sc->cue_cdata.cue_rx_chain[i].cue_mbuf != NULL) {
			m_freem(sc->cue_cdata.cue_rx_chain[i].cue_mbuf);
			sc->cue_cdata.cue_rx_chain[i].cue_mbuf = NULL;
		}
		if (sc->cue_cdata.cue_rx_chain[i].cue_xfer != NULL) {
			usbd_free_xfer(sc->cue_cdata.cue_rx_chain[i].cue_xfer);
			sc->cue_cdata.cue_rx_chain[i].cue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < CUE_TX_LIST_CNT; i++) {
		if (sc->cue_cdata.cue_tx_chain[i].cue_mbuf != NULL) {
			m_freem(sc->cue_cdata.cue_tx_chain[i].cue_mbuf);
			sc->cue_cdata.cue_tx_chain[i].cue_mbuf = NULL;
		}
		if (sc->cue_cdata.cue_tx_chain[i].cue_xfer != NULL) {
			usbd_free_xfer(sc->cue_cdata.cue_tx_chain[i].cue_xfer);
			sc->cue_cdata.cue_tx_chain[i].cue_xfer = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

#ifdef __FreeBSD__
/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
Static void
cue_shutdown(dev)
	device_t		dev;
{
	struct cue_softc	*sc;

	sc = device_get_softc(dev);

	cue_reset(sc);
	cue_stop(sc);
}
#endif
