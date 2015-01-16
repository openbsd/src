/*	$OpenBSD: print-802_11.c,v 1.16 2015/01/16 06:40:21 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_radiotap.h>

#include <pcap.h>
#include <stdio.h>
#include <string.h>

#include "addrtoname.h"
#include "interface.h"

const char *ieee80211_mgt_subtype_name[] = {
	"association request",
	"association response",
	"reassociation request",
	"reassociation response",
	"probe request",
	"probe response",
	"reserved#6",
	"reserved#7",
	"beacon",
	"atim",
	"disassociation",
	"authentication",
	"deauthentication",
	"action",
	"action noack",
	"reserved#15"
};

const char *ieee80211_data_subtype_name[] = {
	"data",
	"data cf ack",
	"data cf poll",
	"data cf poll ack",
	"no-data",
	"no-data cf poll",
	"no-data cf ack",
	"no-data cf poll ack",
	"QoS data",
	"QoS data cf ack",
	"QoS data cf poll",
	"QoS data cf poll ack",
	"QoS no-data",
	"QoS no-data cf poll",
	"QoS no-data cf ack",
	"QoS no-data cf poll ack"
};

int	 ieee80211_hdr(struct ieee80211_frame *);
int	 ieee80211_data(struct ieee80211_frame *, u_int);
void	 ieee80211_print_element(u_int8_t *, u_int);
void	 ieee80211_print_essid(u_int8_t *, u_int);
int	 ieee80211_elements(struct ieee80211_frame *, u_int);
int	 ieee80211_frame(struct ieee80211_frame *, u_int);
int	 ieee80211_print(struct ieee80211_frame *, u_int);
u_int	 ieee80211_any2ieee(u_int, u_int);
void	 ieee80211_reason(u_int16_t);

#define TCARR(a)	TCHECK2(*a, sizeof(a))

int ieee80211_encap = 0;

int
ieee80211_hdr(struct ieee80211_frame *wh)
{
	struct ieee80211_frame_addr4 *w4;

	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		TCARR(wh->i_addr2);
		printf("%s", etheraddr_string(wh->i_addr2));
		TCARR(wh->i_addr1);
		printf(" > %s", etheraddr_string(wh->i_addr1));
		TCARR(wh->i_addr3);
		printf(", bssid %s", etheraddr_string(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		TCARR(wh->i_addr2);
		printf("%s", etheraddr_string(wh->i_addr2));
		TCARR(wh->i_addr3);
		printf(" > %s", etheraddr_string(wh->i_addr3));
		TCARR(wh->i_addr1);
		printf(", bssid %s, > DS", etheraddr_string(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		TCARR(wh->i_addr3);
		printf("%s", etheraddr_string(wh->i_addr3));
		TCARR(wh->i_addr1);
		printf(" > %s", etheraddr_string(wh->i_addr1));
		TCARR(wh->i_addr2);
		printf(", bssid %s, DS >", etheraddr_string(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		w4 = (struct ieee80211_frame_addr4 *) wh;
		TCARR(w4->i_addr4);
		printf("%s", etheraddr_string(w4->i_addr4));
		TCARR(w4->i_addr3);
		printf(" > %s", etheraddr_string(w4->i_addr3));
		TCARR(w4->i_addr2);
		printf(", bssid %s", etheraddr_string(w4->i_addr2));
		TCARR(w4->i_addr1);
		printf(" > %s, DS > DS", etheraddr_string(w4->i_addr1));
		break;
	}
	if (vflag) {
		u_int16_t seq;
		TCARR(wh->i_seq);
		bcopy(wh->i_seq, &seq, sizeof(u_int16_t));
		printf(" (seq %u): ", letoh16(seq));
	} else
		printf(": ");

	return (0);

 trunc:
	/* Truncated elements in frame */
	return (1);
}

