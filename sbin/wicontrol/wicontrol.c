/*	$OpenBSD: wicontrol.c,v 1.43 2002/10/27 16:20:48 millert Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *
 *	$FreeBSD: wicontrol.c,v 1.6 1999/05/22 16:12:49 wpaul Exp $
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#include <net/ethernet.h>

#include <machine/if_wavelan_ieee.h>
#else
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_ieee80211.h>

#include <dev/ic/if_wi_ieee.h>
#include <dev/ic/if_wireg.h>
#include <dev/ic/if_wi_hostap.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#if !defined(lint)
static const char copyright[] = "@(#) Copyright (c) 1997, 1998, 1999\
	Bill Paul. All rights reserved.";
static const char rcsid[] =
	"@(#) $OpenBSD: wicontrol.c,v 1.43 2002/10/27 16:20:48 millert Exp $";
#endif

void wi_getval(char *, struct wi_req *);
void wi_setval(char *, struct wi_req *);
void wi_printstr(struct wi_req *);
void wi_setstr(char *, int, char *);
void wi_setbytes(char *, int, char *, int);
void wi_setword(char *, int, char *);
void wi_sethex(char *, int, char *);
void wi_printwords(struct wi_req *);
void wi_printbool(struct wi_req *);
void wi_printhex(struct wi_req *);
void wi_printalgorithm(struct wi_req *wreq);
void wi_printaplist(char *);
void wi_dumpinfo(char *);
void wi_setkeys(char *, int, char *);
void wi_printkeys(struct wi_req *);
void wi_printcardid(struct wi_req *, u_int16_t);
void wi_dumpstats(char *);
void wi_dumpstations(char *);
void printb(char *, unsigned short, char *);
__dead void usage(void);
char *portid(char *);
int  get_if_flags(int, const char *);
int  set_if_flags(int, const char *, int);

const struct wi_card_ident wi_card_ident[] = {
	WI_CARD_IDS
};

void
wi_getval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCGWAVELAN, &ifr) == -1)
		err(1, "SIOCGWAVELAN");

	close(s);
}

void
wi_setval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCSWAVELAN, &ifr) == -1)
		err(1, "SIOCSWAVELAN");

	close(s);
}

void
wi_printstr(wreq)
	struct wi_req		*wreq;
{
	char			*ptr;
	int			i, max;

	if (wreq->wi_type == WI_RID_SERIALNO) {
		ptr = (char *)&wreq->wi_val;
		max = MIN(sizeof(wreq->wi_val) - 1, (wreq->wi_len - 1) * 2);
		for (i = 0; i < max; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	} else {
		int len = letoh16(wreq->wi_val[0]);

		ptr = (char *)&wreq->wi_val[1];
		max = MIN(sizeof(wreq->wi_val) - 1, len);
		for (i = 0; i < max; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	}

	ptr[i] = '\0';
	printf("[ %s ]", ptr);
}

void
wi_setstr(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
{
	struct wi_req		wreq;

	if (str == NULL)
		errx(1, "must specify string");

	bzero((char *)&wreq, sizeof(wreq));

	if (strlen(str) > IEEE80211_NWID_LEN)
		errx(1, "string too long");

	wreq.wi_type = code;
	wreq.wi_len = 18;
	wreq.wi_val[0] = htole16(strlen(str));
	bcopy(str, (char *)&wreq.wi_val[1], strlen(str));

	wi_setval(iface, &wreq);
}

void
wi_setbytes(iface, code, bytes, len)
	char			*iface;
	int			code;
	char			*bytes;
	int			len;
{
	struct wi_req		wreq;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = (len / 2) + 1;
	bcopy(bytes, (char *)&wreq.wi_val[0], len);

	wi_setval(iface, &wreq);
}

void
wi_setword(iface, code, word)
	char			*iface;
	int			code;
	char			*word;
{
	struct wi_req		wreq;
	int			value = strtol(word, NULL, 10);

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = 2;
	wreq.wi_val[0] = htole16(value);

	wi_setval(iface, &wreq);
}

void
wi_sethex(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
{
	struct ether_addr	*addr;

	if (str == NULL)
		errx(1, "must specify address");

	addr = ether_aton(str);

	if (addr == NULL)
		errx(1, "badly formatted address");

	wi_setbytes(iface, code, (char *)addr, ETHER_ADDR_LEN);
}

int
wi_hex2int(c)
	char			c;
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);

	return (0);
}

void
wi_str2key(s, k)
	char			*s;
	struct wi_key		*k;
{
	int			n, i;
	char			*p;

	/* Is this a hex string? */
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		/* Yes, convert to int. */
		n = 0;
		p = (char *)&k->wi_keydat[0];
		for (i = 2; i < strlen(s); i+= 2) {
			*p++ = (wi_hex2int(s[i]) << 4) + wi_hex2int(s[i + 1]);
			n++;
		}
		k->wi_keylen = htole16(n);
	} else {
		/* No, just copy it in. */
		bcopy(s, k->wi_keydat, strlen(s));
		k->wi_keylen = htole16(strlen(s));
	}
}

