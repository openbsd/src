/*	$OpenBSD: if_urtwn.c,v 1.19 2011/10/29 12:18:14 gsoares Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
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
 * Driver for Realtek RTL8188CE-VAU/RTL8188CUS/RTL8188RU/RTL8192CU.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
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
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_urtwnreg.h>

#ifdef USB_DEBUG
#define URTWN_DEBUG
#endif

#ifdef URTWN_DEBUG
#define DPRINTF(x)	do { if (urtwn_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (urtwn_debug >= (n)) printf x; } while (0)
int urtwn_debug = 4;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static const struct usb_devno urtwn_devs[] = {
	{ USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_RTL8188CU_1 },
	{ USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_RTL8188CU_2 },
	{ USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_RTL8192CU },
	{ USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RTL8188CE_1 },
	{ USB_VENDOR_AZUREWAVE,	USB_PRODUCT_AZUREWAVE_RTL8188CE_2 },
	{ USB_VENDOR_BELKIN,	USB_PRODUCT_BELKIN_RTL8188CU },
	{ USB_VENDOR_COREGA,	USB_PRODUCT_COREGA_RTL8192CU },
	{ USB_VENDOR_DLINK,	USB_PRODUCT_DLINK_RTL8188CU },
	{ USB_VENDOR_DLINK,	USB_PRODUCT_DLINK_RTL8192CU_1 },
	{ USB_VENDOR_DLINK,	USB_PRODUCT_DLINK_RTL8192CU_2 },
	{ USB_VENDOR_DLINK,	USB_PRODUCT_DLINK_RTL8192CU_3 },
	{ USB_VENDOR_EDIMAX,	USB_PRODUCT_EDIMAX_RTL8188CU },
	{ USB_VENDOR_EDIMAX,	USB_PRODUCT_EDIMAX_RTL8192CU },
	{ USB_VENDOR_FEIXUN,	USB_PRODUCT_FEIXUN_RTL8188CU },
	{ USB_VENDOR_FEIXUN,	USB_PRODUCT_FEIXUN_RTL8192CU },
	{ USB_VENDOR_GUILLEMOT,	USB_PRODUCT_GUILLEMOT_HWNUP150 },
	{ USB_VENDOR_HP3,	USB_PRODUCT_HP3_RTL8188CU },
	{ USB_VENDOR_NOVATECH,	USB_PRODUCT_NOVATECH_RTL8188CU },
	{ USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_RTL8188CU_1 },
	{ USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_RTL8188CU_2 },
	{ USB_VENDOR_PLANEX2,	USB_PRODUCT_PLANEX2_RTL8192CU },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188CE_0 },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188CE_1 },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188CU_0 },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188CU_1 },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188CU_2 },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8188RU },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8191CU },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8192CE },
	{ USB_VENDOR_REALTEK,	USB_PRODUCT_REALTEK_RTL8192CU },
	{ USB_VENDOR_SITECOMEU,	USB_PRODUCT_SITECOMEU_RTL8188CU },
	{ USB_VENDOR_TRENDNET,	USB_PRODUCT_TRENDNET_RTL8188CU },
	{ USB_VENDOR_ZYXEL,	USB_PRODUCT_ZYXEL_RTL8192CU }
};

int		urtwn_match(struct device *, void *, void *);
void		urtwn_attach(struct device *, struct device *, void *);
int		urtwn_detach(struct device *, int);
int		urtwn_activate(struct device *, int);
int		urtwn_open_pipes(struct urtwn_softc *);
void		urtwn_close_pipes(struct urtwn_softc *);
int		urtwn_alloc_rx_list(struct urtwn_softc *);
void		urtwn_free_rx_list(struct urtwn_softc *);
int		urtwn_alloc_tx_list(struct urtwn_softc *);
void		urtwn_free_tx_list(struct urtwn_softc *);
void		urtwn_task(void *);
void		urtwn_do_async(struct urtwn_softc *,
		    void (*)(struct urtwn_softc *, void *), void *, int);
void		urtwn_wait_async(struct urtwn_softc *);
int		urtwn_write_region_1(struct urtwn_softc *, uint16_t, uint8_t *,
		    int);
void		urtwn_write_1(struct urtwn_softc *, uint16_t, uint8_t);
void		urtwn_write_2(struct urtwn_softc *, uint16_t, uint16_t);
void		urtwn_write_4(struct urtwn_softc *, uint16_t, uint32_t);
int		urtwn_read_region_1(struct urtwn_softc *, uint16_t, uint8_t *,
		    int);
uint8_t		urtwn_read_1(struct urtwn_softc *, uint16_t);
uint16_t	urtwn_read_2(struct urtwn_softc *, uint16_t);
uint32_t	urtwn_read_4(struct urtwn_softc *, uint16_t);
int		urtwn_fw_cmd(struct urtwn_softc *, uint8_t, const void *, int);
void		urtwn_rf_write(struct urtwn_softc *, int, uint8_t, uint32_t);
uint32_t	urtwn_rf_read(struct urtwn_softc *, int, uint8_t);
void		urtwn_cam_write(struct urtwn_softc *, uint32_t, uint32_t);
int		urtwn_llt_write(struct urtwn_softc *, uint32_t, uint32_t);
uint8_t		urtwn_efuse_read_1(struct urtwn_softc *, uint16_t);
void		urtwn_efuse_read(struct urtwn_softc *);
int		urtwn_read_chipid(struct urtwn_softc *);
void		urtwn_read_rom(struct urtwn_softc *);
int		urtwn_media_change(struct ifnet *);
int		urtwn_ra_init(struct urtwn_softc *);
void		urtwn_tsf_sync_enable(struct urtwn_softc *);
void		urtwn_set_led(struct urtwn_softc *, int, int);
void		urtwn_calib_to(void *);
void		urtwn_calib_cb(struct urtwn_softc *, void *);
void		urtwn_next_scan(void *);
int		urtwn_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
void		urtwn_newstate_cb(struct urtwn_softc *, void *);
void		urtwn_updateedca(struct ieee80211com *);
void		urtwn_updateedca_cb(struct urtwn_softc *, void *);
int		urtwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		urtwn_set_key_cb(struct urtwn_softc *, void *);
void		urtwn_delete_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
void		urtwn_delete_key_cb(struct urtwn_softc *, void *);
void		urtwn_update_avgrssi(struct urtwn_softc *, int, int8_t);
int8_t		urtwn_get_rssi(struct urtwn_softc *, int, void *);
void		urtwn_rx_frame(struct urtwn_softc *, uint8_t *, int);
void		urtwn_rxeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
void		urtwn_txeof(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
int		urtwn_tx(struct urtwn_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		urtwn_start(struct ifnet *);
void		urtwn_watchdog(struct ifnet *);
int		urtwn_ioctl(struct ifnet *, u_long, caddr_t);
int		urtwn_power_on(struct urtwn_softc *);
int		urtwn_llt_init(struct urtwn_softc *);
void		urtwn_fw_reset(struct urtwn_softc *);
int		urtwn_fw_loadpage(struct urtwn_softc *, int, uint8_t *, int);
int		urtwn_load_firmware(struct urtwn_softc *);
int		urtwn_dma_init(struct urtwn_softc *);
void		urtwn_mac_init(struct urtwn_softc *);
void		urtwn_bb_init(struct urtwn_softc *);
void		urtwn_rf_init(struct urtwn_softc *);
void		urtwn_cam_init(struct urtwn_softc *);
void		urtwn_pa_bias_init(struct urtwn_softc *);
void		urtwn_rxfilter_init(struct urtwn_softc *);
void		urtwn_edca_init(struct urtwn_softc *);
void		urtwn_write_txpower(struct urtwn_softc *, int, uint16_t[]);
void		urtwn_get_txpower(struct urtwn_softc *, int,
		    struct ieee80211_channel *, struct ieee80211_channel *,
		    uint16_t[]);
void		urtwn_set_txpower(struct urtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		urtwn_set_chan(struct urtwn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
int		urtwn_iq_calib_chain(struct urtwn_softc *, int, uint16_t[],
		    uint16_t[]);
void		urtwn_iq_calib(struct urtwn_softc *);
void		urtwn_lc_calib(struct urtwn_softc *);
void		urtwn_temp_calib(struct urtwn_softc *);
int		urtwn_init(struct ifnet *);
void		urtwn_stop(struct ifnet *);

/* Aliases. */
#define	urtwn_bb_write	urtwn_write_4
#define urtwn_bb_read	urtwn_read_4

struct cfdriver urtwn_cd = {
	NULL, "urtwn", DV_IFNET
};

const struct cfattach urtwn_ca = {
	sizeof(struct urtwn_softc),
	urtwn_match,
	urtwn_attach,
	urtwn_detach,
	urtwn_activate
};

