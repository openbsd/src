/*	$OpenBSD: if_uncm.c,v 1.1 2026/10/06 12:31:56 stsp Exp $ */

/*
 * Copyright (c) 2016 genua mbH
 * Copyright (c) 2026 Jan Schreiber
 * All rights reserved.
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
 * Based on if_umb.c
 */

/*
 * USB CDC NCM 1.0 specification:
 * https://www.usb.org/sites/default/files/NCM10_012011.zip
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/syslog.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_var.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbcdc.h>

#ifdef UNCM_DEBUG
#define DPRINTF(x...)							\
		do { if (uncm_debug) log(LOG_DEBUG, x); } while (0)
#define DPRINTFN(n, x...)						\
		do { if (uncm_debug >= (n)) log(LOG_DEBUG, x); } while (0)
int	 uncm_debug = 0;
#else
#define DPRINTF(x...)		do { } while (0)
#define DPRINTFN(n, x...)	do { } while (0)
#endif

#define DEVNAM(sc)		(((struct uncm_softc *)(sc))->sc_dev.dv_xname)

struct uncm_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;
#define GET_IFP(sc)	(&(sc)->sc_ac.ac_if)
	struct usbd_device	*sc_udev;

	uint32_t		 sc_maxpktlen;
	unsigned int		 sc_ncm_supported_formats;
	int			 sc_ncm_format;

	int			 sc_maxdgram;
	int			 sc_align;
	int			 sc_ndp_div;
	int			 sc_ndp_remainder;

	uint8_t			 sc_ctrl_ifaceno;
	struct usbd_interface	*sc_ctrl_iface;
	struct usbd_pipe	*sc_ctrl_pipe;
	struct usb_cdc_notification sc_intr_msg;
	struct usbd_interface	*sc_data_iface;

	int			 sc_data_ifaceidx;

	int			 sc_rx_ep;
	struct usbd_xfer	*sc_rx_xfer;
	void			*sc_rx_buf;
	int			 sc_rx_bufsz;
	struct usbd_pipe	*sc_rx_pipe;
	unsigned int		 sc_rx_nerr;

	int			 sc_tx_ep;
	struct usbd_xfer	*sc_tx_xfer;
	void			*sc_tx_buf;
	int			 sc_tx_bufsz;
	struct usbd_pipe	*sc_tx_pipe;
	struct mbuf_list	 sc_tx_ml;
	uint32_t		 sc_tx_seq;

	struct usb_task		 sc_link_task;
	int			 sc_link;
	uint64_t		 sc_baudrate;
};

#define UNCM_RX_NTB_MAX		16384

int		 uncm_match(struct device *, void *, void *);
void		 uncm_attach(struct device *, struct device *, void *);
int		 uncm_detach(struct device *, int);
void		 uncm_ntb_setup(struct uncm_softc *);
void		 uncm_ntb_setup_format(struct uncm_softc *);
void		 uncm_ntb_setup_input_size(struct uncm_softc *);
void		 uncm_get_eaddr(struct uncm_softc *, int);
int		 uncm_alloc_xfers(struct uncm_softc *);
void		 uncm_free_xfers(struct uncm_softc *);
int		 uncm_ioctl(struct ifnet *, u_long, caddr_t);
void		 uncm_init(struct uncm_softc *);
void		 uncm_stop(struct uncm_softc *);
void		 uncm_start(struct ifnet *);
void		 uncm_watchdog(struct ifnet *);
void		 uncm_rx(struct uncm_softc *);
void		 uncm_rxeof(struct usbd_xfer *, void *, usbd_status);
int		 uncm_encap(struct uncm_softc *, int);
void		 uncm_txeof(struct usbd_xfer *, void *, usbd_status);
void		 uncm_decap(struct uncm_softc *, struct usbd_xfer *);
void		 uncm_intr(struct usbd_xfer *, void *, usbd_status);
void		 uncm_link_task(void *);

int uncm_xfer_tout = USBD_DEFAULT_TIMEOUT;

struct cfdriver uncm_cd = {
	NULL, "uncm", DV_IFNET
};

const struct cfattach uncm_ca = {
	sizeof(struct uncm_softc),
	uncm_match,
	uncm_attach,
	uncm_detach,
	NULL,
};

int
uncm_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return UMATCH_NONE;

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return UMATCH_NONE;

	if (id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass == UISUBCLASS_NETWORK_CONTROL_MODEL)
		return UMATCH_IFACECLASS_IFACESUBCLASS;

	return UMATCH_NONE;
}

void
uncm_attach(struct device *parent, struct device *self, void *aux)
{
	struct uncm_softc *sc = (struct uncm_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_status status;
	struct usbd_desc_iter iter;
	const usb_descriptor_t *desc;
	struct usb_cdc_union_descriptor *ud;
	struct usb_cdc_ethernet_descriptor *ed_eth;
	struct usb_cdc_ncm_descriptor *nd;
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t	*cd;
	usb_endpoint_descriptor_t *ed;
	usb_interface_assoc_descriptor_t *ad;
	struct ifnet *ifp;
	int	current_ifaceno = -1;
	int	data_ifaceno = -1;
	int	mac_idx = -1;
	int	ctrl_ep;
	int	altnum, i, s;
	uint32_t maxpktlen;

	sc->sc_udev = uaa->device;
	sc->sc_ctrl_iface = uaa->iface;
	sc->sc_ctrl_ifaceno = uaa->ifaceno;
	sc->sc_maxpktlen = ETHER_MAX_LEN - ETHER_CRC_LEN;
	sc->sc_link = LINK_STATE_UNKNOWN;
	ml_init(&sc->sc_tx_ml);
	usb_init_task(&sc->sc_link_task, uncm_link_task, sc,
	    USB_TASK_TYPE_GENERIC);
	usbd_desc_iter_init(sc->sc_udev, &iter);
	while ((desc = usbd_desc_iter_next(&iter))) {
		if (desc->bDescriptorType == UDESC_IFACE_ASSOC) {
			ad = (usb_interface_assoc_descriptor_t *)desc;
			if (ad->bFirstInterface == uaa->ifaceno &&
			    ad->bInterfaceCount > 1) {
				/* Fall back to the interface following control. */
				data_ifaceno = uaa->ifaceno + 1;
			}
			continue;
		}
		if (desc->bDescriptorType == UDESC_INTERFACE) {
			id = (usb_interface_descriptor_t *)desc;
			current_ifaceno = id->bInterfaceNumber;
			continue;
		}
		if (current_ifaceno != uaa->ifaceno)
			continue;
		if (desc->bDescriptorType != UDESC_CS_INTERFACE)
			continue;
		switch (desc->bDescriptorSubtype) {
		case UDESCSUB_CDC_UNION:
			if (desc->bLength < sizeof(*ud))
				break;
			ud = (struct usb_cdc_union_descriptor *)desc;
			data_ifaceno = ud->bSlaveInterface[0];
			break;
		case UDESCSUB_CDC_ENF:
			if (desc->bLength < sizeof(*ed_eth))
				break;
			ed_eth = (struct usb_cdc_ethernet_descriptor *)desc;
			mac_idx = ed_eth->iMacAddress;
			maxpktlen = UGETW(ed_eth->wMaxSegmentSize);
			if (maxpktlen >= ETHER_MIN_LEN &&
			    maxpktlen <= ETHER_MAX_LEN - ETHER_CRC_LEN)
				sc->sc_maxpktlen = maxpktlen;
			break;
		case UDESCSUB_CDC_NCM:
			if (desc->bLength < sizeof(*nd))
				break;
			nd = (struct usb_cdc_ncm_descriptor *)desc;
			DPRINTFN(2, "%s: NCM version %x.%x, caps 0x%02x\n",
			    DEVNAM(sc), UGETW(nd->bcdNcmVersion) >> 8,
			    UGETW(nd->bcdNcmVersion) & 0xff,
			    nd->bmNetworkCapabilities);
			break;
		default:
			break;
		}
	}
	if (data_ifaceno == -1) {
		printf("%s: no data interface number\n", DEVNAM(sc));
		goto fail;
	}

	for (i = 0; i < uaa->nifaces; i++) {
		if (usbd_iface_claimed(sc->sc_udev, i))
			continue;
		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id != NULL && id->bInterfaceNumber == data_ifaceno) {
			sc->sc_data_iface = uaa->ifaces[i];
			sc->sc_data_ifaceidx = i;
			usbd_claim_iface(sc->sc_udev, i);
		}
	}
	if (sc->sc_data_iface == NULL) {
		printf("%s: no data interface found\n", DEVNAM(sc));
		goto fail;
	}

	id = usbd_get_interface_descriptor(sc->sc_ctrl_iface);
	ctrl_ep = -1;
	for (i = 0; i < id->bNumEndpoints && ctrl_ep == -1; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctrl_iface, i);
		if (ed == NULL)
			break;
		if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			ctrl_ep = ed->bEndpointAddress;
	}
	if (ctrl_ep == -1) {
		printf("%s: missing interrupt endpoint\n", DEVNAM(sc));
		goto fail;
	}

	cd = usbd_get_config_descriptor(sc->sc_udev);
	altnum = usbd_get_no_alts(cd, data_ifaceno);
	for (i = 0; i < altnum; i++) {
		id = usbd_find_idesc(cd, sc->sc_data_ifaceidx, i);
		if (id == NULL)
			continue;
		if (id->bInterfaceClass == UICLASS_CDC_DATA &&
		    id->bNumEndpoints == 2)
			break;
	}
	if (i == altnum || id == NULL) {
		printf("%s: missing alt setting for interface #%d\n",
		    DEVNAM(sc), data_ifaceno);
		goto fail;
	}
	status = usbd_set_interface(sc->sc_data_iface, i);
	if (status) {
		printf("%s: select alt setting %d for interface #%d "
		    "failed: %s\n", DEVNAM(sc), i, data_ifaceno,
		    usbd_errstr(status));
		goto fail;
	}

	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	sc->sc_rx_ep = sc->sc_tx_ep = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		if ((ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface,
		    i)) == NULL)
			break;
		if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
			sc->sc_rx_ep = ed->bEndpointAddress;
		else if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK &&
		    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT)
			sc->sc_tx_ep = ed->bEndpointAddress;
	}
	if (sc->sc_rx_ep == -1 || sc->sc_tx_ep == -1) {
		printf("%s: missing bulk endpoints\n", DEVNAM(sc));
		goto fail;
	}

	DPRINTFN(2, "%s: ctrl-ifno#%d: ep-ctrl=%d, data-ifno#%d: ep-rx=%d, "
	    "ep-tx=%d\n", DEVNAM(sc), sc->sc_ctrl_ifaceno,
	    UE_GET_ADDR(ctrl_ep), data_ifaceno,
	    UE_GET_ADDR(sc->sc_rx_ep), UE_GET_ADDR(sc->sc_tx_ep));

	if (usbd_open_pipe_intr(sc->sc_ctrl_iface, ctrl_ep, USBD_SHORT_XFER_OK,
	    &sc->sc_ctrl_pipe, sc, &sc->sc_intr_msg, sizeof(sc->sc_intr_msg),
	    uncm_intr, USBD_DEFAULT_INTERVAL)) {
		printf("%s: failed to open control pipe\n", DEVNAM(sc));
		goto fail;
	}

	sc->sc_ncm_format = NCM_FORMAT_NTB16;
	uncm_ntb_setup(sc);
	uncm_ntb_setup_format(sc);
	if (sc->sc_ncm_supported_formats == 0)
		goto fail;
	uncm_ntb_setup_input_size(sc);
	DPRINTFN(2, "%s: rx/tx size %d/%d\n", DEVNAM(sc),
	    sc->sc_rx_bufsz, sc->sc_tx_bufsz);

	ifp = GET_IFP(sc);
	uncm_get_eaddr(sc, mac_idx);

	s = splnet();
	printf("%s: address %s\n", DEVNAM(sc),
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = uncm_ioctl;
	ifp->if_start = uncm_start;
	ifp->if_watchdog = uncm_watchdog;
	strlcpy(ifp->if_xname, DEVNAM(sc), IFNAMSIZ);
	ifp->if_hardmtu = sc->sc_maxpktlen - ETHER_HDR_LEN;

	if_attach(ifp);
	ether_ifattach(ifp);
	splx(s);
	return;

fail:
	usbd_deactivate(sc->sc_udev);
	return;
}