void
wi_setkeys(iface, idx, key)
	char			*iface;
	int			 idx;
	char			*key;
{
	struct wi_req		wreq;
	struct wi_ltv_keys	*keys;
	struct wi_key		*k;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	wi_getval(iface, &wreq);
	if (letoh16(wreq.wi_val[0]) == 0)
		err(1, "no WEP option available on this card");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;

	wi_getval(iface, &wreq);
	keys = (struct wi_ltv_keys *)&wreq;

	if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
		if (strlen(key) > 28)
			err(1, "encryption key must be no "
			    "more than 26 hex digits long");
	} else {
		if (strlen(key) > 13)
			err(1, "encryption key must be no "
			    "more than 13 characters long");
	}

	if (idx > 3)
		err(1, "only 4 encryption keys available");

	k = &keys->wi_keys[idx];
	wi_str2key(key, k);

	wreq.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
	wi_setval(iface, &wreq);
}

void
wi_printkeys(wreq)
	struct wi_req		*wreq;
{
	int			 i, j, bn;
	struct wi_key		*k;
	struct wi_ltv_keys	*keys;
	char			*ptr;

	keys = (struct wi_ltv_keys *)wreq;

	for (i = 0, bn = 0; i < 4; i++, bn = 0) {
		k = &keys->wi_keys[i];
		ptr = (char *)k->wi_keydat;
		for (j = 0; j < letoh16(k->wi_keylen); j++) {
			/* Only print 7-bit ASCII keys */
			if (ptr[j] & 0x80 || !isprint((unsigned char)ptr[j])) {
				bn = 1;
				break;
			}
		}

		if (bn)	{
			printf("[ 0x");
			for (j = 0; j < letoh16(k->wi_keylen); j++)
				printf("%02x", ((unsigned char *) ptr)[j]);
			printf(" ]");
		} else {
			ptr[j] = '\0';
			printf("[ %s ]", ptr);
		}
	}
}

void
wi_printcardid(wreq, chip_id)
	struct wi_req *wreq;
	u_int16_t chip_id;
{
	const char *chip_name;
	const struct wi_card_ident *id;

	if (wreq->wi_len < 4)
		return;

	for (id = wi_card_ident; id->firm_type != WI_NOTYPE; id++) {
		if (chip_id == id->card_id)
			break;
	}
	if (id->firm_type != WI_NOTYPE)
		chip_name = id->card_name;
	else {
		if (chip_id & htole16(0x8000))
			chip_name = "Unknown PRISM chip";
		else
			chip_name = "Unknown Lucent chip";
	}

	/* XXX - doesn't decode Symbol firmware */
	if (chip_id & htole16(0x8000))
		printf("[ %s, Firmware %d.%d.%d ]", chip_name,
		    letoh16(wreq->wi_val[2]), letoh16(wreq->wi_val[3]),
		    letoh16(wreq->wi_val[1]));
	else
		printf("[ %s, Firmware %d.%d variant %d ]", chip_name,
		    letoh16(wreq->wi_val[2]), letoh16(wreq->wi_val[3]),
		    letoh16(wreq->wi_val[1]));
}

