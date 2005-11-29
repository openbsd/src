/*	$OpenBSD: if_axe.c,v 1.41 2005/11/29 23:16:58 jsg Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 */

#include <sys/cdefs.h>

/*
 * ASIX Electronics AX88172 USB 2.0 ethernet driver. Used in the
 * LinkSys USB200M and various other adapters.
 *
 * Manuals available from:
 * http://www.asix.com.tw/datasheet/mac/Ax88172.PDF
 * Note: you need the manual for the AX88170 chip (USB 1.x ethernet
 * controller) to find the definitions for the RX control register.
 * http://www.asix.com.tw/datasheet/mac/Ax88170.PDF
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Engineer
 * Wind River Systems
 */

/*
 * The AX88172 provides USB ethernet supports at 10 and 100Mbps.
 * It uses an external PHY (reference designs use a RealTek chip),
 * and has a 64-bit multicast hash filter. There is some information
 * missing from the manual which one needs to know in order to make
 * the chip function:
 *
 * - You must set bit 7 in the RX control register, otherwise the
 *   chip won't receive any packets.
 * - You must initialize all 3 IPG registers, or you won't be able
 *   to send any packets.
 *
 * Note that this device appears to only support loading the station
 * address via autload from the EEPROM (i.e. there's no way to manaully
 * set it).
 *
 * (Adam Weinberger wanted me to name this driver if_gir.c.)
 */

/*
 * Ported to OpenBSD 3/28/2004 by Greg Taleck <taleck@oz.net>
 * with bits and pieces from the aue and url drivers.
 */

#if defined(__NetBSD__)
#include "opt_inet.h"
#include "opt_ns.h"
#include "rnd.h"
#endif

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#if defined(__OpenBSD__)
#include <sys/proc.h>
#endif
#include <sys/socket.h>

#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/bus.h>

#include <net/if.h>
#if defined(__NetBSD__)
#include <net/if_arp.h>
#endif
#include <net/if_dl.h>
#include <net/if_media.h>

#define BPF_MTAP(ifp, m) bpf_mtap((ifp)->if_bpf, (m))

#if NBPFILTER > 0
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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_axereg.h>

#ifdef AXE_DEBUG
#define DPRINTF(x)	do { if (axedebug) logprintf x; } while (0)
#define DPRINTFN(n,x)	do { if (axedebug >= (n)) logprintf x; } while (0)
int	axedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static const struct axe_type axe_devs[] = {
	{ { USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_UF200}, 0 },
	{ { USB_VENDOR_ACERCM, USB_PRODUCT_ACERCM_EP1427X2}, 0 },
	{ { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88172}, 0 },
	{ { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88772}, AX772 },
	{ { USB_VENDOR_ASIX, USB_PRODUCT_ASIX_AX88178}, AX178 },
	{ { USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC210T}, 0 },
	{ { USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_SNAPPORT}, 0 },
	{ { USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USB2AR}, 0},
	{ { USB_VENDOR_CISCOLINKSYS, USB_PRODUCT_CISCOLINKSYS_USB200MV2}, AX772 },
	{ { USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB2_TX }, 0},
	{ { USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DUBE100}, 0 },
	{ { USB_VENDOR_GOODWAY, USB_PRODUCT_GOODWAY_GWUSB2E}, 0 },
	{ { USB_VENDOR_JVC, USB_PRODUCT_JVC_MP_PRX1}, 0 },
	{ { USB_VENDOR_LINKSYS2, USB_PRODUCT_LINKSYS2_USB200M}, 0 },
	{ { USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUAU2KTX}, 0 },
	{ { USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_FA120}, 0 },
	{ { USB_VENDOR_SYSTEMTALKS, USB_PRODUCT_SYSTEMTALKS_SGCX2UL}, 0 },
	{ { USB_VENDOR_SITECOM, USB_PRODUCT_SITECOM_LN029}, 0 },
	{ { 0, 0}, 0 }
};

#define axe_lookup(v, p) ((struct axe_type *)usb_lookup(axe_devs, v, p))

USB_DECLARE_DRIVER_CLASS(axe, DV_IFNET);