int
uncm_detach(struct device *self, int flags)
{
	struct uncm_softc *sc = (struct uncm_softc *)self;
	struct ifnet *ifp = GET_IFP(sc);
	int	 s;

	s = splnet();
	if (ifp->if_flags & IFF_RUNNING)
		uncm_stop(sc);

	usb_rem_wait_task(sc->sc_udev, &sc->sc_link_task);
	if (sc->sc_ctrl_pipe) {
		usbd_close_pipe(sc->sc_ctrl_pipe);
		sc->sc_ctrl_pipe = NULL;
	}
	if (ifp->if_softc != NULL) {
		ether_ifdetach(ifp);
		if_detach(ifp);
	}
	splx(s);
	return 0;
}

static int
uncm_hexval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

void
uncm_get_eaddr(struct uncm_softc *sc, int idx)
{
	struct ifnet *ifp = GET_IFP(sc);
	usb_string_descriptor_t us;
	int	i, len, hi, lo;

	if (idx <= 0)
		goto fake;

	if (usbd_get_string_desc(sc->sc_udev, idx, USB_LANGID_EN_US, &us,
	    &len) || len < 2 + ETHER_ADDR_LEN * 2 * 2)
		goto fake;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		hi = uncm_hexval(UGETW(us.bString[2 * i]));
		lo = uncm_hexval(UGETW(us.bString[2 * i + 1]));
		if (hi < 0 || lo < 0)
			goto fake;
		sc->sc_ac.ac_enaddr[i] = (hi << 4) | lo;
	}
	if (ETHER_IS_MULTICAST(sc->sc_ac.ac_enaddr) ||
	    ETHER_IS_ANYADDR(sc->sc_ac.ac_enaddr))
		goto fake;
	return;