void
wi_printwords(wreq)
	struct wi_req		*wreq;
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", letoh16(wreq->wi_val[i]));
	printf("]");
}

void
wi_printbool(wreq)
	struct wi_req		*wreq;
{
	if (letoh16(wreq->wi_val[0]))
		printf("[ On ]");
	else
		printf("[ Off ]");
}

void
wi_printhex(wreq)
	struct wi_req		*wreq;
{
	int			i;
	unsigned char		*c;

	c = (unsigned char *)&wreq->wi_val;

	printf("[ ");
	for (i = 0; i < (wreq->wi_len - 1) * 2; i++) {
		printf("%02x", c[i]);
		if (i < ((wreq->wi_len - 1) * 2) - 1)
			printf(":");
	}

	printf(" ]");
}

void
wi_printalgorithm(wreq)
	struct wi_req   *wreq;
{
	switch(letoh16(wreq->wi_val[0])) {
	case WI_CRYPTO_FIRMWARE_WEP:
		printf("[ Firmware WEP ]");
		break;
	case WI_CRYPTO_SOFTWARE_WEP:
		printf("[ Software WEP ]");
		break;
	default:
		printf("[ Unknown ]");
		break;
	}
}

void
wi_printaplist(iface)
	char			*iface;
{
	int			prism2, len, i = 0, j, s, flags, nap;
	struct wi_req		wreq;
	struct wi_scan_p2_hdr	*wi_p2_h;
	struct wi_scan_res	*res;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");
	flags = get_if_flags(s, iface);
	if ((flags & IFF_UP) == 0)
		flags = set_if_flags(s, iface, flags | IFF_UP);

	/* first determine whether this is a prism2 card or not */
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_PRISM2;

	wi_getval(iface, &wreq);
	prism2 = wreq.wi_val[0];

	/* send out a scan request */
	wreq.wi_len = prism2 ? 3 : 1;
	wreq.wi_type = WI_RID_SCAN_REQ;

	if (prism2) {
		wreq.wi_val[0] = 0x3FFF;
		wreq.wi_val[1] = 0x000F;
	}

	wi_setval(iface, &wreq);

	/*
	 * sleep for 200 milliseconds so there's enough time for the card
	 * to respond... prism2's take a little longer.
	 */
	usleep(prism2 ? 700000 : 200000);

	/* get the scan results */
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_SCAN_RES;

	wi_getval(iface, &wreq);

	if (prism2) {
		wi_p2_h = (struct wi_scan_p2_hdr *)wreq.wi_val;

		/* if the reason is 0, this info is invalid */
		if (wi_p2_h->wi_reason == 0)
			return;

		i = 4;
	}

	len = prism2 ? WI_PRISM2_RES_SIZE : WI_WAVELAN_RES_SIZE;

	printf("AP Information\n");

	for (nap = 0; i < (wreq.wi_len * 2) - len; i += len) {
		res = (struct wi_scan_res *)((char *)wreq.wi_val + i);

		res->wi_ssid[letoh16(res->wi_ssid_len)] = '\0';
		res->wi_chan = letoh16(res->wi_chan);
		res->wi_noise = letoh16(res->wi_noise);
		res->wi_signal = letoh16(res->wi_signal);
		res->wi_interval = letoh16(res->wi_interval);
		res->wi_capinfo = letoh16(res->wi_capinfo);

		printf("ap[%d]:", nap++);
		printf("\tnetname (SSID):\t\t\t[ %s ]\n", res->wi_ssid);
		printf("\tBSSID:\t\t\t\t[ %02x:%02x:%02x:%02x:%02x:%02x ]\n",
		    res->wi_bssid[0], res->wi_bssid[1],
		    res->wi_bssid[2], res->wi_bssid[3],
		    res->wi_bssid[4], res->wi_bssid[5]);
		printf("\tChannel:\t\t\t[ %d ]\n", res->wi_chan);
		printf("\tBeacon Interval:\t\t[ %d ]\n", res->wi_interval);
		printf("\tQuality/Signal/Noise [signal]:\t[ %d / %d / %d ]\n",
		    res->wi_signal - res->wi_noise, res->wi_signal,
		    res->wi_noise);
		if (!prism2)
			printf("\t\t\t\t[dBm]:\t[ %d / %d / %d ]\n",
			    res->wi_signal - res->wi_noise,
			    res->wi_signal - 149, res->wi_noise - 149);

		if (res->wi_capinfo) {
			printf("\tCapinfo:\t\t\t[ ");
			if (res->wi_capinfo & WI_CAPINFO_ESS)
				printf("ESS ");
			if (res->wi_capinfo & WI_CAPINFO_IBSS)
				printf("IBSS ");
			if (res->wi_capinfo & WI_CAPINFO_PRIV)
				printf("PRIV ");
			printf("]\n");
		}

		if (prism2) {
			printf("\tDataRate [Mbps]:\t\t[ %2.1f ]\n",
			    res->wi_rate == 0xa ? 1 :
			    (res->wi_rate == 0x14 ? 2 :
			    (res->wi_rate == 0x37 ? 5.5 :
			    (res->wi_rate == 0x6e ? 11 : 0))));

			printf("\tAvailableRates [Mbps]:\t\t[ ");
			for (j = 0; res->wi_srates[j] != 0; j++) {
				res->wi_srates[j] = res->wi_srates[j] &
				    WI_VAR_SRATES_MASK;
				printf("%d.%d ", res->wi_srates[j] / 2,
				    (res->wi_srates[j] % 2) * 5);
			}
			printf("]\n");
		}
	}
	set_if_flags(s, iface, flags);
	close(s);
	return;
}

