/*	$OpenBSD: if_kue.c,v 1.3 2000/03/30 16:19:32 aaron Exp $ */
/*	$NetBSD: if_kue.c,v 1.27 2000/03/30 00:18:17 augustss Exp $	*/
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
 * $FreeBSD: src/sys/dev/usb/if_kue.c,v 1.14 2000/01/14 01:36:15 wpaul Exp $
 */

/*
 * Kawasaki LSI KL5KUSB101B USB to ethernet adapter driver.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The KLSI USB to ethernet adapter chip contains an USB serial interface,
 * ethernet MAC and embedded microcontroller (called the QT Engine).
 * The chip must have firmware loaded into it before it will operate.
 * Packets are passed between the chip and host via bulk transfers.
 * There is an interrupt endpoint mentioned in the software spec, however
 * it's currently unused. This device is 10Mbps half-duplex only, hence
 * there is no media selection logic. The MAC supports a 128 entry
 * multicast filter, though the exact size of the filter can depend
 * on the firmware. Curiously, while the software spec describes various
 * ethernet statistics counters, my sample adapter and firmware combination
 * claims not to support any statistics counters at all.
 *
 * Note that once we load the firmware in the device, we have to be
 * careful not to load it again: if you restart your computer but
 * leave the adapter attached to the USB controller, it may remain
 * powered on and retain its firmware. In this case, we don't need
 * to load the firmware a second time.
 *
 * Special thanks to Rob Furr for providing an ADS Technologies
 * adapter for development and testing. No monkeys were harmed during
 * the development of this driver.
 */

/*
 * Ported to NetBSD and somewhat rewritten by Lennart Augustsson.
 */

/*
 * TODO:
 * only use kue_do_request for downloading firmware.
 * more DPRINTF
 * proper cleanup on errors
 */
#if defined(__NetBSD__)
#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h"
#elif defined(__OpenBSD__)
#include "bpfilter.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
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
#endif /* defined (__NetBSD__) */

#if defined(__OpenBSD__)
#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#endif /* defined (__OpenBSD__) */

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

#include <dev/usb/if_kuereg.h>
#include <dev/usb/kue_fw.h>

#ifdef KUE_DEBUG
#define DPRINTF(x)	if (kuedebug) logprintf x
#define DPRINTFN(n,x)	if (kuedebug >= (n)) logprintf x
int	kuedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static struct kue_type kue_devs[] = {
	{ USB_VENDOR_AOX, USB_PRODUCT_AOX_USB101 },
	{ USB_VENDOR_ADS, USB_PRODUCT_ADS_UBS10BT },
	{ USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC10T },
	{ USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_EA101 },
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET },
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET2 },
	{ USB_VENDOR_ENTREGA, USB_PRODUCT_ENTREGA_E45 },
	{ USB_VENDOR_3COM, USB_PRODUCT_3COM_3C19250 },
	{ USB_VENDOR_3COM, USB_PRODUCT_3COM_3C460 },
	{ USB_VENDOR_COREGA, USB_PRODUCT_COREGA_ETHER_USB_T },
	{ USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650C },
	{ USB_VENDOR_SMC, USB_PRODUCT_SMC_2102USB },
	{ USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10T },
	{ USB_VENDOR_KLSI, USB_PRODUCT_KLSI_DUH3E10BT },
	{ 0, 0 }
};

USB_DECLARE_DRIVER(kue);

Static int kue_tx_list_init	__P((struct kue_softc *));
Static int kue_rx_list_init	__P((struct kue_softc *));
Static int kue_newbuf		__P((struct kue_softc *, struct kue_chain *,
				    struct mbuf *));
Static int kue_send		__P((struct kue_softc *, struct mbuf *, int));
Static int kue_open_pipes	__P((struct kue_softc *));
Static void kue_rxeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void kue_txeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void kue_start		__P((struct ifnet *));
Static int kue_ioctl		__P((struct ifnet *, u_long, caddr_t));
Static void kue_init		__P((void *));
Static void kue_stop		__P((struct kue_softc *));
Static void kue_watchdog		__P((struct ifnet *));

