/*	$OpenBSD: if_rum.c,v 1.1 2006/06/16 22:30:46 niallo Exp $  */
/*-
 * Copyright (c) 2005, 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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

/*
 * Ralink Technology RT2501USB chipset driver
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

#include <dev/usb/if_rumreg.h>
#include <dev/usb/if_rumvar.h>

#ifdef USB_DEBUG
#define RUM_DEBUG
#endif

#ifdef RUM_DEBUG
#define DPRINTF(x)	do { if (rum_debug) logprintf x; } while (0)
#define DPRINTFN(n, x)	do { if (rum_debug >= (n)) logprintf x; } while (0)
int rum_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* various supported device vendors/products */
static const struct usb_devno rum_devs[] = {
	{ USB_VENDOR_RALINK,		USB_PRODUCT_RALINK_RT2573 },
	{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_F5D7050A }
};

int		rum_alloc_tx_list(struct rum_softc *);
void		rum_free_tx_list(struct rum_softc *);
int		rum_alloc_rx_list(struct rum_softc *);
void		rum_free_rx_list(struct rum_softc *);
int		rum_media_change(struct ifnet *);
void		rum_next_scan(void *);
void		rum_task(void *);
int		rum_newstate(struct ieee80211com *,
		    enum ieee80211_state, int);
void		rum_txeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
void		rum_rxeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
#if NBPFILTER > 0
uint8_t		rum_rxrate(struct rum_rx_desc *);
#endif
int		rum_ack_rate(struct ieee80211com *, int);
uint16_t	rum_txtime(int, int, uint32_t);
uint8_t		rum_plcp_signal(int);
void		rum_setup_tx_desc(struct rum_softc *,
		    struct rum_tx_desc *, uint32_t, int, int);
int		rum_tx_bcn(struct rum_softc *, struct mbuf *,
		    struct ieee80211_node *);
int		rum_tx_mgt(struct rum_softc *, struct mbuf *,
		    struct ieee80211_node *);
