/*	$OpenBSD: if_ure.c,v 1.14 2020/03/10 01:11:30 kevlo Exp $	*/
/*-
 * Copyright (c) 2015, 2016, 2019 Kevin Lo <kevlo@openbsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/rwlock.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/ic/rtl81x9reg.h>
#include <dev/usb/if_urereg.h>

#ifdef URE_DEBUG
#define DPRINTF(x)	do { if (uredebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uredebug >= (n)) printf x; } while (0)
int	uredebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

const struct usb_devno ure_devs[] = {
	{ USB_VENDOR_LENOVO, USB_PRODUCT_LENOVO_ONELINKPLUS },
	{ USB_VENDOR_LENOVO, USB_PRODUCT_LENOVO_DOCK_ETHERNET },
	{ USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_RTL8152 },
	{ USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_RTL8153 },
	{ USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_RTL8156 }
};

int	ure_match(struct device *, void *, void *);
void	ure_attach(struct device *, struct device *, void *);
int	ure_detach(struct device *, int);

struct cfdriver ure_cd = {
	NULL, "ure", DV_IFNET
};

const struct cfattach ure_ca = {
	sizeof(struct ure_softc), ure_match, ure_attach, ure_detach
};

int		ure_ctl(struct ure_softc *, uint8_t, uint16_t, uint16_t,
		    void *, int);
int		ure_read_mem(struct ure_softc *, uint16_t, uint16_t, void *,
		    int);
int		ure_write_mem(struct ure_softc *, uint16_t, uint16_t, void *,
		    int);
uint8_t		ure_read_1(struct ure_softc *, uint16_t, uint16_t);
uint16_t	ure_read_2(struct ure_softc *, uint16_t, uint16_t);
uint32_t	ure_read_4(struct ure_softc *, uint16_t, uint16_t);
int		ure_write_1(struct ure_softc *, uint16_t, uint16_t, uint32_t);
int		ure_write_2(struct ure_softc *, uint16_t, uint16_t, uint32_t);
int		ure_write_4(struct ure_softc *, uint16_t, uint16_t, uint32_t);
uint16_t	ure_ocp_reg_read(struct ure_softc *, uint16_t);
void		ure_ocp_reg_write(struct ure_softc *, uint16_t, uint16_t);

void		ure_init(void *);
void		ure_stop(struct ure_softc *);
void		ure_start(struct ifnet *);
void		ure_reset(struct ure_softc *);
void		ure_watchdog(struct ifnet *);

void		ure_miibus_statchg(struct device *);
int		ure_miibus_readreg(struct device *, int, int);
void		ure_miibus_writereg(struct device *, int, int, int);
void		ure_lock_mii(struct ure_softc *);
void		ure_unlock_mii(struct ure_softc *);

int		ure_encap(struct ure_softc *, struct mbuf *);
void		ure_rxeof(struct usbd_xfer *, void *, usbd_status);
void		ure_txeof(struct usbd_xfer *, void *, usbd_status);
int		ure_rx_list_init(struct ure_softc *);
int		ure_tx_list_init(struct ure_softc *);

void		ure_tick_task(void *);
void		ure_tick(void *);

int		ure_ifmedia_upd(struct ifnet *);
void		ure_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void		ure_add_media_types(struct ure_softc *);
void		ure_link_state(struct ure_softc *);
int		ure_get_link_status(struct ure_softc *);
void		ure_iff(struct ure_softc *);
void		ure_rxvlan(struct ure_softc *);
int		ure_ioctl(struct ifnet *, u_long, caddr_t);
void		ure_rtl8152_init(struct ure_softc *);
void		ure_rtl8153_init(struct ure_softc *);
void		ure_rtl8153b_init(struct ure_softc *);
void		ure_rtl8152_nic_reset(struct ure_softc *);
void		ure_rtl8153_nic_reset(struct ure_softc *);
void		ure_rtl8153_phy_status(struct ure_softc *, int);
void		ure_reset_bmu(struct ure_softc *);
void		ure_disable_teredo(struct ure_softc *);

#define URE_SETBIT_1(sc, reg, index, x) \
	ure_write_1(sc, reg, index, ure_read_1(sc, reg, index) | (x))
#define URE_SETBIT_2(sc, reg, index, x) \
	ure_write_2(sc, reg, index, ure_read_2(sc, reg, index) | (x))
#define URE_SETBIT_4(sc, reg, index, x) \
	ure_write_4(sc, reg, index, ure_read_4(sc, reg, index) | (x))

#define URE_CLRBIT_1(sc, reg, index, x) \
	ure_write_1(sc, reg, index, ure_read_1(sc, reg, index) & ~(x))
#define URE_CLRBIT_2(sc, reg, index, x) \
	ure_write_2(sc, reg, index, ure_read_2(sc, reg, index) & ~(x))
#define URE_CLRBIT_4(sc, reg, index, x) \
	ure_write_4(sc, reg, index, ure_read_4(sc, reg, index) & ~(x))

int
ure_ctl(struct ure_softc *sc, uint8_t rw, uint16_t val, uint16_t index,
    void *buf, int len)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (usbd_is_dying(sc->ure_udev))
		return 0;

	if (rw == URE_CTL_WRITE)
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, len);

	DPRINTFN(5, ("ure_ctl: rw %d, val 0x%04hu, index 0x%04hu, len %d\n",
	    rw, val, index, len));
	err = usbd_do_request(sc->ure_udev, &req, buf);
	if (err) {
		DPRINTF(("ure_ctl: error %d\n", err));
		return -1;
	}

	return 0;
}

int
ure_read_mem(struct ure_softc *sc, uint16_t addr, uint16_t index,
    void *buf, int len)
{
	return (ure_ctl(sc, URE_CTL_READ, addr, index, buf, len));
}

int
ure_write_mem(struct ure_softc *sc, uint16_t addr, uint16_t index,
    void *buf, int len)
{
	return (ure_ctl(sc, URE_CTL_WRITE, addr, index, buf, len));
}

uint8_t
ure_read_1(struct ure_softc *sc, uint16_t reg, uint16_t index)
{
	uint32_t	val;
	uint8_t		temp[4];
	uint8_t		shift;

	shift = (reg & 3) << 3;
	reg &= ~3;
	
	ure_read_mem(sc, reg, index, &temp, 4);
	val = UGETDW(temp);
	val >>= shift;

	return (val & 0xff);
}

uint16_t
ure_read_2(struct ure_softc *sc, uint16_t reg, uint16_t index)
{
	uint32_t	val;
	uint8_t		temp[4];
	uint8_t		shift;

	shift = (reg & 2) << 3;
	reg &= ~3;

	ure_read_mem(sc, reg, index, &temp, 4);
	val = UGETDW(temp);
	val >>= shift;

	return (val & 0xffff);
}

uint32_t
ure_read_4(struct ure_softc *sc, uint16_t reg, uint16_t index)
{
	uint8_t	temp[4];

	ure_read_mem(sc, reg, index, &temp, 4);
	return (UGETDW(temp));
}

int
ure_write_1(struct ure_softc *sc, uint16_t reg, uint16_t index, uint32_t val)
{
	uint16_t	byen;
	uint8_t		temp[4];
	uint8_t		shift;

	byen = URE_BYTE_EN_BYTE;
	shift = reg & 3;
	val &= 0xff;

	if (reg & 3) {
		byen <<= shift;
		val <<= (shift << 3);
		reg &= ~3;
	}

	USETDW(temp, val);
	return (ure_write_mem(sc, reg, index | byen, &temp, 4));
}

int
ure_write_2(struct ure_softc *sc, uint16_t reg, uint16_t index, uint32_t val)
{
	uint16_t	byen;
	uint8_t		temp[4];
	uint8_t		shift;

	byen = URE_BYTE_EN_WORD;
	shift = reg & 2;
	val &= 0xffff;

	if (reg & 2) {
		byen <<= shift;
		val <<= (shift << 3);
		reg &= ~3;
	}

	USETDW(temp, val);
	return (ure_write_mem(sc, reg, index | byen, &temp, 4));
}

int
ure_write_4(struct ure_softc *sc, uint16_t reg, uint16_t index, uint32_t val)
{
	uint8_t	temp[4];

	USETDW(temp, val);
	return (ure_write_mem(sc, reg, index | URE_BYTE_EN_DWORD, &temp, 4));
}

uint16_t
ure_ocp_reg_read(struct ure_softc *sc, uint16_t addr)
{
	uint16_t	reg;

	ure_write_2(sc, URE_PLA_OCP_GPHY_BASE, URE_MCU_TYPE_PLA, addr & 0xf000);
	reg = (addr & 0x0fff) | 0xb000;

	return (ure_read_2(sc, reg, URE_MCU_TYPE_PLA));
}

void
ure_ocp_reg_write(struct ure_softc *sc, uint16_t addr, uint16_t data)
{
	uint16_t	reg;

	ure_write_2(sc, URE_PLA_OCP_GPHY_BASE, URE_MCU_TYPE_PLA, addr & 0xf000);
	reg = (addr & 0x0fff) | 0xb000;

	ure_write_2(sc, reg, URE_MCU_TYPE_PLA, data);
}

int
ure_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct ure_softc	*sc = (void *)dev;
	uint16_t		val;

	if (usbd_is_dying(sc->ure_udev))
		return 0;

	/* Let the rgephy driver read the URE_PLA_PHYSTATUS register. */
	if (reg == RL_GMEDIASTAT)
		return ure_read_1(sc, URE_PLA_PHYSTATUS, URE_MCU_TYPE_PLA);

	ure_lock_mii(sc);
	val = ure_ocp_reg_read(sc, URE_OCP_BASE_MII + reg * 2);
	ure_unlock_mii(sc);

	return val;	/* letoh16? */
}