fake:
	ether_fakeaddr(ifp);
}

void
uncm_ntb_setup(struct uncm_softc *sc)
{
	usb_device_request_t req;
	struct ncm_ntb_parameters np;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = NCM_GET_NTB_PARAMETERS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, sizeof(np));
	if (usbd_do_request(sc->sc_udev, &req, &np) == USBD_NORMAL_COMPLETION &&
	    UGETW(np.wLength) == sizeof(np)) {
		sc->sc_rx_bufsz = MIN(UGETDW(np.dwNtbInMaxSize), UINT16_MAX);
		sc->sc_tx_bufsz = MIN(UGETDW(np.dwNtbOutMaxSize), UINT16_MAX);
		sc->sc_maxdgram = UGETW(np.wNtbOutMaxDatagrams);
		sc->sc_align = UGETW(np.wNdpOutAlignment);
		sc->sc_ndp_div = UGETW(np.wNdpOutDivisor);
		sc->sc_ndp_remainder = UGETW(np.wNdpOutPayloadRemainder);

		if (!powerof2(sc->sc_align) || sc->sc_align == 0 ||
		    sc->sc_align >= sc->sc_tx_bufsz)
			sc->sc_align = sizeof(uint32_t);
		if (!powerof2(sc->sc_ndp_div) || sc->sc_ndp_div == 0 ||
		    sc->sc_ndp_div >= sc->sc_tx_bufsz)
			sc->sc_ndp_div = sizeof(uint32_t);
		if (sc->sc_ndp_remainder >= sc->sc_ndp_div)
			sc->sc_ndp_remainder = 0;
		DPRINTF("%s: NCM align=%d div=%d rem=%d in=%u out=%u "
		    "maxdgram=%d\n", DEVNAM(sc), sc->sc_align, sc->sc_ndp_div,
		    sc->sc_ndp_remainder, UGETDW(np.dwNtbInMaxSize),
		    UGETDW(np.dwNtbOutMaxSize), sc->sc_maxdgram);
		sc->sc_ncm_supported_formats = UGETW(np.bmNtbFormatsSupported);
	} else {
		sc->sc_rx_bufsz = sc->sc_tx_bufsz = 8 * 1024;
		sc->sc_maxdgram = 0;
		sc->sc_align = sc->sc_ndp_div = sizeof(uint32_t);
		sc->sc_ndp_remainder = 0;
		DPRINTF("%s: align=default div=default rem=default\n",
		    DEVNAM(sc));
		sc->sc_ncm_supported_formats = NCM_FORMAT_NTB16_MASK;
	}

	if (sc->sc_rx_bufsz < sizeof(struct ncm_header16) +
	    sizeof(struct ncm_pointer16) +
	    sizeof(struct ncm_pointer16_dgram) ||
	    sc->sc_tx_bufsz < sizeof(struct ncm_header16) +
	    sizeof(struct ncm_pointer16) +
	    sizeof(struct ncm_pointer16_dgram) ||
	    ((sc->sc_ncm_supported_formats & NCM_FORMAT_NTB32_MASK) &&
	    (sc->sc_rx_bufsz < sizeof(struct ncm_header32) +
	    sizeof(struct ncm_pointer32) +
	    sizeof(struct ncm_pointer32_dgram) ||
	    sc->sc_tx_bufsz < sizeof(struct ncm_header32) +
	    sizeof(struct ncm_pointer32) +
	    sizeof(struct ncm_pointer32_dgram)))) {
		DPRINTF("%s: invalid NTB size %d/%d\n", DEVNAM(sc),
		    sc->sc_rx_bufsz, sc->sc_tx_bufsz);
		sc->sc_ncm_supported_formats = 0;
	}
}