Static int axe_tx_list_init(struct axe_softc *);
Static int axe_rx_list_init(struct axe_softc *);
Static struct mbuf *axe_newbuf(void);
Static int axe_encap(struct axe_softc *, struct mbuf *, int);
Static void axe_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void axe_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void axe_tick(void *);
Static void axe_tick_task(void *);
Static void axe_rxstart(struct ifnet *);
Static void axe_start(struct ifnet *);
Static int axe_ioctl(struct ifnet *, u_long, caddr_t);
Static void axe_init(void *);
Static void axe_stop(struct axe_softc *);
Static void axe_watchdog(struct ifnet *);
Static int axe_miibus_readreg(device_ptr_t, int, int);
Static void axe_miibus_writereg(device_ptr_t, int, int, int);
Static void axe_miibus_statchg(device_ptr_t);
Static int axe_cmd(struct axe_softc *, int, int, int, void *);
Static int axe_ifmedia_upd(struct ifnet *);
Static void axe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
Static void axe_reset(struct axe_softc *sc);

Static void axe_setmulti(struct axe_softc *);
Static void axe_lock_mii(struct axe_softc *sc);
Static void axe_unlock_mii(struct axe_softc *sc);

Static void axe_ax88178_init(struct axe_softc *);
Static void axe_ax88772_init(struct axe_softc *);

/* Get exclusive access to the MII registers */
Static void
axe_lock_mii(struct axe_softc *sc)
{
	sc->axe_refcnt++;
	usb_lockmgr(&sc->axe_mii_lock, LK_EXCLUSIVE, NULL, curproc);
}

Static void
axe_unlock_mii(struct axe_softc *sc)
{
	usb_lockmgr(&sc->axe_mii_lock, LK_RELEASE, NULL, curproc);
	if (--sc->axe_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->axe_dev));
}

Static int
axe_cmd(struct axe_softc *sc, int cmd, int index, int val, void *buf)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->axe_dying)
		return(0);

	axe_lock_mii(sc);
	if (AXE_CMD_DIR(cmd))
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, AXE_CMD_LEN(cmd));

	err = usbd_do_request(sc->axe_udev, &req, buf);
	axe_unlock_mii(sc);

	if (err)
		return(-1);

	return(0);
}

Static int
axe_miibus_readreg(device_ptr_t dev, int phy, int reg)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	usbd_status		err;
	uWord			val;

	if (sc->axe_dying) {
		DPRINTF(("axe: dying\n"));
		return(0);
	}

#ifdef notdef
	/*
	 * The chip tells us the MII address of any supported
	 * PHYs attached to the chip, so only read from those.
	 */

	DPRINTF(("axe_miibus_readreg: phy 0x%x reg 0x%x\n", phy, reg));

	if (sc->axe_phyaddrs[0] != AXE_NOPHY && phy != sc->axe_phyaddrs[0])
		return (0);

	if (sc->axe_phyaddrs[1] != AXE_NOPHY && phy != sc->axe_phyaddrs[1])
		return (0);
#endif
	if (sc->axe_phyaddrs[0] != 0xFF && sc->axe_phyaddrs[0] != phy)
		return (0);

	USETW(val, 0);

	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	axe_unlock_mii(sc);

	if (err) {
		printf("axe%d: read PHY failed\n", sc->axe_unit);
		return(-1);
	}

	if (UGETW(val))
		sc->axe_phyaddrs[0] = phy;

	return (UGETW(val));
}

Static void
axe_miibus_writereg(device_ptr_t dev, int phy, int reg, int val)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	usbd_status		err;
	uWord			uval;

	if (sc->axe_dying)
		return;

	USETW(uval, val);

	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, uval);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	axe_unlock_mii(sc);

	if (err) {
		printf("axe%d: write PHY failed\n", sc->axe_unit);
		return;
	}
}

Static void
axe_miibus_statchg(device_ptr_t dev)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);
	int			val, err;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		val = AXE_MEDIA_FULL_DUPLEX;
	else
		val = 0;
	
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		val |= (AXE_178_MEDIA_RX_EN | AXE_178_MEDIA_MAGIC);

		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_T:
			val |= AXE_178_MEDIA_GMII | AXE_178_MEDIA_ENCK;
			break;
		case IFM_100_TX:
			val |=  AXE_178_MEDIA_100TX;
			break;
		case IFM_10_T:
			/* doesn't need to be handled */
			break;
		}
	}

	DPRINTF(("axe_miibus_statchg: val=0x%x\n", val));
	err = axe_cmd(sc, AXE_CMD_WRITE_MEDIA, 0, val, NULL);
	if (err) {
		printf("%s: media change failed\n", USBDEVNAME(sc->axe_dev));
		return;
	}
}

