/*	$OpenBSD: if_aue.c,v 1.2 2000/03/30 16:19:32 aaron Exp $ */
/*	$NetBSD: if_aue.c,v 1.36 2000/03/30 00:18:17 augustss Exp $	*/
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
 * $FreeBSD: src/sys/dev/usb/if_aue.c,v 1.11 2000/01/14 01:36:14 wpaul Exp $
 */

/*
 * ADMtek AN986 Pegasus USB to ethernet driver. Datasheet is available
 * from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Pegasus chip uses four USB "endpoints" to provide 10/100 ethernet
 * support: the control endpoint for reading/writing registers, burst
 * read endpoint for packet reception, burst write for packet transmission
 * and one for "interrupts." The chip uses the same RX filter scheme
 * as the other ADMtek ethernet parts: one perfect filter entry for the
 * the station address and a 64-bit multicast hash table. The chip supports
 * both MII and HomePNA attachments.
 *
 * Since the maximum data transfer speed of USB is supposed to be 12Mbps,
 * you're never really going to get 100Mbps speeds from this device. I
 * think the idea is to allow the device to connect to 10 or 100Mbps
 * networks, not necessarily to provide 100Mbps performance. Also, since
 * the controller uses an external PHY chip, it's possible that board
 * designers might simply choose a 10Mbps PHY.
 *
 * Registers are accessed using usbd_do_request(). Packet transfers are
 * done using usbd_transfer() and friends.
 */

/*
 * Ported to NetBSD and somewhat rewritten by Lennart Augustsson.
 */

/*
 * TODO:
 * better error messages from rxstat
 * split out if_auevar.h
 * add thread to avoid register reads from interrupt context
 * more error checks
 * investigate short rx problem
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
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#if defined(__FreeBSD__)

#include <net/ethernet.h>
#include <machine/clock.h>	/* for DELAY */
#include <sys/bus.h>
/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

#include <net/if.h>
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <net/if_arp.h>
#endif
#include <net/if_dl.h>
#include <net/if_media.h>

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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#ifdef __FreeBSD__
#include <dev/usb/usb_ethersubr.h>
#endif

#include <dev/usb/if_auereg.h>

#ifdef AUE_DEBUG
#define DPRINTF(x)	if (auedebug) logprintf x
#define DPRINTFN(n,x)	if (auedebug >= (n)) logprintf x
int	auedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static struct aue_type aue_devs[] = {
	{ USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USB100 },
	{ USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUATX },
	{ USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB100TX },
	{ USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUS },
	{ USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX },
	{ USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX_PNA },
	{ USB_VENDOR_SMC, USB_PRODUCT_SMC_2202USB },
	{ USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TX },
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBETTX },
	{ 0, 0 }
};

USB_DECLARE_DRIVER(aue);

Static int aue_tx_list_init	__P((struct aue_softc *));
Static int aue_rx_list_init	__P((struct aue_softc *));
Static int aue_newbuf		__P((struct aue_softc *, struct aue_chain *,
				    struct mbuf *));
Static int aue_send		__P((struct aue_softc *, struct mbuf *, int));
Static void aue_intr		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void aue_rxeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void aue_txeof		__P((usbd_xfer_handle,
				    usbd_private_handle, usbd_status));
Static void aue_tick		__P((void *));
Static void aue_start		__P((struct ifnet *));
Static int aue_ioctl		__P((struct ifnet *, u_long, caddr_t));
Static void aue_init		__P((void *));
Static void aue_stop		__P((struct aue_softc *));
Static void aue_watchdog	__P((struct ifnet *));
#ifdef __FreeBSD__
Static void aue_shutdown	__P((device_ptr_t));
#endif
Static int aue_openpipes	__P((struct aue_softc *));
Static int aue_ifmedia_upd	__P((struct ifnet *));
Static void aue_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