void
uncm_ntb_setup_format(struct uncm_softc *sc)
{
	usb_device_request_t req;
	uWord wFmt;
	uint16_t fmt;

	if (sc->sc_ncm_supported_formats == 0)
		goto fail;

	if (sc->sc_ncm_supported_formats == NCM_FORMAT_NTB16_MASK) {
		DPRINTF("%s: only NTB16 format supported\n", DEVNAM(sc));
		sc->sc_ncm_format = NCM_FORMAT_NTB16;
		return;
	}

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = NCM_GET_NTB_FORMAT;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, sizeof(wFmt));
	if (usbd_do_request(sc->sc_udev, &req, wFmt) != USBD_NORMAL_COMPLETION)
		goto fail;
	fmt = UGETW(wFmt);
	if ((sc->sc_ncm_supported_formats & (1UL << fmt)) == 0)
		goto fail;
	if (fmt != NCM_FORMAT_NTB16 && fmt != NCM_FORMAT_NTB32)
		goto fail;
	sc->sc_ncm_format = fmt;

	DPRINTF("%s: using NCM format %d, supported 0x%x\n",
	    DEVNAM(sc), sc->sc_ncm_format, sc->sc_ncm_supported_formats);
	return;

fail:
	DPRINTF("%s: cannot set up NCM format\n", DEVNAM(sc));
	sc->sc_ncm_supported_formats = 0;
}

void
uncm_ntb_setup_input_size(struct uncm_softc *sc)
{
	usb_device_request_t req;
	uDWord dwSize;

	if (sc->sc_rx_bufsz <= UNCM_RX_NTB_MAX)
		return;

	USETDW(dwSize, UNCM_RX_NTB_MAX);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = NCM_SET_NTB_INPUT_SIZE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, sizeof(dwSize));
	if (usbd_do_request(sc->sc_udev, &req, dwSize) ==
	    USBD_NORMAL_COMPLETION)
		sc->sc_rx_bufsz = UNCM_RX_NTB_MAX;
	else
		DPRINTF("%s: SET_NTB_INPUT_SIZE refused, keeping %d\n",
		    DEVNAM(sc), sc->sc_rx_bufsz);
}

int
uncm_alloc_xfers(struct uncm_softc *sc)
{
	if (sc->sc_rx_xfer == NULL) {
		if ((sc->sc_rx_xfer = usbd_alloc_xfer(sc->sc_udev)) != NULL)
			sc->sc_rx_buf = usbd_alloc_buffer(sc->sc_rx_xfer,
			    sc->sc_rx_bufsz);
	}

    if (sc->sc_tx_xfer == NULL) {
		if ((sc->sc_tx_xfer = usbd_alloc_xfer(sc->sc_udev)) != NULL)
			sc->sc_tx_buf = usbd_alloc_buffer(sc->sc_tx_xfer,
			    sc->sc_tx_bufsz);
    }

	return (sc->sc_rx_buf != NULL && sc->sc_tx_buf != NULL);
}

void
uncm_free_xfers(struct uncm_softc *sc)
{
	if (sc->sc_rx_xfer != NULL) {
		usbd_free_xfer(sc->sc_rx_xfer);
		sc->sc_rx_xfer = NULL;
		sc->sc_rx_buf = NULL;
	}

    if (sc->sc_tx_xfer != NULL) {
		usbd_free_xfer(sc->sc_tx_xfer);
		sc->sc_tx_xfer = NULL;
		sc->sc_tx_buf = NULL;
	}

    ml_purge(&sc->sc_tx_ml);
}