int		rum_tx_data(struct rum_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		rum_start(struct ifnet *);
void		rum_watchdog(struct ifnet *);
int		rum_ioctl(struct ifnet *, u_long, caddr_t);
void		rum_eeprom_read(struct rum_softc *, uint16_t, void *,
		    int);
uint32_t	rum_read(struct rum_softc *, uint32_t);
void		rum_read_multi(struct rum_softc *, uint16_t, void *,
		    int);
void		rum_write(struct rum_softc *, uint16_t, uint32_t);
void		rum_write_multi(struct rum_softc *, uint16_t,
		    void *, size_t);
void		rum_bbp_write(struct rum_softc *, uint8_t, uint8_t);
uint8_t		rum_bbp_read(struct rum_softc *, uint8_t);
void		rum_rf_write(struct rum_softc *, uint8_t, uint32_t);
void		rum_set_chan(struct rum_softc *,
		    struct ieee80211_channel *);
void		rum_enable_tsf_sync(struct rum_softc *);
void		rum_update_slot(struct rum_softc *);
void		rum_set_txpreamble(struct rum_softc *);
void		rum_set_basicrates(struct rum_softc *);
void		rum_set_bssid(struct rum_softc *, uint8_t *);
void		rum_set_macaddr(struct rum_softc *, uint8_t *);
void		rum_update_promisc(struct rum_softc *);
const char	*rum_get_rf(int);
void		rum_read_eeprom(struct rum_softc *);
int		rum_bbp_init(struct rum_softc *);
void		rum_set_txantenna(struct rum_softc *, int);
void		rum_set_rxantenna(struct rum_softc *, int);
int		rum_init(struct ifnet *);
void		rum_stop(struct ifnet *, int);
int		rum_load_microcode(struct rum_softc *,
		    const u_char *, size_t);
int		rum_firmware_run(struct rum_softc *sc);
int		rum_led_write(struct rum_softc *, uint16_t, uint8_t);

void		rum_attachhook(void *);
int		rum_prepare_beacon(struct rum_softc *);
void		rum_select_antenna(struct rum_softc *);

/*
 * Supported rates for 802.11a/b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset rum_rateset_11a =
	{ 8, { 12, 18, 24, 36, 48, 72, 96, 108 } };

static const struct ieee80211_rateset rum_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static const struct ieee80211_rateset rum_rateset_11g =
	{ 12, { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 } };

/*
 * Default values for MAC registers; values taken from the reference driver.
 */
static const struct {
	uint32_t	reg;
	uint32_t	val;
} rum_def_mac[] = {
        { RT2573_TXRX_CSR0,        0x025fb032 },
        { RT2573_TXRX_CSR1,        0x9eaa9eaf },
        { RT2573_TXRX_CSR2,        0x8a8b8c8d },
        { RT2573_TXRX_CSR3,        0x00858687 },
        { RT2573_TXRX_CSR7,        0x2E31353B },
        { RT2573_TXRX_CSR8,        0x2a2a2a2c },
        { RT2573_TXRX_CSR15,       0x0000000f },
        { RT2573_MAC_CSR6,         0x00000fff },
        { RT2573_MAC_CSR8,         0x016c030a },
        { RT2573_MAC_CSR10,        0x00000718 },
        { RT2573_MAC_CSR12,        0x00000004 },
        { RT2573_MAC_CSR13,        0x00007f00 },
        { RT2573_SEC_CSR0,         0x00000000 },
        { RT2573_SEC_CSR1,         0x00000000 },
        { RT2573_SEC_CSR5,         0x00000000 },
        { RT2573_PHY_CSR1,         0x000023b0 },
        { RT2573_PHY_CSR5,         0x00040a06 },
        { RT2573_PHY_CSR6,         0x00080606 },
        { RT2573_PHY_CSR7,         0x00000408 },
        { RT2573_AIFSN_CSR,        0x00002273 },
        { RT2573_CWMIN_CSR,        0x00002344 },
        { RT2573_CWMAX_CSR,        0x000034aa }
};

/*
 * Default values for BBP registers; values taken from the reference driver.
 */
static const struct {
	uint8_t	reg;
	uint8_t	val;
} rum_def_bbp[] = {
	{  3, 0x80 },
	{ 15, 0x30 },
	{ 17, 0x20 },
	{ 21, 0xc8 },
	{ 22, 0x38 },
	{ 23, 0x06 },
	{ 24, 0xfe },
	{ 25, 0x0a },
	{ 26, 0x0d },
	{ 32, 0x0b },
	{ 34, 0x12 },
	{ 37, 0x07 },
	{ 41, 0x60 },
	{ 53, 0x10 },
	{ 54, 0x18 },
	{ 60, 0x10 },
	{ 61, 0x04 },
	{ 62, 0x04 },
	{ 75, 0xfe },
	{ 86, 0xfe },
	{ 88, 0xfe },
	{ 90, 0x0f },
	{ 99, 0x00 },
	{ 102, 0x16 },
	{ 107, 0x04 }
};

/*
 * Default values for RF register R2 indexed by channel numbers.
 */

static const uint32_t rum_rf2528_r2[] = {
	0x001e1, 0x001e1, 0x001e2, 0x001e2, 0x001e3, 0x001e3, 0x001e4,
	0x001e4, 0x001e5, 0x001e5, 0x001e6, 0x001e6, 0x001e7, 0x001e8
};

static const uint32_t rum_rf2528_r4[] = {
	0x30282, 0x30287, 0x30282, 0x30287, 0x30282, 0x30287, 0x30282,
	0x30287, 0x30282, 0x30287, 0x39282, 0x30287, 0x30282, 0x30284
};

/*
 * For dual-band RF, RF registers R1 and R4 also depend on channel number;
 * values taken from the reference driver.
 */
static const struct rfprog {
	uint8_t		chan;
	uint32_t	r1;
	uint32_t	r2;
	uint32_t	r3;
	uint32_t	r4;
} rum_rf5222[] = {
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

USB_DECLARE_DRIVER_CLASS(rum, DV_IFNET);

void
rum_attachhook(void *xsc)
{
	struct rum_softc *sc = xsc;
	u_char				*ucode;
	char				*name;
	size_t				size;
	int				err;

        name = "ral-rt2573";

	if ((err = loadfirmware(name, &ucode, &size)) != 0) {
		printf("%s: failed loadfirmware of file %s: errno %d\n",
		    USBDEVNAME(sc->sc_dev), name, err);
		USB_ATTACH_ERROR_RETURN;
	}

	if (rum_load_microcode(sc, ucode, size) != 0) {
		printf("%s: could not load 8051 microcode\n",
		    USBDEVNAME(sc->sc_dev));
		free(ucode, M_DEVBUF);
		USB_ATTACH_ERROR_RETURN;
	}
	free(ucode, M_DEVBUF);
}

USB_MATCH(rum)
{
	USB_MATCH_START(rum, uaa);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(rum_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

USB_ATTACH(rum)
{
	USB_ATTACH_START(rum, sc, uaa);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	char *devinfop;
	int i, ntries;
	uint32_t tmp;

	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	if (usbd_set_config_no(sc->sc_udev, RT2573_CONFIG_NO, 0) != 0) {
		printf("%s: could not set configuration no\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(sc->sc_udev, RT2573_IFACE_INDEX,
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

	usb_init_task(&sc->sc_task, rum_task, sc);
	timeout_set(&sc->scan_ch, rum_next_scan, sc);

	/* retrieve RT2573 rev. no */
	sc->asic_rev = rum_read(sc, RT2573_MAC_CSR0);

	/* retrieve MAC address and various other things from EEPROM */
	rum_read_eeprom(sc);

	printf("%s: MAC/BBP RT%02x (rev 0x%02x), RF %s, address %s\n",
	    USBDEVNAME(sc->sc_dev), sc->macbbp_rev, sc->asic_rev,
	    rum_get_rf(sc->rf_rev), ether_sprintf(ic->ic_myaddr));

	/* wait for chip to settle */
	for (ntries = 0; ntries < 1000; ntries++) {
		tmp = rum_read(sc, RT2573_MAC_CSR0);
		if (tmp != 0)
			break;
		DELAY(1000);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for chip to settle\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	if (rootvp == NULL)
		mountroothook_establish(rum_attachhook, sc);
	else
		rum_attachhook(sc);


	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAp mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WEP;		/* s/w WEP */

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = rum_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = rum_rateset_11g;

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
	ifp->if_init = rum_init;
	ifp->if_ioctl = rum_ioctl;
	ifp->if_start = rum_start;
	ifp->if_watchdog = rum_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, USBDEVNAME(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = rum_newstate;
	ieee80211_media_init(ifp, rum_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(RT2573_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(RT2573_TX_RADIOTAP_PRESENT);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(rum)
{
	USB_DETACH_START(rum, sc);
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splusb();

	ieee80211_ifdetach(ifp);	/* free all nodes */
	if_detach(ifp);

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_ch);

	if (sc->sc_rx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_rx_pipeh);
		usbd_close_pipe(sc->sc_rx_pipeh);
	}

	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}

	rum_free_rx_list(sc);
	rum_free_tx_list(sc);

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return 0;
}

int
rum_alloc_tx_list(struct rum_softc *sc)
{
	struct rum_tx_data *data;
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < RT2573_TX_LIST_COUNT; i++) {
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
		    RT2573_TX_DESC_SIZE + MCLBYTES);
		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
	}

	return 0;

fail:	rum_free_tx_list(sc);
	return error;
}

void
rum_free_tx_list(struct rum_softc *sc)
{
	struct rum_tx_data *data;
	int i;

	for (i = 0; i < RT2573_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}

		/*
		 * The node has already been freed at that point so don't call
		 * ieee80211_release_node() here.
		 */
		data->ni = NULL;
	}
}

int
rum_alloc_rx_list(struct rum_softc *sc)
{
	struct rum_rx_data *data;
	int i, error;

	for (i = 0; i < RT2573_RX_LIST_COUNT; i++) {
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

fail:	rum_free_tx_list(sc);
	return error;
}

void
rum_free_rx_list(struct rum_softc *sc)
{
	struct rum_rx_data *data;
	int i;

	for (i = 0; i < RT2573_RX_LIST_COUNT; i++) {
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

int
rum_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		rum_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
void
rum_next_scan(void *arg)
{
	struct rum_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
}

void
rum_task(void *arg)
{
	struct rum_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;
	struct ieee80211_node *ni;
	uint32_t tmp;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		timeout_del(&sc->rssadapt_ch);

		if (ostate == IEEE80211_S_RUN) {
			/* abort TSF synchronization */
			tmp = rum_read(sc, RT2573_TXRX_CSR9);
			rum_write(sc, RT2573_TXRX_CSR9, tmp & ~0x00ffffff);
		}
		break;

	case IEEE80211_S_SCAN:
		rum_set_chan(sc, ic->ic_bss->ni_chan);
		timeout_add(&sc->scan_ch, hz / 5);
		break;

	case IEEE80211_S_AUTH:
		rum_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_ASSOC:
		rum_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_RUN:
		rum_set_chan(sc, ic->ic_bss->ni_chan);

		ni = ic->ic_bss;

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			rum_update_slot(sc);
			rum_set_txpreamble(sc);
			rum_set_basicrates(sc);
			rum_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
			rum_prepare_beacon(sc);

		/* make tx led blink on tx (controlled by ASIC) */
		rum_led_write(sc, RT2573_LED_RADIO|RT2573_LED_A|RT2573_LED_G, 1);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			timeout_add(&sc->rssadapt_ch, hz / 10);
			rum_enable_tsf_sync(sc);
		}
		break;
	}

	sc->sc_newstate(ic, sc->sc_state, -1);
}

int
rum_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct rum_softc *sc = ic->ic_if.if_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	usb_add_task(sc->sc_udev, &sc->sc_task);
	return 0;
}

/* quickly determine if a given rate is CCK or OFDM */
#define RT2573_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

#define RT2573_ACK_SIZE	14	/* 10 + 4(FCS) */
#define RT2573_CTS_SIZE	14	/* 10 + 4(FCS) */

#define RT2573_SIFS		10	/* us */

#define RT2573_RXTX_TURNAROUND	5	/* us */

void
rum_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct rum_tx_data *data = priv;
	struct rum_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_tx_pipeh);

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
	rum_start(ifp);

	splx(s);
}

void
rum_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct rum_rx_data *data = priv;
	struct rum_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct rum_rx_desc *desc;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *mnew, *m;
	int s, len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipeh);
		goto skip;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < RT2573_RX_DESC_SIZE + IEEE80211_MIN_LEN) {
		DPRINTF(("%s: xfer too short %d\n", USBDEVNAME(sc->sc_dev),
		    len));
		ifp->if_ierrors++;
		goto skip;
	}

	/* rx descriptor is located at the end */
	desc = (struct rum_rx_desc *)(data->buf + len - RT2573_RX_DESC_SIZE);

	if (letoh32(desc->flags) & (RT2573_RX_PHY_ERROR | RT2573_RX_CRC_ERROR)) {
		/*
		 * This should not happen since we did not request to receive
		 * those frames when we filled RT2573_TXRX_CSR2.
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
	m_adj(m, -IEEE80211_CRC_LEN);	/* trim FCS */

	s = splnet();

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct rum_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = rum_rxrate(desc);
		tap->wr_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);
		tap->wr_antenna = sc->rx_ant;
		tap->wr_antsignal = desc->rssi;

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
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
		rum_start(ifp);

	splx(s);

	DPRINTFN(15, ("rx done\n"));

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->sc_rx_pipeh, data, data->buf, MCLBYTES,
	    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rum_rxeof);
	usbd_transfer(xfer);
}