Static int aue_eeprom_getword	__P((struct aue_softc *, int));
Static void aue_read_mac	__P((struct aue_softc *, u_char *));
Static int aue_miibus_readreg	__P((device_ptr_t, int, int));
#if defined(__FreeBSD__)
Static int aue_miibus_writereg	__P((device_ptr_t, int, int, int));
#elif defined(__NetBSD__) || defined(__OpenBSD__)
Static void aue_miibus_writereg	__P((device_ptr_t, int, int, int));
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
Static void aue_miibus_statchg	__P((device_ptr_t));

Static void aue_setmulti	__P((struct aue_softc *));
Static u_int32_t aue_crc	__P((caddr_t));
Static void aue_reset		__P((struct aue_softc *));

Static int aue_csr_read_1	__P((struct aue_softc *, int));
Static int aue_csr_write_1	__P((struct aue_softc *, int, int));
Static int aue_csr_read_2	__P((struct aue_softc *, int));
Static int aue_csr_write_2	__P((struct aue_softc *, int, int));

#if defined(__FreeBSD__)
#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/usb/if_aue.c,v 1.11 2000/01/14 01:36:14 wpaul Exp $";
#endif

Static void aue_rxstart		__P((struct ifnet *));

Static struct usb_qdat aue_qdat;

Static device_method_t aue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aue_match),
	DEVMETHOD(device_attach,	aue_attach),
	DEVMETHOD(device_detach,	aue_detach),
	DEVMETHOD(device_shutdown,	aue_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	aue_miibus_readreg),
	DEVMETHOD(miibus_writereg,	aue_miibus_writereg),
	DEVMETHOD(miibus_statchg,	aue_miibus_statchg),

	{ 0, 0 }
};

Static driver_t aue_driver = {
	"aue",
	aue_methods,
	sizeof(struct aue_softc)
};

Static devclass_t aue_devclass;

DRIVER_MODULE(if_aue, uhub, aue_driver, aue_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, aue, miibus_driver, miibus_devclass, 0, 0);

#endif /* __FreeBSD__ */

#define AUE_DO_REQUEST(dev, req, data)			\
	usbd_do_request_flags(dev, req, data, USBD_NO_TSLEEP, NULL)

#define AUE_SETBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) | (x))

#define AUE_CLRBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) & ~(x))

