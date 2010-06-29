/*	$OpenBSD: if_cdcef.c,v 1.25 2010/06/29 07:12:31 matthew Exp $	*/

/*
 * Copyright (c) 2007 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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
 * USB Communication Device Class Ethernet Emulation Model function driver
 * (counterpart of the host-side cdce(4) driver)
 */
#include <bpfilter.h>


#include <sys/param.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/timeout.h>

#include <net/if.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbcdc.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>


#define CDCEF_VENDOR_ID		0x0001
#define CDCEF_PRODUCT_ID	0x0001
#define CDCEF_DEVICE_CODE	0x0100
#define CDCEF_VENDOR_STRING	"OpenBSD.org"
#define CDCEF_PRODUCT_STRING	"CDC Ethernet Emulation"
#define CDCEF_SERIAL_STRING	"1.00"

#define CDCEF_BUFSZ		1600


struct cdcef_softc {
	struct usbf_function	sc_dev;
	usbf_config_handle	sc_config;
	usbf_interface_handle	sc_iface;
	usbf_endpoint_handle	sc_ep_in;
	usbf_endpoint_handle	sc_ep_out;
	usbf_pipe_handle	sc_pipe_in;
	usbf_pipe_handle	sc_pipe_out;
	usbf_xfer_handle	sc_xfer_in;
	usbf_xfer_handle	sc_xfer_out;
	void			*sc_buffer_in;
	void			*sc_buffer_out;

	struct timeout		start_to;

	struct mbuf		*sc_xmit_mbuf;

	struct arpcom           sc_arpcom;
#define GET_IFP(sc) (&(sc)->sc_arpcom.ac_if)

	int			sc_rxeof_errors;
	int			sc_unit;
	int			sc_attached;
	int			sc_listening;
};

int		cdcef_match(struct device *, void *, void *);
void		cdcef_attach(struct device *, struct device *, void *);

usbf_status	cdcef_do_request(usbf_function_handle,
				 usb_device_request_t *, void **);

void		cdcef_start(struct ifnet *);

void		cdcef_txeof(usbf_xfer_handle, usbf_private_handle,
			    usbf_status);
void		cdcef_rxeof(usbf_xfer_handle, usbf_private_handle,
			    usbf_status);
int		cdcef_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
void		cdcef_watchdog(struct ifnet *ifp);
void		cdcef_init(struct cdcef_softc *);
void		cdcef_stop(struct cdcef_softc *);
int		cdcef_encap(struct cdcef_softc *sc, struct mbuf *m, int idx);
struct mbuf *	cdcef_newbuf(void);
void		cdcef_start_timeout (void *);

struct cfattach cdcef_ca = {
	sizeof(struct cdcef_softc), cdcef_match, cdcef_attach
};

struct cfdriver cdcef_cd = {
	NULL, "cdcef", DV_IFNET
};

struct usbf_function_methods cdcef_methods = {
	NULL,			/* set_config */
	cdcef_do_request
};

#ifndef CDCEF_DEBUG
#define DPRINTF(x)	do {} while (0)
#else
#define DPRINTF(x)	printf x
#endif

#define DEVNAME(sc)	((sc)->sc_dev.bdev.dv_xname)

extern int ticks;

/*
 * USB function match/attach/detach
 */

int
cdcef_match(struct device *parent, void *match, void *aux)
{
	return UMATCH_GENERIC;
}

