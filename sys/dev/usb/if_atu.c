/*	$OpenBSD: if_atu.c,v 1.18 2004/11/17 14:13:47 deraadt Exp $ */
/*
 * Copyright (c) 2003, 2004
 *	Daan Vreeken <Danovitsch@Vitsch.net>.  All rights reserved.
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
 *	This product includes software developed by Daan Vreeken.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Daan Vreeken AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Daan Vreeken OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Atmel AT76c503 / AT76c503a / AT76c505 / AT76c505a  USB WLAN driver
 * version 0.5 - 2004-08-03
 *
 * Originally written by Daan Vreeken <Danovitsch @ Vitsch . net>
 *  http://vitsch.net/bsd/atuwi
 *
 * Contributed to by :
 *  Chris Whitehouse, Alistair Phillips, Peter Pilka, Martijn van Buul,
 *  Suihong Liang, Arjan van Leeuwen, Stuart Walsh
 *
 * Ported to OpenBSD by Theo de Raadt and David Gwynne.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include "bpfilter.h"
#define BPF_MTAP(ifp, m) bpf_mtap((ifp)->if_bpf, (m))
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <sys/ucred.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#if 0
#include <dev/usb/usb_ethersubr.h>
#endif

#include <dev/usb/usbdevs.h>

#include <dev/ic/if_wi_ieee.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>

#ifdef USB_DEBUG
#define ATU_DEBUG
#endif

#include <dev/usb/if_atureg.h>

#ifdef ATU_DEBUG
#define DPRINTF(x)	do { if (atudebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (atudebug>(n)) printf x; } while (0)
int atudebug = 11;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

USB_DECLARE_DRIVER_CLASS(atu, DV_IFNET);

/*
 * Various supported device vendors/products/radio type.
 */
struct atu_type atu_devs[] = {
	{ USB_VENDOR_ATMEL,	USB_PRODUCT_ATMEL_BW002,
	  RadioRFMD,		ATU_NO_QUIRK },
	{ USB_VENDOR_ATMEL,	USB_PRODUCT_ATMEL_AT76C503A,
	  RadioIntersil,	ATU_NO_QUIRK },
	{ USB_VENDOR_LEXAR,	USB_PRODUCT_LEXAR_2662WAR,
	  RadioRFMD,		ATU_NO_QUIRK },
	{ USB_VENDOR_LINKSYS2,	USB_PRODUCT_LINKSYS2_WUSB11,
	  RadioRFMD,		ATU_NO_QUIRK },
	{ USB_VENDOR_LINKSYS3,	USB_PRODUCT_LINKSYS3_WUSB11V28,
	  RadioRFMD2958,	ATU_NO_QUIRK },
	{ USB_VENDOR_NETGEAR2,	USB_PRODUCT_NETGEAR2_MA101B,
	  RadioRFMD,		ATU_NO_QUIRK },
	{ USB_VENDOR_ACERP,	USB_PRODUCT_ACERP_AWL400,
	  RadioRFMD,		ATU_NO_QUIRK },
	{ USB_VENDOR_ATMEL,	USB_PRODUCT_ATMEL_WL1130,
	  RadioRFMD2958,	ATU_NO_QUIRK },
	{ USB_VENDOR_LINKSYS3,	USB_PRODUCT_LINKSYS3_WUSB11V28,
	  RadioRFMD2958,	ATU_NO_QUIRK },
	{ USB_VENDOR_AINCOMM,	USB_PRODUCT_AINCOMM_AWU2000B,
	  RadioRFMD2958,	ATU_NO_QUIRK },
	/* SMC2662 V.4 */
	{ USB_VENDOR_ATMEL,	USB_PRODUCT_ATMEL_AT76C505A,
	  RadioRFMD2958_SMC,	ATU_QUIRK_NO_REMAP | ATU_QUIRK_FW_DELAY },
	/* XXX is this one right? */
	{ USB_VENDOR_ATMEL,	USB_PRODUCT_ATMEL_DWL120,
	  RadioIntersil,	ATU_NO_QUIRK },
	{ USB_VENDOR_ACERP,	USB_PRODUCT_ACERP_AWL300,
	  RadioIntersil,	ATU_NO_QUIRK },
	{ 0, 0, 0, 0 }
};

int	atu_newbuf(struct atu_softc *, struct atu_chain *, struct mbuf *);
int	atu_encap(struct atu_softc *sc, struct mbuf *m, struct atu_chain *c);
void	atu_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
void	atu_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
void	atu_start(struct ifnet *);
void	atu_mgmt_loop(void *arg);
int	atu_ioctl(struct ifnet *, u_long, caddr_t);
void	atu_init(void *);
void	atu_stop(struct atu_softc *);
void	atu_watchdog(struct ifnet *);
void	atu_msleep(struct atu_softc *, int);
usbd_status atu_usb_request(struct atu_softc *sc, u_int8_t type,
	    u_int8_t request, u_int16_t value, u_int16_t index,
	    u_int16_t length, u_int8_t *data);
int	atu_send_command(struct atu_softc *sc, u_int8_t *command, int size);
int	atu_get_cmd_status(struct atu_softc *sc, u_int8_t cmd,
	    u_int8_t *status);
int	atu_wait_completion(struct atu_softc *sc, u_int8_t cmd,
	    u_int8_t *status);
int	atu_send_mib(struct atu_softc *sc, u_int8_t type,
	    u_int8_t size, u_int8_t index, void *data);
int	atu_get_mib(struct atu_softc *sc, u_int8_t type,
	    u_int8_t size, u_int8_t index, u_int8_t *buf);
int	atu_start_ibss(struct atu_softc *sc);
int	atu_start_scan(struct atu_softc *sc);
int	atu_switch_radio(struct atu_softc *sc, int state);
int	atu_initial_config(struct atu_softc *sc);
int	atu_join(struct atu_softc *sc);
int	atu_send_packet(struct atu_softc *sc, struct atu_chain *c);
int	atu_send_mgmt_packet(struct atu_softc *sc,
	    struct atu_chain *c, u_int16_t length);
int	atu_authenticate(struct atu_softc *sc);
int	atu_associate(struct atu_softc *sc);
int8_t	atu_get_dfu_state(struct atu_softc *sc);
u_int8_t atu_get_opmode(struct atu_softc *sc, u_int8_t *mode);
int	atu_upload_internal_firmware(struct atu_softc *sc);
int	atu_upload_external_firmware(struct atu_softc *sc);
int	atu_get_card_config(struct atu_softc *sc);
int	atu_mgmt_state_machine(struct atu_softc *sc);
int	atu_media_change(struct ifnet *ifp);
void	atu_media_status(struct ifnet *ifp, struct ifmediareq *req);
int	atu_xfer_list_init(struct atu_softc *sc, struct atu_chain *ch,
	    int listlen, int need_mbuf, int bufsize, struct atu_list_head *list);
int	atu_rx_list_init(struct atu_softc *);
void	atu_xfer_list_free(struct atu_softc *sc, struct atu_chain *ch,
	    int listlen);
void	atu_print_beacon(struct atu_softc *sc, struct atu_rxpkt *pkt);
void	atu_handle_mgmt_packet(struct atu_softc *sc, struct atu_rxpkt *pkt);
void	atu_print_a_bunch_of_debug_things(struct atu_softc *sc);
int	atu_set_wepkey(struct atu_softc *sc, int nr, u_int8_t *key, int len);

void
atu_msleep(struct atu_softc *sc, int ms)
{
	u_int8_t	dummy;
	int		ticks;

	usbd_delay_ms(sc->atu_udev, ms);
	return;

	ticks = ms * hz / 1000;
	if (ticks == 0)
		ticks = 1;

	tsleep(&dummy, PZERO | PCATCH, "atus", ms * hz / 1000);
}

static usbd_status
atu_reset(struct atu_softc *sc)
{
	/* We don't need to actually send the device a reset... */
#if 0
	usb_port_status_t	stat;

	usbd_reset_port(sc->atu_udev->myhub,
	    sc->atu_udev->powersrc->portno, &stat);
#endif

	sc->atu_udev->address = USB_START_ADDR;
	return(0);
}

usbd_status
atu_usb_request(struct atu_softc *sc, u_int8_t type,
    u_int8_t request, u_int16_t value, u_int16_t index, u_int16_t length,
    u_int8_t *data)
{
	usb_device_request_t	req;
	usbd_xfer_handle	xfer;
	usbd_status		err;
	int			total_len = 0, s;

	req.bmRequestType = type;
	req.bRequest = request;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, length);

#ifdef ATU_DEBUG
	if (atudebug) {
		if ((data == NULL) || (type & UT_READ)) {
			DPRINTFN(20, ("%s: req=%02x val=%02x ind=%02x "
			    "len=%02x\n", USBDEVNAME(sc->atu_dev), request,
			    value, index, length));
		} else {
			DPRINTFN(20, ("%s: req=%02x val=%02x ind=%02x "
			    "len=%02x [%8D]\n", USBDEVNAME(sc->atu_dev),
			    request, value, index, length, data, " "));
		}
	}
#endif /* ATU_DEBUG */

	s = splnet();

	xfer = usbd_alloc_xfer(sc->atu_udev);
	usbd_setup_default_xfer(xfer, sc->atu_udev, 0, 500000, &req, data,
	    length, USBD_SHORT_XFER_OK, 0);

	err = usbd_sync_transfer(xfer);

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

#ifdef ATU_DEBUG
	if (atudebug) {
		if (type & UT_READ) {
			DPRINTFN(20, ("%s: transfered 0x%x bytes in\n",
			    USBDEVNAME(sc->atu_dev), total_len));
			DPRINTFN(20, ("%s: dump [%10D]\n",
			    USBDEVNAME(sc->atu_dev), data, " "));
		} else {
			if (total_len != length)
				DPRINTF(("%s: ARG! wrote only %x bytes\n",
				    USBDEVNAME(sc->atu_dev), total_len));
		}
	}
#endif /* ATU_DEBUG */

	usbd_free_xfer(xfer);

	splx(s);
	return(err);
}

int
atu_send_command(struct atu_softc *sc, u_int8_t *command, int size)
{
	return atu_usb_request(sc, UT_WRITE_VENDOR_DEVICE, 0x0e, 0x0000,
	    0x0000, size, command);
}

int
atu_get_cmd_status(struct atu_softc *sc, u_int8_t cmd, u_int8_t *status)
{
	/*
	 * all other drivers (including Windoze) request 40 bytes of status
	 * and get a short-xfer of just 6 bytes. we can save 34 bytes of
	 * buffer if we just request those 6 bytes in the first place :)
	 */
	/*
	return atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x22, cmd,
	    0x0000, 40, status);
	*/
	return atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x22, cmd,
	    0x0000, 6, status);
}

int
atu_wait_completion(struct atu_softc *sc, u_int8_t cmd, u_int8_t *status)
{
	int			err;
	u_int8_t		statusreq[6];
	int			idle_count = 0;

	DPRINTFN(15, ("%s: wait-completion: cmd=%02x\n",
	    USBDEVNAME(sc->atu_dev), cmd));

	while (1) {
		err = atu_get_cmd_status(sc, cmd, statusreq);
		if (err)
			return err;

#ifdef ATU_DEBUG
		if (atudebug) {
			DPRINTFN(20, ("%s: status=%s cmd=%02x\n",
			    USBDEVNAME(sc->atu_dev),
			ether_sprintf(statusreq), cmd));
		}
#endif /* ATU_DEBUG */

		/*
		 * during normal operations waiting on STATUS_IDLE
		 * will never happen more than once
		 */
		if ((statusreq[5] == STATUS_IDLE) && (idle_count++ > 20)) {
			DPRINTF(("%s: AAARRGGG!!! FIX ME!\n",
			    USBDEVNAME(sc->atu_dev)));
			return 0;
		}

		if ((statusreq[5] != STATUS_IN_PROGRESS) &&
		    (statusreq[5] != STATUS_IDLE)) {
			if (status != NULL)
				*status = statusreq[5];
			return 0;
		}
		atu_msleep(sc, 25);
	}
}

int
atu_send_mib(struct atu_softc *sc, u_int8_t type, u_int8_t size,
    u_int8_t index, void *data)
{
	int				err;
	struct atu_cmd_set_mib	request;

	/*
	 * We don't construct a MIB packet first and then memcpy it into an
	 * Atmel-command-packet, we just construct it the right way at once :)
	 */

	request.AtCmd = CMD_SET_MIB;
	request.AtReserved = 0;
	request.AtSize = size + 4;

	request.MIBType = type;
	request.MIBSize = size;
	request.MIBIndex = index;
	request.MIBReserved = 0;

	/*
	 * For 1 and 2 byte requests we assume a direct value,
	 * everything bigger than 2 bytes we assume a pointer to the data
	 */
	switch (size) {
	case 0:
		break;
	case 1:
		request.data[0]=(long)data & 0x000000ff;
		break;
	case 2:
		request.data[0]=(long)data & 0x000000ff;
		request.data[1]=(long)data >> 8;
		break;
	default:
		memcpy(request.data, data, size);
	}

	err = atu_usb_request(sc, UT_WRITE_VENDOR_DEVICE, 0x0e, 0x0000,
		0x0000, request.AtSize+4, (u_int8_t *)&request);
	if (err)
		return err;

	DPRINTFN(15, ("%s: sendmib : waitcompletion...\n",
	    USBDEVNAME(sc->atu_dev)));
	return atu_wait_completion(sc, CMD_SET_MIB, NULL);
}

int
atu_get_mib(struct atu_softc *sc, u_int8_t type, u_int8_t size,
    u_int8_t index, u_int8_t *buf)
{

	/* linux/at76c503.c - 478 */
	return atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x033,
		type << 8, index, size, buf);
}