Static int
aue_csr_read_1(sc, reg)
	struct aue_softc	*sc;
	int			reg;
{
	usb_device_request_t	req;
	usbd_status		err;
	uByte			val = 0;
	int			s;

	if (sc->aue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	s = splusb();
	err = AUE_DO_REQUEST(sc->aue_udev, &req, &val);
	splx(s);

	if (err) {
		DPRINTF(("%s: aue_csr_read_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (val);
}

Static int
aue_csr_read_2(sc, reg)
	struct aue_softc	*sc;
	int			reg;
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;
	int			s;

	if (sc->aue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	s = splusb();
	err = AUE_DO_REQUEST(sc->aue_udev, &req, &val);
	splx(s);

	if (err) {
		DPRINTF(("%s: aue_csr_read_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (UGETW(val));
}

Static int
aue_csr_write_1(sc, reg, aval)
	struct aue_softc	*sc;
	int			reg, aval;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;
	uByte			val;

	if (sc->aue_dying)
		return (0);

	val = aval;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	s = splusb();
	err = AUE_DO_REQUEST(sc->aue_udev, &req, &val);
	splx(s);

	if (err) {
		DPRINTF(("%s: aue_csr_write_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

Static int
aue_csr_write_2(sc, reg, aval)
	struct aue_softc	*sc;
	int			reg, aval;
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;
	uWord			val;

	if (sc->aue_dying)
		return (0);

	USETW(val, aval);
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, aval);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	s = splusb();
	err = AUE_DO_REQUEST(sc->aue_udev, &req, &val);
	splx(s);

	if (err) {
		DPRINTF(("%s: aue_csr_write_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
Static int
aue_eeprom_getword(sc, addr)
	struct aue_softc	*sc;
	int			addr;
{
	int		i;

	aue_csr_write_1(sc, AUE_EE_REG, addr);
	aue_csr_write_1(sc, AUE_EE_CTL, AUE_EECTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_EE_CTL) & AUE_EECTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: EEPROM read timed out\n",
		    USBDEVNAME(sc->aue_dev));
	}

	return (aue_csr_read_2(sc, AUE_EE_DATA));
}

/*
 * Read the MAC from the EEPROM.  It's at offset 0.
 */
Static void
aue_read_mac(sc, dest)
	struct aue_softc	*sc;
	u_char			*dest;
{
	int			i;
	int			off = 0;
	int			word;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	for (i = 0; i < 3; i++) {
		word = aue_eeprom_getword(sc, off + i);
		dest[2 * i] = (u_char)word;
		dest[2 * i + 1] = (u_char)(word >> 8);
	}
}

Static int
aue_miibus_readreg(dev, phy, reg)
	device_ptr_t		dev;
	int			phy, reg;
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;
	u_int16_t		val;

	/*
	 * The Am79C901 HomePNA PHY actually contains
	 * two transceivers: a 1Mbps HomePNA PHY and a
	 * 10Mbps full/half duplex ethernet PHY with
	 * NWAY autoneg. However in the ADMtek adapter,
	 * only the 1Mbps PHY is actually connected to
	 * anything, so we ignore the 10Mbps one. It
	 * happens to be configured for MII address 3,
	 * so we filter that out.
	 */
	if (sc->aue_vendor == USB_VENDOR_ADMTEK &&
	    sc->aue_product == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy != 1)
			return (0);
	}

	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: MII read timed out\n",
		    USBDEVNAME(sc->aue_dev));
	}

	val = aue_csr_read_2(sc, AUE_PHY_DATA);

	DPRINTFN(11,("%s: %s: phy=%d reg=%d => 0x%04x\n",
		     USBDEVNAME(sc->aue_dev), __FUNCTION__, phy, reg, val));

	return (val);
}

#if defined(__FreeBSD__)
Static int
#elif defined(__NetBSD__) || defined(__OpenBSD__)
Static void
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
aue_miibus_writereg(dev, phy, reg, data)
	device_ptr_t		dev;
	int			phy, reg, data;
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;

	if (sc->aue_vendor == USB_VENDOR_ADMTEK &&
	    sc->aue_product == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy == 3)
#if defined(__FreeBSD__)
			return (0);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
			return;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
	}

	DPRINTFN(11,("%s: %s: phy=%d reg=%d data=0x%04x\n",
		     USBDEVNAME(sc->aue_dev), __FUNCTION__, phy, reg, data));

	aue_csr_write_2(sc, AUE_PHY_DATA, data);
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_WRITE);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: MII read timed out\n",
		    USBDEVNAME(sc->aue_dev));
	}

#if defined(__FreeBSD__)
	return (0);
#endif
}

Static void
aue_miibus_statchg(dev)
	device_ptr_t		dev;
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	} else {
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);
	else
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);

	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);

	/*
	 * Set the LED modes on the LinkSys adapter.
	 * This turns on the 'dual link LED' bin in the auxmode
	 * register of the Broadcom PHY.
	 */
	if ((sc->aue_vendor == USB_VENDOR_LINKSYS &&
	     sc->aue_product == USB_PRODUCT_LINKSYS_USB100TX) ||
	    (sc->aue_vendor == USB_VENDOR_DLINK &&
	     sc->aue_product == USB_PRODUCT_DLINK_DSB650TX)) {
		u_int16_t               auxmode;
		auxmode = aue_miibus_readreg(dev, 0, 0x1b);
		aue_miibus_writereg(dev, 0, 0x1b, auxmode | 0x04);
	}
}

#define AUE_POLY	0xEDB88320
#define AUE_BITS	6

Static u_int32_t 
aue_crc(addr)
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? AUE_POLY : 0);
	}

	return (crc & ((1 << AUE_BITS) - 1));
}

