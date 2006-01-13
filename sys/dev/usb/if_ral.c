/*	$OpenBSD: if_ral.c,v 1.56 2006/01/13 17:48:25 damien Exp $  */

/*-
 * Copyright (c) 2005, 2006
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
 * Ralink Technology RT2500USB chipset driver
 * http://www.ralinktech.com/
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_ralreg.h>
#include <dev/usb/if_ralvar.h>

#ifdef USB_DEBUG
#define URAL_DEBUG
#endif

#ifdef URAL_DEBUG
#define DPRINTF(x)	do { if (ural_debug) logprintf x; } while (0)
#define DPRINTFN(n, x)	do { if (ural_debug >= (n)) logprintf x; } while (0)
int ural_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* various supported device vendors/products */
static const struct usb_devno ural_devs[] = {
	{ USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2570 },
	{ USB_VENDOR_ASUS,		USB_PRODUCT_ASUS_RT2570_2 },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7050 },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54G },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_WUSB54GP },
	{ USB_VENDOR_CISCOLINKSYS,	USB_PRODUCT_CISCOLINKSYS_HU200TS },
	{ USB_VENDOR_CONCEPTRONIC2,	USB_PRODUCT_CONCEPTRONIC2_C54RU },
	{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_RT2570 },
	{ USB_VENDOR_GIGABYTE,		USB_PRODUCT_GIGABYTE_GNWBKG },
	{ USB_VENDOR_GUILLEMOT,		USB_PRODUCT_GUILLEMOT_HWGUSB254 },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54 },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54AI },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_KG54YB },
	{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_NINWIFI },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570 },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570_2 },
	{ USB_VENDOR_MSI,		USB_PRODUCT_MSI_RT2570_3 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570_2 },
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2570_3 },
	{ USB_VENDOR_SPHAIRON,		USB_PRODUCT_SPHAIRON_UB801R },
	{ USB_VENDOR_SURECOM,		USB_PRODUCT_SURECOM_RT2570 },
	{ USB_VENDOR_VTECH,		USB_PRODUCT_VTECH_RT2570 },
	{ USB_VENDOR_ZINWELL,		USB_PRODUCT_ZINWELL_RT2570 }
};

Static int		ural_alloc_tx_list(struct ural_softc *);
Static void		ural_free_tx_list(struct ural_softc *);
Static int		ural_alloc_rx_list(struct ural_softc *);
Static void		ural_free_rx_list(struct ural_softc *);
Static int		ural_media_change(struct ifnet *);
Static void		ural_next_scan(void *);
Static void		ural_task(void *);
Static int		ural_newstate(struct ieee80211com *,
			    enum ieee80211_state, int);
Static void		ural_txeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
Static void		ural_rxeof(usbd_xfer_handle, usbd_private_handle,
			    usbd_status);
Static int		ural_ack_rate(struct ieee80211com *, int);
Static uint16_t		ural_txtime(int, int, uint32_t);
Static uint8_t		ural_plcp_signal(int);
Static void		ural_setup_tx_desc(struct ural_softc *,
			    struct ural_tx_desc *, uint32_t, int, int);
