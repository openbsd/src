/*	$OpenBSD: ancontrol.c,v 1.23 2004/05/13 16:36:38 mickey Exp $	*/
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Copyright 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 * $FreeBSD: src/usr.sbin/ancontrol/ancontrol.c,v 1.1 2000/01/14 20:40:57 wpaul Exp $
 */

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/ic/anvar.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#if !defined(lint)
static const char copyright[] = "@(#) Copyright (c) 1997, 1998, 1999\
	Bill Paul. All rights reserved.";
static const char rcsid[] =
  "@(#) $FreeBSD: src/usr.sbin/ancontrol/ancontrol.c,v 1.1 2000/01/14 20:40:57 wpaul Exp $";
#endif

#define	an_printbool(val) printf(val? "[ On ]" : "[ Off ]")

void an_getval(struct an_req *);
void an_setval(struct an_req *);
void an_printwords(u_int16_t *, int);
void an_printspeeds(u_int8_t*, int);
void an_printhex(char *, int);
void an_printstr(char *, int);
void an_dumpstatus(void);
void an_dumpstats(void);
void an_dumpconfig(void);
void an_dumpcaps(void);
void an_dumpssid(void);
void an_dumpap(void);
void an_setconfig(int, void *);
void an_setssid(int, void *);
void an_setap(int, void *);
void an_setspeed(void *);
void an_readkeyinfo(void);
#ifdef ANCACHE
void an_zerocache(void);
void an_readcache(void);
#endif
void getsock(void);
static void usage(void);
int main(int, char **);

/* flags to trigger dumping information about configs */
#define STAT_DUMPAP	0x01
#define STAT_DUMPCONFIG	0x02
#define STAT_DUMPCAPS	0x04
#define STAT_DUMPSSID	0x08
#define STAT_DUMPSTATUS	0x10
#define STAT_DUMPSTATS	0x20

#define ACT_SET_OPMODE 7
#define ACT_SET_SSID1 8
#define ACT_SET_SSID2 9
#define ACT_SET_SSID3 10
#define ACT_SET_FREQ 11
#define ACT_SET_AP1 12
#define ACT_SET_AP2 13
#define ACT_SET_AP3 14
#define ACT_SET_AP4 15
#define ACT_SET_DRIVERNAME 16
#define ACT_SET_SCANMODE 17
#define ACT_SET_TXRATE 18
#define ACT_SET_RTS_THRESH 19
#define ACT_SET_PWRSAVE 20
#define ACT_SET_DIVERSITY_RX 21
#define ACT_SET_DIVERSITY_TX 22
#define ACT_SET_RTS_RETRYLIM 23
#define ACT_SET_WAKE_DURATION 24
#define ACT_SET_BEACON_PERIOD 25
#define ACT_SET_TXPWR 26
#define ACT_SET_FRAG_THRESH 27
#define ACT_SET_NETJOIN 28
#define ACT_SET_MYNAME 29
#define ACT_SET_MAC 30

#define ACT_ENABLE_WEP 33
#define ACT_SET_KEY_TYPE 34
#define ACT_SET_KEYS 35
#define ACT_ENABLE_TX_KEY 36

#ifdef ANCACHE
#define ACT_DUMPCACHE 31
#define ACT_ZEROCACHE 32
#define OPTIONS "a:b:c:d:e:f:j:k:l:m:n:o:p:r:s:t:v:w:ACIK:NQSTW:Z"
#else
#define OPTIONS "a:b:c:d:e:f:j:k:l:m:n:o:p:r:s:t:v:w:ACIK:NSTW:"
#endif /* ANCACHE */

int s;			/* Global socket for ioctl's */
struct ifreq ifr;	/* Global ifreq */

void
getsock(void)
{
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		errx(1, "socket");
}

void
an_getval(struct an_req *areq)
{
	ifr.ifr_data = (caddr_t)areq;

	if (ioctl(s, SIOCGAIRONET, &ifr) == -1)
		err(1, "SIOCGAIRONET");

	return;
}

void
an_setval(struct an_req *areq)
{
	ifr.ifr_data = (caddr_t)areq;

	if (ioctl(s, SIOCSAIRONET, &ifr) == -1)
		err(1, "SIOCSAIRONET");

	return;
}

void
an_printstr(char *str, int len)
{
	int			i;

	for (i = 0; i < len - 1; i++) {
		if (str[i] == '\0')
			str[i] = ' ';
	}

	printf("[ %.*s ]", len, str);

	return;
}

void
an_printwords(u_int16_t *w, int len)
{
	int			i;

	printf("[ ");
	for (i = 0; i < len; i++)
		printf("%u ", w[i]);
	printf("]");

	return;
}

void
an_printspeeds(u_int8_t *w, int len)
{
	int			i;

	printf("[ ");
	for (i = 0; i < len && w[i]; i++)
		printf("%2.1fMbps ", w[i] * 0.500);
	printf("]");

	return;
}

void
an_printhex(char *ptr, int len)
{
	int			i;

	printf("[ ");
	for (i = 0; i < len; i++) {
		printf("%02x", ptr[i] & 0xFF);
		if (i < (len - 1))
			printf(":");
	}

	printf(" ]");
	return;
}