int
urtwn_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return ((usb_lookup(urtwn_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void
urtwn_attach(struct device *parent, struct device *self, void *aux)
{
	struct urtwn_softc *sc = (struct urtwn_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int i, error;

	sc->sc_udev = uaa->device;

	usb_init_task(&sc->sc_task, urtwn_task, sc, USB_TASK_TYPE_GENERIC);
	timeout_set(&sc->scan_to, urtwn_next_scan, sc);
	timeout_set(&sc->calib_to, urtwn_calib_to, sc);

	if (usbd_set_config_no(sc->sc_udev, 1, 0) != 0) {
		printf("%s: could not set configuration no\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Get the first interface handle. */
	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (error != 0) {
		printf("%s: could not get interface handle\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	error = urtwn_read_chipid(sc);
	if (error != 0) {
		printf("%s: unsupported test chip\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Determine number of Tx/Rx chains. */
	if (sc->chip & URTWN_CHIP_92C) {
		sc->ntxchains = (sc->chip & URTWN_CHIP_92C_1T2R) ? 1 : 2;
		sc->nrxchains = 2;
	} else {
		sc->ntxchains = 1;
		sc->nrxchains = 1;
	}
	urtwn_read_rom(sc);

	printf("%s: MAC/BB RTL%s, RF 6052 %dT%dR, address %s\n",
	    sc->sc_dev.dv_xname,
	    (sc->chip & URTWN_CHIP_92C) ? "8192CU" :
	    (sc->board_type == R92C_BOARD_TYPE_HIGHPA) ? "8188RU" :
	    (sc->board_type == R92C_BOARD_TYPE_MINICARD) ? "8188CE-VAU" :
	    "8188CUS", sc->ntxchains, sc->nrxchains,
	    ether_sprintf(ic->ic_myaddr));

	if (urtwn_open_pipes(sc) != 0)
		return;

	ic->ic_phytype = IEEE80211_T_OFDM;	/* Not only, but not used. */
	ic->ic_opmode = IEEE80211_M_STA;	/* Default to BSS mode. */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* Monitor mode supported. */
	    IEEE80211_C_SHPREAMBLE |	/* Short preamble supported. */
	    IEEE80211_C_SHSLOT |	/* Short slot time supported. */
	    IEEE80211_C_WEP |		/* WEP. */
	    IEEE80211_C_RSN;		/* WPA/RSN. */

#ifndef IEEE80211_NO_HT
	/* Set HT capabilities. */
	ic->ic_htcaps =
	    IEEE80211_HTCAP_CBW20_40 |
	    IEEE80211_HTCAP_DSSSCCK40;
	/* Set supported HT rates. */
	for (i = 0; i < sc->nrxchains; i++)
		ic->ic_sup_mcs[i] = 0xff;
#endif

	/* Set supported .11b and .11g rates. */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* Set supported .11b and .11g channels (1 through 14). */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}
	/*
	 * The number of STAs that we can support is limited by the number
	 * of CAM entries used for hardware crypto.
	 */
	ic->ic_max_nnodes = R92C_CAM_ENTRY_COUNT - 4;
	if (ic->ic_max_nnodes > IEEE80211_CACHE_SIZE)
		ic->ic_max_nnodes = IEEE80211_CACHE_SIZE;

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = urtwn_ioctl;
	ifp->if_start = urtwn_start;
	ifp->if_watchdog = urtwn_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_updateedca = urtwn_updateedca;
#ifdef notyet
	ic->ic_set_key = urtwn_set_key;
	ic->ic_delete_key = urtwn_delete_key;
#endif
	/* Override state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = urtwn_newstate;
	ieee80211_media_init(ifp, urtwn_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(URTWN_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(URTWN_TX_RADIOTAP_PRESENT);
#endif
}

int
urtwn_detach(struct device *self, int flags)
{
	struct urtwn_softc *sc = (struct urtwn_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splusb();

	if (timeout_initialized(&sc->scan_to))
		timeout_del(&sc->scan_to);
	if (timeout_initialized(&sc->calib_to))
		timeout_del(&sc->calib_to);

	/* Wait for all async commands to complete. */
	usb_rem_wait_task(sc->sc_udev, &sc->sc_task);

	usbd_ref_wait(sc->sc_udev);

	if (ifp->if_softc != NULL) {
		ieee80211_ifdetach(ifp);
		if_detach(ifp);
	}

	/* Abort and close Tx/Rx pipes. */
	urtwn_close_pipes(sc);

	/* Free Tx/Rx buffers. */
	urtwn_free_tx_list(sc);
	urtwn_free_rx_list(sc);
	splx(s);

	return (0);
}

int
urtwn_activate(struct device *self, int act)
{
	struct urtwn_softc *sc = (struct urtwn_softc *)self;

	switch (act) {
	case DVACT_DEACTIVATE:
		usbd_deactivate(sc->sc_udev);
		break;
	}
	return (0);
}

int
urtwn_open_pipes(struct urtwn_softc *sc)
{
	/* Bulk-out endpoints addresses (from highest to lowest prio). */
	const uint8_t epaddr[] = { 0x02, 0x03, 0x05 };
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i, ntx = 0, error;

	/* Determine the number of bulk-out pipes. */
	id = usbd_get_interface_descriptor(sc->sc_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed != NULL &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT)
			ntx++;
	}
	DPRINTF(("found %d bulk-out pipes\n", ntx));
	if (ntx == 0 || ntx > R92C_MAX_EPOUT) {
		printf("%s: %d: invalid number of Tx bulk pipes\n",
		    sc->sc_dev.dv_xname, ntx);
		return (EIO);
	}

	/* Open bulk-in pipe at address 0x81. */
	error = usbd_open_pipe(sc->sc_iface, 0x81, 0, &sc->rx_pipe);
	if (error != 0) {
		printf("%s: could not open Rx bulk pipe\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/* Open bulk-out pipes (up to 3). */
	for (i = 0; i < ntx; i++) {
		error = usbd_open_pipe(sc->sc_iface, epaddr[i], 0,
		    &sc->tx_pipe[i]);
		if (error != 0) {
			printf("%s: could not open Tx bulk pipe 0x%02x\n",
			    sc->sc_dev.dv_xname, epaddr[i]);
			goto fail;
		}
	}

	/* Map 802.11 access categories to USB pipes. */
	sc->ac2idx[EDCA_AC_BK] =
	sc->ac2idx[EDCA_AC_BE] = (ntx == 3) ? 2 : ((ntx == 2) ? 1 : 0);
	sc->ac2idx[EDCA_AC_VI] = (ntx == 3) ? 1 : 0;
	sc->ac2idx[EDCA_AC_VO] = 0;	/* Always use highest prio. */

	if (error != 0)
 fail:		urtwn_close_pipes(sc);
	return (error);
}

void
urtwn_close_pipes(struct urtwn_softc *sc)
{
	int i;

	/* Close Rx pipe. */
	if (sc->rx_pipe != NULL) {
		usbd_abort_pipe(sc->rx_pipe);
		usbd_close_pipe(sc->rx_pipe);
	}
	/* Close Tx pipes. */
	for (i = 0; i < R92C_MAX_EPOUT; i++) {
		if (sc->tx_pipe[i] == NULL)
			continue;
		usbd_abort_pipe(sc->tx_pipe[i]);
		usbd_close_pipe(sc->tx_pipe[i]);
	}
}

int
urtwn_alloc_rx_list(struct urtwn_softc *sc)
{
	struct urtwn_rx_data *data;
	int i, error = 0;

	for (i = 0; i < URTWN_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		data->sc = sc;	/* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate xfer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			break;
		}
		data->buf = usbd_alloc_buffer(data->xfer, URTWN_RXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate xfer buffer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			break;
		}
	}
	if (error != 0)
		urtwn_free_rx_list(sc);
	return (error);
}

void
urtwn_free_rx_list(struct urtwn_softc *sc)
{
	int i;

	/* NB: Caller must abort pipe first. */
	for (i = 0; i < URTWN_RX_LIST_COUNT; i++) {
		if (sc->rx_data[i].xfer != NULL)
			usbd_free_xfer(sc->rx_data[i].xfer);
		sc->rx_data[i].xfer = NULL;
	}
}

int
urtwn_alloc_tx_list(struct urtwn_softc *sc)
{
	struct urtwn_tx_data *data;
	int i, error = 0;

	TAILQ_INIT(&sc->tx_free_list);
	for (i = 0; i < URTWN_TX_LIST_COUNT; i++) {
		data = &sc->tx_data[i];

		data->sc = sc;	/* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate xfer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			break;
		}
		data->buf = usbd_alloc_buffer(data->xfer, URTWN_TXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate xfer buffer\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			break;
		}
		/* Append this Tx buffer to our free list. */
		TAILQ_INSERT_TAIL(&sc->tx_free_list, data, next);
	}
	if (error != 0)
		urtwn_free_tx_list(sc);
	return (error);
}

void
urtwn_free_tx_list(struct urtwn_softc *sc)
{
	int i;

	/* NB: Caller must abort pipe first. */
	for (i = 0; i < URTWN_TX_LIST_COUNT; i++) {
		if (sc->tx_data[i].xfer != NULL)
			usbd_free_xfer(sc->tx_data[i].xfer);
		sc->tx_data[i].xfer = NULL;
	}
}

void
urtwn_task(void *arg)
{
	struct urtwn_softc *sc = arg;
	struct urtwn_host_cmd_ring *ring = &sc->cmdq;
	struct urtwn_host_cmd *cmd;
	int s;

	/* Process host commands. */
	s = splusb();
	while (ring->next != ring->cur) {
		cmd = &ring->cmd[ring->next];
		splx(s);
		/* Invoke callback. */
		cmd->cb(sc, cmd->data);
		s = splusb();
		ring->queued--;
		ring->next = (ring->next + 1) % URTWN_HOST_CMD_RING_COUNT;
	}
	splx(s);
}

void
urtwn_do_async(struct urtwn_softc *sc,
    void (*cb)(struct urtwn_softc *, void *), void *arg, int len)
{
	struct urtwn_host_cmd_ring *ring = &sc->cmdq;
	struct urtwn_host_cmd *cmd;
	int s;

	s = splusb();
	cmd = &ring->cmd[ring->cur];
	cmd->cb = cb;
	KASSERT(len <= sizeof(cmd->data));
	memcpy(cmd->data, arg, len);
	ring->cur = (ring->cur + 1) % URTWN_HOST_CMD_RING_COUNT;

	/* If there is no pending command already, schedule a task. */
	if (++ring->queued == 1)
		usb_add_task(sc->sc_udev, &sc->sc_task);
	splx(s);
}

void
urtwn_wait_async(struct urtwn_softc *sc)
{
	/* Wait for all queued asynchronous commands to complete. */
	usb_wait_task(sc->sc_udev, &sc->sc_task);
}

int
urtwn_write_region_1(struct urtwn_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = R92C_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usbd_do_request(sc->sc_udev, &req, buf));
}

void
urtwn_write_1(struct urtwn_softc *sc, uint16_t addr, uint8_t val)
{
	urtwn_write_region_1(sc, addr, &val, 1);
}

void
urtwn_write_2(struct urtwn_softc *sc, uint16_t addr, uint16_t val)
{
	val = htole16(val);
	urtwn_write_region_1(sc, addr, (uint8_t *)&val, 2);
}

void
urtwn_write_4(struct urtwn_softc *sc, uint16_t addr, uint32_t val)
{
	val = htole32(val);
	urtwn_write_region_1(sc, addr, (uint8_t *)&val, 4);
}

int
urtwn_read_region_1(struct urtwn_softc *sc, uint16_t addr, uint8_t *buf,
    int len)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = R92C_REQ_REGS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usbd_do_request(sc->sc_udev, &req, buf));
}

uint8_t
urtwn_read_1(struct urtwn_softc *sc, uint16_t addr)
{
	uint8_t val;

	if (urtwn_read_region_1(sc, addr, &val, 1) != 0)
		return (0xff);
	return (val);
}

uint16_t
urtwn_read_2(struct urtwn_softc *sc, uint16_t addr)
{
	uint16_t val;

	if (urtwn_read_region_1(sc, addr, (uint8_t *)&val, 2) != 0)
		return (0xffff);
	return (letoh16(val));
}

uint32_t
urtwn_read_4(struct urtwn_softc *sc, uint16_t addr)
{
	uint32_t val;

	if (urtwn_read_region_1(sc, addr, (uint8_t *)&val, 4) != 0)
		return (0xffffffff);
	return (letoh32(val));
}

int
urtwn_fw_cmd(struct urtwn_softc *sc, uint8_t id, const void *buf, int len)
{
	struct r92c_fw_cmd cmd;
	int ntries;

	/* Wait for current FW box to be empty. */
	for (ntries = 0; ntries < 100; ntries++) {
		if (!(urtwn_read_1(sc, R92C_HMETFR) & (1 << sc->fwcur)))
			break;
		DELAY(1);
	}
	if (ntries == 100) {
		printf("%s: could not send firmware command %d\n",
		    sc->sc_dev.dv_xname, id);
		return (ETIMEDOUT);
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.id = id;
	if (len > 3)
		cmd.id |= R92C_CMD_FLAG_EXT;
	KASSERT(len <= sizeof(cmd.msg));
	memcpy(cmd.msg, buf, len);

	/* Write the first word last since that will trigger the FW. */
	urtwn_write_region_1(sc, R92C_HMEBOX_EXT(sc->fwcur),
	    (uint8_t *)&cmd + 4, 2);
	urtwn_write_region_1(sc, R92C_HMEBOX(sc->fwcur),
	    (uint8_t *)&cmd + 0, 4);

	sc->fwcur = (sc->fwcur + 1) % R92C_H2C_NBOX;
	return (0);
}

void
urtwn_rf_write(struct urtwn_softc *sc, int chain, uint8_t addr, uint32_t val)
{
	urtwn_bb_write(sc, R92C_LSSI_PARAM(chain),
	    SM(R92C_LSSI_PARAM_ADDR, addr) |
	    SM(R92C_LSSI_PARAM_DATA, val));
}

uint32_t
urtwn_rf_read(struct urtwn_softc *sc, int chain, uint8_t addr)
{
	uint32_t reg[R92C_MAX_CHAINS], val;

	reg[0] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(0));
	if (chain != 0)
		reg[chain] = urtwn_bb_read(sc, R92C_HSSI_PARAM2(chain));

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] & ~R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(chain),
	    RW(reg[chain], R92C_HSSI_PARAM2_READ_ADDR, addr) |
	    R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	urtwn_bb_write(sc, R92C_HSSI_PARAM2(0),
	    reg[0] | R92C_HSSI_PARAM2_READ_EDGE);
	DELAY(1000);

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM1(chain)) & R92C_HSSI_PARAM1_PI)
		val = urtwn_bb_read(sc, R92C_HSPI_READBACK(chain));
	else
		val = urtwn_bb_read(sc, R92C_LSSI_READBACK(chain));
	return (MS(val, R92C_LSSI_READBACK_DATA));
}

void
urtwn_cam_write(struct urtwn_softc *sc, uint32_t addr, uint32_t data)
{
	urtwn_write_4(sc, R92C_CAMWRITE, data);
	urtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_WRITE |
	    SM(R92C_CAMCMD_ADDR, addr));
}

int
urtwn_llt_write(struct urtwn_softc *sc, uint32_t addr, uint32_t data)
{
	int ntries;

	urtwn_write_4(sc, R92C_LLT_INIT,
	    SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) |
	    SM(R92C_LLT_INIT_ADDR, addr) |
	    SM(R92C_LLT_INIT_DATA, data));
	/* Wait for write operation to complete. */
	for (ntries = 0; ntries < 20; ntries++) {
		if (MS(urtwn_read_4(sc, R92C_LLT_INIT), R92C_LLT_INIT_OP) ==
		    R92C_LLT_INIT_OP_NO_ACTIVE)
			return (0);
		DELAY(5);
	}
	return (ETIMEDOUT);
}

uint8_t
urtwn_efuse_read_1(struct urtwn_softc *sc, uint16_t addr)
{
	uint32_t reg;
	int ntries;

	reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
	reg = RW(reg, R92C_EFUSE_CTRL_ADDR, addr);
	reg &= ~R92C_EFUSE_CTRL_VALID;
	urtwn_write_4(sc, R92C_EFUSE_CTRL, reg);
	/* Wait for read operation to complete. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = urtwn_read_4(sc, R92C_EFUSE_CTRL);
		if (reg & R92C_EFUSE_CTRL_VALID)
			return (MS(reg, R92C_EFUSE_CTRL_DATA));
		DELAY(5);
	}
	printf("%s: could not read efuse byte at address 0x%x\n",
	    sc->sc_dev.dv_xname, addr);
	return (0xff);
}

void
urtwn_efuse_read(struct urtwn_softc *sc)
{
	uint8_t *rom = (uint8_t *)&sc->rom;
	uint16_t addr = 0;
	uint32_t reg;
	uint8_t off, msk;
	int i;

	reg = urtwn_read_2(sc, R92C_SYS_ISO_CTRL);
	if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) {
		urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
		    reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
	}
	reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
	if (!(reg & R92C_SYS_FUNC_EN_ELDR)) {
		urtwn_write_2(sc, R92C_SYS_FUNC_EN,
		    reg | R92C_SYS_FUNC_EN_ELDR);
	}
	reg = urtwn_read_2(sc, R92C_SYS_CLKR);
	if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) !=
	    (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) {
		urtwn_write_2(sc, R92C_SYS_CLKR,
		    reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);
	}
	memset(&sc->rom, 0xff, sizeof(sc->rom));
	while (addr < 512) {
		reg = urtwn_efuse_read_1(sc, addr);
		if (reg == 0xff)
			break;
		addr++;
		off = reg >> 4;
		msk = reg & 0xf;
		for (i = 0; i < 4; i++) {
			if (msk & (1 << i))
				continue;
			rom[off * 8 + i * 2 + 0] =
			    urtwn_efuse_read_1(sc, addr);
			addr++;
			rom[off * 8 + i * 2 + 1] =
			    urtwn_efuse_read_1(sc, addr);
			addr++;
		}
	}
#ifdef URTWN_DEBUG
	if (urtwn_debug >= 2) {
		/* Dump ROM content. */
		printf("\n");
		for (i = 0; i < sizeof(sc->rom); i++)
			printf("%02x:", rom[i]);
		printf("\n");
	}
#endif
}

int
urtwn_read_chipid(struct urtwn_softc *sc)
{
	uint32_t reg;

	reg = urtwn_read_4(sc, R92C_SYS_CFG);
	if (reg & R92C_SYS_CFG_TRP_VAUX_EN)
		return (EIO);

	if (reg & R92C_SYS_CFG_TYPE_92C) {
		sc->chip |= URTWN_CHIP_92C;
		/* Check if it is a castrated 8192C. */
		if (MS(urtwn_read_4(sc, R92C_HPON_FSM),
		    R92C_HPON_FSM_CHIP_BONDING_ID) ==
		    R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
			sc->chip |= URTWN_CHIP_92C_1T2R;
	}
	if (reg & R92C_SYS_CFG_VENDOR_UMC) {
		sc->chip |= URTWN_CHIP_UMC;
		if (MS(reg, R92C_SYS_CFG_CHIP_VER_RTL) == 0)
			sc->chip |= URTWN_CHIP_UMC_A_CUT;
	}
	return (0);
}

void
urtwn_read_rom(struct urtwn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;

	/* Read full ROM image. */
	urtwn_efuse_read(sc);

	/* XXX Weird but this is what the vendor driver does. */
	sc->pa_setting = urtwn_efuse_read_1(sc, 0x1fa);
	DPRINTF(("PA setting=0x%x\n", sc->pa_setting));

	sc->board_type = MS(rom->rf_opt1, R92C_ROM_RF1_BOARD_TYPE);

	sc->regulatory = MS(rom->rf_opt1, R92C_ROM_RF1_REGULATORY);
	DPRINTF(("regulatory type=%d\n", sc->regulatory));

	IEEE80211_ADDR_COPY(ic->ic_myaddr, rom->macaddr);
}

int
urtwn_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return (error);

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		urtwn_stop(ifp);
		urtwn_init(ifp);
	}
	return (0);
}

/*
 * Initialize rate adaptation in firmware.
 */
int
urtwn_ra_init(struct urtwn_softc *sc)
{
	static const uint8_t map[] =
	    { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	struct r92c_fw_cmd_macid_cfg cmd;
	uint32_t rates, basicrates;
	uint8_t mode;
	int maxrate, maxbasicrate, error, i, j;

	/* Get normal and basic rates mask. */
	rates = basicrates = 0;
	maxrate = maxbasicrate = 0;
	for (i = 0; i < rs->rs_nrates; i++) {
		/* Convert 802.11 rate to HW rate index. */
		for (j = 0; j < nitems(map); j++)
			if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == map[j])
				break;
		if (j == nitems(map))	/* Unknown rate, skip. */
			continue;
		rates |= 1 << j;
		if (j > maxrate)
			maxrate = j;
		if (rs->rs_rates[i] & IEEE80211_RATE_BASIC) {
			basicrates |= 1 << j;
			if (j > maxbasicrate)
				maxbasicrate = j;
		}
	}
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mode = R92C_RAID_11B;
	else
		mode = R92C_RAID_11BG;
	DPRINTF(("mode=0x%x rates=0x%08x, basicrates=0x%08x\n",
	    mode, rates, basicrates));

	/* Set rates mask for group addressed frames. */
	cmd.macid = URTWN_MACID_BC | URTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | basicrates);
	error = urtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		printf("%s: could not add broadcast station\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxbasicrate=%d\n", maxbasicrate));
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BC),
	    maxbasicrate);

	/* Set rates mask for unicast frames. */
	cmd.macid = URTWN_MACID_BSS | URTWN_MACID_VALID;
	cmd.mask = htole32(mode << 28 | rates);
	error = urtwn_fw_cmd(sc, R92C_CMD_MACID_CONFIG, &cmd, sizeof(cmd));
	if (error != 0) {
		printf("%s: could not add BSS station\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}
	/* Set initial MRR rate. */
	DPRINTF(("maxrate=%d\n", maxrate));
	urtwn_write_1(sc, R92C_INIDATA_RATE_SEL(URTWN_MACID_BSS),
	    maxrate);

	/* Indicate highest supported rate. */
	ni->ni_txrate = rs->rs_nrates - 1;
	return (0);
}

void
urtwn_tsf_sync_enable(struct urtwn_softc *sc)
{
	struct ieee80211_node *ni = sc->sc_ic.ic_bss;
	uint64_t tsf;

	/* Enable TSF synchronization. */
	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);

	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_EN_BCN);

	/* Set initial TSF. */
	memcpy(&tsf, ni->ni_tstamp, 8);
	tsf = letoh64(tsf);
	tsf = tsf - (tsf % (ni->ni_intval * IEEE80211_DUR_TU));
	tsf -= IEEE80211_DUR_TU;
	urtwn_write_4(sc, R92C_TSFTR + 0, tsf);
	urtwn_write_4(sc, R92C_TSFTR + 4, tsf >> 32);

	urtwn_write_1(sc, R92C_BCN_CTRL,
	    urtwn_read_1(sc, R92C_BCN_CTRL) | R92C_BCN_CTRL_EN_BCN);
}