Static void kue_setmulti	__P((struct kue_softc *));
Static void kue_reset		__P((struct kue_softc *));

Static usbd_status kue_ctl	__P((struct kue_softc *, int, u_int8_t,
				    u_int16_t, void *, u_int32_t));
Static usbd_status kue_setword	__P((struct kue_softc *, u_int8_t, u_int16_t));
Static int kue_is_warm		__P((struct kue_softc *));
Static int kue_load_fw		__P((struct kue_softc *));

#if defined(__FreeBSD__)
#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/usb/if_kue.c,v 1.14 2000/01/14 01:36:15 wpaul Exp $";
#endif

Static void kue_rxstart		__P((struct ifnet *));
Static void kue_shutdown	__P((device_t));

Static struct usb_qdat kue_qdat;

Static device_method_t kue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		kue_match),
	DEVMETHOD(device_attach,	kue_attach),
	DEVMETHOD(device_detach,	kue_detach),
	DEVMETHOD(device_shutdown,	kue_shutdown),

	{ 0, 0 }
};

Static driver_t kue_driver = {
	"kue",
	kue_methods,
	sizeof(struct kue_softc)
};

Static devclass_t kue_devclass;

DRIVER_MODULE(if_kue, uhub, kue_driver, kue_devclass, usbd_driver_load, 0);

#endif /* __FreeBSD__ */

#define KUE_DO_REQUEST(dev, req, data)			\
	usbd_do_request_flags(dev, req, data, USBD_NO_TSLEEP, NULL)

Static usbd_status
kue_setword(sc, breq, word)
	struct kue_softc	*sc;
	u_int8_t		breq;
	u_int16_t		word;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = breq;
	USETW(req.wValue, word);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	s = splusb();
	err = KUE_DO_REQUEST(sc->kue_udev, &req, NULL);
	splx(s);

	return (err);
}

Static usbd_status
kue_ctl(sc, rw, breq, val, data, len)
	struct kue_softc	*sc;
	int			rw;
	u_int8_t		breq;
	u_int16_t		val;
	void			*data;
	u_int32_t		len;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	DPRINTFN(10,("%s: %s: enter, len=%d\n", USBDEVNAME(sc->kue_dev),
		     __FUNCTION__, len));

	if (rw == KUE_CTL_WRITE)
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;

	req.bRequest = breq;
	USETW(req.wValue, val);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	s = splusb();
	err = KUE_DO_REQUEST(sc->kue_udev, &req, data);
	splx(s);

	return (err);
}

Static int
kue_is_warm(sc)
	struct kue_softc	*sc;
{
	usbd_status		err;
	usb_device_request_t	req;

	/* Just issue some random command. */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = KUE_CMD_GET_ETHER_DESCRIPTOR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(sc->kue_desc));

	err = usbd_do_request(sc->kue_udev, &req, &sc->kue_desc);

	return (!err);
}

Static int
kue_load_fw(sc)
	struct kue_softc	*sc;
{
	usbd_status		err;