Static int		ural_tx_bcn(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
Static int		ural_tx_mgt(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
Static int		ural_tx_data(struct ural_softc *, struct mbuf *,
			    struct ieee80211_node *);
Static void		ural_start(struct ifnet *);
Static void		ural_watchdog(struct ifnet *);
Static int		ural_ioctl(struct ifnet *, u_long, caddr_t);
Static void		ural_eeprom_read(struct ural_softc *, uint16_t, void *,
			    int);
Static uint16_t		ural_read(struct ural_softc *, uint16_t);
Static void		ural_read_multi(struct ural_softc *, uint16_t, void *,
			    int);
Static void		ural_write(struct ural_softc *, uint16_t, uint16_t);
Static void		ural_write_multi(struct ural_softc *, uint16_t, void *,
			    int);
Static void		ural_bbp_write(struct ural_softc *, uint8_t, uint8_t);
Static uint8_t		ural_bbp_read(struct ural_softc *, uint8_t);
Static void		ural_rf_write(struct ural_softc *, uint8_t, uint32_t);
Static void		ural_set_chan(struct ural_softc *,
			    struct ieee80211_channel *);
Static void		ural_disable_rf_tune(struct ural_softc *);
Static void		ural_enable_tsf_sync(struct ural_softc *);
Static void		ural_set_bssid(struct ural_softc *, uint8_t *);
Static void		ural_set_macaddr(struct ural_softc *, uint8_t *);
Static void		ural_update_promisc(struct ural_softc *);
Static const char	*ural_get_rf(int);
Static void		ural_read_eeprom(struct ural_softc *);
Static int		ural_bbp_init(struct ural_softc *);
Static void		ural_set_txantenna(struct ural_softc *, int);
Static void		ural_set_rxantenna(struct ural_softc *, int);
Static int		ural_init(struct ifnet *);
Static void		ural_stop(struct ifnet *, int);
Static void		ural_amrr_start(struct ural_softc *,
			    struct ieee80211_node *);
Static void		ural_amrr_timeout(void *);
Static void		ural_amrr_update(usbd_xfer_handle, usbd_private_handle,
			    usbd_status status);
Static void		ural_ratectl(struct ural_amrr *,
			    struct ieee80211_node *);

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset ural_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset ural_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset ural_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

/*
 * Default values for MAC registers; values taken from the reference driver.
 */
static const struct {
	uint16_t	reg;
	uint16_t	val;
} ural_def_mac[] = {
	{ RAL_TXRX_CSR5,  0x8c8d },
	{ RAL_TXRX_CSR6,  0x8b8a },
	{ RAL_TXRX_CSR7,  0x8687 },
	{ RAL_TXRX_CSR8,  0x0085 },
	{ RAL_MAC_CSR13,  0x1111 },
	{ RAL_MAC_CSR14,  0x1e11 },
	{ RAL_TXRX_CSR21, 0xe78f },
	{ RAL_MAC_CSR9,   0xff1d },
	{ RAL_MAC_CSR11,  0x0002 },
	{ RAL_MAC_CSR22,  0x0053 },
	{ RAL_MAC_CSR15,  0x0000 },
	{ RAL_MAC_CSR8,   0x0780 },
	{ RAL_TXRX_CSR19, 0x0000 },
	{ RAL_TXRX_CSR18, 0x005a },
	{ RAL_PHY_CSR2,   0x0000 },
	{ RAL_TXRX_CSR0,  0x1ec0 },
	{ RAL_PHY_CSR4,   0x000f }
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
static const struct {
	uint8_t	reg;
	uint8_t	val;
} ural_def_bbp[] = {
	{  3, 0x02 },
	{  4, 0x19 },
	{ 14, 0x1c },
	{ 15, 0x30 },
	{ 16, 0xac },
	{ 17, 0x48 },
	{ 18, 0x18 },
	{ 19, 0xff },
	{ 20, 0x1e },
	{ 21, 0x08 },
	{ 22, 0x08 },
	{ 23, 0x08 },
	{ 24, 0x80 },
	{ 25, 0x50 },
	{ 26, 0x08 },
	{ 27, 0x23 },
	{ 30, 0x10 },
	{ 31, 0x2b },
	{ 32, 0xb9 },
	{ 34, 0x12 },
	{ 35, 0x50 },
	{ 39, 0xc4 },
	{ 40, 0x02 },
	{ 41, 0x60 },
	{ 53, 0x10 },
	{ 54, 0x18 },
	{ 56, 0x08 },
	{ 57, 0x10 },
	{ 58, 0x08 },
	{ 61, 0x60 },
	{ 62, 0x10 },
	{ 75, 0xff }
};

/*
 * Default values for RF register R2 indexed by channel numbers.
 */
static const uint32_t ural_rf2522_r2[] = {
	0x307f6, 0x307fb, 0x30800, 0x30805, 0x3080a, 0x3080f, 0x30814,
	0x30819, 0x3081e, 0x30823, 0x30828, 0x3082d, 0x30832, 0x3083e
};

static const uint32_t ural_rf2523_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ural_rf2524_r2[] = {
	0x00327, 0x00328, 0x00329, 0x0032a, 0x0032b, 0x0032c, 0x0032d,
	0x0032e, 0x0032f, 0x00340, 0x00341, 0x00342, 0x00343, 0x00346
};

static const uint32_t ural_rf2525_r2[] = {
	0x20327, 0x20328, 0x20329, 0x2032a, 0x2032b, 0x2032c, 0x2032d,
	0x2032e, 0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20346
};

static const uint32_t ural_rf2525_hi_r2[] = {
	0x2032f, 0x20340, 0x20341, 0x20342, 0x20343, 0x20344, 0x20345,
	0x20346, 0x20347, 0x20348, 0x20349, 0x2034a, 0x2034b, 0x2034e
};

static const uint32_t ural_rf2525e_r2[] = {
	0x2044d, 0x2044e, 0x2044f, 0x20460, 0x20461, 0x20462, 0x20463,
	0x20464, 0x20465, 0x20466, 0x20467, 0x20468, 0x20469, 0x2046b
};

static const uint32_t ural_rf2526_hi_r2[] = {
	0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d, 0x0022d,
	0x0022e, 0x0022e, 0x0022f, 0x0022d, 0x00240, 0x00240, 0x00241
};

static const uint32_t ural_rf2526_r2[] = {
	0x00226, 0x00227, 0x00227, 0x00228, 0x00228, 0x00229, 0x00229,
	0x0022a, 0x0022a, 0x0022b, 0x0022b, 0x0022c, 0x0022c, 0x0022d
};

/*
 * For dual-band RF, RF registers R1 and R4 also depend on channel number;
 * values taken from the reference driver.
 */
static const struct {
	uint8_t		chan;
	uint32_t	r1;
	uint32_t	r2;
	uint32_t	r4;
} ural_rf5222[] = {
	{   1, 0x08808, 0x0044d, 0x00282 },
	{   2, 0x08808, 0x0044e, 0x00282 },
	{   3, 0x08808, 0x0044f, 0x00282 },
	{   4, 0x08808, 0x00460, 0x00282 },
	{   5, 0x08808, 0x00461, 0x00282 },
	{   6, 0x08808, 0x00462, 0x00282 },
	{   7, 0x08808, 0x00463, 0x00282 },
	{   8, 0x08808, 0x00464, 0x00282 },
	{   9, 0x08808, 0x00465, 0x00282 },
	{  10, 0x08808, 0x00466, 0x00282 },
	{  11, 0x08808, 0x00467, 0x00282 },
	{  12, 0x08808, 0x00468, 0x00282 },
	{  13, 0x08808, 0x00469, 0x00282 },
	{  14, 0x08808, 0x0046b, 0x00286 },

	{  36, 0x08804, 0x06225, 0x00287 },
	{  40, 0x08804, 0x06226, 0x00287 },
	{  44, 0x08804, 0x06227, 0x00287 },
	{  48, 0x08804, 0x06228, 0x00287 },
	{  52, 0x08804, 0x06229, 0x00287 },
	{  56, 0x08804, 0x0622a, 0x00287 },
	{  60, 0x08804, 0x0622b, 0x00287 },
	{  64, 0x08804, 0x0622c, 0x00287 },

	{ 100, 0x08804, 0x02200, 0x00283 },
	{ 104, 0x08804, 0x02201, 0x00283 },
	{ 108, 0x08804, 0x02202, 0x00283 },
	{ 112, 0x08804, 0x02203, 0x00283 },
	{ 116, 0x08804, 0x02204, 0x00283 },
	{ 120, 0x08804, 0x02205, 0x00283 },
	{ 124, 0x08804, 0x02206, 0x00283 },
	{ 128, 0x08804, 0x02207, 0x00283 },
	{ 132, 0x08804, 0x02208, 0x00283 },
	{ 136, 0x08804, 0x02209, 0x00283 },
	{ 140, 0x08804, 0x0220a, 0x00283 },

	{ 149, 0x08808, 0x02429, 0x00281 },
	{ 153, 0x08808, 0x0242b, 0x00281 },
	{ 157, 0x08808, 0x0242d, 0x00281 },
	{ 161, 0x08808, 0x0242f, 0x00281 }
};

USB_DECLARE_DRIVER_CLASS(ural, DV_IFNET);

USB_MATCH(ural)
{
	USB_MATCH_START(ural, uaa);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(ural_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

USB_ATTACH(ural)
{
	USB_ATTACH_START(ural, sc, uaa);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	char *devinfop;
	int i;

	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	if (usbd_set_config_no(sc->sc_udev, RAL_CONFIG_NO, 0) != 0) {
		printf("%s: could not set configuration no\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, RAL_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		printf("%s: could not get interface handle\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/*
	 * Find endpoints.
	 */
	id = usbd_get_interface_descriptor(sc->sc_iface);

	sc->sc_rx_no = sc->sc_tx_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for iface %d\n",
			    USBDEVNAME(sc->sc_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_rx_no = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_tx_no = ed->bEndpointAddress;
	}
	if (sc->sc_rx_no == -1 || sc->sc_tx_no == -1) {
		printf("%s: missing endpoint\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	usb_init_task(&sc->sc_task, ural_task, sc);
	timeout_set(&sc->scan_ch, ural_next_scan, sc);
	timeout_set(&sc->amrr_ch, ural_amrr_timeout, sc);

	/* retrieve RT2570 rev. no */
	sc->asic_rev = ural_read(sc, RAL_MAC_CSR0);

	/* retrieve MAC address and various other things from EEPROM */
	ural_read_eeprom(sc);

	printf("%s: MAC/BBP RT2570 (rev 0x%02x), RF %s, address %s\n",
	    USBDEVNAME(sc->sc_dev), sc->asic_rev, ural_get_rf(sc->rf_rev),
	    ether_sprintf(ic->ic_myaddr));

	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps = IEEE80211_C_MONITOR | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_SHPREAMBLE | IEEE80211_C_PMGT |
	    IEEE80211_C_TXPMGT | IEEE80211_C_WEP;

	if (sc->rf_rev == RAL_RF_5222) {
		/* set supported .11a rates */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = ural_rateset_11a;

		/* set supported .11a channels */
		for (i = 36; i <= 64; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 100; i <= 140; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
		for (i = 149; i <= 161; i += 4) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_A;
		}
	}

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ural_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ural_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = ural_init;
	ifp->if_ioctl = ural_ioctl;
	ifp->if_start = ural_start;
	ifp->if_watchdog = ural_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, USBDEVNAME(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ural_newstate;
	ieee80211_media_init(ifp, ural_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RAL_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RAL_TX_RADIOTAP_PRESENT);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(ural)
{
	USB_DETACH_START(ural, sc);
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splusb();

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_ch);
	timeout_del(&sc->amrr_ch);

	if (sc->amrr_xfer != NULL) {
		usbd_free_xfer(sc->amrr_xfer);
		sc->amrr_xfer = NULL;
	}

	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
	}

	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}

	ural_free_rx_list(sc);
	ural_free_tx_list(sc);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return 0;
}

Static int
ural_alloc_tx_list(struct ural_softc *sc)
{
	struct ural_tx_data *data;
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < RAL_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		data->buf = usbd_alloc_buffer(data->xfer,
		    RAL_TX_DESC_SIZE + MCLBYTES);
		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
	}

	return 0;

fail:	ural_free_tx_list(sc);
	return error;
}

Static void
ural_free_tx_list(struct ural_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ural_tx_data *data;
	int i;

	for (i = 0; i < RAL_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}

		if (data->ni != NULL) {
			ieee80211_release_node(ic, data->ni);
			data->ni = NULL;
		}
	}
}

Static int
ural_alloc_rx_list(struct ural_softc *sc)
{
	struct ural_rx_data *data;
	int i, error;

	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		if (usbd_alloc_buffer(data->xfer, MCLBYTES) == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		data->buf = mtod(data->m, uint8_t *);
	}

	return 0;

fail:	ural_free_tx_list(sc);
	return error;
}

Static void
ural_free_rx_list(struct ural_softc *sc)
{
	struct ural_rx_data *data;
	int i;

	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}

		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
	}
}

Static int
ural_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		ural_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
Static void
ural_next_scan(void *arg)
{
	struct ural_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
}

Static void
ural_task(void *arg)
{
	struct ural_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;
	struct mbuf *m;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			ural_write(sc, RAL_TXRX_CSR19, 0);

			/* force tx led to stop blinking */
			ural_write(sc, RAL_MAC_CSR20, 0);
		}
		break;

	case IEEE80211_S_SCAN:
		ural_set_chan(sc, ic->ic_bss->ni_chan);
		timeout_add(&sc->scan_ch, hz / 5);
		break;

	case IEEE80211_S_AUTH:
		ural_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_ASSOC:
		ural_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_RUN:
		ural_set_chan(sc, ic->ic_bss->ni_chan);

		/* update basic rate set */
		if (ic->ic_curmode == IEEE80211_MODE_11B) {
			/* 11b basic rates: 1, 2Mbps */
			ural_write(sc, RAL_TXRX_CSR11, 0x3);
		} else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan)) {
			/* 11a basic rates: 6, 12, 24Mbps */
			ural_write(sc, RAL_TXRX_CSR11, 0x150);
		} else {
			/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
			ural_write(sc, RAL_TXRX_CSR11, 0x15f);
		}

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			ural_set_bssid(sc, ic->ic_bss->ni_bssid);

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			m = ieee80211_beacon_alloc(ic, ic->ic_bss);
			if (m == NULL) {
				printf("%s: could not allocate beacon\n",
				    USBDEVNAME(sc->sc_dev));
				return;
			}

			if (ural_tx_bcn(sc, m, ic->ic_bss) != 0) {
				m_freem(m);
				printf("%s: could not transmit beacon\n",
				    USBDEVNAME(sc->sc_dev));
				return;
			}

			/* beacon is no longer needed */
			m_freem(m);
		}

		/* make tx led blink on tx (controlled by ASIC) */
		ural_write(sc, RAL_MAC_CSR20, 1);

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			ural_enable_tsf_sync(sc);

		/* enable automatic rate adaptation in STA mode */
		if (ic->ic_opmode == IEEE80211_M_STA &&
		    ic->ic_fixed_rate == -1)
			ural_amrr_start(sc, ic->ic_bss);

		break;
	}

	sc->sc_newstate(ic, sc->sc_state, -1);
}