#define WI_STRING		0x01
#define WI_BOOL			0x02
#define WI_WORDS		0x03
#define WI_HEXBYTES		0x04
#define WI_KEYSTRUCT		0x05
#define WI_CARDINFO		0x06
#define WI_ALGORITHM		0x07

struct wi_table {
	int			wi_code;
	int			wi_type;
	char			*wi_str;
};

struct wi_table wi_table[] = {
	{ WI_RID_SERIALNO, WI_STRING, "NIC serial number:\t\t\t" },
	{ WI_RID_NODENAME, WI_STRING, "Station name:\t\t\t\t" },
	{ WI_RID_OWN_SSID, WI_STRING, "SSID for IBSS creation:\t\t\t" },
	{ WI_RID_CURRENT_SSID, WI_STRING, "Current netname (SSID):\t\t\t" },
	{ WI_RID_DESIRED_SSID, WI_STRING, "Desired netname (SSID):\t\t\t" },
	{ WI_RID_CURRENT_BSSID, WI_HEXBYTES, "Current BSSID:\t\t\t\t" },
	{ WI_RID_CHANNEL_LIST, WI_WORDS, "Channel list:\t\t\t\t" },
	{ WI_RID_OWN_CHNL, WI_WORDS, "IBSS channel:\t\t\t\t" },
	{ WI_RID_CURRENT_CHAN, WI_WORDS, "Current channel:\t\t\t" },
	{ WI_RID_COMMS_QUALITY, WI_WORDS, "Comms quality/signal/noise:\t\t" },
	{ WI_RID_PROMISC, WI_BOOL, "Promiscuous mode:\t\t\t" },
	{ WI_RID_PROCFRAME, WI_BOOL, "Process 802.11b Frame:\t\t\t" },
	{ WI_RID_PORTTYPE, WI_WORDS, "Port type (1=BSS, 3=ad-hoc, 6=Host AP):\t"},
	{ WI_RID_MAC_NODE, WI_HEXBYTES, "MAC address:\t\t\t\t"},
	{ WI_RID_TX_RATE, WI_WORDS, "TX rate (selection):\t\t\t"},
	{ WI_RID_CUR_TX_RATE, WI_WORDS, "TX rate (actual speed):\t\t\t"},
	{ WI_RID_MAX_DATALEN, WI_WORDS, "Maximum data length:\t\t\t"},
	{ WI_RID_RTS_THRESH, WI_WORDS, "RTS/CTS handshake threshold:\t\t"},
	{ WI_RID_CREATE_IBSS, WI_BOOL, "Create IBSS:\t\t\t\t" },
	{ WI_RID_SYMBOL_DIVERSITY, WI_WORDS, "Antenna diversity (0=auto,1=pri,2=aux):\t"},
	{ WI_RID_MICROWAVE_OVEN, WI_BOOL, "Microwave oven robustness:\t\t"},
	{ WI_RID_ROAMING_MODE, WI_WORDS, "Roaming mode(1=firm,3=disable):\t\t"},
	{ WI_RID_SYSTEM_SCALE, WI_WORDS, "Access point density:\t\t\t" },
	{ WI_RID_PM_ENABLED, WI_BOOL, "Power Management:\t\t\t" },
	{ WI_RID_MAX_SLEEP, WI_WORDS, "Max sleep time:\t\t\t\t" },
	{ WI_RID_PRISM2, WI_WORDS, "Intersil Prism2-based card:\t\t" },
	{ WI_RID_STA_IDENTITY, WI_CARDINFO, "Card info:\t\t\t\t" },
	{ 0, NULL }
};