int
ieee80211_data(struct ieee80211_frame *wh, u_int len)
{
	u_int8_t *t = (u_int8_t *)wh;
	struct ieee80211_frame_addr4 *w4;
	u_int datalen;
	int data = !(wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_NODATA);
	u_char *esrc = NULL, *edst = NULL;

	TCHECK(*wh);
	t += sizeof(struct ieee80211_frame);
	datalen = len - sizeof(struct ieee80211_frame);

	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_TODS:
		esrc = wh->i_addr2;
		edst = wh->i_addr3;
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		esrc = wh->i_addr3;
		edst = wh->i_addr1;
		break;
	case IEEE80211_FC1_DIR_NODS:
		esrc = wh->i_addr2;
		edst = wh->i_addr1;
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		w4 = (struct ieee80211_frame_addr4 *) wh;
		TCHECK(*w4);
		t = (u_int8_t *) (w4 + 1);
		datalen = len - sizeof(*w4);
		esrc = w4->i_addr4;
		edst = w4->i_addr3;
		break;
	}

	if (data && esrc)
		llc_print(t, datalen, datalen, esrc, edst);
	else if (eflag && esrc)
		printf("%s > %s",
		    etheraddr_string(esrc), etheraddr_string(edst));

	return (0);

 trunc:
	/* Truncated elements in frame */
	return (1);
}

/* Caller checks len */
void
ieee80211_print_element(u_int8_t *data, u_int len)
{
	u_int8_t *p;
	int i;

	printf(" 0x");
	for (i = 0, p = data; i < len; i++, p++)
		printf("%02x", *p);
}

/* Caller checks len */
void
ieee80211_print_essid(u_int8_t *essid, u_int len)
{
	u_int8_t *p;
	int i;

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;

	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf(" (");
		for (i = 0, p = essid; i < len; i++, p++)
			putchar(*p);
		putchar(')');
	} else
		ieee80211_print_element(essid, len);
}

int
ieee80211_elements(struct ieee80211_frame *wh, u_int flen)
{
	u_int8_t *buf, *frm;
	u_int64_t tstamp;
	u_int16_t bintval, capinfo;
	int i;

	buf = (u_int8_t *)wh;
	frm = (u_int8_t *)&wh[1];

	TCHECK2(*frm, 8);
	bcopy(frm, &tstamp, sizeof(u_int64_t));
	frm += 8;

	if (vflag > 1)
		printf(", timestamp %llu", letoh64(tstamp));

	TCHECK2(*frm, 2);
	bcopy(frm, &bintval, sizeof(u_int16_t));
	frm += 2;

	if (vflag > 1)
		printf(", interval %u", letoh16(bintval));

	TCHECK2(*frm, 2);
	bcopy(frm, &capinfo, sizeof(u_int16_t));
	frm += 2;

	if (vflag)
		printb(", caps", letoh16(capinfo),
		    IEEE80211_CAPINFO_BITS);

	while (TTEST2(*frm, 2)) {
		u_int len = frm[1];
		u_int8_t *data = frm + 2;

		if (!TTEST2(*data, len))
			break;

#define ELEM_CHECK(l)	if (len != l) break

		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			printf(", ssid");
			ieee80211_print_essid(data, len);
			break;
		case IEEE80211_ELEMID_RATES:
			printf(", rates");
			if (!vflag)
				break;
			for (i = len; i > 0; i--, data++)
				printf(" %uM",
				    (data[0] & IEEE80211_RATE_VAL) / 2);
			break;
		case IEEE80211_ELEMID_FHPARMS:
			ELEM_CHECK(5);
			printf(", fh (dwell %u, chan %u, index %u)",
			    (data[1] << 8) | data[0],
			    (data[2] - 1) * 80 + data[3],	/* FH_CHAN */
			    data[4]);
			break;
		case IEEE80211_ELEMID_DSPARMS:
			ELEM_CHECK(1);
			printf(", ds");
			if (vflag)
				printf(" (chan %u)", data[0]);
			break;
		case IEEE80211_ELEMID_CFPARMS:
			printf(", cf");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_TIM:
			printf(", tim");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_IBSSPARMS:
			printf(", ibss");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_COUNTRY:
			printf(", country");
			for (i = len; i > 0; i--, data++)
				printf(" %u", data[0]);
			break;
		case IEEE80211_ELEMID_CHALLENGE:
			printf(", challenge");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_ERP:
			printf(", erp");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_RSN:
			printf(", rsn");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_XRATES:
			printf(", xrates");
			if (!vflag)
				break;
			for (i = len; i > 0; i--, data++)
				printf(" %uM",
				    (data[0] & IEEE80211_RATE_VAL) / 2);
			break;
		case IEEE80211_ELEMID_TPC:
			printf(", tpc");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_CCKM:
			printf(", cckm");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		case IEEE80211_ELEMID_VENDOR:
			printf(", vendor");
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		default:
			printf(", %u:%u", (u_int) *frm, len);
			if (vflag)
				ieee80211_print_element(data, len);
			break;
		}
		frm += len + 2;

		if (frm >= snapend)
			break;
	}