int
atu_start_ibss(struct atu_softc *sc)
{
	int				err;
	struct atu_cmd_start_ibss	Request;

	Request.Cmd = CMD_START_IBSS;
	Request.Reserved = 0;
	Request.Size = sizeof(Request) - 4;

	memset(Request.BSSID, 0x00, sizeof(Request.BSSID));
	memset(Request.SSID, 0x00, sizeof(Request.SSID));
	memcpy(Request.SSID, sc->atu_ssid, sc->atu_ssidlen);
	Request.SSIDSize = sc->atu_ssidlen;
	if (sc->atu_desired_channel != IEEE80211_CHAN_ANY)
		Request.Channel = (u_int8_t)sc->atu_desired_channel;
	else
		Request.Channel = ATU_DEFAULT_CHANNEL;
	Request.BSSType = AD_HOC_MODE;
	memset(Request.Res, 0x00, sizeof(Request.Res));

	/* Write config to adapter */
	err = atu_send_command(sc, (u_int8_t *)&Request, sizeof(Request));
	if (err) {
		DPRINTF(("%s: start ibss failed!\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}

	/* Wait for the adapter to do it's thing */
	err = atu_wait_completion(sc, CMD_START_IBSS, NULL);
	if (err) {
		DPRINTF(("%s: error waiting for start_ibss\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}

	/* Get the current BSSID */
	err = atu_get_mib(sc, MIB_MAC_MGMT__CURRENT_BSSID, sc->atu_bssid);
	if (err) {
		DPRINTF(("%s: could not get BSSID!\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}

	DPRINTF(("%s: started a new IBSS (BSSID=%s)\n",
		USBDEVNAME(sc->atu_dev), ether_sprintf(sc->atu_bssid)));
	return 0;
}

int
atu_start_scan(struct atu_softc *sc)
{
	struct atu_cmd_do_scan	Scan;
	usbd_status			err;
	int				Cnt;

	Scan.Cmd = CMD_START_SCAN;
	Scan.Reserved = 0;
	Scan.Size = sizeof(Scan) - 4;

	/* use the broadcast BSSID (in active scan) */
	for (Cnt=0; Cnt<6; Cnt++)
		Scan.BSSID[Cnt] = 0xff;

	memset(Scan.SSID, 0x00, sizeof(Scan.SSID));
	memcpy(Scan.SSID, sc->atu_ssid, sc->atu_ssidlen);
	Scan.SSID_Len = sc->atu_ssidlen;

	/* default values for scan */
	Scan.ScanType = ATU_SCAN_ACTIVE;
	if (sc->atu_desired_channel != IEEE80211_CHAN_ANY)
		Scan.Channel = (u_int8_t)sc->atu_desired_channel;
	else
		Scan.Channel = sc->atu_channel;
	Scan.ProbeDelay = 3550;
	Scan.MinChannelTime = 250;
	Scan.MaxChannelTime = 3550;

	/* we like scans to be quick :) */
	/* the time we wait before sending probe's */
	Scan.ProbeDelay = 0;
	/* the time we stay on one channel */
	Scan.MinChannelTime = 100;
	Scan.MaxChannelTime = 200;
	/* wether or not we scan all channels */
	Scan.InternationalScan = 0xc1;

#ifdef ATU_DEBUG
	if (atudebug) {
		DPRINTFN(20, ("%s: scan cmd len=%02x\n",
		    USBDEVNAME(sc->atu_dev), sizeof(Scan)));
		DPRINTFN(20, ("%s: scan cmd: %52D\n", USBDEVNAME(sc->atu_dev),
		    (u_int8_t *)&Scan, " "));
	}
#endif /* ATU_DEBUG */

	/* Write config to adapter */
	err = atu_send_command(sc, (u_int8_t *)&Scan, sizeof(Scan));
	if (err)
		return err;

	/*
	 * We don't wait for the command to finish... the mgmt-thread will do
	 * that for us
	 */
	/*
	err = atu_wait_completion(sc, CMD_START_SCAN, NULL);
	if (err)
		return err;
	*/
	return 0;
}

int
atu_switch_radio(struct atu_softc *sc, int state)
{
	usbd_status		err;
	struct atu_cmd	CmdRadio = {CMD_RADIO_ON, 0, 0};

	if (sc->atu_radio == RadioIntersil) {
		/*
		 * Intersil doesn't seem to need/support switching the radio
		 * on/off
		 */
		return 0;
	}

	if (sc->atu_radio_on != state) {
		if (state == 0)
			CmdRadio.Cmd = CMD_RADIO_OFF;

		err = atu_send_command(sc, (u_int8_t *)&CmdRadio,
			sizeof(CmdRadio));
		if (err)
			return err;

		err = atu_wait_completion(sc, CmdRadio.Cmd, NULL);
		if (err)
			return err;

		DPRINTFN(10, ("%s: radio turned %s\n",
		    USBDEVNAME(sc->atu_dev), state ? "on" : "off"));
		sc->atu_radio_on = state;
	}
	return 0;
}

int
atu_initial_config(struct atu_softc *sc)
{
	usbd_status			err;
/*	u_int8_t			rates[4] = {0x82, 0x84, 0x8B, 0x96};*/
	u_int8_t			rates[4] = {0x82, 0x04, 0x0B, 0x16};
	struct atu_cmd_card_config	cmd;
	u_int8_t			reg_domain;

	DPRINTFN(10, ("%s: sending mac-addr\n", USBDEVNAME(sc->atu_dev)));
	err = atu_send_mib(sc, MIB_MAC_ADDR__ADDR, sc->atu_mac_addr);
	if (err) {
		DPRINTF(("%s: error setting mac-addr\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}

	/*
	DPRINTF(("%s: sending reg-domain\n", USBDEVNAME(sc->atu_dev)));
	err = atu_send_mib(sc, MIB_PHY__REG_DOMAIN, NR(0x30));
	if (err) {
		DPRINTF(("%s: error setting mac-addr\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}
	*/

	memset(&cmd, 0, sizeof(cmd));
	cmd.Cmd = CMD_STARTUP;
	cmd.Reserved = 0;
	cmd.Size = sizeof(cmd) - 4;

	if (sc->atu_desired_channel != IEEE80211_CHAN_ANY)
		cmd.Channel = (u_int8_t)sc->atu_desired_channel;
	else
		cmd.Channel = sc->atu_channel;
	cmd.AutoRateFallback = 1;
	memcpy(cmd.BasicRateSet, rates, 4);

	/* ShortRetryLimit should be 7 according to 802.11 spec */
	cmd.ShortRetryLimit = 7;
	cmd.RTS_Threshold = 2347;
	cmd.FragThreshold = 2346;

	/* Doesn't seem to work, but we'll set it to 1 anyway */
	cmd.PromiscuousMode = 1;

	/* this goes into the beacon we transmit */
	if (sc->atu_encrypt == ATU_WEP_OFF)
		cmd.PrivacyInvoked = 0;
	else
		cmd.PrivacyInvoked = 1;

	cmd.ExcludeUnencrypted = 0;
	cmd.EncryptionType = sc->atu_wepkeylen;

	/* Setting the SSID here doesn't seem to do anything */
	memset(cmd.SSID, 0, sizeof(cmd.SSID));
	memcpy(cmd.SSID, sc->atu_ssid, sc->atu_ssidlen);
	cmd.SSID_Len = sc->atu_ssidlen;

	cmd.WEP_DefaultKeyID = sc->atu_wepkey;
	memcpy(cmd.WEP_DefaultKey, sc->atu_wepkeys,
	    sizeof(cmd.WEP_DefaultKey));

	cmd.ShortPreamble = 1;
	cmd.ShortPreamble = 0;
	cmd.BeaconPeriod = 100;
	/* cmd.BeaconPeriod = 65535; */

	/*
	 * TODO:
	 * read reg domain MIB_PHY @ 0x17 (1 byte), (reply = 0x30)
	 * we should do something usefull with this info. right now it's just
	 * ignored
	 */
	err = atu_get_mib(sc, MIB_PHY__REG_DOMAIN, &reg_domain);
	if (err) {
		DPRINTF(("%s: could not get regdomain!\n",
		    USBDEVNAME(sc->atu_dev)));
	} else {
		DPRINTF(("%s: we're in reg domain 0x%x according to the "
		    "adapter\n", USBDEVNAME(sc->atu_dev), reg_domain));
	}

#ifdef ATU_DEBUG
	if (atudebug) {
		DPRINTFN(20, ("%s: configlen=%02x\n", USBDEVNAME(sc->atu_dev),
		    sizeof(cmd)));
		DPRINTFN(20, ("%s: configdata= %108D\n",
		    USBDEVNAME(sc->atu_dev), (u_int8_t *)&cmd, " "));
	}
#endif /* ATU_DEBUG */

	/* Windoze : driver says exclude-unencrypted=1 & encr-type=1 */

	err = atu_send_command(sc, (u_int8_t *)&cmd, sizeof(cmd));
	if (err)
		return err;
	err = atu_wait_completion(sc, CMD_STARTUP, NULL);
	if (err)
		return err;

	/* Turn on radio now */
	err = atu_switch_radio(sc, 1);
	if (err)
		return err;

	/* preamble type = short */
	err = atu_send_mib(sc, MIB_LOCAL__PREAMBLE, NR(PREAMBLE_SHORT));
	if (err)
		return err;

	/* frag = 1536 */
	err = atu_send_mib(sc, MIB_MAC__FRAG, NR(2346));
	if (err)
		return err;

	/* rts = 1536 */
	err = atu_send_mib(sc, MIB_MAC__RTS, NR(2347));
	if (err)
		return err;

	/* auto rate fallback = 1 */
	err = atu_send_mib(sc, MIB_LOCAL__AUTO_RATE_FALLBACK, NR(1));
	if (err)
		return err;

	/* power mode = full on, no power saving */
	err = atu_send_mib(sc, MIB_MAC_MGMT__POWER_MODE,
	    NR(POWER_MODE_ACTIVE));
	if (err)
		return err;

	DPRINTFN(10, ("%s: completed initial config\n",
	   USBDEVNAME(sc->atu_dev)));
	return 0;
}

int
atu_join(struct atu_softc *sc)
{
	struct atu_cmd_join		join;
	u_int8_t			status;
	usbd_status			err;

	join.Cmd = CMD_JOIN;
	join.Reserved = 0x00;
	join.Size = sizeof(join) - 4;

	DPRINTFN(15, ("%s: pre-join sc->atu_bssid=%s\n",
	    USBDEVNAME(sc->atu_dev), ether_sprintf(sc->atu_bssid)));
	DPRINTFN(15, ("%s: mode=%d\n", USBDEVNAME(sc->atu_dev),
	    sc->atu_mode));
	memcpy(join.bssid, sc->atu_bssid, ETHER_ADDR_LEN);
	memset(join.essid, 0x00, 32);
	memcpy(join.essid, sc->atu_ssid, sc->atu_ssidlen);
	join.essid_size = sc->atu_ssidlen;
	join.bss_type = sc->atu_mode;
	join.channel = sc->atu_channel;

	join.timeout = ATU_JOIN_TIMEOUT;
	join.reserved = 0x00;

	DPRINTFN(10, ("%s: trying to join BSSID=%s\n",
	    USBDEVNAME(sc->atu_dev), ether_sprintf(join.bssid)));
	err = atu_send_command(sc, (u_int8_t *)&join, sizeof(join));
	if (err) {
		DPRINTF(("%s: ERROR trying to join IBSS\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}
	err = atu_wait_completion(sc, CMD_JOIN, &status);
	if (err) {
		DPRINTF(("%s: error joining BSS!\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}
	if (status != STATUS_COMPLETE) {
		DPRINTF(("%s: error joining... [status=%02x]\n",
		    USBDEVNAME(sc->atu_dev), status));
		return status;
	} else {
		DPRINTFN(10, ("%s: joined BSS\n", USBDEVNAME(sc->atu_dev)));
	}
	return err;
}

int
atu_send_packet(struct atu_softc *sc, struct atu_chain *c)
{
	usbd_status		err;
	struct atu_txpkt	*pkt;

	/* Don't try to send when we're shutting down the driver */
	if (sc->atu_dying)
		return(EIO);

	pkt = (struct atu_txpkt *)c->atu_buf;

	usbd_setup_xfer(c->atu_xfer, sc->atu_ep[ATU_ENDPT_TX],
	    c, c->atu_buf, c->atu_length, USBD_NO_COPY, ATU_TX_TIMEOUT,
	    atu_txeof);

	/* Let's get this thing into the air! */
	c->atu_in_xfer = 1;
	err = usbd_transfer(c->atu_xfer);
	if (err != USBD_IN_PROGRESS) {
		atu_stop(sc);
		return(EIO);
	}

	DPRINTFN(10, ("%s: tx packet...\n", USBDEVNAME(sc->atu_dev)));
	return 0;
}

int
atu_send_mgmt_packet(struct atu_softc *sc, struct atu_chain *c,
    u_int16_t length)
{
	struct atu_mgmt_packet	*packet;

	packet = (struct atu_mgmt_packet *)c->atu_buf;

	packet->athdr.wlength = length - sizeof(packet->athdr);
	packet->athdr.tx_rate = 4;
	packet->athdr.padding = 0;
	memset(packet->athdr.reserved, 0x00, 4);

	packet->mgmt_hdr.duration = 0x8000;
	memcpy(packet->mgmt_hdr.dst_addr, sc->atu_bssid, ETHER_ADDR_LEN);
	memcpy(packet->mgmt_hdr.src_addr, sc->atu_mac_addr, ETHER_ADDR_LEN);
	memcpy(packet->mgmt_hdr.bssid, sc->atu_bssid, ETHER_ADDR_LEN);
	packet->mgmt_hdr.seq_ctl = 0;

	c->atu_length = length;
	return atu_send_packet(sc, c);
}

int
atu_authenticate(struct atu_softc *sc)
{
	usbd_status			err;
	struct atu_chain		*ch;
	struct atu_auth_packet	*packet;

	/*
	 * now we should authenticate :
	 *  7.2.3.10 - page 64 of 802.11b spec
	 *  8.1 - page 74 of 802.11b spec
	 *  see 7.3.1.9 - page 69 for status codes
	 *
	 * open systems :
	 *  send: seq_nr=1	auth req
	 *  recv: seq_nr=2	auth resp. (with status code)
	 *
	 * shared key systems :
	 *  send: seq_nr=1	auth req
	 *  recv: seq_nr=2	auth challenge (with status code & challenge
	 *                        text)
	 *  send: seq_nr=3	auth reponse (wep encr challenge text)
	 *  recv: seq_nr=4	auth result
	 *
	 * algorithm number :
	 *  0 = open
	 *  1 = shared
	 */

	ch = SLIST_FIRST(&sc->atu_cdata.atu_mgmt_free);
	if (ch == NULL) {
		DPRINTF(("%s: authenticate: no mgmt transfers available\n",
		    USBDEVNAME(sc->atu_dev)));
		return ENOMEM;
	}
	SLIST_REMOVE_HEAD(&sc->atu_cdata.atu_mgmt_free, atu_list);

	packet = (struct atu_auth_packet *)ch->atu_buf;

	packet->mgmt_hdr.frame_ctl = WI_FTYPE_MGMT |
	    IEEE80211_FC0_SUBTYPE_AUTH;

	packet->auth_hdr.wi_algo = 0;
	packet->auth_hdr.wi_seq = 1;
	packet->auth_hdr.wi_status = 0;

	DPRINTFN(15, ("%s: auth packet: %30D\n", USBDEVNAME(sc->atu_dev),
		((u_int8_t *)packet)+8, " "));

	err = atu_send_mgmt_packet(sc, ch, sizeof(*packet));
	if (err) {
		DPRINTF(("%s: could not send auth packet\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	/*
	 * TODO: implement shared key auth
	 */
	/*
	packet->algoritm = 1;
	packet->sequence = 3;
	packet->status = 0;

	memcpy(packet->challenge, the_challenge_text, the_challenge_length);

	DPRINTFN(15, ("%s: auth packet: %30D\n", USBDEVNAME(sc->atu_dev),
		((u_int8_t *)packet)+8, " "));

	if (sc->atu_encrypt & ATU_WEP_TX) {
		packet->mgmt_hdr.frame_ctl |= WI_FCTL_WEP;
		DPRINTFN(20, ("%s: ==> WEP on please\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	err = atu_send_mgmt_packet(sc, ch, sizeof(*packet) + challenge_len);
	if (err) {
		DPRINTF(("%s: could not send auth packet 2\n",
		    USBDEVNAME(sc->atu_dev)));
	}
	*/
	return 0;
}

int
atu_associate(struct atu_softc *sc)
{
	usbd_status			err;
	struct atu_chain		*ch;
	u_int8_t			*ptr;
	struct atu_assoc_packet	*packet;

	/*
	 * associate :
	 *  7.2.3.4 - page 62 of 802.11b spec
	 *
	 */

	ch = SLIST_FIRST(&sc->atu_cdata.atu_mgmt_free);
	if (ch == NULL) {
		DPRINTF(("%s: associate: no mgmt transfers left\n",
		    USBDEVNAME(sc->atu_dev)));
		return ENOMEM;
	}
	SLIST_REMOVE_HEAD(&sc->atu_cdata.atu_mgmt_free, atu_list);

	packet = (struct atu_assoc_packet *)ch->atu_buf;

	packet->mgmt_hdr.frame_ctl = WI_FTYPE_MGMT |
	    IEEE80211_FC0_SUBTYPE_ASSOC_REQ;

	packet->capability = 1 + 32;	/* ess & short preamble */
	packet->capability = 1;
	packet->listen_interval = 100;	/* beacon interval */

	ptr = packet->data;
	*ptr++ = WI_VAR_SSID;		/* SSID */
	*ptr++ = sc->atu_ssidlen;
	memcpy(ptr, sc->atu_ssid, sc->atu_ssidlen);
	ptr += sc->atu_ssidlen;

	*ptr++ = WI_VAR_SRATES;		/* supported rates */
	*ptr++ = 0x04;
	*ptr++ = 0x82;
	*ptr++ = 0x84;
	*ptr++ = 0x8b;
	*ptr++ = 0x96;

	DPRINTFN(15, ("%s: associate packet: %50D\n",
	    USBDEVNAME(sc->atu_dev), (u_int8_t *)packet, " "));

	err = atu_send_mgmt_packet(sc, ch, sizeof(*packet) + 2 +
	    sc->atu_ssidlen + 6);
	if (err) {
		DPRINTF(("%s: could not send associate packet\n",
		    USBDEVNAME(sc->atu_dev)));
	}
	return 0;
}

/*
 * Get the state of the DFU unit
 */
int8_t
atu_get_dfu_state(struct atu_softc *sc)
{
	u_int8_t	state;

	if (atu_usb_request(sc, DFU_GETSTATE, 0, 0, 1, &state))
		return -1;
	return state;
}

/*
 * Get MAC opmode
 */
u_int8_t
atu_get_opmode(struct atu_softc *sc, u_int8_t *mode)
{

	return atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x33, 0x0001,
	    0x0000, 1, mode);
}

/*
 * Upload the internal firmware into the device
 */
int
atu_upload_internal_firmware(struct atu_softc *sc)
{
	u_char	state, *ptr = NULL, *firm = NULL, status[6];
	int	block_size, block = 0, err;
	size_t	bytes_left = 0;
	char	*name = NULL;

	/*
	 * Uploading firmware is done with the DFU (Device Firmware Upgrade)
	 * interface. See "Universal Serial Bus - Device Class Specification
	 * for Device Firmware Upgrade" pdf for details of the protocol.
	 * Maybe this could be moved to a seperate 'firmware driver' once more
	 * device drivers need it... For now we'll just do it here.
	 *
	 * Just for your information, the Atmel's DFU descriptor looks like
	 * this:
	 *
	 * 07		size
	 * 21		type
	 * 01		capabilities : only firmware download, need reset
	 *		  after download
	 * 13 05	detach timeout : max 1299ms between DFU_DETACH and
	 *		  reset
	 * 00 04	max bytes of firmware per transaction : 1024
	 */

	/* Choose the right firmware for the device */
	switch (sc->atu_radio) {
	case RadioRFMD:
		name = "atu-rfmd-int";
		break;
	case RadioRFMD2958:
		name = "atu-rfmd2958-int";
		break;
	case RadioRFMD2958_SMC:
		name = "atu-rfmd2958smc-int";
		break;
	case RadioIntersil:
		name = "atu-intersil-int";
		break;
	default:
		break;
	}

	DPRINTF(("%s: loading firmware %s...\n",
	    USBDEVNAME(sc->atu_dev), name));
	err = loadfirmware(name, &firm, &bytes_left);
	if (err != 0) {
		printf("%s: loadfirmware error %d\n",
		    USBDEVNAME(sc->atu_dev), err);
		return (err);
	}

	ptr = firm;
	state = atu_get_dfu_state(sc);

	while (bytes_left >= 0 && state > 0) {
		switch (state) {
		case DFUState_DnLoadSync:
			/* get DFU status */
			err = atu_usb_request(sc, DFU_GETSTATUS, 0, 0 , 6,
			    status);
			if (err) {
				DPRINTF(("%s: dfu_getstatus failed!\n",
				    USBDEVNAME(sc->atu_dev)));
				free(firm, M_DEVBUF);
				return err;
			}
			/* success means state => DnLoadIdle */
			state = DFUState_DnLoadIdle;
			continue;
			break;

		case DFUState_DFUIdle:
		case DFUState_DnLoadIdle:
			if (bytes_left>=DFU_MaxBlockSize)
				block_size = DFU_MaxBlockSize;
			else
				block_size = bytes_left;
			DPRINTFN(15, ("%s: firmware block %d\n",
				USBDEVNAME(sc->atu_dev), block));

			err = atu_usb_request(sc, DFU_DNLOAD, block++, 0,
			    block_size, ptr);
			if (err) {
				DPRINTF(("%s: dfu_dnload failed\n",
				    USBDEVNAME(sc->atu_dev)));
				free(firm, M_DEVBUF);
				return err;
			}

			ptr += block_size;
			bytes_left -= block_size;
			if (block_size == 0)
				bytes_left = -1;
			break;

		default:
			atu_msleep(sc, 100);
			DPRINTFN(20, ("%s: sleeping for a while\n",
			    USBDEVNAME(sc->atu_dev)));
		}

		state = atu_get_dfu_state(sc);
	}
	free(firm, M_DEVBUF);

	if (state != DFUState_ManifestSync) {
		DPRINTF(("%s: state != manifestsync... eek!\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	err = atu_usb_request(sc, DFU_GETSTATUS, 0, 0, 6, status);
	if (err) {
		DPRINTF(("%s: dfu_getstatus failed!\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}

	DPRINTFN(15, ("%s: sending remap\n", USBDEVNAME(sc->atu_dev)));
	err = atu_usb_request(sc, DFU_REMAP, 0, 0, 0, NULL);
	if ((err) && (! sc->atu_quirk & ATU_QUIRK_NO_REMAP)) {
		DPRINTF(("%s: remap failed!\n", USBDEVNAME(sc->atu_dev)));
		return err;
	}

	/* after a lot of trying and measuring I found out the device needs
	 * about 56 miliseconds after sending the remap command before
	 * it's ready to communicate again. So we'll wait just a little bit
	 * longer than that to be sure...
	 */
	atu_msleep(sc, 56+100);

	/* reset the device to get the firmware to boot */
	DPRINTFN(10, ("%s: trying to reset device...\n",
	    USBDEVNAME(sc->atu_dev)));
	err = atu_reset(sc);
	if (err) {
		DPRINTF(("%s: reset failed...\n", USBDEVNAME(sc->atu_dev)));
		return err;
	}

	DPRINTFN(10, ("%s: internal firmware upload done\n",
	    USBDEVNAME(sc->atu_dev)));
	return 0;
}

int
atu_upload_external_firmware(struct atu_softc *sc)
{
	u_char	*ptr = NULL, *firm = NULL, mode, channel;
	int	block_size, block = 0, err;
	size_t	bytes_left = 0;
	char	*name = NULL;

	err = atu_get_opmode(sc, &mode);
	if (err) {
		DPRINTF(("%s: could not get opmode\n",
		    USBDEVNAME(sc->atu_dev)));
		return err;
	}
	DPRINTFN(20, ("%s: opmode: %d\n", USBDEVNAME(sc->atu_dev), mode));

	if (mode == MODE_NETCARD) {
		DPRINTFN(15, ("%s: device doesn't need external "
		    "firmware\n", USBDEVNAME(sc->atu_dev)));
		return 0;
	}
	if (mode != MODE_NOFLASHNETCARD) {
		DPRINTF(("%s: EEK! unexpected opmode=%d\n",
		    USBDEVNAME(sc->atu_dev), mode));
	}

	/*
	 * There is no difference in opmode before and after external firmware
	 * upload with the SMC2662 V.4 . So instead we'll try to read the
	 * channel number. If we succeed, external firmware must have been
	 * already uploaded...
	 */
	err = atu_get_mib(sc, MIB_PHY__CHANNEL, &channel);
	if (! err) {
		DPRINTF(("%s: external firmware has already been "
		    "downloaded\n", USBDEVNAME(sc->atu_dev)));
		return 0;
	}

	switch (sc->atu_radio) {
	case RadioRFMD:
		name = "atu-rfmd-ext";
		break;
	case RadioRFMD2958:
		name = "atu-rfmd2958-ext";
		break;
	case RadioRFMD2958_SMC:
		name = "atu-rfmd2958smc-ext";
		break;
	case RadioIntersil:
		name = "atu-intersil-ext";
		break;
	default:
		bytes_left = 0;
		break;
	}

	DPRINTF(("%s: loading external firmware %s\n",
	    USBDEVNAME(sc->atu_dev), name));
	err = loadfirmware(name, &firm, &bytes_left);
	if (err != 0) {
		printf("%s: loadfirmware error %d\n",
		    USBDEVNAME(sc->atu_dev), err);
		return (err);
	}
	ptr = firm;

	while (bytes_left) {
		if (bytes_left > 1024)
			block_size = 1024;
		else
			block_size = bytes_left;

		DPRINTFN(15, ("%s: block:%d size:%d\n",
		    USBDEVNAME(sc->atu_dev), block, block_size));
		err = atu_usb_request(sc, UT_WRITE_VENDOR_DEVICE, 0x0e,
		    0x0802, block, block_size, ptr);
		if (err) {
			DPRINTF(("%s: could not load external firmware "
			    "block\n", USBDEVNAME(sc->atu_dev)));
			free(firm, M_DEVBUF);
			return err;
		}

		ptr += block_size;
		block++;
		bytes_left -= block_size;
	}
	free(firm, M_DEVBUF);

	err = atu_usb_request(sc, UT_WRITE_VENDOR_DEVICE, 0x0e, 0x0802,
	    block, 0, NULL);
	if (err) {
		DPRINTF(("%s: could not load last zero-length firmware "
		    "block\n", USBDEVNAME(sc->atu_dev)));
		return err;
	}

	/*
	 * The SMC2662w V.4 seems to require some time to do it's thing with
	 * the external firmware... 20 ms isn't enough, but 21 ms works 100
	 * times out of 100 tries. We'll wait a bit longer just to be sure
	 */
	if (sc->atu_quirk & ATU_QUIRK_FW_DELAY) {
		atu_msleep(sc, 21 + 100);
	}

	DPRINTFN(10, ("%s: external firmware upload done\n",
	    USBDEVNAME(sc->atu_dev)));
	return 0;
}

int
atu_get_card_config(struct atu_softc *sc)
{
	struct atu_rfmd_conf		rfmd_conf;
	struct atu_intersil_conf	intersil_conf;
	int				err;

	switch (sc->atu_radio) {

	case RadioRFMD:
	case RadioRFMD2958:
	case RadioRFMD2958_SMC:
		err = atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x33,
		    0x0a02, 0x0000, sizeof(rfmd_conf),
		    (u_int8_t *)&rfmd_conf);
		if (err) {
			DPRINTF(("%s: could not get rfmd config!\n",
			    USBDEVNAME(sc->atu_dev)));
			return err;
		}
		memcpy(sc->atu_mac_addr, rfmd_conf.MACAddr, ETHER_ADDR_LEN);
		break;

	case RadioIntersil:
		err = atu_usb_request(sc, UT_READ_VENDOR_INTERFACE, 0x33,
		    0x0902, 0x0000, sizeof(intersil_conf),
		    (u_int8_t *)&intersil_conf);
		if (err) {
			DPRINTF(("%s: could not get intersil config!\n",
			    USBDEVNAME(sc->atu_dev)));
			return err;
		}
		memcpy(sc->atu_mac_addr, intersil_conf.MACAddr,
		    ETHER_ADDR_LEN);
		break;
	}
	return 0;
}

/*
 * Probe for an AT76c503 chip.
 */
USB_MATCH(atu)
{
	USB_MATCH_START(atu, uaa);
	struct atu_type		*t;

	if (!uaa->iface)
		return(UMATCH_NONE);

	t = atu_devs;
	while(t->atu_vid) {
		if (uaa->vendor == t->atu_vid &&
		    uaa->product == t->atu_pid) {
			return(UMATCH_VENDOR_PRODUCT);
		}
		t++;
	}
	return(UMATCH_NONE);
}

/*
 * this routine gets called from the mgmt thread at least once a second.
 * if we return 0 the thread will go to sleep, if we return 1 we will be
 * called again immediately (like 'continue' does in a while-loop)
 */
int
atu_mgmt_state_machine(struct atu_softc *sc)
{
	struct atu_mgmt	*vars = &sc->atu_mgmt_vars;
	usbd_status		err;
	u_int8_t		statusreq[6];
	int s;

	if (sc->atu_mgmt_flags & ATU_CHANGED_SETTINGS) {
		sc->atu_mgmt_flags &= ~ATU_CHANGED_SETTINGS;
		sc->atu_mgmt_flags &= ~ATU_FOUND_BSSID;
		sc->atu_mgmt_flags &= ~ATU_AUTH_OK;
		sc->atu_mgmt_flags &= ~ATU_RE_AUTH;
		sc->atu_mgmt_flags &= ~ATU_ASSOC_OK;
		sc->atu_mgmt_flags |= ATU_SEARCHING;
		vars->state = STATE_LISTENING;
		vars->retry = 0;
	}

	DPRINTFN(5, ("%s: [state=%s, retry=%d, chan=%d d-chan=%d enc=%d]\n",
	    USBDEVNAME(sc->atu_dev), atu_mgmt_statename[vars->state],
	    vars->retry, sc->atu_channel, sc->atu_desired_channel,
	    sc->atu_encrypt));

	/* Fall back to authentication if needed */
	/* TODO: should we only allow this when in infra-mode? */
	if ((sc->atu_mgmt_flags & ATU_RE_AUTH) &&
	    (vars->state >= STATE_AUTHENTICATING)) {
		vars->state = STATE_AUTHENTICATING;
		sc->atu_mgmt_flags &= ~(ATU_RE_AUTH | ATU_AUTH_OK |
		    ATU_ASSOC_OK);
		vars->retry = 0;
	}

	/* Fall back to association if needed */
	/* TODO: should we only allow this when in infra-mode? */
	if ((sc->atu_mgmt_flags & ATU_RE_ASSOC) &&
	    (vars->state >= STATE_ASSOCIATING)) {
		vars->state = STATE_ASSOCIATING;
		sc->atu_mgmt_flags &= ~(ATU_RE_ASSOC | ATU_AUTH_OK |
		    ATU_ASSOC_OK);
		vars->retry = 0;
	}

	switch (vars->state) {
	case STATE_NONE:
		/* awaiting orders */
		break;
	case STATE_LISTENING:
		/* do some nifty scanning here */

		if (sc->atu_mgmt_flags & ATU_FOUND_BSSID) {
			vars->state = STATE_JOINING;
			vars->retry = 0;
			return 1;
		}
		s = splusb();
		err = atu_get_cmd_status(sc, CMD_JOIN, statusreq);
		if (err) {
			DPRINTF(("%s: get_cmd_status failed in mgmt_loop\n",
			    USBDEVNAME(sc->atu_dev)));
			vars->state = STATE_GIVEN_UP;
			vars->retry = 0;
		}
		if (statusreq[5]==STATUS_IN_PROGRESS) {
			DPRINTFN(10, ("%s: scanning in progress...\n",
			    USBDEVNAME(sc->atu_dev)));
		} else {
			err = atu_start_scan(sc);
			if (vars->retry++ > ATU_SCAN_RETRIES &&
			    sc->atu_mode == AD_HOC_MODE) {
				DPRINTFN(10, ("%s: scanned long enough\n",
				    USBDEVNAME(sc->atu_dev)));
				sc->atu_mgmt_flags &= ~ATU_SEARCHING;
				vars->state = STATE_CREATING_IBSS;
				vars->retry = 0;
			}
			if (err) {
				DPRINTF(("%s: get_cmd_failed in mgmt_loop\n",
				    USBDEVNAME(sc->atu_dev)));
				vars->state = STATE_GIVEN_UP;
				vars->retry = 0;
			}
		}
		splx(s);
		break;
	case STATE_JOINING:
		DPRINTFN(10, ("%s: going to join\n",
		    USBDEVNAME(sc->atu_dev)));
		err = atu_join(sc);
		if (err) {
			if (vars->retry++ > ATU_JOIN_RETRIES) {
				if (sc->atu_mode == AD_HOC_MODE)
					vars->state = STATE_CREATING_IBSS;
				else
					vars->state = STATE_GIVEN_UP;
				vars->retry = 0;
			}
			DPRINTF(("%s: error joining\n",
			    USBDEVNAME(sc->atu_dev)));
		} else {
			if (sc->atu_mode == AD_HOC_MODE)
				vars->state = STATE_HAPPY_NETWORKING;
			else
				vars->state = STATE_AUTHENTICATING;
			vars->retry = 0;
		}
		break;
	case STATE_AUTHENTICATING:
		if (sc->atu_mgmt_flags & ATU_AUTH_OK) {
			vars->state = STATE_ASSOCIATING;
			vars->retry = 0;
			return 1;
		}

		DPRINTFN(10, ("%s: trying authentication\n",
		    USBDEVNAME(sc->atu_dev)));
		atu_authenticate(sc);
		if (vars->retry++ > ATU_AUTH_RETRIES) {
			vars->state = STATE_GIVEN_UP;
			DPRINTF(("%s: error authenticating...\n",
			    USBDEVNAME(sc->atu_dev)));
		}
		break;
	case STATE_ASSOCIATING:
		if (sc->atu_mgmt_flags & ATU_ASSOC_OK) {
			vars->state = STATE_HAPPY_NETWORKING;
			vars->retry = 0;
			return 1;
		}
		DPRINTFN(10, ("%s: trying to associate\n",
		    USBDEVNAME(sc->atu_dev)));
		atu_associate(sc);
		if (vars->retry++ > ATU_ASSOC_RETRIES) {
			vars->state = STATE_GIVEN_UP;
			DPRINTF(("%s: error associating...\n",
			    USBDEVNAME(sc->atu_dev)));
		}
		break;
	case STATE_CREATING_IBSS:
		DPRINTFN(10, ("%s: trying to create IBSS\n",
		    USBDEVNAME(sc->atu_dev)));
		err = atu_start_ibss(sc);
		if (err) {
			if (vars->retry++ > ATU_IBSS_RETRIES)
				vars->state = STATE_GIVEN_UP;
			DPRINTF(("%s: error creating IBSS...\n",
			    USBDEVNAME(sc->atu_dev)));
		} else {
			vars->state = STATE_HAPPY_NETWORKING;
			vars->retry = 0;
		}
		break;
	case STATE_HAPPY_NETWORKING:
		/* happy networking
		 *
		 * TODO:
		 * we should bounce back to previous states from here
		 * on beacon timeout.
		 */
		break;
	case STATE_GIVEN_UP:
		/*
		 * can only leave this state if someone changes the
		 * config
		 */
		break;
	}

	if (vars->state == STATE_HAPPY_NETWORKING)
		sc->atu_mgmt_flags |= ATU_NETWORK_OK;
	else
		sc->atu_mgmt_flags &= ~ATU_NETWORK_OK;
	return 0;
}

void
atu_mgmt_loop(void *arg)
{
	struct atu_softc	*sc = arg;
	int			again;
	int s;

	DPRINTFN(10, ("%s: mgmt task initialised\n",
	    USBDEVNAME(sc->atu_dev)));

	sc->atu_mgmt_vars.state = STATE_NONE;
	sc->atu_mgmt_vars.retry = 0;

	while (!sc->atu_dying) {
		s = splnet();
		again = atu_mgmt_state_machine(sc);
		while (again)
			again = atu_mgmt_state_machine(sc);
		splx(s);

		/*
		 * wait for something to happen (but not too long :)
		 * if someone changes the config or a mgmt packet is received
		 * we will be waken up
		 */
		tsleep(sc, PZERO | PCATCH, "atum", ATU_MGMT_INTERVAL);
	}

	DPRINTFN(10, ("%s: mgmt thread stops now...\n",
	    USBDEVNAME(sc->atu_dev)));
	sc->atu_dying++;
	sc->atu_mgmt_flags &= ~ATU_TASK_RUNNING;
	kthread_exit(0);
}

int
atu_media_change(struct ifnet *ifp)
{
	struct atu_softc	*sc;
	struct ifmedia_entry	*ime;

	sc = ifp->if_softc;
	ime = sc->atu_media.ifm_cur;

	/* TODO: fully implement - see if_wi.c @ 1189 */

	DPRINTFN(10, ("%s: subtype=%d %d\n", USBDEVNAME(sc->atu_dev),
	    IFM_SUBTYPE(ime->ifm_media), ime->ifm_media));

	if ((ime->ifm_media & IFM_IEEE80211_ADHOC) &&
	    (sc->atu_mode != AD_HOC_MODE)) {
		DPRINTFN(10, ("%s: mode changed to adhoc\n",
		    USBDEVNAME(sc->atu_dev)));
		sc->atu_mode = AD_HOC_MODE;
		sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
		wakeup(sc);
	}

	if ((!(ime->ifm_media & IFM_IEEE80211_ADHOC)) &&
	    (sc->atu_mode != INFRASTRUCTURE_MODE)) {
		DPRINTFN(10, ("%s: mode changed to infra\n",
		    USBDEVNAME(sc->atu_dev)));
		sc->atu_mode = INFRASTRUCTURE_MODE;
		sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
		wakeup(sc);
	}

	DPRINTFN(10, ("%s: media_change...\n", USBDEVNAME(sc->atu_dev)));
	return 0;
}

void
atu_media_status(struct ifnet *ifp, struct ifmediareq *req)
{
	struct atu_softc	*sc;

	sc = ifp->if_softc;

	/* TODO: fully implement */

	req->ifm_status = IFM_AVALID;
	req->ifm_active = IFM_IEEE80211;

	if (sc->atu_mgmt_flags & ATU_NETWORK_OK)
		req->ifm_status |= IFM_ACTIVE;

	/* req->ifm_active |= ieee80211_rate2media(2*11, IEEE80211_T_DS); */

	if (sc->atu_mode == AD_HOC_MODE) {
		req->ifm_active |= IFM_IEEE80211_ADHOC;
	}
	DPRINTFN(10, ("%s: atu_media_status\n", USBDEVNAME(sc->atu_dev)));
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(atu)
{
	USB_ATTACH_START(atu, sc, uaa);
	char				devinfo[1024];
	struct ifnet			*ifp;
	usbd_status			err;
	usbd_device_handle		dev = uaa->device;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int				i, s;
	u_int8_t			mode;
	struct atu_type		*t;
	struct atu_fw			fw;

	usbd_devinfo(uaa->device, 0, devinfo, sizeof devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->atu_dev), devinfo);

	err = usbd_set_config_no(dev, ATU_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->atu_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_device2interface_handle(dev, ATU_IFACE_IDX,
	    &sc->atu_iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
			USBDEVNAME(sc->atu_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->atu_unit = self->dv_unit;
	sc->atu_udev = dev;

	id = usbd_get_interface_descriptor(sc->atu_iface);

	/*
	 * look up the radio_type for the device
	 * basically does the same as USB_MATCH
	 */
	t = atu_devs;
	while(t->atu_vid) {
		if (uaa->vendor == t->atu_vid &&
		    uaa->product == t->atu_pid) {
			sc->atu_radio = t->atu_radio;
			sc->atu_quirk = t->atu_quirk;
		}
		t++;
	}

	s = splnet();

	/*
	 * Check in the interface descriptor if we're in DFU mode
	 * If we're in DFU mode, we upload the external firmware
	 * If we're not, the PC must have rebooted without power-cycling
	 * the device.. I've tried this out, a reboot only requeres the
	 * external firmware to be reloaded :)
	 *
	 * Hmm. The at76c505a doesn't report a DFU descriptor when it's
	 * in DFU mode... Let's just try to get the opmode
	 */
	err = atu_get_opmode(sc, &mode);
	if (err || (mode != MODE_NETCARD &&
	    mode != MODE_NOFLASHNETCARD)) {
		DPRINTF(("%s: starting internal firmware download\n",
		    USBDEVNAME(sc->atu_dev)));

		/* upload internal firmware */
		err = atu_upload_internal_firmware(sc);
		if (err) {
			splx(s);
			USB_ATTACH_ERROR_RETURN;
		}

		DPRINTFN(10, ("%s: done...\n", USBDEVNAME(sc->atu_dev)));
		splx(s);
		USB_ATTACH_NEED_RESET;
	}

	uaa->iface = sc->atu_iface;

	/* upload external firmware */
	DPRINTF(("%s: starting external firmware download\n",
	    USBDEVNAME(sc->atu_dev)));
	err = atu_upload_external_firmware(sc);
	if (err) {
		splx(s);
		USB_ATTACH_ERROR_RETURN;
	}

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (!ed) {
			DPRINTF(("%s: num_endp:%d\n", USBDEVNAME(sc->atu_dev),
			    uaa->iface->idesc->bNumEndpoints));
			DPRINTF(("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->atu_dev), i));
			splx(s);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->atu_ed[ATU_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->atu_ed[ATU_ENDPT_TX] = ed->bEndpointAddress;
		}
	}

	/* read device config & get MAC address */
	err = atu_get_card_config(sc);
	if (err) {
		DPRINTF(("%s: could not get card cfg!\n",
		    USBDEVNAME(sc->atu_dev)));
		splx(s);
		USB_ATTACH_ERROR_RETURN;
	}

	/* DEBUG : try to get firmware version */
	err = atu_get_mib(sc, MIB_FW_VERSION, sizeof(fw), 0,
	    (u_int8_t *)&fw);
	if (!err) {
#if 0
		DPRINTFN(15, ("%s: firmware: maj:%d min:%d patch:%d "
		    "build:%d\n", USBDEVNAME(sc->atu_dev), fw.major, fw.minor,
		    fw.patch, fw.build));
#endif
	} else {
		DPRINTF(("%s: get firmware version failed\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	/* Show the world our MAC address */
	printf("%s: address %s\n", USBDEVNAME(sc->atu_dev),
	    ether_sprintf(sc->atu_mac_addr));

	bcopy(sc->atu_mac_addr,
	    (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	for (i=0; i<ATU_AVG_TIME; i++)
		sc->atu_signalarr[i] = 0;
	sc->atu_signaltotal = 0;
	sc->atu_signalptr = 0;
	sc->atu_cdata.atu_tx_inuse = 0;
	sc->atu_encrypt = ATU_WEP_OFF;
	sc->atu_wepkeylen = ATU_WEP_104BITS;
	sc->atu_wepkey = 0;
	sc->atu_mgmt_flags = 0;

	bzero(sc->atu_bssid, ETHER_ADDR_LEN);
	sc->atu_ssidlen = strlen(ATU_DEFAULT_SSID);
	memcpy(sc->atu_ssid, ATU_DEFAULT_SSID, sc->atu_ssidlen);
	sc->atu_channel = ATU_DEFAULT_CHANNEL;
	sc->atu_desired_channel = IEEE80211_CHAN_ANY;
	sc->atu_mode = INFRASTRUCTURE_MODE;
	sc->atu_encrypt = ATU_WEP_OFF;

	/* Initialise transfer lists */
	SLIST_INIT(&sc->atu_cdata.atu_tx_free);
	SLIST_INIT(&sc->atu_cdata.atu_mgmt_free);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	strncpy(ifp->if_xname, USBDEVNAME(sc->atu_dev), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = atu_ioctl;
	ifp->if_start = atu_start;
	ifp->if_watchdog = atu_watchdog;
	ifp->if_baudrate = 10000000;
	ifp->if_mtu = ATU_DEFAULT_MTU;

	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	Ether_ifattach(ifp, sc->atu_mac_addr);

	sc->atu_dying = 0;

	/* setup ifmedia interface */
	ifmedia_init(&sc->atu_media, 0, atu_media_change,
	    atu_media_status);

#define ADD(s, o)       ifmedia_add(&sc->atu_media, \
	IFM_MAKEWORD(IFM_IEEE80211, (s), (o), 0), 0, NULL)

	ADD(IFM_AUTO, 0);
	ADD(IFM_AUTO, IFM_IEEE80211_ADHOC);

	/*
	 * TODO:
	 * add a list of supported rates here.
	 * (can't do that as long as we only support 'auto fallback'
	 *
	for (i = 0; i < nrate; i++) {
		r = ic->ic_sup_rates[i];
		mword = ieee80211_rate2media(r, IEEE80211_T_DS);
		if (mword == 0)
			continue;
		printf("%s%d%sMbps", (i != 0 ? " " : ""),
		    (r & IEEE80211_RATE_VAL) / 2, ((r & 0x1) != 0 ? ".5" : ""));
		ADD(mword, 0);
		if (ic->ic_flags & IEEE80211_F_HASHOSTAP)
			ADD(mword, IFM_IEEE80211_HOSTAP);
		if (ic->ic_flags & IEEE80211_F_HASIBSS)
			ADD(mword, IFM_IEEE80211_ADHOC);
		ADD(mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
	}
	printf("\n");
	*/

	ADD(11, 0);
	ADD(11, IFM_IEEE80211_ADHOC);

	ifmedia_set(&sc->atu_media, IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0,
	    0));
#undef ADD

	splx(s);
	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(atu)
{
	USB_DETACH_START(atu, sc);
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	atu_stop(sc);

	ether_ifdetach(ifp);
	if_detach(ifp);

	if (sc->atu_ep[ATU_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->atu_ep[ATU_ENDPT_TX]);
	if (sc->atu_ep[ATU_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->atu_ep[ATU_ENDPT_RX]);
	return(0);
}

int
atu_activate(device_ptr_t self, enum devact act)
{
	struct atu_softc *sc = (struct atu_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->atu_ec.ec_if);
		sc->atu_dying = 1;
		break;
	}
	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int
atu_newbuf(struct atu_softc *sc, struct atu_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			DPRINTF(("%s: no memory for rx list\n",
			    USBDEVNAME(sc->atu_dev)));
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			DPRINTF(("%s: no memory for rx list\n",
			    USBDEVNAME(sc->atu_dev)));
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	c->atu_mbuf = m_new;
	return(0);
}

int
atu_rx_list_init(struct atu_softc *sc)
{
	struct atu_cdata	*cd = &sc->atu_cdata;
	struct atu_chain	*c;
	int			i;

	DPRINTFN(15, ("%s: atu_rx_list_init: enter\n",
	    USBDEVNAME(sc->atu_dev)));

	for (i = 0; i < ATU_RX_LIST_CNT; i++) {
		c = &cd->atu_rx_chain[i];
		c->atu_sc = sc;
		c->atu_idx = i;
		if (c->atu_xfer != NULL) {
			printf("UGH RX\n");
		}
		if (c->atu_xfer == NULL) {
			c->atu_xfer = usbd_alloc_xfer(sc->atu_udev);
			if (c->atu_xfer == NULL)
				return (ENOBUFS);
			c->atu_buf = usbd_alloc_buffer(c->atu_xfer,
			    ATU_RX_BUFSZ);
			if (c->atu_buf == NULL) /* XXX free xfer */
				return (ENOBUFS);
			if (atu_newbuf(sc, c, NULL) == ENOBUFS) /* XXX free? */
				return(ENOBUFS);
		}
	}

	return (0);
}

int
atu_xfer_list_init(struct atu_softc *sc, struct atu_chain *ch,
    int listlen, int need_mbuf, int bufsize, struct atu_list_head *list)
{
	struct atu_cdata	*cd;
	int			i;

	cd = &sc->atu_cdata;

	DPRINTFN(15, ("%s: list init (%d entries of %d bytes)\n",
	    USBDEVNAME(sc->atu_dev), listlen, bufsize));

	for (i = 0; i < listlen; i++) {
		ch->atu_sc = sc;
		ch->atu_idx = i;
		if (ch->atu_xfer == NULL) {
			ch->atu_xfer = usbd_alloc_xfer(sc->atu_udev);
			if (ch->atu_xfer == NULL)
				return(ENOBUFS);
		}

		if (need_mbuf) {
			if (atu_newbuf(sc, ch, NULL) == ENOBUFS)
				return(ENOBUFS);
		} else {
			ch->atu_mbuf = NULL;
		}

		if ((bufsize > 0) && (ch->atu_buf == NULL)) {
			ch->atu_buf = usbd_alloc_buffer(ch->atu_xfer,
			    bufsize);
			if (ch->atu_buf == NULL)
				return(ENOBUFS);
		}

		if (list != NULL) {
			SLIST_INSERT_HEAD(list, ch, atu_list);
		}

		ch++;
	}
	return(0);
}

void
atu_xfer_list_free(struct atu_softc *sc, struct atu_chain *ch,
    int listlen)
{
	int			i;

	/* Free resources. */
	for (i = 0; i < listlen; i++) {
		if (ch[i].atu_buf != NULL)
			ch[i].atu_buf = NULL;
		if (ch[i].atu_mbuf != NULL) {
			m_freem(ch[i].atu_mbuf);
			ch[i].atu_mbuf = NULL;
		}
		if (ch[i].atu_xfer != NULL) {
			usbd_free_xfer(ch[i].atu_xfer);
			ch[i].atu_xfer = NULL;
		}
	}
}

void
atu_print_beacon(struct atu_softc *sc, struct atu_rxpkt *pkt)
{
	u_int8_t		*ptr;
	struct tlv		*tlv;
	u_int8_t		*end;
	u_int8_t		tmp;
	u_int8_t		rate;

	/* Let's have a closer look at this beacon... */
	ptr = (u_int8_t *)pkt->WiHeader.addr4 + 12;
	end = ptr + pkt->AtHeader.wlength - 24 - 12 - 4;
	tlv = (struct tlv *)ptr;

	while ((ptr<end) && (ptr + 2 + tlv->length <= end)) {
		switch (tlv->type) {
		case WI_VAR_SSID: /* SSID */
			/* sanity check */
			if (tlv->length > 32)
				break;

			tmp = tlv->value[tlv->length];
			tlv->value[tlv->length] = 0;
			DPRINTF(("%s:  ssid=[%s]\n", USBDEVNAME(sc->atu_dev),
			    tlv->value));
			tlv->value[tlv->length] = tmp;
			break;
		case WI_VAR_SRATES: /* Supported rates */
			for (rate=0; rate<tlv->length; rate++) {
				tmp = tlv->value[rate] & (~0x80);
				DPRINTF(("%s:  rate: %d kbps (%02x)\n",
				    USBDEVNAME(sc->atu_dev), 500 * tmp,
				    tlv->value[rate]));
			}
			break;
		case WI_VAR_DS: /* DS (channel) */
			DPRINTF(("%s:  channel=%d\n",
			    USBDEVNAME(sc->atu_dev), *tlv->value));
			break;
		default :
			DPRINTF(("%s:  tlv: t=%02x l=%02x v[0]=%02x\n",
			    USBDEVNAME(sc->atu_dev), tlv->type, tlv->length,
			    tlv->value[0]));
		}

		ptr += 2 + tlv->length;
		tlv = (struct tlv *)ptr;
	}
}

void
atu_handle_mgmt_packet(struct atu_softc *sc, struct atu_rxpkt *pkt)
{
	u_int8_t		*ptr;
	struct tlv		*tlv;
	u_int8_t		*end;
	u_int8_t		tmp;
	int			match;
	int			match_channel = 1;
	struct wi_80211_beacon	*beacon;
	struct wi_mgmt_auth_hdr	*auth;
	struct wi_mgmt_deauth_hdr	*deauth;
	struct wi_mgmt_disas_hdr	*deassoc;
	struct wi_mgmt_asresp_hdr	*assoc;

	switch (pkt->WiHeader.frame_ctl & WI_FCTL_STYPE) {
	case WI_STYPE_MGMT_AUTH:
		DPRINTFN(15, ("%s: received auth response...\n",
		    USBDEVNAME(sc->atu_dev)));

		auth = (struct wi_mgmt_auth_hdr *)pkt->WiHeader.addr4;

		if (auth->wi_seq != 2) {
			DPRINTF(("%s: auth wrong seq.nr (%x)\n",
			    USBDEVNAME(sc->atu_dev), auth->wi_seq));
			break;
		}
		if (auth->wi_status != 0) {
			DPRINTF(("%s: auth status error (%x)\n",
			    USBDEVNAME(sc->atu_dev), auth->wi_status));
			break;
		}

		/* TODO: should check bssid & mac ! */

		sc->atu_mgmt_flags |= ATU_AUTH_OK;
		wakeup(sc);

		/*
		 * TODO: DAAN Daan daan - Add challenge text blah blah
		 * (for shared-key systems)
		 */
		/*
		memcpy(the_challenge_text, ((u_int8_t *)&pkt->WiHeader) + 30,
		    the_challenge_length);
		DPRINTFN(15, ("%s: challenge= %100D\n",
		    USBDEVNAME(sc->atu_dev), the_challende_text, " "));
		*/
		break;

	case WI_STYPE_MGMT_DEAUTH:
		DPRINTF(("%s: the AP has de-authenticated us\n",
		    USBDEVNAME(sc->atu_dev)));

		deauth = (struct wi_mgmt_deauth_hdr *)pkt->WiHeader.addr4;

		DPRINTF(("%s: de-authentication reason: %04x\n",
		    USBDEVNAME(sc->atu_dev), deauth->wi_reason));

		/* TODO: should check bssid & mac ! */

		/* wake up the state machine to get us re-authenticated */
		sc->atu_mgmt_flags |= ATU_RE_AUTH;
		wakeup(sc);
		break;

	case WI_STYPE_MGMT_ASRESP:
		DPRINTFN(15, ("%s: received assoc response...\n",
		    USBDEVNAME(sc->atu_dev)));

		assoc = (struct wi_mgmt_asresp_hdr *)pkt->WiHeader.addr4;

		if (assoc->wi_status == 0) {
			sc->atu_mgmt_flags |= ATU_ASSOC_OK;
			wakeup(sc);
		} else {
			DPRINTF(("%s: assoc status error (%x)\n",
			    USBDEVNAME(sc->atu_dev), assoc->wi_status));
			break;
		}

		/* TODO: should check bssid & mac ! */
		wakeup(sc);

		break;

	case WI_STYPE_MGMT_DISAS:
		DPRINTF(("%s: the AP has de-associated us",
		    USBDEVNAME(sc->atu_dev)));

		deassoc = (struct wi_mgmt_disas_hdr *)pkt->WiHeader.addr4;

		DPRINTF(("%s: de-association reason: %04x\n",
		    USBDEVNAME(sc->atu_dev), deassoc->wi_reason));

		/* TODO: should check bssid & mac ! */

		/* wake up the state machine to get us re-authenticated */
		sc->atu_mgmt_flags |= ATU_RE_ASSOC;
		wakeup(sc);
		break;

	case WI_STYPE_MGMT_PROBERESP:
		DPRINTFN(20, ("%s: PROBE RESPONSE\n",
		    USBDEVNAME(sc->atu_dev)));
		/* FALLTHROUGH */
	case WI_STYPE_MGMT_BEACON:

		beacon = (struct wi_80211_beacon *)&pkt->WiHeader.addr4;

		/* Show beacon src MAC & signal strength */
		DPRINTFN(18, ("%s: mgmt bssid=%s", USBDEVNAME(sc->atu_dev),
		    ether_sprintf(pkt->WiHeader.addr3)));
		DPRINTFN(18, (" mac=%s signal=%d\n",
		   ether_sprintf(pkt->WiHeader.addr2), pkt->AtHeader.rssi));

#ifdef ATU_DEBUG
		if (atudebug > 20) {
			/*
			 * calculate average signal strength, can be very
			 *  usefull when precisely aiming antenna's
			 * NOTE: this is done on ALL beacons, so multiple
			 * stations can end up in the average. this only
			 * works well if we're only receiving one station.
			 */
			sc->atu_signaltotal += pkt->AtHeader.rssi;
			sc->atu_signaltotal -=
			    sc->atu_signalarr[sc->atu_signalptr];
			sc->atu_signalarr[sc->atu_signalptr] =
			    pkt->AtHeader.rssi;
			sc->atu_signalptr=(sc->atu_signalptr+1) %
			    ATU_AVG_TIME;
			DPRINTF(("%s: mgmt mac=%s signal=%02d ptr=%02d "
			    "avg=%02d.%02d\n", USBDEVNAME(sc->atu_dev),
			    ether_sprintf(pkt->WiHeader.addr2),
			    pkt->AtHeader.rssi, sc->atu_signalptr,
			    sc->atu_signaltotal / ATU_AVG_TIME,
			    (sc->atu_signaltotal * 100 / ATU_AVG_TIME) %
			    100));
		}
#endif

		DPRINTFN(18, ("%s: mgmt capabilities=%04x (mode=%s, wep=%s, "
		    "short-preamble=%s)\n", USBDEVNAME(sc->atu_dev),
		    beacon->flags,
		    (beacon->flags & IEEE80211_CAPINFO_ESS) ?
		    "infra" : "ad-hoc",
		    (beacon->flags & IEEE80211_CAPINFO_PRIVACY) ?
		    "on" : "off",
		    (beacon->flags & IEEE80211_CAPINFO_SHORT_PREAMBLE) ?
		    "yes" : "no"));

#ifdef ATU_DEBUG
		if (atudebug > 18)
			atu_print_beacon(sc, pkt);
#endif
		if (!(sc->atu_mgmt_flags & ATU_SEARCHING))
			break;

		/* Let's have a closer look at this beacon... */
		ptr = (u_int8_t *)pkt->WiHeader.addr4 + 12;
		end = ptr + pkt->AtHeader.wlength - 24 - 12 - 4;
		tlv = (struct tlv *)ptr;
		match = 0;
		while ((ptr<end) && (ptr + 2 + tlv->length <= end)) {
			switch (tlv->type) {
			case WI_VAR_SSID: /* SSID */
				/* sanity check */
				if (tlv->length > 32)
					break;

				tmp = tlv->value[tlv->length];
				tlv->value[tlv->length] = 0;
				sc->atu_ssid[sc->atu_ssidlen] = 0;
				if (!strcmp(tlv->value, sc->atu_ssid)) {
					match = 1;
				}
				tlv->value[tlv->length] = tmp;
				break;
			case WI_VAR_SRATES: /* Supported rates */
				/*
				 * TODO: should check if we support all
				 *  mandatory rates
				 */
				break;
			case WI_VAR_DS: /* DS (channel) */
				if (match)
					match_channel = *tlv->value;
				break;
			}

			ptr += 2 + tlv->length;
			tlv = (struct tlv *)ptr;
		}

		/* check mode... */
		beacon = (struct wi_80211_beacon *)&pkt->WiHeader.addr4;
		if (match) {
			if ((sc->atu_mode == AD_HOC_MODE) &&
			    (beacon->flags & IEEE80211_CAPINFO_ESS)) {
				match = 0;
				DPRINTF(("%s: SSID matches, but we're in "
				    "adhoc mode instead of infra\n",
				    USBDEVNAME(sc->atu_dev)));
			}
			if ((sc->atu_mode == INFRASTRUCTURE_MODE) &&
			    (!(beacon->flags & IEEE80211_CAPINFO_ESS))) {
				match = 0;
				DPRINTF(("%s: SSID matches, but we're in "
				    "infra mode instead of adhoc\n",
				    USBDEVNAME(sc->atu_dev)));
			}
			if ((sc->atu_desired_channel != IEEE80211_CHAN_ANY) &&
			    (match_channel != sc->atu_desired_channel)) {
				match = 0;
				DPRINTF(("%s: SSID matches, but the channel "
				    "doesn't (%d != %d)\n",
				    USBDEVNAME(sc->atu_dev), match_channel,
				    sc->atu_desired_channel));
			}

			if (!match) {
				break;
			}
		}

		if (match) {
			DPRINTF(("%s: ==> MATCH! (BSSID=%s, ch=%d)\n",
			    USBDEVNAME(sc->atu_dev),
			    ether_sprintf(pkt->WiHeader.addr3),
			    match_channel));

			/*
			 * TODO: should do some channel-checking here instead
			 * of just ignoring the channel the user sets
			 */

			memcpy(sc->atu_bssid, pkt->WiHeader.addr3,
			    ETHER_ADDR_LEN);
			sc->atu_channel = match_channel;

			sc->atu_mgmt_flags &= ~ATU_SEARCHING;
			sc->atu_mgmt_flags |= ATU_FOUND_BSSID;
		}

		break;

	default:
		DPRINTF(("%s: FIXME: unhandled mgmt type! (stype=%x)\n",
		    USBDEVNAME(sc->atu_dev),
		    pkt->WiHeader.frame_ctl & WI_FCTL_STYPE));
	}
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
atu_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct atu_chain	*c = (struct atu_chain *)priv;
	struct atu_softc	*sc = c->atu_sc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mbuf		*m;
	u_int32_t		total_len;
	int			s;

	struct atu_rxpkt	*pkt;
	struct ether_header	*eth_hdr;
	int			offset;

	DPRINTFN(25, ("%s: atu_rxeof: enter\n", USBDEVNAME(sc->atu_dev)));

	if (sc->atu_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	DPRINTFN(25, ("%s: got a packet\n", USBDEVNAME(sc->atu_dev)));

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: status != USBD_NORMAL_COMPLETION\n",
		    USBDEVNAME(sc->atu_dev)));
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
#if 0
		if (status == USBD_IOERROR) {
			DPRINTF(("%s: rx: EEK! lost device?\n",
			    USBDEVNAME(sc->atu_dev)));

			/*
			 * My experience with USBD_IOERROR is that trying to
			 * restart the transfer will always fail and we'll
			 * keep on looping restarting transfers untill someone
			 * pulls the plug of the device.
			 * So we don't restart the transfer, but just let it
			 * die... If someone knows of a situation where we can
			 * recover from USBD_IOERROR, let me know.
			 */
			splx(s);
			return;
		}
#endif /* 0 */

		if (usbd_ratecheck(&sc->atu_rx_notice)) {
			DPRINTF(("%s: usb error on rx: %s\n",
			    USBDEVNAME(sc->atu_dev), usbd_errstr(status)));
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(
			    sc->atu_ep[ATU_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	if (total_len <= 1) {
		DPRINTF(("%s: atu_rxeof: too short\n",
		    USBDEVNAME(sc->atu_dev)));
		goto done;
	}

	m = c->atu_mbuf;
	memcpy(mtod(m, char *), c->atu_buf, total_len);

	pkt = mtod(m, struct atu_rxpkt *);

	DPRINTFN(25, ("%s: -- RX (rate=%d enc=%d) mac=%s",
	    USBDEVNAME(sc->atu_dev), pkt->AtHeader.rx_rate,
	    (pkt->WiHeader.frame_ctl & WI_FCTL_WEP) != 0,
	    ether_sprintf(pkt->WiHeader.addr3)));
	DPRINTFN(25, (" bssid=%s\n", ether_sprintf(pkt->WiHeader.addr2)));

	if (pkt->WiHeader.frame_ctl & WI_FCTL_WEP) {
		DPRINTFN(25, ("%s: WEP enabled on RX\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	/* Is it a managment packet? */
	if ((pkt->WiHeader.frame_ctl & WI_FCTL_FTYPE) == WI_FTYPE_MGMT) {
		atu_handle_mgmt_packet(sc, pkt);
		goto done;
	}

	/* Everything but data packets we just ignore from here */
	if ((pkt->WiHeader.frame_ctl & WI_FCTL_FTYPE) != WI_FTYPE_DATA) {
		DPRINTFN(25, ("%s: ---- not a data packet? ---\n",
		    USBDEVNAME(sc->atu_dev)));
		goto done;
	}

	/* Woohaa! It's an ethernet packet! */
	DPRINTFN(25, ("%s: received a packet! rx-rate: %d\n",
	    USBDEVNAME(sc->atu_dev), pkt->AtHeader.rx_rate));

	/* drop non-encrypted packets if wep-mode=on */
	if ((!(pkt->WiHeader.frame_ctl & WI_FCTL_WEP)) &&
	    (sc->atu_encrypt & ATU_WEP_RX)) {
		DPRINTFN(25, ("%s: dropping RX packet. (wep=off)\n",
		    USBDEVNAME(sc->atu_dev)));
		goto done;
	}

	DPRINTFN(25, ("%s: rx frag:%02x rssi:%02x q:%02x nl:%02x time:%d\n",
	    USBDEVNAME(sc->atu_dev), pkt->AtHeader.fragmentation,
	    pkt->AtHeader.rssi, pkt->AtHeader.link_quality,
	    pkt->AtHeader.noise_level, pkt->AtHeader.rx_time));

	/* Do some sanity checking... */
	if (total_len < sizeof(struct ether_header)) {
		DPRINTFN(25, ("%s: Packet too small?? (size:%d)\n",
		    USBDEVNAME(sc->atu_dev), total_len));
		ifp->if_ierrors++;
		goto done;
	}
	/* if (total_len > 1514) { */
	if (total_len > 1548) {
		DPRINTF(("%s: AAARRRGGGHHH!! Invalid packet size? (%d)\n",
		    USBDEVNAME(sc->atu_dev), total_len));
		ifp->if_ierrors++;
		goto done;
	}

	/*
	 * Copy src & dest mac to the right place (overwriting part of the
	 * 802.11 header)
	 */
	eth_hdr = (struct ether_header *)(pkt->Packet - 2 * ETHER_ADDR_LEN);

	switch (pkt->WiHeader.frame_ctl & (WI_FCTL_TODS | WI_FCTL_FROMDS)) {
	case 0:
		/* ad-hoc: copy order doesn't matter here */
		memcpy(eth_hdr->ether_shost, pkt->WiHeader.addr2,
		    ETHER_ADDR_LEN);
		memcpy(eth_hdr->ether_dhost, pkt->WiHeader.addr1,
		    ETHER_ADDR_LEN);
		break;

	case WI_FCTL_FROMDS:
		/* infra mode: MUST be done in this order! */
		memcpy(eth_hdr->ether_shost, pkt->WiHeader.addr3,
		    ETHER_ADDR_LEN);
		memcpy(eth_hdr->ether_dhost, pkt->WiHeader.addr1,
		    ETHER_ADDR_LEN);

		DPRINTFN(25, ("%s: infra decap (%d bytes)\n",
		    USBDEVNAME(sc->atu_dev), pkt->AtHeader.wlength));
		DPRINTFN(25, ("%s: RX: %50D\n", USBDEVNAME(sc->atu_dev),
		    (u_int8_t *)&pkt->WiHeader, " "));
		break;

	default:
		DPRINTFN(25, ("%s: we shouldn't receive this (f_cntl=%02x)\n",
		    USBDEVNAME(sc->atu_dev), pkt->WiHeader.frame_ctl));
	}

	/* calculate 802.3 packet length (= packet - 802.11 hdr - fcs) */
	total_len = pkt->AtHeader.wlength - sizeof(struct wi_80211_hdr) +
	    2 * ETHER_ADDR_LEN - 4;

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = &sc->arpcom.ac_if;

	/* Adjust mbuf for headers */
	offset = sizeof(struct at76c503_rx_buffer) +
	    sizeof(struct wi_80211_hdr) - 12;
	m->m_pkthdr.len = m->m_len = total_len + offset;
	m_adj(m, offset);

	s = splnet();

	if (atu_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	IF_INPUT(ifp, m);

done1:
	splx(s);
done:
	/* Setup new transfer. */
	usbd_setup_xfer(c->atu_xfer, sc->atu_ep[ATU_ENDPT_RX],
	    c, c->atu_buf, ATU_RX_BUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, atu_rxeof);
	usbd_transfer(c->atu_xfer);
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
void
atu_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct atu_softc	*sc;
	struct atu_chain	*c;
	struct ifnet		*ifp;
	usbd_status		err;
	int s;

	c = priv;
	sc = c->atu_sc;
	s = splusb();

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	c->atu_in_xfer = 0;

	if (c->atu_mbuf != NULL) {
		/* put it back on the tx_list */
		sc->atu_cdata.atu_tx_inuse--;
		SLIST_INSERT_HEAD(&sc->atu_cdata.atu_tx_free, c,
		    atu_list);
	} else {
		/* put it back on the mgmt_list */
		SLIST_INSERT_HEAD(&sc->atu_cdata.atu_mgmt_free, c,
		    atu_list);
	}
	/*
	 * turn off active flag if we're done transmitting.
	 * we don't depend on the active flag anywhere. do we still need to
	 * set it then?
	 */
	if (sc->atu_cdata.atu_tx_inuse == 0) {
		ifp->if_flags &= ~IFF_OACTIVE;
	}
	DPRINTFN(25, ("%s: txeof me=%d  status=%d\n", USBDEVNAME(sc->atu_dev),
	    c->atu_idx, status));

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}

		DPRINTF(("%s: usb error on tx: %s\n", USBDEVNAME(sc->atu_dev),
		    usbd_errstr(status)));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(
			    sc->atu_ep[ATU_ENDPT_TX]);
		splx(s);
		return;
	}

	usbd_get_xfer_status(c->atu_xfer, NULL, NULL, NULL, &err);

	if (c->atu_mbuf != NULL) {
		m_freem(c->atu_mbuf);
		c->atu_mbuf = NULL;
		if (ifp->if_snd.ifq_head != NULL)
			atu_start(ifp);
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;
	splx(s);
}

#ifdef ATU_TX_PADDING
u_int8_t
atu_calculate_padding(int size)
{
	size %= 64;

	if (size < 50)
		return 50 - size;
	if (size >=61)
		return 64 + 50 - size;
	return 0;
}
#endif /* ATU_TX_PADDING */

int
atu_encap(struct atu_softc *sc, struct mbuf *m, struct atu_chain *c)
{
	int			total_len;
	struct atu_txpkt	*pkt;
	struct ether_header	*eth_hdr;
#ifdef ATU_TX_PADDING
	u_int8_t		padding;
#endif /* ATU_TX_PADDING */

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving
	 * enough room for the atmel & 802.11 headers
	 */
	total_len = m->m_pkthdr.len;

	m_copydata(m, 0, m->m_pkthdr.len, c->atu_buf +
		sizeof(pkt->AtHeader) + sizeof(struct wi_80211_hdr) -
		2 * ETHER_ADDR_LEN);

	total_len += sizeof(struct wi_80211_hdr) - 2 * ETHER_ADDR_LEN;

	pkt = (struct atu_txpkt *)c->atu_buf;
	pkt->AtHeader.wlength = total_len;
	pkt->AtHeader.tx_rate = 4;			 /* rate = auto */
	pkt->AtHeader.padding = 0;
	memset(pkt->AtHeader.reserved, 0x00, 4);

	pkt->WiHeader.dur_id = 0x0000;			/* ? */
	pkt->WiHeader.frame_ctl = WI_FTYPE_DATA;

	eth_hdr = (struct ether_header *)(pkt->Packet - 2 * ETHER_ADDR_LEN);

	switch(sc->atu_mode) {
	case AD_HOC_MODE:
		/* dest */
		memcpy(pkt->WiHeader.addr1, eth_hdr->ether_dhost,
		    ETHER_ADDR_LEN);
		/* src */
		memcpy(pkt->WiHeader.addr2, eth_hdr->ether_shost,
		    ETHER_ADDR_LEN);
		/* bssid */
		memcpy(pkt->WiHeader.addr3, sc->atu_bssid, ETHER_ADDR_LEN);
		DPRINTFN(25, ("%s: adhoc encap (bssid=%s)\n",
		    USBDEVNAME(sc->atu_dev), ether_sprintf(sc->atu_bssid)));
		break;

	case INFRASTRUCTURE_MODE:
		pkt->WiHeader.frame_ctl|=WI_FCTL_TODS;
		/* bssid */
		memcpy(pkt->WiHeader.addr1, sc->atu_bssid, ETHER_ADDR_LEN);
		/* src */
		memcpy(pkt->WiHeader.addr2, eth_hdr->ether_shost,
		    ETHER_ADDR_LEN);
		/* dst */
		memcpy(pkt->WiHeader.addr3, eth_hdr->ether_dhost,
		    ETHER_ADDR_LEN);

		DPRINTFN(25, ("%s: infra encap (bssid=%s)\n",
		    USBDEVNAME(sc->atu_dev), ether_sprintf(sc->atu_bssid)));
	}
	memset(pkt->WiHeader.addr4, 0x00, ETHER_ADDR_LEN);
	pkt->WiHeader.seq_ctl = 0;

	if (sc->atu_encrypt & ATU_WEP_TX) {
		pkt->WiHeader.frame_ctl |= WI_FCTL_WEP;
		DPRINTFN(25, ("%s: turning WEP on on packet\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	total_len += sizeof(pkt->AtHeader);
#ifdef ATU_TX_PADDING
	padding = atu_calculate_padding(total_len % 64);
	total_len += padding;
	pkt->AtHeader.padding = padding;
#endif /* ATU_TX_PADDING */
	c->atu_length = total_len;
	c->atu_mbuf = m;
	return(0);
}

void
atu_start(struct ifnet *ifp)
{
	struct atu_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;
	struct atu_cdata	*cd;
	struct atu_chain	*entry;
	usbd_status		err;
	int s;

	s = splnet();
	if (ifp->if_flags & IFF_OACTIVE) {
		splx(s);
		return;
	}

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		splx(s);
		return;
	}

	entry = SLIST_FIRST(&sc->atu_cdata.atu_tx_free);
	while (entry) {
		if (entry == NULL) {
			/* all transfers are in use at this moment */
			splx(s);
			return;
		}

		if (sc->atu_mgmt_vars.state != STATE_HAPPY_NETWORKING) {
			/* don't try to send if we're not associated */
			splx(s);
			return;
		}

		cd = &sc->atu_cdata;

		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL) {
			/* no packets on queue */
			splx(s);
			return;
		}

		SLIST_REMOVE_HEAD(&sc->atu_cdata.atu_tx_free, atu_list);

		ifp->if_flags |= IFF_OACTIVE;
		cd->atu_tx_inuse++;

		DPRINTFN(25, ("%s: index:%d (inuse=%d)\n",
		    USBDEVNAME(sc->atu_dev), entry->atu_idx,
		    cd->atu_tx_inuse));

		err = atu_encap(sc, m_head, entry);
		if (err) {
			DPRINTF(("%s: error encapsulating packet!\n",
			    USBDEVNAME(sc->atu_dev)));
			IF_PREPEND(&ifp->if_snd, m_head);
			if (--cd->atu_tx_inuse == 0)
				ifp->if_flags &= ~IFF_OACTIVE;
			splx(s);
			return;
		}
		err = atu_send_packet(sc, entry);
		if (err) {
			DPRINTF(("%s: error sending packet!\n",
			    USBDEVNAME(sc->atu_dev)));
			splx(s);
			return;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			BPF_MTAP(ifp, m_head);
#endif

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
		entry = SLIST_FIRST(&sc->atu_cdata.atu_tx_free);
	}
	splx(s);
}

void
atu_init(void *xsc)
{
	struct atu_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct atu_chain	*c;
	struct atu_cdata	*cd = &sc->atu_cdata;
	usbd_status		err;
	int			i, s;

	s = splnet();

	DPRINTFN(10, ("%s: atu_init\n", USBDEVNAME(sc->atu_dev)));

	if (ifp->if_flags & IFF_RUNNING) {
		splx(s);
		return;
	}

	/* Init TX ring */
	if (atu_xfer_list_init(sc, cd->atu_tx_chain, ATU_TX_LIST_CNT, 0,
	    ATU_TX_BUFSZ, &cd->atu_tx_free)) {
		DPRINTF(("%s: tx list init failed\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	/* Init RX ring */
	if (atu_rx_list_init(sc)) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->atu_dev));
	}

	/* Init mgmt ring */
	if (atu_xfer_list_init(sc, cd->atu_mgmt_chain,
	    ATU_MGMT_LIST_CNT, 0, ATU_MGMT_BUFSZ, &cd->atu_mgmt_free)) {
		DPRINTF(("%s: rx list init failed\n",
		    USBDEVNAME(sc->atu_dev)));
	}

	/* Load the multicast filter. */
	/*atu_setmulti(sc); */

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->atu_iface, sc->atu_ed[ATU_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->atu_ep[ATU_ENDPT_RX]);
	if (err) {
		DPRINTF(("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		splx(s);
		return;
	}

	err = usbd_open_pipe(sc->atu_iface, sc->atu_ed[ATU_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->atu_ep[ATU_ENDPT_TX]);
	if (err) {
		DPRINTF(("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		splx(s);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < ATU_RX_LIST_CNT; i++) {
		c = &sc->atu_cdata.atu_rx_chain[i];

		usbd_setup_xfer(c->atu_xfer, sc->atu_ep[ATU_ENDPT_RX],
		    c, c->atu_buf, ATU_RX_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, atu_rxeof);
		usbd_transfer(c->atu_xfer);
	}

	bcopy((char *)&sc->arpcom.ac_enaddr, sc->atu_mac_addr,
	    ETHER_ADDR_LEN);
	DPRINTFN(10, ("%s: starting up using MAC=%s\n",
	    USBDEVNAME(sc->atu_dev), ether_sprintf(sc->atu_mac_addr)));

	/* Do initial setup */
	err = atu_initial_config(sc);
	if (err) {
		DPRINTF(("%s: initial config failed!\n",
			USBDEVNAME(sc->atu_dev)));
		splx(s);
		return;
	}
	DPRINTFN(10, ("%s: initialised transceiver\n",
	    USBDEVNAME(sc->atu_dev)));

	/* Fire up managment task */
	DPRINTFN(10, ("%s: trying to start mgmt task...\n",
	    USBDEVNAME(sc->atu_dev)));
	if (!(sc->atu_mgmt_flags & ATU_TASK_RUNNING)) {
		sc->atu_dying = 0;
		err = kthread_create(atu_mgmt_loop, sc,
		    &sc->atu_mgmt_thread, USBDEVNAME(sc->atu_dev));
		if (err) {
			DPRINTF(("%s: failed to create kthread\n",
			    USBDEVNAME(sc->atu_dev)));
		}

		sc->atu_mgmt_flags |= ATU_TASK_RUNNING;
	}

	/* sc->atu_rxfilt = ATU_RXFILT_UNICAST|ATU_RXFILT_BROADCAST; */

	/* If we want promiscuous mode, set the allframes bit. */
	/*
	if (ifp->if_flags & IFF_PROMISC)
		sc->atu_rxfilt |= ATU_RXFILT_PROMISC;
	*/

	sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
	wakeup(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);
}

void
atu_print_a_bunch_of_debug_things(struct atu_softc *sc)
{
	usbd_status		err;
	u_int8_t		tmp[32];

	/* DEBUG */
	err = atu_get_mib(sc, MIB_MAC_MGMT__CURRENT_BSSID, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: current BSSID=%s\n", USBDEVNAME(sc->atu_dev),
	    ether_sprintf(tmp)));

	err = atu_get_mib(sc, MIB_MAC_MGMT__BEACON_PERIOD, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: beacon period=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_MAC_WEP__PRIVACY_INVOKED, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: privacy invoked=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_MAC_WEP__ENCR_LEVEL, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: encr_level=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_MAC_WEP__ICV_ERROR_COUNT, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: icv error count=%d\n", USBDEVNAME(sc->atu_dev),
	    *(short *)tmp));

	err = atu_get_mib(sc, MIB_MAC_WEP__EXCLUDED_COUNT, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: wep excluded count=%d\n",
	    USBDEVNAME(sc->atu_dev), *(short *)tmp));

	err = atu_get_mib(sc, MIB_MAC_MGMT__POWER_MODE, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: power mode=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_PHY__CHANNEL, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: channel=%d\n", USBDEVNAME(sc->atu_dev), tmp[0]));

	err = atu_get_mib(sc, MIB_PHY__REG_DOMAIN, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: reg domain=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_LOCAL__SSID_SIZE, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: ssid size=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_LOCAL__BEACON_ENABLE, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: beacon enable=%d\n", USBDEVNAME(sc->atu_dev),
	    tmp[0]));

	err = atu_get_mib(sc, MIB_LOCAL__AUTO_RATE_FALLBACK, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: auto rate fallback=%d\n",
	    USBDEVNAME(sc->atu_dev), tmp[0]));

	err = atu_get_mib(sc, MIB_MAC_ADDR__ADDR, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: mac addr=%s\n", USBDEVNAME(sc->atu_dev),
	    ether_sprintf(tmp)));

	err = atu_get_mib(sc, MIB_MAC__DESIRED_SSID, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: desired ssid=%s\n", USBDEVNAME(sc->atu_dev),
	    tmp));

	err = atu_get_mib(sc, MIB_MAC_MGMT__CURRENT_ESSID, tmp);
	if (err) return;
	DPRINTF(("%s: DEBUG: current ESSID=%s\n", USBDEVNAME(sc->atu_dev),
	    tmp));

}

int
atu_set_wepkey(struct atu_softc *sc, int nr, u_int8_t *key, int len)
{
	if ((len != 5) && (len != 13))
		return EINVAL;

	DPRINTFN(10, ("%s: changed wepkey %d (len=%d)\n",
	    USBDEVNAME(sc->atu_dev), nr, len));

	memcpy(sc->atu_wepkeys[nr], key, len);
	if (len == 13)
		sc->atu_wepkeylen = ATU_WEP_104BITS;
	else
		sc->atu_wepkeylen = ATU_WEP_40BITS;

	atu_send_mib(sc, MIB_MAC_WEP__ENCR_LEVEL, NR(sc->atu_wepkeylen));
	return atu_send_mib(sc, MIB_MAC_WEP__KEYS(nr), key);
}

int
atu_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct atu_softc		*sc = ifp->if_softc;

	struct ifreq			*ifr = (struct ifreq *)data;
	struct ifaddr			*ifa = (struct ifaddr *)data;
	struct ieee80211req		*ireq;
	struct ieee80211_bssid		*bssid;
	struct ieee80211chanreq		*chanreq;
	struct ieee80211_power		*power;
	int				err = 0;
#if 0
	u_int8_t			tmp[32] = "";
	int				len = 0;
#endif
	struct ieee80211_nwid		nwid;
	struct wi_req			wreq;
	int				change, s;

	s = splnet();
	ireq = (struct ieee80211req *)data;
	change = ifp->if_flags ^ sc->atu_if_flags;

	DPRINTFN(15, ("%s: atu_ioctl: command=%lu\n", USBDEVNAME(sc->atu_dev),
	    command));

	switch (command) {
	case SIOCSIFADDR:
		DPRINTFN(15, ("%s: SIOCSIFADDR\n", USBDEVNAME(sc->atu_dev)));

		ifp->if_flags |= IFF_UP;
		atu_init(sc);

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif /* INET */
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ATU_MAX_MTU)
			err = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		DPRINTFN(15, ("%s: SIOCSIFFLAGS\n", USBDEVNAME(sc->atu_dev)));

		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->atu_if_flags & IFF_PROMISC)) {
/* enable promisc */
#if 0
				sc->atu_rxfilt |= ATU_RXFILT_PROMISC;
				atu_setword(sc, ATU_CMD_SET_PKT_FILTER,
				    sc->atu_rxfilt);
#endif
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->atu_if_flags & IFF_PROMISC) {
/* disable promisc */
#if 0
				sc->atu_rxfilt &= ~ATU_RXFILT_PROMISC;
				atu_setword(sc, ATU_CMD_SET_PKT_FILTER,
				    sc->atu_rxfilt);
#endif
			} else if (!(ifp->if_flags & IFF_RUNNING))
				atu_init(sc);

#if 0
			DPRINTFN(15, ("%s: ioctl calling atu_init()\n",
			    USBDEVNAME(sc->atu_dev)));
			atu_init(sc);
			err = atu_switch_radio(sc, 1);
#endif
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				atu_stop(sc);
			err = atu_switch_radio(sc, 0);
		}
		sc->atu_if_flags = ifp->if_flags;

		err = 0;
		break;

	case SIOCADDMULTI:
		DPRINTFN(15, ("%s: SIOCADDMULTI\n", USBDEVNAME(sc->atu_dev)));
		/* TODO: implement */
		err = 0;
		break;

	case SIOCDELMULTI:
		DPRINTFN(15, ("%s: SIOCDELMULTI\n", USBDEVNAME(sc->atu_dev)));
		/* TODO: implement */
		err = 0;
		break;

	case SIOCS80211NWID:
		DPRINTFN(15, ("%s: SIOCS80211NWID\n",
		    USBDEVNAME(sc->atu_dev)));
		err = copyin(ifr->ifr_data, &nwid, sizeof(nwid));
		if (err)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			err = EINVAL;
			break;
		}
		sc->atu_ssidlen = nwid.i_len;
		memcpy(sc->atu_ssid, nwid.i_nwid, nwid.i_len);
		sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
		wakeup(sc);
		break;

	case SIOCG80211NWID:
		DPRINTFN(15, ("%s: SIOGS80211NWID\n",
		    USBDEVNAME(sc->atu_dev)));
		nwid.i_len = sc->atu_ssidlen;
		memcpy(nwid.i_nwid, sc->atu_ssid, nwid.i_len);
		err = copyout(&nwid, ifr->ifr_data, sizeof(nwid));
#ifdef ATU_DEBUG
		if (atudebug > 20)
			atu_print_a_bunch_of_debug_things(sc);
#endif /* ATU_DEBUG */
		break;

	case SIOCG80211BSSID:
		DPRINTF(("%s: ioctl 80211 get BSSID\n",
		    USBDEVNAME(sc->atu_dev)));
		bssid = (struct ieee80211_bssid *)data;
		IEEE80211_ADDR_COPY(bssid->i_bssid, sc->atu_bssid);
		DPRINTF(("%s: returned %s\n", USBDEVNAME(sc->atu_dev),
		    ether_sprintf(sc->atu_bssid)));
		break;
	case SIOCS80211CHANNEL:
		chanreq = (struct ieee80211chanreq *)data;
		DPRINTF(("%s: ioctl 80211 set CHANNEL (%d)\n",
		    USBDEVNAME(sc->atu_dev), chanreq->i_channel));

		if (((chanreq->i_channel < 1) || (chanreq->i_channel > 14)) &&
		    (chanreq->i_channel != IEEE80211_CHAN_ANY)) {
			err = EINVAL;
			break;
		}
		/* restart scan / join / etc now */
		sc->atu_desired_channel = chanreq->i_channel;
		sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
		wakeup(sc);
		break;
	case SIOCG80211CHANNEL:
		DPRINTF(("%s: ioctl 80211 get CHANNEL\n",
		    USBDEVNAME(sc->atu_dev)));
		chanreq = (struct ieee80211chanreq *)data;
		if ((sc->atu_desired_channel == IEEE80211_CHAN_ANY) &&
		    (!(sc->atu_mgmt_flags & ATU_NETWORK_OK)))
			chanreq->i_channel = IEEE80211_CHAN_ANY;
		else
			chanreq->i_channel = sc->atu_channel;
		break;
	case SIOCG80211POWER:
		DPRINTF(("%s: ioctl 80211 get POWER\n",
		    USBDEVNAME(sc->atu_dev)));
		power = (struct ieee80211_power *)data;
		/* Dummmy, we don't do power saving at the moment */
		power->i_enabled = 0;
		power->i_maxsleep = 0;
		break;

#if 0
	case SIOCG80211:
		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			err = copyout(sc->atu_ssid, ireq->i_data,
			    sc->atu_ssidlen);
			ireq->i_len = sc->atu_ssidlen;
			break;

		case IEEE80211_IOC_NUMSSIDS:
			ireq->i_val = 1;
			break;

		case IEEE80211_IOC_CHANNEL:
			ireq->i_val = sc->atu_channel;

			/*
			 * every time the channel is requested, we errr...
			 * print a bunch of debug things :)
			 */
#ifdef ATU_DEBUG
			if (atudebug > 20)
				atu_print_a_bunch_of_debug_things(sc);
#endif /* ATU_DEBUG */
			break;

		case IEEE80211_IOC_AUTHMODE:
			/* TODO: change this when shared-key is implemented */
			ireq->i_val = IEEE80211_AUTH_OPEN;
			break;

		case IEEE80211_IOC_WEP:
			switch (sc->atu_encrypt) {
			case ATU_WEP_TX:
				ireq->i_val = IEEE80211_WEP_MIXED;
				break;
			case ATU_WEP_TXRX:
				ireq->i_val = IEEE80211_WEP_ON;
				break;
			default:
				ireq->i_val = IEEE80211_WEP_OFF;
			}
			break;

		case IEEE80211_IOC_NUMWEPKEYS:
			ireq->i_val = 4;
			break;

		case IEEE80211_IOC_WEPKEY:
			err = suser(curproc, 0);
			if (err)
				break;

			if((ireq->i_val < 0) || (ireq->i_val > 3)) {
				err = EINVAL;
				break;
			}

			if (sc->atu_encrypt == ATU_WEP_40BITS)
				len = 5;
			else
				len = 13;

			err = copyout(sc->atu_wepkeys[ireq->i_val],
			    ireq->i_data, len);
			break;

		case IEEE80211_IOC_WEPTXKEY:
			ireq->i_val = sc->atu_wepkey;
			break;

		default:
			DPRINTF(("%s: ioctl:  unknown 80211: %04x %d\n",
			    USBDEVNAME(sc->atu_dev), ireq->i_type,
			    ireq->i_type));
			err = EINVAL;
		}
		break;

	case SIOCS80211:
		err = suser(curproc, 0);
		if (err)
			break;

		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_len < 0 || ireq->i_len > 32) {
				err = EINVAL;
				break;
			}

			err = copyin(ireq->i_data, tmp, ireq->i_len);
			if (err)
				break;

			sc->atu_ssidlen = ireq->i_len;
			memcpy(sc->atu_ssid, tmp, ireq->i_len);

			sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
			wakeup(sc);
			break;

		case IEEE80211_IOC_CHANNEL:
			if (ireq->i_val < 1 || ireq->i_val > 14) {
				err = EINVAL;
				break;
			}

			sc->atu_channel = ireq->i_val;

			/* restart scan / join / etc now */
			sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
			wakeup(sc);
			break;

		case IEEE80211_IOC_WEP:
			switch (ireq->i_val) {
			case IEEE80211_WEP_OFF:
				sc->atu_encrypt = ATU_WEP_OFF;
				break;
			case IEEE80211_WEP_MIXED:
				sc->atu_encrypt = ATU_WEP_TX;
				break;
			case IEEE80211_WEP_ON:
				sc->atu_encrypt = ATU_WEP_TXRX;
				break;
			default:
				err = EINVAL;
			}
			if (err)
				break;

			/*
			 * to change the wep-bit in our beacon we HAVE to send
			 * CMD_STARTUP again
			 */
			err = atu_initial_config(sc);
			/*
			 * after that we have to send CMD_JOIN again to get
			 * the receiver running again. so we'll just
			 * restart the entire join/assoc/auth state-machine.
			 */
			sc->atu_mgmt_flags |= ATU_CHANGED_SETTINGS;
			wakeup(sc);
			break;

		case IEEE80211_IOC_WEPKEY:
			if ((ireq->i_val < 0) || (ireq->i_val > 3) ||
			    (ireq->i_len > 13)) {
				err = EINVAL;
				break;
			}
			err = copyin(ireq->i_data, tmp, ireq->i_len);
			if (err)
				break;
			err = atu_set_wepkey(sc, ireq->i_val, tmp,
			    ireq->i_len);
			break;

		case IEEE80211_IOC_WEPTXKEY:
			if ((ireq->i_val < 0) || (ireq->i_val > 3)) {
				err = EINVAL;
				break;
			}
			sc->atu_wepkey = ireq->i_val;
			err = atu_send_mib(sc, MIB_MAC_WEP__KEY_ID,
			    NR(sc->atu_wepkey));

			break;

		case IEEE80211_IOC_AUTHMODE:
			/* TODO: change when shared-key is implemented */
			if (ireq->i_val != IEEE80211_AUTH_OPEN)
				err = EINVAL;
			break;

		default:
			err = EINVAL;
		}
		break;
#endif

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, ifr, &sc->atu_media, command);
		break;

	case SIOCGWAVELAN:
		DPRINTFN(15, ("%s: ioctl: get wavelan\n",
		    USBDEVNAME(sc->atu_dev)));
		/*
		err = ether_ioctl(ifp, &sc->arpcom, command, data);
		break;
		*/

		/* TODO: implement */

		err = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (err)
			break;

		DPRINTFN(15, ("%s: SIOCGWAVELAN\n", USBDEVNAME(sc->atu_dev)));
		if (wreq.wi_len > WI_MAX_DATALEN) {
			err = EINVAL;
			break;
		}

		DPRINTFN(15, ("%s: ioctl: wi_type=%04x %d\n",
		    USBDEVNAME(sc->atu_dev), wreq.wi_type, wreq.wi_type));
		err = 0;
		/* err = EINVAL; */
		break;

	case SIOCSWAVELAN:
		DPRINTFN(15, ("%s: ioctl: wavset type=%x\n",
		    USBDEVNAME(sc->atu_dev), 0));
		err = 0;
		break;

	default:
		DPRINTFN(15, ("%s: ioctl: default\n",
		    USBDEVNAME(sc->atu_dev)));
		err = ether_ioctl(ifp, &sc->arpcom, command, data);
		break;
	}

	sc->atu_if_flags = ifp->if_flags;
	splx(s);
	return(err);
}

void
atu_watchdog(struct ifnet *ifp)
{
	struct atu_softc	*sc;
	struct atu_chain	*c;
	usbd_status		stat;
	int			cnt, s;

	sc = ifp->if_softc;
	s = splnet();
	ifp->if_oerrors++;
	DPRINTF(("%s: watchdog timeout\n", USBDEVNAME(sc->atu_dev)));

	/*
	 * TODO:
	 * we should change this since we have multiple TX tranfers...
	 */
	for (cnt = 0; cnt < ATU_TX_LIST_CNT; cnt++) {
		c = &sc->atu_cdata.atu_tx_chain[cnt];
		if (c->atu_in_xfer) {
			usbd_get_xfer_status(c->atu_xfer, NULL, NULL, NULL,
			    &stat);
			atu_txeof(c->atu_xfer, c, stat);
		}
	}
	for (cnt = 0; cnt < ATU_MGMT_LIST_CNT; cnt++) {
		c = &sc->atu_cdata.atu_mgmt_chain[cnt];
		if (c->atu_in_xfer) {
			usbd_get_xfer_status(c->atu_xfer, NULL, NULL, NULL,
			    &stat);
			atu_txeof(c->atu_xfer, c, stat);
		}
	}

	if (ifp->if_snd.ifq_head != NULL)
		atu_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
atu_stop(struct atu_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	struct atu_cdata	*cd;
	int s;

	s = splnet();
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	/* there must be a better way to clean up the mgmt task... */
	sc->atu_dying = 1;
	splx(s);

	if (sc->atu_mgmt_flags & ATU_TASK_RUNNING) {
		DPRINTFN(10, ("%s: waiting for mgmt task to die\n",
		    USBDEVNAME(sc->atu_dev)));
		wakeup(sc);
		while (sc->atu_dying == 1) {
			atu_msleep(sc, 100);
		}
	}
	s = splnet();
	DPRINTFN(10, ("%s: stopped managment thread\n",
	    USBDEVNAME(sc->atu_dev)));

	/* Stop transfers. */
	if (sc->atu_ep[ATU_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->atu_ep[ATU_ENDPT_RX]);
		if (err) {
			DPRINTF(("%s: abort rx pipe failed: %s\n",
			    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		}
		err = usbd_close_pipe(sc->atu_ep[ATU_ENDPT_RX]);
		if (err) {
			DPRINTF(("%s: close rx pipe failed: %s\n",
			    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		}
		sc->atu_ep[ATU_ENDPT_RX] = NULL;
	}

	if (sc->atu_ep[ATU_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->atu_ep[ATU_ENDPT_TX]);
		if (err) {
			DPRINTF(("%s: abort tx pipe failed: %s\n",
			    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		}
		err = usbd_close_pipe(sc->atu_ep[ATU_ENDPT_TX]);
		if (err) {
			DPRINTF(("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->atu_dev), usbd_errstr(err)));
		}
		sc->atu_ep[ATU_ENDPT_TX] = NULL;
	}

	/* Free RX/TX/MGMT list resources. */
	cd = &sc->atu_cdata;
	atu_xfer_list_free(sc, cd->atu_rx_chain, ATU_RX_LIST_CNT);
	atu_xfer_list_free(sc, cd->atu_tx_chain, ATU_TX_LIST_CNT);
	atu_xfer_list_free(sc, cd->atu_mgmt_chain, ATU_MGMT_LIST_CNT);

	/* Let's be nice and turn off the radio before we leave */
	atu_switch_radio(sc, 0);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	splx(s);
}