struct wi_table wi_crypt_table[] = {
	{ WI_RID_ENCRYPTION, WI_BOOL, "Encryption:\t\t\t\t" },
	{ WI_FRID_CRYPTO_ALG, WI_ALGORITHM, "Encryption algorithm:\t\t\t" },
	{ WI_RID_CNFAUTHMODE, WI_WORDS,
	  "Authentication type \n(1=OpenSys, 2=Shared Key):\t\t" },
	{ WI_RID_TX_CRYPT_KEY, WI_WORDS, "TX encryption key:\t\t\t" },
	{ WI_RID_DEFLT_CRYPT_KEYS, WI_KEYSTRUCT, "Encryption keys:\t\t\t" },
	{ 0, NULL }
};

void
wi_dumpinfo(iface)
	char			*iface;
{
	struct wi_req		wreq;
	int			i, has_wep, chip_id;
	struct wi_table		*w;

	/* Get chip ID. */
	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_type = WI_RID_CARD_ID;
	wreq.wi_len = 5;
	wi_getval(iface, &wreq);
	chip_id = letoh16(wreq.wi_val[0]);

	/* Check for WEP support. */
	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_type = WI_RID_WEP_AVAIL;
	wreq.wi_len = 2;
	wi_getval(iface, &wreq);
	has_wep = letoh16(wreq.wi_val[0]);

	w = wi_table;

	for (i = 0; w[i].wi_type; i++) {
		bzero((char *)&wreq, sizeof(wreq));

		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = w[i].wi_code;

		wi_getval(iface, &wreq);
		printf("%s", w[i].wi_str);
		switch (w[i].wi_type) {
		case WI_STRING:
			wi_printstr(&wreq);
			break;
		case WI_WORDS:
			wi_printwords(&wreq);
			break;
		case WI_BOOL:
			wi_printbool(&wreq);
			break;
		case WI_HEXBYTES:
			wi_printhex(&wreq);
			break;
		case WI_CARDINFO:
			wi_printcardid(&wreq, chip_id);
			break;
		default:
			break;
		}	
		printf("\n");
	}

	if (has_wep) {
		w = wi_crypt_table;
		for (i = 0; w[i].wi_type; i++) {
			bzero((char *)&wreq, sizeof(wreq));

			wreq.wi_len = WI_MAX_DATALEN;
			wreq.wi_type = w[i].wi_code;

			wi_getval(iface, &wreq);
			printf("%s", w[i].wi_str);
			switch (w[i].wi_type) {
			case WI_STRING:
				wi_printstr(&wreq);
				break;
			case WI_WORDS:
				if (wreq.wi_type == WI_RID_TX_CRYPT_KEY)
					wreq.wi_val[0] =
					   htole16(letoh16(wreq.wi_val[0]) + 1);
				wi_printwords(&wreq);
				break;
			case WI_BOOL:
				wi_printbool(&wreq);
				break;
			case WI_HEXBYTES:
				wi_printhex(&wreq);
				break;
			case WI_KEYSTRUCT:
				wi_printkeys(&wreq);
				break;
			case WI_ALGORITHM:
				wi_printalgorithm(&wreq);
				break;
			default:
				break;
			}	
			printf("\n");
		}
	}
}