	DPRINTFN(1,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	/*
	 * First, check if we even need to load the firmware.
	 * If the device was still attached when the system was
	 * rebooted, it may already have firmware loaded in it.
	 * If this is the case, we don't need to do it again.
	 * And in fact, if we try to load it again, we'll hang,
	 * so we have to avoid this condition if we don't want
	 * to look stupid.
	 *
	 * We can test this quickly by issuing a request that
	 * is only valid after firmware download.
	 */
	if (kue_is_warm(sc)) {
		printf("%s: warm boot, no firmware download\n",
		       USBDEVNAME(sc->kue_dev));
		return (0);
	}

	printf("%s: cold boot, downloading firmware\n",
	       USBDEVNAME(sc->kue_dev));

	/* Load code segment */
	DPRINTFN(1,("%s: kue_load_fw: download code_seg\n", 
		    USBDEVNAME(sc->kue_dev)));
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_code_seg, sizeof(kue_code_seg));
	if (err) {
		printf("%s: failed to load code segment: %s\n",
		    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
			return (EIO);
	}

	/* Load fixup segment */
	DPRINTFN(1,("%s: kue_load_fw: download fix_seg\n", 
		    USBDEVNAME(sc->kue_dev)));
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_fix_seg, sizeof(kue_fix_seg));
	if (err) {
		printf("%s: failed to load fixup segment: %s\n",
		    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
			return (EIO);
	}

	/* Send trigger command. */
	DPRINTFN(1,("%s: kue_load_fw: download trig_seg\n", 
		    USBDEVNAME(sc->kue_dev)));
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_trig_seg, sizeof(kue_trig_seg));
	if (err) {
		printf("%s: failed to load trigger segment: %s\n",
		    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
			return (EIO);
	}

	usbd_delay_ms(sc->kue_udev, 10);

	/*
	 * Reload device descriptor.
	 * Why? The chip without the firmware loaded returns
	 * one revision code. The chip with the firmware
	 * loaded and running returns a *different* revision
	 * code. This confuses the quirk mechanism, which is
	 * dependent on the revision data.
	 */
	(void)usbd_reload_device_desc(sc->kue_udev);

	DPRINTFN(1,("%s: %s: done\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	/* Reset the adapter. */
	kue_reset(sc);

	return (0);
}

Static void
kue_setmulti(sc)
	struct kue_softc	*sc;
{
	struct ifnet		*ifp = GET_IFP(sc);
#if defined(__FreeBSD__)
	struct ifmultiaddr	*ifma;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct ether_multi	*enm;
	struct ether_multistep	step;
#endif
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		sc->kue_rxfilt |= KUE_RXFILT_ALLMULTI;
		sc->kue_rxfilt &= ~KUE_RXFILT_MULTICAST;
		kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);
		return;
	}

	sc->kue_rxfilt &= ~KUE_RXFILT_ALLMULTI;

	i = 0;
#if defined(__FreeBSD__)
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		/*
		 * If there are too many addresses for the
		 * internal filter, switch over to allmulti mode.
		 */
		if (i == KUE_MCFILTCNT(sc))
			break;
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    KUE_MCFILT(sc, i), ETHER_ADDR_LEN);
		i++;
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#if defined (__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->kue_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
		if (i == KUE_MCFILTCNT(sc))
			break;
#if 0
		if (memcmp(enm->enm_addrlo,
			   enm->enm_addrhi, ETHER_ADDR_LEN) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			/* XXX what now? */
			return;
		}
#endif
		memcpy(KUE_MCFILT(sc, i), enm->enm_addrlo, ETHER_ADDR_LEN);
		ETHER_NEXT_MULTI(step, enm);
		i++;
	}
#endif

	if (i == KUE_MCFILTCNT(sc))
		sc->kue_rxfilt |= KUE_RXFILT_ALLMULTI;
	else {
		sc->kue_rxfilt |= KUE_RXFILT_MULTICAST;
		kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MCAST_FILTERS,
		    i, sc->kue_mcfilters, i * ETHER_ADDR_LEN);
	}

	kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);
}

/*
 * Issue a SET_CONFIGURATION command to reset the MAC. This should be
 * done after the firmware is loaded into the adapter in order to
 * bring it into proper operation.
 */
Static void
kue_reset(sc)
	struct kue_softc	*sc;
{
	usbd_status		err;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	err = usbd_set_config_no(sc->kue_udev, KUE_CONFIG_NO, 0);
	if (err)
		printf("%s: reset failed\n", USBDEVNAME(sc->kue_dev));

	/* Wait a little while for the chip to get its brains in order. */
	usbd_delay_ms(sc->kue_udev, 10);
}