void
uncm_init(struct uncm_softc *sc)
{
	struct ifnet *ifp = GET_IFP(sc);

	splassert(IPL_NET);

	if (ifp->if_flags & IFF_RUNNING)
		return;

	sc->sc_tx_seq = 0;
	if (!uncm_alloc_xfers(sc)) {
		printf("%s: allocation of xfers failed\n", DEVNAM(sc));
		uncm_free_xfers(sc);
		return;
	}
	if (usbd_open_pipe(sc->sc_data_iface, sc->sc_rx_ep,
	    USBD_EXCLUSIVE_USE, &sc->sc_rx_pipe)) {
		printf("%s: open rx pipe failed\n", DEVNAM(sc));
		goto fail;
	}
	if (usbd_open_pipe(sc->sc_data_iface, sc->sc_tx_ep,
	    USBD_EXCLUSIVE_USE, &sc->sc_tx_pipe)) {
		printf("%s: open tx pipe failed\n", DEVNAM(sc));
		goto fail;
	}

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
	uncm_rx(sc);
	return;

fail:
	uncm_stop(sc);
}

void
uncm_stop(struct uncm_softc *sc)
{
	struct ifnet *ifp = GET_IFP(sc);

	splassert(IPL_NET);

	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_timer = 0;
	if (sc->sc_rx_pipe) {
		usbd_close_pipe(sc->sc_rx_pipe);
		sc->sc_rx_pipe = NULL;
	}
	if (sc->sc_tx_pipe) {
		usbd_close_pipe(sc->sc_tx_pipe);
		sc->sc_tx_pipe = NULL;
	}
	uncm_free_xfers(sc);
}

int
uncm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct uncm_softc *sc = ifp->if_softc;
	int	 s, error = 0;

	if (usbd_is_dying(sc->sc_udev))
		return ENXIO;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			uncm_init(sc);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				uncm_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				uncm_stop(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	if (error == ENETRESET)
		error = 0;

	splx(s);
	return error;
}

static inline int
uncm_align(size_t bufsz, int offs, int alignment, int remainder)
{
	size_t	 m = alignment - 1;
	int	 newoff;

	newoff = (((size_t)offs + m) & ~m) - alignment + remainder;
	if (newoff < offs)
		newoff += alignment;
	if (newoff > bufsz)
		newoff = bufsz;
	return newoff - offs;
}

static inline int
uncm_padding(void *buf, size_t bufsz, int offs, int alignment, int remainder)
{
	int	 nb;

	nb = uncm_align(bufsz, offs, alignment, remainder);
	if (nb > 0)
		memset(buf + offs, 0, nb);
	return nb;
}