void
ure_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct ure_softc	*sc = (void *)dev;

	ure_lock_mii(sc);
	ure_ocp_reg_write(sc, URE_OCP_BASE_MII + reg * 2, val);	/* htole16? */
	ure_unlock_mii(sc);
}

void
ure_miibus_statchg(struct device *dev)
{
	struct ure_softc	*sc = (void *)dev;
	struct mii_data		*mii = &sc->ure_mii;
	struct ifnet		*ifp = &sc->ure_ac.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	sc->ure_flags &= ~URE_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
		case IFM_100_TX:
			sc->ure_flags |= URE_FLAG_LINK;
			break;
		case IFM_1000_T:
			if ((sc->ure_flags & URE_FLAG_8152) != 0)
				break;
			sc->ure_flags |= URE_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	/* Lost link, do nothing. */
	if ((sc->ure_flags & URE_FLAG_LINK) == 0)
		return;
}

int
ure_ifmedia_upd(struct ifnet *ifp)
{
	struct ure_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = &sc->ure_mii;
	struct ifmedia		*ifm = &sc->ure_ifmedia;
	int			anar, gig, err, reg;

	if (sc->ure_flags & URE_FLAG_8156) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return (EINVAL);

		reg = ure_ocp_reg_read(sc, 0xa5d4);
		reg &= ~URE_ADV_2500TFDX;

		anar = gig = 0;
		switch (IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			anar |= ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10;
			gig |= GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX;
			reg |= URE_ADV_2500TFDX;
			break;
		case IFM_2500_T:
			anar |= ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10;
			gig |= GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX;
			reg |= URE_ADV_2500TFDX;
			ifp->if_baudrate = IF_Mbps(2500);
			break;
		case IFM_1000_T:
			anar |= ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10;
			gig |= GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX;
			ifp->if_baudrate = IF_Gbps(1);
			break;
		case IFM_100_TX:
			anar |= ANAR_TX | ANAR_TX_FD;
			ifp->if_baudrate = IF_Mbps(100);
			break;
		case IFM_10_T:
			anar |= ANAR_10 | ANAR_10_FD;
			ifp->if_baudrate = IF_Mbps(10);
			break;
		default:
			printf("%s: unsupported media type\n",
			    sc->ure_dev.dv_xname);
			return (EINVAL);
		}

		ure_ocp_reg_write(sc, URE_OCP_BASE_MII + MII_ANAR * 2,
		    anar | ANAR_PAUSE_ASYM | ANAR_FC); 
		ure_ocp_reg_write(sc, URE_OCP_BASE_MII + MII_100T2CR * 2, gig); 
		ure_ocp_reg_write(sc, 0xa5d4, reg);
		ure_ocp_reg_write(sc, URE_OCP_BASE_MII + MII_BMCR,
		    BMCR_AUTOEN | BMCR_STARTNEG);

		return (0);
	}

	if (mii->mii_instance) {
		struct mii_softc *miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			PHY_RESET(miisc);
	}

	err = mii_mediachg(mii);
	if (err == ENXIO)
		return (0);
	else
		return (err);
}

void
ure_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ure_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = &sc->ure_mii;
	uint16_t		status = 0;

	if (sc->ure_flags & URE_FLAG_8156) {
		ifmr->ifm_status = IFM_AVALID;
		if (ure_get_link_status(sc)) {
			ifmr->ifm_status |= IFM_ACTIVE;
			status = ure_read_2(sc, URE_PLA_PHYSTATUS,
			    URE_MCU_TYPE_PLA);
			if ((status & URE_PHYSTATUS_FDX) ||
			    (status & URE_PHYSTATUS_2500MBPS)) 
				ifmr->ifm_active |= IFM_FDX;
			else
				ifmr->ifm_active |= IFM_HDX;
			if (status & URE_PHYSTATUS_10MBPS)
				ifmr->ifm_active |= IFM_10_T;
			else if (status & URE_PHYSTATUS_100MBPS)
				ifmr->ifm_active |= IFM_100_TX;
			else if (status & URE_PHYSTATUS_1000MBPS)
				ifmr->ifm_active |= IFM_1000_T;
			else if (status & URE_PHYSTATUS_2500MBPS)
				ifmr->ifm_active |= IFM_2500_T;
		}
		return;
	}

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

void
ure_add_media_types(struct ure_softc *sc)
{
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_100_TX | IFM_FDX, 0,
	    NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_1000_T, 0, NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_1000_T | IFM_FDX, 0,
	    NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_2500_T, 0, NULL);
	ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_2500_T | IFM_FDX, 0,
	    NULL);
}

void
ure_link_state(struct ure_softc *sc)
{
	struct ifnet	*ifp = &sc->ure_ac.ac_if;
	int		link = LINK_STATE_DOWN;

	if (ure_get_link_status(sc))
		link = LINK_STATE_UP;

	if (ifp->if_link_state != link) {
		ifp->if_link_state = link;
		if_link_state_change(ifp);
	}
}

int
ure_get_link_status(struct ure_softc *sc)
{
	if (ure_read_2(sc, URE_PLA_PHYSTATUS, URE_MCU_TYPE_PLA) &
	    URE_PHYSTATUS_LINK) {
		sc->ure_flags |= URE_FLAG_LINK;
		return (1);
	} else {
		sc->ure_flags &= ~URE_FLAG_LINK;
		return (0);
	}
}

void
ure_iff(struct ure_softc *sc)
{
	struct ifnet		*ifp = &sc->ure_ac.ac_if;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	uint32_t		hashes[2] = { 0, 0 };
	uint32_t		hash;
	uint32_t		rxmode;

	if (usbd_is_dying(sc->ure_udev))
		return;

	rxmode = ure_read_4(sc, URE_PLA_RCR, URE_MCU_TYPE_PLA);
	rxmode &= ~URE_RCR_ACPT_ALL;
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Always accept frames destined to our station address.
	 * Always accept broadcast frames.
	 */
	rxmode |= URE_RCR_APM | URE_RCR_AB;

	if (ifp->if_flags & IFF_PROMISC || sc->ure_ac.ac_multirangecnt > 0) {
		ifp->if_flags |= IFF_ALLMULTI;
		rxmode |= URE_RCR_AM;
		if (ifp->if_flags & IFF_PROMISC)
			rxmode |= URE_RCR_AAP;
		hashes[0] = hashes[1] = 0xffffffff;
	} else {
		rxmode |= URE_RCR_AM;

		ETHER_FIRST_MULTI(step, &sc->ure_ac, enm);
		while (enm != NULL) {
			hash = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN)
			    >> 26;
			if (hash < 32)
				hashes[0] |= (1 << hash);
			else
				hashes[1] |= (1 << (hash - 32));

			ETHER_NEXT_MULTI(step, enm);
		}

		hash = swap32(hashes[0]);
		hashes[0] = swap32(hashes[1]);
		hashes[1] = hash;
	}

	ure_write_mem(sc, URE_PLA_MAR, URE_MCU_TYPE_PLA | URE_BYTE_EN_DWORD,
	    hashes, sizeof(hashes));
	ure_write_4(sc, URE_PLA_RCR, URE_MCU_TYPE_PLA, rxmode);
}

