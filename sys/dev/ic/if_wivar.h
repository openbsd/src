/*	$OpenBSD: if_wivar.h,v 1.6 2002/03/28 18:21:06 mickey Exp $	*/

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
 *	From: if_wireg.h,v 1.8.2.2 2001/08/25 00:48:25 nsayer Exp $
 */

#include <dev/ic/if_wi_hostap.h>

struct wi_softc	{
#ifndef __FreeBSD__
	struct device		sc_dev;
#endif	/* !__FreeBSD__ */
	struct arpcom		arpcom;
	struct ifmedia		sc_media;
	bus_space_handle_t	wi_bhandle;
	bus_space_tag_t		wi_btag;
	int			wi_tx_data_id;
	int			wi_tx_mgmt_id;
	int			wi_gone;
	int			wi_if_flags;
	u_int16_t		wi_ptype;
	u_int16_t		wi_portnum;
	u_int16_t		wi_max_data_len;
	u_int16_t		wi_rts_thresh;
	u_int16_t		wi_ap_density;
	u_int16_t		wi_tx_rate;
	u_int16_t		wi_create_ibss;
	u_int16_t		wi_channel;
	u_int16_t		wi_pm_enabled;
	u_int16_t		wi_mor_enabled;
	u_int16_t		wi_max_sleep;
	u_int16_t		wi_authtype;
	u_int16_t		wi_roaming;
	u_int16_t		wi_supprates;

	struct ieee80211_nwid	wi_node_name;
	struct ieee80211_nwid	wi_net_name;
	struct ieee80211_nwid	wi_ibss_name;

	u_int8_t		wi_txbuf[1596];
	int			wi_has_wep;
	int			wi_use_wep;
	int			wi_authmode;
	int			wi_tx_key;
	struct wi_ltv_keys	wi_keys;
	struct wi_counters	wi_stats;
	void			*sc_ih;
	struct timeout		sc_timo;
	int			sc_enabled;
	int			sc_prism2;
	int			sc_prism2_ver;
	int			sc_pci;
	struct wihap_info	wi_hostap_info;
	u_int32_t		wi_icv;
	int			wi_icv_flag;
};

#define WI_PRT_FMT "%s"
#define WI_PRT_ARG(sc)	(sc)->sc_dev.dv_xname

int wi_mgmt_xmit(struct wi_softc *, caddr_t, int);