void
an_dumpstatus(void)
{
	struct an_ltv_status	*sts;
	struct an_req		areq;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_STATUS;

	an_getval(&areq);

	sts = (struct an_ltv_status *)&areq;

	printf("MAC address:\t\t");
	an_printhex((char *)&sts->an_macaddr, ETHER_ADDR_LEN);
	printf("\nOperating mode:\t\t[ ");
	if (sts->an_opmode & AN_STATUS_OPMODE_CONFIGURED)
		printf("configured ");
	if (sts->an_opmode & AN_STATUS_OPMODE_MAC_ENABLED)
		printf("MAC ON ");
	if (sts->an_opmode & AN_STATUS_OPMODE_RX_ENABLED)
		printf("RX ON ");
	if (sts->an_opmode & AN_STATUS_OPMODE_IN_SYNC)
		printf("synced ");
	if (sts->an_opmode & AN_STATUS_OPMODE_ASSOCIATED)
		printf("associated ");
	if (sts->an_opmode & AN_STATUS_OPMODE_ERROR)
		printf("error ");
	printf("]\n");
	printf("Error code:\t\t");
	an_printhex((char *)&sts->an_errcode, 1);
	printf("\nSignal strength:\t[ %u%% ]",sts->an_normalized_rssi);
	printf("\nSignal quality:\t\t");
	an_printhex((char *)&sts->an_cur_signal_quality, 1);
	printf("\nCurrent SSID:\t\t");
	an_printstr((char *)&sts->an_ssid, sts->an_ssidlen);
	printf("\nCurrent AP name:\t");
	an_printstr((char *)&sts->an_ap_name, 16);
	printf("\nCurrent BSSID:\t\t");
	an_printhex((char *)&sts->an_cur_bssid, ETHER_ADDR_LEN);
	printf("\nBeacon period:\t\t");
	an_printwords(&sts->an_beacon_period, 1);
	printf("\nDTIM period:\t\t");
	an_printwords(&sts->an_dtim_period, 1);
	printf("\nATIM duration:\t\t");
	an_printwords(&sts->an_atim_duration, 1);
	printf("\nHOP period:\t\t");
	an_printwords(&sts->an_hop_period, 1);
	printf("\nChannel set:\t\t");
	an_printwords(&sts->an_channel_set, 1);
	printf("\nCurrent channel:\t");
	an_printwords(&sts->an_cur_channel, 1);
	printf("\nHops to backbone:\t");
	an_printwords(&sts->an_hops_to_backbone, 1);
	printf("\nTotal AP load:\t\t");
	an_printwords(&sts->an_ap_total_load, 1);
	printf("\nOur generated load:\t");
	an_printwords(&sts->an_our_generated_load, 1);
	printf("\nAccumulated ARL:\t");
	an_printwords(&sts->an_accumulated_arl, 1);
	printf("\n");
	return;
}

void
an_dumpcaps(void)
{
	struct an_ltv_caps	*caps;
	struct an_req		areq;
	u_int16_t		tmp;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_CAPABILITIES;

	an_getval(&areq);

	caps = (struct an_ltv_caps *)&areq;

	printf("OUI:\t\t\t");
	an_printhex((char *)&caps->an_oui, 3);
	printf("\nProduct number:\t\t");
	an_printwords(&caps->an_prodnum, 1);
	printf("\nManufacturer name:\t");
	an_printstr((char *)&caps->an_manufname, 32);
	printf("\nProduce name:\t\t");
	an_printstr((char *)&caps->an_prodname, 16);
	printf("\nFirmware version:\t");
	an_printstr((char *)&caps->an_prodvers, 1);
	printf("\nOEM MAC address:\t");
	an_printhex((char *)&caps->an_oemaddr, ETHER_ADDR_LEN);
	printf("\nAironet MAC address:\t");
	an_printhex((char *)&caps->an_aironetaddr, ETHER_ADDR_LEN);
	printf("\nRadio type:\t\t[ ");
	if (caps->an_radiotype & AN_RADIOTYPE_80211_FH)
		printf("802.11 FH");
	else if (caps->an_radiotype & AN_RADIOTYPE_80211_DS)
		printf("802.11 DS");
	else if (caps->an_radiotype & AN_RADIOTYPE_LM2000_DS)
		printf("LM2000 DS");
	else
		printf("unknown (%x)", caps->an_radiotype);
	printf(" ]");
	printf("\nRegulatory domain:\t");
	an_printwords(&caps->an_regdomain, 1);
	printf("\nAssigned CallID:\t");
	an_printhex((char *)&caps->an_callid, 6);
	printf("\nSupported speeds:\t");
	an_printspeeds(caps->an_rates, 8);
	printf("\nRX Diversity:\t\t[ ");
	if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_1_ONLY)
		printf("antenna 1 only");
	else if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_2_ONLY)
		printf("antenna 2 only");
	else if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_1_AND_2)
		printf("antenna 1 and 2");
	printf(" ]");
	printf("\nTX Diversity:\t\t[ ");
	if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_1_ONLY)
		printf("antenna 1 only");
	else if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_2_ONLY)
		printf("antenna 2 only");
	else if (caps->an_rx_diversity == AN_DIVERSITY_ANTENNA_1_AND_2)
		printf("antenna 1 and 2");
	printf(" ]");
	printf("\nSupported power levels:\t");
	an_printwords(caps->an_tx_powerlevels, 8);
	printf("\nHardware revision:\t");
	tmp = ntohs(caps->an_hwrev);
	an_printhex((char *)&tmp, 2);
	printf("\nSoftware revision:\t");
	tmp = ntohs(caps->an_fwrev);
	an_printhex((char *)&tmp, 2);
	printf("\nSoftware subrevision:\t");
	tmp = ntohs(caps->an_fwsubrev);
	an_printhex((char *)&tmp, 2);
	printf("\nInterface revision:\t");
	tmp = ntohs(caps->an_ifacerev);
	an_printhex((char *)&tmp, 2);
	printf("\nBootblock revision:\t");
	tmp = ntohs(caps->an_bootblockrev);
	an_printhex((char *)&tmp, 2);
	printf("\n");
	return;
}