void
ure_rxvlan(struct ure_softc *sc)
{
	struct ifnet	*ifp = &sc->ure_ac.ac_if;
	uint16_t	reg;

	if (sc->ure_flags & URE_FLAG_8156) {
		reg = ure_read_2(sc, 0xc012, URE_MCU_TYPE_PLA);
		reg &= ~0x00c0;
		if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
			reg |= 0x00c0;
		ure_write_2(sc, 0xc012, URE_MCU_TYPE_PLA, reg);
	} else {
		reg = ure_read_2(sc, URE_PLA_CPCR, URE_MCU_TYPE_PLA);
		reg &= ~URE_CPCR_RX_VLAN;
		if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
			reg |= URE_CPCR_RX_VLAN;
		ure_write_2(sc, URE_PLA_CPCR, URE_MCU_TYPE_PLA, reg);
	}
}

void
ure_reset(struct ure_softc *sc)
{
	int	i;

	ure_write_1(sc, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RST);

	for (i = 0; i < URE_TIMEOUT; i++) {
		if (!(ure_read_1(sc, URE_PLA_CR, URE_MCU_TYPE_PLA) &
		    URE_CR_RST))
			break;
		DELAY(100);
	}
	if (i == URE_TIMEOUT)
		printf("%s: reset never completed\n", sc->ure_dev.dv_xname);
}

void
ure_watchdog(struct ifnet *ifp)
{
	struct ure_softc	*sc = ifp->if_softc;

	if (usbd_is_dying(sc->ure_udev))
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", sc->ure_dev.dv_xname);
}

void
ure_init(void *xsc)
{
	struct ure_softc	*sc = xsc;
	struct ure_chain	*c;
	struct ifnet		*ifp = &sc->ure_ac.ac_if;
	usbd_status		err;
	uint32_t		reg = 0;
	int			s, i;

	s = splnet();

	/* Cancel pending I/O. */
	ure_stop(sc);

	if (sc->ure_flags & URE_FLAG_8152)
		ure_rtl8152_nic_reset(sc);
	else
		ure_rtl8153_nic_reset(sc);

	if (ure_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", sc->ure_dev.dv_xname);
		splx(s);
		return;
	}

	if (ure_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", sc->ure_dev.dv_xname);
		splx(s);
		return;
	}

	/* Set MAC address. */
	ure_write_1(sc, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_CONFIG);
	ure_write_mem(sc, URE_PLA_IDR, URE_MCU_TYPE_PLA | URE_BYTE_EN_SIX_BYTES,
	    sc->ure_ac.ac_enaddr, 8);
	ure_write_1(sc, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);

	if (!(sc->ure_flags & URE_FLAG_8152)) {
		reg = sc->ure_rxbufsz - URE_FRAMELEN(ifp->if_mtu) +
		    sizeof(struct ure_rxpkt) + URE_RX_BUF_ALIGN;
		if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156)) {
			ure_write_2(sc, URE_USB_RX_EARLY_SIZE, URE_MCU_TYPE_USB,
			    reg / 8);

			ure_write_2(sc, URE_USB_RX_EARLY_AGG, URE_MCU_TYPE_USB,
			    (sc->ure_flags & URE_FLAG_8153B) ? 16 : 80);
			ure_write_2(sc, URE_USB_PM_CTRL_STATUS,
			    URE_MCU_TYPE_USB, 1875);
		} else {
			ure_write_2(sc, URE_USB_RX_EARLY_SIZE, URE_MCU_TYPE_USB,
			    reg / 4);
			switch (sc->ure_udev->speed) {
			case USB_SPEED_SUPER:
				reg = URE_COALESCE_SUPER / 8;
				break;
			case USB_SPEED_HIGH:
				reg = URE_COALESCE_HIGH / 8;
				break;
			default:
				reg = URE_COALESCE_SLOW / 8;
				break;
			}
			ure_write_2(sc, URE_USB_RX_EARLY_AGG, URE_MCU_TYPE_USB,
			    reg);
		}
	}
		
	/* Reset the packet filter. */
	URE_CLRBIT_2(sc, URE_PLA_FMC, URE_MCU_TYPE_PLA, URE_FMC_FCR_MCU_EN);
	URE_SETBIT_2(sc, URE_PLA_FMC, URE_MCU_TYPE_PLA, URE_FMC_FCR_MCU_EN);

	/* Enable transmit and receive. */
	URE_SETBIT_1(sc, URE_PLA_CR, URE_MCU_TYPE_PLA, URE_CR_RE | URE_CR_TE);

	if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156)) {
		ure_write_1(sc, URE_USB_UPT_RXDMA_OWN, URE_MCU_TYPE_USB,
		    URE_OWN_UPDATE | URE_OWN_CLEAR);
	}

	URE_CLRBIT_2(sc, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);

	/* Load the multicast filter. */
	ure_iff(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->ure_iface, sc->ure_ed[URE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->ure_ep[URE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    sc->ure_dev.dv_xname, usbd_errstr(err));
		splx(s);
		return;
	}

	err = usbd_open_pipe(sc->ure_iface, sc->ure_ed[URE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->ure_ep[URE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    sc->ure_dev.dv_xname, usbd_errstr(err));
		splx(s);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < URE_RX_LIST_CNT; i++) {
		c = &sc->ure_cdata.rx_chain[i];
		usbd_setup_xfer(c->uc_xfer, sc->ure_ep[URE_ENDPT_RX],
		    c, c->uc_buf, sc->ure_rxbufsz,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, ure_rxeof);
		usbd_transfer(c->uc_xfer);
	}

	ure_ifmedia_upd(ifp);

	/* Indicate we are up and running. */
	sc->ure_flags &= ~URE_FLAG_LINK;
	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	timeout_add_sec(&sc->ure_stat_ch, 1);

	splx(s);
}

void
ure_start(struct ifnet *ifp)
{
	struct ure_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd) ||
	    !(sc->ure_flags & URE_FLAG_LINK))
		return;

	for (;;) {
		if (sc->ure_cdata.tx_cnt == sc->ure_tx_list_cnt) {
			ifq_set_oactive(&ifp->if_snd);
			break;
		}
		
		m_head = ifq_deq_begin(&ifp->if_snd);
		if (m_head == NULL)
			break;

		if (ure_encap(sc, m_head)) {
			ifq_deq_rollback(&ifp->if_snd, m_head);
			ifq_set_oactive(&ifp->if_snd);
			break;
		}
		ifq_deq_commit(&ifp->if_snd, m_head);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
		ifp->if_timer = 5;
	}
}

void
ure_tick(void *xsc)
{
	struct ure_softc	*sc = xsc;

	if (sc == NULL)
		return;

	if (usbd_is_dying(sc->ure_udev))
		return;

	usb_add_task(sc->ure_udev, &sc->ure_tick_task);
}

void
ure_stop(struct ure_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	ure_reset(sc);

	ifp = &sc->ure_ac.ac_if;
	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	timeout_del(&sc->ure_stat_ch);
	sc->ure_flags &= ~URE_FLAG_LINK;

	if (sc->ure_ep[URE_ENDPT_RX] != NULL) {
		usbd_abort_pipe(sc->ure_ep[URE_ENDPT_RX]);
		err = usbd_close_pipe(sc->ure_ep[URE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			    sc->ure_dev.dv_xname, usbd_errstr(err));
		}
		sc->ure_ep[URE_ENDPT_RX] = NULL;
	}

	if (sc->ure_ep[URE_ENDPT_TX] != NULL) {
		usbd_abort_pipe(sc->ure_ep[URE_ENDPT_TX]);
		err = usbd_close_pipe(sc->ure_ep[URE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    sc->ure_dev.dv_xname, usbd_errstr(err));
		}
		sc->ure_ep[URE_ENDPT_TX] = NULL;
	}

	for (i = 0; i < URE_RX_LIST_CNT; i++) {
		if (sc->ure_cdata.rx_chain[i].uc_mbuf != NULL) {
			m_freem(sc->ure_cdata.rx_chain[i].uc_mbuf);
			sc->ure_cdata.rx_chain[i].uc_mbuf = NULL;
		}
		if (sc->ure_cdata.rx_chain[i].uc_xfer != NULL) {
			usbd_free_xfer(sc->ure_cdata.rx_chain[i].uc_xfer);
			sc->ure_cdata.rx_chain[i].uc_xfer = NULL;
		}
	}

	for (i = 0; i < sc->ure_tx_list_cnt; i++) {
		if (sc->ure_cdata.tx_chain[i].uc_mbuf != NULL) {
			m_freem(sc->ure_cdata.tx_chain[i].uc_mbuf);
			sc->ure_cdata.tx_chain[i].uc_mbuf = NULL;
		}
		if (sc->ure_cdata.tx_chain[i].uc_xfer != NULL) {
			usbd_free_xfer(sc->ure_cdata.tx_chain[i].uc_xfer);
			sc->ure_cdata.tx_chain[i].uc_xfer = NULL;
		}
	}
}