/*
 * This function is only used by the Rx radiotap code. It returns the rate at
 * which a given frame was received.
 */
#if NBPFILTER > 0
uint8_t
rum_rxrate(struct rum_rx_desc *desc)
{
	if (letoh32(desc->flags) & RT2573_RX_OFDM) {
		/* reverse function of rum_plcp_signal */
		switch (desc->rate) {
		case 0xb:	return 12;
		case 0xf:	return 18;
		case 0xa:	return 24;
		case 0xe:	return 36;
		case 0x9:	return 48;
		case 0xd:	return 72;
		case 0x8:	return 96;
		case 0xc:	return 108;
		}
	} else {
		if (desc->rate == 10)
			return 2;
		if (desc->rate == 20)
			return 4;
		if (desc->rate == 55)
			return 11;
		if (desc->rate == 110)
			return 22;
	}
	return 2;	/* should not get there */
}
#endif

/*
 * Return the expected ack rate for a frame transmitted at rate `rate'.
 * XXX: this should depend on the destination node basic rate set.
 */
int
rum_ack_rate(struct ieee80211com *ic, int rate)
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
uint16_t
rum_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;

	if (RT2573_RATE_IS_OFDM(rate)) {
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

uint8_t
rum_plcp_signal(int rate)
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

void
rum_setup_tx_desc(struct rum_softc *sc, struct rum_tx_desc *desc,
    uint32_t flags, int len, int rate)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t plcp_length;
	int remainder;

	desc->flags = htole32(flags);
	desc->flags |= htole32(RT2573_TX_NEWSEQ);
	desc->flags |= htole32(len << 16);

	desc->wme = htole16(RT2573_AIFSN(2) | RT2573_LOGCWMIN(4) | RT2573_LOGCWMAX(10));

	/* setup PLCP fields */
	desc->plcp_signal  = rum_plcp_signal(rate);
	desc->plcp_service = 4;

	len += IEEE80211_CRC_LEN;
	if (RT2573_RATE_IS_OFDM(rate)) {
		desc->flags |= htole32(RT2573_TX_OFDM);

		plcp_length = len & 0xfff;
		desc->plcp_length_hi = plcp_length >> 6;
		desc->plcp_length_lo = plcp_length & 0x3f;
	} else {
		plcp_length = (16 * len + rate - 1) / rate;
		if (rate == 22) {
			remainder = (16 * len) % 22;
			if (remainder != 0 && remainder < 7)
				desc->plcp_service |= RT2573_PLCP_LENGEXT;
		}
		desc->plcp_length_hi = plcp_length >> 8;
		desc->plcp_length_lo = plcp_length & 0xff;

		if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			desc->plcp_signal |= 0x08;
	}

	desc->iv = 0;
	desc->eiv = 0;
}

