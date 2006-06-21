/*	$OpenBSD: if_zyd.c,v 1.2 2006/06/21 18:49:20 deraadt Exp $	*/

/*
 * Copyright (c) 2006 by Florian Stoehr <ich@florian-stoehr.de>
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
 * ZyDAS ZD1211 USB WLAN driver
 */

#define ZYD_DEBUG

#include <sys/cdefs.h>

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/usbdevs.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#ifdef USB_DEBUG
#define ZYD_DEBUG
//#define ZYD_INTRDUMP
#endif

#include <dev/usb/if_zydreg.h>

/* Debug printf helper macros */
#ifdef ZYD_DEBUG
#define DPRINTF(x)	do { if (zyddebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (zyddebug>(n)) printf x; } while (0)
int zyddebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

USB_DECLARE_DRIVER(zyd);

/*
 * Prototypes
 */
uint16_t zyd_getrealaddr(struct zyd_softc *, uint32_t);
usbd_status zyd_usbrequest(struct zyd_softc *, uint8_t, uint8_t,
	uint16_t, uint16_t, uint16_t, uint8_t *);
usbd_status zyd_usbrequestzc(struct zyd_softc *, struct zyd_control *);
void zyd_reset(struct zyd_softc *);
usbd_status zyd_usb_bulk_read(struct zyd_softc *, void *, uint32_t, uint32_t *);
usbd_status zyd_usb_bulk_write(struct zyd_softc *, void *, uint32_t);
Static void zydintr(usbd_xfer_handle, usbd_private_handle, usbd_status);
int zyd_usb_intr_read(struct zyd_softc *, void *, uint32_t);
usbd_status zyd_usb_intr_write(struct zyd_softc *, void *, uint32_t);
uint32_t zyd_addrinc(uint32_t);
int zyd_singleregread16(struct zyd_softc *, uint32_t, uint16_t *);
int zyd_singleregread32(struct zyd_softc *, uint32_t, uint32_t *);
int zyd_multiregread16(struct zyd_softc *, uint32_t *, uint16_t *, uint8_t);
int zyd_multiregread32(struct zyd_softc *, uint32_t *, uint32_t *, uint8_t);

usbd_status zyd_singleregwrite16(struct zyd_softc *, uint32_t, uint16_t);
usbd_status zyd_singleregwrite32(struct zyd_softc *, uint32_t, uint32_t);
usbd_status zyd_multiregwrite16(struct zyd_softc *, uint32_t *, uint16_t *, uint8_t);
usbd_status zyd_multiregwrite32(struct zyd_softc *, uint32_t *, uint32_t *, uint8_t);
usbd_status zyd_batchwrite16(struct zyd_softc *,
	const struct zyd_adpairs16 *, int);
usbd_status zyd_batchwrite32(struct zyd_softc *,
	const struct zyd_adpairs32 *, int);
usbd_status zyd_rfwrite(struct zyd_softc *, uint32_t, uint8_t);

int zyd_openpipes(struct zyd_softc *);
void zyd_closepipes(struct zyd_softc *);
int zyd_alloc_tx(struct zyd_softc *);
void zyd_free_tx(struct zyd_softc *);
int zyd_alloc_rx(struct zyd_softc *);
void zyd_free_rx(struct zyd_softc *);
void zyd_stateoutput(struct zyd_softc *);
void zyd_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
void zyd_rxframeproc(struct zyd_rx_data *, uint8_t *, uint16_t);
void zyd_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);

int zyd_uploadfirmware(struct zyd_softc *);
void zyd_lock_phy(struct zyd_softc *);
void zyd_unlock_phy(struct zyd_softc *);
usbd_status zyd_get_aw_pt_bi(struct zyd_softc *, struct zyd_aw_pt_bi *);
usbd_status zyd_set_aw_pt_bi(struct zyd_softc *, struct zyd_aw_pt_bi *);
usbd_status zyd_set_beacon_interval(struct zyd_softc *, uint32_t);
const char *zyd_rf_name(uint8_t);
usbd_status zyd_read_rf_pa_types(struct zyd_softc *, uint8_t *, uint8_t *);

usbd_status zyd_rf_rfmd_init(struct zyd_softc *, struct zyd_rf *);
usbd_status zyd_rf_rfmd_switchradio(struct zyd_softc *, uint8_t);
usbd_status zyd_rf_rfmd_set_channel(struct zyd_softc *, struct zyd_rf *, uint8_t);

usbd_status zyd_rf_al2230_init(struct zyd_softc *, struct zyd_rf *);
usbd_status zyd_rf_al2230_switchradio(struct zyd_softc *, uint8_t);
usbd_status zyd_rf_al2230_set_channel(struct zyd_softc *, struct zyd_rf *, uint8_t);

usbd_status zyd_rf_init_hw(struct zyd_softc *, struct zyd_rf *, uint8_t);

usbd_status zyd_hw_init(struct zyd_softc *, struct ieee80211com *);
usbd_status zyd_get_e2p_mac_addr(struct zyd_softc *, struct zyd_macaddr *);
usbd_status zyd_set_mac_addr(struct zyd_softc *, const struct zyd_macaddr *);
usbd_status zyd_read_regdomain(struct zyd_softc *, uint8_t *);
int zyd_regdomain_supported(uint8_t);

int zyd_tblreader(struct zyd_softc *, uint8_t *, size_t, uint32_t, uint32_t);
int zyd_readcaltables(struct zyd_softc *);

int zyd_reset_channel(struct zyd_softc *);
usbd_status zyd_set_encryption_type(struct zyd_softc *, uint32_t);
usbd_status zyd_switch_radio(struct zyd_softc *, uint8_t);
usbd_status zyd_enable_hwint(struct zyd_softc *);
usbd_status zyd_disable_hwint(struct zyd_softc *);
usbd_status zyd_set_basic_rates(struct zyd_softc *, int);
usbd_status zyd_set_mandatory_rates(struct zyd_softc *, int);
usbd_status zyd_reset_mode(struct zyd_softc *);
usbd_status zyd_set_bssid(struct zyd_softc *, uint8_t *);
usbd_status zyd_complete_attach(struct zyd_softc *);
int zyd_media_change(struct ifnet *);
int zyd_newstate(struct ieee80211com *, enum ieee80211_state, int);
int zyd_initial_config(struct zyd_softc *);
void zyd_update_promisc(struct zyd_softc *);
uint16_t zyd_txtime(int, int, uint32_t);
uint8_t zyd_plcp_signal(int);
uint16_t zyd_calc_useclen(int, uint16_t, uint8_t *);

void zyd_setup_tx_desc(struct zyd_softc *, struct zyd_controlsetformat *,
	struct mbuf *, int, int);

int zyd_tx_mgt(struct zyd_softc *, struct mbuf *, struct ieee80211_node *);
int zyd_tx_data(struct zyd_softc *, struct mbuf *, struct ieee80211_node *);
int	zyd_tx_bcn(struct zyd_softc *, struct mbuf *, struct ieee80211_node *);

void zyd_set_chan(struct zyd_softc *, struct ieee80211_channel *);

/* Registered @ if */
int	zyd_if_init(struct ifnet *);
/*void zyd_if_stop(struct ifnet *, int);*/
void zyd_if_start(struct ifnet *);
int	zyd_if_ioctl(struct ifnet *, u_long, caddr_t);
void zyd_if_watchdog(struct ifnet *);

void zyd_next_scan(void *);
void zyd_task(void *);

/*
 * Debug dump
 */
void bindump(uint8_t *, int);
void bindump(uint8_t *ptr, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		DPRINTF(("%02x=%02x ", i, *(ptr + i)));

		if ((i > 0) && (!((i + 1) % 7)))
			DPRINTF(("\n"));
	}

	DPRINTF(("\n"));
}


/*
 * Get the real address from a range-mangled address
 */
uint16_t
zyd_getrealaddr(struct zyd_softc *sc, uint32_t mangled_addr)
{
	uint16_t add;
	uint16_t res;
	uint16_t blubb;

	add = 0;

	switch (ZYD_GET_RANGE(mangled_addr)) {
	case ZYD_RANGE_USB:
		break;

	case ZYD_RANGE_CTL:
		add = ZYD_CTRL_START_ADDR;
		break;

	case ZYD_RANGE_E2P:
		add = ZYD_E2P_START_ADDR;
		break;

	case ZYD_RANGE_FW:
		add = sc->firmware_base;
		break;
	}

	res = (add + ZYD_GET_OFFS(mangled_addr));
	blubb = ZYD_GET_OFFS(mangled_addr);

/*	DPRINTF(("mangled = %x, add = %x, blubb = %x, result = %x\n",
		mangled_addr, add, blubb, res));*/

	return res;
}


/*
 * USB request basic wrapper
 */
usbd_status
zyd_usbrequest(struct zyd_softc *sc, uint8_t type, uint8_t request,
	uint16_t value, uint16_t index, uint16_t length, uint8_t *data)
{
	usb_device_request_t req;
	usbd_xfer_handle xfer;
	usbd_status err;
	int total_len = 0, s;

	req.bmRequestType = type;
	req.bRequest = request;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, length);

#ifdef ZYD_DEBUG
	if (zyddebug) {
		DPRINTFN(20, ("%s: req=%02x val=%02x ind=%02x "
		    "len=%02x\n", USBDEVNAME(sc->zyd_dev), request,
		    value, index, length));
	}
#endif /* ZYD_DEBUG */

	/* Block network interrupts */
	s = splnet();

	xfer = usbd_alloc_xfer(sc->zyd_udev);
	usbd_setup_default_xfer(xfer, sc->zyd_udev, 0, 500000, &req, data,
	    length, USBD_SHORT_XFER_OK, 0);

	err = usbd_sync_transfer(xfer);

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

#ifdef ZYD_DEBUG
	if (zyddebug) {
		if (type & UT_READ) {
			DPRINTFN(20, ("%s: transfered 0x%x bytes in\n",
			    USBDEVNAME(sc->zyd_dev), total_len));
		} else {
			if (total_len != length)
				DPRINTF(("%s: wrote only %x bytes\n",
				    USBDEVNAME(sc->zyd_dev), total_len));
		}
	}
#endif /* ZYD_DEBUG */

	usbd_free_xfer(xfer);

	/* Allow interrupts again */
	splx(s);

	return (err);
}

/*
 * Same, higher level
 */
usbd_status
zyd_usbrequestzc(struct zyd_softc *sc, struct zyd_control *zc)
{
	return zyd_usbrequest(sc, zc->type, zc->id, zc->value,
	    zc->index, zc->length, zc->data);
}

/*
 * Issue a SET_CONFIGURATION command, which will reset the device.
 */
void
zyd_reset(struct zyd_softc *sc)
{
	if (usbd_set_config_no(sc->zyd_udev, ZYD_CONFIG_NO, 1) ||
	    usbd_device2interface_handle(sc->zyd_udev, ZYD_IFACE_IDX, &sc->zyd_iface))
		printf("%s: reset failed\n", USBDEVNAME(sc->zyd_dev));

	/* Wait a little while for the chip to get its brains in order. */
	usbd_delay_ms(sc->zyd_udev, 100);
}

/*
 * Bulk transfer, read
 */
usbd_status
zyd_usb_bulk_read(struct zyd_softc *sc, void *data, uint32_t size,
	uint32_t *readbytes)
{
	usbd_xfer_handle xfer;
	usbd_status err;
	int timeout = 1000;

	xfer = usbd_alloc_xfer(sc->zyd_udev);

	if (xfer == NULL)
		return (EIO);

	err = usbd_bulk_transfer(xfer, sc->zyd_ep[ZYD_ENDPT_BIN], 0,
	    timeout, data, &size, "zydrb");

	usbd_free_xfer(xfer);

	*readbytes = size;

	return (err);
}

/*
 * Bulk transfer, write
 */
usbd_status
zyd_usb_bulk_write(struct zyd_softc *sc, void *data, uint32_t size)
{
	usbd_xfer_handle xfer;
	usbd_status err;
	int timeout = 1000;

	xfer = usbd_alloc_xfer(sc->zyd_udev);

	if (xfer == NULL)
		return (EIO);

	err = usbd_bulk_transfer(xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], 0,
	    timeout, data, &size, "zydwb");

	usbd_free_xfer(xfer);

	return (err);
}

/*
 * Callback handler for interrupt transfer
 */
Static void
zydintr(usbd_xfer_handle xfer, usbd_private_handle thehandle,
	usbd_status status)
{
	struct zyd_softc *sc = thehandle;
	uint32_t count;
/*	char rawbuf[1024];
	char tmpbuf[100];
	int i;*/

	DPRINTF(("zydintr: status=%d\n", status));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->zyd_ep[ZYD_ENDPT_IIN]);

		return;
	}

/*	DPRINTF(("zydintr: getting xfer status\n"));*/
	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);

/*	DPRINTF(("zydintr: xfer=%p status=%d count=%d\n", xfer, status, count));*/

/*	memset(rawbuf, 0, 1024);
	strcpy(rawbuf, "data: ");

	for (i = 0; i < count; ++i) {
		sprintf(tmpbuf, "%d:%02X ", i, *(sc->ibuf + i));
		strcat(rawbuf, tmpbuf);
	}*/

/*	DPRINTF(("zydintr: raw buffer is %s\n", rawbuf));*/

	(void)b_to_q(sc->ibuf, count, &sc->q_reply);

	if (sc->pending) {
		sc->pending = 0;
		DPRINTFN(5, ("zydintr: waking %p\n", sc));
		wakeup(sc);
	}

/*	selnotify(&sc->rsel, 0);*/
}