void
ure_rtl8152_init(struct ure_softc *sc)
{
	uint32_t	pwrctrl;

	/* Disable ALDPS. */
	ure_ocp_reg_write(sc, URE_OCP_ALDPS_CONFIG, URE_ENPDNPS | URE_LINKENA |
	    URE_DIS_SDSAVE);
	usbd_delay_ms(sc->ure_udev, 20);

	if (sc->ure_chip & URE_CHIP_VER_4C00)
		URE_CLRBIT_2(sc, URE_PLA_LED_FEATURE, URE_MCU_TYPE_PLA,
		    URE_LED_MODE_MASK);

	URE_CLRBIT_2(sc, URE_USB_UPS_CTRL, URE_MCU_TYPE_USB, URE_POWER_CUT);
	URE_CLRBIT_2(sc, URE_USB_PM_CTRL_STATUS, URE_MCU_TYPE_USB,
	    URE_RESUME_INDICATE);

	URE_SETBIT_2(sc, URE_PLA_PHY_PWR, URE_MCU_TYPE_PLA,
	    URE_TX_10M_IDLE_EN | URE_PFM_PWM_SWITCH);
	pwrctrl = ure_read_4(sc, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA);
	pwrctrl &= ~URE_MCU_CLK_RATIO_MASK;
	pwrctrl |= URE_MCU_CLK_RATIO | URE_D3_CLK_GATED_EN;
	ure_write_4(sc, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, pwrctrl);
	ure_write_2(sc, URE_PLA_GPHY_INTR_IMR, URE_MCU_TYPE_PLA,
	    URE_GPHY_STS_MSK | URE_SPEED_DOWN_MSK | URE_SPDWN_RXDV_MSK |
	    URE_SPDWN_LINKCHG_MSK);

	URE_SETBIT_2(sc, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);

	/* Enable Rx aggregation. */
	URE_CLRBIT_2(sc, URE_USB_USB_CTRL, URE_MCU_TYPE_USB,
	    URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);
}

void
ure_rtl8153_init(struct ure_softc *sc)
{
	uint16_t	reg;
	uint8_t		u1u2[8];
	int		i;

	memset(u1u2, 0x00, sizeof(u1u2));
	ure_write_mem(sc, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u1u2,
	    sizeof(u1u2));

        for (i = 0; i < 500; i++) {
		if (ure_read_2(sc, URE_PLA_BOOT_CTRL, URE_MCU_TYPE_PLA) &
		    URE_AUTOLOAD_DONE)
			break;
		usbd_delay_ms(sc->ure_udev, 20);
	}
	if (i == 500)
		printf("%s: timeout waiting for chip autoload\n",
		    sc->ure_dev.dv_xname);

	ure_rtl8153_phy_status(sc, 0);

	if (sc->ure_chip & (URE_CHIP_VER_5C00 | URE_CHIP_VER_5C10 |
	    URE_CHIP_VER_5C20)) {
		ure_ocp_reg_write(sc, URE_OCP_ADC_CFG,
		    URE_CKADSEL_L | URE_ADC_EN | URE_EN_EMI_L);
	}

	ure_rtl8153_phy_status(sc, 1);

	URE_CLRBIT_2(sc, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);

	if (sc->ure_chip & URE_CHIP_VER_5C10) {
		reg = ure_read_2(sc, URE_USB_SSPHYLINK2, URE_MCU_TYPE_USB);
		reg &= ~URE_PWD_DN_SCALE_MASK;
		reg |= URE_PWD_DN_SCALE(96);
		ure_write_2(sc, URE_USB_SSPHYLINK2, URE_MCU_TYPE_USB, reg);

		URE_SETBIT_1(sc, URE_USB_USB2PHY, URE_MCU_TYPE_USB,
		    URE_USB2PHY_L1 | URE_USB2PHY_SUSPEND);
	} else if (sc->ure_chip & URE_CHIP_VER_5C20) {
		URE_CLRBIT_1(sc, URE_PLA_DMY_REG0, URE_MCU_TYPE_PLA,
		    URE_ECM_ALDPS);
	}
	if (sc->ure_chip & (URE_CHIP_VER_5C20 | URE_CHIP_VER_5C30)) {
		if (ure_read_2(sc, URE_USB_BURST_SIZE, URE_MCU_TYPE_USB))
			URE_SETBIT_1(sc, URE_USB_CSR_DUMMY1, URE_MCU_TYPE_USB,
			    URE_DYNAMIC_BURST);
		else
			URE_CLRBIT_1(sc, URE_USB_CSR_DUMMY1, URE_MCU_TYPE_USB,
			    URE_DYNAMIC_BURST);
	}

	URE_SETBIT_1(sc, URE_USB_CSR_DUMMY2, URE_MCU_TYPE_USB, URE_EP4_FULL_FC);
	
	URE_CLRBIT_2(sc, URE_USB_WDT11_CTRL, URE_MCU_TYPE_USB, URE_TIMER11_EN);

	URE_CLRBIT_2(sc, URE_PLA_LED_FEATURE, URE_MCU_TYPE_PLA,
	    URE_LED_MODE_MASK);
	    
	if ((sc->ure_chip & URE_CHIP_VER_5C10) &&
	    sc->ure_udev->speed != USB_SPEED_SUPER)
		reg = URE_LPM_TIMER_500MS;
	else
		reg = URE_LPM_TIMER_500US;
	ure_write_1(sc, URE_USB_LPM_CTRL, URE_MCU_TYPE_USB,
	    URE_FIFO_EMPTY_1FB | URE_ROK_EXIT_LPM | reg);

	reg = ure_read_2(sc, URE_USB_AFE_CTRL2, URE_MCU_TYPE_USB);
	reg &= ~URE_SEN_VAL_MASK;
	reg |= URE_SEN_VAL_NORMAL | URE_SEL_RXIDLE;
	ure_write_2(sc, URE_USB_AFE_CTRL2, URE_MCU_TYPE_USB, reg);

	ure_write_2(sc, URE_USB_CONNECT_TIMER, URE_MCU_TYPE_USB, 0x0001);

	URE_CLRBIT_2(sc, URE_USB_POWER_CUT, URE_MCU_TYPE_USB,
	    URE_PWR_EN | URE_PHASE2_EN);
	URE_CLRBIT_2(sc, URE_USB_MISC_0, URE_MCU_TYPE_USB, URE_PCUT_STATUS);

	memset(u1u2, 0xff, sizeof(u1u2));
	ure_write_mem(sc, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES, u1u2,
	    sizeof(u1u2));

	ure_write_2(sc, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, 0);
	ure_write_2(sc, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA, 0);
	ure_write_2(sc, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, 0);
	ure_write_2(sc, URE_PLA_MAC_PWR_CTRL4, URE_MCU_TYPE_PLA, 0);

	/* Enable Rx aggregation. */
	URE_CLRBIT_2(sc, URE_USB_USB_CTRL, URE_MCU_TYPE_USB,
	    URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);

	URE_SETBIT_2(sc, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);
}