/*
 * Set media options.
 */
Static int
axe_ifmedia_upd(struct ifnet *ifp)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        sc->axe_link = 0;
        if (mii->mii_instance) {
                struct mii_softc        *miisc;
                LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
                         mii_phy_reset(miisc);
        }
        mii_mediachg(mii);

        return (0);
}

/*
 * Report current media status.
 */
Static void
axe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        mii_pollstat(mii);
        ifmr->ifm_active = mii->mii_media_active;
        ifmr->ifm_status = mii->mii_media_status;
}

Static void
axe_setmulti(struct axe_softc *sc)
{
	struct ifnet		*ifp;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int32_t		h = 0;
	uWord			urxmode;
	u_int16_t		rxmode;
	u_int8_t		hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if (sc->axe_dying)
		return;

	ifp = GET_IFP(sc);

	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, urxmode);
	rxmode = UGETW(urxmode);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
allmulti:
		rxmode |= AXE_RXCMD_ALLMULTI;
		axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
		return;
	} else
		rxmode &= ~AXE_RXCMD_ALLMULTI;

	/* now program new ones */
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			   ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;
		hashtbl[h / 8] |= 1 << (h % 8);
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	axe_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, (void *)&hashtbl);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	return;
}

Static void
axe_reset(struct axe_softc *sc)
{
	if (sc->axe_dying)
		return;
	/* XXX What to reset? */

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	return;
}

Static void
axe_ax88178_init(struct axe_softc *sc)
{
	int gpio0 = 0, phymode = 0;
	u_int16_t eeprom;

	axe_cmd(sc, AXE_CMD_SROM_WR_ENABLE, 0, 0, NULL);
	/* XXX magic */
	axe_cmd(sc, AXE_CMD_SROM_READ, 0, 0x0017, &eeprom);
	axe_cmd(sc, AXE_CMD_SROM_WR_DISABLE, 0, 0, NULL);

	DPRINTF((" EEPROM is 0x%x\n", eeprom));

	/* if EEPROM is invalid we have to use to GPIO0 */
	if (eeprom == 0xffff) {
		phymode = 0;
		gpio0 = 1;
	} else {
		phymode = eeprom & 7;
		if (eeprom & 0x80)
			gpio0 = 0;
	}

	DPRINTF(("use gpio0: %d, phymode %d\n", gpio0, phymode));

	/* GPIO voodoo required to turn on PHY */
	if (gpio0)
		printf("gpio0 path not done! PHY not enabled\n");
	else {
		axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x008c, NULL);
		usbd_delay_ms(sc->axe_udev, 40);
		if (phymode != 1) {
			axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
			usbd_delay_ms(sc->axe_udev, 30);

			axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x001c, NULL);
			usbd_delay_ms(sc->axe_udev, 300);

			axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x003c, NULL);
			usbd_delay_ms(sc->axe_udev, 30);
		} else {
			DPRINTF(("axe gpio phymode == 1 path\n"));
			axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x0004, NULL);
			usbd_delay_ms(sc->axe_udev, 30);
			axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x000c, NULL);
			usbd_delay_ms(sc->axe_udev, 30);
		}
	}

	/* soft reset */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0,
	    AXE_178_RESET_PRL | AXE_178_RESET_MAGIC, NULL);
	usbd_delay_ms(sc->axe_udev, 150);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
	/* XXX is a delay this long required for PHY to work? */
	usbd_delay_ms(sc->axe_udev, 1500);
}