Static int
ural_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ural_softc *sc = ic->ic_if.if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_ch);
	timeout_del(&sc->amrr_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	usb_add_task(sc->sc_udev, &sc->sc_task);

	return 0;
}

/* quickly determine if a given rate is CCK or OFDM */
#define RAL_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

#define RAL_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RAL_CTS_SIZE	14	/* 10 + 4(FCS) */
#define RAL_SIFS	10

Static void
ural_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ural_tx_data *data = priv;
	struct ural_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->sc_tx_pipeh);

		ifp->if_oerrors++;
		return;
	}

	s = splnet();

	m_freem(data->m);
	data->m = NULL;
	ieee80211_release_node(ic, data->ni);
	data->ni = NULL;

	sc->tx_queued--;
	ifp->if_opackets++;

	DPRINTFN(10, ("tx done\n"));

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	ural_start(ifp);

	splx(s);
}

Static void
ural_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ural_rx_data *data = priv;
	struct ural_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ural_rx_desc *desc;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *mnew, *m;
	int s, len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->sc_rx_pipeh);
		goto skip;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < RAL_RX_DESC_SIZE + IEEE80211_MIN_LEN) {
		DPRINTF(("%s: xfer too short %d\n", USBDEVNAME(sc->sc_dev),
		    len));
		ifp->if_ierrors++;
		goto skip;
	}

	/* rx descriptor is located at the end */
	desc = (struct ural_rx_desc *)(data->buf + len - RAL_RX_DESC_SIZE);

	if (letoh32(desc->flags) & (RAL_RX_PHY_ERROR | RAL_RX_CRC_ERROR)) {
		/*
		 * This should not happen since we did not request to receive
		 * those frames when we filled RAL_TXRX_CSR2.
		 */
		DPRINTFN(5, ("PHY or CRC error\n"));
		ifp->if_ierrors++;
		goto skip;
	}

	MGETHDR(mnew, M_DONTWAIT, MT_DATA);
	if (mnew == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    USBDEVNAME(sc->sc_dev));
		ifp->if_ierrors++;
		goto skip;
	}

	MCLGET(mnew, M_DONTWAIT);
	if (!(mnew->m_flags & M_EXT)) {
		printf("%s: could not allocate rx mbuf cluster\n",
		    USBDEVNAME(sc->sc_dev));
		m_freem(mnew);
		ifp->if_ierrors++;
		goto skip;
	}

	m = data->m;
	data->m = mnew;
	data->buf = mtod(data->m, uint8_t *);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = (letoh32(desc->flags) >> 16) & 0xfff;
	m->m_flags |= M_HASFCS; /* hardware appends FCS */

	s = splnet();

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct ural_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wr_antenna = sc->rx_ant;
		tap->wr_antsignal = desc->rssi;

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}
#endif

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ifp, m, ni, desc->rssi, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);

	/*
	 * In HostAP mode, ieee80211_input() will enqueue packets in if_snd
	 * without calling if_start().
	 */
	if (!IFQ_IS_EMPTY(&ifp->if_snd) && !(ifp->if_flags & IFF_OACTIVE))
		ural_start(ifp);

	splx(s);

	DPRINTFN(15, ("rx done\n"));

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data, data->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, ural_rxeof);
	usbd_transfer(xfer);
}