void
ure_rtl8153b_init(struct ure_softc *sc)
{
	uint16_t	reg;
	int		i;

	if (sc->ure_flags & URE_FLAG_8156) {
		URE_CLRBIT_1(sc, 0xd26b, URE_MCU_TYPE_USB, 0x01);
		ure_write_2(sc, 0xd32a, URE_MCU_TYPE_USB, 0);
		URE_SETBIT_2(sc, 0xcfee, URE_MCU_TYPE_USB, 0x0020);
	}

	URE_CLRBIT_2(sc, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);

        for (i = 0; i < 500; i++) {
		if (ure_read_2(sc, URE_PLA_BOOT_CTRL, URE_MCU_TYPE_PLA) &
		    URE_AUTOLOAD_DONE)
			break;
		usbd_delay_ms(sc->ure_udev, 20);
	}
	if (i == 500)
		printf("%s: timeout waiting for chip autoload\n",
		    sc->ure_dev.dv_xname);

	ure_rtl8153_phy_status(sc, 0);
	ure_rtl8153_phy_status(sc, 1);

	URE_CLRBIT_2(sc, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);

	/* MSC timer, 32760 ms. */
	ure_write_2(sc, URE_USB_MSC_TIMER, URE_MCU_TYPE_USB, 0x0fff);

	/* U1/U2/L1 idle timer, 500 us. */
	ure_write_2(sc, URE_USB_U1U2_TIMER, URE_MCU_TYPE_USB, 500);

	URE_CLRBIT_2(sc, URE_USB_POWER_CUT, URE_MCU_TYPE_USB, URE_PWR_EN);
	URE_CLRBIT_2(sc, URE_USB_MISC_0, URE_MCU_TYPE_USB, URE_PCUT_STATUS);

	URE_CLRBIT_1(sc, URE_USB_POWER_CUT, URE_MCU_TYPE_USB,
	    URE_UPS_EN | URE_USP_PREWAKE);
	URE_CLRBIT_1(sc, 0xcfff, URE_MCU_TYPE_USB, 0x01);

	if (!(sc->ure_flags & URE_FLAG_8156)) {
		URE_CLRBIT_2(sc, URE_USB_MISC_0, URE_MCU_TYPE_USB,
		    URE_PCUT_STATUS);
		ure_rtl8153_phy_status(sc, 0);
	}

	URE_CLRBIT_1(sc, URE_PLA_INDICATE_FALG, URE_MCU_TYPE_PLA,
	    URE_UPCOMING_RUNTIME_D3);
	URE_CLRBIT_1(sc, URE_PLA_SUSPEND_FLAG, URE_MCU_TYPE_PLA,
	    URE_LINK_CHG_EVENT);
	URE_CLRBIT_2(sc, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA,
	    URE_LINK_CHANGE_FLAG);

	ure_write_1(sc, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_CONFIG);
	URE_CLRBIT_2(sc, URE_PLA_CONFIG34, URE_MCU_TYPE_PLA,
	    URE_LINK_OFF_WAKE_EN);
	ure_write_1(sc, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);
	
	URE_SETBIT_2(sc, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB, LPM_U1U2_EN);

	/* MAC clock speed down. */
	if (sc->ure_flags & URE_FLAG_8156) {
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, 0x0403);
		reg = ure_read_2(sc, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA) &
		    ~0xff;
		reg |= URE_MAC_CLK_SPDWN_EN | 0x0003;
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA, reg);

		URE_CLRBIT_2(sc, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA,
		    0x4000);

		reg = ure_read_2(sc, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA);
		if (ure_get_link_status(sc))
			reg |= 0x8000;
		else
			reg &= ~0x8000;
		reg |= 0x0001;
		ure_write_2(sc, URE_PLA_EXTRA_STATUS, URE_MCU_TYPE_PLA, reg);
	} else
		URE_SETBIT_2(sc, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA,
		    URE_MAC_CLK_SPDWN_EN);

	/* Enable Rx aggregation. */
	URE_CLRBIT_2(sc, URE_USB_USB_CTRL, URE_MCU_TYPE_USB,
	    URE_RX_AGG_DISABLE | URE_RX_ZERO_EN);

	if (sc->ure_flags & URE_FLAG_8156)
		URE_SETBIT_1(sc, 0xcfd9, URE_MCU_TYPE_USB, 0x04);

	URE_SETBIT_2(sc, URE_PLA_RSTTALLY, URE_MCU_TYPE_PLA, URE_TALLY_RESET);
}

void
ure_rtl8152_nic_reset(struct ure_softc *sc)
{
	uint32_t	rx_fifo1, rx_fifo2;
	int		i;

	/* Disable ALDPS. */
	ure_ocp_reg_write(sc, URE_OCP_ALDPS_CONFIG, URE_ENPDNPS | URE_LINKENA |
	    URE_DIS_SDSAVE);
	usbd_delay_ms(sc->ure_udev, 20);

	URE_CLRBIT_4(sc, URE_PLA_RCR, URE_MCU_TYPE_PLA, URE_RCR_ACPT_ALL);

	URE_SETBIT_2(sc, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);
	ure_disable_teredo(sc);
	ure_write_1(sc, URE_PLA_CRWECR, URE_MCU_TYPE_PLA, URE_CRWECR_NORAML);
	ure_write_1(sc, URE_PLA_CR, URE_MCU_TYPE_PLA, 0);

	URE_CLRBIT_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA, URE_NOW_IS_OOB);
	URE_CLRBIT_2(sc, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_MCU_BORW_EN);
	for (i = 0; i < URE_TIMEOUT; i++) {
		if (ure_read_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA) &
		    URE_LINK_LIST_READY)
			break;
		usbd_delay_ms(sc->ure_udev, 1);
	}
	if (i == URE_TIMEOUT)
		printf("%s: timeout waiting for OOB control\n",
		    sc->ure_dev.dv_xname);
	URE_SETBIT_2(sc, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_RE_INIT_LL);
	for (i = 0; i < URE_TIMEOUT; i++) {
		if (ure_read_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA) &
		    URE_LINK_LIST_READY)
			break;
		usbd_delay_ms(sc->ure_udev, 1);
	}
	if (i == URE_TIMEOUT)
		printf("%s: timeout waiting for OOB control\n",
		    sc->ure_dev.dv_xname);

	ure_reset(sc);

	/* Configure Rx FIFO threshold. */
	ure_write_4(sc, URE_PLA_RXFIFO_CTRL0, URE_MCU_TYPE_PLA,
	    URE_RXFIFO_THR1_NORMAL);
	if (sc->ure_udev->speed == USB_SPEED_FULL) {
		rx_fifo1 = URE_RXFIFO_THR2_FULL;
		rx_fifo2 = URE_RXFIFO_THR3_FULL;
	} else {
		rx_fifo1 = URE_RXFIFO_THR2_HIGH;
		rx_fifo2 = URE_RXFIFO_THR3_HIGH;
	}
	ure_write_4(sc, URE_PLA_RXFIFO_CTRL1, URE_MCU_TYPE_PLA, rx_fifo1);
	ure_write_4(sc, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA, rx_fifo2);

	/* Configure Tx FIFO threshold. */
	ure_write_4(sc, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA,
	    URE_TXFIFO_THR_NORMAL);

	ure_write_1(sc, URE_USB_TX_AGG, URE_MCU_TYPE_USB,
	    URE_TX_AGG_MAX_THRESHOLD);
	ure_write_4(sc, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB, URE_RX_THR_HIGH);
	ure_write_4(sc, URE_USB_TX_DMA, URE_MCU_TYPE_USB,
	    URE_TEST_MODE_DISABLE | URE_TX_SIZE_ADJUST1);

	ure_rxvlan(sc);
        ure_write_2(sc, URE_PLA_RMS, URE_MCU_TYPE_PLA,
	    ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
	URE_SETBIT_2(sc, URE_PLA_TCR0, URE_MCU_TYPE_PLA, URE_TCR0_AUTO_FIFO);

	/* Enable ALDPS. */
	ure_ocp_reg_write(sc, URE_OCP_ALDPS_CONFIG,
	    URE_ENPWRSAVE | URE_ENPDNPS | URE_LINKENA | URE_DIS_SDSAVE);
}