Static void
axe_ax88772_init(struct axe_softc *sc)
{
	axe_cmd(sc, AXE_CMD_WRITE_GPIO, 0, 0x00b0, NULL);
	usbd_delay_ms(sc->axe_udev, 40);

	/* ask for embedded PHY */
	axe_cmd(sc, AXE_CMD_SW_PHY_SELECT, 0, 0x01, NULL);
	usbd_delay_ms(sc->axe_udev, 10);

	/* power down and reset state, pin reset state */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0x00, NULL);
	usbd_delay_ms(sc->axe_udev, 60);

	/* power down/reset state, pin operating state */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0x48, NULL);
	usbd_delay_ms(sc->axe_udev, 150);

	/* power up, reset */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0x08, NULL);

	/* power up, operating */
	axe_cmd(sc, AXE_CMD_SW_RESET_REG, 0, 0x28, NULL);

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, 0, NULL);
}

/*
 * Probe for a AX88172 chip.
 */
USB_MATCH(axe)
{
	USB_MATCH_START(axe, uaa);

	if (!uaa->iface) {
		return(UMATCH_NONE);
	}

	return (axe_lookup(uaa->vendor, uaa->product) != NULL ? 
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(axe)
{
	USB_ATTACH_START(axe, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct mii_data	*mii;
	u_char eaddr[ETHER_ADDR_LEN];
	char *devinfop;
	char *devname = USBDEVNAME(sc->axe_dev);
	struct ifnet *ifp;
	int i, s;

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;

	sc->axe_unit = self->dv_unit; /*device_get_unit(self);*/

	err = usbd_set_config_no(dev, AXE_CONFIG_NO, 1);
	if (err) {
		printf("axe%d: getting interface handle failed\n",
		    sc->axe_unit);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->axe_flags = axe_lookup(uaa->vendor, uaa->product)->axe_flags;

	usb_init_task(&sc->axe_tick_task, axe_tick_task, sc);
	lockinit(&sc->axe_mii_lock, PZERO, "axemii", 0, LK_CANRECURSE);
	usb_init_task(&sc->axe_stop_task, (void (*)(void *))axe_stop, sc);

	err = usbd_device2interface_handle(dev, AXE_IFACE_IDX, &sc->axe_iface);
	if (err) {
		printf("axe%d: getting interface handle failed\n",
			       sc->axe_unit);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->axe_udev = dev;
	sc->axe_product = uaa->product;
	sc->axe_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(sc->axe_iface);

	printf("%s: %s", USBDEVNAME(sc->axe_dev), devinfop);
	usbd_devinfo_free(devinfop);

	/* decide on what our bufsize will be */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		sc->axe_bufsz = (sc->axe_udev->speed == USB_SPEED_HIGH) ? 
		    AXE_178_MAX_BUFSZ : AXE_178_MIN_BUFSZ; 
	else
		sc->axe_bufsz = AXE_172_BUFSZ;

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->axe_iface, i);
		if (!ed) {
			printf(" couldn't get ep %d\n", i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->axe_ed[AXE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	s = splnet();

	if (sc->axe_flags & AX178)
		axe_ax88178_init(sc);
	else if (sc->axe_flags & AX772)
		axe_ax88772_init(sc);

	/*
	 * Get station address.
	 */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		axe_cmd(sc, AXE_178_CMD_READ_NODEID, 0, 0, &eaddr);
	else
		axe_cmd(sc, AXE_172_CMD_READ_NODEID, 0, 0, &eaddr);

	/*
	 * Load IPG values and PHY indexes.
	 */
	axe_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, (void *)&sc->axe_ipgs);
	axe_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, (void *)&sc->axe_phyaddrs);

	DPRINTF((" phyaddrs[0]: %x phyaddrs[1]: %x\n",
	    sc->axe_phyaddrs[0], sc->axe_phyaddrs[1]));

	/*
	 * Work around broken adapters that appear to lie about
	 * their PHY addresses.
	 */
	sc->axe_phyaddrs[0] = sc->axe_phyaddrs[1] = 0xFF;

	/*
	 * An ASIX chip was detected. Inform the world.
	 */
	printf(", address %s\n", ether_sprintf(eaddr));

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/* Initialize interface info.*/
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, devname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = axe_ioctl;
	ifp->if_start = axe_start;

	ifp->if_watchdog = axe_watchdog;

/*	ifp->if_baudrate = 10000000; */
/*	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;*/

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->axe_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = axe_miibus_readreg;
	mii->mii_writereg = axe_miibus_writereg;
	mii->mii_statchg = axe_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;

	ifmedia_init(&mii->mii_media, 0, axe_ifmedia_upd, axe_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->axe_dev),
	    RND_TYPE_NET, 0);
#endif

	usb_callout_init(sc->axe_stat_ch);

	sc->axe_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->axe_udev,
			   USBDEV(sc->axe_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(axe)
{
	USB_DETACH_START(axe, sc);
	int			s;
	struct ifnet		*ifp = GET_IFP(sc);

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	/* Detached before attached finished, so just bail out. */
	if (!sc->axe_attached)
		return (0);

	usb_uncallout(sc->axe_stat_ch, axe_tick, sc);

	sc->axe_dying = 1;

	ether_ifdetach(ifp);

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);

	/*
	 * Remove any pending tasks.  They cannot be executing because they run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->axe_udev, &sc->axe_tick_task);
	usb_rem_task(sc->axe_udev, &sc->axe_stop_task);

	s = splusb();

	if (--sc->axe_refcnt >= 0) {
		/* Wait for processes to go away */
		usb_detach_wait(USBDEV(sc->axe_dev));
	}

	if (ifp->if_flags & IFF_RUNNING)
		axe_stop(sc);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
#endif /* __NetBSD__ */
	mii_detach(&sc->axe_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->axe_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->axe_ep[AXE_ENDPT_TX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_RX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->axe_dev));
#endif

	sc->axe_attached = 0;

	if (--sc->axe_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->axe_dev));
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->axe_udev,
			   USBDEV(sc->axe_dev));

	return (0);
}

int
axe_activate(device_ptr_t self, enum devact act)
{
	struct axe_softc *sc = (struct axe_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->axe_ec.ec_if);
		sc->axe_dying = 1;
		break;
	}
	return (0);
}

Static struct mbuf *
axe_newbuf(void)
{
	struct mbuf		*m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		return (NULL);
	}

	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, ETHER_ALIGN);

	return (m);
}