/*
 * Interrupt call reply transfer, read
 */
int
zyd_usb_intr_read(struct zyd_softc *sc, void *data, uint32_t size)
{
	int s, error;

	error = 0;

	/* Block until we got the interrupt */
	s = splusb();

	while (sc->q_reply.c_cc == 0) {
		/* It was an interrupt, but it is not affecting us */
		sc->pending = 1;
		DPRINTFN(5, ("zyd_usb_intr_read: sleep on %p\n", sc));
		error = tsleep(sc, PZERO | PCATCH, "zydri", 0);
		DPRINTFN(5, ("zyd_usb_intr_read: woke, error=%d\n", error));

		if (error) {
			sc->pending = 0;
			break;
		}
	}

	/*
	 * Unfortunately, the ZD1211 transmits more bytes than
	 * actually requested. Fetch everything in the queue
	 * here. zyd_intr() uses different queues for different
	 * types of data, "q_reply" is always the "reply-to-call"
	 * queue, so it's safe to fetch the whole buffer here,
	 * parallel register read request are not allowed.
	 */

	/*
	 * The buffer contains 2 bytes header, then pairs
	 * of 2 bytes address + 2 bytes data. Plus some
	 * ZD1211-garbage (grrr....).
	 */
	if ((sc->q_reply.c_cc > 0) && (!error))
		q_to_b(&sc->q_reply, data, size);

	/* Flush the queue */
	ndflush(&sc->q_reply, sc->q_reply.c_cc);

	/* Allow all interrupts again */
	splx(s);

	return (error);
}

/*
 * Interrupt transfer, write.
 *
 * Not always an "interrupt transfer", as if operating in
 * full speed mode, EP4 is bulk out, not interrupt out.
 */
usbd_status
zyd_usb_intr_write(struct zyd_softc *sc, void *data, uint32_t size)
{
	usbd_xfer_handle xfer;
	usbd_status err;
	int timeout = 1000;

/*	uint32_t size1 = size;*/

#ifdef ZYD_INTRDUMP
	DPRINTF(("intrwrite raw dump:\n"));
	bindump(data, size);
#endif

	xfer = usbd_alloc_xfer(sc->zyd_udev);

	if (xfer == NULL)
		return (EIO);

	/* Only use the interrupt transfer in high speed mode */
	if (sc->zyd_udev->speed == USB_SPEED_HIGH) {
		err = usbd_intr_transfer(xfer, sc->zyd_ep[ZYD_ENDPT_IOUT], 0,
			timeout, (uint8_t *)data, &size, "zydwi");
	} else {
		err = usbd_bulk_transfer(xfer, sc->zyd_ep[ZYD_ENDPT_IOUT], 0,
			timeout, (uint8_t *)data, &size, "zydwi");
	}

/*	DPRINTF(("zyd_usb_intr_write: err = %d, size = %d, size1 = %d\n",
		err, size, size1));*/

	usbd_free_xfer(xfer);

	return (err);
}

/*
 * Offset correction (all ranges except CTL use word addressing)
 */
uint32_t
zyd_addrinc(uint32_t addr)
{
	uint32_t range = ZYD_GET_RANGE(addr);
	uint32_t offs = ZYD_GET_OFFS(addr);

	offs += (range == ZYD_RANGE_CTL) ? 2 : 1;

	return (range | offs);
}

/*
 * Read a single 16-bit register
 */
int
zyd_singleregread16(struct zyd_softc *sc, uint32_t addr, uint16_t *value)
{
	return zyd_multiregread16(sc, &addr, value, 1);
}

/*
 * Read a single 32-bit register
 */
int
zyd_singleregread32(struct zyd_softc *sc, uint32_t addr, uint32_t *value)
{
	return zyd_multiregread32(sc, &addr, value, 1);
}

/*
 * Read up to 15 16-bit registers (newer firmware versions)
 */
int
zyd_multiregread16(struct zyd_softc *sc, uint32_t *addrs, uint16_t *data,
	uint8_t usecount)
{
	struct zyd_intoutmultiread in;
	struct zyd_intinmultioutput op;
	int i, rv;
	int s;

	memset(&in, 0, sizeof(struct zyd_intoutmultiread));
	memset(&op, 0, sizeof(struct zyd_intinmultioutput));

	USETW(in.id, ZYD_CMD_IORDREQ);

	for (i = 0; i < usecount; i++)
		USETW(in.addr[i], zyd_getrealaddr(sc, addrs[i]));

	s = splnet();
	zyd_usb_intr_write(sc, &in, (2 + (usecount * 2)));
	rv = zyd_usb_intr_read(sc, &op, (2 + (usecount * 4)));
	splx(s);

	for (i = 0; i < usecount; i++) {
		data[i] = UGETW(op.registers[i].data);
	}

	return rv;
}

/*
 * Read up to 7 32-bit registers (newer firmware versions)
 */
int
zyd_multiregread32(struct zyd_softc *sc, uint32_t *addrs, uint32_t *data,
	uint8_t usecount)
{
	struct zyd_intoutmultiread in;
	struct zyd_intinmultioutput op;
	int i, rv;
	int realcount;
	int s;

	realcount = usecount * 2;

	memset(&in, 0, sizeof(struct zyd_intoutmultiread));
	memset(&op, 0, sizeof(struct zyd_intinmultioutput));

	USETW(in.id, ZYD_CMD_IORDREQ);

	for (i = 0; i < usecount; i++) {
		/* high word is first */
		USETW(in.addr[i * 2], zyd_getrealaddr(sc, zyd_addrinc(addrs[i])));
		USETW(in.addr[(i * 2) + 1], zyd_getrealaddr(sc, addrs[i]));
	}

	s = splnet();
	zyd_usb_intr_write(sc, &in, (2 + (realcount * 2)));
	rv = zyd_usb_intr_read(sc, &op, (2 + (realcount * 4)));
	splx(s);

	for (i = 0; i < usecount; i++) {
		data[i] =
		    (UGETW(op.registers[i * 2].data) << 16) |
		    UGETW(op.registers[(i * 2) + 1].data);
	}

	return rv;
}

/*
 * Write a single 16-bit register
 */
usbd_status
zyd_singleregwrite16(struct zyd_softc *sc, uint32_t addr, uint16_t value)
{
	return zyd_multiregwrite16(sc, &addr, &value, 1);
}

/*
 * Write a single 32-bit register
 */
usbd_status
zyd_singleregwrite32(struct zyd_softc *sc, uint32_t addr, uint32_t value)
{
	return zyd_multiregwrite32(sc, &addr, &value, 1);
}

/*
 * Write up to 15 16-bit registers (newer firmware versions)
 */
usbd_status
zyd_multiregwrite16(struct zyd_softc *sc, uint32_t *addrs, uint16_t *data,
	uint8_t usecount)
{
	struct zyd_intoutmultiwrite mw;
	int i;
	int s;
	usbd_status rw;

	memset(&mw, 0, sizeof(struct zyd_intoutmultiwrite));

	USETW(mw.id, ZYD_CMD_IOWRREQ);

	for (i = 0; i < usecount; i++) {
		USETW(mw.registers[i].addr, zyd_getrealaddr(sc, addrs[i]));
		USETW(mw.registers[i].data, data[i]);
	}

	s = splnet();
	rw = zyd_usb_intr_write(sc, &mw, (2 + (usecount * 4)));
	splx(s);

	return rw;
}

/*
 * Write up to 7 32-bit registers (newer firmware versions)
 */
usbd_status
zyd_multiregwrite32(struct zyd_softc *sc, uint32_t *addrs, uint32_t *data,
	uint8_t usecount)
{
	struct zyd_intoutmultiwrite mw;
	int i;
	int realcount;
	int s;
	usbd_status rw;

	realcount = usecount * 2;

	memset(&mw, 0, sizeof(struct zyd_intoutmultiwrite));

	USETW(mw.id, ZYD_CMD_IOWRREQ);

	for (i = 0; i < usecount; i++) {
		/* high word is first */
		USETW(mw.registers[i * 2].addr, zyd_getrealaddr(sc, zyd_addrinc(addrs[i])));
		USETW(mw.registers[i * 2].data, (*data >> 16));

		USETW(mw.registers[(i * 2) + 1].addr, zyd_getrealaddr(sc, addrs[i]));
		USETW(mw.registers[(i * 2) + 1].data, (*data));
	}

	s = splnet();
	rw = zyd_usb_intr_write(sc, &mw, (2 + (realcount * 4)));
	splx(s);

	return rw;
}

/*
 * Batch write 16-bit data
 */
usbd_status
zyd_batchwrite16(struct zyd_softc *sc, const struct zyd_adpairs16 *data,
	int count)
{
	/* TODO: Optimize, use multi-writer */
	usbd_status rv;
	int i;

	rv = 0;

/*	DPRINTF(("zyd_batchwrite16: %d items\n", count));*/

	for (i = 0; i < count; i++) {
/*		DPRINTF(("zyd_batchwrite16: item %d: @%x -> %02x\n", i, data[i].addr, data[i].data));*/
		rv = zyd_singleregwrite16(sc, data[i].addr, data[i].data);

		if (rv)
			break;
	}

	return rv;
}

/*
 * Batch write 32-bit data
 */
usbd_status
zyd_batchwrite32(struct zyd_softc *sc, const struct zyd_adpairs32 *data,
	int count)
{
	/* TODO: Optimize, use multi-writer */
	usbd_status rv;
	int i;

	rv = 0;

/*	DPRINTF(("zyd_batchwrite32: %d items\n", count));*/

	for (i = 0; i < count; i++) {
/*		DPRINTF(("zyd_batchwrite32: item %d: @%x -> %08x\n", i, data[i].addr, data[i].data));*/
		rv = zyd_singleregwrite32(sc, data[i].addr, data[i].data);

		if (rv)
			break;
	}

	return rv;
}

/*
 * Write RF registers
 */
usbd_status
zyd_rfwrite(struct zyd_softc *sc, uint32_t value, uint8_t bits)
{
	struct zyd_req_rfwrite *req = NULL;
	int len, i;
	uint16_t bw_template;
	usbd_status rv;

	DPRINTF(("Entering zyd_rfwrite()\n"));

	rv = zyd_singleregread16(sc, ZYD_CR203, &bw_template);

	if (rv)
		goto leave;

	/* Clear template */
	bw_template &= ~(ZYD_RF_IF_LE | ZYD_RF_CLK | ZYD_RF_DATA);

	len = sizeof(struct zyd_req_rfwrite) + (bits * sizeof(uWord));
	req = malloc(len, M_TEMP, M_WAITOK);

	if (!req)
		return USBD_NOMEM;

	USETW(req->id, ZYD_CMD_RFCFGREQ);
	USETW(req->value, 2);
	USETW(req->bits, bits);

	for (i = 0; i < bits; i++) {
		uint16_t bv = bw_template;

		if (value & (1 << (bits - 1 - i)))
			bv |= ZYD_RF_DATA;

		USETW(req->bit_values[i], bv);
	}

	rv = zyd_usb_intr_write(sc, req, len);

	free(req, M_TEMP);

	DPRINTF(("Finished zyd_rfwrite(): rv = %d, wrote %d bits\n", rv, bits));

leave:
	return rv;
}

/*
 * Open bulk and interrupt pipes
 */
int
zyd_openpipes(struct zyd_softc *sc)
{
	usbd_status err;
	int isize;
	usb_endpoint_descriptor_t *edesc;

	/* Interrupt in */
	edesc = usbd_interface2endpoint_descriptor(sc->zyd_iface, ZYD_ENDPT_IIN);

	isize = UGETW(edesc->wMaxPacketSize);

	if (isize == 0)	/* shouldn't happen */
		return (EINVAL);

	sc->ibuf = malloc(isize, M_USBDEV, M_WAITOK);
			
	if (clalloc(&sc->q_reply, 1024, 0) == -1)
		return (ENOMEM);

	err = usbd_open_pipe_intr(sc->zyd_iface, sc->zyd_ed[ZYD_ENDPT_IIN],
	    USBD_SHORT_XFER_OK, &sc->zyd_ep[ZYD_ENDPT_IIN],
	    sc, sc->ibuf, isize, zydintr, USBD_DEFAULT_INTERVAL);

	if (err) {
		free(sc->ibuf, M_USBDEV);
		clfree(&sc->q_reply);
		return (EIO);
	}

	/* Interrupt out (not neccessary really an interrupt pipe) */
	err = usbd_open_pipe(sc->zyd_iface, sc->zyd_ed[ZYD_ENDPT_IOUT],
	    0, &sc->zyd_ep[ZYD_ENDPT_IOUT]);

	if (err) {
		free(sc->ibuf, M_USBDEV);
		clfree(&sc->q_reply);
		return (EIO);
	}

	/* Bulk in */
	err = usbd_open_pipe(sc->zyd_iface, sc->zyd_ed[ZYD_ENDPT_BIN],
	    0, &sc->zyd_ep[ZYD_ENDPT_BIN]);

	if (err) {
		free(sc->ibuf, M_USBDEV);
		clfree(&sc->q_reply);
		return (EIO);
	}

	/* Bulk out */
	err = usbd_open_pipe(sc->zyd_iface, sc->zyd_ed[ZYD_ENDPT_BOUT],
	    0, &sc->zyd_ep[ZYD_ENDPT_BOUT]);

	if (err) {
		free(sc->ibuf, M_USBDEV);
		clfree(&sc->q_reply);
		return (EIO);
	}

	return 0;
}

/*
 * Close bulk and interrupt pipes
 */