/*
 * Probe for a KLSI chip.
 */
USB_MATCH(kue)
{
	USB_MATCH_START(kue, uaa);
	struct kue_type			*t;

	DPRINTFN(25,("kue_match: enter\n"));

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (t = kue_devs; t->kue_vid != 0; t++)
		if (uaa->vendor == t->kue_vid && uaa->product == t->kue_did)
			return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(kue)
{
	USB_ATTACH_START(kue, sc, uaa);
	char			devinfo[1024];
	int			s;
	struct ifnet		*ifp;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

#ifdef __FreeBSD__
	bzero(sc, sizeof(struct kue_softc));
#endif

	DPRINTFN(5,(" : kue_attach: sc=%p, dev=%p", sc, dev));

	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->kue_dev), devinfo);

	err = usbd_set_config_no(dev, KUE_CONFIG_NO, 0);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->kue_udev = dev;
	sc->kue_product = uaa->product;
	sc->kue_vendor = uaa->vendor;

	/* Load the firmware into the NIC. */
	if (kue_load_fw(sc)) {
		printf("%s: loading firmware failed\n",
		    USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_device2interface_handle(dev, KUE_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->kue_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->kue_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->kue_ed[KUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->kue_ed[KUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->kue_ed[KUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	if (sc->kue_ed[KUE_ENDPT_RX] == 0 || sc->kue_ed[KUE_ENDPT_TX] == 0) {
		printf("%s: missing endpoint\n", USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Read ethernet descriptor */
	err = kue_ctl(sc, KUE_CTL_READ, KUE_CMD_GET_ETHER_DESCRIPTOR,
	    0, &sc->kue_desc, sizeof(sc->kue_desc));
	if (err) {
		printf("%s: could not read Ethernet descriptor\n",
		    USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->kue_mcfilters = malloc(KUE_MCFILTCNT(sc) * ETHER_ADDR_LEN,
	    M_USBDEV, M_NOWAIT);
	if (sc->kue_mcfilters == NULL) {
		printf("%s: no memory for multicast filter buffer\n",
		    USBDEVNAME(sc->kue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	s = splimp();

	/*
	 * A KLSI chip was detected. Inform the world.
	 */
#if defined(__FreeBSD__)
	printf("%s: Ethernet address: %6D\n", USBDEVNAME(sc->kue_dev),
	    sc->kue_desc.kue_macaddr, ":");

	bcopy(sc->kue_desc.kue_macaddr,
	    (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_unit = sc->kue_unit;
	ifp->if_name = "kue";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kue_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = kue_start;
	ifp->if_watchdog = kue_watchdog;
	ifp->if_init = kue_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	kue_qdat.ifp = ifp;
	kue_qdat.if_rxstart = kue_rxstart;

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
	usb_register_netisr();

#elif defined(__NetBSD__) || defined(__OpenBSD__)

	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->kue_dev),
	    ether_sprintf(sc->kue_desc.kue_macaddr));

	/* Initialize interface info.*/
	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kue_ioctl;
	ifp->if_start = kue_start;
	ifp->if_watchdog = kue_watchdog;
#if defined(__OpenBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
	strncpy(ifp->if_xname, USBDEVNAME(sc->kue_dev), IFNAMSIZ);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, sc->kue_desc.kue_macaddr);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->kue_dev),
	    RND_TYPE_NET, 0);
#endif

#endif /* __NetBSD__ */
	sc->kue_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->kue_udev,
			   USBDEV(sc->kue_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(kue)
{
	USB_DETACH_START(kue, sc);
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	s = splusb();		/* XXX why? */

	if (sc->kue_mcfilters != NULL) {
		free(sc->kue_mcfilters, M_USBDEV);
		sc->kue_mcfilters = NULL;
	}

	if (!sc->kue_attached) {
		/* Detached before attached finished, so just bail out. */
		splx(s);
		return (0);
	}

	if (ifp->if_flags & IFF_RUNNING)
		kue_stop(sc);

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
	if (sc->kue_ep[KUE_ENDPT_TX] != NULL ||
	    sc->kue_ep[KUE_ENDPT_RX] != NULL ||
	    sc->kue_ep[KUE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->kue_dev));
#endif

	sc->kue_attached = 0;
	splx(s);

	return (0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
kue_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct kue_softc *sc = (struct kue_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
#if defined(__NetBSD__)
		/* Deactivate the interface. */
		if_deactivate(&sc->kue_ec.ec_if);
#endif
		sc->kue_dying = 1;
		break;
	}
	return (0);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
kue_newbuf(sc, c, m)
	struct kue_softc	*sc;
	struct kue_chain	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->kue_dev));
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->kue_dev));
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	c->kue_mbuf = m_new;

	return (0);
}

Static int
kue_rx_list_init(sc)
	struct kue_softc	*sc;
{
	struct kue_cdata	*cd;
	struct kue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	cd = &sc->kue_cdata;
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		c = &cd->kue_rx_chain[i];
		c->kue_sc = sc;
		c->kue_idx = i;
		if (kue_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->kue_xfer == NULL) {
			c->kue_xfer = usbd_alloc_xfer(sc->kue_udev);
			if (c->kue_xfer == NULL)
				return (ENOBUFS);
			c->kue_buf = usbd_alloc_buffer(c->kue_xfer, KUE_BUFSZ);
			if (c->kue_buf == NULL)
				return (ENOBUFS); /* XXX free xfer */
		}
	}

	return (0);
}

Static int
kue_tx_list_init(sc)
	struct kue_softc	*sc;
{
	struct kue_cdata	*cd;
	struct kue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev), __FUNCTION__));

	cd = &sc->kue_cdata;
	for (i = 0; i < KUE_TX_LIST_CNT; i++) {
		c = &cd->kue_tx_chain[i];
		c->kue_sc = sc;
		c->kue_idx = i;
		c->kue_mbuf = NULL;
		if (c->kue_xfer == NULL) {
			c->kue_xfer = usbd_alloc_xfer(sc->kue_udev);
			if (c->kue_xfer == NULL)
				return (ENOBUFS);
			c->kue_buf = usbd_alloc_buffer(c->kue_xfer, KUE_BUFSZ);
			if (c->kue_buf == NULL)
				return (ENOBUFS);
		}
	}

	return (0);
}

#ifdef __FreeBSD__
Static void
kue_rxstart(ifp)
	struct ifnet		*ifp;
{
	struct kue_softc	*sc;
	struct kue_chain	*c;

	sc = ifp->if_softc;
	c = &sc->kue_cdata.kue_rx_chain[sc->kue_cdata.kue_rx_prod];

	if (kue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
	    c, c->kue_buf, KUE_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, kue_rxeof);
	usbd_transfer(c->kue_xfer);
}
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
kue_rxeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct kue_chain	*c = priv;
	struct kue_softc	*sc = c->kue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m;
	int			total_len = 0;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int			s;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->kue_dev),
		     __FUNCTION__, status));

	if (sc->kue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->kue_rx_errs++;
		if (usbd_ratecheck(&sc->kue_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			    USBDEVNAME(sc->kue_dev), sc->kue_rx_errs,
			    usbd_errstr(status));
			sc->kue_rx_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->kue_ep[KUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	DPRINTFN(10,("%s: %s: total_len=%d len=%d\n", USBDEVNAME(sc->kue_dev),
		     __FUNCTION__, total_len, 
		     UGETW(mtod(c->kue_mbuf, u_int8_t *))));

	if (total_len <= 1)
		goto done;

	m = c->kue_mbuf;
	/* copy data to mbuf */
	memcpy(mtod(m, char*), c->kue_buf, total_len);

	/* No errors; receive the packet. */
	total_len = UGETW(mtod(m, u_int8_t *));
	m_adj(m, sizeof(u_int16_t));

	if (total_len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	m->m_pkthdr.len = m->m_len = total_len;

#if defined(__FreeBSD__)
	m->m_pkthdr.rcvif = (struct ifnet *)&kue_qdat;
	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);

	return;

#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m->m_pkthdr.rcvif = ifp;

	s = splimp();

	/* XXX ugly */
	if (kue_newbuf(sc, c, NULL) == ENOBUFS) {
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

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->kue_dev),
		    __FUNCTION__, m->m_len));
	IF_INPUT(ifp, m);
 done1:
	splx(s);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

 done:

	/* Setup new transfer. */
	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
	    c, c->kue_buf, KUE_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, kue_rxeof);
	usbd_transfer(c->kue_xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->kue_dev),
		    __FUNCTION__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
kue_txeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct kue_chain	*c = priv;
	struct kue_softc	*sc = c->kue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	if (sc->kue_dying)
		return;

	s = splimp();

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->kue_dev),
		    __FUNCTION__, status));

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->kue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->kue_ep[KUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

#if defined(__FreeBSD__)
	c->kue_mbuf->m_pkthdr.rcvif = ifp;
	usb_tx_done(c->kue_mbuf);
	c->kue_mbuf = NULL;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m_freem(c->kue_mbuf);
	c->kue_mbuf = NULL;

	if (ifp->if_snd.ifq_head != NULL)
		kue_start(ifp);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	splx(s);
}