/*
 * Return the expected ack rate for a frame transmitted at rate `rate'.
 * XXX: this should depend on the destination node basic rate set.
 */
Static int
ural_ack_rate(struct ieee80211com *ic, int rate)
{
	switch (rate) {
	/* CCK rates */
	case 2:
		return 2;
	case 4:
	case 11:
	case 22:
		return (ic->ic_curmode == IEEE80211_MODE_11B) ? 4 : rate;

	/* OFDM rates */
	case 12:
	case 18:
		return 12;
	case 24:
	case 36:
		return 24;
	case 48:
	case 72:
	case 96:
	case 108:
		return 48;
	}

	/* default to 1Mbps */
	return 2;
}

/*
 * Compute the duration (in us) needed to transmit `len' bytes at rate `rate'.
 * The function automatically determines the operating mode depending on the
 * given rate. `flags' indicates whether short preamble is in use or not.
 */
Static uint16_t
ural_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;

	if (RAL_RATE_IS_OFDM(rate)) {
		/* IEEE Std 802.11a-1999, pp. 37 */
		txtime = (8 + 4 * len + 3 + rate - 1) / rate;
		txtime = 16 + 4 + 4 * txtime + 6;
	} else {
		/* IEEE Std 802.11b-1999, pp. 28 */
		txtime = (16 * len + rate - 1) / rate;
		if (rate != 2 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime +=  72 + 24;
		else
			txtime += 144 + 48;
	}
	return txtime;
}

Static uint8_t
ural_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 0x0;
	case 4:		return 0x1;
	case 11:	return 0x2;
	case 22:	return 0x3;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:	return 0xb;
	case 18:	return 0xf;
	case 24:	return 0xa;
	case 36:	return 0xe;
	case 48:	return 0x9;
	case 72:	return 0xd;
	case 96:	return 0x8;
	case 108:	return 0xc;

	/* unsupported rates (should not get there) */
	default:	return 0xff;
	}
}