Static void
aue_setmulti(sc)
	struct aue_softc	*sc;
{
	struct ifnet		*ifp;
#if defined(__FreeBSD__)
	struct ifmultiaddr	*ifma;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct ether_multi	*enm;
	struct ether_multistep	step;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
	u_int32_t		h = 0, i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	ifp = GET_IFP(sc);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
		return;
	}

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 8; i++)
		aue_csr_write_1(sc, AUE_MAR0 + i, 0);

	/* now program new ones */
#if defined(__FreeBSD__)
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = aue_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		AUE_SETBIT(sc, AUE_MAR + (h >> 3), 1 << (h & 0xF));
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->aue_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
#if 1
		if (memcmp(enm->enm_addrlo,
			   enm->enm_addrhi, ETHER_ADDR_LEN) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
			return;
		}
#endif
		h = aue_crc(enm->enm_addrlo);
		AUE_SETBIT(sc, AUE_MAR + (h >> 3), 1 << (h & 0xF));
		ETHER_NEXT_MULTI(step, enm);
	}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
}

Static void
aue_reset(sc)
	struct aue_softc	*sc;
{
	int		i;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_RESETMAC);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (!(aue_csr_read_1(sc, AUE_CTL1) & AUE_CTL1_RESETMAC))
			break;
	}

	if (i == AUE_TIMEOUT)
		printf("%s: reset failed\n", USBDEVNAME(sc->aue_dev));

	/*
	 * The PHY(s) attached to the Pegasus chip may be held
	 * in reset until we flip on the GPIO outputs. Make sure
	 * to set the GPIO pins high so that the PHY(s) will
	 * be enabled.
	 *
	 * Note: We force all of the GPIO pins low first, *then*
	 * enable the ones we want.
  	 */
	aue_csr_write_1(sc, AUE_GPIO0, 
	    AUE_GPIO_OUT0 | AUE_GPIO_SEL0);
  	aue_csr_write_1(sc, AUE_GPIO0,
	    AUE_GPIO_OUT0 | AUE_GPIO_SEL0 | AUE_GPIO_SEL1);
  
	/* Grrr. LinkSys has to be different from everyone else. */
	if ((sc->aue_vendor == USB_VENDOR_LINKSYS &&
	     sc->aue_product == USB_PRODUCT_LINKSYS_USB100TX) ||
	    (sc->aue_vendor == USB_VENDOR_DLINK &&
	     sc->aue_product == USB_PRODUCT_DLINK_DSB650TX)) {
		aue_csr_write_1(sc, AUE_GPIO0, 
		    AUE_GPIO_SEL0 | AUE_GPIO_SEL1);
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_SEL0 | AUE_GPIO_SEL1 | AUE_GPIO_OUT0);
	}

	/* Wait a little while for the chip to get its brains in order. */
	delay(10000);		/* XXX */
}

/*
 * Probe for a Pegasus chip.
 */