void
zyd_closepipes(struct zyd_softc *sc)
{
	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_IIN]);
	usbd_close_pipe(sc->zyd_ep[ZYD_ENDPT_IIN]);

	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_IOUT]);
	usbd_close_pipe(sc->zyd_ep[ZYD_ENDPT_IOUT]);

	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BIN]);
	usbd_close_pipe(sc->zyd_ep[ZYD_ENDPT_BIN]);

	usbd_abort_pipe(sc->zyd_ep[ZYD_ENDPT_BOUT]);
	usbd_close_pipe(sc->zyd_ep[ZYD_ENDPT_BOUT]);

	ndflush(&sc->q_reply, sc->q_reply.c_cc);
	clfree(&sc->q_reply);

	free(sc->ibuf, M_USBDEV);
	sc->ibuf = NULL;
}

/*
 * Allocate TX list
 */
int
zyd_alloc_tx(struct zyd_softc *sc)
{
	struct zyd_tx_data *data;
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		data = &sc->tx_data[i];
		data->sc = sc;
		data->xfer = usbd_alloc_xfer(sc->zyd_udev);

		if (data->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}

		data->buf = usbd_alloc_buffer(data->xfer,
			ZYD_TX_DESC_SIZE + MCLBYTES);

		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}
	}

	return 0;

fail:
	zyd_free_tx(sc);
	return error;
}

/*
 * Free TX list
 */
void zyd_free_tx(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_tx_data *data;
	int i;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
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

/*
 * Allocate RX list
 */
int zyd_alloc_rx(struct zyd_softc *sc)
{
	struct zyd_rx_data *data;
	int i, error;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		data = &sc->rx_data[i];

		data->sc = sc;

		data->xfer = usbd_alloc_xfer(sc->zyd_udev);

		if (data->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}

		if (usbd_alloc_buffer(data->xfer, MCLBYTES) == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);

		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);

		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    USBDEVNAME(sc->zyd_dev));
			error = ENOMEM;
			goto fail;
		}

		data->buf = mtod(data->m, uint8_t *);
	}

	return 0;

fail:
	zyd_free_tx(sc);
	return error;
}

/*
 * Free RX list
 */