Static int
kue_send(sc, m, idx)
	struct kue_softc	*sc;
	struct mbuf		*m;
	int			idx;
{
	int			total_len;
	struct kue_chain	*c;
	usbd_status		err;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	c = &sc->kue_cdata.kue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->kue_buf + 2);
	c->kue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;
	/* XXX what's this? */
	total_len += 64 - (total_len % 64);

	/* Frame length is specified in the first 2 bytes of the buffer. */
	c->kue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->kue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_TX],
	    c, c->kue_buf, total_len, USBD_NO_COPY, USBD_DEFAULT_TIMEOUT, kue_txeof);

	/* Transmit */
	err = usbd_transfer(c->kue_xfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("%s: kue_send err=%s\n", USBDEVNAME(sc->kue_dev),
			 usbd_errstr(err)));
		kue_stop(sc);
		return (EIO);
	}

	sc->kue_cdata.kue_tx_cnt++;

	return (0);
}

Static void
kue_start(ifp)
	struct ifnet		*ifp;
{
	struct kue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	if (sc->kue_dying)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (kue_send(sc, m_head, 0)) {
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
kue_init(xsc)
	void			*xsc;
{
	struct kue_softc	*sc = xsc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;
	u_char			*eaddr;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splimp();

#if defined(__FreeBSD__) || defined(__OpenBSD__)
	eaddr = sc->arpcom.ac_enaddr;
#elif defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#endif /* defined(__NetBSD__) */
	/* Set MAC address */
	kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MAC, 0, eaddr, ETHER_ADDR_LEN);