void
wi_dumpstats(iface)
	char			*iface;
{
	struct wi_req		wreq;
	struct wi_counters	*c;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_IFACE_STATS;

	wi_getval(iface, &wreq);

	c = (struct wi_counters *)&wreq.wi_val;

	/* XXX native byte order */
	printf("Transmitted unicast frames:\t\t%u\n",
	    c->wi_tx_unicast_frames);
	printf("Transmitted multicast frames:\t\t%u\n",
	    c->wi_tx_multicast_frames);
	printf("Transmitted fragments:\t\t\t%u\n",
	    c->wi_tx_fragments);
	printf("Transmitted unicast octets:\t\t%u\n",
	    c->wi_tx_unicast_octets);
	printf("Transmitted multicast octets:\t\t%u\n",
	    c->wi_tx_multicast_octets);
	printf("Single transmit retries:\t\t%u\n",
	    c->wi_tx_single_retries);
	printf("Multiple transmit retries:\t\t%u\n",
	    c->wi_tx_multi_retries);
	printf("Transmit retry limit exceeded:\t\t%u\n",
	    c->wi_tx_retry_limit);
	printf("Transmit discards:\t\t\t%u\n",
	    c->wi_tx_discards);
	printf("Transmit discards due to wrong SA:\t%u\n",
	    c->wi_tx_discards_wrong_sa);
	printf("Received unicast frames:\t\t%u\n",
	    c->wi_rx_unicast_frames);
	printf("Received multicast frames:\t\t%u\n",
	    c->wi_rx_multicast_frames);
	printf("Received fragments:\t\t\t%u\n",
	    c->wi_rx_fragments);
	printf("Received unicast octets:\t\t%u\n",
	    c->wi_rx_unicast_octets);
	printf("Received multicast octets:\t\t%u\n",
	    c->wi_rx_multicast_octets);
	printf("Receive FCS errors:\t\t\t%u\n",
	    c->wi_rx_fcs_errors);
	printf("Receive discards due to no buffer:\t%u\n",
	    c->wi_rx_discards_nobuf);
	printf("Can't decrypt WEP frame:\t\t%u\n",
	    c->wi_rx_WEP_cant_decrypt);
	printf("Received message fragments:\t\t%u\n",
	    c->wi_rx_msg_in_msg_frags);
	printf("Received message bad fragments:\t\t%u\n",
	    c->wi_rx_msg_in_bad_msg_frags);
}