void zyd_free_rx(struct zyd_softc *sc)
{
	struct zyd_rx_data *data;
	int i;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
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

/*
 * Fetch and print state flags of zydas
 */
void zyd_stateoutput(struct zyd_softc *sc)
{
	uint32_t debug;

	DPRINTF(("In zyd_stateoutput()\n"));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6D4), &debug);
	DPRINTF(("DEBUG: Tx complete: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6F4), &debug);
	DPRINTF(("DEBUG: Tx total packet: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x69C), &debug);
	DPRINTF(("DEBUG: Rx timeout count: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6A0), &debug);
	DPRINTF(("DEBUG: Rx total frame count: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6A4), &debug);
	DPRINTF(("DEBUG: Rx CRC32: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6A8), &debug);
	DPRINTF(("DEBUG: Rx CRC16: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6AC), &debug);
	DPRINTF(("DEBUG: Rx unicast decr error: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6B0), &debug);
	DPRINTF(("DEBUG: Rx FIFO overrun: %x\n", debug));

	debug = 0;
	zyd_singleregread32(sc, ZYD_REG_CTL(0x6BC), &debug);
	DPRINTF(("DEBUG: Rx multicast decr error: %x\n", debug));
}

/*
 * EOF handler for TX transfer
 */
void
zyd_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_tx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	DPRINTF(("Entering zyd_txeof()\n"));

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    USBDEVNAME(sc->zyd_dev), usbd_errstr(status));

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->zyd_ep[ZYD_ENDPT_BOUT]);

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

	sc->tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
/*	zyd_if_start(ifp);*/

	splx(s);

	DPRINTF(("Leaving zyd_txeof()\n"));
}

/*
 * RX frame processor.
 *
 * Needed because rxeof might fetch multiple frames
 * inside a single USB transfer.
 */
void
zyd_rxframeproc(struct zyd_rx_data *data, uint8_t *buf, uint16_t len)
{
	struct zyd_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct zyd_rxstatusreport *desc;
	struct ieee80211_node *ni;
	struct mbuf *m;
	uint8_t *useptr;
	int s;
	int optype;

	/* Too small for at least an RX status report? */
	if (len < ZYD_RX_DESC_SIZE) {
		printf("%s: xfer too short %d\n", USBDEVNAME(sc->zyd_dev), len);
		ifp->if_ierrors++;
		goto skip;
	}

	/* An RX status report is appended */
	desc = (struct zyd_rxstatusreport *)(buf + len - ZYD_RX_DESC_SIZE);

	/*
	 * TODO: Signal strength and quality have to be calculated in
	 * conjunction with the PLCP header! The printed values are RAW!
	 */

	/* Print RX debug info */
	DPRINTF(("Rx status: signalstrength = %d, signalqualitycck = %d, "
	    "signalqualityofdm = %d, decryptiontype = %d, "
	    "modulationtype = %d, rxerrorreason = %d, errorindication = %d\n",
	    desc->signalstrength, desc->signalqualitycck, desc->signalqualityofdm,
	    desc->decryptiontype, desc->modulationtype, desc->rxerrorreason,
	    desc->errorindication));

	/* Bad frame? */
	if (desc->errorindication) {
		DPRINTF(("RX status indicated error\n"));
		ifp->if_ierrors++;
		goto skip;
	}

	/* Setup a new mbuf for this */
	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    USBDEVNAME(sc->zyd_dev));
		return;
	}

	MCLGET(m, M_DONTWAIT);

	if (!(m->m_flags & M_EXT)) {
		printf("%s: could not allocate rx mbuf cluster\n",
		    USBDEVNAME(sc->zyd_dev));
		m_freem(m);
		m = NULL;
		return;
	}

	useptr = mtod(m, uint8_t *);
	memcpy(useptr, buf, len);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len - ZYD_RX_DESC_SIZE;
	m->m_flags |= M_HASFCS; /* hardware appends FCS */

	s = splnet();

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);
	ieee80211_input(ifp, m, ni, desc->signalstrength, 0);
	ieee80211_release_node(ic, ni);

	DPRINTF(("iee80211_input() -> %d\n", optype));

	splx(s);

skip:
	;
}

/*
 * EOF handler for RX transfer
 */
void
zyd_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_rx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int len;
	struct zyd_rxleninfoapp *leninfoapp;
/*	int i;
	uint16_t tfs;*/

	DPRINTF(("Entering zyd_rxeof\n"));

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->zyd_ep[ZYD_ENDPT_BIN]);

		goto skip;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	DPRINTF(("zyd_rxeof: Len = %d\n", len));
	DPRINTF(("zyd_rxeof: Raw dump follows\n"));

	bindump(data->buf, len);

	/*
	 * It must be at least 4 bytes - still broken if it is
	 * 4 bytes, but that's enough to hold the multi-frame
	 * append header
	 */
	if (len < sizeof(struct zyd_rxleninfoapp)) {
		printf("%s: xfer too short %d\n", USBDEVNAME(sc->zyd_dev), len);
		ifp->if_ierrors++;
		goto skip;
	}

	/* See whether this is a multi-frame tansmission */
	leninfoapp = (struct zyd_rxleninfoapp *)
		(data->buf + len - sizeof(struct zyd_rxleninfoapp));

	if (UGETW(leninfoapp->marker) == ZYD_MULTIFRAME_MARKER) {
		/* Multiframe received */
		DPRINTF(("Received multi-frame transmission\n"));

		/* TODO: Support 'em properly */

		/* Append PLCP header size */
/*		tfs = ZYD_PLCP_HDR_SIZE;

		for (i = 0; i < 3; ++i) {
			uint16_t tfl = UGETW(leninfoapp->len[i]);

			zyd_rxframeproc(data, data->buf + tfs, tfl);
			tfs += tfl;
		}*/

		goto skip;

	} else {
		DPRINTF(("Received single-frame transmission\n"));
		zyd_rxframeproc(data, data->buf + ZYD_PLCP_HDR_SIZE,
		    len - ZYD_PLCP_HDR_SIZE);
	}

	/* Reestablish the buf for the next round */
	MGETHDR(data->m, M_DONTWAIT, MT_DATA);

	if (data->m == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    USBDEVNAME(sc->zyd_dev));
		return;
	}

	MCLGET(data->m, M_DONTWAIT);

	if (!(data->m->m_flags & M_EXT)) {
		printf("%s: could not allocate rx mbuf cluster\n",
		    USBDEVNAME(sc->zyd_dev));
		m_freem(data->m);
		data->m = NULL;
		return;
	}

	data->buf = mtod(data->m, uint8_t *);

	DPRINTF(("Leaving zyd_rxeof()\n"));

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data, data->buf,
	    MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, zyd_rxeof);
	usbd_transfer(xfer);
}

/*
 * Upload firmware to device.
 *
 * Returns nozero on error.
 *
 * The whole upload procedure was implemented accordingly to
 * what ZyDAS' Linux driver does. It does however *NOT* match
 * what their documentation says (argh...)!
 */
int
zyd_uploadfirmware(struct zyd_softc *sc)
{
	/* ZD1211 uses a proprietary "setup" command to upload the fw */
	struct zyd_control zc;
	uint8_t stsresult;
	int result;
	size_t imgsize;
	u_char *imgptr, *imgptr0;

	memset(&zc, 0, sizeof(struct zyd_control));
	zc.type = ZYD_FIRMDOWN_REQ;
	zc.id = ZYD_FIRMDOWN_ID;
	zc.value = ZYD_FIRMWARE_START_ADDR; /* TODO: Different on old ones! */

	result = loadfirmware("zd1211", &imgptr0, &imgsize);

	if (result) {
		printf("%s: failed loadfirmware of file %s: errno %d\n",
		    USBDEVNAME(sc->zyd_dev), "zyd", result);

		return -1;
	}

	imgptr = imgptr0;

	DPRINTF(("Firmware upload: imgsize=%d\n", imgsize));

	/* Issue upload command(s) */
	while (imgsize > 0) {
		/* Transfer 4KB max */
		int tlen = (imgsize > 4096) ? 4096 : imgsize;

		DPRINTF(("Firmware upload: tlen=%d, value=%x\n", tlen, zc.value));

		zc.length = tlen;
		zc.data = imgptr;

		if (zyd_usbrequestzc(sc, &zc) != USBD_NORMAL_COMPLETION) {
			printf("%s: Error: Cannot upload firmware to device\n",
			    USBDEVNAME(sc->zyd_dev));

			result = -1;
			goto cleanup;
		};

		imgsize -= tlen;
		imgptr += tlen;

		zc.value += (uint16_t)(tlen / 2); /* Requires word */
	};

	/* See whether the upload succeeded */
	memset(&zc, 0, sizeof(struct zyd_control));
	zc.type = ZYD_FIRMSTAT_REQ;
	zc.id = ZYD_FIRMSTAT_ID;
	zc.value = 0;
	zc.length = 1;
	zc.data = &stsresult;

	if (zyd_usbrequestzc(sc, &zc) != USBD_NORMAL_COMPLETION) {
		printf("%s: Error: Cannot check for proper firmware upload\n",
		    USBDEVNAME(sc->zyd_dev));

		result = -1;
		goto cleanup;
	};

	/* Firmware successfully uploaded? */
	if (stsresult == ZYD_FIRMWAREUP_FAILURE) {
		printf("%s: Error: Firmware upload failed: 0x%X\n",
		    USBDEVNAME(sc->zyd_dev), stsresult);

		result = -1;
		goto cleanup;
	} else {
		DPRINTF(("%s: Firmware successfully uploaded\n",
		    USBDEVNAME(sc->zyd_dev)));
	}

	result = 0;

cleanup:
	free(imgptr0, M_DEVBUF);

	return result;
}

/*
 * Driver OS interface
 */

/*
 * Probe for a ZD1211-containing product
 */
USB_MATCH(zyd)
{
	USB_MATCH_START(zyd, uaa);
	int i;

	if (!uaa->iface)
		return (UMATCH_NONE);

	for (i = 0; i < sizeof(zyd_devs)/sizeof(zyd_devs[0]); i++) {
		struct zyd_type *t = &zyd_devs[i];

		if ((uaa->vendor == t->vid) && (uaa->product == t->pid)) {
			return (UMATCH_VENDOR_PRODUCT);
		}
	}

	return (UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(zyd)
{
	USB_ATTACH_START(zyd, sc, uaa);
	char *devinfop;
	usbd_status err;
	usbd_device_handle dev = uaa->device;
	usb_device_descriptor_t* ddesc;

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->zyd_dev), devinfop);
	usbd_devinfo_free(devinfop);

	ddesc = usbd_get_device_descriptor(dev);

	if (UGETW(ddesc->bcdDevice) != ZYD_ALLOWED_DEV_VERSION) {
		printf("%s: device version mismatch: 0x%X (only 43.30 supported)\n",
		    USBDEVNAME(sc->zyd_dev), UGETW(ddesc->bcdDevice));

		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_set_config_no(dev, ZYD_CONFIG_NO, 1);

	if (err) {
		printf("%s: setting config no failed\n", USBDEVNAME(sc->zyd_dev));

		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_device2interface_handle(dev, ZYD_IFACE_IDX, &sc->zyd_iface);

	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->zyd_dev));

		USB_ATTACH_ERROR_RETURN;
	}

	sc->zyd_unit = self->dv_unit;
	sc->zyd_udev = dev;

	/* Now upload the firmware */
	if (zyd_uploadfirmware(sc) != 0)
		USB_ATTACH_ERROR_RETURN;

	DPRINTF(("Setting debug flags\n"));
	/* TODO: What about debugging flags in OpenBSD? */
/*	sc->sc_ic.ic_debug = IEEE80211_MSG_ANY;*/  /* <<<--- this is the NetBSD version */

	/* Perform a device reset */
	zyd_reset(sc);

	/* Complete the attach process (hardware init) */
	if (zyd_complete_attach(sc) != 0)
		USB_ATTACH_ERROR_RETURN;

	USB_ATTACH_SUCCESS_RETURN;
}

/*
 * Lock PHY registers
 */
void
zyd_lock_phy(struct zyd_softc *sc)
{
	uint32_t temp;

	zyd_singleregread32(sc, ZYD_MAC_MISC, &temp);
	temp &= ~ZYD_UNLOCK_PHY_REGS;
	zyd_singleregwrite32(sc, ZYD_MAC_MISC, temp);
}

/*
 * Unlock PHY registers
 */
void
zyd_unlock_phy(struct zyd_softc *sc)
{
	uint32_t temp;

	zyd_singleregread32(sc, ZYD_MAC_MISC, &temp);
	temp |= ZYD_UNLOCK_PHY_REGS;
	zyd_singleregwrite32(sc, ZYD_MAC_MISC, temp);
}

/*
 * Helper beacon (get)
 */
usbd_status
zyd_get_aw_pt_bi(struct zyd_softc *sc, struct zyd_aw_pt_bi *s)
{
	static uint32_t addrs[] =
	    { ZYD_CR_ATIM_WND_PERIOD, ZYD_CR_PRE_TBTT, ZYD_CR_BCN_INTERVAL };
	uint32_t values[3];
	usbd_status rv;

	rv = zyd_multiregread32(sc, addrs, values, 3);

	if (rv) {
		memset(s, 0, sizeof(*s));
	} else {
		s->atim_wnd_period = values[0];
		s->pre_tbtt = values[1];
		s->beacon_interval = values[2];
		DPRINTF(("aw %u pt %u bi %u\n", s->atim_wnd_period,
		    s->pre_tbtt, s->beacon_interval));
	}

	return rv;
}

/*
 * Helper beacon (set)
 */
usbd_status
zyd_set_aw_pt_bi(struct zyd_softc *sc, struct zyd_aw_pt_bi *s)
{
	static uint32_t addrs[] =
	    { ZYD_CR_ATIM_WND_PERIOD, ZYD_CR_PRE_TBTT, ZYD_CR_BCN_INTERVAL };
	uint32_t data[3];

	if (s->beacon_interval <= 5)
		s->beacon_interval = 5;

	if (s->pre_tbtt < 4 || s->pre_tbtt >= s->beacon_interval)
		s->pre_tbtt = s->beacon_interval - 1;

	if (s->atim_wnd_period >= s->pre_tbtt)
		s->atim_wnd_period = s->pre_tbtt - 1;

	data[0] = s->atim_wnd_period;
	data[1] = s->pre_tbtt;
	data[2] = s->beacon_interval;

	return zyd_multiregwrite32(sc, addrs, data, 3);
}

/*
 * Set beacon interval
 */
usbd_status
zyd_set_beacon_interval(struct zyd_softc *sc, uint32_t interval)
{
	struct zyd_aw_pt_bi s;
	usbd_status rv;

	rv = zyd_get_aw_pt_bi(sc, &s);

	if (rv)
		goto out;

	s.beacon_interval = interval;
	rv = zyd_set_aw_pt_bi(sc, &s);

out:
	return rv;
}

/*
 * Get RF name
 */
const char *
zyd_rf_name(uint8_t type)
{
	if (type & 0xf0)
		type = 0;

	return zyd_rfs[type];
}

/*
 * Read RF PA types
 */
usbd_status
zyd_read_rf_pa_types(struct zyd_softc *sc, uint8_t *rf_type,
	uint8_t *pa_type)
{
	uint32_t value;
	uint8_t rf, pa;
	usbd_status rv;

	rf = pa = 0;

	rv = zyd_singleregread32(sc, ZYD_E2P_POD, &value);

	if (!rv) {
		rf = value & 0x0f;
		pa = (value >> 16) & 0x0f;

		printf("%s: Radio %s (%#01x), PA %#01x\n",
		    USBDEVNAME(sc->zyd_dev), zyd_rf_name(rf), rf, pa);
	}

	*rf_type = rf;
	*pa_type = pa;

	return rv;
}

/*
 * RF driver: Init for RFMD chip
 */
usbd_status
zyd_rf_rfmd_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	/* Copied nearly verbatim from the Linux driver rewrite */
	static const struct zyd_adpairs16 ir1[] = {
		{ ZYD_CR2,   0x1E }, { ZYD_CR9,   0x20 }, { ZYD_CR10,  0x89 },
		{ ZYD_CR11,  0x00 }, { ZYD_CR15,  0xD0 }, { ZYD_CR17,  0x68 },
		{ ZYD_CR19,  0x4a }, { ZYD_CR20,  0x0c }, { ZYD_CR21,  0x0E },
		{ ZYD_CR23,  0x48 },
		/* normal size for cca threshold */
		{ ZYD_CR24,  0x14 },
		/* { ZYD_CR24,  0x20 }, */
		{ ZYD_CR26,  0x90 }, { ZYD_CR27,  0x30 }, { ZYD_CR29,  0x20 },
		{ ZYD_CR31,  0xb2 }, { ZYD_CR32,  0x43 }, { ZYD_CR33,  0x28 },
		{ ZYD_CR38,  0x30 }, { ZYD_CR34,  0x0f }, { ZYD_CR35,  0xF0 },
		{ ZYD_CR41,  0x2a }, { ZYD_CR46,  0x7F }, { ZYD_CR47,  0x1e },
		{ ZYD_CR51,  0xc5 }, { ZYD_CR52,  0xc5 }, { ZYD_CR53,  0xc5 },
		{ ZYD_CR79,  0x58 }, { ZYD_CR80,  0x30 }, { ZYD_CR81,  0x30 },
		{ ZYD_CR82,  0x00 }, { ZYD_CR83,  0x24 }, { ZYD_CR84,  0x04 },
		{ ZYD_CR85,  0x00 }, { ZYD_CR86,  0x10 }, { ZYD_CR87,  0x2A },
		{ ZYD_CR88,  0x10 }, { ZYD_CR89,  0x24 }, { ZYD_CR90,  0x18 },
		/* { ZYD_CR91,  0x18 }, */
		/* should solve continous CTS frame problems */
		{ ZYD_CR91,  0x00 },
		{ ZYD_CR92,  0x0a }, { ZYD_CR93,  0x00 }, { ZYD_CR94,  0x01 },
		{ ZYD_CR95,  0x00 }, { ZYD_CR96,  0x40 }, { ZYD_CR97,  0x37 },
		{ ZYD_CR98,  0x05 }, { ZYD_CR99,  0x28 }, { ZYD_CR100, 0x00 },
		{ ZYD_CR101, 0x13 }, { ZYD_CR102, 0x27 }, { ZYD_CR103, 0x27 },
		{ ZYD_CR104, 0x18 }, { ZYD_CR105, 0x12 },
		/* normal size */
		{ ZYD_CR106, 0x1a },
		/* { ZYD_CR106, 0x22 }, */
		{ ZYD_CR107, 0x24 }, { ZYD_CR108, 0x0a }, { ZYD_CR109, 0x13 },
		{ ZYD_CR110, 0x2F }, { ZYD_CR111, 0x27 }, { ZYD_CR112, 0x27 },
		{ ZYD_CR113, 0x27 }, { ZYD_CR114, 0x27 }, { ZYD_CR115, 0x40 },
		{ ZYD_CR116, 0x40 }, { ZYD_CR117, 0xF0 }, { ZYD_CR118, 0xF0 },
		{ ZYD_CR119, 0x16 },
		/* no TX continuation */
		{ ZYD_CR122, 0x00 },
		/* { ZYD_CR122, 0xff }, */
		{ ZYD_CR127, 0x03 }, { ZYD_CR131, 0x08 }, { ZYD_CR138, 0x28 },
		{ ZYD_CR148, 0x44 }, { ZYD_CR150, 0x10 }, { ZYD_CR169, 0xBB },
		{ ZYD_CR170, 0xBB }
	};

	static const uint32_t ir2[] = {
		0x000007,  /* REG0(CFG1) */
		0x07dd43,  /* REG1(IFPLL1) */
		0x080959,  /* REG2(IFPLL2) */
		0x0e6666,
		0x116a57,  /* REG4 */
		0x17dd43,  /* REG5 */
		0x1819f9,  /* REG6 */
		0x1e6666,
		0x214554,
		0x25e7fa,
		0x27fffa,
		/* The Zydas driver somehow forgets to set this value. It's
		 * only set for Japan. We are using internal power control
		 * for now.
		 */
		0x294128, /* internal power */
		/* 0x28252c, */ /* External control TX power */
		/* CR31_CCK, CR51_6-36M, CR52_48M, CR53_54M */
		0x2c0000,
		0x300000,
		0x340000,  /* REG13(0xD) */
		0x381e0f,  /* REG14(0xE) */
		/* Bogus, RF2959's data sheet doesn't know register 27, which is
		 * actually referenced here.
		 */
		0x6c180f  /* REG27(0x11) */
	};

	int i;
	usbd_status rv;

	DPRINTF(("rf_init(): ir1 = %d, ir2 = %d\n",
	    (sizeof(ir1) / sizeof(struct zyd_adpairs16)),
	    (sizeof(ir2) / sizeof(uint32_t))));

	rv = zyd_batchwrite16(sc, ir1, (sizeof(ir1) / sizeof(struct zyd_adpairs16)));

	if (rv)
		return rv;

	for (i = 0; i < (sizeof(ir2) / sizeof(uint32_t)); i++) {
		rv = zyd_rfwrite(sc, ir2[i], ZYD_RF_RV_BITS);

		if (rv)
			break;
	}

	DPRINTF(("rf_init(). rv = %d\n", rv));

	return rv;
}

/*
 * RF driver: Switch radio on/off for RFMD chip
 */
usbd_status
zyd_rf_rfmd_switchradio(struct zyd_softc *sc, uint8_t onoff)
{
	static const struct zyd_adpairs16 ir_on[] = {
		{ ZYD_CR10, 0x89 },
		{ ZYD_CR11, 0x00 }
	};

	static const struct zyd_adpairs16 ir_off[] = {
		{ ZYD_CR10, 0x15 },
		{ ZYD_CR11, 0x81 }
	};

	if (onoff)
		return zyd_batchwrite16(sc, ir_on, (sizeof(ir_on) /
		    sizeof(struct zyd_adpairs16)));

	return zyd_batchwrite16(sc, ir_off, (sizeof(ir_off) /
	    sizeof(struct zyd_adpairs16)));
}

/*
 * RF driver: Channel setting for RFMD chip
 */
usbd_status
zyd_rf_rfmd_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
	uint8_t channel)
{
	static const uint32_t rfmd_table[][2] = {
		{ 0x181979, 0x1e6666 },
		{ 0x181989, 0x1e6666 },
		{ 0x181999, 0x1e6666 },
		{ 0x1819a9, 0x1e6666 },
		{ 0x1819b9, 0x1e6666 },
		{ 0x1819c9, 0x1e6666 },
		{ 0x1819d9, 0x1e6666 },
		{ 0x1819e9, 0x1e6666 },
		{ 0x1819f9, 0x1e6666 },
		{ 0x181a09, 0x1e6666 },
		{ 0x181a19, 0x1e6666 },
		{ 0x181a29, 0x1e6666 },
		{ 0x181a39, 0x1e6666 },
		{ 0x181a60, 0x1c0000 }
	};

	const uint32_t *dp;
	int i;
	usbd_status rv;

	DPRINTF(("Entering zyd_rf_rfmd_set_channel()\n"));

	dp = rfmd_table[channel - 1];

	for (i = 0; i < 2; i++) {
		rv = zyd_rfwrite(sc, dp[i], ZYD_RF_RV_BITS);

		if (rv)
			break;
	}

	DPRINTF(("Finished zyd_rf_rfmd_set_channel()\n"));

	return rv;
}

/*
 * RF driver: Switch radio on/off for AL2230 chip
 */
usbd_status
zyd_rf_al2230_switchradio(struct zyd_softc *sc, uint8_t onoff)
{
	return 0;
}

/*
 * RF driver: Init for AL2230 chip
 */
usbd_status
zyd_rf_al2230_init(struct zyd_softc *sc, struct zyd_rf *rf)
{
	return 0;
}

/*
 * RF driver: Channel setting for AL2230 chip
 */
usbd_status
zyd_rf_al2230_set_channel(struct zyd_softc *sc, struct zyd_rf *rf,
	uint8_t channel)
{
	return 0;
}

/*
 * Assign drivers and init the RF
 */
usbd_status
zyd_rf_init_hw(struct zyd_softc *sc, struct zyd_rf *rf, uint8_t type)
{
	int rv;

	switch (type) {
	case ZYD_RF_RFMD:
		rf->init_hw = zyd_rf_rfmd_init;
		rf->switch_radio = zyd_rf_rfmd_switchradio;
		rf->set_channel = zyd_rf_rfmd_set_channel;
		break;

	case ZYD_RF_AL2230:
		rf->init_hw = zyd_rf_al2230_init;
		rf->switch_radio = zyd_rf_al2230_switchradio;
		rf->set_channel = zyd_rf_al2230_set_channel;
		break;

	default:
		printf("%s: Sorry, radio %s is not supported yet\n",
		    USBDEVNAME(sc->zyd_dev), zyd_rf_name(type));
		rf->type = 0;
		rv = USBD_INVAL;
		goto leave;
	}

	rf->flags = 0;
	rf->type = type;

	zyd_lock_phy(sc);
	rv = rf->init_hw(sc, rf);
	zyd_unlock_phy(sc);

leave:
	return rv;
}