Static void
ural_setup_tx_desc(struct ural_softc *sc, struct ural_tx_desc *desc,
    uint32_t flags, int len, int rate)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(RAL_TX_NEWSEQ);
	desc->flags |= htole32(len << 16);

	desc->wme = htole16(RAL_AIFSN(2) | RAL_LOGCWMIN(3) | RAL_LOGCWMAX(5));

	/* setup PLCP fields */
	desc->plcp_signal  = ural_plcp_signal(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (RAL_RATE_IS_OFDM(rate)) {
		desc->flags |= htole32(RAL_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RAL_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}

	desc->iv = 0;
	desc->eiv = 0;
}

#define RAL_TX_TIMEOUT	5000

Static int
ural_tx_bcn(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ural_tx_desc *desc;
	usbd_xfer_handle xfer;
	usbd_status error;
	uint8_t cmd = 0;
	uint8_t *buf;
	int xferlen, rate;

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 4;

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return ENOMEM;

	/* xfer length needs to be a multiple of two! */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	buf = usbd_alloc_buffer(xfer, xferlen);
	if (buf == NULL) {
		usbd_free_xfer(xfer);
		return ENOMEM;
	}

	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, &cmd, sizeof cmd,
	    USBD_FORCE_SHORT_XFER, RAL_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	if (error != 0) {
		usbd_free_xfer(xfer);
		return error;
	}

	desc = (struct ural_tx_desc *)buf;

	m_copydata(m0, 0, m0->m_pkthdr.len, buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, RAL_TX_IFS_NEWBACKOFF | RAL_TX_TIMESTAMP,
	    m0->m_pkthdr.len, rate);

	DPRINTFN(10, ("sending beacon frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	usbd_free_xfer(xfer);

	return error;
}

Static int
ural_tx_mgt(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	struct ieee80211_frame *wh;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	data = &sc->tx_data[0];
	desc = (struct ural_tx_desc *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 4;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;

		dur = ural_txtime(RAL_ACK_SIZE, rate, ic->ic_flags) + RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RAL_TX_TIMESTAMP;
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		M_DUP_PKTHDR(&mb, m0);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}
#endif

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending mgt frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT, ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;

	return 0;
}

Static int
ural_tx_data(struct ural_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_rateset *rs;
	struct ural_tx_desc *desc;
	struct ural_tx_data *data;
	struct ieee80211_frame *wh;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	if (ic->ic_fixed_rate != -1) {
		if (ic->ic_curmode != IEEE80211_MODE_AUTO)
			rs = &ic->ic_sup_rates[ic->ic_curmode];
		else
			rs = &ic->ic_sup_rates[IEEE80211_MODE_11G];

		rate = rs->rs_rates[ic->ic_fixed_rate];
	} else {
		rs = &ni->ni_rates;
		rate = rs->rs_rates[ni->ni_txrate];
	}
	rate &= IEEE80211_RATE_VAL;

	if (ic->ic_flags & IEEE80211_F_WEPON) {
		m0 = ieee80211_wep_crypt(ifp, m0, 1);
		if (m0 == NULL)
			return ENOBUFS;
	}

	data = &sc->tx_data[0];
	desc = (struct ural_tx_desc *)data->buf;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RAL_TX_ACK;
		flags |= RAL_TX_RETRY(7);

		dur = ural_txtime(RAL_ACK_SIZE, ural_ack_rate(ic, rate),
		    ic->ic_flags) + RAL_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct ural_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wt_antenna = sc->tx_ant;

		M_DUP_PKTHDR(&mb, m0);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}
#endif

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RAL_TX_DESC_SIZE);
	ural_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RAL_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending data frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RAL_TX_TIMEOUT, ural_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;

	return 0;
}

Static void
ural_start(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= RAL_TX_LIST_COUNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);
#endif
			if (ural_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= RAL_TX_LIST_COUNT) {
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0);
#endif
			m0 = ieee80211_encap(ifp, m0, &ni);
			if (m0 == NULL)
				continue;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0);