USB_MATCH(aue)
{
	USB_MATCH_START(aue, uaa);
	struct aue_type			*t;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	for (t = aue_devs; t->aue_vid != 0; t++)
		if (uaa->vendor == t->aue_vid && uaa->product == t->aue_did)
			return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(aue)
{
	USB_ATTACH_START(aue, sc, uaa);
	char			devinfo[1024];
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	struct mii_data		*mii;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

#ifdef __FreeBSD__
	bzero(sc, sizeof(struct aue_softc));
#endif

	DPRINTFN(5,(" : aue_attach: sc=%p", sc));

	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->aue_dev), devinfo);

	err = usbd_set_config_no(dev, AUE_CONFIG_NO, 0);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_device2interface_handle(dev, AUE_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->aue_udev = dev;
	sc->aue_iface = iface;
	sc->aue_product = uaa->product;
	sc->aue_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get endpoint descriptor %d\n",
			    USBDEVNAME(sc->aue_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->aue_ed[AUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	if (sc->aue_ed[AUE_ENDPT_RX] == 0 || sc->aue_ed[AUE_ENDPT_TX] == 0 ||
	    sc->aue_ed[AUE_ENDPT_INTR] == 0) {
		printf("%s: missing endpoint\n", USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}


	s = splimp();

	/* Reset the adapter. */
	aue_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	aue_read_mac(sc, eaddr);

	/*
	 * A Pegasus chip was detected. Inform the world.
	 */
	ifp = GET_IFP(sc);
#if defined(__FreeBSD__)
	printf("%s: Ethernet address: %6D\n", USBDEVNAME(sc->aue_dev),
	    eaddr, ":");

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	ifp->if_softc = sc;
	ifp->if_unit = sc->aue_unit;
	ifp->if_name = "aue";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = aue_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = aue_start;
	ifp->if_watchdog = aue_watchdog;
	ifp->if_init = aue_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	/*
	 * Do MII setup.
	 * NOTE: Doing this causes child devices to be attached to us,
	 * which we would normally disconnect at in the detach routine
	 * using device_delete_child(). However the USB code is set up
	 * such that when this driver is removed, all childred devices
	 * are removed as well. In effect, the USB code ends up detaching
	 * all of our children for us, so we don't have to do is ourselves
	 * in aue_detach(). It's important to point this out since if
	 * we *do* try to detach the child devices ourselves, we will
	 * end up getting the children deleted twice, which will crash
	 * the system.
	 */
	if (mii_phy_probe(self, &sc->aue_miibus,
	    aue_ifmedia_upd, aue_ifmedia_sts)) {
		printf("%s: MII without any PHY!\n", USBDEVNAME(sc->aue_dev));
		splx(s);
		USB_ATTACH_ERROR_RETURN;
	}

	aue_qdat.ifp = ifp;
	aue_qdat.if_rxstart = aue_rxstart;

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));

	usb_register_netisr();

#elif defined(__NetBSD__) || defined(__OpenBSD__)

	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->aue_dev),
	    ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = aue_ioctl;
	ifp->if_start = aue_start;
	ifp->if_watchdog = aue_watchdog;
#if defined(__OpenBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
	strncpy(ifp->if_xname, USBDEVNAME(sc->aue_dev), IFNAMSIZ);

	/* Initialize MII/media info. */
	mii = &sc->aue_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = aue_miibus_readreg;
	mii->mii_writereg = aue_miibus_writereg;
	mii->mii_statchg = aue_miibus_statchg;
	ifmedia_init(&mii->mii_media, 0, aue_ifmedia_upd, aue_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->aue_dev),
	    RND_TYPE_NET, 0);
#endif

#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	usb_callout_init(sc->aue_stat_ch);

	sc->aue_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->aue_udev,
			   USBDEV(sc->aue_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(aue)
{
	USB_DETACH_START(aue, sc);
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	s = splusb();

	usb_uncallout(sc->aue_stat_ch, aue_tick, sc);

	if (!sc->aue_attached) {
		/* Detached before attached finished, so just bail out. */
		splx(s);
		return (0);
	}

	if (ifp->if_flags & IFF_RUNNING)
		aue_stop(sc);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
	mii_detach(&sc->aue_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->aue_mii.mii_media, IFM_INST_ANY);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ether_ifdetach(ifp);
#endif /* __NetBSD__ */

	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->aue_ep[AUE_ENDPT_TX] != NULL ||
	    sc->aue_ep[AUE_ENDPT_RX] != NULL ||
	    sc->aue_ep[AUE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->aue_dev));
#endif

	sc->aue_attached = 0;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->aue_udev, 
			   USBDEV(sc->aue_dev));

	return (0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
aue_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct aue_softc *sc = (struct aue_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->aue_ec.ec_if);
		sc->aue_dying = 1;
		break;
	}
	return (0);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
aue_newbuf(sc, c, m)
	struct aue_softc	*sc;
	struct aue_chain	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__FUNCTION__));

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->aue_dev));
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->aue_dev));
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
	c->aue_mbuf = m_new;

	return (0);
}