Static int
axe_rx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &cd->axe_rx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		c->axe_mbuf = NULL;
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return (ENOBUFS);
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer,
			    sc->axe_bufsz);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static int
axe_tx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		c = &cd->axe_tx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		c->axe_mbuf = NULL;
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return (ENOBUFS);
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer,
			    sc->axe_bufsz);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static void
axe_rxstart(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;

	sc = ifp->if_softc;
	axe_lock_mii(sc);
	c = &sc->axe_cdata.axe_rx_chain[sc->axe_cdata.axe_rx_prod];

	memset(c->axe_buf, 0, sc->axe_bufsz);

	/* Setup new transfer. */
	usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, c->axe_buf, sc->axe_bufsz,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(c->axe_xfer);
	axe_unlock_mii(sc);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
axe_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_chain	*c = (struct axe_chain *)priv;
	struct axe_softc	*sc = c->axe_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	u_char			*buf = c->axe_buf;
	u_int32_t		total_len;
	u_int16_t		pktlen = 0;
	struct mbuf		*m;
	struct axe_sframe_hdr	hdr;
	int			s;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev),__func__));

	if (sc->axe_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (usbd_ratecheck(&sc->axe_rx_notice)) {
			printf("%s: usb errors on rx: %s\n",
			    USBDEVNAME(sc->axe_dev), usbd_errstr(status));
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->axe_ep[AXE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	do {
		if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
			if (total_len < sizeof(hdr)) {
				ifp->if_ierrors++;
				goto done;
			}

			if ((pktlen % 2) != 0)
				pktlen++;

			buf += pktlen;

			memcpy(&hdr, buf, sizeof(hdr));
			total_len -= sizeof(hdr);

			if ((hdr.len ^ hdr.ilen) != 0xffff ||
			    (hdr.len > total_len)) {
				ifp->if_ierrors++;
				goto done;
			}

			pktlen = hdr.len;
			buf += sizeof(hdr);
			total_len -= pktlen + (pktlen % 2);
		} else {
			pktlen = total_len; /* crc on the end? */
			total_len = 0;
		}

		m = axe_newbuf();
		if (m == NULL) {
			ifp->if_ierrors++;
			goto done;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = pktlen;

		memcpy(mtod(m, char *), buf, pktlen);

		/* push the packet up */
		s = splnet();
#if NBPFILTER > 0
		if (ifp->if_bpf)
			BPF_MTAP(ifp, m);
#endif

		IF_INPUT(ifp, m);

		splx(s);

	} while (total_len > 0);

done:
	memset(c->axe_buf, 0, sc->axe_bufsz);

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, c->axe_buf, sc->axe_bufsz,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->axe_dev), __func__));

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
axe_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;
	struct ifnet		*ifp;
	int			s;

	c = priv;
	sc = c->axe_sc;
	ifp = &sc->arpcom.ac_if;

	if (sc->axe_dying)
		return;

	s = splnet();

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("axe%d: usb error on tx: %s\n", sc->axe_unit,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->axe_ep[AXE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	m_freem(c->axe_mbuf);
	c->axe_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		axe_start(ifp);

	ifp->if_opackets++;
	splx(s);
	return;
}

Static void
axe_tick(void *xsc)
{
	struct axe_softc *sc = xsc;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", USBDEVNAME(sc->axe_dev),
			__func__));

	if (sc->axe_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->axe_udev, &sc->axe_tick_task);

}