void
urtwn_set_led(struct urtwn_softc *sc, int led, int on)
{
	uint8_t reg;

	if (led == URTWN_LED_LINK) {
		reg = urtwn_read_1(sc, R92C_LEDCFG0) & 0x70;
		if (!on)
			reg |= R92C_LEDCFG0_DIS;
		urtwn_write_1(sc, R92C_LEDCFG0, reg);
		sc->ledlink = on;	/* Save LED state. */
	}
}

void
urtwn_calib_to(void *arg)
{
	struct urtwn_softc *sc = arg;

	if (usbd_is_dying(sc->sc_udev))
		return;

	usbd_ref_incr(sc->sc_udev);

	/* Do it in a process context. */
	urtwn_do_async(sc, urtwn_calib_cb, NULL, 0);

	usbd_ref_decr(sc->sc_udev);
}

/* ARGSUSED */
void
urtwn_calib_cb(struct urtwn_softc *sc, void *arg)
{
	struct r92c_fw_cmd_rssi cmd;

	if (sc->avg_pwdb != -1) {
		/* Indicate Rx signal strength to FW for rate adaptation. */
		memset(&cmd, 0, sizeof(cmd));
		cmd.macid = 0;	/* BSS. */
		cmd.pwdb = sc->avg_pwdb;
		DPRINTFN(3, ("sending RSSI command avg=%d\n", sc->avg_pwdb));
		urtwn_fw_cmd(sc, R92C_CMD_RSSI_SETTING, &cmd, sizeof(cmd));
	}

	/* Do temperature compensation. */
	urtwn_temp_calib(sc);

	if (!usbd_is_dying(sc->sc_udev))
		timeout_add_sec(&sc->calib_to, 2);
}

void
urtwn_next_scan(void *arg)
{
	struct urtwn_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	if (usbd_is_dying(sc->sc_udev))
		return;

	usbd_ref_incr(sc->sc_udev);

	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&ic->ic_if);
	splx(s);

	usbd_ref_decr(sc->sc_udev);
}

int
urtwn_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_cmd_newstate cmd;

	/* Do it in a process context. */
	cmd.state = nstate;
	cmd.arg = arg;
	urtwn_do_async(sc, urtwn_newstate_cb, &cmd, sizeof(cmd));
	return (0);
}

void
urtwn_newstate_cb(struct urtwn_softc *sc, void *arg)
{
	struct urtwn_cmd_newstate *cmd = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;
	uint32_t reg;
	int s;

	s = splnet();
	ostate = ic->ic_state;
	DPRINTF(("newstate %d -> %d\n", ostate, cmd->state));

	if (ostate == IEEE80211_S_RUN) {
		/* Stop calibration. */
		timeout_del(&sc->calib_to);

		/* Turn link LED off. */
		urtwn_set_led(sc, URTWN_LED_LINK, 0);

		/* Set media status to 'No Link'. */
		reg = urtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_NOLINK);
		urtwn_write_4(sc, R92C_CR, reg);

		/* Stop Rx of data frames. */
		urtwn_write_2(sc, R92C_RXFLTMAP2, 0);

		/* Rest TSF. */
		urtwn_write_1(sc, R92C_DUAL_TSF_RST, 0x03);

		/* Disable TSF synchronization. */
		urtwn_write_1(sc, R92C_BCN_CTRL,
		    urtwn_read_1(sc, R92C_BCN_CTRL) |
		    R92C_BCN_CTRL_DIS_TSF_UDT0);

		/* Reset EDCA parameters. */
		urtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002f3217);
		urtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005e4317);
		urtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x00105320);
		urtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a444);
	}
	switch (cmd->state) {
	case IEEE80211_S_INIT:
		/* Turn link LED off. */
		urtwn_set_led(sc, URTWN_LED_LINK, 0);
		break;
	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			/* Allow Rx from any BSSID. */
			urtwn_write_4(sc, R92C_RCR,
			    urtwn_read_4(sc, R92C_RCR) &
			    ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));

			/* Set gain for scanning. */
			reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

			reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
			reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x20);
			urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);
		}

		/* Make link LED blink during scan. */
		urtwn_set_led(sc, URTWN_LED_LINK, !sc->ledlink);

		/* Pause AC Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE,
		    urtwn_read_1(sc, R92C_TXPAUSE) | 0x0f);

		urtwn_set_chan(sc, ic->ic_bss->ni_chan, NULL);
		if (!usbd_is_dying(sc->sc_udev))
			timeout_add_msec(&sc->scan_to, 200);
		break;

	case IEEE80211_S_AUTH:
		/* Set initial gain under link. */
		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(0));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(0), reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCCORE1(1));
		reg = RW(reg, R92C_OFDM0_AGCCORE1_GAIN, 0x32);
		urtwn_bb_write(sc, R92C_OFDM0_AGCCORE1(1), reg);

		urtwn_set_chan(sc, ic->ic_bss->ni_chan, NULL);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			urtwn_set_chan(sc, ic->ic_ibss_chan, NULL);

			/* Enable Rx of data frames. */
			urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

			/* Turn link LED on. */
			urtwn_set_led(sc, URTWN_LED_LINK, 1);
			break;
		}
		ni = ic->ic_bss;

		/* Set media status to 'Associated'. */
		reg = urtwn_read_4(sc, R92C_CR);
		reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
		urtwn_write_4(sc, R92C_CR, reg);

		/* Set BSSID. */
		urtwn_write_4(sc, R92C_BSSID + 0, LE_READ_4(&ni->ni_bssid[0]));
		urtwn_write_4(sc, R92C_BSSID + 4, LE_READ_2(&ni->ni_bssid[4]));

		if (ic->ic_curmode == IEEE80211_MODE_11B)
			urtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 0);
		else	/* 802.11b/g */
			urtwn_write_1(sc, R92C_INIRTS_RATE_SEL, 3);

		/* Enable Rx of data frames. */
		urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);

		/* Flush all AC queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0);

		/* Set beacon interval. */
		urtwn_write_2(sc, R92C_BCN_INTERVAL, ni->ni_intval);

		/* Allow Rx from our BSSID only. */
		urtwn_write_4(sc, R92C_RCR,
		    urtwn_read_4(sc, R92C_RCR) |
		    R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);

		/* Enable TSF synchronization. */
		urtwn_tsf_sync_enable(sc);

		urtwn_write_1(sc, R92C_SIFS_CCK + 1, 10);
		urtwn_write_1(sc, R92C_SIFS_OFDM + 1, 10);
		urtwn_write_1(sc, R92C_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_MAC_SPEC_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_R2T_SIFS + 1, 10);
		urtwn_write_1(sc, R92C_T2T_SIFS + 1, 10);

		/* Intialize rate adaptation. */
		urtwn_ra_init(sc);
		/* Turn link LED on. */
		urtwn_set_led(sc, URTWN_LED_LINK, 1);

		sc->avg_pwdb = -1;	/* Reset average RSSI. */
		/* Reset temperature calibration state machine. */
		sc->thcal_state = 0;
		sc->thcal_lctemp = 0;
		/* Start periodic calibration. */
		if (!usbd_is_dying(sc->sc_udev))
			timeout_add_sec(&sc->calib_to, 2);
		break;
	}
	(void)sc->sc_newstate(ic, cmd->state, cmd->arg);
	splx(s);
}