void
uncm_start(struct ifnet *ifp)
{
	struct uncm_softc *sc = ifp->if_softc;
	struct mbuf *m = NULL;
	int	 ndgram = 0;
	int	 offs, len, mlen;
	int	 maxoverhead;

	if (usbd_is_dying(sc->sc_udev) ||
	    !(ifp->if_flags & IFF_RUNNING) ||
	    ifq_is_oactive(&ifp->if_snd))
		return;

	KASSERT(ml_empty(&sc->sc_tx_ml));
	KASSERT(sc->sc_ncm_format == NCM_FORMAT_NTB16 ||
	    sc->sc_ncm_format == NCM_FORMAT_NTB32);

	switch (sc->sc_ncm_format) {
	case NCM_FORMAT_NTB16:
		offs = sizeof(struct ncm_header16);
		offs += uncm_align(sc->sc_tx_bufsz, offs, sc->sc_align, 0);
		offs += sizeof(struct ncm_pointer16);
		maxoverhead = sizeof(struct ncm_pointer16_dgram);
		break;
	case NCM_FORMAT_NTB32:
		offs = sizeof(struct ncm_header32);
		offs += uncm_align(sc->sc_tx_bufsz, offs, sc->sc_align, 0);
		offs += sizeof(struct ncm_pointer32);
		maxoverhead = sizeof(struct ncm_pointer32_dgram);
		break;
	}

	maxoverhead += sc->sc_ndp_div - 1;

	len = 0;
	while (1) {
		m = ifq_deq_begin(&ifp->if_snd);
		if (m == NULL)
			break;

		mlen = maxoverhead +  m->m_pkthdr.len;
		if ((sc->sc_maxdgram != 0 && ndgram >= sc->sc_maxdgram) ||
		    (offs + len + mlen > sc->sc_tx_bufsz)) {
			ifq_deq_rollback(&ifp->if_snd, m);
			break;
		}
		ifq_deq_commit(&ifp->if_snd, m);

		ndgram++;
		len += mlen;
		ml_enqueue(&sc->sc_tx_ml, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}
	if (ml_empty(&sc->sc_tx_ml))
		return;
	if (uncm_encap(sc, ndgram)) {
		ifq_set_oactive(&ifp->if_snd);
		ifp->if_timer = (2 * uncm_xfer_tout) / 1000;
	}
}

void
uncm_watchdog(struct ifnet *ifp)
{
	struct uncm_softc *sc = ifp->if_softc;

	if (usbd_is_dying(sc->sc_udev))
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", DEVNAM(sc));
	usbd_abort_pipe(sc->sc_tx_pipe);
}

void
uncm_rx(struct uncm_softc *sc)
{
	struct ifnet *ifp = GET_IFP(sc);
	usbd_status err;

	usbd_setup_xfer(sc->sc_rx_xfer, sc->sc_rx_pipe, sc, sc->sc_rx_buf,
	    sc->sc_rx_bufsz, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, uncm_rxeof);
	err = usbd_transfer(sc->sc_rx_xfer);
	if (err != USBD_IN_PROGRESS) {
		ifp->if_ierrors++;
		DPRINTF("%s: start rx error: %s\n", DEVNAM(sc),
		    usbd_errstr(err));
	}
}

void
uncm_rxeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct uncm_softc *sc = priv;
	struct ifnet *ifp = GET_IFP(sc);

	if (usbd_is_dying(sc->sc_udev) || !(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		DPRINTF("%s: rx error: %s\n", DEVNAM(sc), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_rx_pipe);
		if (++sc->sc_rx_nerr > 100) {
			log(LOG_ERR, "%s: too many rx errors, disabling\n",
			    DEVNAM(sc));
			usbd_deactivate(sc->sc_udev);
		}
	} else {
		sc->sc_rx_nerr = 0;
		uncm_decap(sc, xfer);
	}

	uncm_rx(sc);
}

int
uncm_encap(struct uncm_softc *sc, int ndgram)
{
	struct ncm_header16 *hdr16 = NULL;
	struct ncm_header32 *hdr32 = NULL;
	struct ncm_pointer16 *ptr16 = NULL;
	struct ncm_pointer32 *ptr32 = NULL;
	struct ncm_pointer16_dgram *dgram16 = NULL;
	struct ncm_pointer32_dgram *dgram32 = NULL;
	int	 offs = 0, plen = 0;
	int	 dgoffs = 0, poffs;
	struct mbuf *m;
	usbd_status  err;

	KASSERT(sc->sc_ncm_format == NCM_FORMAT_NTB16 ||
	    sc->sc_ncm_format == NCM_FORMAT_NTB32);

	switch (sc->sc_ncm_format) {
	case NCM_FORMAT_NTB16:
		hdr16 = sc->sc_tx_buf;
		USETDW(hdr16->dwSignature, NCM_HDR16_SIG);
		USETW(hdr16->wHeaderLength, sizeof(*hdr16));
		USETW(hdr16->wSequence, sc->sc_tx_seq);
		USETW(hdr16->wBlockLength, 0);
		offs = sizeof(*hdr16);
		break;
	case NCM_FORMAT_NTB32:
		hdr32 = sc->sc_tx_buf;
		USETDW(hdr32->dwSignature, NCM_HDR32_SIG);
		USETW(hdr32->wHeaderLength, sizeof(*hdr32));
		USETW(hdr32->wSequence, sc->sc_tx_seq);
		USETDW(hdr32->dwBlockLength, 0);
		offs = sizeof(*hdr32);
		break;
	}
	offs += uncm_padding(sc->sc_tx_buf, sc->sc_tx_bufsz, offs,
	    sc->sc_align, 0);
	poffs = offs;

	switch (sc->sc_ncm_format) {
	case NCM_FORMAT_NTB16:
		USETW(hdr16->wNdpIndex, poffs);
		ptr16 = (struct ncm_pointer16 *)(sc->sc_tx_buf + poffs);
		plen = sizeof(*ptr16) + ndgram * sizeof(*dgram16);
		USETDW(ptr16->dwSignature, NCM_NDP16_SIG_NOCRC);
		USETW(ptr16->wLength, plen);
		USETW(ptr16->wNextNdpIndex, 0);
		dgram16 = ptr16->dgram;
		break;
	case NCM_FORMAT_NTB32:
		USETDW(hdr32->dwNdpIndex, poffs);
		ptr32 = (struct ncm_pointer32 *)(sc->sc_tx_buf + poffs);
		plen = sizeof(*ptr32) + ndgram * sizeof(*dgram32);
		USETDW(ptr32->dwSignature, NCM_NDP32_SIG_NOCRC);
		USETW(ptr32->wLength, plen);
		USETW(ptr32->wReserved6, 0);
		USETDW(ptr32->dwNextNdpIndex, 0);
		USETDW(ptr32->dwReserved12, 0);
		dgram32 = ptr32->dgram;
		break;
	}
	dgoffs = offs + plen;

	sc->sc_tx_seq++;
	while ((m = ml_dequeue(&sc->sc_tx_ml)) != NULL) {
		dgoffs += uncm_padding(sc->sc_tx_buf, sc->sc_tx_bufsz, dgoffs,
		    sc->sc_ndp_div, sc->sc_ndp_remainder);
		switch (sc->sc_ncm_format) {
		case NCM_FORMAT_NTB16:
			USETW(dgram16->wDatagramIndex, dgoffs);
			USETW(dgram16->wDatagramLen, m->m_pkthdr.len);
			dgram16++;
			break;
		case NCM_FORMAT_NTB32:
			USETDW(dgram32->dwDatagramIndex, dgoffs);
			USETDW(dgram32->dwDatagramLen, m->m_pkthdr.len);
			dgram32++;
			break;
		}
		m_copydata(m, 0, m->m_pkthdr.len, sc->sc_tx_buf + dgoffs);
		dgoffs += m->m_pkthdr.len;
		m_freem(m);
	}
	offs = dgoffs;

	switch (sc->sc_ncm_format) {
	case NCM_FORMAT_NTB16:
		USETW(dgram16->wDatagramIndex, 0);
		USETW(dgram16->wDatagramLen, 0);
		USETW(hdr16->wBlockLength, offs);
		KASSERT(dgram16 - ptr16->dgram == ndgram);
		break;
	case NCM_FORMAT_NTB32:
		USETDW(dgram32->dwDatagramIndex, 0);
		USETDW(dgram32->dwDatagramLen, 0);
		USETDW(hdr32->dwBlockLength, offs);
		KASSERT(dgram32 - ptr32->dgram == ndgram);
		break;
	}

	DPRINTFN(3, "%s: encap %d bytes, %d dgrams\n", DEVNAM(sc), offs,
	    ndgram);
	KASSERT(offs <= sc->sc_tx_bufsz);

	usbd_setup_xfer(sc->sc_tx_xfer, sc->sc_tx_pipe, sc, sc->sc_tx_buf, offs,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, uncm_xfer_tout, uncm_txeof);
	err = usbd_transfer(sc->sc_tx_xfer);
	if (err != USBD_IN_PROGRESS) {
		GET_IFP(sc)->if_oerrors++;
		DPRINTF("%s: start tx error: %s\n", DEVNAM(sc),
		    usbd_errstr(err));
		return 0;
	}
	return 1;
}

void
uncm_txeof(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct uncm_softc *sc = priv;
	struct ifnet *ifp = GET_IFP(sc);
	int	 s;

	s = splnet();
	ifq_clr_oactive(&ifp->if_snd);
	ifp->if_timer = 0;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status != USBD_NOT_STARTED && status != USBD_CANCELLED) {
			ifp->if_oerrors++;
			DPRINTF("%s: tx error: %s\n", DEVNAM(sc),
			    usbd_errstr(status));
			if (status == USBD_STALLED)
				usbd_clear_endpoint_stall_async(sc->sc_tx_pipe);
		}
	}
	if (ifq_empty(&ifp->if_snd) == 0)
		uncm_start(ifp);

	splx(s);
}