Static void
axe_tick_task(void *xsc)
{
	int			s;
	struct axe_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	sc = xsc;

	if (sc == NULL)
		return;

	if (sc->axe_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);
	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if (!sc->axe_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		DPRINTF(("%s: %s: got link\n",
			 USBDEVNAME(sc->axe_dev), __func__));
		sc->axe_link++;
		if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
			   axe_start(ifp);
	}

	usb_callout(sc->axe_stat_ch, hz, axe_tick, sc);

	splx(s);
}

Static int
axe_encap(struct axe_softc *sc, struct mbuf *m, int idx)
{
	struct axe_chain	*c;
	usbd_status		err;
	struct axe_sframe_hdr	hdr;
	int			length, boundary;

	c = &sc->axe_cdata.axe_tx_chain[idx];

	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		boundary = (sc->axe_udev->speed == USB_SPEED_HIGH) ? 512 : 64;

		hdr.len = m->m_pkthdr.len;
		hdr.ilen = ~hdr.len;

		memcpy(c->axe_buf, &hdr, sizeof(hdr));
		length = sizeof(hdr);

		m_copydata(m, 0, m->m_pkthdr.len, c->axe_buf + length);
		length += m->m_pkthdr.len;

		if ((length % boundary) == 0) {
			hdr.len = 0x0000;
			hdr.ilen = 0xffff;
			memcpy(c->axe_buf + length, &hdr, sizeof(hdr));
			length += sizeof(hdr);
		}

	} else {
		m_copydata(m, 0, m->m_pkthdr.len, c->axe_buf);
		length = m->m_pkthdr.len;
	}

	c->axe_mbuf = m;

	usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_TX],
	    c, c->axe_buf, length, USBD_FORCE_SHORT_XFER, 10000,
	    axe_txeof);

	/* Transmit */
	err = usbd_transfer(c->axe_xfer);
	if (err != USBD_IN_PROGRESS) {
		axe_stop(sc);
		return(EIO);
	}

	sc->axe_cdata.axe_tx_cnt++;

	return(0);
}