void
cdcef_attach(struct device *parent, struct device *self, void *aux)
{
	struct cdcef_softc *sc = (struct cdcef_softc *)self;
	struct usbf_attach_arg *uaa = aux;
	usbf_device_handle dev = uaa->device;
	struct ifnet *ifp;
	usbf_status err;
	usb_cdc_union_descriptor_t udesc;
	int s;
	u_int16_t macaddr_hi;


	/* Set the device identification according to the function. */
	usbf_devinfo_setup(dev, UDCLASS_IN_INTERFACE, 0, 0, CDCEF_VENDOR_ID,
	    CDCEF_PRODUCT_ID, CDCEF_DEVICE_CODE, CDCEF_VENDOR_STRING,
	    CDCEF_PRODUCT_STRING, CDCEF_SERIAL_STRING);

	/* Fill in the fields needed by the parent device. */
	sc->sc_dev.methods = &cdcef_methods;

	/* timeout to start delayed transfers */
	timeout_set(&sc->start_to, cdcef_start_timeout, sc);

	/*
	 * Build descriptors according to the device class specification.
	 */
	err = usbf_add_config(dev, &sc->sc_config);
	if (err) {
		printf(": usbf_add_config failed\n");
		return;
	}
	err = usbf_add_interface(sc->sc_config, UICLASS_CDC,
	    UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL, 0, NULL,
	    &sc->sc_iface);
	if (err) {
		printf(": usbf_add_interface failed\n");
		return;
	}
	/* XXX don't use hard-coded values 128 and 16. */
	err = usbf_add_endpoint(sc->sc_iface, UE_DIR_IN | 2, UE_BULK,
	    64, 16, &sc->sc_ep_in) ||
	    usbf_add_endpoint(sc->sc_iface, UE_DIR_OUT | 1, UE_BULK,
	    64, 16, &sc->sc_ep_out);
	if (err) {
		printf(": usbf_add_endpoint failed\n");
		return;
	}

	/* Append a CDC union descriptor. */
	bzero(&udesc, sizeof udesc);
	udesc.bLength = sizeof udesc;
	udesc.bDescriptorType = UDESC_CS_INTERFACE;
	udesc.bDescriptorSubtype = UDESCSUB_CDC_UNION;
	udesc.bSlaveInterface[0] = usbf_interface_number(sc->sc_iface);
	err = usbf_add_config_desc(sc->sc_config,
	    (usb_descriptor_t *)&udesc, NULL);
	if (err) {
		printf(": usbf_add_config_desc failed\n");
		return;
	}

	/*
	 * Close the configuration and build permanent descriptors.
	 */
	err = usbf_end_config(sc->sc_config);
	if (err) {
		printf(": usbf_end_config failed\n");
		return;
	}

	/* Preallocate xfers and data buffers. */
	sc->sc_xfer_in = usbf_alloc_xfer(dev);
	sc->sc_xfer_out = usbf_alloc_xfer(dev);
	sc->sc_buffer_in = usbf_alloc_buffer(sc->sc_xfer_in,
	    CDCEF_BUFSZ);
	sc->sc_buffer_out = usbf_alloc_buffer(sc->sc_xfer_out,
	    CDCEF_BUFSZ);
	if (sc->sc_buffer_in == NULL || sc->sc_buffer_out == NULL) {
		printf(": usbf_alloc_buffer failed\n");
		return;
	}

	/* Open the bulk pipes. */
	err = usbf_open_pipe(sc->sc_iface,
	    usbf_endpoint_address(sc->sc_ep_out), &sc->sc_pipe_out) ||
	    usbf_open_pipe(sc->sc_iface,
	    usbf_endpoint_address(sc->sc_ep_in), &sc->sc_pipe_in);
	if (err) {
		printf(": usbf_open_pipe failed\n");
		return;
	}

	/* Get ready to receive packets. */
	usbf_setup_xfer(sc->sc_xfer_out, sc->sc_pipe_out, sc,
	    sc->sc_buffer_out, CDCEF_BUFSZ, USBD_SHORT_XFER_OK, 0, cdcef_rxeof);
	err = usbf_transfer(sc->sc_xfer_out);
	if (err && err != USBF_IN_PROGRESS) {
		printf(": usbf_transfer failed\n");
		return;
	}

	s = splnet();

	macaddr_hi = htons(0x2acb);
	bcopy(&macaddr_hi, &sc->sc_arpcom.ac_enaddr[0], sizeof(u_int16_t));
	bcopy(&ticks, &sc->sc_arpcom.ac_enaddr[2], sizeof(u_int32_t));
	sc->sc_arpcom.ac_enaddr[5] = (u_int8_t)(sc->sc_unit);

	printf(": address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cdcef_ioctl;
	ifp->if_start = cdcef_start;
	ifp->if_watchdog = cdcef_watchdog;
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_attached = 1;
	splx(s);
}

usbf_status
cdcef_do_request(usbf_function_handle fun, usb_device_request_t *req,
    void **data)
{
	printf("cdcef_do_request\n");
	return USBF_STALLED;
}

void
cdcef_start(struct ifnet *ifp)
{
	struct cdcef_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	if(ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		return;
	}

	if (sc->sc_listening == 0 || m_head->m_pkthdr.len > CDCEF_BUFSZ) {
		/*
		 * drop packet because receiver is not listening,
		 * or if packet is larger than xmit buffer
		 */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		m_freem(m_head);
		return;
	}

	if (cdcef_encap(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
					
	ifp->if_flags |= IFF_OACTIVE;

	ifp->if_timer = 6;
}

void
cdcef_txeof(usbf_xfer_handle xfer, usbf_private_handle priv,
    usbf_status err)
{
	struct cdcef_softc *sc = priv;
	struct ifnet *ifp = GET_IFP(sc);
	int s;

	s = splnet();
#if 0
	printf("cdcef_txeof: xfer=%p, priv=%p, %s\n", xfer, priv,
	    usbf_errstr(err));
#endif

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->sc_xmit_mbuf != NULL) {
		m_freem(sc->sc_xmit_mbuf);
		sc->sc_xmit_mbuf = NULL;
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		timeout_add(&sc->start_to, 1); /* XXX  */

	splx(s);
}
void
cdcef_start_timeout (void *v)
{
	struct cdcef_softc *sc = v;
	struct ifnet *ifp = GET_IFP(sc);
	int s;

	s = splnet();
	cdcef_start(ifp);
	splx(s);
}


void
cdcef_rxeof(usbf_xfer_handle xfer, usbf_private_handle priv,
    usbf_status status)
{
	struct cdcef_softc	*sc = priv;
	int total_len = 0;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m = NULL;


	int s;

#if 0
	printf("cdcef_rxeof: xfer=%p, priv=%p, %s\n", xfer, priv,
	    usbf_errstr(status));
#endif

	if (status != USBF_NORMAL_COMPLETION) {
		if (status == USBF_NOT_STARTED || status == USBF_CANCELLED)	
			return;
		if (sc->sc_rxeof_errors == 0)
			printf("%s: usb error on rx: %s\n",
			    DEVNAME(sc), usbf_errstr(status));
		/* XXX - no stalls on client */
		if (sc->sc_rxeof_errors++ > 10) {
			printf("%s: too many errors, disabling\n",
			    DEVNAME(sc));
			/* sc->sc_dying = 1; */
			// return;
		}
		goto done;
	}
	sc->sc_rxeof_errors = 0;

	/* upon first incoming packet we know the host is listening */
	if (sc->sc_listening == 0) {
		sc->sc_listening = 1;
	}


	usbf_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	/* total_len -= 4; Strip off CRC added for Zaurus - XXX*/
	if (total_len <= 1)
		goto done;

	if (total_len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	s = splnet();
	if (ifp->if_flags & IFF_RUNNING) {
		m = cdcef_newbuf();
		if (m == NULL) {
			/* message? */
			ifp->if_ierrors++;
			goto done1;
		}

		m->m_pkthdr.len = m->m_len = total_len;
		bcopy(sc->sc_buffer_out, mtod(m, char *), total_len);
		m->m_pkthdr.rcvif = ifp;

		ifp->if_ipackets++;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);
	}

done1:
	splx(s);

done:
	/* Setup another xfer. */
	usbf_setup_xfer(xfer, sc->sc_pipe_out, sc, sc->sc_buffer_out,
	    CDCEF_BUFSZ, USBD_SHORT_XFER_OK, 0, cdcef_rxeof);

	status = usbf_transfer(xfer);
	if (status && status != USBF_IN_PROGRESS) {
		printf("%s: usbf_transfer failed\n", DEVNAME(sc));
		return;
	}
}

struct mbuf *
cdcef_newbuf(void)
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

int
cdcef_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cdcef_softc	*sc = ifp->if_softc;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	int			 s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		cdcef_init(sc);
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				cdcef_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				cdcef_stop(sc);
		}
		error = 0;
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_arpcom, command, data);
	}

	if (error == ENETRESET)
		error = 0;

	splx(s);
	return (error);
}