#endif
			if (ural_tx_data(sc, m0, ni) != 0) {
				if (ni != NULL)
					ieee80211_release_node(ic, ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

Static void
ural_watchdog(struct ifnet *ifp)
{
	struct ural_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", USBDEVNAME(sc->sc_dev));
			/*ural_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

Static int
ural_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				ural_update_promisc(sc);
			else
				ural_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ural_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);

		if (error == ENETRESET)
			error = 0;
		break;

	case SIOCS80211CHANNEL:
		/*
		 * This allows for fast channel switching in monitor mode
		 * (used by kismet). In IBSS mode, we must explicitly reset
		 * the interface to generate a new beacon frame.
		 */
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			ural_set_chan(sc, ic->ic_ibss_chan);
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			ural_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

Static void
ural_eeprom_read(struct ural_softc *sc, uint16_t addr, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not read EEPROM: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

Static uint16_t
ural_read(struct ural_softc *sc, uint16_t reg)
{
	usb_device_request_t req;
	usbd_status error;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, sizeof (uint16_t));

	error = usbd_do_request(sc->sc_udev, &req, &val);
	if (error != 0) {
		printf("%s: could not read MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		return 0;
	}

	return le16toh(val);
}

Static void
ural_read_multi(struct ural_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not read MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

Static void
ural_write(struct ural_softc *sc, uint16_t reg, uint16_t val)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MAC;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	error = usbd_do_request(sc->sc_udev, &req, NULL);
	if (error != 0) {
		printf("%s: could not write MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

Static void
ural_write_multi(struct ural_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RAL_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not write MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

Static void
ural_bbp_write(struct ural_softc *sc, uint8_t reg, uint8_t val)
{
	uint16_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR8) & RAL_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to BBP\n", USBDEVNAME(sc->sc_dev));
		return;
	}

	tmp = reg << 8 | val;
	ural_write(sc, RAL_PHY_CSR7, tmp);
}

Static uint8_t
ural_bbp_read(struct ural_softc *sc, uint8_t reg)
{
	uint16_t val;
	int ntries;

	val = RAL_BBP_WRITE | reg << 8;
	ural_write(sc, RAL_PHY_CSR7, val);

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR8) & RAL_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not read BBP\n", USBDEVNAME(sc->sc_dev));
		return 0;
	}

	return ural_read(sc, RAL_PHY_CSR7) & 0xff;
}

Static void
ural_rf_write(struct ural_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(ural_read(sc, RAL_PHY_CSR10) & RAL_RF_LOBUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to RF\n", USBDEVNAME(sc->sc_dev));
		return;
	}

	tmp = RAL_RF_BUSY | RAL_RF_20BIT | (val & 0xfffff) << 2 | (reg & 0x3);
	ural_write(sc, RAL_PHY_CSR9,  tmp & 0xffff);
	ural_write(sc, RAL_PHY_CSR10, tmp >> 16);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 0x3, val & 0xfffff));
}

Static void
ural_set_chan(struct ural_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t power, tmp;
	u_int i, chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	if (IEEE80211_IS_CHAN_2GHZ(c))
		power = min(sc->txpow[chan - 1], 31);
	else
		power = 31;

	DPRINTFN(2, ("setting channel to %u, txpower to %u\n", chan, power));

	switch (sc->rf_rev) {
	case RAL_RF_2522:
		ural_rf_write(sc, RAL_RF1, 0x00814);
		ural_rf_write(sc, RAL_RF2, ural_rf2522_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		break;

	case RAL_RF_2523:
		ural_rf_write(sc, RAL_RF1, 0x08804);
		ural_rf_write(sc, RAL_RF2, ural_rf2523_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x38044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2524:
		ural_rf_write(sc, RAL_RF1, 0x0c808);
		ural_rf_write(sc, RAL_RF2, ural_rf2524_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525:
		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525_hi_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);

		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00280 : 0x00286);
		break;

	case RAL_RF_2525E:
		ural_rf_write(sc, RAL_RF1, 0x08808);
		ural_rf_write(sc, RAL_RF2, ural_rf2525e_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan == 14) ? 0x00286 : 0x00282);
		break;

	case RAL_RF_2526:
		ural_rf_write(sc, RAL_RF2, ural_rf2526_hi_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		ural_rf_write(sc, RAL_RF1, 0x08804);

		ural_rf_write(sc, RAL_RF2, ural_rf2526_r2[chan - 1]);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x18044);
		ural_rf_write(sc, RAL_RF4, (chan & 1) ? 0x00386 : 0x00381);
		break;

	/* dual-band RF */
	case RAL_RF_5222:
		for (i = 0; ural_rf5222[i].chan != chan; i++);

		ural_rf_write(sc, RAL_RF1, ural_rf5222[i].r1);
		ural_rf_write(sc, RAL_RF2, ural_rf5222[i].r2);
		ural_rf_write(sc, RAL_RF3, power << 7 | 0x00040);
		ural_rf_write(sc, RAL_RF4, ural_rf5222[i].r4);
		break;
	}

	if (ic->ic_opmode != IEEE80211_M_MONITOR &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		/* set Japan filter bit for channel 14 */
		tmp = ural_bbp_read(sc, 70);

		tmp &= ~RAL_JAPAN_FILTER;
		if (chan == 14)
			tmp |= RAL_JAPAN_FILTER;

		ural_bbp_write(sc, 70, tmp);

		/* clear CRC errors */
		ural_read(sc, RAL_STA_CSR0);

		DELAY(1000); /* RF needs a 1ms delay here */
		ural_disable_rf_tune(sc);
	}
}

/*
 * Disable RF auto-tuning.
 */
Static void
ural_disable_rf_tune(struct ural_softc *sc)
{
	uint32_t tmp;

	if (sc->rf_rev != RAL_RF_2523) {
		tmp = sc->rf_regs[RAL_RF1] & ~RAL_RF1_AUTOTUNE;
		ural_rf_write(sc, RAL_RF1, tmp);
	}

	tmp = sc->rf_regs[RAL_RF3] & ~RAL_RF3_AUTOTUNE;
	ural_rf_write(sc, RAL_RF3, tmp);

	DPRINTFN(2, ("disabling RF autotune\n"));
}

/*
 * Refer to IEEE Std 802.11-1999 pp. 123 for more information on TSF
 * synchronization.
 */
Static void
ural_enable_tsf_sync(struct ural_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t logcwmin, preload, tmp;

	/* first, disable TSF synchronization */
	ural_write(sc, RAL_TXRX_CSR19, 0);

	tmp = (16 * ic->ic_bss->ni_intval) << 4;
	ural_write(sc, RAL_TXRX_CSR18, tmp);

	logcwmin = (ic->ic_opmode == IEEE80211_M_IBSS) ? 2 : 0;
	preload = (ic->ic_opmode == IEEE80211_M_IBSS) ? 320 : 6;
	tmp = logcwmin << 12 | preload;
	ural_write(sc, RAL_TXRX_CSR20, tmp);

	/* finally, enable TSF synchronization */
	tmp = RAL_ENABLE_TSF | RAL_ENABLE_TBCN;
	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= RAL_ENABLE_TSF_SYNC(1);
	else
		tmp |= RAL_ENABLE_TSF_SYNC(2) | RAL_ENABLE_BEACON_GENERATOR;
	ural_write(sc, RAL_TXRX_CSR19, tmp);

	DPRINTF(("enabling TSF synchronization\n"));
}

Static void
ural_set_bssid(struct ural_softc *sc, uint8_t *bssid)
{
	uint16_t tmp;

	tmp = bssid[0] | bssid[1] << 8;
	ural_write(sc, RAL_MAC_CSR5, tmp);

	tmp = bssid[2] | bssid[3] << 8;
	ural_write(sc, RAL_MAC_CSR6, tmp);

	tmp = bssid[4] | bssid[5] << 8;
	ural_write(sc, RAL_MAC_CSR7, tmp);

	DPRINTF(("setting BSSID to %s\n", ether_sprintf(bssid)));
}

Static void
ural_set_macaddr(struct ural_softc *sc, uint8_t *addr)
{
	uint16_t tmp;

	tmp = addr[0] | addr[1] << 8;
	ural_write(sc, RAL_MAC_CSR2, tmp);

	tmp = addr[2] | addr[3] << 8;
	ural_write(sc, RAL_MAC_CSR3, tmp);

	tmp = addr[4] | addr[5] << 8;
	ural_write(sc, RAL_MAC_CSR4, tmp);

	DPRINTF(("setting MAC address to %s\n", ether_sprintf(addr)));
}

Static void
ural_update_promisc(struct ural_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint16_t tmp;

	tmp = ural_read(sc, RAL_TXRX_CSR2);

	tmp &= ~RAL_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RAL_DROP_NOT_TO_ME;

	ural_write(sc, RAL_TXRX_CSR2, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

Static const char *
ural_get_rf(int rev)
{
	switch (rev) {
	case RAL_RF_2522:	return "RT2522";
	case RAL_RF_2523:	return "RT2523";
	case RAL_RF_2524:	return "RT2524";
	case RAL_RF_2525:	return "RT2525";
	case RAL_RF_2525E:	return "RT2525e";
	case RAL_RF_2526:	return "RT2526";
	case RAL_RF_5222:	return "RT5222";
	default:		return "unknown";
	}
}

Static void
ural_read_eeprom(struct ural_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;

	ural_eeprom_read(sc, RAL_EEPROM_CONFIG0, &val, 2);
	val = letoh16(val);
	sc->rf_rev =   (val >> 11) & 0x7;
	sc->hw_radio = (val >> 10) & 0x1;
	sc->led_mode = (val >> 6)  & 0x7;
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	/* read MAC address */
	ural_eeprom_read(sc, RAL_EEPROM_ADDRESS, ic->ic_myaddr, 6);

	/* read default values for BBP registers */
	ural_eeprom_read(sc, RAL_EEPROM_BBP_BASE, sc->bbp_prom, 2 * 16);

	/* read Tx power for all b/g channels */
	ural_eeprom_read(sc, RAL_EEPROM_TXPOWER, sc->txpow, 14);
}

Static int
ural_bbp_init(struct ural_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;

	/* wait for BBP to be ready */
	for (ntries = 0; ntries < 100; ntries++) {
		if (ural_bbp_read(sc, RAL_BBP_VERSION) != 0)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for BBP\n", USBDEVNAME(sc->sc_dev));
		return EIO;
	}

	/* initialize BBP registers to default values */
	for (i = 0; i < N(ural_def_bbp); i++)
		ural_bbp_write(sc, ural_def_bbp[i].reg, ural_def_bbp[i].val);

#if 0
	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0xff)
			continue;
		ural_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}
#endif

	return 0;
#undef N
}

Static void
ural_set_txantenna(struct ural_softc *sc, int antenna)
{
	uint16_t tmp;
	uint8_t tx;

	tx = ural_bbp_read(sc, RAL_BBP_TX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		tx |= RAL_BBP_ANTB;
	else
		tx |= RAL_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526 ||
	    sc->rf_rev == RAL_RF_5222)
		tx |= RAL_BBP_FLIPIQ;

	ural_bbp_write(sc, RAL_BBP_TX, tx);

	/* update flags in PHY_CSR5 and PHY_CSR6 too */
	tmp = ural_read(sc, RAL_PHY_CSR5) & ~0x7;
	ural_write(sc, RAL_PHY_CSR5, tmp | (tx & 0x7));

	tmp = ural_read(sc, RAL_PHY_CSR6) & ~0x7;
	ural_write(sc, RAL_PHY_CSR6, tmp | (tx & 0x7));
}

Static void
ural_set_rxantenna(struct ural_softc *sc, int antenna)
{
	uint8_t rx;

	rx = ural_bbp_read(sc, RAL_BBP_RX) & ~RAL_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RAL_BBP_ANTA;
	else if (antenna == 2)
		rx |= RAL_BBP_ANTB;
	else
		rx |= RAL_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */
	if (sc->rf_rev == RAL_RF_2525E || sc->rf_rev == RAL_RF_2526)
		rx &= ~RAL_BBP_FLIPIQ;

	ural_bbp_write(sc, RAL_BBP_RX, rx);
}

Static int
ural_init(struct ifnet *ifp)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_wepkey *wk;
	struct ural_rx_data *data;
	uint16_t tmp;
	usbd_status error;
	int i, ntries;

	ural_stop(ifp, 0);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(ural_def_mac); i++)
		ural_write(sc, ural_def_mac[i].reg, ural_def_mac[i].val);

	/* wait for BBP and RF to wake up (this can take a long time!) */
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = ural_read(sc, RAL_MAC_CSR17);
		if ((tmp & (RAL_BBP_AWAKE | RAL_RF_AWAKE)) ==
		    (RAL_BBP_AWAKE | RAL_RF_AWAKE))
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for BBP/RF to wakeup\n",
		    USBDEVNAME(sc->sc_dev));
		error = EIO;
		goto fail;
	}

	/* we're ready! */
	ural_write(sc, RAL_MAC_CSR1, RAL_HOST_READY);

	/* set basic rate set (will be updated later) */
	ural_write(sc, RAL_TXRX_CSR11, 0x153);

	error = ural_bbp_init(sc);
	if (error != 0)
		goto fail;

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	ural_set_chan(sc, ic->ic_bss->ni_chan);

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_read_multi(sc, RAL_STA_CSR0, sc->sta, sizeof sc->sta);

	/* set default sensitivity */
	ural_bbp_write(sc, 17, 0x48);

	ural_set_txantenna(sc, 1);
	ural_set_rxantenna(sc, 1);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	ural_set_macaddr(sc, ic->ic_myaddr);

	/*
	 * Copy WEP keys into adapter's memory (SEC_CSR0 to SEC_CSR31).
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		wk = &ic->ic_nw_keys[i];
		ural_write_multi(sc, RAL_SEC_CSR0 + i * IEEE80211_KEYBUF_SIZE,
		    wk->wk_key, IEEE80211_KEYBUF_SIZE);
	}

	/*
	 * Allocate xfer for AMRR statistics requests.
	 */
	sc->amrr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->amrr_xfer == NULL) {
		printf("%s: could not allocate AMRR xfer\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/*
	 * Open Tx and Rx USB bulk pipes.
	 */
	error = usbd_open_pipe(sc->sc_iface, sc->sc_tx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != 0) {
		printf("%s: could not open Tx pipe: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	error = usbd_open_pipe(sc->sc_iface, sc->sc_rx_no, USBD_EXCLUSIVE_USE,
	    &sc->sc_rx_pipeh);
	if (error != 0) {
		printf("%s: could not open Rx pipe: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	error = ural_alloc_tx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Tx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = ural_alloc_rx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < RAL_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->sc_rx_pipeh, data, data->buf,
		    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, ural_rxeof);
		usbd_transfer(data->xfer);
	}

	/* kick Rx */
	tmp = RAL_DROP_PHY_ERROR | RAL_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RAL_DROP_CTL | RAL_DROP_VERSION_ERROR;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RAL_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RAL_DROP_NOT_TO_ME;
	}
	ural_write(sc, RAL_TXRX_CSR2, tmp);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;

fail:	ural_stop(ifp, 1);
	return error;
#undef N
}

Static void
ural_stop(struct ifnet *ifp, int disable)
{
	struct ural_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* disable Rx */
	ural_write(sc, RAL_TXRX_CSR2, RAL_DISABLE_RX);

	/* reset ASIC and BBP (but won't reset MAC registers!) */
	ural_write(sc, RAL_MAC_CSR1, RAL_RESET_ASIC | RAL_RESET_BBP);
	ural_write(sc, RAL_MAC_CSR1, 0);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	if (sc->amrr_xfer != NULL) {
		usbd_free_xfer(sc->amrr_xfer);
		sc->amrr_xfer = NULL;
	}

	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
		sc->sc_rx_pipeh = NULL;
	}

	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
		sc->sc_tx_pipeh = NULL;
	}

	ural_free_rx_list(sc);
	ural_free_tx_list(sc);
}