void
an_dumpstats(void)
{
	struct an_ltv_stats	*stats;
	struct an_req		areq;
	caddr_t			ptr;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_32BITS_CUM;

	an_getval(&areq);

	ptr = (caddr_t)&areq;
	ptr -= 2;
	stats = (struct an_ltv_stats *)ptr;

	printf("RX overruns:\t\t\t\t\t[ %u ]\n", stats->an_rx_overruns);
	printf("RX PLCP CSUM errors:\t\t\t\t[ %u ]\n",
	    stats->an_rx_plcp_csum_errs);
	printf("RX PLCP format errors:\t\t\t\t[ %u ]\n",
	    stats->an_rx_plcp_format_errs);
	printf("RX PLCP length errors:\t\t\t\t[ %u ]\n",
	    stats->an_rx_plcp_len_errs);
	printf("RX MAC CRC errors:\t\t\t\t[ %u ]\n",
	    stats->an_rx_mac_crc_errs);
	printf("RX MAC CRC OK:\t\t\t\t\t[ %u ]\n",
	    stats->an_rx_mac_crc_ok);
	printf("RX WEP errors:\t\t\t\t\t[ %u ]\n",
	    stats->an_rx_wep_errs);
	printf("RX WEP OK:\t\t\t\t\t[ %u ]\n",
	    stats->an_rx_wep_ok);
	printf("Long retries:\t\t\t\t\t[ %u ]\n",
	    stats->an_retry_long);
	printf("Short retries:\t\t\t\t\t[ %u ]\n",
	    stats->an_retry_short);
	printf("Retries exhausted:\t\t\t\t[ %u ]\n",
	    stats->an_retry_max);
	printf("Bad ACK:\t\t\t\t\t[ %u ]\n",
	    stats->an_no_ack);
	printf("Bad CTS:\t\t\t\t\t[ %u ]\n",
	    stats->an_no_cts);
	printf("RX good ACKs:\t\t\t\t\t[ %u ]\n",
	    stats->an_rx_ack_ok);
	printf("RX good CTSs:\t\t\t\t\t[ %u ]\n",
	    stats->an_rx_cts_ok);
	printf("TX good ACKs:\t\t\t\t\t[ %u ]\n",
	    stats->an_tx_ack_ok);
	printf("TX good RTSs:\t\t\t\t\t[ %u ]\n",
	    stats->an_tx_rts_ok);
	printf("TX good CTSs:\t\t\t\t\t[ %u ]\n",
	    stats->an_tx_cts_ok);
	printf("LMAC multicasts transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_lmac_mcasts);
	printf("LMAC broadcasts transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_lmac_bcasts);
	printf("LMAC unicast frags transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_lmac_ucast_frags);
	printf("LMAC unicasts transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_lmac_ucasts);
	printf("Beacons transmitted:\t\t\t\t[ %u ]\n",
	    stats->an_tx_beacons);
	printf("Beacons received:\t\t\t\t[ %u ]\n",
	    stats->an_rx_beacons);
	printf("Single transmit collisions:\t\t\t[ %u ]\n",
	    stats->an_tx_single_cols);
	printf("Multiple transmit collisions:\t\t\t[ %u ]\n",
	    stats->an_tx_multi_cols);
	printf("Transmits without deferrals:\t\t\t[ %u ]\n",
	    stats->an_tx_defers_no);
	printf("Transmits deferred due to protocol:\t\t[ %u ]\n",
	    stats->an_tx_defers_prot);
	printf("Transmits deferred due to energy detect:\t\t[ %u ]\n",
	    stats->an_tx_defers_energy);
	printf("RX duplicate frames/frags:\t\t\t[ %u ]\n",
	    stats->an_rx_dups);
	printf("RX partial frames:\t\t\t\t[ %u ]\n",
	    stats->an_rx_partial);
	printf("TX max lifetime exceeded:\t\t\t[ %u ]\n",
	    stats->an_tx_too_old);
	printf("RX max lifetime exceeded:\t\t\t[ %u ]\n",
	    stats->an_tx_too_old);
	printf("Sync lost due to too many missed beacons:\t[ %u ]\n",
	    stats->an_lostsync_missed_beacons);
	printf("Sync lost due to ARL exceeded:\t\t\t[ %u ]\n",
	    stats->an_lostsync_arl_exceeded);
	printf("Sync lost due to deauthentication:\t\t[ %u ]\n",
	    stats->an_lostsync_deauthed);
	printf("Sync lost due to disassociation:\t\t[ %u ]\n",
	    stats->an_lostsync_disassociated);
	printf("Sync lost due to excess change in TSF timing:\t[ %u ]\n",
	    stats->an_lostsync_tsf_timing);
	printf("Host transmitted multicasts:\t\t\t[ %u ]\n",
	    stats->an_tx_host_mcasts);
	printf("Host transmitted broadcasts:\t\t\t[ %u ]\n",
	    stats->an_tx_host_bcasts);
	printf("Host transmitted unicasts:\t\t\t[ %u ]\n",
	    stats->an_tx_host_ucasts);
	printf("Host transmission failures:\t\t\t[ %u ]\n",
	    stats->an_tx_host_failed);
	printf("Host received multicasts:\t\t\t[ %u ]\n",
	    stats->an_rx_host_mcasts);
	printf("Host received broadcasts:\t\t\t[ %u ]\n",
	    stats->an_rx_host_bcasts);
	printf("Host received unicasts:\t\t\t\t[ %u ]\n",
	    stats->an_rx_host_ucasts);
	printf("Host receive discards:\t\t\t\t[ %u ]\n",
	    stats->an_rx_host_discarded);
	printf("HMAC transmitted multicasts:\t\t\t[ %u ]\n",
	    stats->an_tx_hmac_mcasts);
	printf("HMAC transmitted broadcasts:\t\t\t[ %u ]\n",
	    stats->an_tx_hmac_bcasts);
	printf("HMAC transmitted unicasts:\t\t\t[ %u ]\n",
	    stats->an_tx_hmac_ucasts);
	printf("HMAC transmissions failed:\t\t\t[ %u ]\n",
	    stats->an_tx_hmac_failed);
	printf("HMAC received multicasts:\t\t\t[ %u ]\n",
	    stats->an_rx_hmac_mcasts);
	printf("HMAC received broadcasts:\t\t\t[ %u ]\n",
	    stats->an_rx_hmac_bcasts);
	printf("HMAC received unicasts:\t\t\t\t[ %u ]\n",
	    stats->an_rx_hmac_ucasts);
	printf("HMAC receive discards:\t\t\t\t[ %u ]\n",
	    stats->an_rx_hmac_discarded);
	printf("HMAC transmits accepted:\t\t\t[ %u ]\n",
	    stats->an_tx_hmac_accepted);
	printf("SSID mismatches:\t\t\t\t[ %u ]\n",
	    stats->an_ssid_mismatches);
	printf("Access point mismatches:\t\t\t[ %u ]\n",
	    stats->an_ap_mismatches);
	printf("Speed mismatches:\t\t\t\t[ %u ]\n",
	    stats->an_rates_mismatches);
	printf("Authentication rejects:\t\t\t\t[ %u ]\n",
	    stats->an_auth_rejects);
	printf("Authentication timeouts:\t\t\t[ %u ]\n",
	    stats->an_auth_timeouts);
	printf("Association rejects:\t\t\t\t[ %u ]\n",
	    stats->an_assoc_rejects);
	printf("Association timeouts:\t\t\t\t[ %u ]\n",
	    stats->an_assoc_timeouts);
	printf("Management frames received:\t\t\t[ %u ]\n",
	    stats->an_rx_mgmt_pkts);
	printf("Management frames transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_mgmt_pkts);
	printf("Refresh frames received:\t\t\t[ %u ]\n",
	    stats->an_rx_refresh_pkts),
	printf("Refresh frames transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_refresh_pkts),
	printf("Poll frames received:\t\t\t\t[ %u ]\n",
	    stats->an_rx_poll_pkts);
	printf("Poll frames transmitted:\t\t\t[ %u ]\n",
	    stats->an_tx_poll_pkts);
	printf("Host requested sync losses:\t\t\t[ %u ]\n",
	    stats->an_lostsync_hostreq);
	printf("Host transmitted bytes:\t\t\t\t[ %u ]\n",
	    stats->an_host_tx_bytes);
	printf("Host received bytes:\t\t\t\t[ %u ]\n",
	    stats->an_host_rx_bytes);
	printf("Uptime in microseconds:\t\t\t\t[ %u ]\n",
	    stats->an_uptime_usecs);
	printf("Uptime in seconds:\t\t\t\t[ %u ]\n",
	    stats->an_uptime_secs);
	printf("Sync lost due to better AP:\t\t\t[ %u ]\n",
	    stats->an_lostsync_better_ap);

	return;
}

void
an_dumpap(void)
{
	struct an_ltv_aplist	*ap;
	struct an_req		areq;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_APLIST;

	an_getval(&areq);

	ap = (struct an_ltv_aplist *)&areq;
	printf("Access point 1:\t\t\t");
	an_printhex((char *)&ap->an_ap1, ETHER_ADDR_LEN);
	printf("\nAccess point 2:\t\t\t");
	an_printhex((char *)&ap->an_ap2, ETHER_ADDR_LEN);
	printf("\nAccess point 3:\t\t\t");
	an_printhex((char *)&ap->an_ap3, ETHER_ADDR_LEN);
	printf("\nAccess point 4:\t\t\t");
	an_printhex((char *)&ap->an_ap4, ETHER_ADDR_LEN);
	printf("\n");

	return;
}

void
an_dumpssid(void)
{
	struct an_ltv_ssidlist	*ssid;
	struct an_req		areq;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_SSIDLIST;

	an_getval(&areq);

	ssid = (struct an_ltv_ssidlist *)&areq;
	printf("SSID 1:\t\t\t[ %.*s ]\n", ssid->an_ssid1_len, ssid->an_ssid1);
	printf("SSID 2:\t\t\t[ %.*s ]\n", ssid->an_ssid2_len, ssid->an_ssid2);
	printf("SSID 3:\t\t\t[ %.*s ]\n", ssid->an_ssid3_len, ssid->an_ssid3);

	return;
}

void
an_dumpconfig(void)
{
	struct an_ltv_genconfig	*cfg;
	struct an_req		areq;
	unsigned char		div;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_ACTUALCFG;

	an_getval(&areq);

	cfg = (struct an_ltv_genconfig *)&areq;

	printf("Operating mode:\t\t\t\t[ ");
	if ((cfg->an_opmode & 0x7) == AN_OPMODE_IBSS_ADHOC)
		printf("ad-hoc");
	if ((cfg->an_opmode & 0x7) == AN_OPMODE_INFRASTRUCTURE_STATION)
		printf("infrastructure");
	if ((cfg->an_opmode & 0x7) == AN_OPMODE_AP)
		printf("access point");
	if ((cfg->an_opmode & 0x7) == AN_OPMODE_AP_REPEATER)
		printf("access point repeater");
	printf(" ]");
	printf("\nReceive mode:\t\t\t\t[ ");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_BC_MC_ADDR)
		printf("broadcast/multicast/unicast");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_BC_ADDR)
		printf("broadcast/unicast");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_ADDR)
		printf("unicast");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_80211_MONITOR_CURBSS)
		printf("802.11 monitor, current BSSID");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_80211_MONITOR_ANYBSS)
		printf("802.11 monitor, any BSSID");
	if ((cfg->an_rxmode & 0x7) == AN_RXMODE_LAN_MONITOR_CURBSS)
		printf("LAN monitor, current BSSID");
	printf(" ]");
	printf("\nFragment threshold:\t\t\t");
	an_printwords(&cfg->an_fragthresh, 1);
	printf("\nRTS threshold:\t\t\t\t");
	an_printwords(&cfg->an_rtsthresh, 1);
	printf("\nMAC address:\t\t\t\t");
	an_printhex((char *)&cfg->an_macaddr, ETHER_ADDR_LEN);
	printf("\nSupported rates:\t\t\t");
	an_printspeeds(cfg->an_rates, 8);
	printf("\nShort retry limit:\t\t\t");
	an_printwords(&cfg->an_shortretry_limit, 1);
	printf("\nLong retry limit:\t\t\t");
	an_printwords(&cfg->an_longretry_limit, 1);
	printf("\nTX MSDU lifetime:\t\t\t");
	an_printwords(&cfg->an_tx_msdu_lifetime, 1);
	printf("\nRX MSDU lifetime:\t\t\t");
	an_printwords(&cfg->an_rx_msdu_lifetime, 1);
	printf("\nStationary:\t\t\t\t");
	an_printbool(cfg->an_stationary);
	printf("\nOrdering:\t\t\t\t");
	an_printbool(cfg->an_ordering);
	printf("\nDevice type:\t\t\t\t[ ");
	if (cfg->an_devtype == AN_DEVTYPE_PC4500)
		printf("PC4500");
	else if (cfg->an_devtype == AN_DEVTYPE_PC4800)
		printf("PC4800");
	else
		printf("unknown (%x)", cfg->an_devtype);
	printf(" ]");
	printf("\nScanning mode:\t\t\t\t[ ");
	if (cfg->an_scanmode == AN_SCANMODE_ACTIVE)
		printf("active");
	if (cfg->an_scanmode == AN_SCANMODE_PASSIVE)
		printf("passive");
	if (cfg->an_scanmode == AN_SCANMODE_AIRONET_ACTIVE)
		printf("Aironet active");
	printf(" ]");
	printf("\nProbe delay:\t\t\t\t");
	an_printwords(&cfg->an_probedelay, 1);
	printf("\nProbe energy timeout:\t\t\t");
	an_printwords(&cfg->an_probe_energy_timeout, 1);
	printf("\nProbe response timeout:\t\t\t");
	an_printwords(&cfg->an_probe_response_timeout, 1);
	printf("\nBeacon listen timeout:\t\t\t");
	an_printwords(&cfg->an_beacon_listen_timeout, 1);
	printf("\nIBSS join network timeout:\t\t");
	an_printwords(&cfg->an_ibss_join_net_timeout, 1);
	printf("\nAuthentication timeout:\t\t\t");
	an_printwords(&cfg->an_auth_timeout, 1);
	printf("\nWEP enabled:\t\t\t\t[ ");
	if (cfg->an_authtype & AN_AUTHTYPE_PRIVACY_IN_USE) {
		if (cfg->an_authtype & AN_AUTHTYPE_ALLOW_UNENCRYPTED)
			printf("mixed cell");
		else
			printf("full");
	} else
		printf("no");
	printf(" ]");
	printf("\nAuthentication type:\t\t\t[ ");
	if ((cfg->an_authtype & AN_AUTHTYPE_MASK) == AN_AUTHTYPE_NONE)
		printf("none");
	if ((cfg->an_authtype & AN_AUTHTYPE_MASK) == AN_AUTHTYPE_OPEN)
		printf("open");
	if ((cfg->an_authtype & AN_AUTHTYPE_MASK) == AN_AUTHTYPE_SHAREDKEY)
		printf("shared key");
	printf(" ]");
	printf("\nAssociation timeout:\t\t\t");
	an_printwords(&cfg->an_assoc_timeout, 1);
	printf("\nSpecified AP association timeout:\t");
	an_printwords(&cfg->an_specified_ap_timeout, 1);
	printf("\nOffline scan interval:\t\t\t");
	an_printwords(&cfg->an_offline_scan_interval, 1);
	printf("\nOffline scan duration:\t\t\t");
	an_printwords(&cfg->an_offline_scan_duration, 1);
	printf("\nLink loss delay:\t\t\t");
	an_printwords(&cfg->an_link_loss_delay, 1);
	printf("\nMax beacon loss time:\t\t\t");
	an_printwords(&cfg->an_max_beacon_lost_time, 1);
	printf("\nRefresh interval:\t\t\t");
	an_printwords(&cfg->an_refresh_interval, 1);
	printf("\nPower save mode:\t\t\t[ ");
	if (cfg->an_psave_mode == AN_PSAVE_NONE)
		printf("none");
	if (cfg->an_psave_mode == AN_PSAVE_CAM)
		printf("constantly awake mode");
	if (cfg->an_psave_mode == AN_PSAVE_PSP)
		printf("PSP");
	if (cfg->an_psave_mode == AN_PSAVE_PSP_CAM)
		printf("PSP-CAM (fast PSP)");
	printf(" ]");
	printf("\nSleep through DTIMs:\t\t\t");
	an_printbool(cfg->an_sleep_for_dtims);
	printf("\nPower save listen interval:\t\t");
	an_printwords(&cfg->an_listen_interval, 1);
	printf("\nPower save fast listen interval:\t");
	an_printwords(&cfg->an_fast_listen_interval, 1);
	printf("\nPower save listen decay:\t\t");
	an_printwords(&cfg->an_listen_decay, 1);
	printf("\nPower save fast listen decay:\t\t");
	an_printwords(&cfg->an_fast_listen_decay, 1);
	printf("\nAP/ad-hoc Beacon period:\t\t");
	an_printwords(&cfg->an_beacon_period, 1);
	printf("\nAP/ad-hoc ATIM duration:\t\t");
	an_printwords(&cfg->an_atim_duration, 1);
	printf("\nAP/ad-hoc current channel:\t\t");
	an_printwords(&cfg->an_ds_channel, 1);
	printf("\nAP/ad-hoc DTIM period:\t\t\t");
	an_printwords(&cfg->an_dtim_period, 1);
	printf("\nRadio type:\t\t\t\t[ ");
	if (cfg->an_radiotype & AN_RADIOTYPE_80211_FH)
		printf("802.11 FH");
	else if (cfg->an_radiotype & AN_RADIOTYPE_80211_DS)
		printf("802.11 DS");
	else if (cfg->an_radiotype & AN_RADIOTYPE_LM2000_DS)
		printf("LM2000 DS");
	else
		printf("unknown (%x)", cfg->an_radiotype);
	printf(" ]");
	printf("\nRX Diversity:\t\t\t\t[ ");
	div = cfg->an_diversity & 0xFF;
	if (div == AN_DIVERSITY_ANTENNA_1_ONLY)
		printf("antenna 1 only");
	else if (div == AN_DIVERSITY_ANTENNA_2_ONLY)
		printf("antenna 2 only");
	else if (div == AN_DIVERSITY_ANTENNA_1_AND_2)
		printf("antenna 1 and 2");
	printf(" ]");
	printf("\nTX Diversity:\t\t\t\t[ ");
	div = (cfg->an_diversity >> 8) & 0xFF;
	if (div == AN_DIVERSITY_ANTENNA_1_ONLY)
		printf("antenna 1 only");
	else if (div == AN_DIVERSITY_ANTENNA_2_ONLY)
		printf("antenna 2 only");
	else if (div == AN_DIVERSITY_ANTENNA_1_AND_2)
		printf("antenna 1 and 2");
	printf(" ]");
	printf("\nTransmit power level:\t\t\t");
	an_printwords(&cfg->an_tx_power, 1);
	printf("\nRSS threshold:\t\t\t\t");
	an_printwords(&cfg->an_rss_thresh, 1);
	printf("\nNode name:\t\t\t\t");
	an_printstr((char *)&cfg->an_nodename, 16);
	printf("\nARL threshold:\t\t\t\t");
	an_printwords(&cfg->an_arl_thresh, 1);
	printf("\nARL decay:\t\t\t\t");
	an_printwords(&cfg->an_arl_decay, 1);
	printf("\nARL delay:\t\t\t\t");
	an_printwords(&cfg->an_arl_delay, 1);

	printf("\n");
	an_readkeyinfo();

	return;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ancontrol interface [-ACINSTh] [-t 0|1|2|3|4]\n"
	    "       [-s 0|1|2|3] [-v 1|2|3|4] [-a AP] [-b beacon period] [-v 0|1]\n"
	    "       [-d 1|2|3|4] [-e 0|1|2|3] [-j netjoin timeout] [-v 0|1|2|3|4|5|6|7[\n"
	    "       [-k key] [-K 0|1|2] [-l station name] [-m macaddress] [-v 1|2|3]\n"
	    "       [-n SSID] [-o 0|1] [-p tx power] [-c channel number]\n"
	    "       [-f fragmentation threshold] [-r RTS threshold] [-W 0|1|2]\n");
#ifdef ANCACHE
	fprintf(stderr,
	    "       [-Q] [-Z]\n");
#endif
	exit(1);
}