Static int 
aue_rx_list_init(sc)
	struct aue_softc	*sc;
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &cd->aue_rx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		if (aue_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return (ENOBUFS);
			c->aue_buf = usbd_alloc_buffer(c->aue_xfer, AUE_BUFSZ);
			if (c->aue_buf == NULL)
				return (ENOBUFS); /* XXX free xfer */
		}
	}

	return (0);
}

Static int
aue_tx_list_init(sc)
	struct aue_softc	*sc;
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		c = &cd->aue_tx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		c->aue_mbuf = NULL;
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return (ENOBUFS);
			c->aue_buf = usbd_alloc_buffer(c->aue_xfer, AUE_BUFSZ);
			if (c->aue_buf == NULL)
				return (ENOBUFS);
		}
	}

	return (0);
}

Static void
aue_intr(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct aue_softc	*sc = priv;
	struct ifnet		*ifp = GET_IFP(sc);
	struct aue_intrpkt	*p = &sc->aue_cdata.aue_ibuf;

	DPRINTFN(15,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__FUNCTION__));

	if (sc->aue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
		printf("%s: usb error on intr: %s\n", USBDEVNAME(sc->aue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		return;
	}

	if (p->aue_txstat0)
		ifp->if_oerrors++;

	if (p->aue_txstat0 & (AUE_TXSTAT0_LATECOLL | AUE_TXSTAT0_EXCESSCOLL))
		ifp->if_collisions++;
}

#if defined(__FreeBSD__)
Static void
aue_rxstart(ifp)
	struct ifnet		*ifp;
{
	struct aue_softc	*sc;
	struct aue_chain	*c;

	sc = ifp->if_softc;
	c = &sc->aue_cdata.aue_rx_chain[sc->aue_cdata.aue_rx_prod];

	if (aue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, mtod(c->aue_mbuf, char *), AUE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(c->aue_xfer);
}
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * Grrr. Receiving transfers larger than about 1152 bytes sometimes
 * doesn't work. We get an incomplete frame. In order to avoid
 * this, we queue up RX transfers that are shorter than a full sized
 * frame. If the received frame is larger than our transfer size,
 * we snag the rest of the data using a second transfer. Does this
 * hurt performance? Yes. But after fighting with this stupid thing
 * for three days, I'm willing to settle. I'd rather have reliable
 * receive performance that fast but spotty performance.
 */
Static void
aue_rxeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct aue_chain	*c = priv;
	struct aue_softc	*sc = c->aue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m;
	u_int32_t		total_len;
	struct aue_rxpkt	r;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int			s;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__FUNCTION__));

	if (sc->aue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->aue_rx_errs++;
		if (usbd_ratecheck(&sc->aue_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			    USBDEVNAME(sc->aue_dev), sc->aue_rx_errs,
			    usbd_errstr(status));
			sc->aue_rx_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	memcpy(mtod(c->aue_mbuf, char*), c->aue_buf, total_len);

	if (total_len <= 4 + ETHER_CRC_LEN) {
		ifp->if_ierrors++;
		goto done;
	}

	memcpy(&r, c->aue_buf + total_len - 4, sizeof(r));

	/* Turn off all the non-error bits in the rx status word. */
	r.aue_rxstat &= AUE_RXSTAT_MASK;
	if (r.aue_rxstat) {
		ifp->if_ierrors++;
		goto done;
	}

	/* No errors; receive the packet. */
	m = c->aue_mbuf;
	total_len -= ETHER_CRC_LEN + 4;
	m->m_pkthdr.len = m->m_len = total_len;
	ifp->if_ipackets++;

#if defined(__FreeBSD__)
	m->m_pkthdr.rcvif = (struct ifnet *)&kue_qdat;
	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);

	return;

#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m->m_pkthdr.rcvif = ifp;

	s = splimp();

	/* XXX ugly */
	if (aue_newbuf(sc, c, NULL) == ENOBUFS) {
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

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->aue_dev),
		    __FUNCTION__, m->m_len));
	IF_INPUT(ifp, m);
 done1:
	splx(s);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

 done:

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, c->aue_buf, AUE_BUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->aue_dev),
		    __FUNCTION__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