#define URAL_AMRR_MIN_SUCCESS_THRESHOLD	 1
#define URAL_AMRR_MAX_SUCCESS_THRESHOLD	10

Static void
ural_amrr_start(struct ural_softc *sc, struct ieee80211_node *ni)
{
	struct ural_amrr *amrr = &sc->amrr;
	int i;

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	ural_read_multi(sc, RAL_STA_CSR0, sc->sta, sizeof sc->sta);

	amrr->success = 0;
	amrr->recovery = 0;
	amrr->txcnt = amrr->retrycnt = 0;
	amrr->success_threshold = URAL_AMRR_MIN_SUCCESS_THRESHOLD;

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);

	ni->ni_txrate = i;

	timeout_add(&sc->amrr_ch, hz);
}

Static void
ural_amrr_timeout(void *arg)
{
	struct ural_softc *sc = (struct ural_softc *)arg;
	usb_device_request_t req;
	int s;

	s = splusb();

	/*
	 * Asynchronously read statistic registers (cleared by read).
	 */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RAL_READ_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, RAL_STA_CSR0);
	USETW(req.wLength, sizeof sc->sta);

	usbd_setup_default_xfer(sc->amrr_xfer, sc->sc_udev, sc,
	    USBD_DEFAULT_TIMEOUT, &req, sc->sta, sizeof sc->sta, 0,
	    ural_amrr_update);
	(void)usbd_transfer(sc->amrr_xfer);

	splx(s);
}