void
ure_rtl8153_nic_reset(struct ure_softc *sc)
{
	struct ifnet	*ifp = &sc->ure_ac.ac_if;
	uint32_t	reg = 0;
	uint8_t		u1u2[8] = { 0 };
	int		i;

	if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156)) {
		URE_CLRBIT_2(sc, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB,
		    LPM_U1U2_EN);
	} else {
		memset(u1u2, 0x00, sizeof(u1u2));
		ure_write_mem(sc, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES,
		    u1u2, sizeof(u1u2));
	}
	URE_CLRBIT_2(sc, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB, URE_U2P3_ENABLE);

	/* Disable ALDPS. */
	ure_ocp_reg_write(sc, URE_OCP_POWER_CFG,
	    ure_ocp_reg_read(sc, URE_OCP_POWER_CFG) & ~URE_EN_ALDPS);
	for (i = 0; i < 20; i++) {
		usbd_delay_ms(sc->ure_udev, 1);
		if (ure_read_2(sc, 0xe000, URE_MCU_TYPE_PLA) & 0x0100)
			break;
	}
	if (sc->ure_flags & URE_FLAG_8153B) {
		URE_CLRBIT_4(sc, URE_USB_UPS_FLAGS, URE_MCU_TYPE_USB,
		    URE_UPS_FLAGS_EN_ALDPS);
	}

	if (!(sc->ure_flags & URE_FLAG_8156)) {
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL, URE_MCU_TYPE_PLA, 0);
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL2, URE_MCU_TYPE_PLA, 0);
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA, 0);
		ure_write_2(sc, URE_PLA_MAC_PWR_CTRL4, URE_MCU_TYPE_PLA, 0);
	}
	URE_SETBIT_2(sc, URE_PLA_MISC_1, URE_MCU_TYPE_PLA, URE_RXDY_GATED_EN);
	ure_disable_teredo(sc);

	URE_CLRBIT_4(sc, URE_PLA_RCR, URE_MCU_TYPE_PLA, URE_RCR_ACPT_ALL);

	if (sc->ure_flags & URE_FLAG_8156)
		ure_write_1(sc, URE_PLA_CR, URE_MCU_TYPE_PLA, 0);
	else
		ure_reset(sc);

	ure_reset_bmu(sc);

	URE_CLRBIT_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA, URE_NOW_IS_OOB);
	URE_CLRBIT_2(sc, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA, URE_MCU_BORW_EN);

	if (!(sc->ure_flags & URE_FLAG_8156)) {
		for (i = 0; i < URE_TIMEOUT; i++) {
			if (ure_read_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA) &
			    URE_LINK_LIST_READY)
				break;
			usbd_delay_ms(sc->ure_udev, 1);
		}
		if (i == URE_TIMEOUT)
			printf("%s: timeout waiting for OOB control\n",
			    sc->ure_dev.dv_xname);
		URE_SETBIT_2(sc, URE_PLA_SFF_STS_7, URE_MCU_TYPE_PLA,
		    URE_RE_INIT_LL);
		for (i = 0; i < URE_TIMEOUT; i++) {
			if (ure_read_1(sc, URE_PLA_OOB_CTRL, URE_MCU_TYPE_PLA) &
			    URE_LINK_LIST_READY)
				break;
			usbd_delay_ms(sc->ure_udev, 1);
		}
		if (i == URE_TIMEOUT)
			printf("%s: timeout waiting for OOB control\n",
			    sc->ure_dev.dv_xname);
	}
	ure_rxvlan(sc);
	ure_write_2(sc, URE_PLA_RMS, URE_MCU_TYPE_PLA,
	    URE_FRAMELEN(ifp->if_mtu));
	ure_write_1(sc, URE_PLA_MTPS, URE_MCU_TYPE_PLA, MTPS_JUMBO);

	if (!(sc->ure_flags & URE_FLAG_8156)) {
		URE_SETBIT_2(sc, URE_PLA_TCR0, URE_MCU_TYPE_PLA,
		    URE_TCR0_AUTO_FIFO);
		ure_reset(sc);
	}

	/* Configure Rx FIFO threshold. */
	if (sc->ure_flags & URE_FLAG_8156) {
		ure_write_2(sc, URE_PLA_RXFIFO_CTRL0 + 2, URE_MCU_TYPE_PLA,
		    0x0008);
		ure_write_2(sc, URE_PLA_RXFIFO_CTRL1 + 2, URE_MCU_TYPE_PLA,
		    0x0100);
		ure_write_2(sc, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA, 0);
	} else {
		ure_write_4(sc, URE_PLA_RXFIFO_CTRL0, URE_MCU_TYPE_PLA,
		    URE_RXFIFO_THR1_NORMAL);
		ure_write_2(sc, URE_PLA_RXFIFO_CTRL1, URE_MCU_TYPE_PLA,
		    URE_RXFIFO_THR2_NORMAL);
		ure_write_2(sc, URE_PLA_RXFIFO_CTRL2, URE_MCU_TYPE_PLA,
		    URE_RXFIFO_THR3_NORMAL);
	}

	/* Configure Tx FIFO threshold. */
	ure_write_4(sc, URE_PLA_TXFIFO_CTRL, URE_MCU_TYPE_PLA,
	    URE_TXFIFO_THR_NORMAL2);

	if (sc->ure_flags & URE_FLAG_8156) {
		URE_CLRBIT_2(sc, URE_PLA_MAC_PWR_CTRL3, URE_MCU_TYPE_PLA,
		    0x4000);
		ure_write_4(sc, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB,
		    0x00600400);
	} else if (sc->ure_flags & URE_FLAG_8153B) {
		ure_write_4(sc, URE_USB_RX_BUF_TH, URE_MCU_TYPE_USB,
		    URE_RX_THR_B);
	}

	/* Enable ALDPS. */
	ure_ocp_reg_write(sc, URE_OCP_POWER_CFG,
	    ure_ocp_reg_read(sc, URE_OCP_POWER_CFG) | URE_EN_ALDPS);
	if (sc->ure_flags & URE_FLAG_8153B) {
		reg = ure_read_4(sc, URE_USB_UPS_FLAGS, URE_MCU_TYPE_USB) &
		    URE_UPS_FLAGS_MASK;
		ure_write_4(sc, URE_USB_UPS_FLAGS, URE_MCU_TYPE_USB,
		    reg | URE_UPS_FLAGS_EN_ALDPS);
	}

	if ((sc->ure_chip & (URE_CHIP_VER_5C20 | URE_CHIP_VER_5C30)) ||
	    (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156)))
		URE_SETBIT_2(sc, URE_USB_U2P3_CTRL, URE_MCU_TYPE_USB,
		    URE_U2P3_ENABLE);

	if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156)) {
		URE_SETBIT_2(sc, URE_USB_LPM_CONFIG, URE_MCU_TYPE_USB,
		    LPM_U1U2_EN);
	} else {
		memset(u1u2, 0xff, sizeof(u1u2));
		ure_write_mem(sc, URE_USB_TOLERANCE, URE_BYTE_EN_SIX_BYTES,
		    u1u2, sizeof(u1u2));
	}
}

void
ure_rtl8153_phy_status(struct ure_softc *sc, int enable)
{
	uint16_t	reg;
	int		i;

	for (i = 0; i < 500; i++) {
		reg = ure_ocp_reg_read(sc, URE_OCP_PHY_STATUS) &
		    URE_PHY_STAT_MASK;
		if (enable) {
			if (reg == URE_PHY_STAT_LAN_ON)
				break;
		} else {
			if (reg == URE_PHY_STAT_LAN_ON ||
			    reg == URE_PHY_STAT_PWRDN ||
			    reg == URE_PHY_STAT_EXT_INIT)
				break;
		}
		usbd_delay_ms(sc->ure_udev, 20);
	}
	if (i == 500)
		printf("%s: timeout waiting for phy to stabilize\n",
		    sc->ure_dev.dv_xname);
}
	
void
ure_reset_bmu(struct ure_softc *sc)
{
	URE_CLRBIT_1(sc, URE_USB_BMU_RESET, URE_MCU_TYPE_USB,
	    BMU_RESET_EP_IN | BMU_RESET_EP_OUT);
	URE_SETBIT_1(sc, URE_USB_BMU_RESET, URE_MCU_TYPE_USB,
	    BMU_RESET_EP_IN | BMU_RESET_EP_OUT);
}

void
ure_disable_teredo(struct ure_softc *sc)
{
	if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156))
		ure_write_1(sc, URE_PLA_TEREDO_CFG, URE_MCU_TYPE_PLA, 0xff);
	else {
		URE_CLRBIT_2(sc, URE_PLA_TEREDO_CFG, URE_MCU_TYPE_PLA,
		    URE_TEREDO_SEL | URE_TEREDO_RS_EVENT_MASK |
		    URE_OOB_TEREDO_EN);
	}
	ure_write_2(sc, URE_PLA_WDT6_CTRL, URE_MCU_TYPE_PLA, URE_WDT6_SET_MODE);
	ure_write_2(sc, URE_PLA_REALWOW_TIMER, URE_MCU_TYPE_PLA, 0);
	ure_write_4(sc, URE_PLA_TEREDO_TIMER, URE_MCU_TYPE_PLA, 0);
}