void
wi_dumpstations(iface)
	char			*iface;
{
	struct hostap_getall    reqall;
	struct hostap_sta	stas[WIHAP_MAX_STATIONS];
	struct ifreq		ifr;
	int i, s;

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t) & reqall;
	bzero(&reqall, sizeof(reqall));
	reqall.size = sizeof(stas);
	reqall.addr = stas;
	bzero(&stas, sizeof(stas));

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCHOSTAP_GETALL, &ifr) < 0)
		err(1, "SIOCHOSTAP_GETALL");

	printf("%d station%s:\n", reqall.nstations, reqall.nstations>1?"s":"");
	for (i = 0; i < reqall.nstations; i++) {
		struct hostap_sta *info = &stas[i];

		printf("%02x:%02x:%02x:%02x:%02x:%02x  asid=%04x",
			info->addr[0], info->addr[1], info->addr[2],
			info->addr[3], info->addr[4], info->addr[5],
			info->asid - 0xc001);

		printb(", flags", info->flags, HOSTAP_FLAGS_BITS);
		printb(", caps", info->capinfo, IEEE80211_CAPINFO_BITS);
		printb(", rates", info->rates, WI_RATES_BITS);
		if (info->sig_info)
			printf(", sig=%d/%d",
			    info->sig_info >> 8, info->sig_info & 0xff);
		putchar('\n');
	}
}

__dead void
usage()
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [interface] [-olL] [-t tx rate] [-n network name]\n"
	    "       [-s station name] [-e 0|1] [-k key [-v 1|2|3|4]] [-T 1|2|3|4]\n"
	    "       [-x 0|1] [-F 0|1] [-c 0|1] [-q SSID] [-p port type]\n"
	    "       [-a access point density] [-m MAC address] [-d max data length]\n"
	    "       [-r RTS threshold] [-f frequency] [-M 0|1] [-P 0|1]\n"
	    "       [-S max sleep duration] [-A 1|2|3] [-D 0|1|2] [-R 1|3]\n",
	    __progname);
	exit(1);
}

struct wi_func {
	int   key;
	void (*function) (char *, int, char *);
	int   wi_code;
	char  *optarg;
};

struct wi_func wi_opt[] = {
	{ 'k', wi_setkeys, 0, NULL },  /* MUST be first entry in table */
	{ 'a', wi_setword, WI_RID_SYSTEM_SCALE, NULL },
	{ 'c', wi_setword, WI_RID_CREATE_IBSS, NULL },
	{ 'd', wi_setword, WI_RID_MAX_DATALEN, NULL },
	{ 'e', wi_setword, WI_RID_ENCRYPTION, NULL },
	{ 'f', wi_setword, WI_RID_OWN_CHNL, NULL },
	{ 'm', wi_sethex, WI_RID_MAC_NODE, NULL },
	{ 'n', wi_setstr, WI_RID_DESIRED_SSID, NULL },
	{ 'p', wi_setword, WI_RID_PORTTYPE, NULL },
	{ 'q', wi_setstr, WI_RID_OWN_SSID, NULL },
	{ 'r', wi_setword, WI_RID_RTS_THRESH, NULL },
	{ 's', wi_setstr, WI_RID_NODENAME, NULL },
	{ 't', wi_setword, WI_RID_TX_RATE, NULL },
	{ 'x', wi_setword, WI_FRID_CRYPTO_ALG, NULL },
	{ 'A', wi_setword, WI_RID_CNFAUTHMODE, NULL },
	{ 'D', wi_setword, WI_RID_SYMBOL_DIVERSITY, NULL },
	{ 'M', wi_setword, WI_RID_MICROWAVE_OVEN, NULL },
	{ 'P', wi_setword, WI_RID_PM_ENABLED, NULL },
	{ 'R', wi_setword, WI_RID_ROAMING_MODE, NULL },
	{ 'S', wi_setword, WI_RID_MAX_SLEEP, NULL },
	{ 'T', wi_setword, WI_RID_TX_CRYPT_KEY, NULL },
	{ 'F', wi_setword, WI_RID_PROCFRAME, NULL },