#define RT2573_TX_TIMEOUT	5000

int
rum_tx_bcn(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct rum_tx_desc *desc;
	usbd_xfer_handle xfer;
	usbd_status error;
	uint8_t cmd = 0;
	uint8_t *buf;
	int xferlen, rate;

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 2;

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return ENOMEM;

	/* xfer length needs to be a multiple of two! */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	buf = usbd_alloc_buffer(xfer, xferlen);
	if (buf == NULL) {
		usbd_free_xfer(xfer);
		return ENOMEM;
	}

	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, &cmd, sizeof cmd,
	    USBD_FORCE_SHORT_XFER, RT2573_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	if (error != 0) {
		usbd_free_xfer(xfer);
		return error;
	}

	desc = (struct rum_tx_desc *)buf;

	m_copydata(m0, 0, m0->m_pkthdr.len, buf + RT2573_TX_DESC_SIZE);
	rum_setup_tx_desc(sc, desc, RT2573_TX_IFS_NEWBACKOFF | RT2573_TX_TIMESTAMP,
	    m0->m_pkthdr.len, rate);

	DPRINTFN(10, ("sending beacon frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(xfer, sc->sc_tx_pipeh, NULL, buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RT2573_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	usbd_free_xfer(xfer);

	return error;
}

int
rum_tx_mgt(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc *desc;
	struct rum_tx_data *data;
	struct ieee80211_frame *wh;
	uint32_t flags = 0;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	data = &sc->tx_data[0];
	desc = (struct rum_tx_desc *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) ? 12 : 2;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2573_TX_ACK;

		dur = rum_txtime(RT2573_ACK_SIZE, rate, ic->ic_flags) + RT2573_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);

		/* tell hardware to add timestamp for probe responses */
		if ((wh->i_fc[0] &
		    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
		    (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RT2573_TX_TIMESTAMP;
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

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
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RT2573_TX_DESC_SIZE);
	rum_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending mgt frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RT2573_TX_TIMEOUT, rum_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;

	return 0;
}

int
rum_tx_data(struct rum_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_rateset *rs;
	struct rum_tx_desc *desc;
	struct rum_tx_data *data;
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
	desc = (struct rum_tx_desc *)data->buf;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= RT2573_TX_ACK;
		flags |= RT2573_TX_RETRY(7);

		dur = rum_txtime(RT2573_ACK_SIZE, rum_ack_rate(ic, rate),
		    ic->ic_flags) + RT2573_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct rum_tx_radiotap_header *tap = &sc->sc_txtap;

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
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + RT2573_TX_DESC_SIZE);
	rum_setup_tx_desc(sc, desc, flags, m0->m_pkthdr.len, rate);

	/* align end on a 2-bytes boundary */
	xferlen = (RT2573_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/*
	 * No space left in the last URB to store the extra 2 bytes, force
	 * sending of another URB.
	 */
	if ((xferlen % 64) == 0)
		xferlen += 2;

	DPRINTFN(10, ("sending data frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->sc_tx_pipeh, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, RT2573_TX_TIMEOUT, rum_txeof);

	error = usbd_transfer(data->xfer);
	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;
	return 0;
}

void
rum_start(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	/*
	 * net80211 may still try to send management frames even if the
	 * IFF_RUNNING flag is not set...
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= RT2573_TX_LIST_COUNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (rum_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= RT2573_TX_LIST_COUNT) {
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
			m0 = ieee80211_encap(ifp, m0, &ni);
			if (m0 == NULL)
				continue;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (rum_tx_data(sc, m0, ni) != 0) {
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

void
rum_watchdog(struct ifnet *ifp)
{
	struct rum_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", USBDEVNAME(sc->sc_dev));
			/*rum_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
rum_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct rum_softc *sc = ifp->if_softc;
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
				rum_update_promisc(sc);
			else
				rum_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rum_stop(ifp, 1);
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
			rum_set_chan(sc, ic->ic_ibss_chan);
		}
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			rum_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

void
rum_eeprom_read(struct rum_softc *sc, uint16_t addr, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_EEPROM;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not read EEPROM: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

uint32_t
rum_read(struct rum_softc *sc, uint32_t reg)
{
	usb_device_request_t req;
	usbd_status error;
	uint32_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = RT2573_READ_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, sizeof (uint32_t));

	error = usbd_do_request(sc->sc_udev, &req, &val);
	if (error != 0) {
		printf("%s: could not read MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		return 0;
	}

	return le16toh(val);
}

void
rum_read_multi(struct rum_softc *sc, uint16_t reg, void *buf, int len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 0x7;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0x0800);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not multi read MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

void
rum_write(struct rum_softc *sc, uint16_t reg, uint32_t val)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_WRITE_MAC;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	error = usbd_do_request(sc->sc_udev, &req, NULL);
	if (error != 0) {
		printf("%s: could not write MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

void
rum_write_multi(struct rum_softc *sc, uint16_t reg, void *buf, size_t len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = RT2573_WRITE_MULTI_MAC;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != 0) {
		printf("%s: could not multi write MAC register: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
	}
}

void
rum_bbp_write(struct rum_softc *sc, uint8_t reg, uint8_t val)
{
	uint16_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!((rum_read(sc, RT2573_PHY_CSR3_RT71) >> 16) & RT2573_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to BBP\n", USBDEVNAME(sc->sc_dev));
		return;
	}

	tmp = RT2573_BBP_BUSY | (reg & 0x7f) << 8 | val;
	rum_write(sc, RT2573_PHY_CSR3, tmp);
}

uint8_t
rum_bbp_read(struct rum_softc *sc, uint8_t reg)
{
	uint16_t val;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!((rum_read(sc, RT2573_PHY_CSR3) >> 16) & RT2573_BBP_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not read BBP\n", USBDEVNAME(sc->sc_dev));
		return 0;
	}

	val = RT2573_BBP_BUSY | RT2573_BBP_READ | reg << 8;
	rum_write(sc, RT2573_PHY_CSR3, val);

	for (ntries = 0; ntries < 100; ntries++) {
		val - rum_read(sc, RT2573_PHY_CSR3);
		if (!(val & RT2573_BBP_BUSY))
			return (val & 0xff);
		DELAY(1);
	}

	printf("%s: could not read BBP\n", USBDEVNAME(sc->sc_dev));
	return 0;
}

void
rum_rf_write(struct rum_softc *sc, uint8_t reg, uint32_t val)
{
	uint32_t tmp;
	int ntries;

	for (ntries = 0; ntries < 5; ntries++) {
		if (!(rum_read(sc, RT2573_PHY_CSR4) & RT2573_RF_BUSY))
			break;
	}
	if (ntries == 5) {
		printf("%s: could not write to RF\n", USBDEVNAME(sc->sc_dev));
		return;
	}

	tmp = RT2573_RF_BUSY | RT2573_RF_21BIT | (val & 0xfffff) << 2 |
	    (reg & 3);
	rum_write(sc, RT2573_PHY_CSR4,  tmp);

	/* remember last written value in sc */
	sc->rf_regs[reg] = val;

	//DPRINTFN(15, ("RF R[%u] <- 0x%05x\n", reg & 3, val & 0xfffff));
}

void
rum_set_chan(struct rum_softc *sc, struct ieee80211_channel *c)
{
        struct ieee80211com *ic = &sc->sc_ic;
        uint8_t bbp3, bbp94 = RT2573_BBPR94_DEFAULT;
        int8_t power;
        u_int chan;

        chan = ieee80211_chan2ieee(ic, c);
        if (chan == 0 || chan == IEEE80211_CHAN_ANY)
                return;

        power = sc->txpow[chan - 1];
        if (power < 0) {
                bbp94 += power;
                power = 0;
        } else if (power > 31) {
                bbp94 += power - 31;
                power = 31;
        }
        sc->sc_curchan = c;

	rum_rf_write(sc, RT2573_RF1, 0x0c808);
	rum_rf_write(sc, RT2573_RF2, rum_rf2528_r2[chan - 1]);
	rum_rf_write(sc, RT2573_RF3, power << 7 | 0x18044);
	rum_rf_write(sc, RT2573_RF4, rum_rf2528_r4[chan - 1]);

        DELAY(200);

        bbp3 = rum_bbp_read(sc, 3);

        if (bbp94 != RT2573_BBPR94_DEFAULT)
                rum_bbp_write(sc, 94, bbp94);

        /* 5GHz radio needs a 1ms delay here */
        if (IEEE80211_IS_CHAN_5GHZ(c))
                DELAY(1000);
}

/*
 * Enable TSF synchronization and tell h/w to start sending beacons for IBSS
 * and HostAP operating modes.
 */
void
rum_enable_tsf_sync(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
        uint32_t tmp;

        if (ic->ic_opmode != IEEE80211_M_STA) {
                /*
                 * Change default 16ms TBTT adjustment to 8ms.
                 * Must be done before enabling beacon generation.
                 */
                rum_write(sc, RT2573_TXRX_CSR10, 1 << 12 | 8);
        }

        tmp = rum_read(sc, RT2573_TXRX_CSR9) & 0xff000000;

        /* set beacon interval (in 1/16ms unit) */
        tmp |= ic->ic_bss->ni_intval * 16;

        tmp |= RT2573_TSF_TICKING | RT2573_ENABLE_TBTT;
        if (ic->ic_opmode == IEEE80211_M_STA)
                tmp |= RT2573_TSF_MODE(1);
        else
                tmp |= RT2573_TSF_MODE(2) | RT2573_GENERATE_BEACON;

	rum_write(sc, RT2573_TXRX_CSR9, tmp);

	DPRINTF(("enabling TSF synchronization\n"));
}

void
rum_update_slot(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t slottime;
	uint32_t tmp;

	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;

	tmp = rum_read(sc, RT2573_MAC_CSR9);
	tmp = (tmp & ~0xff) | slottime;

	rum_write(sc, RT2573_MAC_CSR9, tmp);
}

void
rum_set_txpreamble(struct rum_softc *sc)
{
	uint16_t tmp;

	tmp = rum_read(sc, RT2573_TXRX_CSR10);

	tmp &= ~RT2573_SHORT_PREAMBLE;
	if (sc->sc_ic.ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= RT2573_SHORT_PREAMBLE;

	rum_write(sc, RT2573_TXRX_CSR10, tmp);
}

void
rum_set_basicrates(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	/* update basic rate set */
	if (ic->ic_curmode == IEEE80211_MODE_11B) {
		/* 11b basic rates: 1, 2Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0x3);
	} else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan)) {
		/* 11a basic rates: 6, 12, 24Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0x150);
	} else {
		/* 11g basic rates: 1, 2, 5.5, 11, 6, 12, 24Mbps */
		rum_write(sc, RT2573_TXRX_CSR5, 0x15f);
	}
}

void
rum_set_bssid(struct rum_softc *sc, uint8_t *bssid)
{
	uint32_t tmp;

	tmp = bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24;
	rum_write(sc, RT2573_MAC_CSR4, tmp);

	/* XXX: magic number! */
	tmp = bssid[4] | bssid[5] << 8 | 0x00030000;
	rum_write(sc, RT2573_MAC_CSR5, tmp);

	DPRINTF(("setting BSSID to %s\n", ether_sprintf(bssid)));
}

void
rum_set_macaddr(struct rum_softc *sc, uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24;
	rum_write(sc, RT2573_MAC_CSR2, tmp);

	tmp = addr[4] | addr[5] << 8;
	rum_write(sc, RT2573_MAC_CSR3, tmp);

	DPRINTF(("setting MAC address to %s\n", ether_sprintf(addr)));
}

void
rum_update_promisc(struct rum_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	uint16_t tmp;

	tmp = rum_read(sc, RT2573_TXRX_CSR0);

	tmp &= ~RT2573_DROP_NOT_TO_ME;
	if (!(ifp->if_flags & IFF_PROMISC))
		tmp |= RT2573_DROP_NOT_TO_ME;

	rum_write(sc, RT2573_TXRX_CSR0, tmp);

	DPRINTF(("%s promiscuous mode\n", (ifp->if_flags & IFF_PROMISC) ?
	    "entering" : "leaving"));
}

const char *
rum_get_rf(int rev)
{
	switch (rev) {
	case RT2573_RF_2528:	return "RT2528";
	default:		return "unknown";
	}
}

void
rum_read_eeprom(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;

	/* retrieve MAC/BBP type */
	rum_eeprom_read(sc, RT2573_EEPROM_MACBBP, &val, 2);
	sc->macbbp_rev = letoh16(val);

	rum_eeprom_read(sc, RT2573_EEPROM_CONFIG0_RT71, &val, 2);
	val = letoh16(val);
	sc->rf_rev =   ((val << 5) & 0x700);
	sc->hw_radio = (val >> 10) & 0x1;
	/* reserved  (val >> 7) & 0x3; */
	/* frametype (val >> 5) & 0x01; */
	sc->rx_ant =   (val >> 4)  & 0x3;
	sc->tx_ant =   (val >> 2)  & 0x3;
	sc->nb_ant =   val & 0x3;

	/* read MAC address */
	rum_eeprom_read(sc, RT2573_EEPROM_ADDRESS, ic->ic_myaddr, 6);

	/* read default values for BBP registers */
	rum_eeprom_read(sc, RT2573_EEPROM_BBP_BASE_RT71, sc->bbp_prom, 2 * 16);

	/* read Tx power for all b/g channels */
	rum_eeprom_read(sc, RT2573_EEPROM_TXPOWER_RT71, sc->txpow, 14);
}

int
rum_bbp_init(struct rum_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	int i, ntries;
	uint16_t tmp;

	/* wait for BBP and RF to wake up (this can take a long time!) */
	for (ntries = 0; ntries < 1000; ntries++) {
		tmp = rum_read(sc, RT2573_MAC_CSR12);
		if (tmp & 8)
			break;
		/* force wakeup */
		rum_write(sc, RT2573_MAC_CSR12, 0x4);
		DELAY(1000);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for BBP/RF to wakeup\n",
		    USBDEVNAME(sc->sc_dev));
	}

	/* wait for BBP */
	for (ntries = 0; ntries < 100; ntries++) {
		tmp = rum_read(sc, 0);
		if ((tmp < 0xff) || (tmp > 0))
			break;
		DELAY(1000);
	}
	if (ntries == 100)
		printf("timeout reading BBP version\n");

	/* initialize BBP registers to default values */
	for (i = 0; i < N(rum_def_bbp); i++)
		rum_bbp_write(sc, rum_def_bbp[i].reg, rum_def_bbp[i].val);

	/* initialize BBP registers to values stored in EEPROM */
	for (i = 0; i < 16; i++) {
		if (sc->bbp_prom[i].reg == 0)
			continue;
		rum_bbp_write(sc, sc->bbp_prom[i].reg, sc->bbp_prom[i].val);
	}

	return 0;
#undef N
}

void
rum_set_txantenna(struct rum_softc *sc, int antenna)
{
	uint16_t tmp;
	uint8_t tx;

	tx = rum_bbp_read(sc, RT2573_BBP_TX) & ~RT2573_BBP_ANTMASK;
	if (antenna == 1)
		tx |= RT2573_BBP_ANTA;
	else if (antenna == 2)
		tx |= RT2573_BBP_ANTB;
	else
		tx |= RT2573_BBP_DIVERSITY;

	/* need to force I/Q flip for RF 2525e, 2526 and 5222 */
	if (sc->rf_rev == RT2573_RF_2525E || sc->rf_rev == RT2573_RF_2526 ||
	    sc->rf_rev == RT2573_RF_5222)
		tx |= RT2573_BBP_FLIPIQ;

	rum_bbp_write(sc, RT2573_BBP_TX, tx);

	/* update flags in PHY_CSR5 and PHY_CSR6 too */
	tmp = rum_read(sc, RT2573_PHY_CSR5) & ~0x7;
	rum_write(sc, RT2573_PHY_CSR5, tmp | (tx & 0x7));

	tmp = rum_read(sc, RT2573_PHY_CSR6) & ~0x7;
	rum_write(sc, RT2573_PHY_CSR6, tmp | (tx & 0x7));
}

void
rum_set_rxantenna(struct rum_softc *sc, int antenna)
{
	uint8_t rx;

	rx = rum_bbp_read(sc, RT2573_BBP_RX) & ~RT2573_BBP_ANTMASK;
	if (antenna == 1)
		rx |= RT2573_BBP_ANTA;
	else if (antenna == 2)
		rx |= RT2573_BBP_ANTB;
	else
		rx |= RT2573_BBP_DIVERSITY;

	/* need to force no I/Q flip for RF 2525e and 2526 */
	if (sc->rf_rev == RT2573_RF_2525E || sc->rf_rev == RT2573_RF_2526)
		rx &= ~RT2573_BBP_FLIPIQ;

	rum_bbp_write(sc, RT2573_BBP_RX, rx);
}

int
rum_init(struct ifnet *ifp)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_wepkey *wk;
	struct rum_rx_data *data;
	usbd_status error;
	int i;
	uint32_t tmp;

	rum_stop(ifp, 0);


	tmp = rum_read(sc, RT2573_MAC_CSR0);

	/* initialize MAC registers to default values */
	for (i = 0; i < N(rum_def_mac); i++)
		rum_write(sc, rum_def_mac[i].reg, rum_def_mac[i].val);

	/* set host ready */
	rum_write(sc, RT2573_MAC_CSR1, 0x3);
	rum_write(sc, RT2573_MAC_CSR1, 0x0);


	error = rum_bbp_init(sc);
	if (error != 0)
		goto fail;
	/* set host ready */
	rum_write(sc, RT2573_MAC_CSR1, 0x4);

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	rum_select_antenna(sc);
	rum_set_chan(sc, ic->ic_bss->ni_chan);

	/* clear statistic registers (STA_CSR0 to STA_CSR10) */
	rum_read_multi(sc, RT2573_STA_CSR0, sc->sta, sizeof sc->sta);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	rum_set_macaddr(sc, ic->ic_myaddr);

	/*
	 * Copy WEP keys into adapter's memory (SEC_CSR0 to SEC_CSR31).
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		wk = &ic->ic_nw_keys[i];
		rum_write_multi(sc, RT2573_SEC_CSR0 + i * IEEE80211_KEYBUF_SIZE,
		    wk->wk_key, IEEE80211_KEYBUF_SIZE);
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
	error = rum_alloc_tx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Tx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = rum_alloc_rx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < RT2573_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->sc_rx_pipeh, data, data->buf,
		    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, rum_rxeof);
		usbd_transfer(data->xfer);
	}

	/* kick Rx */
	tmp = RT2573_DROP_PHY_ERROR | RT2573_DROP_CRC_ERROR;
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		tmp |= RT2573_DROP_CTL | RT2573_DROP_VERSION_ERROR;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			tmp |= RT2573_DROP_TODS;
		if (!(ifp->if_flags & IFF_PROMISC))
			tmp |= RT2573_DROP_NOT_TO_ME;
	}
	rum_write(sc, RT2573_TXRX_CSR2, tmp);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	/* turn on the LED */
	rum_led_write(sc, RT2573_LED_A|RT2573_LED_G|RT2573_LED_RADIO,
	    5);
	rum_write(sc, RT2573_MAC_CSR14, RT2573_LED_ON);


	return 0;

fail:	rum_stop(ifp, 1);
	return error;
#undef N
}

void
rum_stop(struct ifnet *ifp, int disable)
{
	struct rum_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);	/* free all nodes */

	/* turn off LED */
	rum_write(sc, RT2573_MAC_CSR14, RT2573_LED_OFF);

	/* disable Rx */
	rum_write(sc, RT2573_TXRX_CSR0, RT2573_DISABLE_RX);
	rum_write(sc, RT2573_MAC_CSR10, 0x0018);
	rum_led_write(sc, 0, 0);
	/* reset ASIC and BBP (but won't reset MAC registers!) */
	/*
	rum_write(sc, RT2573_MAC_CSR1, RT2573_RESET_ASIC | RT2573_RESET_BBP);
	rum_write(sc, RT2573_MAC_CSR1, 0);
	*/
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

	rum_free_rx_list(sc);
	rum_free_tx_list(sc);
}

int
rum_activate(device_ptr_t self, enum devact act)
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

int
rum_led_write(struct rum_softc *sc, uint16_t reg, uint8_t strength)
{
        usb_device_request_t req;
        usbd_status error;

        req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
        req.bRequest = RT2573_WRITE_LED;
        USETW(req.wValue, reg);
        USETW(req.wIndex, strength);
        USETW(req.wLength, 0);

        error = usbd_do_request(sc->sc_udev, &req, NULL);
        if (error != 0) {
                printf("%s: could not write LED register: %s\n",
                    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		return (-1);
        }

	return (0);
}

int
rum_firmware_run(struct rum_softc *sc)
{
        usb_device_request_t req;
        usbd_status error;

        req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
        req.bRequest = RT2573_FIRMWARE_RUN;
        USETW(req.wValue, 0x8);
        USETW(req.wIndex, 0);
        USETW(req.wLength, 0);

        error = usbd_do_request(sc->sc_udev, &req, NULL);
        if (error != 0) {
                printf("%s: could not run firmware: %s\n",
                    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		return (-1);
	}
	return (0);
}

int
rum_load_microcode(struct rum_softc *sc, const u_char *ucode, size_t size)
{
	size_t i;

	for (i = 0; i < size; i += 2) {
		rum_write(sc, RT2573_MCU_CODE_BASE + i,
			(ucode[i+1] << 8) | ucode[i]);
	}
	/* run the firmware */
        if (rum_firmware_run(sc) < 0) {
		return (-1);
	}
	DELAY(1000);

        return (0);
}

int
rum_prepare_beacon(struct rum_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct rum_tx_desc desc;
	struct mbuf *m0;
	int rate;

	m0 = ieee80211_beacon_alloc(ic, ic->ic_bss);
	if (m0 == NULL) {
		printf("%s: could not allocate beacon frame\n",
		    sc->sc_dev.dv_xname);
		return ENOBUFS;
	}

	/* send beacons at the lowest available rate */
	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan) ? 12 : 2;

	rum_setup_tx_desc(sc, &desc, RT2573_TX_TIMESTAMP,
	    m0->m_pkthdr.len, rate);

	/* copy the first 24 bytes of Tx descriptor into NIC memory */
	rum_write_multi(sc, RT2573_HW_BEACON_BASE0, (uint8_t *)&desc, 24);

	/* copy beacon header and payload into NIC memory */
	rum_write_multi(sc, RT2573_HW_BEACON_BASE0 + 24,
	    mtod(m0, uint8_t *), m0->m_pkthdr.len);

	m_freem(m0);
	return 0;
}

void
rum_select_antenna(struct rum_softc *sc)
{
	uint8_t bbp4, bbp77;
	uint32_t tmp;

	bbp4 = rum_bbp_read(sc, 4);
	bbp77 = rum_bbp_read(sc, 77);

	tmp = rum_read(sc, RT2573_TXRX_CSR0);
	rum_write(sc, RT2573_TXRX_CSR0, tmp | RT2573_DISABLE_RX);

	rum_bbp_write(sc, 4, bbp4);
	rum_bbp_write(sc, 77, bbp77);

	rum_write(sc, RT2573_TXRX_CSR0, tmp);
}