Static void
ural_amrr_update(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ural_softc *sc = (struct ural_softc *)priv;
	struct ural_amrr *amrr = &sc->amrr;

	if (status != USBD_NORMAL_COMPLETION)
		return;

	amrr->retrycnt =
	    sc->sta[7] +	/* TX one-retry ok count */
	    sc->sta[8] +	/* TX more-retry ok count */
	    sc->sta[9];		/* TX retry-fail count */

	amrr->txcnt =
	    amrr->retrycnt +
	    sc->sta[6];		/* TX no-retry ok count */

	ural_ratectl(amrr, sc->sc_ic.ic_bss);

	timeout_add(&sc->amrr_ch, hz);
}

/*-
 * Naive implementation of the Adaptive Multi Rate Retry algorithm:
 *     "IEEE 802.11 Rate Adaptation: A Practical Approach"
 *     Mathieu Lacage, Hossein Manshaei, Thierry Turletti
 *     INRIA Sophia - Projet Planete
 *     http://www-sop.inria.fr/rapports/sophia/RR-5208.html
 *
 * This algorithm is particularly well suited for ural since it does not
 * require per-frame retry statistics.  Note however that since h/w does
 * not provide per-frame stats, we can't do per-node rate adaptation and
 * thus automatic rate adaptation is only enabled in STA operating mode.
 */
#define is_success(amrr)	\
	((amrr)->retrycnt < (amrr)->txcnt / 10)
#define is_failure(amrr)	\
	((amrr)->retrycnt > (amrr)->txcnt / 3)
#define is_enough(amrr)		\
	((amrr)->txcnt > 10)
#define is_min_rate(ni)		\
	((ni)->ni_txrate == 0)
#define is_max_rate(ni)		\
	((ni)->ni_txrate == (ni)->ni_rates.rs_nrates - 1)
#define increase_rate(ni)	\
	((ni)->ni_txrate++)
#define decrease_rate(ni)	\
	((ni)->ni_txrate--)
#define reset_cnt(amrr)		\
	do { (amrr)->txcnt = (amrr)->retrycnt = 0; } while (0)
Static void
ural_ratectl(struct ural_amrr *amrr, struct ieee80211_node *ni)
{
	int need_change = 0;

	if (is_success(amrr) && is_enough(amrr)) {
		amrr->success++;
		if (amrr->success >= amrr->success_threshold &&
		    !is_max_rate(ni)) {
			amrr->recovery = 1;
			amrr->success = 0;
			increase_rate(ni);
			need_change = 1;
		} else {
			amrr->recovery = 0;
		}
	} else if (is_failure(amrr)) {
		amrr->success = 0;
		if (!is_min_rate(ni)) {
			if (amrr->recovery) {
				amrr->success_threshold *= 2;
				if (amrr->success_threshold >
				    URAL_AMRR_MAX_SUCCESS_THRESHOLD)
					amrr->success_threshold =
					    URAL_AMRR_MAX_SUCCESS_THRESHOLD;
			} else {
				amrr->success_threshold =
				    URAL_AMRR_MIN_SUCCESS_THRESHOLD;
			}
			decrease_rate(ni);
			need_change = 1;
		}
		amrr->recovery = 0;	/* original paper was incorrect */
	}

	if (is_enough(amrr) || need_change)
		reset_cnt(amrr);
}

Static int
ural_activate(device_ptr_t self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		return EOPNOTSUPP;

	case DVACT_DEACTIVATE:
		/*if_deactivate(&sc->sc_ic.ic_if);*/
		break;
	}

	return 0;
}