#undef ELEM_CHECK

	return (0);

 trunc:
	/* Truncated elements in frame */
	return (1);
}

int
ieee80211_frame(struct ieee80211_frame *wh, u_int len)
{
	u_int8_t subtype, type, *frm;

	TCARR(wh->i_fc);

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	frm = (u_int8_t *)&wh[1];

	if (vflag)
		printb(" flags", wh->i_fc[1], IEEE80211_FC1_BITS);

	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(": %s: ", ieee80211_data_subtype_name[
		    subtype >> IEEE80211_FC0_SUBTYPE_SHIFT]);
		ieee80211_data(wh, len);
		break;
	case IEEE80211_FC0_TYPE_MGT:
		printf(": %s", ieee80211_mgt_subtype_name[
		    subtype >> IEEE80211_FC0_SUBTYPE_SHIFT]);
		switch (subtype) {
		case IEEE80211_FC0_SUBTYPE_BEACON:
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			if (ieee80211_elements(wh, len) != 0)
				goto trunc;
			break;
		case IEEE80211_FC0_SUBTYPE_AUTH:
			TCHECK2(*frm, 2);		/* Auth Algorithm */
			switch (IEEE80211_AUTH_ALGORITHM(frm)) {
			case IEEE80211_AUTH_ALG_OPEN:
				TCHECK2(*frm, 4);	/* Auth Transaction */
				switch (IEEE80211_AUTH_TRANSACTION(frm)) {
				case IEEE80211_AUTH_OPEN_REQUEST:
					printf(" request");
					break;
				case IEEE80211_AUTH_OPEN_RESPONSE:
					printf(" response");
					break;
				}
				break;
			case IEEE80211_AUTH_ALG_SHARED:
				TCHECK2(*frm, 4);	/* Auth Transaction */
				switch (IEEE80211_AUTH_TRANSACTION(frm)) {
				case IEEE80211_AUTH_SHARED_REQUEST:
					printf(" request");
					break;
				case IEEE80211_AUTH_SHARED_CHALLENGE:
					printf(" challenge");
					break;
				case IEEE80211_AUTH_SHARED_RESPONSE:
					printf(" response");
					break;
				case IEEE80211_AUTH_SHARED_PASS:
					printf(" pass");
					break;
				}
				break;
			case IEEE80211_AUTH_ALG_LEAP:
				printf(" (leap)");
				break;
			}
			break;
		case IEEE80211_FC0_SUBTYPE_DEAUTH:
		case IEEE80211_FC0_SUBTYPE_DISASSOC:
			TCHECK2(*frm, 2);		/* Reason Code */
			ieee80211_reason(frm[0] | (frm[1] << 8));
			break;
		}
		break;
	default:
		printf(": type#%d", type);
		break;
	}

	return (0);

 trunc:
	/* Truncated 802.11 frame */
	return (1);
}