/*
 * Init the hardware
 */
usbd_status
zyd_hw_init(struct zyd_softc *sc, struct ieee80211com *ic)
{
	/* Copied nearly verbatim from the Linux driver rewrite */
	static const struct zyd_adpairs16 ir1[] = {
		{ ZYD_CR0,   0x0a }, { ZYD_CR1,   0x06 }, { ZYD_CR2,   0x26 },
		{ ZYD_CR3,   0x38 }, { ZYD_CR4,   0x80 }, { ZYD_CR9,   0xa0 },
		{ ZYD_CR10,  0x81 }, { ZYD_CR11,  0x00 }, { ZYD_CR12,  0x7f },
		{ ZYD_CR13,  0x8c }, { ZYD_CR14,  0x80 }, { ZYD_CR15,  0x3d },
		{ ZYD_CR16,  0x20 }, { ZYD_CR17,  0x1e }, { ZYD_CR18,  0x0a },
		{ ZYD_CR19,  0x48 }, { ZYD_CR20,  0x0c }, { ZYD_CR21,  0x0c },
		{ ZYD_CR22,  0x23 }, { ZYD_CR23,  0x90 }, { ZYD_CR24,  0x14 },
		{ ZYD_CR25,  0x40 }, { ZYD_CR26,  0x10 }, { ZYD_CR27,  0x19 },
		{ ZYD_CR28,  0x7f }, { ZYD_CR29,  0x80 }, { ZYD_CR30,  0x4b },
		{ ZYD_CR31,  0x60 }, { ZYD_CR32,  0x43 }, { ZYD_CR33,  0x08 },
		{ ZYD_CR34,  0x06 }, { ZYD_CR35,  0x0a }, { ZYD_CR36,  0x00 },
		{ ZYD_CR37,  0x00 }, { ZYD_CR38,  0x38 }, { ZYD_CR39,  0x0c },
		{ ZYD_CR40,  0x84 }, { ZYD_CR41,  0x2a }, { ZYD_CR42,  0x80 },
		{ ZYD_CR43,  0x10 }, { ZYD_CR44,  0x12 }, { ZYD_CR46,  0xff },
		{ ZYD_CR47,  0x08 }, { ZYD_CR48,  0x26 }, { ZYD_CR49,  0x5b },
		{ ZYD_CR64,  0xd0 }, { ZYD_CR65,  0x04 }, { ZYD_CR66,  0x58 },
		{ ZYD_CR67,  0xc9 }, { ZYD_CR68,  0x88 }, { ZYD_CR69,  0x41 },
		{ ZYD_CR70,  0x23 }, { ZYD_CR71,  0x10 }, { ZYD_CR72,  0xff },
		{ ZYD_CR73,  0x32 }, { ZYD_CR74,  0x30 }, { ZYD_CR75,  0x65 },
		{ ZYD_CR76,  0x41 }, { ZYD_CR77,  0x1b }, { ZYD_CR78,  0x30 },
		{ ZYD_CR79,  0x68 }, { ZYD_CR80,  0x64 }, { ZYD_CR81,  0x64 },
		{ ZYD_CR82,  0x00 }, { ZYD_CR83,  0x00 }, { ZYD_CR84,  0x00 },
		{ ZYD_CR85,  0x02 }, { ZYD_CR86,  0x00 }, { ZYD_CR87,  0x00 },
		{ ZYD_CR88,  0xff }, { ZYD_CR89,  0xfc }, { ZYD_CR90,  0x00 },
		{ ZYD_CR91,  0x00 }, { ZYD_CR92,  0x00 }, { ZYD_CR93,  0x08 },
		{ ZYD_CR94,  0x00 }, { ZYD_CR95,  0x00 }, { ZYD_CR96,  0xff },
		{ ZYD_CR97,  0xe7 }, { ZYD_CR98,  0x00 }, { ZYD_CR99,  0x00 },
		{ ZYD_CR100, 0x00 }, { ZYD_CR101, 0xae }, { ZYD_CR102, 0x02 },
		{ ZYD_CR103, 0x00 }, { ZYD_CR104, 0x03 }, { ZYD_CR105, 0x65 },
		{ ZYD_CR106, 0x04 }, { ZYD_CR107, 0x00 }, { ZYD_CR108, 0x0a },
		{ ZYD_CR109, 0xaa }, { ZYD_CR110, 0xaa }, { ZYD_CR111, 0x25 },
		{ ZYD_CR112, 0x25 }, { ZYD_CR113, 0x00 }, { ZYD_CR119, 0x1e },
		{ ZYD_CR125, 0x90 }, { ZYD_CR126, 0x00 }, { ZYD_CR127, 0x00 },
		{ ZYD_CR5,   0x00 }, { ZYD_CR6,   0x00 }, { ZYD_CR7,   0x00 },
		{ ZYD_CR8,   0x00 }, { ZYD_CR9,   0x20 }, { ZYD_CR12,  0xf0 },
		{ ZYD_CR20,  0x0e }, { ZYD_CR21,  0x0e }, { ZYD_CR27,  0x10 },
		{ ZYD_CR44,  0x33 }, { ZYD_CR47,  0x30 }, { ZYD_CR83,  0x24 },
		{ ZYD_CR84,  0x04 }, { ZYD_CR85,  0x00 }, { ZYD_CR86,  0x0C },
		{ ZYD_CR87,  0x12 }, { ZYD_CR88,  0x0C }, { ZYD_CR89,  0x00 },
		{ ZYD_CR90,  0x10 }, { ZYD_CR91,  0x08 }, { ZYD_CR93,  0x00 },
		{ ZYD_CR94,  0x01 }, { ZYD_CR95,  0x00 }, { ZYD_CR96,  0x50 },
		{ ZYD_CR97,  0x37 }, { ZYD_CR98,  0x35 }, { ZYD_CR101, 0x13 },
		{ ZYD_CR102, 0x27 }, { ZYD_CR103, 0x27 }, { ZYD_CR104, 0x18 },
		{ ZYD_CR105, 0x12 }, { ZYD_CR109, 0x27 }, { ZYD_CR110, 0x27 },
		{ ZYD_CR111, 0x27 }, { ZYD_CR112, 0x27 }, { ZYD_CR113, 0x27 },
		{ ZYD_CR114, 0x27 }, { ZYD_CR115, 0x26 }, { ZYD_CR116, 0x24 },
		{ ZYD_CR117, 0xfc }, { ZYD_CR118, 0xfa }, { ZYD_CR120, 0x4f },
		{ ZYD_CR123, 0x27 }, { ZYD_CR125, 0xaa }, { ZYD_CR127, 0x03 },
		{ ZYD_CR128, 0x14 }, { ZYD_CR129, 0x12 }, { ZYD_CR130, 0x10 },
		{ ZYD_CR131, 0x0C }, { ZYD_CR136, 0xdf }, { ZYD_CR137, 0x40 },
		{ ZYD_CR138, 0xa0 }, { ZYD_CR139, 0xb0 }, { ZYD_CR140, 0x99 },
		{ ZYD_CR141, 0x82 }, { ZYD_CR142, 0x54 }, { ZYD_CR143, 0x1c },
		{ ZYD_CR144, 0x6c }, { ZYD_CR147, 0x07 }, { ZYD_CR148, 0x4c },
		{ ZYD_CR149, 0x50 }, { ZYD_CR150, 0x0e }, { ZYD_CR151, 0x18 },
		{ ZYD_CR160, 0xfe }, { ZYD_CR161, 0xee }, { ZYD_CR162, 0xaa },
		{ ZYD_CR163, 0xfa }, { ZYD_CR164, 0xfa }, { ZYD_CR165, 0xea },
		{ ZYD_CR166, 0xbe }, { ZYD_CR167, 0xbe }, { ZYD_CR168, 0x6a },
		{ ZYD_CR169, 0xba }, { ZYD_CR170, 0xba }, { ZYD_CR171, 0xba },
		/* Note: ZYD_CR204 must lead the ZYD_CR203 */
		{ ZYD_CR204, 0x7d }, { ZYD_CR203, 0x30 }/*, { ZYD_CR240, 0x80 }*/
	};

	static const struct zyd_adpairs32 ir2[] = {
		{ ZYD_MAC_ACK_EXT,		0x20 },
		{ ZYD_CR_ADDA_MBIAS_WARMTIME,	0x30000808 },
		{ ZYD_MAC_RETRY,		0x2 },
		{ ZYD_MAC_SNIFFER,		0 },
		{ ZYD_MAC_STOHOSTSETTING,	0 },//ZYD_AP_RX_FILTER },
		{ ZYD_MAC_GHTBL,		0x00 },
		{ ZYD_MAC_GHTBH,		0x80000000 },
		{ ZYD_MAC_MISC,			0xa4 },
		{ ZYD_CR_ADDA_PWR_DWN,		0x7f },
		{ ZYD_MAC_BCNCFG,		0x00f00401 },
		{ ZYD_MAC_PHY_DELAY2,		0x00 },
		{ ZYD_MAC_ACK_EXT,		0x80 },
		{ ZYD_CR_ADDA_PWR_DWN,		0x00 },
		{ ZYD_MAC_SIFS_ACK_TIME,	0x100 },
		{ ZYD_MAC_DIFS_EIFS_SIFS,	0x547c032 },
		{ ZYD_CR_RX_PE_DELAY,		0x70 },
		{ ZYD_CR_PS_CTRL,		0x10000000 },
		{ ZYD_MAC_RTSCTSRATE,		0x02030203 },
		{ ZYD_MAC_RX_THRESHOLD,		0x000c0640 },
		{ ZYD_MAC_AFTER_PNP,		0x1 },
		{ ZYD_MAC_BACKOFF_PROTECT,	0x114 }
	};

	usbd_status rv;
	int stage = 0;
	uint8_t rf, pa;
	uint16_t theversion;

	rv = zyd_singleregwrite32(sc, ZYD_MAC_AFTER_PNP, 1);

	if (rv)
		goto leave;

	stage++;

	rv = zyd_singleregread16(sc, ZYD_REG_USB(ZYD_FIRMWARE_BASE_ADDR),
	    &sc->firmware_base);

	DPRINTF(("zyd_hw_init: firmware_base = 0x%04X\n", sc->firmware_base));

	/* Print the firmware version */
	rv = zyd_singleregread16(sc, ZYD_FW_FIRMWARE_VER, &theversion);

	if (rv)
		goto leave;

	stage++;

	printf("%s: Firmware version is 0x%04X\n",
	    USBDEVNAME(sc->zyd_dev), theversion);

	rv = zyd_singleregwrite32(sc, ZYD_CR_GPI_EN, 0);

	if (rv)
		goto leave;

	stage++;

	rv = zyd_singleregwrite32(sc, ZYD_MAC_CONT_WIN_LIMIT, 0x007f043f);

	if (rv)
		goto leave;

	stage++;

	zyd_set_mandatory_rates(sc, ic->ic_curmode);

	zyd_disable_hwint(sc);

	/* PHY init ("reset") */
	zyd_lock_phy(sc);
	rv = zyd_batchwrite16(sc, ir1,
	    (sizeof(ir1) / sizeof(struct zyd_adpairs16)));
	zyd_unlock_phy(sc);

	if (rv)
		goto leave;

	stage++;

	/* HMAC init */
	rv = zyd_batchwrite32(sc, ir2,
	    (sizeof(ir2) / sizeof(struct zyd_adpairs32)));

	if (rv)
		goto leave;

	stage++;

	/* RF/PA types */
	rv = zyd_read_rf_pa_types(sc, &rf, &pa);

	if (rv)
		goto leave;

	stage++;

	/* Now init the RF chip */
	rv = zyd_rf_init_hw(sc, &sc->rf, rf);

	if (rv)
		goto leave;

	stage++;

	/* Init beacon to 100 * 1024 µs */
	rv = zyd_set_beacon_interval(sc, 100);

	if (rv)
		goto leave;

	stage++;

leave:
	DPRINTF(("zyd_hw_init: rv = %d, stage = %d\n", rv, stage));
	return rv;
}

/*
 * Get MAC address from EEPROM
 */
usbd_status
zyd_get_e2p_mac_addr(struct zyd_softc *sc, struct zyd_macaddr *mac_addr)
{
	uint32_t mac[2];
	usbd_status rv;

	rv = zyd_singleregread32(sc, ZYD_E2P_MAC_ADDR_P1, &mac[0]);

	if (rv)
		goto leave;

	rv = zyd_singleregread32(sc, ZYD_E2P_MAC_ADDR_P2, &mac[1]);

	if (rv)
		goto leave;

	mac_addr->addr[0] = mac[0];
	mac_addr->addr[1] = mac[0] >>  8;
	mac_addr->addr[2] = mac[0] >> 16;
	mac_addr->addr[3] = mac[0] >> 24;
	mac_addr->addr[4] = mac[1];
	mac_addr->addr[5] = mac[1] >>  8;

	printf("%s: E2P MAC address is %02X:%02X:%02X:%02X:%02X:%02X\n",
	    USBDEVNAME(sc->zyd_dev), mac_addr->addr[0], mac_addr->addr[1],
	    mac_addr->addr[2], mac_addr->addr[3], mac_addr->addr[4],
	    mac_addr->addr[5]);

leave:
	return rv;
}