/*
 * XXX Only the first NDP is processed; wNextNdpIndex not followed
 * (same as umb(4)).
 */
void
uncm_decap(struct uncm_softc *sc, struct usbd_xfer *xfer)
{
	struct ifnet *ifp = GET_IFP(sc);
	int	s;
	void *buf;
	uint32_t len;
	char *dp;
	struct ncm_header16 *hdr16;
	struct ncm_header32 *hdr32;
	struct ncm_pointer16 *ptr16;
	struct ncm_pointer16_dgram *dgram16;
	struct ncm_pointer32_dgram *dgram32;
	uint32_t hsig, psig;
	uint32_t ptrlen, dgentryoff;
	uint64_t blen, ptroff, doff, dlen;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;

	usbd_get_xfer_status(xfer, NULL, &buf, &len, NULL);
	DPRINTFN(4, "%s: recv %d bytes\n", DEVNAM(sc), len);
	s = splnet();
	if (len < sizeof(*hdr16))
		goto toosmall;

	hdr16 = (struct ncm_header16 *)buf;
	hsig = UGETDW(hdr16->dwSignature);

	switch (hsig) {
	case NCM_HDR16_SIG:
		blen = UGETW(hdr16->wBlockLength);
		ptroff = UGETW(hdr16->wNdpIndex);
		if (UGETW(hdr16->wHeaderLength) != sizeof(*hdr16)) {
			DPRINTF("%s: bad header len %d for NTH16 (exp %zu)\n",
			    DEVNAM(sc), UGETW(hdr16->wHeaderLength),
			    sizeof(*hdr16));
			goto fail;
		}
		break;
	case NCM_HDR32_SIG:
		if (len < sizeof(*hdr32))
			goto toosmall;
		hdr32 = (struct ncm_header32 *)hdr16;
		blen = UGETDW(hdr32->dwBlockLength);
		ptroff = UGETDW(hdr32->dwNdpIndex);
		if (UGETW(hdr32->wHeaderLength) != sizeof(*hdr32)) {
			DPRINTF("%s: bad header len %d for NTH32 (exp %zu)\n",
			    DEVNAM(sc), UGETW(hdr32->wHeaderLength),
			    sizeof(*hdr32));
			goto fail;
		}
		break;
	default:
		DPRINTF("%s: unsupported NCM header signature (0x%08x)\n",
		    DEVNAM(sc), hsig);
		goto fail;
	}
	if (blen != 0 && len < blen) {
		DPRINTF("%s: bad NTB len (%llu) for %d bytes of data\n",
		    DEVNAM(sc), blen, len);
		goto fail;
	}

	if ((uint64_t)len < ptroff + sizeof(*ptr16))
		goto toosmall;
	ptr16 = (struct ncm_pointer16 *)(buf + ptroff);
	psig = UGETDW(ptr16->dwSignature);
	ptrlen = UGETW(ptr16->wLength);
	if ((uint64_t)len < (uint64_t)ptrlen + ptroff)
		goto toosmall;

	switch (psig) {
	case NCM_NDP16_SIG_NOCRC:
		if (hsig != NCM_HDR16_SIG)
			goto badsig;
		dgentryoff = offsetof(struct ncm_pointer16, dgram);
		if (ptrlen < dgentryoff + sizeof(*dgram16))
			goto toosmall;
		break;
	case NCM_NDP32_SIG_NOCRC:
		if (hsig != NCM_HDR32_SIG)
			goto badsig;
		dgentryoff = offsetof(struct ncm_pointer32, dgram);
		if (ptrlen < dgentryoff + sizeof(*dgram32))
			goto toosmall;
		break;
	case NCM_NDP16_SIG_CRC:
	case NCM_NDP32_SIG_CRC:
	default:
	badsig:
		DPRINTF("%s: unsupported NCM pointer signature (0x%08x)\n",
		    DEVNAM(sc), psig);
		goto fail;
	}

	while (dgentryoff < ptrlen) {
		switch (hsig) {
		case NCM_HDR16_SIG:
			if (ptrlen - dgentryoff < sizeof(*dgram16))
				goto done;
			dgram16 = (struct ncm_pointer16_dgram *)
			    (buf + ptroff + dgentryoff);
			dgentryoff += sizeof(*dgram16);
			dlen = UGETW(dgram16->wDatagramLen);
			doff = UGETW(dgram16->wDatagramIndex);
			break;
		case NCM_HDR32_SIG:
			if (ptrlen - dgentryoff < sizeof(*dgram32))
				goto done;
			dgram32 = (struct ncm_pointer32_dgram *)
			    (buf + ptroff + dgentryoff);
			dgentryoff += sizeof(*dgram32);
			dlen = UGETDW(dgram32->dwDatagramLen);
			doff = UGETDW(dgram32->dwDatagramIndex);
			break;
		default:
			ifp->if_ierrors++;
			goto done;
		}

		if (dlen == 0 || doff == 0)
			break;
		if ((uint64_t)len < dlen + doff ||
		    (blen != 0 && blen < dlen + doff)) {
			DPRINTF("%s: datagram too large (%llu @ off %llu)\n",
			    DEVNAM(sc), dlen, doff);
			ifp->if_ierrors++;
			continue;
		}
		if (dlen < ETHER_HDR_LEN || dlen > sc->sc_maxpktlen) {
			ifp->if_ierrors++;
			continue;
		}

		dp = buf + doff;
		DPRINTFN(3, "%s: decap %llu bytes\n", DEVNAM(sc), dlen);
		m = m_devget(dp, (int)dlen, ETHER_ALIGN);
		if (m == NULL) {
			ifp->if_iqdrops++;
			continue;
		}
		ml_enqueue(&ml, m);
	}
done:
	if_input(ifp, &ml);
	splx(s);
	return;
toosmall:
	DPRINTF("%s: packet too small (%d)\n", DEVNAM(sc), len);
fail:
	ifp->if_ierrors++;
	splx(s);
}