	sc->kue_rxfilt = KUE_RXFILT_UNICAST | KUE_RXFILT_BROADCAST;

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		sc->kue_rxfilt |= KUE_RXFILT_PROMISC;

	kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);

	/* I'm not sure how to tune these. */
#if 0
	/*
	 * Leave this one alone for now; setting it
	 * wrong causes lockups on some machines/controllers.
	 */
	kue_setword(sc, KUE_CMD_SET_SOFS, 1);
#endif
	kue_setword(sc, KUE_CMD_SET_URB_SIZE, 64);

	/* Init TX ring. */
	if (kue_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->kue_dev));
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (kue_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->kue_dev));
		splx(s);
		return;
	}

	/* Load the multicast filter. */
	kue_setmulti(sc);

	if (sc->kue_ep[KUE_ENDPT_RX] == NULL) {
		if (kue_open_pipes(sc)) {
			splx(s);
			return;
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

Static int
kue_open_pipes(sc)
	struct kue_softc	*sc;
{
	usbd_status		err;
	struct kue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->kue_iface, sc->kue_ed[KUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->kue_ep[KUE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		return (EIO);
	}

	err = usbd_open_pipe(sc->kue_iface, sc->kue_ed[KUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->kue_ep[KUE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		return (EIO);
	}

	/* Start up the receive pipe. */
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		c = &sc->kue_cdata.kue_rx_chain[i];
		usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
		    c, c->kue_buf, KUE_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    kue_rxeof);
		DPRINTFN(5,("%s: %s: start read\n", USBDEVNAME(sc->kue_dev),
			    __FUNCTION__));
		usbd_transfer(c->kue_xfer);
	}

	return (0);
}

Static int
kue_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct kue_softc	*sc = ifp->if_softc;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
#endif
	int			s, error = 0;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	if (sc->kue_dying)
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
		kue_init(sc);

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
			    !(sc->kue_if_flags & IFF_PROMISC)) {
				sc->kue_rxfilt |= KUE_RXFILT_PROMISC;
				kue_setword(sc, KUE_CMD_SET_PKT_FILTER,
				    sc->kue_rxfilt);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->kue_if_flags & IFF_PROMISC) {
				sc->kue_rxfilt &= ~KUE_RXFILT_PROMISC;
				kue_setword(sc, KUE_CMD_SET_PKT_FILTER,
				    sc->kue_rxfilt);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				kue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				kue_stop(sc);
		}
		sc->kue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		kue_setmulti(sc);
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
kue_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct kue_softc	*sc = ifp->if_softc;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	if (sc->kue_dying)
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->kue_dev));

	/*
	 * The polling business is a kludge to avoid allowing the
	 * USB code to call tsleep() in usbd_delay_ms(), which will
	 * kill us since the watchdog routine is invoked from
	 * interrupt context.
	 */
	usbd_set_polling(sc->kue_udev, 1);
	kue_stop(sc);
	kue_init(sc);
	usbd_set_polling(sc->kue_udev, 0);

	if (ifp->if_snd.ifq_head != NULL)
		kue_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