/*
 * Set MAC address (will accept ANY address)
 */
usbd_status
zyd_set_mac_addr(struct zyd_softc *sc, const struct zyd_macaddr *mac_addr)
{
	uint32_t addrs[2];
	uint32_t trans[2];

	addrs[0] = ZYD_MAC_MACADDRL;
	addrs[1] = ZYD_MAC_MACADDRH;

	trans[0] = (
	    (mac_addr->addr[3] << 24) | (mac_addr->addr[2] << 16) |
	    (mac_addr->addr[1] << 8) | (mac_addr->addr[0]));

	trans[1] = (
	    (mac_addr->addr[5] << 8) | (mac_addr->addr[4]));

	return zyd_multiregwrite32(sc, addrs, trans, 2);
}

/*
 * Read regdomain
 */
usbd_status
zyd_read_regdomain(struct zyd_softc *sc, uint8_t *regdomain)
{
	uint32_t value;
	usbd_status rv;

	rv = zyd_singleregread32(sc, ZYD_E2P_SUBID, &value);

	if (!rv)
		*regdomain = value >> 16;

	return rv;
}

/*
 * Check whether a particular regdomain is supported
 */
int
zyd_regdomain_supported(uint8_t regdomain)
{
	const struct zyd_channel_range *range;

	range = &zyd_channel_ranges[0];

	for ( ; ; ) {
		if (range->regdomain == regdomain)
			return (range->start != 0);
		else if (range->regdomain == -1)
			break; /* end of list */

		range++;
	}

	return 0;
}

/*
 * Helper used by all table readers
 */
int
zyd_tblreader(struct zyd_softc *sc, uint8_t *values, size_t count,
	uint32_t e2p_addr, uint32_t guard)
{
	int r;
	int i;
	uint32_t v;

	for (i = 0;;) {
		r = zyd_singleregread32(sc, (e2p_addr + (i / 2)), &v);

		if (r)
			return r;

		v -= guard;

		if (i+4 < count) {
			values[i++] = v;
			values[i++] = v >>  8;
			values[i++] = v >> 16;
			values[i++] = v >> 24;
			continue;
		}

		for (;i < count; i++)
			values[i] = v >> (8*(i%3));

		return 0;
	}

	return 0;
}

/*
 * Read calibration tables
 */
int
zyd_readcaltables(struct zyd_softc *sc)
{
	int rv;
	int i;

	static const uint32_t addresses[] = {
		ZYD_E2P_36M_CAL_VALUE1,
		ZYD_E2P_48M_CAL_VALUE1,
		ZYD_E2P_54M_CAL_VALUE1,
	};

	rv = zyd_tblreader(sc, sc->pwr_cal_values,
	    ZYD_E2P_CHANNEL_COUNT, ZYD_E2P_PWR_CAL_VALUE1, 0);
		
	if (rv)
		goto leave;

	rv = zyd_tblreader(sc, sc->pwr_int_values,
	    ZYD_E2P_CHANNEL_COUNT, ZYD_E2P_PWR_INT_VALUE1,	ZYD_E2P_PWR_INT_GUARD);

	if (rv)
		goto leave;

	for (i = 0; i < 3; i++) {
		rv = zyd_tblreader(sc, sc->ofdm_cal_values[i],
		    ZYD_E2P_CHANNEL_COUNT, addresses[i], 0);
		if (rv)
			goto leave;
	}

leave:
	return rv;
}

/*
 * Reset channel
 */
int
zyd_reset_channel(struct zyd_softc *sc)
{
	const struct zyd_channel_range *range;

	range = &zyd_channel_ranges[0];

	for ( ; ; ) {
		if (range->regdomain == sc->regdomain)
			if (range->start == 0)
				return 1;
			else
			{
				sc->channel = range->start;
				sc->mac_flags &= ~ZMF_FIXED_CHANNEL;
			}
		else if (range->regdomain == -1)
			return 1; /* end of list */

		range++;
	}

	return 0;
}

/*
 * Set encryption type
 */
usbd_status
zyd_set_encryption_type(struct zyd_softc *sc, uint32_t type)
{
	return zyd_singleregwrite32(sc, ZYD_MAC_ENCRYPTION_TYPE, type);
}

/*
 * Switch radio on/off
 */
usbd_status
zyd_switch_radio(struct zyd_softc *sc, uint8_t onoff)
{
	usbd_status rv;

	zyd_lock_phy(sc);
	rv = sc->rf.switch_radio(sc, onoff);
	zyd_unlock_phy(sc);

	if (!rv)
		sc->zyd_radio_on = onoff;

	return rv;
}

/*
 * Enable hardware interrupt
 */
usbd_status
zyd_enable_hwint(struct zyd_softc *sc)
{
	return zyd_singleregwrite32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_ENABLED);
}

/*
 * Disable hardware interrupt
 */
usbd_status
zyd_disable_hwint(struct zyd_softc *sc)
{
	return zyd_singleregwrite32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_DISABLED);
}

/*
 * Set basic rates
 */
usbd_status
zyd_set_basic_rates(struct zyd_softc *sc, int mode)
{
	/* Do not request high rates for the basic set */
	uint32_t outf = 0;

	switch (mode) {
	case IEEE80211_MODE_11B:
		/* 11B: 1, 2 MBPS */
		outf = 3;
		break;

	case IEEE80211_MODE_11G:
		/* 11G: 6, 12, 24 MBPS */
		outf = (21 << 8);
		break;

	default:
		return -1;
	}

	return zyd_singleregwrite32(sc, ZYD_MAC_BASICRATE, outf);
}

/*
 * Set mandatory rates. This is the full spectrum of a certain mode.
 */
usbd_status
zyd_set_mandatory_rates(struct zyd_softc *sc, int mode)
{
	uint32_t outf = 0;

	switch (mode) {
	case IEEE80211_MODE_11B:
		/* 11B: 1, 2, 5.5, 11 */
		outf = CSF_RT_CCK_1 | CSF_RT_CCK_2 | CSF_RT_CCK_5_5 | CSF_RT_CCK_11;
		break;

	case IEEE80211_MODE_11G:
		/* 11G: 6, 9, 12, 18, 24, 36, 48, 54 */
		outf = CSF_RT_OFDM_6 | CSF_RT_OFDM_9 | CSF_RT_OFDM_12 |
		    CSF_RT_OFDM_18 | CSF_RT_OFDM_24 | CSF_RT_OFDM_36 |
		    CSF_RT_OFDM_48 | CSF_RT_OFDM_54;
		break;

	default:
		return -1;
	}

	return zyd_singleregwrite32(sc, ZYD_MAC_MANDATORYRATE, outf);
}

/*
 * Reset mode
 */
usbd_status
zyd_reset_mode(struct zyd_softc *sc)
{
	struct zyd_adpairs32 io[3] = {
		{ ZYD_MAC_STOHOSTSETTING, STH_BCN | STH_PRB_RSP | STH_AUTH | STH_ASS_RSP },
		{ ZYD_MAC_SNIFFER, 0U },
		{ ZYD_MAC_ENCRYPTION_TYPE, 0U }
	};
/*
	if (ieee->iw_mode == IW_MODE_MONITOR) {
		ioreqs[0].value = 0xffffffff;
		ioreqs[1].value = 0x1;
		ioreqs[2].value = ENC_SNIFFER;
	}*/

	DPRINTF(("In zyd_reset_mode()\n"));

	return zyd_batchwrite32(sc, io, 3);
}


/*
 * Set BSSID
 */
usbd_status
zyd_set_bssid(struct zyd_softc *sc, uint8_t *addr)
{
	uint32_t addrh;
	uint32_t addrl;
	usbd_status rv;

	addrh = (*((uint32_t *)addr) >> 16);
	addrl = *((uint32_t *)(addr + 2));

	DPRINTF(("Setting BSSID\n"));

	DPRINTF(("addrh = %x, addrl = %x\n", addrh, addrl));

	rv = zyd_singleregwrite32(sc, ZYD_MAC_BSSADRL, addrl);

	if (!rv)
		rv = zyd_singleregwrite32(sc, ZYD_MAC_BSSADRH, addrh);

	return rv;
}

/*
 * Complete the attach process
 */
usbd_status
zyd_complete_attach(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct zyd_macaddr mac;

	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status rv;
	int i;

	id = usbd_get_interface_descriptor(sc->zyd_iface);

	/*
	 * Endpoint 1 = Bulk out (512b @ high speed / 64b @ full speed)
	 * Endpoint 2 = Bulk in  (512b @ high speed / 64b @ full speed)
	 * Endpoint 3 = Intr in (64b)
	 * Endpoint 4 = Intr out @ high speed / bulk out @ full speed (64b)
	 */
	
	DPRINTF(("A total of %d endpoints available\n", id->bNumEndpoints));
	
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->zyd_iface, i);

		if (ed == NULL) {
			printf("%s: no endpoint descriptor for iface %d\n",
			    USBDEVNAME(sc->zyd_dev), i);
			return -1;
		}

		DPRINTF(("Endpoint %d: ", i));

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) {
			DPRINTF(("in "));

			if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				DPRINTF(("bulk\n"));
			else
				DPRINTF(("int\n"));
		} else {
			DPRINTF(("out "));

			if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				DPRINTF(("bulk\n"));
			else
				DPRINTF(("int\n"));
		}

/*		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->zyd_ep[ZYD_ENDPT_BIN] = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			sc->sc_tx_no = ed->bEndpointAddress;*/
	}
	
	ed = usbd_interface2endpoint_descriptor(sc->zyd_iface, 0);
	sc->zyd_ed[ZYD_ENDPT_BOUT] = ed->bEndpointAddress;

	ed = usbd_interface2endpoint_descriptor(sc->zyd_iface, 1);
	sc->zyd_ed[ZYD_ENDPT_BIN] = ed->bEndpointAddress;

	ed = usbd_interface2endpoint_descriptor(sc->zyd_iface, 2);
	sc->zyd_ed[ZYD_ENDPT_IIN] = ed->bEndpointAddress;

	ed = usbd_interface2endpoint_descriptor(sc->zyd_iface, 3);
	sc->zyd_ed[ZYD_ENDPT_IOUT] = ed->bEndpointAddress;

	/* Open the pipes */
	zyd_openpipes(sc);

	/* Init hardware */
	rv = zyd_hw_init(sc, ic);

	if (rv)
		goto leave;

	/* Read MAC from EEPROM and copy to interface */
	rv = zyd_get_e2p_mac_addr(sc, &mac);
	memcpy(&sc->sc_ic.ic_myaddr, &mac, IEEE80211_ADDR_LEN);

	if (rv)
		goto leave;

	/* Read calibration tables from EEPROM */
	rv = zyd_readcaltables(sc);

	if (rv)
		goto leave;

	DPRINTF(("Loading regdomain\n"));
	/* Load the regdomain and see whether it is supported */
	rv = zyd_read_regdomain(sc, &sc->default_regdomain);

	if (rv)
		goto leave;

	DPRINTF(("Regdomain supported?\n"));
	if (!zyd_regdomain_supported(sc->default_regdomain)) {
		printf("%s: Error: Regulatory Domain %#04x is not supported.",
		    USBDEVNAME(sc->zyd_dev), sc->default_regdomain);
			
		rv = USBD_INVAL;
		goto leave;
	}

	sc->regdomain = sc->default_regdomain;

	sc->zyd_encrypt = ENC_NOWEP;
	sc->zyd_wepkeylen = 0;
	sc->zyd_wepkey = 0;

	bzero(sc->zyd_bssid, ETHER_ADDR_LEN);
	sc->zyd_ssidlen = strlen(ZYD_DEFAULT_SSID);
	memcpy(sc->zyd_ssid, ZYD_DEFAULT_SSID, sc->zyd_ssidlen);

	/* TODO: Is this an allowed channel in the domain? */
	sc->channel = ZYD_DEFAULT_CHANNEL;
	sc->zyd_desired_channel = IEEE80211_CHAN_ANY;
	sc->zyd_operation = OM_INFRASTRUCTURE;

	/* Network interface setup */
	ic->ic_softc = sc;
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities */
	ic->ic_caps = IEEE80211_C_MONITOR | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_SHPREAMBLE | IEEE80211_C_PMGT |
	    IEEE80211_C_TXPMGT | IEEE80211_C_WEP;

	/* Rates are in 0,5 MBps units */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = zyd_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = zyd_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	memcpy(ifp->if_xname, USBDEVNAME(sc->zyd_dev), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = zyd_if_init;
	ifp->if_start = zyd_if_start;
	ifp->if_ioctl = zyd_if_ioctl;
	ifp->if_watchdog = zyd_if_watchdog;
	ifp->if_mtu = ZYD_DEFAULT_MTU;
	IFQ_SET_READY(&ifp->if_snd);

	/* Call MI attach routine. */
	if_attach(ifp);
	ieee80211_ifattach(ifp);

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = zyd_newstate;

	/* setup ifmedia interface */
	ieee80211_media_init(ifp, zyd_media_change, ieee80211_media_status);

	usb_init_task(&sc->sc_task, zyd_task, sc);

/*	ieee80211_announce(ic);*/

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->zyd_udev,
	    USBDEV(sc->zyd_dev));

	timeout_set(&sc->scan_ch, zyd_next_scan, sc);
	timeout_add(&sc->scan_ch, hz);

leave:
	DPRINTF(("EXITING complete_attach(): Status = %d\n", rv));
	return rv;
}

/*
 * Detach device
 */
