/*	$OpenBSD: net80211.c,v 1.12 2014/11/21 17:49:00 mikeb Exp $	*/

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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

/*
 * Dump IEEE802.11 per-interface statistics
 */
void
net80211_ifstats(char *ifname)
{
	struct ifreq ifr;
	struct ieee80211_stats stats;
	int s;

#define	p(f, m)	printf(m, (unsigned long)stats.f, plural(stats.f))

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket(AF_INET)");

	ifr.ifr_data = (caddr_t)&stats;
	strlcpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCG80211STATS, &ifr) < 0)
		err(1, "ioctl(SIOCG80211STATS)");

	printf("ieee80211 on %s:\n", ifr.ifr_name);

	p(is_rx_badversion, "\t%lu input packet%s with bad version\n");
	p(is_rx_tooshort, "\t%lu input packet%s too short\n");
	p(is_rx_wrongbss, "\t%lu input packet%s from wrong bssid\n");
	p(is_rx_dup, "\t%lu input packet duplicate%s discarded\n");
	p(is_rx_wrongdir, "\t%lu input packet%s with wrong direction\n");
	p(is_rx_mcastecho, "\t%lu input multicast echo packet%s discarded\n");
	p(is_rx_notassoc, "\t%lu input packet%s from unassociated station discarded\n");
	p(is_rx_nowep, "\t%lu input encrypted packet%s without wep/wpa config discarded\n");
	p(is_rx_unencrypted, "\t%lu input unencrypted packet%s with wep/wpa config discarded\n");
	p(is_rx_wepfail, "\t%lu input wep/wpa packet%s processing failed\n");
	p(is_rx_decap, "\t%lu input packet decapsulation%s failed\n");
	p(is_rx_mgtdiscard, "\t%lu input management packet%s discarded\n");
	p(is_rx_ctl, "\t%lu input control packet%s discarded\n");
	p(is_rx_rstoobig, "\t%lu input packet%s with truncated rate set\n");
	p(is_rx_elem_missing, "\t%lu input packet%s with missing elements\n");
	p(is_rx_elem_toobig, "\t%lu input packet%s with elements too big\n");
	p(is_rx_elem_toosmall, "\t%lu input packet%s with elements too small\n");
	p(is_rx_badchan, "\t%lu input packet%s with invalid channel\n");
	p(is_rx_chanmismatch, "\t%lu input packet%s with mismatched channel\n");
	p(is_rx_nodealloc, "\t%lu node allocation%s failed\n");
	p(is_rx_ssidmismatch, "\t%lu input packet%s with mismatched ssid\n");
	p(is_rx_auth_unsupported, "\t%lu input packet%s with unsupported auth algorithm\n");
	p(is_rx_auth_fail, "\t%lu input authentication%s failed\n");
	p(is_rx_assoc_bss, "\t%lu input association%s from wrong bssid\n");
	p(is_rx_assoc_notauth, "\t%lu input association%s without authentication\n");
	p(is_rx_assoc_capmismatch, "\t%lu input association%s with mismatched capabilities\n");
	p(is_rx_assoc_norate, "\t%lu input association%s without matching rates\n");
	p(is_rx_assoc_badrsnie, "\t%lu input association%s with bad rsn ie\n");
	p(is_rx_deauth, "\t%lu input deauthentication packet%s\n");
	p(is_rx_disassoc, "\t%lu input disassociation packet%s\n");
	p(is_rx_badsubtype, "\t%lu input packet%s with unknown subtype\n");
	p(is_rx_nombuf, "\t%lu input packet%s failed for lack of mbufs\n");
	p(is_rx_decryptcrc, "\t%lu input decryption%s failed on crc\n");
	p(is_rx_ahdemo_mgt, "\t%lu input ahdemo management packet%s discarded\n");
	p(is_rx_bad_auth, "\t%lu input packet%s with bad auth request\n");
	p(is_rx_eapol_key, "\t%lu input eapol-key packet%s\n");
	p(is_rx_eapol_badmic, "\t%lu input eapol-key packet%s with bad mic\n");
	p(is_rx_eapol_replay, "\t%lu input eapol-key packet%s replayed\n");
	p(is_rx_locmicfail, "\t%lu input packet%s with bad tkip mic\n");
	p(is_rx_remmicfail, "\t%lu input tkip mic failure notification%s\n");
	p(is_rx_unauth, "\t%lu input packet%s on unauthenticated port\n");
	p(is_tx_nombuf, "\t%lu output packet%s failed for lack of mbufs\n");
	p(is_tx_nonode, "\t%lu output packet%s failed for no nodes\n");
	p(is_tx_unknownmgt, "\t%lu output packet%s of unknown management type\n");
	p(is_tx_noauth, "\t%lu output packet%s on unauthenticated port\n");
	p(is_scan_active, "\t%lu active scan%s started\n");
	p(is_scan_passive, "\t%lu passive scan%s started\n");
	p(is_node_timeout, "\t%lu node%s timed out\n");
	p(is_crypto_nomem, "\t%lu failure%s with no memory for crypto ctx\n");
	p(is_ccmp_dec_errs, "\t%lu ccmp decryption error%s\n");
	p(is_ccmp_replays, "\t%lu ccmp replayed frame%s \n");
	p(is_cmac_icv_errs, "\t%lu cmac icv error%s\n");
	p(is_cmac_replays, "\t%lu cmac replayed frame%s\n");
	p(is_tkip_icv_errs, "\t%lu tkip icv error%s\n");
	p(is_tkip_replays, "\t%lu tkip replay%s\n");

	close(s);

#undef p
}