void
cdcef_watchdog(struct ifnet *ifp)
{
	struct cdcef_softc	*sc = ifp->if_softc;
	int s;

#if 0
	if (sc->sc_dying)
		return;
#endif

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", DEVNAME(sc));

	s = splusb();
	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* cancel receive pipe? */
	usbf_abort_pipe(sc->sc_pipe_in); /* in is tx pipe */
	splx(s);
}

void
cdcef_init(struct cdcef_softc *sc)
{
	int s;
	struct ifnet    *ifp = GET_IFP(sc);
	if (ifp->if_flags & IFF_RUNNING)
		return;
	s = splnet();

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

int
cdcef_encap(struct cdcef_softc *sc, struct mbuf *m, int idx)
{
	usbf_status err;

	m_copydata(m, 0, m->m_pkthdr.len, sc->sc_buffer_in);
	/* NO CRC */

	usbf_setup_xfer(sc->sc_xfer_in, sc->sc_pipe_in, sc, sc->sc_buffer_in,
	    m->m_pkthdr.len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    10000, cdcef_txeof);

	err = usbf_transfer(sc->sc_xfer_in);
	if (err && err != USBD_IN_PROGRESS) {
		printf("encap error\n");
		cdcef_stop(sc);
		return (EIO);
	}
	sc->sc_xmit_mbuf = m;

	return (0);
}


void
cdcef_stop(struct cdcef_softc *sc)
{
	struct ifnet    *ifp = GET_IFP(sc);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* cancel receive pipe? */

	if (sc->sc_xmit_mbuf != NULL) {
		m_freem(sc->sc_xmit_mbuf);
		sc->sc_xmit_mbuf = NULL;
	}
}