void
urtwn_updateedca(struct ieee80211com *ic)
{
	/* Do it in a process context. */
	urtwn_do_async(ic->ic_softc, urtwn_updateedca_cb, NULL, 0);
}

/* ARGSUSED */
void
urtwn_updateedca_cb(struct urtwn_softc *sc, void *arg)
{
	const uint16_t aci2reg[EDCA_NUM_AC] = {
		R92C_EDCA_BE_PARAM,
		R92C_EDCA_BK_PARAM,
		R92C_EDCA_VI_PARAM,
		R92C_EDCA_VO_PARAM
	};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_edca_ac_params *ac;
	int s, aci, aifs, slottime;

	s = splnet();
	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		ac = &ic->ic_edca_ac[aci];
		/* AIFS[AC] = AIFSN[AC] * aSlotTime + aSIFSTime. */
		aifs = ac->ac_aifsn * slottime + 10;
		urtwn_write_4(sc, aci2reg[aci],
		    SM(R92C_EDCA_PARAM_TXOP, ac->ac_txoplimit) |
		    SM(R92C_EDCA_PARAM_ECWMIN, ac->ac_ecwmin) |
		    SM(R92C_EDCA_PARAM_ECWMAX, ac->ac_ecwmax) |
		    SM(R92C_EDCA_PARAM_AIFS, aifs));
	}
	splx(s);
}

int
urtwn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_cmd_key cmd;

	/* Defer setting of WEP keys until interface is brought up. */
	if ((ic->ic_if.if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return (0);

	/* Do it in a process context. */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	urtwn_do_async(sc, urtwn_set_key_cb, &cmd, sizeof(cmd));
	return (0);
}

void
urtwn_set_key_cb(struct urtwn_softc *sc, void *arg)
{
	static const uint8_t etherzeroaddr[6] = { 0 };
	struct ieee80211com *ic = &sc->sc_ic;
	struct urtwn_cmd_key *cmd = arg;
	struct ieee80211_key *k = &cmd->key;
	const uint8_t *macaddr;
	uint8_t keybuf[16], algo;
	int i, entry;

	/* Map net80211 cipher to HW crypto algorithm. */
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
		algo = R92C_CAM_ALGO_WEP40;
		break;
	case IEEE80211_CIPHER_WEP104:
		algo = R92C_CAM_ALGO_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		algo = R92C_CAM_ALGO_TKIP;
		break;
	case IEEE80211_CIPHER_CCMP:
		algo = R92C_CAM_ALGO_AES;
		break;
	default:
		return;
	}
	if (k->k_flags & IEEE80211_KEY_GROUP) {
		macaddr = etherzeroaddr;
		entry = k->k_id;
	} else {
		macaddr = ic->ic_bss->ni_macaddr;
		entry = 4;
	}
	/* Write key. */
	memset(keybuf, 0, sizeof(keybuf));
	memcpy(keybuf, k->k_key, MIN(k->k_len, sizeof(keybuf)));
	for (i = 0; i < 4; i++) {
		urtwn_cam_write(sc, R92C_CAM_KEY(entry, i),
		    LE_READ_4(&keybuf[i * 4]));
	}
	/* Write CTL0 last since that will validate the CAM entry. */
	urtwn_cam_write(sc, R92C_CAM_CTL1(entry),
	    LE_READ_4(&macaddr[2]));
	urtwn_cam_write(sc, R92C_CAM_CTL0(entry),
	    SM(R92C_CAM_ALGO, algo) |
	    SM(R92C_CAM_KEYID, k->k_id) |
	    SM(R92C_CAM_MACLO, LE_READ_2(&macaddr[0])) |
	    R92C_CAM_VALID);
}

void
urtwn_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct urtwn_softc *sc = ic->ic_softc;
	struct urtwn_cmd_key cmd;

	if (!(ic->ic_if.if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */

	/* Do it in a process context. */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	urtwn_do_async(sc, urtwn_delete_key_cb, &cmd, sizeof(cmd));
}

void
urtwn_delete_key_cb(struct urtwn_softc *sc, void *arg)
{
	struct urtwn_cmd_key *cmd = arg;
	struct ieee80211_key *k = &cmd->key;
	int i, entry;

	if (k->k_flags & IEEE80211_KEY_GROUP)
		entry = k->k_id;
	else
		entry = 4;
	urtwn_cam_write(sc, R92C_CAM_CTL0(entry), 0);
	urtwn_cam_write(sc, R92C_CAM_CTL1(entry), 0);
	/* Clear key. */
	for (i = 0; i < 4; i++)
		urtwn_cam_write(sc, R92C_CAM_KEY(entry, i), 0);
}

void
urtwn_update_avgrssi(struct urtwn_softc *sc, int rate, int8_t rssi)
{
	int pwdb;

	/* Convert antenna signal to percentage. */
	if (rssi <= -100 || rssi >= 20)
		pwdb = 0;
	else if (rssi >= 0)
		pwdb = 100;
	else
		pwdb = 100 + rssi;
	if (rate <= 3) {
		/* CCK gain is smaller than OFDM/MCS gain. */
		pwdb += 6;
		if (pwdb > 100)
			pwdb = 100;
		if (pwdb <= 14)
			pwdb -= 4;
		else if (pwdb <= 26)
			pwdb -= 8;
		else if (pwdb <= 34)
			pwdb -= 6;
		else if (pwdb <= 42)
			pwdb -= 2;
	}
	if (sc->avg_pwdb == -1)	/* Init. */
		sc->avg_pwdb = pwdb;
	else if (sc->avg_pwdb < pwdb)
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20) + 1;
	else
		sc->avg_pwdb = ((sc->avg_pwdb * 19 + pwdb) / 20);
	DPRINTFN(4, ("PWDB=%d EMA=%d\n", pwdb, sc->avg_pwdb));
}

int8_t
urtwn_get_rssi(struct urtwn_softc *sc, int rate, void *physt)
{
	static const int8_t cckoff[] = { 16, -12, -26, -46 };
	struct r92c_rx_phystat *phy;
	struct r92c_rx_cck *cck;
	uint8_t rpt;
	int8_t rssi;

	if (rate <= 3) {
		cck = (struct r92c_rx_cck *)physt;
		if (sc->sc_flags & URTWN_FLAG_CCK_HIPWR) {
			rpt = (cck->agc_rpt >> 5) & 0x3;
			rssi = (cck->agc_rpt & 0x1f) << 1;
		} else {
			rpt = (cck->agc_rpt >> 6) & 0x3;
			rssi = cck->agc_rpt & 0x3e;
		}
		rssi = cckoff[rpt] - rssi;
	} else {	/* OFDM/HT. */
		phy = (struct r92c_rx_phystat *)physt;
		rssi = ((letoh32(phy->phydw1) >> 1) & 0x7f) - 110;
	}
	return (rssi);
}

void
urtwn_rx_frame(struct urtwn_softc *sc, uint8_t *buf, int pktlen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_rxinfo rxi;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct r92c_rx_stat *stat;
	uint32_t rxdw0, rxdw3;
	struct mbuf *m;
	uint8_t rate;
	int8_t rssi = 0;
	int s, infosz;

	stat = (struct r92c_rx_stat *)buf;
	rxdw0 = letoh32(stat->rxdw0);
	rxdw3 = letoh32(stat->rxdw3);

	if (__predict_false(rxdw0 & (R92C_RXDW0_CRCERR | R92C_RXDW0_ICVERR))) {
		/*
		 * This should not happen since we setup our Rx filter
		 * to not receive these frames.
		 */
		ifp->if_ierrors++;
		return;
	}
	if (__predict_false(pktlen < sizeof(*wh) || pktlen > MCLBYTES)) {
		ifp->if_ierrors++;
		return;
	}

	rate = MS(rxdw3, R92C_RXDW3_RATE);
	infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;

	/* Get RSSI from PHY status descriptor if present. */
	if (infosz != 0 && (rxdw0 & R92C_RXDW0_PHYST)) {
		rssi = urtwn_get_rssi(sc, rate, &stat[1]);
		/* Update our average RSSI. */
		urtwn_update_avgrssi(sc, rate, rssi);
	}

	DPRINTFN(5, ("Rx frame len=%d rate=%d infosz=%d rssi=%d\n",
	    pktlen, rate, infosz, rssi));

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (__predict_false(m == NULL)) {
		ifp->if_ierrors++;
		return;
	}
	if (pktlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (__predict_false(!(m->m_flags & M_EXT))) {
			ifp->if_ierrors++;
			m_freem(m);
			return;
		}
	}
	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	wh = (struct ieee80211_frame *)((uint8_t *)&stat[1] + infosz);
	memcpy(mtod(m, uint8_t *), wh, pktlen);
	m->m_pkthdr.len = m->m_len = pktlen;

	s = splnet();
#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct urtwn_rx_radiotap_header *tap = &sc->sc_rxtap;
		struct mbuf mb;

		tap->wr_flags = 0;
		/* Map HW rate index to 802.11 rate. */
		tap->wr_flags = 2;
		if (!(rxdw3 & R92C_RXDW3_HT)) {
			switch (rate) {
			/* CCK. */
			case  0: tap->wr_rate =   2; break;
			case  1: tap->wr_rate =   4; break;
			case  2: tap->wr_rate =  11; break;
			case  3: tap->wr_rate =  22; break;
			/* OFDM. */
			case  4: tap->wr_rate =  12; break;
			case  5: tap->wr_rate =  18; break;
			case  6: tap->wr_rate =  24; break;
			case  7: tap->wr_rate =  36; break;
			case  8: tap->wr_rate =  48; break;
			case  9: tap->wr_rate =  72; break;
			case 10: tap->wr_rate =  96; break;
			case 11: tap->wr_rate = 108; break;
			}
		} else if (rate >= 12) {	/* MCS0~15. */
			/* Bit 7 set means HT MCS instead of rate. */
			tap->wr_rate = 0x80 | (rate - 12);
		}
		tap->wr_dbm_antsignal = rssi;
		tap->wr_chan_freq = htole16(ic->ic_ibss_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_ibss_chan->ic_flags);

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif

	ni = ieee80211_find_rxnode(ic, wh);
	rxi.rxi_flags = 0;
	rxi.rxi_rssi = rssi;
	rxi.rxi_tstamp = 0;	/* Unused. */
	ieee80211_input(ifp, m, ni, &rxi);
	/* Node is no longer needed. */
	ieee80211_release_node(ic, ni);
	splx(s);
}

void
urtwn_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct urtwn_rx_data *data = priv;
	struct urtwn_softc *sc = data->sc;
	struct r92c_rx_stat *stat;
	uint32_t rxdw0;
	uint8_t *buf;
	int len, totlen, pktlen, infosz, npkts;

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("RX status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->rx_pipe);
		if (status != USBD_CANCELLED)
			goto resubmit;
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (__predict_false(len < sizeof(*stat))) {
		DPRINTF(("xfer too short %d\n", len));
		goto resubmit;
	}
	buf = data->buf;

	/* Get the number of encapsulated frames. */
	stat = (struct r92c_rx_stat *)buf;
	npkts = MS(letoh32(stat->rxdw2), R92C_RXDW2_PKTCNT);
	DPRINTFN(6, ("Rx %d frames in one chunk\n", npkts));

	/* Process all of them. */
	while (npkts-- > 0) {
		if (__predict_false(len < sizeof(*stat)))
			break;
		stat = (struct r92c_rx_stat *)buf;
		rxdw0 = letoh32(stat->rxdw0);

		pktlen = MS(rxdw0, R92C_RXDW0_PKTLEN);
		if (__predict_false(pktlen == 0))
			break;

		infosz = MS(rxdw0, R92C_RXDW0_INFOSZ) * 8;

		/* Make sure everything fits in xfer. */
		totlen = sizeof(*stat) + infosz + pktlen;
		if (__predict_false(totlen > len))
			break;

		/* Process 802.11 frame. */
		urtwn_rx_frame(sc, buf, pktlen);

		/* Next chunk is 128-byte aligned. */
		totlen = (totlen + 127) & ~127;
		buf += totlen;
		len -= totlen;
	}

 resubmit:
	/* Setup a new transfer. */
	usbd_setup_xfer(xfer, sc->rx_pipe, data, data->buf, URTWN_RXBUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT, urtwn_rxeof);
	(void)usbd_transfer(xfer);
}