USB_DETACH(zyd)
{
	USB_DETACH_START(zyd, sc);
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splusb();

/*	zyd_if_stop(ifp, 1);*/

	usb_rem_task(sc->zyd_udev, &sc->sc_task);

	timeout_del(&sc->scan_ch);

	zyd_closepipes(sc);

	zyd_free_rx(sc);
	zyd_free_tx(sc);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	splx(s);

	return 0;
}

int
zyd_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);

	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		zyd_if_init(ifp);

	return 0;
}

int
zyd_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct zyd_softc *sc = ic->ic_if.if_softc;

	DPRINTF(("zyd_newstate(): %d\n", nstate));

	usb_rem_task(sc->zyd_udev, &sc->sc_task);

	timeout_del(&sc->scan_ch);

	/* do it in a process context */
	sc->sc_state = nstate;
	usb_add_task(sc->zyd_udev, &sc->sc_task);

	return 0;
}

/*
 * Initial configuration
 *
 * - Copy MAC address
 * - Init channel (to first in allowed range)
 * - Set encryption type
 */
int
zyd_initial_config(struct zyd_softc *sc)
{
/*	struct ieee80211com *ic = &sc->sc_ic;*/
/*	uint32_t i;*/
	usbd_status rv;

	DPRINTF(("Setting mac-addr\n"));
	rv = zyd_set_mac_addr(sc, (const struct zyd_macaddr *)&sc->sc_ic.ic_myaddr);

	if (rv)
		return rv;

/*	DPRINTF(("Reset channel\n"));
	if (zyd_reset_channel(sc) != 0) {
		return USBD_INVAL;
	}*/

	DPRINTF(("Setting encryption type\n"));
	rv = zyd_set_encryption_type(sc, sc->zyd_encrypt);

	if (rv)
		return rv;

	/* TODO: Check what we've already initialized in the hw_init section */

	DPRINTFN(10, ("%s: completed initial config\n",
	   USBDEVNAME(sc->zyd_dev)));
	return 0;
}


void
zyd_update_promisc(struct zyd_softc *sc)
{
}

/*
 * Compute the duration (in us) needed to transmit `len' bytes at rate `rate'.
 * The function automatically determines the operating mode depending on the
 * given rate. `flags' indicates whether short preamble is in use or not.
 */
uint16_t
zyd_txtime(int len, int rate, uint32_t flags)
{
	uint16_t txtime;
	int ceil, dbps;

	if (ZYD_RATE_IS_OFDM(rate)) {
		/*
		 * OFDM TXTIME calculation.
		 * From IEEE Std 802.11a-1999, pp. 37.
		 */
		dbps = rate * 2; /* data bits per OFDM symbol */

		ceil = (16 + 8 * len + 6) / dbps;

		if ((16 + 8 * len + 6) % dbps != 0)
			ceil++;

		txtime = 16 + 4 + 4 * ceil + 6;
	} else {
		/*
		 * High Rate TXTIME calculation.
		 * From IEEE Std 802.11b-1999, pp. 28.
		 */
		ceil = (8 * len * 2) / rate;

		if ((8 * len * 2) % rate != 0)
			ceil++;

		if (rate != 2 && (flags & IEEE80211_F_SHPREAMBLE))
			txtime =  72 + 24 + ceil;
		else
			txtime = 144 + 48 + ceil;
	}

	return txtime;
}

/*
 * Rate-to-bit-converter (Field "rate" in zyd_controlsetformat)
 */
uint8_t
zyd_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates */
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
/*
int zyd_calc_useclen2(uint8_t *service, uint8_t cs_rate, uint16_t tx_length)
{
	static const uint8_t rate_divisor[] = {
		[ZD_CS_CCK_RATE_1M]	=  1,
		[ZD_CS_CCK_RATE_2M]	=  2,
		[ZD_CS_CCK_RATE_5_5M]	= 11, // bits must be doubled
		[ZD_CS_CCK_RATE_11M]	= 11,
		[ZD_OFDM_RATE_6M]	=  6,
		[ZD_OFDM_RATE_9M]	=  9,
		[ZD_OFDM_RATE_12M]	= 12,
		[ZD_OFDM_RATE_18M]	= 18,
		[ZD_OFDM_RATE_24M]	= 24,
		[ZD_OFDM_RATE_36M]	= 36,
		[ZD_OFDM_RATE_48M]	= 48,
		[ZD_OFDM_RATE_54M]	= 54,
	};

	uint32_t bits = (uint32_t)tx_length * 8;
	uint32_t divisor;

	divisor = rate_divisor[cs_rate];
	if (divisor == 0)
		return -EINVAL;

	switch (cs_rate) {
	case ZD_CS_CCK_RATE_5_5M:
		bits = (2*bits) + 10; // round up to the next integer
		break;
	case ZD_CS_CCK_RATE_11M:
		if (service) {
			uint32_t t = bits % 11;
			*service &= ~ZD_PLCP_SERVICE_LENGTH_EXTENSION;
			if (0 < t && t <= 3) {
				*service |= ZD_PLCP_SERVICE_LENGTH_EXTENSION;
			}
		}
		bits += 10; // round up to the next integer
		break;
	}

	return bits/divisor;
}

enum {
	R2M_SHORT_PREAMBLE = 0x01,
	R2M_11A		   = 0x02,
};
*/

/*
 * Calculate frame transmit length in microseconds
 */
uint16_t
zyd_calc_useclen(int rate, uint16_t len, uint8_t *service)
{
	uint32_t remainder;
	uint32_t delta;
	uint16_t leninus;

	leninus = 0;
	*(service) = 0;

	switch (rate) {
	case 2:	/* 1M bps */
		leninus = len << 3;
		break;

	case 4:	/* 2M bps */
		leninus = len << 2;
		break;

	case 11: /* 5.5M bps */
		leninus = (uint16_t)(((uint32_t)len << 4) / 11);
		remainder = (((uint32_t)len << 4) % 11);

		if (remainder)
			leninus += 1;
		break;

	case 22: /* 11M bps */
		leninus = (uint16_t)(((uint32_t)len << 3) / 11);
		remainder = (((uint32_t)len << 3) % 11);
		delta = 11 - remainder;

		if (remainder) {
			leninus += 1;
			if (delta >= 8)
				*(service) |= 0x80; /* Bit 7 */
		}
		break;

	case 12:/* 6M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 6);
		break;

	case 18:/* 9M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 9);
		break;

	case 24:/* 12M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 12);
		break;

	case 36:/* 18M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 18);
		break;

	case 48:/* 24M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 24);
		break;

	case 72:/* 36M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 36);
		break;

	case 96:/* 48M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 48);
		break;

	case 108: /* 54M */
		leninus = (uint16_t)(((uint32_t)len << 3) / 54);
		break;
	}

	return leninus;
}

/*
 * Setup the controlsetformat structure
 */
void
zyd_setup_tx_desc(struct zyd_softc *sc, struct zyd_controlsetformat *desc,
	struct mbuf *m, int len, int rate)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
	u_int8_t more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
	uint8_t type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	uint16_t txlen;

	DPRINTF(("Entering zyd_setup_tx_desc()\n"));
	DPRINTF(("sizeof (zyd_controlsetformat) = %d\n",
	    sizeof(struct zyd_controlsetformat)));

	memset(desc, 0, ZYD_TX_DESC_SIZE);

	/* Rate (CCK and OFDM) */
	desc->rate = zyd_plcp_signal(rate);

	/* Modulation type (CCK/OFDM) */
	if (ZYD_RATE_IS_OFDM(rate))
		desc->modulationtype = CSF_MT_OFDM;
	else
		desc->modulationtype = CSF_MT_CCK;

	/* Preamble/a/g (depending on modtype) */
	if (desc->modulationtype == CSF_MT_CCK) {
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			desc->preamble = CSF_PM_CCK_SHORT;
	}

	// DEBUG!
	desc->preamble = 0;

	/*
	 * Transmit frame length in bytes:
	 * 802.11 MAC header length + raw data length
	 * + ICV/(MIC) length + FCS length.
	 */
	txlen = len; /* + 4;*/
	desc->txlen = htole16(txlen);

	/*
	 * If no more fragments, enable backoff protection,
	 * 80211-1999 p. 77
	 */
	if (!more_frag)
		desc->needbackoff = CSF_BO_RAND;

	/* Multicast */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1))
		desc->multicast = CSF_MC_MULTICAST;

	/* Frame type */
	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		desc->frametype = CSF_FT_DATAFRAME;
		break;

	case IEEE80211_FC0_TYPE_MGT:
		desc->frametype = CSF_FT_MGMTFRAME;
		break;

	case IEEE80211_FC0_TYPE_CTL:
		/* Only subtype PS_POLL has seq control */
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL)
			desc->frametype = CSF_FT_POLLFRAME;
		else
			desc->frametype = CSF_FT_NOSEQCONTROL;
		break;

	/* All other don't have a sequence control field */
	default:
		desc->frametype = CSF_FT_NOSEQCONTROL;
	}

	/* Wake dst. ignored */

	/*
	 * RTS/CTS
	 * If the frame is non-multicast, non-mgt, set "RTS" if
	 * fragment size > RTS threshold in CCK mode. In OFDM, set
	 * self cts instead.
	 */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)
		&& (type != IEEE80211_FC0_TYPE_MGT)
		&& (txlen > ic->ic_rtsthreshold)) {

		if (ZYD_RATE_IS_OFDM(rate))
			desc->selfcts = CSF_SC_SCFRAME;
		else
			desc->rts = CSF_RTS_NEEDRTSFRAME;
	}

	/* Encryption */

	/*
	 * TODO: Hmm ... only set this if hardware performs
	 * encryption. Does it???
	 */	

	/* Self cts */
/*	if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
		desc->selfcts = CSF_SC_SCFRAME;*/

	/* Packet length */
	/* DEBUG: appendet 25... */
	desc->packetlength = htole16(len + 25);
/*	desc->packetlength = (ZYD_TX_DESC_SIZE + len + 1) & ~1; */

	/* Service (PLCP) */
	desc->service = 0;

	/* Current length (usec) */
	desc->currentlength = htole16(
		zyd_calc_useclen(rate, txlen, &desc->service));

	/* Next frame length (usec) */
	if (more_frag)
		desc->nextframelen = desc->currentlength; // DEBUG!

	DPRINTF(("desc: rate=%d, modulationtype=%d, preamble=%d, "
	    "txlen=%d, needbackoff=%d, multicast=%d, frametype=%d, "
	    "wakedst=%d, rts=%d, encryption=%d, selfcts=%d, "
	    "packetlength=%d, currentlength=%d, service=%d, nextframelen=%d\n",
	    desc->rate, desc->modulationtype, desc->preamble,
	    desc->txlen, desc->needbackoff, desc->multicast, desc->frametype,
	    desc->wakedst, desc->rts, desc->encryption, desc->selfcts,
	    desc->packetlength, desc->currentlength, desc->service,
	    desc->nextframelen));
}

void dump_fw_registers(struct zyd_softc *);

void
dump_fw_registers(struct zyd_softc *sc)
{
	static const uint32_t addr[4] = {
		ZYD_FW_FIRMWARE_VER,
		ZYD_FW_USB_SPEED,
		ZYD_FW_FIX_TX_RATE,
		ZYD_FW_LINK_STATUS
	};

/*	int rv, i;*/
	int i;
	uint16_t values[4];

	for (i = 0; i < 4; ++i)
		zyd_singleregread16(sc, addr[i], &values[i]);

	DPRINTF(("FW_FIRMWARE_VER %#06hx\n", values[0]));
	DPRINTF(("FW_USB_SPEED %#06hx\n", values[1]));
	DPRINTF(("FW_FIX_TX_RATE %#06hx\n", values[2]));
	DPRINTF(("FW_LINK_STATUS %#06hx\n", values[3]));
}


int
zyd_tx_mgt(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	static const uint8_t winbuf[]  = {
		0x01, 0x2e, 0x00, 0x03, 0x43, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x11, 0xf6, 0x7f, 0x9b, 0x3c, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x82, 0x84, 0x0b, 0x16, 0x32, 0x08, 0x0c, 0x12, 0x18,
		0x24, 0x30, 0x48, 0x60, 0x6c
	};

	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_controlsetformat *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	uint16_t dur;
	usbd_status error;
	int xferlen, rate;

	DPRINTF(("Entering zyd_tx_mgt()\n"));

/*	dump_fw_registers(sc);*/

	data = &sc->tx_data[0];
	desc = (struct zyd_controlsetformat *)data->buf;

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan) ? 12 : 4;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		dur = zyd_txtime(ZYD_ACK_SIZE, rate, ic->ic_flags) + ZYD_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);

/*		// tell hardware to add timestamp for probe responses
		if ((wh->i_fc[0] &
			(IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
			(IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP))
			flags |= RAL_TX_TIMESTAMP;*/
	}

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + ZYD_TX_DESC_SIZE);


/* DEBUG: Use exactly what windoof does */
	memcpy(data->buf, winbuf, sizeof(winbuf));
	xferlen = sizeof(winbuf);


	DPRINTF(("Raw dump before desc setup:\n"));

	bindump(data->buf, xferlen);
/*	bindump(data->buf + ZYD_TX_DESC_SIZE, m0->m_pkthdr.len);*/