aue_txeof(xfer, priv, status)
	usbd_xfer_handle	xfer;
	usbd_private_handle	priv;
	usbd_status		status;
{
	struct aue_chain	*c = priv;
	struct aue_softc	*sc = c->aue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	if (sc->aue_dying)
		return;

	s = splimp();

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->aue_dev),
		    __FUNCTION__, status));

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->aue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

#if defined(__FreeBSD__)
	c->aue_mbuf->m_pkthdr.rcvif = ifp;
	usb_tx_done(c->aue_mbuf);
  	c->aue_mbuf = NULL;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	m_freem(c->aue_mbuf);
	c->aue_mbuf = NULL;

	if (ifp->if_snd.ifq_head != NULL)
		aue_start(ifp);
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

	splx(s);
}

Static void
aue_tick(xsc)
	void			*xsc;
{
	struct aue_softc	*sc = xsc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	int			s;

	DPRINTFN(15,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__FUNCTION__));

	if (sc == NULL)
		return;

	if (sc->aue_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);
	if (mii == NULL)
		return;

	s = splimp();

	mii_tick(mii);
	if (!sc->aue_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			DPRINTFN(2,("%s: %s: got link\n",
				    USBDEVNAME(sc->aue_dev),__FUNCTION__));
			sc->aue_link++;
			if (ifp->if_snd.ifq_head != NULL)
				aue_start(ifp);
		}
	}

	usb_callout(sc->aue_stat_ch, hz, aue_tick, sc);

	splx(s);
}

Static int
aue_send(sc, m, idx)
	struct aue_softc	*sc;
	struct mbuf		*m;
	int			idx;
{
	int			total_len;
	struct aue_chain	*c;
	usbd_status		err;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__FUNCTION__));

	c = &sc->aue_cdata.aue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->aue_buf + 2);
	c->aue_mbuf = m;

	/*
	 * The ADMtek documentation says that the packet length is
	 * supposed to be specified in the first two bytes of the
	 * transfer, however it actually seems to ignore this info
	 * and base the frame size on the bulk transfer length.
	 */
	c->aue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->aue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);
	total_len = m->m_pkthdr.len + 2;

	usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_TX],
	    c, c->aue_buf, total_len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    AUE_TX_TIMEOUT, aue_txeof);

	/* Transmit */
	err = usbd_transfer(c->aue_xfer);
	if (err != USBD_IN_PROGRESS) {
		aue_stop(sc);
		return (EIO);
	}
	DPRINTFN(5,("%s: %s: send %d bytes\n", USBDEVNAME(sc->aue_dev),
		    __FUNCTION__, total_len));

	sc->aue_cdata.aue_tx_cnt++;

	return (0);
}