u_int
ieee80211_any2ieee(u_int freq, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return (freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {
		return (freq - 5000) / 5;
	} else {
		/* Assume channel is already an IEEE number */
		return (freq);
	}
}

int
ieee80211_print(struct ieee80211_frame *wh, u_int len)
{
	if (eflag)
		if (ieee80211_hdr(wh))
			return (1);

	printf("802.11");

	return (ieee80211_frame(wh, len));
}

void
ieee802_11_if_print(u_char *user, const struct pcap_pkthdr *h,
    const u_char *p)
{
	struct ieee80211_frame *wh = (struct ieee80211_frame*)p;

	if (!ieee80211_encap)
		ts_print(&h->ts);

	packetp = p;
	snapend = p + h->caplen;

	if (ieee80211_print(wh, (u_int)h->len) != 0)
		printf("[|802.11]");

	if (!ieee80211_encap) {
		if (xflag)
			default_print(p, (u_int)h->len);
		putchar('\n');
	}
}

void
ieee802_11_radio_if_print(u_char *user, const struct pcap_pkthdr *h,
    const u_char *p)
{
	struct ieee80211_radiotap_header *rh =
	    (struct ieee80211_radiotap_header*)p;
	struct ieee80211_frame *wh;
	u_int8_t *t;
	u_int32_t present;
	u_int len, rh_len;
	u_int16_t tmp;

	if (!ieee80211_encap)
		ts_print(&h->ts);

	packetp = p;
	snapend = p + h->caplen;

	TCHECK(*rh);

	len = h->len;
	rh_len = letoh16(rh->it_len);
	if (rh->it_version != 0) {
		printf("[?radiotap + 802.11 v:%u]", rh->it_version);
		goto out;
	}

	wh = (struct ieee80211_frame *)(p + rh_len);
	if (len <= rh_len || ieee80211_print(wh, len - rh_len))
		printf("[|802.11]");

	t = (u_int8_t*)p + sizeof(struct ieee80211_radiotap_header);

	if ((present = letoh32(rh->it_present)) == 0)
		goto out;

	printf(", <radiotap v%u", rh->it_version);

#define RADIOTAP(_x)	\
	(present & (1 << IEEE80211_RADIOTAP_##_x))

	if (RADIOTAP(TSFT)) {
		u_int64_t tsf;

		TCHECK2(*t, 8);
		bcopy(t, &tsf, sizeof(u_int64_t));
		if (vflag > 1)
			printf(", tsf %llu", letoh64(tsf));
		t += 8;
	}

	if (RADIOTAP(FLAGS)) {
		u_int8_t flags = *(u_int8_t*)t;
		TCHECK2(*t, 1);

		if (flags & IEEE80211_RADIOTAP_F_CFP)
			printf(", CFP");
		if (flags & IEEE80211_RADIOTAP_F_SHORTPRE)
			printf(", SHORTPRE");
		if (flags & IEEE80211_RADIOTAP_F_WEP)
			printf(", WEP");
		if (flags & IEEE80211_RADIOTAP_F_FRAG)
			printf(", FRAG");
		t += 1;
	}

	if (RADIOTAP(RATE)) {
		TCHECK2(*t, 1);
		if (vflag)
			printf(", %uMbit/s", (*(u_int8_t*)t) / 2);
		t += 1;
	}

	if (RADIOTAP(CHANNEL)) {
		u_int16_t freq, flags;
		TCHECK2(*t, 2);

		bcopy(t, &freq, sizeof(u_int16_t));
		freq = letoh16(freq);
		t += 2;
		TCHECK2(*t, 2);
		bcopy(t, &flags, sizeof(u_int16_t));
		flags = letoh16(flags);
		t += 2;

		printf(", chan %u", ieee80211_any2ieee(freq, flags));

		if (flags & IEEE80211_CHAN_DYN &&
		    flags & IEEE80211_CHAN_2GHZ)
			printf(", 11g");
		else if (flags & IEEE80211_CHAN_CCK &&
		    flags & IEEE80211_CHAN_2GHZ)
			printf(", 11b");
		else if (flags & IEEE80211_CHAN_OFDM &&
		    flags & IEEE80211_CHAN_2GHZ)
			printf(", 11G");
		else if (flags & IEEE80211_CHAN_OFDM &&
		    flags & IEEE80211_CHAN_5GHZ)
			printf(", 11a");

		if (flags & IEEE80211_CHAN_TURBO)
			printf(", TURBO");
		if (flags & IEEE80211_CHAN_XR)
			printf(", XR");
	}

	if (RADIOTAP(FHSS)) {
		TCHECK2(*t, 2);
		printf(", fhss %u/%u", *(u_int8_t*)t, *(u_int8_t*)t + 1);
		t += 2;
	}

	if (RADIOTAP(DBM_ANTSIGNAL)) {
		TCHECK(*t);
		printf(", sig %ddBm", *(int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(DBM_ANTNOISE)) {
		TCHECK(*t);
		printf(", noise %ddBm", *(int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(LOCK_QUALITY)) {
		TCHECK2(*t, 2);
		if (vflag) {
			bcopy(t, &tmp, sizeof(u_int16_t));
			printf(", quality %u", letoh16(tmp));
		}
		t += 2;
	}

	if (RADIOTAP(TX_ATTENUATION)) {
		TCHECK2(*t, 2);
		if (vflag) {
			bcopy(t, &tmp, sizeof(u_int16_t));
			printf(", txatt %u", letoh16(tmp));
		}
		t += 2;
	}

	if (RADIOTAP(DB_TX_ATTENUATION)) {
		TCHECK2(*t, 2);
		if (vflag) {
			bcopy(t, &tmp, sizeof(u_int16_t));
			printf(", txatt %udB", letoh16(tmp));
		}
		t += 2;
	}

	if (RADIOTAP(DBM_TX_POWER)) {
		TCHECK(*t);
		printf(", txpower %ddBm", *(int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(ANTENNA)) {
		TCHECK(*t);
		if (vflag)
			printf(", antenna %u", *(u_int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(DB_ANTSIGNAL)) {
		TCHECK(*t);
		printf(", signal %udB", *(u_int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(DB_ANTNOISE)) {
		TCHECK(*t);
		printf(", noise %udB", *(u_int8_t*)t);
		t += 1;
	}

	if (RADIOTAP(FCS)) {
		TCHECK2(*t, 4);
		if (vflag) {
			u_int32_t fcs;
			bcopy(t, &fcs, sizeof(u_int32_t));
			printf(", fcs %08x", letoh32(fcs));
		}
		t += 4;
	}

	if (RADIOTAP(RSSI)) {
		u_int8_t rssi, max_rssi;
		TCHECK(*t);
		rssi = *(u_int8_t*)t;
		t += 1;
		TCHECK(*t);
		max_rssi = *(u_int8_t*)t;
		t += 1;

		printf(", rssi %u/%u", rssi, max_rssi);
	}

#undef RADIOTAP

	putchar('>');
	goto out;

 trunc:
	/* Truncated frame */
	printf("[|radiotap + 802.11]");

 out:
	if (!ieee80211_encap) {
		if (xflag)
			default_print(p, h->len);
		putchar('\n');
	}
}

void
ieee80211_reason(u_int16_t reason)
{
	if (!vflag)
		return;

	switch (reason) {
	case IEEE80211_REASON_UNSPECIFIED:
		printf(", unspecified failure");
		break;
	case IEEE80211_REASON_AUTH_EXPIRE:
		printf(", authentication expired");
		break;
	case IEEE80211_REASON_AUTH_LEAVE:
		printf(", deauth - station left");
		break;
	case IEEE80211_REASON_ASSOC_EXPIRE:
		printf(", association expired");
		break;
	case IEEE80211_REASON_ASSOC_TOOMANY:
		printf(", too many associated stations");
		break;
	case IEEE80211_REASON_NOT_AUTHED:
		printf(", not authenticated");
		break;
	case IEEE80211_REASON_NOT_ASSOCED:
		printf(", not associated");
		break;
	case IEEE80211_REASON_ASSOC_LEAVE:
		printf(", disassociated - station left");
		break;
	case IEEE80211_REASON_ASSOC_NOT_AUTHED:
		printf(", association but not authenticated");
		break;
	case IEEE80211_REASON_RSN_REQUIRED:
		printf(", rsn required");
		break;
	case IEEE80211_REASON_RSN_INCONSISTENT:
		printf(", rsn inconsistent");
		break;
	case IEEE80211_REASON_IE_INVALID:
		printf(", ie invalid");
		break;
	case IEEE80211_REASON_MIC_FAILURE:
		printf(", mic failure");
		break;
	default:
		printf(", unknown reason %u", reason);
	}
}