void
uncm_intr(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
	struct uncm_softc *sc = priv;
	struct ifnet *ifp = GET_IFP(sc);
	uint32_t up, down;
	int	total_len;

	if (usbd_is_dying(sc->sc_udev))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF("%s: notification error: %s\n", DEVNAM(sc),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_ctrl_pipe);
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);
	if (total_len < UCDC_NOTIFICATION_LENGTH) {
		DPRINTF("%s: short notification (%d<%d)\n", DEVNAM(sc),
		    total_len, UCDC_NOTIFICATION_LENGTH);
		return;
	}
	if (sc->sc_intr_msg.bmRequestType != UCDC_NOTIFICATION) {
		DPRINTF("%s: unexpected notification (type=0x%02x)\n",
		    DEVNAM(sc), sc->sc_intr_msg.bmRequestType);
		return;
	}

	switch (sc->sc_intr_msg.bNotification) {
	case UCDC_N_NETWORK_CONNECTION:
		if (ifp->if_flags & IFF_DEBUG)
			log(LOG_DEBUG, "%s: network %sconnected\n", DEVNAM(sc),
			    UGETW(sc->sc_intr_msg.wValue) ? "" : "dis");
		sc->sc_link = UGETW(sc->sc_intr_msg.wValue) ?
		    LINK_STATE_UP : LINK_STATE_DOWN;
		usb_add_task(sc->sc_udev, &sc->sc_link_task);
		break;
	case UCDC_N_CONNECTION_SPEED_CHANGE:
		if (total_len >= UCDC_NOTIFICATION_LENGTH + 8) {
			down = UGETDW(&sc->sc_intr_msg.data[0]);
			up = UGETDW(&sc->sc_intr_msg.data[4]);
			sc->sc_baudrate = MIN(up, down);
			DPRINTFN(2, "%s: speed down %u up %u\n", DEVNAM(sc),
			    down, up);
			usb_add_task(sc->sc_udev, &sc->sc_link_task);
		}
		break;
	default:
		DPRINTF("%s: unexpected notification (0x%02x)\n",
		    DEVNAM(sc), sc->sc_intr_msg.bNotification);
		break;
	}
}

void
uncm_link_task(void *arg)
{
	struct uncm_softc *sc = arg;
	struct ifnet *ifp = GET_IFP(sc);
	int	 s;

	if (usbd_is_dying(sc->sc_udev))
		return;

	s = splnet();
	ifp->if_baudrate = sc->sc_baudrate;
	if (sc->sc_link != LINK_STATE_UNKNOWN &&
	    ifp->if_link_state != sc->sc_link) {
		if (ifp->if_flags & IFF_DEBUG)
			log(LOG_DEBUG, "%s: link state changed from %s to %s\n",
			    DEVNAM(sc),
			    LINK_STATE_IS_UP(ifp->if_link_state) ? "up" : "down",
			    LINK_STATE_IS_UP(sc->sc_link) ? "up" : "down");
		ifp->if_link_state = sc->sc_link;
		if_link_state_change(ifp);
		if (LINK_STATE_IS_UP(sc->sc_link) &&
		    (ifp->if_flags & IFF_RUNNING) && !ifq_empty(&ifp->if_snd))
			uncm_start(ifp);
	}
	splx(s);
}