Static void
axe_start(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;

	if (!sc->axe_link) {
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		return;
	}

	if (axe_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
#if NBPFILTER > 0
	 if (ifp->if_bpf)
	 	BPF_MTAP(ifp, m_head);
#endif

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

Static void
axe_init(void *xsc)
{
	struct axe_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct axe_chain	*c;
	usbd_status		err;
	int			rxmode;
	int			i, s;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	axe_reset(sc);

	/* Enable RX logic. */

	/* Init RX ring. */
	if (axe_rx_list_init(sc) == ENOBUFS) {
		printf("axe%d: rx list init failed\n", sc->axe_unit);
		splx(s);
		return;
	}

	/* Init TX ring. */
	if (axe_tx_list_init(sc) == ENOBUFS) {
		printf("axe%d: tx list init failed\n", sc->axe_unit);
		splx(s);
		return;
	}

	/* Set transmitter IPG values */
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772)
		axe_cmd(sc, AXE_178_CMD_WRITE_IPG012, 0,
		    (sc->axe_ipgs[0]) | (sc->axe_ipgs[1] << 8) |
		    (sc->axe_ipgs[2] << 16), NULL);
	else {
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG0, 0, sc->axe_ipgs[0], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG1, 0, sc->axe_ipgs[1], NULL);
		axe_cmd(sc, AXE_172_CMD_WRITE_IPG2, 0, sc->axe_ipgs[2], NULL);
	}

	/* Enable receiver, set RX mode */
	rxmode = AXE_RXCMD_MULTICAST|AXE_RXCMD_ENABLE;
	if (sc->axe_flags & AX178 || sc->axe_flags & AX772) {
		if (sc->axe_udev->speed == USB_SPEED_HIGH) {
			/* largest possible USB buffer size for AX88178 */
			rxmode |= AXE_178_RXCMD_MFB;  
		}
	} else
		rxmode |= AXE_172_RXCMD_UNICAST;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= AXE_RXCMD_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		rxmode |= AXE_RXCMD_BROADCAST;

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	/* Load the multicast filter. */
	axe_setmulti(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_RX]);
	if (err) {
		printf("axe%d: open rx pipe failed: %s\n",
		    sc->axe_unit, usbd_errstr(err));
		splx(s);
		return;
	}

	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_TX]);
	if (err) {
		printf("axe%d: open tx pipe failed: %s\n",
		    sc->axe_unit, usbd_errstr(err));
		splx(s);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &sc->axe_cdata.axe_rx_chain[i];
		usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_RX],
		    c, c->axe_buf, sc->axe_bufsz,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, axe_rxeof);
		usbd_transfer(c->axe_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout_init(sc->axe_stat_ch);
	usb_callout(sc->axe_stat_ch, hz, axe_tick, sc);
	return;
}

Static int
axe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct axe_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct mii_data		*mii;
	uWord			rxmode;
	int			error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		axe_init(sc);

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif /* INET */
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->axe_if_flags & IFF_PROMISC)) {

				axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, rxmode);
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0,
				    UGETW(rxmode) | AXE_RXCMD_PROMISC, NULL);

				axe_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->axe_if_flags & IFF_PROMISC) {
				axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, rxmode);
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0,
				    UGETW(rxmode) & ~AXE_RXCMD_PROMISC, NULL);
				axe_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				axe_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				axe_stop(sc);
		}
		sc->axe_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				axe_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	default:
		error = EINVAL;
		break;
	}

	return(error);
}

Static void
axe_watchdog(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;
	usbd_status		stat;
	int			s;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("axe%d: watchdog timeout\n", sc->axe_unit);

	s = splusb();
	c = &sc->axe_cdata.axe_tx_chain[0];
	usbd_get_xfer_status(c->axe_xfer, NULL, NULL, NULL, &stat);
	axe_txeof(c->axe_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		axe_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
axe_stop(struct axe_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	axe_reset(sc);

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	usb_uncallout(sc->axe_stat_ch, axe_tick, sc);

	/* Stop transfers. */
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			printf("axe%d: abort rx pipe failed: %s\n",
		    	sc->axe_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			printf("axe%d: close rx pipe failed: %s\n",
		    	sc->axe_unit, usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_RX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			printf("axe%d: abort tx pipe failed: %s\n",
		    	sc->axe_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			printf("axe%d: close tx pipe failed: %s\n",
			    sc->axe_unit, usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_TX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			printf("axe%d: abort intr pipe failed: %s\n",
		    	sc->axe_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			printf("axe%d: close intr pipe failed: %s\n",
			    sc->axe_unit, usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_rx_chain[i].axe_mbuf != NULL) {
			m_freem(sc->axe_cdata.axe_rx_chain[i].axe_mbuf);
			sc->axe_cdata.axe_rx_chain[i].axe_mbuf = NULL;
		}
		if (sc->axe_cdata.axe_rx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_rx_chain[i].axe_xfer);
			sc->axe_cdata.axe_rx_chain[i].axe_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_tx_chain[i].axe_mbuf != NULL) {
			m_freem(sc->axe_cdata.axe_tx_chain[i].axe_mbuf);
			sc->axe_cdata.axe_tx_chain[i].axe_mbuf = NULL;
		}
		if (sc->axe_cdata.axe_tx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_tx_chain[i].axe_xfer);
			sc->axe_cdata.axe_tx_chain[i].axe_xfer = NULL;
		}
	}

	sc->axe_link = 0;
}