/*	zyd_setup_tx_desc(sc, desc, m0, m0->m_pkthdr.len, rate);*/

	// xfer length needs to be a multiple of two!
	xferlen = (ZYD_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	/* Make sure padding is 0x00 */
	if (xferlen != (ZYD_TX_DESC_SIZE + m0->m_pkthdr.len))
		*(data->buf + xferlen - 1) = 0x00;

	DPRINTF(("sending mgt frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	DPRINTF(("Raw send data output:\n"));

	bindump(data->buf, xferlen);

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);

	error = usbd_transfer(data->xfer);

	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		DPRINTF(("zyd_tx_mgt(): Error %d\n", error));
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;

/*	zyd_stateoutput(sc);*/

	DPRINTF(("Leaving zyd_tx_mgt()\n"));

	return 0;
}

int
zyd_tx_data(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
/*	struct ifnet *ifp = &ic->ic_if;*/
	struct ieee80211_rateset *rs;
	struct zyd_controlsetformat *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
/*	uint16_t dur;*/
	usbd_status error;
	int xferlen, rate;

	DPRINTF(("Entering zyd_tx_data()\n"));

	/* XXX this should be reworked! */
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

/*	if (ic->ic_flags & IEEE80211_F_WEPON) {
		m0 = ieee80211_wep_crypt(ifp, m0, 1);
		if (m0 == NULL)
			return ENOBUFS;
	}*/

	data = &sc->tx_data[0];
	desc = (struct zyd_controlsetformat *)data->buf;

	data->m = m0;
	data->ni = ni;

	wh = mtod(m0, struct ieee80211_frame *);

/*	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		dur = zyd_txtime(ZYD_ACK_SIZE, zyd_ack_rate(ic, rate),
			ic->ic_flags) + ZYD_SIFS;
		*(uint16_t *)wh->i_dur = htole16(dur);
	}*/

	m_copydata(m0, 0, m0->m_pkthdr.len, data->buf + ZYD_TX_DESC_SIZE);
	zyd_setup_tx_desc(sc, desc, m0, m0->m_pkthdr.len, rate);

	// xfer length needs to be a multiple of two!
	xferlen = (ZYD_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	DPRINTF(("sending data frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);

	error = usbd_transfer(data->xfer);

	if (error != USBD_NORMAL_COMPLETION && error != USBD_IN_PROGRESS) {
		m_freem(m0);
		return error;
	}

	sc->tx_queued++;

	DPRINTF(("Leaving zyd_tx_data()\n"));

	return 0;
}

/*
 * Transmit beacon frame
 */
int
zyd_tx_bcn(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_controlsetformat *desc;
	usbd_xfer_handle xfer;
	usbd_status error;
	uint8_t cmd = 0;
	uint8_t *buf;
	int xferlen, rate;

	DPRINTF(("Entering zyd_tx_bcn()\n"));

	rate = IEEE80211_IS_CHAN_5GHZ(ic->ic_bss->ni_chan) ? 12 : 4;

	xfer = usbd_alloc_xfer(sc->zyd_udev);

	if (xfer == NULL)
		return ENOMEM;

	/* xfer length needs to be a multiple of two! */
	xferlen = (ZYD_TX_DESC_SIZE + m0->m_pkthdr.len + 1) & ~1;

	buf = usbd_alloc_buffer(xfer, xferlen);

	if (buf == NULL) {
		usbd_free_xfer(xfer);
		return ENOMEM;
	}

	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], NULL, &cmd, sizeof cmd,
		USBD_FORCE_SHORT_XFER, ZYD_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);

	if (error != 0) {
		usbd_free_xfer(xfer);
		return error;
	}

	desc = (struct zyd_controlsetformat *)buf;

	m_copydata(m0, 0, m0->m_pkthdr.len, buf + ZYD_TX_DESC_SIZE);
	zyd_setup_tx_desc(sc, desc, m0, m0->m_pkthdr.len, rate);

	DPRINTF(("sending beacon frame len=%u rate=%u xfer len=%u\n",
	    m0->m_pkthdr.len, rate, xferlen));

	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], NULL, buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, ZYD_TX_TIMEOUT, NULL);

	error = usbd_sync_transfer(xfer);
	usbd_free_xfer(xfer);

	DPRINTF(("Leaving zyd_tx_bcn()\n"));

	return error;
}


void
zyd_set_chan(struct zyd_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
/*	uint8_t power, tmp;
	u_int i, chan;*/
	unsigned int chan;

	DPRINTF(("Entering zyd_set_chan()\n"));

	chan = ieee80211_chan2ieee(ic, c);

	DPRINTF(("zyd_set_chan: Will try %d\n", chan));

	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
	{
		DPRINTF(("zyd_set_chan(): 0 or ANY, exiting\n"));
		return;
	}

	DPRINTF(("@1: zyd_set_chan()\n"));

	zyd_lock_phy(sc);

	sc->rf.set_channel(sc, &sc->rf, chan);

	/* Power integration */
	zyd_singleregwrite32(sc, ZYD_CR31, sc->pwr_int_values[chan - 1]);

	/* Power calibration */
	zyd_singleregwrite32(sc, ZYD_CR68, sc->pwr_cal_values[chan - 1]);

	zyd_unlock_phy(sc);

	DPRINTF(("Finished zyd_set_chan()\n"));
}

/*
 * Interface: init
 */
int
zyd_if_init(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com	*ic = &sc->sc_ic;
	struct zyd_rx_data *data;
	usbd_status	err;
	uint32_t statedata;
	int	i, s;

	DPRINTF(("Entering zyd_if_init()\n"));

	s = splnet();

/*	zyd_if_stop(ifp, 0);*/

	/* Do initial setup */
	err = zyd_initial_config(sc);

	if (err) {
		DPRINTF(("%s: initial config failed!\n",
		    USBDEVNAME(sc->zyd_dev)));
		splx(s);
		return(EIO);
	}

	/* Additional init */
	zyd_reset_mode(sc);
	zyd_switch_radio(sc, 1);

	/* Set basic rates */
	zyd_set_basic_rates(sc, ic->ic_curmode);

	/* Set mandatory rates */
/*	zyd_set_mandatory_rates(sc, ic->ic_curmode);	*/

	DPRINTF(("@1: zyd_if_init()\n"));

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	DPRINTF(("Setting channel from if_init()\n"));
	zyd_set_chan(sc, ic->ic_bss->ni_chan);

	zyd_enable_hwint(sc);

	DPRINTF(("@2: zyd_if_init()\n"));

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));

	DPRINTFN(10, ("%s: zyd_init\n", USBDEVNAME(sc->zyd_dev)));

	if (ifp->if_flags & IFF_RUNNING) {
		splx(s);
		return(0);
	}

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	DPRINTF(("@3: zyd_if_init()\n"));
	err = zyd_alloc_tx(sc);

	if (err != 0) {
		printf("%s: could not allocate Tx list\n",
		    USBDEVNAME(sc->zyd_dev));
		goto fail;
	}

	DPRINTF(("@4: zyd_if_init()\n"));
	err = zyd_alloc_rx(sc);

	if (err != 0) {
		printf("%s: could not allocate Rx list\n",
		    USBDEVNAME(sc->zyd_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	DPRINTF(("@5: zyd_if_init()\n"));
	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data,
		    data->buf, MCLBYTES, USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT,
		    zyd_rxeof);

		usbd_transfer(data->xfer);
	}

	/* Load the multicast filter. */
	/*zyd_setmulti(sc); */
	DPRINTF(("@6: zyd_if_init()\n"));

	DPRINTFN(10, ("%s: starting up using MAC=%s\n",
	    USBDEVNAME(sc->zyd_dev), ether_sprintf(ic->ic_myaddr)));

	DPRINTFN(10, ("%s: initialised transceiver\n",
	    USBDEVNAME(sc->zyd_dev)));

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	DPRINTF(("@7: zyd_if_init()\n"));

	zyd_singleregread32(sc, ZYD_REG_CTL(0x684), &statedata);
	DPRINTF(("State machine: %x\n", statedata));

	return 0;

fail:
/*	zyd_if_stop(ifp, 1);*/
	splx(s);
	return err;
}

/*
 * Interface: stop
 */
/*
void
zyd_if_stop(struct ifnet *ifp, int disable)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("Entering zyd_if_stop()\n"));

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	sc->tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	zyd_free_rx(sc);
	zyd_free_tx(sc);

	DPRINTF(("Leaving zyd_if_stop()\n"));
}*/

/*
 * Interface: start
 */
void
zyd_if_start(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	DPRINTF(("Entering zyd_if_start()\n"));

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			DPRINTF(("zyd_if_start: m0 != NULL, tx_queued = %d\n",
			    sc->tx_queued));

			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;

			DPRINTF(("if_state: @1\n"));

			if (zyd_tx_mgt(sc, m0, ni) != 0)
				break;

		} else {
			DPRINTF(("if_state: @2\n"));

			if (ic->ic_state != IEEE80211_S_RUN)
				break;

			IFQ_DEQUEUE(&ifp->if_snd, m0);
			DPRINTF(("if_state: @3\n"));

			if (m0 == NULL)
				break;

			DPRINTF(("if_state: @4\n"));

			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}

			DPRINTF(("if_state: @5\n"));

			if (m0->m_len < sizeof (struct ether_header) &&
				!(m0 = m_pullup(m0, sizeof (struct ether_header))))
				continue;

			DPRINTF(("if_state: @6\n"));
			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);

			if (ni == NULL) {
				m_freem(m0);
				continue;
			}

			DPRINTF(("if_state: @7\n"));
			m0 = ieee80211_encap(ifp, m0, &ni);

			if (m0 == NULL) {
				ieee80211_release_node(ic, ni);
				continue;
			}

			DPRINTF(("if_state: @8\n"));

			if (zyd_tx_data(sc, m0, ni) != 0) {
				ieee80211_release_node(ic, ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->tx_timer = 5;
		ifp->if_timer = 1;
	}

	DPRINTF(("Finished zyd_if_start()\n"));
}

/*
 * Interface: ioctl
 */
int
zyd_if_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int err = 0, s;

	DPRINTF(("Entering zyd_if_ioctl()\n"));

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		DPRINTF(("IOCTL: SIOCSIFFLAGS\n"));
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				zyd_update_promisc(sc);
			else
				zyd_if_init(ifp);
		} else {
/*			if (ifp->if_flags & IFF_RUNNING)
				zyd_if_stop(ifp, 1);*/
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		DPRINTF(("IOCTL: SIOCADDMULTI\n"));
		ifr = (struct ifreq *)data;
		err = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);

		if (err == ENETRESET)
			err = 0;
		break;

	case SIOCS80211CHANNEL:
		/*
		 * This allows for fast channel switching in monitor mode
		 * (used by kismet). In IBSS mode, we must explicitly reset
		 * the interface to generate a new beacon frame.
		 */
		DPRINTF(("IOCTL: SIOCS80211CHANNEL (Setting channel from ioctl\n"));
		
		err = ieee80211_ioctl(ifp, command, data);

		if (err == ENETRESET &&
			ic->ic_opmode == IEEE80211_M_MONITOR) {
			zyd_set_chan(sc, ic->ic_ibss_chan);
			err = 0;
		}
		break;

		default:
			DPRINTFN(15, ("%s: ieee80211_ioctl (%lu)\n",
			    USBDEVNAME(sc->zyd_dev), command));
			err = ieee80211_ioctl(ifp, command, data);
			break;
	}

	if (err == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) ==
		    (IFF_RUNNING | IFF_UP)) {
			DPRINTF(("%s: zyd_if_ioctl(): netreset\n",
			    USBDEVNAME(sc->zyd_dev)));
			zyd_if_init(ifp);
		}
		err = 0;
	}

	splx(s);

	DPRINTF(("Finished zyd_if_ioctl()\n"));

	return (err);
}

/*
 * Interface: watchdog
 */
void
zyd_if_watchdog(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;

	DPRINTF(("zyd_if_watchdog()\n"));

	ifp->if_timer = 0;

	if (sc->tx_timer > 0) {
		if (--sc->tx_timer == 0) {
			printf("%s: device timeout\n", USBDEVNAME(sc->zyd_dev));
			/*zyd_init(ifp); XXX needs a process context ? */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
void
zyd_next_scan(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("Executing next_scan\n"));

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&ic->ic_if);
}

/*
 * USB task callback
 */
void
zyd_task(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;
	struct mbuf *m;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
		}
		break;

	case IEEE80211_S_SCAN:
		DPRINTF(("Setting channel from task (SCAN)\n"));
		zyd_set_chan(sc, ic->ic_bss->ni_chan);

		timeout_add(&sc->scan_ch, hz / 5);
		break;

	case IEEE80211_S_AUTH:
		DPRINTF(("Setting channel from task (AUTH)\n"));
		zyd_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_ASSOC:
		DPRINTF(("Setting channel from task (ASSOC)\n"));
		zyd_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_RUN:
		DPRINTF(("Setting channel from task (RUN)\n"));
		zyd_set_chan(sc, ic->ic_bss->ni_chan);

		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			zyd_set_bssid(sc, ic->ic_bss->ni_bssid);

		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
			ic->ic_opmode == IEEE80211_M_IBSS) {
			m = ieee80211_beacon_alloc(ic, ic->ic_bss);

			if (m == NULL) {
				printf("%s: could not allocate beacon\n",
				    USBDEVNAME(sc->zyd_dev));
				return;
			}

			if (zyd_tx_bcn(sc, m, ic->ic_bss) != 0) {
				m_freem(m);
				printf("%s: could not transmit beacon\n",
				    USBDEVNAME(sc->zyd_dev));
				return;
			}

			/* beacon is no longer needed */
			m_freem(m);
		}
		break;
	}

	sc->sc_newstate(ic, sc->sc_state, -1);
}

int
zyd_activate(device_ptr_t self, enum devact act)
{
	DPRINTF(("Entering zyd_activate()\n"));

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		break;
	}

	return 0;
}