Static void
aue_start(ifp)
	struct ifnet		*ifp;
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	DPRINTFN(5,("%s: %s: enter, link=%d\n", USBDEVNAME(sc->aue_dev),
		    __FUNCTION__, sc->aue_link));

	if (sc->aue_dying)
		return;

	if (!sc->aue_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (aue_send(sc, m_head, 0)) {
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
aue_init(xsc)
	void			*xsc;
{
	struct aue_softc	*sc = xsc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mii_data		*mii = GET_MII(sc);
	int			i, s;
	u_char			*eaddr;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	if (sc->aue_dying)
		return;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	aue_reset(sc);

#if defined(__FreeBSD__) || defined(__OpenBSD__)
	eaddr = sc->arpcom.ac_enaddr;
#elif defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#endif /* defined(__NetBSD__) */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		aue_csr_write_1(sc, AUE_PAR0 + i, eaddr[i]);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	else
		AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);

	/* Init TX ring. */
	if (aue_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->aue_dev));
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (aue_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->aue_dev));
		splx(s);
		return;
	}

	/* Load the multicast filter. */
	aue_setmulti(sc);

	/* Enable RX and TX */
	aue_csr_write_1(sc, AUE_CTL0, AUE_CTL0_RXSTAT_APPEND | AUE_CTL0_RX_ENB);
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_TX_ENB);
	AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_EP3_CLR);

	mii_mediachg(mii);

	if (sc->aue_ep[AUE_ENDPT_RX] == NULL) {
		if (aue_openpipes(sc)) {
			splx(s);
			return;
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout(sc->aue_stat_ch, hz, aue_tick, sc);
}

Static int
aue_openpipes(sc)
	struct aue_softc	*sc;
{
	struct aue_chain	*c;
	usbd_status		err;
	int i;

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}
	usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe_intr(sc->aue_iface, sc->aue_ed[AUE_ENDPT_INTR],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_INTR], sc,
	    &sc->aue_cdata.aue_ibuf, AUE_INTR_PKTLEN, aue_intr, 
	    AUE_INTR_INTERVAL);
	if (err) {
		printf("%s: open intr pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}

	/* Start up the receive pipe. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &sc->aue_cdata.aue_rx_chain[i];
		usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_RX],
		    c, c->aue_buf, AUE_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    aue_rxeof);
		(void)usbd_transfer(c->aue_xfer); /* XXX */
		DPRINTFN(5,("%s: %s: start read\n", USBDEVNAME(sc->aue_dev),
			    __FUNCTION__));

	}
	return (0);
}

/*
 * Set media options.
 */
Static int
aue_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	if (sc->aue_dying)
		return (0);

	sc->aue_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			 mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */
Static void
aue_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

Static int
aue_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct aue_softc	*sc = ifp->if_softc;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct ifaddr 		*ifa = (struct ifaddr *)data;
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */
	struct ifreq		*ifr = (struct ifreq *)data;
	struct mii_data		*mii;
	int			s, error = 0;

	if (sc->aue_dying)
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
		aue_init(sc);

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
			    !(sc->aue_if_flags & IFF_PROMISC)) {
				AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->aue_if_flags & IFF_PROMISC) {
				AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				aue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				aue_stop(sc);
		}
		sc->aue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		aue_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return (error);
}

Static void
aue_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct aue_softc	*sc = ifp->if_softc;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->aue_dev));

	/*
	 * The polling business is a kludge to avoid allowing the
	 * USB code to call tsleep() in usbd_delay_ms(), which will
	 * kill us since the watchdog routine is invoked from
	 * interrupt context.
	 */
	usbd_set_polling(sc->aue_udev, 1);
	aue_stop(sc);
	aue_init(sc);
	usbd_set_polling(sc->aue_udev, 0);

	if (ifp->if_snd.ifq_head != NULL)
		aue_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
aue_stop(sc)
	struct aue_softc	*sc;
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	aue_csr_write_1(sc, AUE_CTL0, 0);
	aue_csr_write_1(sc, AUE_CTL1, 0);
	aue_reset(sc);
	usb_uncallout(sc->aue_stat_ch, aue_tick, sc);

	/* Stop transfers. */
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_RX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_TX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_rx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_rx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_rx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_rx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_rx_chain[i].aue_xfer);
			sc->aue_cdata.aue_rx_chain[i].aue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_tx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_tx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_tx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_tx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_tx_chain[i].aue_xfer);
			sc->aue_cdata.aue_tx_chain[i].aue_xfer = NULL;
		}
	}

	sc->aue_link = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

#ifdef __FreeBSD__
/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
Static void
aue_shutdown(dev)
	device_ptr_t		dev;
{
	struct aue_softc	*sc = USBGETSOFTC(dev);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __FUNCTION__));

	aue_reset(sc);
	aue_stop(sc);
}
#endif