kue_stop(sc)
	struct kue_softc	*sc;
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->kue_dev),__FUNCTION__));

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	/* Stop transfers. */
	if (sc->kue_ep[KUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_RX] = NULL;
	}

	if (sc->kue_ep[KUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_TX] = NULL;
	}

	if (sc->kue_ep[KUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->kue_dev), usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		if (sc->kue_cdata.kue_rx_chain[i].kue_mbuf != NULL) {
			m_freem(sc->kue_cdata.kue_rx_chain[i].kue_mbuf);
			sc->kue_cdata.kue_rx_chain[i].kue_mbuf = NULL;
		}
		if (sc->kue_cdata.kue_rx_chain[i].kue_xfer != NULL) {
			usbd_free_xfer(sc->kue_cdata.kue_rx_chain[i].kue_xfer);
			sc->kue_cdata.kue_rx_chain[i].kue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < KUE_TX_LIST_CNT; i++) {
		if (sc->kue_cdata.kue_tx_chain[i].kue_mbuf != NULL) {
			m_freem(sc->kue_cdata.kue_tx_chain[i].kue_mbuf);
			sc->kue_cdata.kue_tx_chain[i].kue_mbuf = NULL;
		}
		if (sc->kue_cdata.kue_tx_chain[i].kue_xfer != NULL) {
			usbd_free_xfer(sc->kue_cdata.kue_tx_chain[i].kue_xfer);
			sc->kue_cdata.kue_tx_chain[i].kue_xfer = NULL;
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
kue_shutdown(dev)
	device_t		dev;
{
	struct kue_softc	*sc;

	sc = device_get_softc(dev);

	kue_stop(sc);
}
#endif