void
an_setconfig(int act, void *arg)
{
	struct an_ltv_genconfig	*cfg;
	struct an_ltv_caps	*caps;
	struct an_req		areq;
	struct an_req		areq_caps;
	u_int16_t		diversity = 0;
	struct ether_addr	*addr;
	int			i;

	bzero((char *)&areq, sizeof(struct an_req));
	bzero((char *)&areq_caps, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_GENCONFIG;
	an_getval(&areq);
	cfg = (struct an_ltv_genconfig *)&areq;

	areq_caps.an_len = sizeof(areq);
	areq_caps.an_type = AN_RID_CAPABILITIES;
	an_getval(&areq_caps);
	caps = (struct an_ltv_caps *)&areq_caps;

	switch(act) {
	case ACT_SET_OPMODE:
		cfg->an_opmode = atoi(arg);
		break;
	case ACT_SET_FREQ:
		cfg->an_ds_channel = atoi(arg);
		break;
	case ACT_SET_PWRSAVE:
		cfg->an_psave_mode = atoi(arg);
		break;
	case ACT_SET_SCANMODE:
		cfg->an_scanmode = atoi(arg);
		break;
	case ACT_SET_DIVERSITY_RX:
	case ACT_SET_DIVERSITY_TX:
		switch(atoi(arg)) {
		case 0:
			diversity = AN_DIVERSITY_FACTORY_DEFAULT;
			break;
		case 1:
			diversity = AN_DIVERSITY_ANTENNA_1_ONLY;
			break;
		case 2:
			diversity = AN_DIVERSITY_ANTENNA_2_ONLY;
			break;
		case 3:
			diversity = AN_DIVERSITY_ANTENNA_1_AND_2;
			break;
		default:
			errx(1, "bad diversity setting: %u", diversity);
			break;
		}
		if (atoi(arg) == ACT_SET_DIVERSITY_RX) {
			cfg->an_diversity &= 0x00FF;
			cfg->an_diversity |= (diversity << 8);
		} else {
			cfg->an_diversity &= 0xFF00;
			cfg->an_diversity |= diversity;
		}
		break;
	case ACT_SET_TXPWR:
		for (i = 0; i < 8; i++) {
			if (caps->an_tx_powerlevels[i] == atoi(arg))
				break;
		}
		if (i == 8)
			errx(1, "unsupported power level: %dmW", atoi(arg));

		cfg->an_tx_power = atoi(arg);
		break;
	case ACT_SET_RTS_THRESH:
		cfg->an_rtsthresh = atoi(arg);
		break;
	case ACT_SET_RTS_RETRYLIM:
		cfg->an_shortretry_limit =
		   cfg->an_longretry_limit = atoi(arg);
		break;
	case ACT_SET_BEACON_PERIOD:
		cfg->an_beacon_period = atoi(arg);
		break;
	case ACT_SET_WAKE_DURATION:
		cfg->an_atim_duration = atoi(arg);
		break;
	case ACT_SET_FRAG_THRESH:
		cfg->an_fragthresh = atoi(arg);
		break;
	case ACT_SET_NETJOIN:
		cfg->an_ibss_join_net_timeout = atoi(arg);
		break;
	case ACT_SET_MYNAME:
		bzero(cfg->an_nodename, 16);
		strncpy((char *)&cfg->an_nodename, optarg, 16);
		break;
	case ACT_SET_MAC:
		addr = ether_aton((char *)arg);

		if (addr == NULL)
			errx(1, "badly formatted address");
		bzero(cfg->an_macaddr, ETHER_ADDR_LEN);
		bcopy((char *)addr, (char *)&cfg->an_macaddr, ETHER_ADDR_LEN);
		break;
	case ACT_ENABLE_WEP:
		switch(atoi(arg)) {
		case 0:	/* WEP disabled */
			cfg->an_authtype &= ~(AN_AUTHTYPE_PRIVACY_IN_USE
					| AN_AUTHTYPE_ALLOW_UNENCRYPTED);
			break;
		case 1:	/* WEP enabled */
			cfg->an_authtype |= AN_AUTHTYPE_PRIVACY_IN_USE;
			cfg->an_authtype &= ~AN_AUTHTYPE_ALLOW_UNENCRYPTED;
			break;
		case 2:	/* WEP optional */
			cfg->an_authtype = AN_AUTHTYPE_PRIVACY_IN_USE
					| AN_AUTHTYPE_ALLOW_UNENCRYPTED;
			break;
		}
		break;
	case ACT_SET_KEY_TYPE:
		cfg->an_authtype = (cfg->an_authtype & ~AN_AUTHTYPE_MASK)
			| atoi(arg);
		break;
	default:
		errx(1, "unknown action");
		break;
	}

	an_setval(&areq);
}

void
an_setspeed(void *arg)
{
	struct an_req		areq;
	struct an_ltv_caps	*caps;
	u_int16_t		speed;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_CAPABILITIES;

	an_getval(&areq);
	caps = (struct an_ltv_caps *)&areq;

	switch(atoi(arg)) {
	case 0:
		speed = 0;
		break;
	case 1:
		speed = AN_RATE_1MBPS;
		break;
	case 2:
		speed = AN_RATE_2MBPS;
		break;
	case 3:
		if (caps->an_rates[2] != AN_RATE_5_5MBPS)
			errx(1, "5.5Mbps not supported on this card");
		speed = AN_RATE_5_5MBPS;
		break;
	case 4:
		if (caps->an_rates[3] != AN_RATE_11MBPS)
			errx(1, "11Mbps not supported on this card");
		speed = AN_RATE_11MBPS;
		break;
	default:
		errx(1, "unsupported speed");
		break;
	}

	areq.an_len = 6;
	areq.an_type = AN_RID_TX_SPEED;
	areq.an_val[0] = speed;

	an_setval(&areq);
}

void
an_setap(int act, void *arg)
{
	struct an_ltv_aplist	*ap;
	struct an_req		areq;
	struct ether_addr	*addr;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_APLIST;

	an_getval(&areq);
	ap = (struct an_ltv_aplist *)&areq;

	addr = ether_aton((char *)arg);

	if (addr == NULL)
		errx(1, "badly formatted address");

	switch(act) {
	case ACT_SET_AP1:
		bzero(ap->an_ap1, ETHER_ADDR_LEN);
		bcopy((char *)addr, (char *)&ap->an_ap1, ETHER_ADDR_LEN);
		break;
	case ACT_SET_AP2:
		bzero(ap->an_ap2, ETHER_ADDR_LEN);
		bcopy((char *)addr, (char *)&ap->an_ap2, ETHER_ADDR_LEN);
		break;
	case ACT_SET_AP3:
		bzero(ap->an_ap3, ETHER_ADDR_LEN);
		bcopy((char *)addr, (char *)&ap->an_ap3, ETHER_ADDR_LEN);
		break;
	case ACT_SET_AP4:
		bzero(ap->an_ap4, ETHER_ADDR_LEN);
		bcopy((char *)addr, (char *)&ap->an_ap4, ETHER_ADDR_LEN);
		break;
	default:
		errx(1, "unknown action");
		break;
	}

	an_setval(&areq);
}

void
an_setssid(int act, void *arg)
{
	struct an_ltv_ssidlist	*ssid;
	struct an_req		areq;

	bzero((char *)&areq, sizeof(struct an_req));
	areq.an_len = sizeof(areq);
	areq.an_type = AN_RID_SSIDLIST;

	an_getval(&areq);
	ssid = (struct an_ltv_ssidlist *)&areq;

	switch (act) {
	case ACT_SET_SSID1:
		bzero(ssid->an_ssid1, sizeof(ssid->an_ssid1));
		strlcpy(ssid->an_ssid1, (char *)arg, sizeof(ssid->an_ssid1));
		ssid->an_ssid1_len = strlen(ssid->an_ssid1);
		break;
	case ACT_SET_SSID2:
		bzero(ssid->an_ssid2, sizeof(ssid->an_ssid2));
		strlcpy(ssid->an_ssid2, (char *)arg, sizeof(ssid->an_ssid2));
		ssid->an_ssid2_len = strlen(ssid->an_ssid2);
		break;
	case ACT_SET_SSID3:
		bzero(ssid->an_ssid3, sizeof(ssid->an_ssid3));
		strlcpy(ssid->an_ssid3, (char *)arg, sizeof(ssid->an_ssid3));
		ssid->an_ssid3_len = strlen(ssid->an_ssid3);
		break;
	default:
		errx(1, "unknown action");
		break;
	}

	an_setval(&areq);
}

#ifdef ANCACHE
void
an_zerocache(void)
{
	struct an_req		areq;

	bzero((char *)&areq, sizeof(areq));
	areq.an_len = 0;
	areq.an_type = AN_RID_ZERO_CACHE;

	an_getval(&areq);

	return;
}

void
an_readcache(void)
{
	struct an_req		areq;
	int			*an_sigitems;
	struct an_sigcache	*sc;
	char *			pt;
	int			i;

	bzero((char *)&areq, sizeof(areq));
	areq.an_len = AN_MAX_DATALEN;
	areq.an_type = AN_RID_READ_CACHE;

	an_getval(&areq);

	an_sigitems = (int *) &areq.an_val;
	pt = ((char *) &areq.an_val);
	pt += sizeof(int);
	sc = (struct an_sigcache *) pt;

	for (i = 0; i < *an_sigitems; i++) {
		printf("[%d/%d]:", i+1, *an_sigitems);
		printf(" %02x:%02x:%02x:%02x:%02x:%02x,",
		    sc->macsrc[0]&0xff, sc->macsrc[1]&0xff,
		    sc->macsrc[2]&0xff, sc->macsrc[3]&0xff,
		    sc->macsrc[4]&0xff, sc->macsrc[5]&0xff);
		printf(" %u.%u.%u.%u,",
		    ((sc->ipsrc >> 0) & 0xff), ((sc->ipsrc >> 8) & 0xff),
		    ((sc->ipsrc >> 16) & 0xff), ((sc->ipsrc >> 24) & 0xff));
		printf(" sig: %d\n", sc->signal);
		sc++;
	}

	return;
}
#endif /* ANCACHE */

int
an_hex2int(char c)
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
an_str2key(char *s, struct an_ltv_key *k)
{
	int			n, i;
	char			*p;

	/* Is this a hex string? */
	if ((s[0] = '0' && (s[1] == 'x' || s[1] == 'X'))) {
		/* Yes, convert to int */
		n = 0;
		p = (char *)&k->key[0];
		for (i = 2; i < strlen(s); i += 2) {
			*p++ = (an_hex2int(s[i]) << 4) + an_hex2int(s[i + 1]);
			n++;
		}
		k->klen = n;
	} else {
		/* No, just copy it in */
		bcopy(s, k->key, strlen(s));
		k->klen = strlen(s);
	}

	return;
}

void
an_setkeys(char *key, int keytype)
{
	struct an_req		areq;
	struct an_ltv_key	*k;

	bzero((char *)&areq, sizeof(areq));
	k = (struct an_ltv_key *)&areq;

	if (strlen(key) > 28)
		err(1, "encryption key must be no more than 18 chars long");

	an_str2key(key, k);

	k->kindex = keytype / 2;

	if (!(k->klen == 0 || k->klen == 5 || k->klen == 13)) {
		err(1, "encryption key must be 0, 5 or 13 bytes long");
	}

	/* default mac and only valid one (from manual) 1:0:0:0:0:0 */
	k->mac[0] = 1;
	k->mac[1] = 0;
	k->mac[2] = 0;
	k->mac[3] = 0;
	k->mac[4] = 0;
	k->mac[5] = 0;

	areq.an_len = sizeof(struct an_ltv_key);
	areq.an_type = (keytype & 1)
	    ? AN_RID_WEP_VOLATILE : AN_RID_WEP_PERMANENT;
	an_setval(&areq);

	return;
}

void
an_readkeyinfo(void)
{
	struct an_req		areq;
	struct an_ltv_key	*k;
	int			i;

	bzero((char *)&areq, sizeof(areq));
	k = (struct an_ltv_key *)&areq;

	printf ("\nWEP Key status:\n");
	areq.an_type = AN_RID_WEP_VOLATILE;	/* read first key */
	for (i = 0; i < 4; i++) {
		areq.an_len = sizeof(struct an_ltv_key);
		an_getval(&areq);
		for (; i < k->kindex && i < 4; i++)
			printf("\tKey %d is unset\n", i);
		if (i < 4) {
			switch (k->klen) {
			case 0:
				printf("\tKey %d is unset\n", i);
				break;
			case 5:
				printf("\tKey %d is set  40 bits\n", i);
				break;
			case 13:
				printf("\tKey %d is set 128 bits\n", i);
				break;
			default:
				printf("\tKey %d has an unknown size %d\n", i, k->klen);
				break;
			}
		}
		areq.an_type = AN_RID_WEP_PERMANENT;	/* read next key */
	}
	k->kindex = 0xffff;
	areq.an_len = sizeof(struct an_ltv_key);
	an_getval(&areq);
	printf("\tThe active transmit key is %d\n", k->mac[0]);

	return;
}

void
an_enable_tx_key(char *arg)
{
	struct an_req		areq;
	struct an_ltv_key	*k;

	bzero((char *)&areq, sizeof(areq));
	k = (struct an_ltv_key *)&areq;

	/*
	 * From a Cisco engineer:  Write the transmit key
	 * to use in the first MAC, index is FFFF
	 */
	k->kindex = 0xFFFF;
	k->klen = 0;

	k->mac[0] = atoi(arg);
	k->mac[1] = 0;
	k->mac[2] = 0;
	k->mac[3] = 0;
	k->mac[4] = 0;
	k->mac[5] = 0;

	areq.an_len = sizeof(struct an_ltv_key);
	areq.an_type = AN_RID_WEP_PERMANENT;
	an_setval(&areq);

	return;
}

int
main(int argc, char *argv[])
{
	int ch;
	int modifier = 0;
	int print_stat = 0;

	/* Grab device name, if one is given. Default to "an0" */
	opterr = 0;
	ch = getopt(argc, argv, "i:");
	if (ch == 'i') {
		strlcpy(ifr.ifr_name, optarg, sizeof(ifr.ifr_name));
	} else {
		if (argc > 1 && argv[1][0] != '-') {
			strlcpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
			optind = 2;
		} else {
			strlcpy(ifr.ifr_name, "an0", sizeof(ifr.ifr_name));
			optind = 1;
		}
	}
	opterr = optreset = 1;

	/* Grab a socket to do our ioctl's */
	getsock();

	while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
		switch(ch) {
		case 'A':
			print_stat |= STAT_DUMPAP;
			break;
		case 'C':
			print_stat |= STAT_DUMPCONFIG;
			break;
		case 'I':
			print_stat |= STAT_DUMPCAPS;
			break;
		case 'K':
			an_setconfig(ACT_SET_KEY_TYPE, optarg);
			break;
		case 'N':
			print_stat |= STAT_DUMPSSID;
			break;
		case 'S':
			print_stat |= STAT_DUMPSTATUS;
			break;
		case 'T':
			print_stat |= STAT_DUMPSTATS;
			break;
		case 'W':
			an_setconfig(ACT_ENABLE_WEP, optarg);
			break;
#ifdef ANCACHE
		case 'Q':
			an_readcache();
			break;
		case 'Z':
			an_zerocache();
			break;
#endif /* ANCACHE */
		case 'a':
			switch (modifier) {
			case 0:
			case 1:
				an_setap(ACT_SET_AP1, optarg);
				break;
			case 2:
				an_setap(ACT_SET_AP2, optarg);
				break;
			case 3:
				an_setap(ACT_SET_AP3, optarg);
				break;
			case 4:
				an_setap(ACT_SET_AP4, optarg);
				break;
			default:
				errx(1, "bad modifier %d", modifier);
			}
			modifier = 0;
			break;
		case 'b':
			an_setconfig(ACT_SET_BEACON_PERIOD, optarg);
			break;
		case 'c':
			an_setconfig(ACT_SET_FREQ, optarg);
			break;
		case 'd':
			switch (modifier) {
			case 0:
				an_setconfig(ACT_SET_DIVERSITY_RX, optarg);
				break;
			case 1:
				an_setconfig(ACT_SET_DIVERSITY_RX, optarg);
				break;
			default:
				errx(1, "must specify RX or TX diversity");
			}
			modifier = 0;
			break;
		case 'e':
			an_enable_tx_key(optarg);
			break;
		case 'f':
			an_setconfig(ACT_SET_FRAG_THRESH, optarg);
			break;
		case 'h':
			usage();
			break;
		case 'j':
			an_setconfig(ACT_SET_NETJOIN, optarg);
			break;
		case 'k':
			an_setkeys(optarg, modifier);
			break;
		case 'l':
			an_setconfig(ACT_SET_MYNAME, optarg);
			break;
		case 'm':
			an_setconfig(ACT_SET_MAC, optarg);
			break;
		case 'n':
			switch (modifier) {
			case 0:
			case 1:
				an_setssid(ACT_SET_SSID1, optarg);
				break;
			case 2:
				an_setssid(ACT_SET_SSID2, optarg);
				break;
			case 3:
				an_setssid(ACT_SET_SSID3, optarg);
				break;
			default:
				errx(1, "bad modifier %d", modifier);
			}
			modifier = 0;
			break;
		case 'o':
			an_setconfig(ACT_SET_OPMODE, optarg);
			break;
		case 'p':
			an_setconfig(ACT_SET_TXPWR, optarg);
			break;
		case 'q':
			an_setconfig(ACT_SET_RTS_RETRYLIM, optarg);
			break;
		case 'r':
			an_setconfig(ACT_SET_RTS_THRESH, optarg);
			break;
		case 's':
			an_setconfig(ACT_SET_PWRSAVE, optarg);
			break;
		case 't':
			an_setspeed(optarg);
			break;
		case 'v':
			modifier = atoi(optarg);
			break;
		case 'w':
			an_setconfig(ACT_SET_WAKE_DURATION, optarg);
			break;
		default:
			usage();
		}
	}

	/*
	 * Show configuration status first. Do not allow
	 * the showing of and setting of options to be done
	 * on the same command line.
	 */
	if (print_stat) {
		if (print_stat & STAT_DUMPAP)
			an_dumpap();
		if (print_stat & STAT_DUMPCONFIG)
			an_dumpconfig();
		if (print_stat & STAT_DUMPCAPS)
			an_dumpcaps();
		if (print_stat & STAT_DUMPSSID)
			an_dumpssid();
		if (print_stat & STAT_DUMPSTATUS)
			an_dumpstatus();
		if (print_stat & STAT_DUMPSTATS)
			an_dumpstats();
	}

	/* Close our socket */
	if (s)
		close(s);

	return (0);
}