void
urtwn_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct urtwn_tx_data *data = priv;
	struct urtwn_softc *sc = data->sc;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splnet();
	/* Put this Tx buffer back to our free list. */
	TAILQ_INSERT_TAIL(&sc->tx_free_list, data, next);

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("TX status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(data->pipe);
		ifp->if_oerrors++;
		splx(s);
		return;
	}
	sc->sc_tx_timer = 0;
	ifp->if_opackets++;

	/* We just released a Tx buffer, notify Tx. */
	if (ifp->if_flags & IFF_OACTIVE) {
		ifp->if_flags &= ~IFF_OACTIVE;
		urtwn_start(ifp);
	}
	splx(s);
}

int
urtwn_tx(struct urtwn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct urtwn_tx_data *data;
	struct r92c_tx_desc *txd;
	usbd_pipe_handle pipe;
	uint16_t qos, sum;
	uint8_t raid, type, tid, qid;
	int i, hasqos, xferlen, error;

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return (ENOBUFS);
		wh = mtod(m, struct ieee80211_frame *);
	}

	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = ieee80211_up_to_ac(ic, tid);
	} else if (type != IEEE80211_FC0_TYPE_DATA) {
		/* Use AC VO for management frames. */
		qid = EDCA_AC_VO;
	} else
		qid = EDCA_AC_BE;

	/* Get the USB pipe to use for this AC. */
	pipe = sc->tx_pipe[sc->ac2idx[qid]];

	/* Grab a Tx buffer from our free list. */
	data = TAILQ_FIRST(&sc->tx_free_list);
	TAILQ_REMOVE(&sc->tx_free_list, data, next);

	/* Fill Tx descriptor. */
	txd = (struct r92c_tx_desc *)data->buf;
	memset(txd, 0, sizeof(*txd));

	txd->txdw0 |= htole32(
	    SM(R92C_TXDW0_PKTLEN, m->m_pkthdr.len) |
	    SM(R92C_TXDW0_OFFSET, sizeof(*txd)) |
	    R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG);
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		txd->txdw0 |= htole32(R92C_TXDW0_BMCAST);

#ifdef notyet
	if (k != NULL) {
		switch (k->k_cipher) {
		case IEEE80211_CIPHER_WEP40:
		case IEEE80211_CIPHER_WEP104:
		case IEEE80211_CIPHER_TKIP:
			cipher = R92C_TXDW1_CIPHER_RC4;
			break;
		case IEEE80211_CIPHER_CCMP:
			cipher = R92C_TXDW1_CIPHER_AES;
			break;
		default:
			cipher = R92C_TXDW1_CIPHER_NONE;
		}
		txd->txdw1 |= htole32(SM(R92C_TXDW1_CIPHER, cipher));
	}
#endif
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type == IEEE80211_FC0_TYPE_DATA) {
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			raid = R92C_RAID_11B;
		else
			raid = R92C_RAID_11BG;
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, URTWN_MACID_BSS) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BE) |
		    SM(R92C_TXDW1_RAID, raid) |
		    R92C_TXDW1_AGGBK);

		if (ic->ic_flags & IEEE80211_F_USEPROT) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
				txd->txdw4 |= htole32(R92C_TXDW4_CTS2SELF |
				    R92C_TXDW4_HWRTSEN);
			} else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
				txd->txdw4 |= htole32(R92C_TXDW4_RTSEN |
				    R92C_TXDW4_HWRTSEN);
			}
		}
		/* Send RTS at OFDM24. */
		txd->txdw4 |= htole32(SM(R92C_TXDW4_RTSRATE, 8));
		txd->txdw5 |= htole32(0x0001ff00);
		/* Send data at OFDM54. */
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 11));

	} else {
		txd->txdw1 |= htole32(
		    SM(R92C_TXDW1_MACID, 0) |
		    SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_MGNT) |
		    SM(R92C_TXDW1_RAID, R92C_RAID_11B));

		/* Force CCK1. */
		txd->txdw4 |= htole32(R92C_TXDW4_DRVRATE);
		txd->txdw5 |= htole32(SM(R92C_TXDW5_DATARATE, 0));
	}
	/* Set sequence number (already little endian). */
	txd->txdseq |= *(uint16_t *)wh->i_seq;

	if (!hasqos) {
		/* Use HW sequence numbering for non-QoS frames. */
		txd->txdw4  |= htole32(R92C_TXDW4_HWSEQ);
		txd->txdseq |= htole16(0x8000);		/* WTF? */
	} else
		txd->txdw4 |= htole32(R92C_TXDW4_QOS);

	/* Compute Tx descriptor checksum. */
	sum = 0;
	for (i = 0; i < sizeof(*txd) / 2; i++)
		sum ^= ((uint16_t *)txd)[i];
	txd->txdsum = sum;	/* NB: already little endian. */

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct urtwn_tx_radiotap_header *tap = &sc->sc_txtap;
		struct mbuf mb;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	xferlen = sizeof(*txd) + m->m_pkthdr.len;
	m_copydata(m, 0, m->m_pkthdr.len, (caddr_t)&txd[1]);
	m_freem(m);

	data->pipe = pipe;
	usbd_setup_xfer(data->xfer, pipe, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, URTWN_TX_TIMEOUT,
	    urtwn_txeof);
	error = usbd_transfer(data->xfer);
	if (__predict_false(error != USBD_IN_PROGRESS && error != 0)) {
		/* Put this Tx buffer back to our free list. */
		TAILQ_INSERT_TAIL(&sc->tx_free_list, data, next);
		return (error);
	}
	ieee80211_release_node(ic, ni);
	return (0);
}