	/* These options will never be command line options which is why
	   they are not 'quoted' */
	{ 1, wi_setkeys, 0, NULL }, /* Dummy option for key 0 */
	{ 2, wi_setkeys, 1, NULL }, /* key 1 */
	{ 3, wi_setkeys, 2, NULL }, /* key 2 */
	{ 4, wi_setkeys, 3, NULL }, /* key 3 */
	{ 0, NULL, 0, NULL }
};

int
main(argc, argv)
	int			argc;
	char			*argv[];
{
	char	*iface = "wi0";
	int	ch, p, dumpstats, dumpinfo = 1, ifspecified;
	int	listaps, dumpstations;

	dumpstats = ifspecified = listaps = dumpstations = 0;
	if (argc > 1 && argv[1][0] != '-') {
		iface = argv[1];
		memcpy(&argv[1], &argv[2], argc * sizeof(char *));
		argc--;
		ifspecified = 1;
	}

	while ((ch = getopt(argc, argv,
	    "a:c:d:e:f:hi:k:lm:n:op:q:r:s:t:v:x:A:D:F:LM:S:P:R:T:")) != -1) {
		for (p = 0; ch && wi_opt[p].key; p++)
			if (ch == wi_opt[p].key) {
				if (ch == 'p' && !isdigit(*optarg))
					wi_opt[p].optarg = portid(optarg);
				else
					wi_opt[p].optarg = optarg;
				if (ch == 'T')	/* key 1-4/0-3 kludge */
					(*optarg)--;
				dumpinfo = ch = 0;
			}
		switch (ch) {
		case 0:
			break;
		case 'i':
			if (!ifspecified)
				iface = optarg;
			break;
		case 'o':
			dumpstats++;
			break;
		case 'L':
			listaps++;
			break;
		case 'l':
			dumpstations++;
			break;
		case 'v':
			for (p = 0; wi_opt[p].key; p++)
				if (wi_opt[p].key ==
				    strtol(optarg, NULL, 10)) {
					wi_opt[p].optarg = wi_opt[0].optarg;
					/* prevent multiple -v without
					   multiple -k */
					wi_opt[0].optarg = NULL;
					break;
				}
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	for (p = 0; wi_opt[p].key; p++)
		if (wi_opt[p].optarg != NULL)
			wi_opt[p].function(iface, wi_opt[p].wi_code,
					   wi_opt[p].optarg);

	if (listaps)
		wi_printaplist(iface);

	if (dumpstations)
		wi_dumpstations(iface);

	if (dumpstats && !listaps && !dumpstations)
		wi_dumpstats(iface);

	if (dumpinfo && !dumpstats && !listaps && !dumpstations)
		wi_dumpinfo(iface);

	exit(0);
}

int
get_if_flags(s, name)
	int		s;
	const char	*name;
{
	struct ifreq	ifr;
	int		flags;

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) == -1)
		  err(1, "SIOCGIFFLAGS");
	flags = ifr.ifr_flags;

	return (flags);
}

int
set_if_flags(s, name, flags)
	int		s;
	const char	*name;
	int		flags;
{
	struct ifreq ifr;

	ifr.ifr_flags = flags;
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) == -1)
		err(1, "SIOCSIFFLAGS");

	return 0;
}

/*
 * Print a value a la the %b format of the kernel's printf
 * (ripped screaming from ifconfig/ifconfig.c)
 */
void
printb(s, v, bits)
	char *s;
	char *bits;
	unsigned short v;
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

char *
portid(char *name)
{
	char *id;

	if (strcasecmp(name, "bss") == 0)
		id = "1";
	else if (strcasecmp(name, "adhoc") == 0 ||
	    strcasecmp(name, "ad-hoc") == 0)
		id = "3";
	else if (strcasecmp(name, "ibss") == 0)
		id = "4";
	else if (strcasecmp(name, "hostap") == 0)
		id = "6";
	else
		errx(1, "unknown port type %s", name);

	return(id);
}