int
ure_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ure_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	int			s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			ure_init(sc);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				ure_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ure_stop(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->ure_flags & URE_FLAG_8156)
			error = ifmedia_ioctl(ifp, ifr, &sc->ure_ifmedia, cmd);
		else
			error = ifmedia_ioctl(ifp, ifr, &sc->ure_mii.mii_media,
			    cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->ure_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			ure_iff(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

int
ure_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg	*uaa = aux;

	if (uaa->iface == NULL || uaa->configno != 1)
		return (UMATCH_NONE);
	
	return (usb_lookup(ure_devs, uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT_CONF_IFACE : UMATCH_NONE);
}

void
ure_attach(struct device *parent, struct device *self, void *aux)
{
	struct ure_softc		*sc = (struct ure_softc *)self;
	struct usb_attach_arg		*uaa = aux;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	u_char				eaddr[8]; /* 4byte padded */
	struct ifnet			*ifp;
	int				i, mii_flags = 0, s;
	uint16_t			ver;

	sc->ure_udev = uaa->device;
	sc->ure_iface = uaa->iface;

	usb_init_task(&sc->ure_tick_task, ure_tick_task, sc,
	    USB_TASK_TYPE_GENERIC);
	usb_init_task(&sc->ure_stop_task, (void (*)(void *))ure_stop, sc,
	    USB_TASK_TYPE_GENERIC);

	id = usbd_get_interface_descriptor(sc->ure_iface);

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->ure_iface, i);
		if (!ed) {
			printf("%s: couldn't get ep %d\n",
			    sc->ure_dev.dv_xname, i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->ure_ed[URE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->ure_ed[URE_ENDPT_TX] = ed->bEndpointAddress;
		}
	}

	switch (uaa->product) {
	case USB_PRODUCT_REALTEK_RTL8152:
		sc->ure_flags |= URE_FLAG_8152;
		sc->ure_rxbufsz = URE_8152_RXBUFSZ;
		sc->ure_tx_list_cnt = 1;
		break;
	case USB_PRODUCT_REALTEK_RTL8156:
		sc->ure_flags |= URE_FLAG_8156;
		sc->ure_rxbufsz = URE_8153_RXBUFSZ;
		sc->ure_tx_list_cnt = URE_TX_LIST_CNT;
		break;
	default:
		sc->ure_rxbufsz = URE_8153_RXBUFSZ;
		sc->ure_tx_list_cnt = 1;
	}

	s = splnet();

	sc->ure_phyno = 0;
	printf("%s: ", sc->ure_dev.dv_xname);

	ver = ure_read_2(sc, URE_PLA_TCR1, URE_MCU_TYPE_PLA) & URE_VERSION_MASK;
	switch (ver) {
	case 0x4c00:
		sc->ure_chip |= URE_CHIP_VER_4C00;
		printf("RTL8152 (0x4c00)");
		break;
	case 0x4c10:
		sc->ure_chip |= URE_CHIP_VER_4C10;
		printf("RTL8152 (0x4c10)");
		break;
	case 0x5c00:
		sc->ure_chip |= URE_CHIP_VER_5C00;
		printf("RTL8153 (0x5c00)");
		break;
	case 0x5c10:
		sc->ure_chip |= URE_CHIP_VER_5C10;
		printf("RTL8153 (0x5c10)");
		break;
	case 0x5c20:
		sc->ure_chip |= URE_CHIP_VER_5C20;
		printf("RTL8153 (0x5c20)");
		break;
	case 0x5c30:
		sc->ure_chip |= URE_CHIP_VER_5C30;
		printf("RTL8153 (0x5c30)");
		break;
	case 0x6000:
		sc->ure_flags = URE_FLAG_8153B;
		sc->ure_tx_list_cnt = URE_TX_LIST_CNT;
		printf("RTL8153B (0x6000)");
		break;
	case 0x6010:
		sc->ure_flags = URE_FLAG_8153B;
		sc->ure_tx_list_cnt = URE_TX_LIST_CNT;
		printf("RTL8153B (0x6010)");
		break;
	case 0x7020:
		printf("RTL8156 (0x7020)");
		break;
	case 0x7030:
		printf("RTL8156 (0x7030)");
		break;
	default:
		printf(", unknown ver %02x", ver);
		break;
	}

	if (sc->ure_flags & URE_FLAG_8152)
		ure_rtl8152_init(sc);
	else if (sc->ure_flags & (URE_FLAG_8153B | URE_FLAG_8156))
		ure_rtl8153b_init(sc);
	else
		ure_rtl8153_init(sc);

	if (sc->ure_chip & URE_CHIP_VER_4C00)
		ure_read_mem(sc, URE_PLA_IDR, URE_MCU_TYPE_PLA, eaddr,
		    sizeof(eaddr));
	else
		ure_read_mem(sc, URE_PLA_BACKUP, URE_MCU_TYPE_PLA, eaddr,
		    sizeof(eaddr));

	printf(", address %s\n", ether_sprintf(eaddr));

	bcopy(eaddr, (char *)&sc->ure_ac.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->ure_ac.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->ure_dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ure_ioctl;
	ifp->if_start = ure_start;
	ifp->if_watchdog = ure_watchdog;

	ifp->if_capabilities = IFCAP_VLAN_MTU | IFCAP_CSUM_IPv4 |
	    IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	if (sc->ure_flags & URE_FLAG_8156) {
		ifmedia_init(&sc->ure_ifmedia, IFM_IMASK, ure_ifmedia_upd,
		    ure_ifmedia_sts);
		ure_add_media_types(sc);
		ifmedia_add(&sc->ure_ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->ure_ifmedia, IFM_ETHER | IFM_AUTO);
		sc->ure_ifmedia.ifm_media = sc->ure_ifmedia.ifm_cur->ifm_media;
	} else {
		rw_init(&sc->ure_mii_lock, "uremii");

		sc->ure_mii.mii_ifp = ifp;
		sc->ure_mii.mii_readreg = ure_miibus_readreg;
		sc->ure_mii.mii_writereg = ure_miibus_writereg;
		sc->ure_mii.mii_statchg = ure_miibus_statchg;
		sc->ure_mii.mii_flags = MIIF_AUTOTSLEEP;

		ifmedia_init(&sc->ure_mii.mii_media, 0, ure_ifmedia_upd,
		    ure_ifmedia_sts);
		if (!(sc->ure_flags & URE_FLAG_8152))
			mii_flags |= MIIF_DOPAUSE;
		mii_attach(self, &sc->ure_mii, 0xffffffff, sc->ure_phyno,
		    MII_OFFSET_ANY, mii_flags);
		if (LIST_FIRST(&sc->ure_mii.mii_phys) == NULL) {
			ifmedia_add(&sc->ure_mii.mii_media,
			    IFM_ETHER | IFM_NONE, 0, NULL);
			ifmedia_set(&sc->ure_mii.mii_media,
			    IFM_ETHER | IFM_NONE);
		} else
			ifmedia_set(&sc->ure_mii.mii_media,
			    IFM_ETHER | IFM_AUTO);
	}

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->ure_stat_ch, ure_tick, sc);

	splx(s);
}

int
ure_detach(struct device *self, int flags)
{
	struct ure_softc	*sc = (struct ure_softc *)self;
	struct ifnet		*ifp = &sc->ure_ac.ac_if;
	int			s;

	if (timeout_initialized(&sc->ure_stat_ch))
		timeout_del(&sc->ure_stat_ch);

	if (sc->ure_ep[URE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->ure_ep[URE_ENDPT_TX]);
	if (sc->ure_ep[URE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->ure_ep[URE_ENDPT_RX]);

	usb_rem_task(sc->ure_udev, &sc->ure_tick_task);
	usb_rem_task(sc->ure_udev, &sc->ure_stop_task);

	s = splusb();

	if (--sc->ure_refcnt >= 0) {
		usb_detach_wait(&sc->ure_dev);
	}

	if (ifp->if_flags & IFF_RUNNING)
		ure_stop(sc);

	mii_detach(&sc->ure_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->ure_mii.mii_media, IFM_INST_ANY);
	if (ifp->if_softc != NULL) {
		ether_ifdetach(ifp);
		if_detach(ifp);
	}

	splx(s);

	return 0;
}

void
ure_tick_task(void *xsc)
{
	int			s;
	struct ure_softc	*sc = xsc;
	struct mii_data		*mii;

	if (sc == NULL)
		return;

	if (usbd_is_dying(sc->ure_udev))
		return;
	mii = &sc->ure_mii;

	s = splnet();
	if (sc->ure_flags & URE_FLAG_8156)
		ure_link_state(sc);
	else {
		mii_tick(mii);
		if ((sc->ure_flags & URE_FLAG_LINK) == 0)
			ure_miibus_statchg(&sc->ure_dev);
	}
	timeout_add_sec(&sc->ure_stat_ch, 1);
	splx(s);
}

void
ure_lock_mii(struct ure_softc *sc)
{
	sc->ure_refcnt++;
	rw_enter_write(&sc->ure_mii_lock);
}

void
ure_unlock_mii(struct ure_softc *sc)
{
	rw_exit_write(&sc->ure_mii_lock);
	if (--sc->ure_refcnt < 0)
		usb_detach_wakeup(&sc->ure_dev);
}

void
ure_rxeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct ure_chain	*c = (struct ure_chain *)priv;
	struct ure_softc	*sc = c->uc_sc;
	struct ifnet		*ifp = &sc->ure_ac.ac_if;
	u_char			*buf = c->uc_buf;
	uint32_t		cflags, rxvlan, total_len;
	struct mbuf_list	ml = MBUF_LIST_INITIALIZER();
	struct mbuf		*m;
	int			pktlen = 0, s;
	struct ure_rxpkt	rxhdr;
	
	if (usbd_is_dying(sc->ure_udev))
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (usbd_ratecheck(&sc->ure_rx_notice)) {
			printf("%s: usb errors on rx: %s\n",
				sc->ure_dev.dv_xname, usbd_errstr(status));
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(
			    sc->ure_ep[URE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);
	DPRINTFN(3, ("received %d bytes\n", total_len));

	do {
		if (total_len < sizeof(rxhdr)) {
			DPRINTF(("too few bytes left for a packet header\n"));
			ifp->if_ierrors++;
			goto done;
		}

		buf += roundup(pktlen, 8);

		memcpy(&rxhdr, buf, sizeof(rxhdr));
		total_len -= sizeof(rxhdr);

		pktlen = letoh32(rxhdr.ure_pktlen) & URE_RXPKT_LEN_MASK;
		DPRINTFN(4, ("next packet is %d bytes\n", pktlen));
		if (pktlen > total_len) {
			DPRINTF(("not enough bytes left for next packet\n"));
			ifp->if_ierrors++;
			goto done;
		}

		total_len -= roundup(pktlen, 8);
		buf += sizeof(rxhdr);

		m = m_devget(buf, pktlen, ETHER_ALIGN);
		if (m == NULL) {
			DPRINTF(("unable to allocate mbuf for next packet\n"));
			ifp->if_ierrors++;
			goto done;
		}

		cflags = letoh32(rxhdr.ure_csum);
		rxvlan = letoh32(rxhdr.ure_vlan);

		/* Check IP header checksum. */
		if ((rxvlan & URE_RXPKT_IPV4) &&
		    !(cflags & URE_RXPKT_IPSUMBAD))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;

		/* Check TCP/UDP checksum. */
		if ((rxvlan & (URE_RXPKT_IPV4 | URE_RXPKT_IPV6)) &&
		    (((rxvlan & URE_RXPKT_TCP) &&
		    !(cflags & URE_RXPKT_TCPSUMBAD)) ||
		    ((rxvlan & URE_RXPKT_UDP) &&
		    !(cflags & URE_RXPKT_UDPSUMBAD))))
			 m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK |
			     M_UDP_CSUM_IN_OK;
#if NVLAN > 0
		if (rxvlan & URE_RXPKT_VLAN_TAG) {
			m->m_pkthdr.ether_vtag =
			    swap16(rxvlan & URE_RXPKT_VLAN_DATA);
			 m->m_flags |= M_VLANTAG;
		}
#endif

		ml_enqueue(&ml, m);
	} while (total_len > 0);

done:
	s = splnet();
	if_input(ifp, &ml);
	splx(s);
	memset(c->uc_buf, 0, sc->ure_rxbufsz);

	usbd_setup_xfer(xfer, sc->ure_ep[URE_ENDPT_RX], c, c->uc_buf,
	    sc->ure_rxbufsz, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, ure_rxeof);
	usbd_transfer(xfer);
}


void
ure_txeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct ure_softc	*sc;
	struct ure_chain	*c;
	struct ifnet		*ifp;
	int			s;

	c = priv;
	sc = c->uc_sc;
	ifp = &sc->ure_ac.ac_if;

	if (usbd_is_dying(sc->ure_udev))
		return;

	DPRINTFN(2, ("tx completion\n"));

	s = splnet();
	sc->ure_cdata.tx_cnt--;
	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", sc->ure_dev.dv_xname,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(
			    sc->ure_ep[URE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_timer = 0;
	ifq_clr_oactive(&ifp->if_snd);

	m_freem(c->uc_mbuf);
	c->uc_mbuf = NULL;

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		ure_start(ifp);

	splx(s);
}

int
ure_tx_list_init(struct ure_softc *sc)
{
	struct ure_cdata *cd;
	struct ure_chain *c;
	int i;

	cd = &sc->ure_cdata;
	for (i = 0; i < sc->ure_tx_list_cnt; i++) {
		c = &cd->tx_chain[i];
		c->uc_sc = sc;
		c->uc_idx = i;
		c->uc_mbuf = NULL;
		if (c->uc_xfer == NULL) {
			c->uc_xfer = usbd_alloc_xfer(sc->ure_udev);
			if (c->uc_xfer == NULL)
				return ENOBUFS;
			c->uc_buf = usbd_alloc_buffer(c->uc_xfer, URE_TXBUFSZ);
			if (c->uc_buf == NULL) {
				usbd_free_xfer(c->uc_xfer);
				return ENOBUFS;
			}
		}
	}

	cd->tx_prod = cd->tx_cnt = 0;

	return (0);
}

int
ure_rx_list_init(struct ure_softc *sc)
{
	struct ure_cdata *cd;
	struct ure_chain *c;
	int i;

	cd = &sc->ure_cdata;
	for (i = 0; i < URE_RX_LIST_CNT; i++) {
		c = &cd->rx_chain[i];
		c->uc_sc = sc;
		c->uc_idx = i;
		c->uc_mbuf = NULL;
		if (c->uc_xfer == NULL) {
			c->uc_xfer = usbd_alloc_xfer(sc->ure_udev);
			if (c->uc_xfer == NULL)
				return ENOBUFS;
			c->uc_buf = usbd_alloc_buffer(c->uc_xfer,
			    sc->ure_rxbufsz);
			if (c->uc_buf == NULL) {
				usbd_free_xfer(c->uc_xfer);
				return ENOBUFS;
			}
		}
	}

	return (0);
}

int
ure_encap(struct ure_softc *sc, struct mbuf *m)
{
	struct ure_chain	*c;
	usbd_status		err;
	struct ure_txpkt	txhdr;
	uint32_t		frm_len = 0, cflags = 0;

	if ((m->m_pkthdr.csum_flags &
	    (M_IPV4_CSUM_OUT | M_TCP_CSUM_OUT | M_UDP_CSUM_OUT)) != 0) {
		cflags |= URE_TXPKT_IPV4;
		if (m->m_pkthdr.csum_flags & M_TCP_CSUM_OUT)
			cflags |= URE_TXPKT_TCP;
		if (m->m_pkthdr.csum_flags & M_UDP_CSUM_OUT)
			cflags |= URE_TXPKT_UDP;
	}

#if NVLAN > 0
	if (m->m_flags & M_VLANTAG)
		cflags |= swap16(m->m_pkthdr.ether_vtag | URE_TXPKT_VLAN_TAG);
#endif

	c = &sc->ure_cdata.tx_chain[sc->ure_cdata.tx_prod];

	/* header */
	txhdr.ure_pktlen = htole32(m->m_pkthdr.len | URE_TXPKT_TX_FS |
	    URE_TXPKT_TX_LS);
	txhdr.ure_vlan = htole32(cflags);
	memcpy(c->uc_buf, &txhdr, sizeof(txhdr));
	frm_len = sizeof(txhdr);

	/* packet */
	m_copydata(m, 0, m->m_pkthdr.len, c->uc_buf + frm_len);
	frm_len += m->m_pkthdr.len;

	c->uc_mbuf = m;

	DPRINTFN(2, ("tx %d bytes\n", frm_len));
	usbd_setup_xfer(c->uc_xfer, sc->ure_ep[URE_ENDPT_TX], c, c->uc_buf,
	    frm_len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY, 10000, ure_txeof);

	err = usbd_transfer(c->uc_xfer);
	if (err != 0 && err != USBD_IN_PROGRESS) {
		ure_stop(sc);
		return (EIO);
	}

	sc->ure_cdata.tx_cnt++;
	sc->ure_cdata.tx_prod = (sc->ure_cdata.tx_prod + 1) %
	    sc->ure_tx_list_cnt;

	return (0);
}