void
urtwn_start(struct ifnet *ifp)
{
	struct urtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (TAILQ_EMPTY(&sc->tx_free_list)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL)
			continue;
sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		if (urtwn_tx(sc, m, ni) != 0) {
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
urtwn_watchdog(struct ifnet *ifp)
{
	struct urtwn_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			/* urtwn_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(ifp);
}

int
urtwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct urtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	if (usbd_is_dying(sc->sc_udev))
		return ENXIO;

	usbd_ref_incr(sc->sc_udev);

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
			if (!(ifp->if_flags & IFF_RUNNING))
				urtwn_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				urtwn_stop(ifp);
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
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING))
				urtwn_set_chan(sc, ic->ic_ibss_chan, NULL);
			error = 0;
		}
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			urtwn_stop(ifp);
			urtwn_init(ifp);
		}
		error = 0;
	}
	splx(s);

	usbd_ref_decr(sc->sc_udev);

	return (error);
}

int
urtwn_power_on(struct urtwn_softc *sc)
{
	uint32_t reg;
	int ntries;

	/* Wait for autoload done bit. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_1(sc, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for chip autoload\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Unlock ISO/CLK/Power control register. */
	urtwn_write_1(sc, R92C_RSV_CTRL, 0);
	/* Move SPS into PWM mode. */
	urtwn_write_1(sc, R92C_SPS0_CTRL, 0x2b);
	DELAY(100);

	reg = urtwn_read_1(sc, R92C_LDOV12D_CTRL);
	if (!(reg & R92C_LDOV12D_CTRL_LDV12_EN)) {
		urtwn_write_1(sc, R92C_LDOV12D_CTRL,
		    reg | R92C_LDOV12D_CTRL_LDV12_EN);
		DELAY(100);
		urtwn_write_1(sc, R92C_SYS_ISO_CTRL,
		    urtwn_read_1(sc, R92C_SYS_ISO_CTRL) &
		    ~R92C_SYS_ISO_CTRL_MD2PP);
	}

	/* Auto enable WLAN. */
	urtwn_write_2(sc, R92C_APS_FSMCO,
	    urtwn_read_2(sc, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_2(sc, R92C_APS_FSMCO) &
		    R92C_APS_FSMCO_APFM_ONMAC)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for MAC auto ON\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Enable radio, GPIO and LED functions. */
	urtwn_write_2(sc, R92C_APS_FSMCO,
	    R92C_APS_FSMCO_AFSM_HSUS |
	    R92C_APS_FSMCO_PDN_EN |
	    R92C_APS_FSMCO_PFM_ALDN);
	/* Release RF digital isolation. */
	urtwn_write_2(sc, R92C_SYS_ISO_CTRL,
	    urtwn_read_2(sc, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);

	/* Initialize MAC. */
	urtwn_write_1(sc, R92C_APSD_CTRL,
	    urtwn_read_1(sc, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
	for (ntries = 0; ntries < 200; ntries++) {
		if (!(urtwn_read_1(sc, R92C_APSD_CTRL) &
		    R92C_APSD_CTRL_OFF_STATUS))
			break;
		DELAY(5);
	}
	if (ntries == 200) {
		printf("%s: timeout waiting for MAC initialization\n",
		    sc->sc_dev.dv_xname);
		return (ETIMEDOUT);
	}

	/* Enable MAC DMA/WMAC/SCHEDULE/SEC blocks. */
	reg = urtwn_read_2(sc, R92C_CR);
	reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN |
	    R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
	    R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN |
	    R92C_CR_ENSEC;
	urtwn_write_2(sc, R92C_CR, reg);

	urtwn_write_1(sc, 0xfe10, 0x19);
	return (0);
}

int
urtwn_llt_init(struct urtwn_softc *sc)
{
	int i, error;

	/* Reserve pages [0; R92C_TX_PAGE_COUNT]. */
	for (i = 0; i < R92C_TX_PAGE_COUNT; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* NB: 0xff indicates end-of-list. */
	if ((error = urtwn_llt_write(sc, i, 0xff)) != 0)
		return (error);
	/*
	 * Use pages [R92C_TX_PAGE_COUNT + 1; R92C_TXPKTBUF_COUNT - 1]
	 * as ring buffer.
	 */
	for (++i; i < R92C_TXPKTBUF_COUNT - 1; i++) {
		if ((error = urtwn_llt_write(sc, i, i + 1)) != 0)
			return (error);
	}
	/* Make the last page point to the beginning of the ring buffer. */
	error = urtwn_llt_write(sc, i, R92C_TX_PAGE_COUNT + 1);
	return (error);
}

void
urtwn_fw_reset(struct urtwn_softc *sc)
{
	uint16_t reg;
	int ntries;

	/* Tell 8051 to reset itself. */
	urtwn_write_1(sc, R92C_HMETFR + 3, 0x20);

	/* Wait until 8051 resets by itself. */
	for (ntries = 0; ntries < 100; ntries++) {
		reg = urtwn_read_2(sc, R92C_SYS_FUNC_EN);
		if (!(reg & R92C_SYS_FUNC_EN_CPUEN))
			return;
		DELAY(50);
	}
	/* Force 8051 reset. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
}

int
urtwn_fw_loadpage(struct urtwn_softc *sc, int page, uint8_t *buf, int len)
{
	uint32_t reg;
	int off, mlen, error = 0;

	reg = urtwn_read_4(sc, R92C_MCUFWDL);
	reg = RW(reg, R92C_MCUFWDL_PAGE, page);
	urtwn_write_4(sc, R92C_MCUFWDL, reg);

	off = R92C_FW_START_ADDR;
	while (len > 0) {
		if (len > 196)
			mlen = 196;
		else if (len > 4)
			mlen = 4;
		else
			mlen = 1;
		error = urtwn_write_region_1(sc, off, buf, mlen);
		if (error != 0)
			break;
		off += mlen;
		buf += mlen;
		len -= mlen;
	}
	return (error);
}

int
urtwn_load_firmware(struct urtwn_softc *sc)
{
	const struct r92c_fw_hdr *hdr;
	const char *name;
	u_char *fw, *ptr;
	size_t len;
	uint32_t reg;
	int mlen, ntries, page, error;

	/* Read firmware image from the filesystem. */
	if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
	    URTWN_CHIP_UMC_A_CUT)
		name = "urtwn-rtl8192cfwU";
	else
		name = "urtwn-rtl8192cfwT";
	if ((error = loadfirmware(name, &fw, &len)) != 0) {
		printf("%s: failed loadfirmware of file %s (error %d)\n",
		    sc->sc_dev.dv_xname, name, error);
		return (error);
	}
	if (len < sizeof(*hdr)) {
		printf("%s: firmware too short\n", sc->sc_dev.dv_xname);
		error = EINVAL;
		goto fail;
	}
	ptr = fw;
	hdr = (const struct r92c_fw_hdr *)ptr;
	/* Check if there is a valid FW header and skip it. */
	if ((letoh16(hdr->signature) >> 4) == 0x88c ||
	    (letoh16(hdr->signature) >> 4) == 0x92c) {
		DPRINTF(("FW V%d.%d %02d-%02d %02d:%02d\n",
		    letoh16(hdr->version), letoh16(hdr->subversion),
		    hdr->month, hdr->date, hdr->hour, hdr->minute));
		ptr += sizeof(*hdr);
		len -= sizeof(*hdr);
	}

	if (urtwn_read_1(sc, R92C_MCUFWDL) & 0x80) {
		urtwn_fw_reset(sc);
		urtwn_write_1(sc, R92C_MCUFWDL, 0);
	}
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_CPUEN);
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
	urtwn_write_1(sc, R92C_MCUFWDL + 2,
	    urtwn_read_1(sc, R92C_MCUFWDL + 2) & ~0x08);

	for (page = 0; len > 0; page++) {
		mlen = MIN(len, R92C_FW_PAGE_SIZE);
		error = urtwn_fw_loadpage(sc, page, ptr, mlen);
		if (error != 0) {
			printf("%s: could not load firmware page %d\n",
			    sc->sc_dev.dv_xname, page);
			goto fail;
		}
		ptr += mlen;
		len -= mlen;
	}
	urtwn_write_1(sc, R92C_MCUFWDL,
	    urtwn_read_1(sc, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
	urtwn_write_1(sc, R92C_MCUFWDL + 1, 0);

	/* Wait for checksum report. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for checksum report\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail;
	}

	reg = urtwn_read_4(sc, R92C_MCUFWDL);
	reg = (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY;
	urtwn_write_4(sc, R92C_MCUFWDL, reg);
	/* Wait for firmware readiness. */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (urtwn_read_4(sc, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY)
			break;
		DELAY(5);
	}
	if (ntries == 1000) {
		printf("%s: timeout waiting for firmware readiness\n",
		    sc->sc_dev.dv_xname);
		error = ETIMEDOUT;
		goto fail;
	}
 fail:
	free(fw, M_DEVBUF);
	return (error);
}

int
urtwn_dma_init(struct urtwn_softc *sc)
{
	int hashq, hasnq, haslq, nqueues, nqpages, nrempages;
	uint32_t reg;
	int error;

	/* Initialize LLT table. */
	error = urtwn_llt_init(sc);
	if (error != 0)
		return (error);

	/* Get Tx queues to USB endpoints mapping. */
	hashq = hasnq = haslq = 0;
	reg = urtwn_read_2(sc, R92C_USB_EP + 1);
	DPRINTFN(2, ("USB endpoints mapping 0x%x\n", reg));
	if (MS(reg, R92C_USB_EP_HQ) != 0)
		hashq = 1;
	if (MS(reg, R92C_USB_EP_NQ) != 0)
		hasnq = 1;
	if (MS(reg, R92C_USB_EP_LQ) != 0)
		haslq = 1;
	nqueues = hashq + hasnq + haslq;
	if (nqueues == 0)
		return (EIO);
	/* Get the number of pages for each queue. */
	nqpages = (R92C_TX_PAGE_COUNT - R92C_PUBQ_NPAGES) / nqueues;
	/* The remaining pages are assigned to the high priority queue. */
	nrempages = (R92C_TX_PAGE_COUNT - R92C_PUBQ_NPAGES) % nqueues;

	/* Set number of pages for normal priority queue. */
	urtwn_write_1(sc, R92C_RQPN_NPQ, hasnq ? nqpages : 0);
	urtwn_write_4(sc, R92C_RQPN,
	    /* Set number of pages for public queue. */
	    SM(R92C_RQPN_PUBQ, R92C_PUBQ_NPAGES) |
	    /* Set number of pages for high priority queue. */
	    SM(R92C_RQPN_HPQ, hashq ? nqpages + nrempages : 0) |
	    /* Set number of pages for low priority queue. */
	    SM(R92C_RQPN_LPQ, haslq ? nqpages : 0) |
	    /* Load values. */
	    R92C_RQPN_LD);

	urtwn_write_1(sc, R92C_TXPKTBUF_BCNQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TXPKTBUF_MGQ_BDNY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TXPKTBUF_WMAC_LBK_BF_HD, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TRXFF_BNDY, R92C_TX_PAGE_BOUNDARY);
	urtwn_write_1(sc, R92C_TDECTRL + 1, R92C_TX_PAGE_BOUNDARY);

	/* Set queue to USB pipe mapping. */
	reg = urtwn_read_2(sc, R92C_TRXDMA_CTRL);
	reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
	if (nqueues == 1) {
		if (hashq)
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ;
		else if (hasnq)
			reg |= R92C_TRXDMA_CTRL_QMAP_NQ;
		else
			reg |= R92C_TRXDMA_CTRL_QMAP_LQ;
	} else if (nqueues == 2) {
		/* All 2-endpoints configs have a high priority queue. */
		if (!hashq)
			return (EIO);
		if (hasnq)
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ_NQ;
		else
			reg |= R92C_TRXDMA_CTRL_QMAP_HQ_LQ;
	} else
		reg |= R92C_TRXDMA_CTRL_QMAP_3EP;
	urtwn_write_2(sc, R92C_TRXDMA_CTRL, reg);

	/* Set Tx/Rx transfer page boundary. */
	urtwn_write_2(sc, R92C_TRXFF_BNDY + 2, 0x27ff);

	/* Set Tx/Rx transfer page size. */
	urtwn_write_1(sc, R92C_PBP,
	    SM(R92C_PBP_PSRX, R92C_PBP_128) |
	    SM(R92C_PBP_PSTX, R92C_PBP_128));
	return (0);
}

void
urtwn_mac_init(struct urtwn_softc *sc)
{
	int i;

	/* Write MAC initialization values. */
	for (i = 0; i < nitems(rtl8192cu_mac); i++)
		urtwn_write_1(sc, rtl8192cu_mac[i].reg, rtl8192cu_mac[i].val);
}

void
urtwn_bb_init(struct urtwn_softc *sc)
{
	const struct urtwn_bb_prog *prog;
	uint32_t reg;
	int i;

	/* Enable BB and RF. */
	urtwn_write_2(sc, R92C_SYS_FUNC_EN,
	    urtwn_read_2(sc, R92C_SYS_FUNC_EN) |
	    R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
	    R92C_SYS_FUNC_EN_DIO_RF);

	urtwn_write_2(sc, R92C_AFE_PLL_CTRL, 0xdb83);

	urtwn_write_1(sc, R92C_RF_CTRL,
	    R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
	urtwn_write_1(sc, R92C_SYS_FUNC_EN,
	    R92C_SYS_FUNC_EN_USBA | R92C_SYS_FUNC_EN_USBD |
	    R92C_SYS_FUNC_EN_BB_GLB_RST | R92C_SYS_FUNC_EN_BBRSTB);

	urtwn_write_1(sc, R92C_LDOHCI12_CTRL, 0x0f);
	urtwn_write_1(sc, 0x15, 0xe9);
	urtwn_write_1(sc, R92C_AFE_XTAL_CTRL + 1, 0x80);

	/* Select BB programming based on board type. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = &rtl8188ce_bb_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = &rtl8188ru_bb_prog;
		else
			prog = &rtl8188cu_bb_prog;
	} else {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = &rtl8192ce_bb_prog;
		else
			prog = &rtl8192cu_bb_prog;
	}
	/* Write BB initialization values. */
	for (i = 0; i < prog->count; i++) {
		urtwn_bb_write(sc, prog->regs[i], prog->vals[i]);
		DELAY(1);
	}

	if (sc->chip & URTWN_CHIP_92C_1T2R) {
		/* 8192C 1T only configuration. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_TXINFO);
		reg = (reg & ~0x00000003) | 0x2;
		urtwn_bb_write(sc, R92C_FPGA0_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_FPGA1_TXINFO);
		reg = (reg & ~0x00300033) | 0x00200022;
		urtwn_bb_write(sc, R92C_FPGA1_TXINFO, reg);

		reg = urtwn_bb_read(sc, R92C_CCK0_AFESETTING);
		reg = (reg & ~0xff000000) | 0x45 << 24;
		urtwn_bb_write(sc, R92C_CCK0_AFESETTING, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_TRXPATHENA);
		reg = (reg & ~0x000000ff) | 0x23;
		urtwn_bb_write(sc, R92C_OFDM0_TRXPATHENA, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM0_AGCPARAM1);
		reg = (reg & ~0x00000030) | 1 << 4;
		urtwn_bb_write(sc, R92C_OFDM0_AGCPARAM1, reg);

		reg = urtwn_bb_read(sc, 0xe74);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe74, reg);
		reg = urtwn_bb_read(sc, 0xe78);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe78, reg);
		reg = urtwn_bb_read(sc, 0xe7c);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe7c, reg);
		reg = urtwn_bb_read(sc, 0xe80);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe80, reg);
		reg = urtwn_bb_read(sc, 0xe88);
		reg = (reg & ~0x0c000000) | 2 << 26;
		urtwn_bb_write(sc, 0xe88, reg);
	}

	/* Write AGC values. */
	for (i = 0; i < prog->agccount; i++) {
		urtwn_bb_write(sc, R92C_OFDM0_AGCRSSITABLE,
		    prog->agcvals[i]);
		DELAY(1);
	}

	if (urtwn_bb_read(sc, R92C_HSSI_PARAM2(0)) &
	    R92C_HSSI_PARAM2_CCK_HIPWR)
		sc->sc_flags |= URTWN_FLAG_CCK_HIPWR;
}

void
urtwn_rf_init(struct urtwn_softc *sc)
{
	const struct urtwn_rf_prog *prog;
	uint32_t reg, type;
	int i, j, idx, off;

	/* Select RF programming based on board type. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_MINICARD)
			prog = rtl8188ce_rf_prog;
		else if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			prog = rtl8188ru_rf_prog;
		else
			prog = rtl8188cu_rf_prog;
	} else
		prog = rtl8192ce_rf_prog;

	for (i = 0; i < sc->nrxchains; i++) {
		/* Save RF_ENV control type. */
		idx = i / 2;
		off = (i % 2) * 16;
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		type = (reg >> off) & 0x10;

		/* Set RF_ENV enable. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x100000;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set RF_ENV output high. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACEOE(i));
		reg |= 0x10;
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACEOE(i), reg);
		DELAY(1);
		/* Set address and data lengths of RF registers. */
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_ADDR_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);
		reg = urtwn_bb_read(sc, R92C_HSSI_PARAM2(i));
		reg &= ~R92C_HSSI_PARAM2_DATA_LENGTH;
		urtwn_bb_write(sc, R92C_HSSI_PARAM2(i), reg);
		DELAY(1);

		/* Write RF initialization values for this chain. */
		for (j = 0; j < prog[i].count; j++) {
			if (prog[i].regs[j] >= 0xf9 &&
			    prog[i].regs[j] <= 0xfe) {
				/*
				 * These are fake RF registers offsets that
				 * indicate a delay is required.
				 */
				usbd_delay_ms(sc->sc_udev, 50);
				continue;
			}
			urtwn_rf_write(sc, i, prog[i].regs[j],
			    prog[i].vals[j]);
			DELAY(1);
		}

		/* Restore RF_ENV control type. */
		reg = urtwn_bb_read(sc, R92C_FPGA0_RFIFACESW(idx));
		reg &= ~(0x10 << off) | (type << off);
		urtwn_bb_write(sc, R92C_FPGA0_RFIFACESW(idx), reg);

		/* Cache RF register CHNLBW. */
		sc->rf_chnlbw[i] = urtwn_rf_read(sc, i, R92C_RF_CHNLBW);
	}

	if ((sc->chip & (URTWN_CHIP_UMC_A_CUT | URTWN_CHIP_92C)) ==
	    URTWN_CHIP_UMC_A_CUT) {
		urtwn_rf_write(sc, 0, R92C_RF_RX_G1, 0x30255);
		urtwn_rf_write(sc, 0, R92C_RF_RX_G2, 0x50a00);
	}
}

void
urtwn_cam_init(struct urtwn_softc *sc)
{
	/* Invalidate all CAM entries. */
	urtwn_write_4(sc, R92C_CAMCMD,
	    R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

void
urtwn_pa_bias_init(struct urtwn_softc *sc)
{
	uint8_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		if (sc->pa_setting & (1 << i))
			continue;
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x0f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x4f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0x8f406);
		urtwn_rf_write(sc, i, R92C_RF_IPA, 0xcf406);
	}
	if (!(sc->pa_setting & 0x10)) {
		reg = urtwn_read_1(sc, 0x16);
		reg = (reg & ~0xf0) | 0x90;
		urtwn_write_1(sc, 0x16, reg);
	}
}

void
urtwn_rxfilter_init(struct urtwn_softc *sc)
{
	/* Initialize Rx filter. */
	/* TODO: use better filter for monitor mode. */
	urtwn_write_4(sc, R92C_RCR,
	    R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB |
	    R92C_RCR_APP_ICV | R92C_RCR_AMF | R92C_RCR_HTC_LOC_CTRL |
	    R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
	/* Accept all multicast frames. */
	urtwn_write_4(sc, R92C_MAR + 0, 0xffffffff);
	urtwn_write_4(sc, R92C_MAR + 4, 0xffffffff);
	/* Accept all management frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP0, 0xffff);
	/* Reject all control frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP1, 0x0000);
	/* Accept all data frames. */
	urtwn_write_2(sc, R92C_RXFLTMAP2, 0xffff);
}

void
urtwn_edca_init(struct urtwn_softc *sc)
{
	urtwn_write_2(sc, R92C_SPEC_SIFS, 0x100a);
	urtwn_write_2(sc, R92C_MAC_SPEC_SIFS, 0x100a);
	urtwn_write_2(sc, R92C_SIFS_CCK, 0x100a);
	urtwn_write_2(sc, R92C_SIFS_OFDM, 0x100a);
	urtwn_write_4(sc, R92C_EDCA_BE_PARAM, 0x005ea42b);
	urtwn_write_4(sc, R92C_EDCA_BK_PARAM, 0x0000a44f);
	urtwn_write_4(sc, R92C_EDCA_VI_PARAM, 0x005ea324);
	urtwn_write_4(sc, R92C_EDCA_VO_PARAM, 0x002fa226);
}

void
urtwn_write_txpower(struct urtwn_softc *sc, int chain,
    uint16_t power[URTWN_RIDX_COUNT])
{
	uint32_t reg;

	/* Write per-CCK rate Tx power. */
	if (chain == 0) {
		reg = urtwn_bb_read(sc, R92C_TXAGC_A_CCK1_MCS32);
		reg = RW(reg, R92C_TXAGC_A_CCK1,  power[0]);
		urtwn_bb_write(sc, R92C_TXAGC_A_CCK1_MCS32, reg);
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_A_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_A_CCK55, power[2]);
		reg = RW(reg, R92C_TXAGC_A_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	} else {
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK1_55_MCS32);
		reg = RW(reg, R92C_TXAGC_B_CCK1,  power[0]);
		reg = RW(reg, R92C_TXAGC_B_CCK2,  power[1]);
		reg = RW(reg, R92C_TXAGC_B_CCK55, power[2]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK1_55_MCS32, reg);
		reg = urtwn_bb_read(sc, R92C_TXAGC_B_CCK11_A_CCK2_11);
		reg = RW(reg, R92C_TXAGC_B_CCK11, power[3]);
		urtwn_bb_write(sc, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
	}
	/* Write per-OFDM rate Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_RATE18_06(chain),
	    SM(R92C_TXAGC_RATE06, power[ 4]) |
	    SM(R92C_TXAGC_RATE09, power[ 5]) |
	    SM(R92C_TXAGC_RATE12, power[ 6]) |
	    SM(R92C_TXAGC_RATE18, power[ 7]));
	urtwn_bb_write(sc, R92C_TXAGC_RATE54_24(chain),
	    SM(R92C_TXAGC_RATE24, power[ 8]) |
	    SM(R92C_TXAGC_RATE36, power[ 9]) |
	    SM(R92C_TXAGC_RATE48, power[10]) |
	    SM(R92C_TXAGC_RATE54, power[11]));
	/* Write per-MCS Tx power. */
	urtwn_bb_write(sc, R92C_TXAGC_MCS03_MCS00(chain),
	    SM(R92C_TXAGC_MCS00,  power[12]) |
	    SM(R92C_TXAGC_MCS01,  power[13]) |
	    SM(R92C_TXAGC_MCS02,  power[14]) |
	    SM(R92C_TXAGC_MCS03,  power[15]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS07_MCS04(chain),
	    SM(R92C_TXAGC_MCS04,  power[16]) |
	    SM(R92C_TXAGC_MCS05,  power[17]) |
	    SM(R92C_TXAGC_MCS06,  power[18]) |
	    SM(R92C_TXAGC_MCS07,  power[19]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS11_MCS08(chain),
	    SM(R92C_TXAGC_MCS08,  power[20]) |
	    SM(R92C_TXAGC_MCS08,  power[21]) |
	    SM(R92C_TXAGC_MCS10,  power[22]) |
	    SM(R92C_TXAGC_MCS11,  power[23]));
	urtwn_bb_write(sc, R92C_TXAGC_MCS15_MCS12(chain),
	    SM(R92C_TXAGC_MCS12,  power[24]) |
	    SM(R92C_TXAGC_MCS13,  power[25]) |
	    SM(R92C_TXAGC_MCS14,  power[26]) |
	    SM(R92C_TXAGC_MCS15,  power[27]));
}

void
urtwn_get_txpower(struct urtwn_softc *sc, int chain,
    struct ieee80211_channel *c, struct ieee80211_channel *extc,
    uint16_t power[URTWN_RIDX_COUNT])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct r92c_rom *rom = &sc->rom;
	uint16_t cckpow, ofdmpow, htpow, diff, max;
	const struct urtwn_txpwr *base;
	int ridx, chan, group;

	/* Determine channel group. */
	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */
	if (chan <= 3)
		group = 0;
	else if (chan <= 9)
		group = 1;
	else
		group = 2;

	/* Get original Tx power based on board type and RF chain. */
	if (!(sc->chip & URTWN_CHIP_92C)) {
		if (sc->board_type == R92C_BOARD_TYPE_HIGHPA)
			base = &rtl8188ru_txagc[chain];
		else
			base = &rtl8192cu_txagc[chain];
	} else
		base = &rtl8192cu_txagc[chain];

	memset(power, 0, URTWN_RIDX_COUNT * sizeof(power[0]));
	if (sc->regulatory == 0) {
		for (ridx = 0; ridx <= 3; ridx++)
			power[ridx] = base->pwr[0][ridx];
	}
	for (ridx = 4; ridx < URTWN_RIDX_COUNT; ridx++) {
		if (sc->regulatory == 3) {
			power[ridx] = base->pwr[0][ridx];
			/* Apply vendor limits. */
			if (extc != NULL)
				max = rom->ht40_max_pwr[group];
			else
				max = rom->ht20_max_pwr[group];
			max = (max >> (chain * 4)) & 0xf;
			if (power[ridx] > max)
				power[ridx] = max;
		} else if (sc->regulatory == 1) {
			if (extc == NULL)
				power[ridx] = base->pwr[group][ridx];
		} else if (sc->regulatory != 2)
			power[ridx] = base->pwr[0][ridx];
	}

	/* Compute per-CCK rate Tx power. */
	cckpow = rom->cck_tx_pwr[chain][group];
	for (ridx = 0; ridx <= 3; ridx++) {
		power[ridx] += cckpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	htpow = rom->ht40_1s_tx_pwr[chain][group];
	if (sc->ntxchains > 1) {
		/* Apply reduction for 2 spatial streams. */
		diff = rom->ht40_2s_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow = (htpow > diff) ? htpow - diff : 0;
	}

	/* Compute per-OFDM rate Tx power. */
	diff = rom->ofdm_tx_pwr_diff[group];
	diff = (diff >> (chain * 4)) & 0xf;
	ofdmpow = htpow + diff;	/* HT->OFDM correction. */
	for (ridx = 4; ridx <= 11; ridx++) {
		power[ridx] += ofdmpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}

	/* Compute per-MCS Tx power. */
	if (extc == NULL) {
		diff = rom->ht20_tx_pwr_diff[group];
		diff = (diff >> (chain * 4)) & 0xf;
		htpow += diff;	/* HT40->HT20 correction. */
	}
	for (ridx = 12; ridx <= 27; ridx++) {
		power[ridx] += htpow;
		if (power[ridx] > R92C_MAX_TX_PWR)
			power[ridx] = R92C_MAX_TX_PWR;
	}
#ifdef URTWN_DEBUG
	if (urtwn_debug >= 4) {
		/* Dump per-rate Tx power values. */
		printf("Tx power for chain %d:\n", chain);
		for (ridx = 0; ridx < URTWN_RIDX_COUNT; ridx++)
			printf("Rate %d = %u\n", ridx, power[ridx]);
	}
#endif
}

void
urtwn_set_txpower(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint16_t power[URTWN_RIDX_COUNT];
	int i;

	for (i = 0; i < sc->ntxchains; i++) {
		/* Compute per-rate Tx power values. */
		urtwn_get_txpower(sc, i, c, extc, power);
		/* Write per-rate Tx power values to hardware. */
		urtwn_write_txpower(sc, i, power);
	}
}

void
urtwn_set_chan(struct urtwn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t reg;
	u_int chan;
	int i;

	chan = ieee80211_chan2ieee(ic, c);	/* XXX center freq! */

	/* Set Tx power for this new channel. */
	urtwn_set_txpower(sc, c, extc);

	for (i = 0; i < sc->nrxchains; i++) {
		urtwn_rf_write(sc, i, R92C_RF_CHNLBW,
		    RW(sc->rf_chnlbw[i], R92C_RF_CHNLBW_CHNL, chan));
	}
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		/* Is secondary channel below or above primary? */
		int prichlo = c->ic_freq < extc->ic_freq;

		urtwn_write_1(sc, R92C_BWOPMODE,
		    urtwn_read_1(sc, R92C_BWOPMODE) & ~R92C_BWOPMODE_20MHZ);

		reg = urtwn_read_1(sc, R92C_RRSR + 2);
		reg = (reg & ~0x6f) | (prichlo ? 1 : 2) << 5;
		urtwn_write_1(sc, R92C_RRSR + 2, reg);

		urtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA0_RFMOD) | R92C_RFMOD_40MHZ);
		urtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA1_RFMOD) | R92C_RFMOD_40MHZ);

		/* Set CCK side band. */
		reg = urtwn_bb_read(sc, R92C_CCK0_SYSTEM);
		reg = (reg & ~0x00000010) | (prichlo ? 0 : 1) << 4;
		urtwn_bb_write(sc, R92C_CCK0_SYSTEM, reg);

		reg = urtwn_bb_read(sc, R92C_OFDM1_LSTF);
		reg = (reg & ~0x00000c00) | (prichlo ? 1 : 2) << 10;
		urtwn_bb_write(sc, R92C_OFDM1_LSTF, reg);

		urtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    urtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) &
		    ~R92C_FPGA0_ANAPARAM2_CBW20);

		reg = urtwn_bb_read(sc, 0x818);
		reg = (reg & ~0x0c000000) | (prichlo ? 2 : 1) << 26;
		urtwn_bb_write(sc, 0x818, reg);

		/* Select 40MHz bandwidth. */
		urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | chan);
	} else
#endif
	{
		urtwn_write_1(sc, R92C_BWOPMODE,
		    urtwn_read_1(sc, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);

		urtwn_bb_write(sc, R92C_FPGA0_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
		urtwn_bb_write(sc, R92C_FPGA1_RFMOD,
		    urtwn_bb_read(sc, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);

		urtwn_bb_write(sc, R92C_FPGA0_ANAPARAM2,
		    urtwn_bb_read(sc, R92C_FPGA0_ANAPARAM2) |
		    R92C_FPGA0_ANAPARAM2_CBW20);

		/* Select 20MHz bandwidth. */
		urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
		    (sc->rf_chnlbw[0] & ~0xfff) | R92C_RF_CHNLBW_BW20 | chan);
	}
}

int
urtwn_iq_calib_chain(struct urtwn_softc *sc, int chain, uint16_t tx[2],
    uint16_t rx[2])
{
	uint32_t status;
	int offset = chain * 0x20;

	if (chain == 0) {	/* IQ calibration for chain 0. */
		/* IQ calibration settings for chain 0. */
		urtwn_bb_write(sc, 0xe30, 0x10008c1f);
		urtwn_bb_write(sc, 0xe34, 0x10008c1f);
		urtwn_bb_write(sc, 0xe38, 0x82140102);

		if (sc->ntxchains > 1) {
			urtwn_bb_write(sc, 0xe3c, 0x28160202);	/* 2T */
			/* IQ calibration settings for chain 1. */
			urtwn_bb_write(sc, 0xe50, 0x10008c22);
			urtwn_bb_write(sc, 0xe54, 0x10008c22);
			urtwn_bb_write(sc, 0xe58, 0x82140102);
			urtwn_bb_write(sc, 0xe5c, 0x28160202);
		} else
			urtwn_bb_write(sc, 0xe3c, 0x28160502);	/* 1T */

		/* LO calibration settings. */
		urtwn_bb_write(sc, 0xe4c, 0x001028d1);
		/* We're doing LO and IQ calibration in one shot. */
		urtwn_bb_write(sc, 0xe48, 0xf9000000);
		urtwn_bb_write(sc, 0xe48, 0xf8000000);

	} else {		/* IQ calibration for chain 1. */
		/* We're doing LO and IQ calibration in one shot. */
		urtwn_bb_write(sc, 0xe60, 0x00000002);
		urtwn_bb_write(sc, 0xe60, 0x00000000);
	}

	/* Give LO and IQ calibrations the time to complete. */
	usbd_delay_ms(sc->sc_udev, 1);

	/* Read IQ calibration status. */
	status = urtwn_bb_read(sc, 0xeac);

	if (status & (1 << (28 + chain * 3)))
		return (0);	/* Tx failed. */
	/* Read Tx IQ calibration results. */
	tx[0] = (urtwn_bb_read(sc, 0xe94 + offset) >> 16) & 0x3ff;
	tx[1] = (urtwn_bb_read(sc, 0xe9c + offset) >> 16) & 0x3ff;
	if (tx[0] == 0x142 || tx[1] == 0x042)
		return (0);	/* Tx failed. */

	if (status & (1 << (27 + chain * 3)))
		return (1);	/* Rx failed. */
	/* Read Rx IQ calibration results. */
	rx[0] = (urtwn_bb_read(sc, 0xea4 + offset) >> 16) & 0x3ff;
	rx[1] = (urtwn_bb_read(sc, 0xeac + offset) >> 16) & 0x3ff;
	if (rx[0] == 0x132 || rx[1] == 0x036)
		return (1);	/* Rx failed. */

	return (3);	/* Both Tx and Rx succeeded. */
}

void
urtwn_iq_calib(struct urtwn_softc *sc)
{
	/* TODO */
}

void
urtwn_lc_calib(struct urtwn_softc *sc)
{
	uint32_t rf_ac[2];
	uint8_t txmode;
	int i;

	txmode = urtwn_read_1(sc, R92C_OFDM1_LSTF + 3);
	if ((txmode & 0x70) != 0) {
		/* Disable all continuous Tx. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode & ~0x70);

		/* Set RF mode to standby mode. */
		for (i = 0; i < sc->nrxchains; i++) {
			rf_ac[i] = urtwn_rf_read(sc, i, R92C_RF_AC);
			urtwn_rf_write(sc, i, R92C_RF_AC,
			    RW(rf_ac[i], R92C_RF_AC_MODE,
				R92C_RF_AC_MODE_STANDBY));
		}
	} else {
		/* Block all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0xff);
	}
	/* Start calibration. */
	urtwn_rf_write(sc, 0, R92C_RF_CHNLBW,
	    urtwn_rf_read(sc, 0, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);

	/* Give calibration the time to complete. */
	usbd_delay_ms(sc->sc_udev, 100);

	/* Restore configuration. */
	if ((txmode & 0x70) != 0) {
		/* Restore Tx mode. */
		urtwn_write_1(sc, R92C_OFDM1_LSTF + 3, txmode);
		/* Restore RF mode. */
		for (i = 0; i < sc->nrxchains; i++)
			urtwn_rf_write(sc, i, R92C_RF_AC, rf_ac[i]);
	} else {
		/* Unblock all Tx queues. */
		urtwn_write_1(sc, R92C_TXPAUSE, 0x00);
	}
}

void
urtwn_temp_calib(struct urtwn_softc *sc)
{
	int temp;

	if (sc->thcal_state == 0) {
		/* Start measuring temperature. */
		urtwn_rf_write(sc, 0, R92C_RF_T_METER, 0x60);
		sc->thcal_state = 1;
		return;
	}
	sc->thcal_state = 0;

	/* Read measured temperature. */
	temp = urtwn_rf_read(sc, 0, R92C_RF_T_METER) & 0x1f;
	if (temp == 0)	/* Read failed, skip. */
		return;
	DPRINTFN(2, ("temperature=%d\n", temp));

	/*
	 * Redo LC calibration if temperature changed significantly since
	 * last calibration.
	 */
	if (sc->thcal_lctemp == 0) {
		/* First LC calibration is performed in urtwn_init(). */
		sc->thcal_lctemp = temp;
	} else if (abs(temp - sc->thcal_lctemp) > 1) {
		DPRINTF(("LC calib triggered by temp: %d -> %d\n",
		    sc->thcal_lctemp, temp));
		urtwn_lc_calib(sc);
		/* Record temperature of last LC calibration. */
		sc->thcal_lctemp = temp;
	}
}

int
urtwn_init(struct ifnet *ifp)
{
	struct urtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct urtwn_rx_data *data;
	uint32_t reg;
	int i, error;

	/* Init host async commands ring. */
	sc->cmdq.cur = sc->cmdq.next = sc->cmdq.queued = 0;
	/* Init firmware commands ring. */
	sc->fwcur = 0;

	/* Allocate Tx/Rx buffers. */
	error = urtwn_alloc_rx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Rx buffers\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}
	error = urtwn_alloc_tx_list(sc);
	if (error != 0) {
		printf("%s: could not allocate Tx buffers\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}
	/* Power on adapter. */
	error = urtwn_power_on(sc);
	if (error != 0)
		goto fail;

	/* Initialize DMA. */
	error = urtwn_dma_init(sc);
	if (error != 0)
		goto fail;

	/* Set info size in Rx descriptors (in 64-bit words). */
	urtwn_write_1(sc, R92C_RX_DRVINFO_SZ, 4);

	/* Init interrupts. */
	urtwn_write_4(sc, R92C_HISR, 0xffffffff);
	urtwn_write_4(sc, R92C_HIMR, 0xffffffff);

	/* Set MAC address. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	urtwn_write_region_1(sc, R92C_MACID, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN);

	/* Set initial network type. */
	reg = urtwn_read_4(sc, R92C_CR);
	reg = RW(reg, R92C_CR_NETTYPE, R92C_CR_NETTYPE_INFRA);
	urtwn_write_4(sc, R92C_CR, reg);

	urtwn_rxfilter_init(sc);

	reg = urtwn_read_4(sc, R92C_RRSR);
	reg = RW(reg, R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_CCK_ONLY_1M);
	urtwn_write_4(sc, R92C_RRSR, reg);

	/* Set short/long retry limits. */
	urtwn_write_2(sc, R92C_RL,
	    SM(R92C_RL_SRL, 0x30) | SM(R92C_RL_LRL, 0x30));

	/* Initialize EDCA parameters. */
	urtwn_edca_init(sc);

	/* Setup rate fallback. */
	urtwn_write_4(sc, R92C_DARFRC + 0, 0x00000000);
	urtwn_write_4(sc, R92C_DARFRC + 4, 0x10080404);
	urtwn_write_4(sc, R92C_RARFRC + 0, 0x04030201);
	urtwn_write_4(sc, R92C_RARFRC + 4, 0x08070605);

	urtwn_write_1(sc, R92C_FWHW_TXQ_CTRL,
	    urtwn_read_1(sc, R92C_FWHW_TXQ_CTRL) |
	    R92C_FWHW_TXQ_CTRL_AMPDU_RTY_NEW);
	/* Set ACK timeout. */
	urtwn_write_1(sc, R92C_ACKTO, 0x40);

	/* Setup USB aggregation. */
	reg = urtwn_read_4(sc, R92C_TDECTRL);
	reg = RW(reg, R92C_TDECTRL_BLK_DESC_NUM, 6);
	urtwn_write_4(sc, R92C_TDECTRL, reg);
	urtwn_write_1(sc, R92C_TRXDMA_CTRL,
	    urtwn_read_1(sc, R92C_TRXDMA_CTRL) |
	    R92C_TRXDMA_CTRL_RXDMA_AGG_EN);
	urtwn_write_1(sc, R92C_USB_SPECIAL_OPTION,
	    urtwn_read_1(sc, R92C_USB_SPECIAL_OPTION) |
	    R92C_USB_SPECIAL_OPTION_AGG_EN);
	urtwn_write_1(sc, R92C_RXDMA_AGG_PG_TH, 48);
	urtwn_write_1(sc, R92C_USB_DMA_AGG_TO, 4);
	urtwn_write_1(sc, R92C_USB_AGG_TH, 8);
	urtwn_write_1(sc, R92C_USB_AGG_TO, 6);

	/* Initialize beacon parameters. */
	urtwn_write_2(sc, R92C_TBTT_PROHIBIT, 0x6404);
	urtwn_write_1(sc, R92C_DRVERLYINT, 0x05);
	urtwn_write_1(sc, R92C_BCNDMATIM, 0x02);
	urtwn_write_2(sc, R92C_BCNTCFG, 0x660f);

	/* Setup AMPDU aggregation. */
	urtwn_write_4(sc, R92C_AGGLEN_LMT, 0x99997631);	/* MCS7~0 */
	urtwn_write_1(sc, R92C_AGGR_BREAK_TIME, 0x16);
	urtwn_write_2(sc, 0x4ca, 0x0708);

	urtwn_write_1(sc, R92C_BCN_MAX_ERR, 0xff);
	urtwn_write_1(sc, R92C_BCN_CTRL, R92C_BCN_CTRL_DIS_TSF_UDT0);

	/* Load 8051 microcode. */
	error = urtwn_load_firmware(sc);
	if (error != 0)
		goto fail;

	/* Initialize MAC/BB/RF blocks. */
	urtwn_mac_init(sc);
	urtwn_bb_init(sc);
	urtwn_rf_init(sc);

	/* Turn CCK and OFDM blocks on. */
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_CCK_EN;
	urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);
	reg = urtwn_bb_read(sc, R92C_FPGA0_RFMOD);
	reg |= R92C_RFMOD_OFDM_EN;
	urtwn_bb_write(sc, R92C_FPGA0_RFMOD, reg);

	/* Clear per-station keys table. */
	urtwn_cam_init(sc);

	/* Enable hardware sequence numbering. */
	urtwn_write_1(sc, R92C_HWSEQ_CTRL, 0xff);

	/* Perform LO and IQ calibrations. */
	urtwn_iq_calib(sc);
	/* Perform LC calibration. */
	urtwn_lc_calib(sc);

	/* Fix USB interference issue. */
	urtwn_write_1(sc, 0xfe40, 0xe0);
	urtwn_write_1(sc, 0xfe41, 0x8d);
	urtwn_write_1(sc, 0xfe42, 0x80);

	urtwn_pa_bias_init(sc);

	/* Initialize GPIO setting. */
	urtwn_write_1(sc, R92C_GPIO_MUXCFG,
	    urtwn_read_1(sc, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);

	/* Fix for lower temperature. */
	urtwn_write_1(sc, 0x15, 0xe9);

	/* Set default channel. */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	urtwn_set_chan(sc, ic->ic_ibss_chan, NULL);

	/* Queue Rx xfers. */
	for (i = 0; i < URTWN_RX_LIST_COUNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->rx_pipe, data, data->buf,
		    URTWN_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, urtwn_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != 0 && error != USBD_IN_PROGRESS)
			goto fail;
	}

	/* We're ready to go. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

#ifdef notyet
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		/* Install WEP keys. */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			urtwn_set_key(ic, NULL, &ic->ic_nw_keys[i]);
		urtwn_wait_async(sc);
	}
#endif
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	return (0);
 fail:
	urtwn_stop(ifp);
	return (error);
}

void
urtwn_stop(struct ifnet *ifp)
{
	struct urtwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i, s;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	s = splusb();
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	/* Wait for all async commands to complete. */
	urtwn_wait_async(sc);
	splx(s);

	timeout_del(&sc->scan_to);
	timeout_del(&sc->calib_to);

	/* Abort Tx. */
	for (i = 0; i < R92C_MAX_EPOUT; i++) {
		if (sc->tx_pipe[i] != NULL)
			usbd_abort_pipe(sc->tx_pipe[i]);
	}
	/* Stop Rx pipe. */
	usbd_abort_pipe(sc->rx_pipe);
	/* Free Tx/Rx buffers. */
	urtwn_free_tx_list(sc);
	urtwn_free_rx_list(sc);
}
