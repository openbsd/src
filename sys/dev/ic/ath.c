/*      $OpenBSD: ath.c,v 1.6 2005/01/03 19:59:18 jsg Exp $  */
/*	$NetBSD: ath.c,v 1.37 2004/08/18 21:59:39 dyoung Exp $	*/

/*-
 * Copyright (c) 2002-2004 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/timeout.h>
#include <sys/gpio.h>

#include <machine/endian.h>
#include <machine/bus.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>

#include <dev/gpio/gpiovar.h>

#include <dev/ic/athvar.h>

/* unaligned little endian access */     
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	  (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

#ifdef __FreeBSD__
void	ath_init(void *);
#else
int	ath_init(struct ifnet *);
#endif
int	ath_init1(struct ath_softc *);
int	ath_intr1(struct ath_softc *);
void	ath_stop(struct ifnet *);
void	ath_start(struct ifnet *);
void	ath_reset(struct ath_softc *);
int	ath_media_change(struct ifnet *);
void	ath_watchdog(struct ifnet *);
int	ath_ioctl(struct ifnet *, u_long, caddr_t);
void	ath_fatal_proc(void *, int);
void	ath_rxorn_proc(void *, int);
void	ath_bmiss_proc(void *, int);
u_int   ath_chan2flags(struct ieee80211com *, struct ieee80211_channel *);
void	ath_initkeytable(struct ath_softc *);
void    ath_mcastfilter_accum(caddr_t, u_int32_t (*)[2]);
void    ath_mcastfilter_compute(struct ath_softc *, u_int32_t (*)[2]);
u_int32_t ath_calcrxfilter(struct ath_softc *);
void	ath_mode_init(struct ath_softc *);
int	ath_beacon_alloc(struct ath_softc *, struct ieee80211_node *);
void	ath_beacon_proc(struct ath_softc *, int);
void	ath_beacon_free(struct ath_softc *);
void	ath_beacon_config(struct ath_softc *);
int	ath_desc_alloc(struct ath_softc *);
void	ath_desc_free(struct ath_softc *);
struct ieee80211_node *ath_node_alloc(struct ieee80211com *);
struct mbuf *ath_getmbuf(int, int, u_int);
void	ath_node_free(struct ieee80211com *, struct ieee80211_node *);
void	ath_node_copy(struct ieee80211com *,
			struct ieee80211_node *, const struct ieee80211_node *);
u_int8_t	ath_node_getrssi(struct ieee80211com *,
			struct ieee80211_node *);
int	ath_rxbuf_init(struct ath_softc *, struct ath_buf *);
void	ath_rx_proc(void *, int);
int	ath_tx_start(struct ath_softc *, struct ieee80211_node *,
			     struct ath_buf *, struct mbuf *);
void	ath_tx_proc(void *, int);
int	ath_chan_set(struct ath_softc *, struct ieee80211_channel *);
void	ath_draintxq(struct ath_softc *);
void	ath_stoprecv(struct ath_softc *);
int	ath_startrecv(struct ath_softc *);
void	ath_next_scan(void *);
void	ath_calibrate(void *);
HAL_LED_STATE ath_state_to_led(enum ieee80211_state);
int	ath_newstate(struct ieee80211com *, enum ieee80211_state, int);
void	ath_newassoc(struct ieee80211com *,
			struct ieee80211_node *, int);
int	ath_getchannels(struct ath_softc *, u_int cc, HAL_BOOL outdoor,
			HAL_BOOL xchanmode);

int	ath_rate_setup(struct ath_softc *sc, u_int mode);
void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);
void	ath_rate_ctl_reset(struct ath_softc *, enum ieee80211_state);
void	ath_rate_ctl(void *, struct ieee80211_node *);
void	ath_recv_mgmt(struct ieee80211com *, struct mbuf *,
			struct ieee80211_node *, int, int, u_int32_t);

int	ath_enable(struct ath_softc *);
void	ath_disable(struct ath_softc *);
void	ath_power(int, void *);

#if NGPIO > 0
int	ath_gpio_attach(struct ath_softc *);
int	ath_gpio_pin_read(void *, int);
void	ath_gpio_pin_write(void *, int, int);
void	ath_gpio_pin_ctl(void *, int, int);
#endif

#ifdef __FreeBSD__
SYSCTL_DECL(_hw_ath);
/* XXX validate sysctl values */
SYSCTL_INT(_hw_ath, OID_AUTO, dwell, CTLFLAG_RW, &ath_dwelltime,
	    0, "channel dwell time (ms) for AP/station scanning");
SYSCTL_INT(_hw_ath, OID_AUTO, calibrate, CTLFLAG_RW, &ath_calinterval,
	    0, "chip calibration interval (secs)");
SYSCTL_INT(_hw_ath, OID_AUTO, outdoor, CTLFLAG_RD, &ath_outdoor,
	    0, "enable/disable outdoor operation");
TUNABLE_INT("hw.ath.outdoor", &ath_outdoor);
SYSCTL_INT(_hw_ath, OID_AUTO, countrycode, CTLFLAG_RD, &ath_countrycode,
	    0, "country code");
TUNABLE_INT("hw.ath.countrycode", &ath_countrycode);
SYSCTL_INT(_hw_ath, OID_AUTO, regdomain, CTLFLAG_RD, &ath_regdomain,
	    0, "regulatory domain");
#endif /* __FreeBSD__ */

int ath_dwelltime_nodenum, ath_calibrate_nodenum, ath_outdoor_nodenum,
           ath_countrycode_nodenum, ath_regdomain_nodenum, ath_debug_nodenum;

static	int ath_dwelltime = 200;		/* 5 channels/second */
static	int ath_calinterval = 30;		/* calibrate every 30 secs */
static	int ath_outdoor = AH_TRUE;		/* outdoor operation */
static	int ath_xchanmode = AH_TRUE;		/* enable extended channels */
static	int ath_countrycode = CTRY_DEFAULT;	/* country code */
static	int ath_regdomain = DMN_DEFAULT;	/* regulatory domain */

struct cfdriver ath_cd = {
	NULL, "ath", DV_IFNET
};

#ifdef AR_DEBUG
enum {
	ATH_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	ATH_DEBUG_XMIT_DESC	= 0x00000002,	/* xmit descriptors */
	ATH_DEBUG_RECV		= 0x00000004,	/* basic recv operation */
	ATH_DEBUG_RECV_DESC	= 0x00000008,	/* recv descriptors */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_RESET		= 0x00000020,	/* reset processing */
	ATH_DEBUG_MODE		= 0x00000040,	/* mode init/setup */
	ATH_DEBUG_BEACON 	= 0x00000080,	/* beacon handling */
	ATH_DEBUG_WATCHDOG 	= 0x00000100,	/* watchdog timeout */
	ATH_DEBUG_INTR		= 0x00001000,	/* ISR */
	ATH_DEBUG_TX_PROC	= 0x00002000,	/* tx ISR proc */
	ATH_DEBUG_RX_PROC	= 0x00004000,	/* rx ISR proc */
	ATH_DEBUG_BEACON_PROC	= 0x00008000,	/* beacon ISR proc */
	ATH_DEBUG_CALIBRATE	= 0x00010000,	/* periodic calibration */
	ATH_DEBUG_ANY		= 0xffffffff
};
int	ath_debug = ATH_DEBUG_ANY;
#ifdef __FreeBSD__
SYSCTL_INT(_hw_ath, OID_AUTO, debug, CTLFLAG_RW, &ath_debug,
	    0, "control debugging printfs");
TUNABLE_INT("hw.ath.debug", &ath_debug);
#endif /* __FreeBSD__ */
#define	IFF_DUMPPKTS(_ifp, _m) \
	((ath_debug & _m) || \
	    ((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
static	void ath_printrxbuf(struct ath_buf *bf, int);
static	void ath_printtxbuf(struct ath_buf *bf, int);
#define	DPRINTF(_m,X)	if (ath_debug & (_m)) printf X
#else
#define	IFF_DUMPPKTS(_ifp, _m) \
	(((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(_m, X)
#endif

#if 0
int
ath_activate(struct device *self, enum devact act)
{
	struct ath_softc *sc = (struct ath_softc *)self;
	int rv = 0, s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ic.ic_if);
		break;
	}
	splx(s);
	return rv;
}
#endif

int
ath_enable(struct ath_softc *sc)
{
	if (ATH_IS_ENABLED(sc) == 0) {
		if (sc->sc_enable != NULL && (*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
				sc->sc_dev.dv_xname);
			return (EIO);
		}
		sc->sc_flags |= ATH_ENABLED;
	}
	return (0);
}

void
ath_disable(struct ath_softc *sc)
{
	if (!ATH_IS_ENABLED(sc))
		return;
	if (sc->sc_disable != NULL)
		(*sc->sc_disable)(sc);
	sc->sc_flags &= ~ATH_ENABLED;
}

#if 0
int
sysctl_ath_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	DPRINTF(ATH_DEBUG_ANY, ("%s: t = %d, nodenum = %d, rnodenum = %d\n",
	    __func__, t, node.sysctl_num, rnode->sysctl_num));

	if (node.sysctl_num == ath_dwelltime_nodenum) {
		if (t <= 0)
			return (EINVAL);
	} else if (node.sysctl_num == ath_calibrate_nodenum) {
		if (t <= 0)
			return (EINVAL);
#ifdef AR_DEBUG
	} else if (node.sysctl_num == ath_debug_nodenum) {
		if (t < 0 || t > 2)
			return (EINVAL);
#endif /* AR_DEBUG */
	} else
		return (EINVAL);

	*(int*)rnode->sysctl_data = t;

	return (0);
}

/*
 * Setup sysctl(3) MIB, ath.*.
 *
 * TBD condition CTLFLAG_PERMANENT on being an LKM or not
 */
SYSCTL_SETUP(sysctl_ath, "sysctl ath subtree setup")
{
	int rc, ath_node_num;
	struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "hw", NULL,
	    NULL, 0, NULL, 0, CTL_HW, CTL_EOL)) != 0)
		goto err;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "ath",
	    SYSCTL_DESCR("ath information and options"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	ath_node_num = node->sysctl_num;

	/* channel dwell time (ms) for AP/station scanning */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
	    CTLTYPE_INT, "dwell",
	    SYSCTL_DESCR("Channel dwell time (ms) for AP/station scanning"),
	    sysctl_ath_verify, 0, &ath_dwelltime,
	    0, CTL_HW, ath_node_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	ath_dwelltime_nodenum = node->sysctl_num;

	/* chip calibration interval (secs) */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
	    CTLTYPE_INT, "calibrate",
	    SYSCTL_DESCR("Chip calibration interval (secs)"), sysctl_ath_verify,
	    0, &ath_calinterval, 0, CTL_HW,
	    ath_node_num, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	ath_calibrate_nodenum = node->sysctl_num;

	/* enable/disable outdoor operation */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "outdoor", SYSCTL_DESCR("Enable/disable outdoor operation"),
	    NULL, 0, &ath_outdoor, 0,
	    CTL_HW, ath_node_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	ath_outdoor_nodenum = node->sysctl_num;

	/* country code */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "countrycode", SYSCTL_DESCR("Country code"),
	    NULL, 0, &ath_countrycode, 0,
	    CTL_HW, ath_node_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	ath_countrycode_nodenum = node->sysctl_num;

	/* regulatory domain */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READONLY, CTLTYPE_INT,
	    "regdomain", SYSCTL_DESCR("Regulatory domain"),
	    NULL, 0, &ath_regdomain, 0,
	    CTL_HW, ath_node_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	ath_regdomain_nodenum = node->sysctl_num;

#ifdef AR_DEBUG

	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable/disable ath debugging output"),
	    sysctl_ath_verify, 0, &ath_debug, 0,
	    CTL_HW, ath_node_num, CTL_CREATE,
	    CTL_EOL)) != 0)
		goto err;

	ath_debug_nodenum = node->sysctl_num;

#endif /* AR_DEBUG */
	return;
err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}
#endif /* 0 */

int
ath_attach(u_int16_t devid, struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah;
	HAL_STATUS status;
	HAL_TXQ_INFO qinfo;
	int error = 0, i = 0;

	DPRINTF(ATH_DEBUG_ANY, ("%s: devid 0x%x\n", __func__, devid));

#ifdef __FreeBSD__
	/* set these up early for if_printf use */
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));
#else
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
#endif

	ah = ath_hal_attach(devid, sc, sc->sc_st, sc->sc_sh, &status);
	if (ah == NULL) {
		if_printf(ifp, "unable to attach hardware; HAL status %d\n",
			status);
		error = ENXIO;
		goto bad;
	}
	if (ah->ah_abi != HAL_ABI_VERSION) {
		if_printf(ifp, "HAL ABI mismatch detected (0x%x != 0x%x)\n",
			ah->ah_abi, HAL_ABI_VERSION);
		error = ENXIO;
		goto bad;
	}
	if_printf(ifp, "mac %d.%d phy %d.%d",
		ah->ah_macVersion, ah->ah_macRev,
		ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
	printf(" radio %d.%d", ah->ah_analog5GhzRev >> 4,
	    ah->ah_analog5GhzRev & 0xf);
	if (ah->ah_analog2GhzRev != 0)
		printf(" %d.%d", ah->ah_analog2GhzRev >> 4,
		    ah->ah_analog2GhzRev & 0xf);
#define ah_mode(_m)	{						\
		if (i++)						\
                        printf("/%s", #_m);				\
                else							\
                        printf(", 802.11%s", #_m);			\
}									\
	if (ah->ah_modes & HAL_MODE_11A)
		ah_mode(a);
	if (ah->ah_modes & HAL_MODE_11B)
		ah_mode(b);
	if (ah->ah_modes & HAL_MODE_11G)
		ah_mode(g);
#undef ah_mode	

	sc->sc_ah = ah;
	sc->sc_invalid = 0;	/* ready to go, enable interrupt handling */

	/*
	 * Collect the channel list using the default country
	 * code and including outdoor channels.  The 802.11 layer
	 * is resposible for filtering this list based on settings
	 * like the phy mode.
	 */
	error = ath_getchannels(sc, ath_countrycode, ath_outdoor,
	    ath_xchanmode);
	if (error != 0)
		goto bad;
	/*
	 * Copy these back; they are set as a side effect
	 * of constructing the channel list.
	 */
	ath_hal_getregdomain(ah, &ath_regdomain);
	ath_hal_getcountrycode(ah, &ath_countrycode);

	/*
	 * Setup rate tables for all potential media types.
	 */
	ath_rate_setup(sc, IEEE80211_MODE_11A);
	ath_rate_setup(sc, IEEE80211_MODE_11B);
	ath_rate_setup(sc, IEEE80211_MODE_11G);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO);

	error = ath_desc_alloc(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}
	timeout_set(&sc->sc_scan_to, ath_next_scan, sc);
	timeout_set(&sc->sc_cal_to, ath_calibrate, sc);

#ifdef __FreeBSD__
	ATH_TXBUF_LOCK_INIT(sc);
	ATH_TXQ_LOCK_INIT(sc);
#endif

	ATH_TASK_INIT(&sc->sc_txtask, ath_tx_proc, sc);
	ATH_TASK_INIT(&sc->sc_rxtask, ath_rx_proc, sc);
	ATH_TASK_INIT(&sc->sc_rxorntask, ath_rxorn_proc, sc);
	ATH_TASK_INIT(&sc->sc_fataltask, ath_fatal_proc, sc);
	ATH_TASK_INIT(&sc->sc_bmisstask, ath_bmiss_proc, sc);

	/*
	 * For now just pre-allocate one data queue and one
	 * beacon queue.  Note that the HAL handles resetting
	 * them at the needed time.  Eventually we'll want to
	 * allocate more tx queues for splitting management
	 * frames and for QOS support.
	 */
	sc->sc_bhalq = ath_hal_setuptxqueue(ah,HAL_TX_QUEUE_BEACON,NULL);
	if (sc->sc_bhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a beacon xmit queue!\n");
		goto bad2;
	}

	memset(&qinfo, 0, sizeof(qinfo));
	qinfo.tqi_subtype = HAL_WME_AC_BE;
	sc->sc_txhalq = ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_DATA, &qinfo);
	if (sc->sc_txhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a data xmit queue!\n");
		goto bad2;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST | IFF_NOTRAILERS;
	ifp->if_start = ath_start;
	ifp->if_watchdog = ath_watchdog;
	ifp->if_ioctl = ath_ioctl;
#ifndef __OpenBSD__
	ifp->if_init = ath_init;
#endif
#ifdef __FreeBSD__
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#else
#if 0
	ifp->if_stop = ath_stop;		/* XXX */
#endif
	IFQ_SET_READY(&ifp->if_snd);
#endif

	ic->ic_softc = sc;
	ic->ic_newassoc = ath_newassoc;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_WEP		/* wep supported */
	        | IEEE80211_C_PMGT
		| IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		;

	/* get mac address from hardware */
	ath_hal_getmac(ah, ic->ic_myaddr);

	if_attach(ifp);

	/* call MI attach routine. */
	ieee80211_ifattach(ifp);

	/* override default methods */
	ic->ic_node_alloc = ath_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = ath_node_free;
	sc->sc_node_copy = ic->ic_node_copy;
	ic->ic_node_copy = ath_node_copy;
	ic->ic_node_getrssi = ath_node_getrssi;
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ath_newstate;
	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = ath_recv_mgmt;
	memset(&sc->sc_broadcast_addr, 0xFF, IEEE80211_ADDR_LEN);

	/* complete initialization */
	ieee80211_media_init(ifp, ath_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64);
#endif
	/*
	 * Initialize constant fields.
	 * XXX make header lengths a multiple of 32-bits so subsequent
	 *     headers are properly aligned; this is a kludge to keep
	 *     certain applications happy.
	 *
	 * NB: the channel is setup each time we transition to the
	 *     RUN state to avoid filling it in for each frame.
	 */
	sc->sc_tx_th_len = roundup(sizeof(sc->sc_tx_th), sizeof(u_int32_t));
	sc->sc_tx_th.wt_ihdr.it_len = htole16(sc->sc_tx_th_len);
	sc->sc_tx_th.wt_ihdr.it_present = htole32(ATH_TX_RADIOTAP_PRESENT);

	sc->sc_rx_th_len = roundup(sizeof(sc->sc_rx_th), sizeof(u_int32_t));
	sc->sc_rx_th.wr_ihdr.it_len = htole16(sc->sc_rx_th_len);
	sc->sc_rx_th.wr_ihdr.it_present = htole32(ATH_RX_RADIOTAP_PRESENT);

	sc->sc_flags |= ATH_ATTACHED;
	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	sc->sc_sdhook = shutdownhook_establish(ath_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
			sc->sc_dev.dv_xname);
	sc->sc_powerhook = powerhook_establish(ath_power, sc);
	if (sc->sc_powerhook == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
			sc->sc_dev.dv_xname);

	printf(", %s, address %s\n", ieee80211_regdomain2name(ath_regdomain),
	       ether_sprintf(ic->ic_myaddr));

#if NGPIO > 0
	if (ath_gpio_attach(sc) == 0)
		sc->sc_flags |= ATH_GPIO;
#endif

	return 0;
bad2:
	ath_desc_free(sc);
bad:
	if (ah)
		ath_hal_detach(ah);
	sc->sc_invalid = 1;
	return error;
}

int
ath_detach(struct ath_softc *sc, int flags)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	config_detach_children(&sc->sc_dev, flags);

	if ((sc->sc_flags & ATH_ATTACHED) == 0)
		return (0);

	DPRINTF(ATH_DEBUG_ANY, ("%s: if_flags %x\n", __func__, ifp->if_flags));
	
	s = splnet();
	ath_stop(ifp);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ath_desc_free(sc);
	ath_hal_detach(sc->sc_ah);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	splx(s);
	powerhook_disestablish(sc->sc_powerhook);
	shutdownhook_disestablish(sc->sc_sdhook);
#ifdef __FreeBSD__

	ATH_TXBUF_LOCK_DESTROY(sc);
	ATH_TXQ_LOCK_DESTROY(sc);

#endif /* __FreeBSD__ */

	return 0;
}

void
ath_power(int why, void *arg)
{
	struct ath_softc *sc = arg;
	int s;

	DPRINTF(ATH_DEBUG_ANY, ("ath_power(%d)\n", why));

	s = splnet();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		ath_suspend(sc, why);
		break;
	case PWR_RESUME:
		ath_resume(sc, why);
		break;
#if !defined(__OpenBSD__)
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
#endif
	}
	splx(s);
}

void
ath_suspend(struct ath_softc *sc, int why)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(ATH_DEBUG_ANY, ("%s: if_flags %x\n", __func__, ifp->if_flags));

	ath_stop(ifp);
	if (sc->sc_power != NULL)
		(*sc->sc_power)(sc, why);
}

void
ath_resume(struct ath_softc *sc, int why)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(ATH_DEBUG_ANY, ("%s: if_flags %x\n", __func__, ifp->if_flags));

	if (ifp->if_flags & IFF_UP) {
		ath_init(ifp);
#if 0
		(void)ath_intr(sc);
#endif
		if (sc->sc_power != NULL)
			(*sc->sc_power)(sc, why);
		if (ifp->if_flags & IFF_RUNNING)
			ath_start(ifp);
	}
}

void
ath_shutdown(void *arg)
{
	struct ath_softc *sc = arg;

	ath_stop(&sc->sc_ic.ic_if);
}

int
ath_intr(void *arg)
{
	return ath_intr1((struct ath_softc *)arg);
}

int
ath_intr1(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status;

	if (sc->sc_invalid) {
		/*
		 * The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.
		 */
		DPRINTF(ATH_DEBUG_ANY, ("%s: invalid; ignored\n", __func__));
		return 0;
	}
	if (!ath_hal_intrpend(ah))		/* shared irq, not for us */
		return 0;
	if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP)) {
		DPRINTF(ATH_DEBUG_ANY, ("%s: if_flags 0x%x\n",
			__func__, ifp->if_flags));
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		return 1; /* XXX */
	}
	ath_hal_getisr(ah, &status);		/* NB: clears ISR too */
	DPRINTF(ATH_DEBUG_INTR, ("%s: status 0x%x\n", __func__, status));
	status &= sc->sc_imask;			/* discard unasked for bits */
	if (status & HAL_INT_FATAL) {
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		ATH_TASK_RUN_OR_ENQUEUE(&sc->sc_fataltask);
	} else if (status & HAL_INT_RXORN) {
		sc->sc_stats.ast_rxorn++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		ATH_TASK_RUN_OR_ENQUEUE(&sc->sc_rxorntask);
	} else {
		if (status & HAL_INT_RXEOL) {
			/*
			 * NB: the hardware should re-read the link when
			 *     RXE bit is written, but it doesn't work at
			 *     least on older hardware revs.
			 */
			sc->sc_stats.ast_rxeol++;
			sc->sc_rxlink = NULL;
		}
		if (status & HAL_INT_TXURN) {
			sc->sc_stats.ast_txurn++;
			/* bump tx trigger level */
			ath_hal_updatetxtriglevel(ah, AH_TRUE);
		}
		if (status & HAL_INT_RX)
			ATH_TASK_RUN_OR_ENQUEUE(&sc->sc_rxtask);
		if (status & HAL_INT_TX)
			ATH_TASK_RUN_OR_ENQUEUE(&sc->sc_txtask);
		if (status & HAL_INT_SWBA) {
			/*
			 * Handle beacon transmission directly; deferring
			 * this is too slow to meet timing constraints
			 * under load.
			 */
			ath_beacon_proc(sc, 0);
		}
		if (status & HAL_INT_BMISS) {
			sc->sc_stats.ast_bmiss++;
			ATH_TASK_RUN_OR_ENQUEUE(&sc->sc_bmisstask);
		}
	}
	return 1;
}

void
ath_fatal_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if_printf(ifp, "hardware error; resetting\n");
	ath_reset(sc);
}

void
ath_rxorn_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if_printf(ifp, "rx FIFO overrun; resetting\n");
	ath_reset(sc);
}

void
ath_bmiss_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(ATH_DEBUG_ANY, ("%s: pending %u\n", __func__, pending));
	if (ic->ic_opmode != IEEE80211_M_STA)
		return;
	if (ic->ic_state == IEEE80211_S_RUN) {
		/*
		 * Rather than go directly to scan state, try to
		 * reassociate first.  If that fails then the state
		 * machine will drop us into scanning after timing
		 * out waiting for a probe response.
		 */
		ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
	}
}

u_int
ath_chan2flags(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	enum ieee80211_phymode mode = ieee80211_chan2mode(ic, chan);

	switch (mode) {
	case IEEE80211_MODE_AUTO:
		return 0;
	case IEEE80211_MODE_11A:
		return CHANNEL_A;
	case IEEE80211_MODE_11B:
		return CHANNEL_B;
	case IEEE80211_MODE_11G:
		return CHANNEL_PUREG;
	case IEEE80211_MODE_TURBO:
		return CHANNEL_T;
	default:
		panic("%s: unsupported mode %d\n", __func__, mode);
		return 0;
	}
}

int
ath_init(struct ifnet *ifp)
{
	return ath_init1((struct ath_softc *)ifp->if_softc);
}

int
ath_init1(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	enum ieee80211_phymode mode;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	HAL_CHANNEL hchan;
	int error = 0, s;

	DPRINTF(ATH_DEBUG_ANY, ("%s: if_flags 0x%x\n",
		__func__, ifp->if_flags));

	if ((error = ath_enable(sc)) != 0)
		return error;

	s = splnet();
	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	ath_stop(ifp);

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	hchan.channel = ic->ic_ibss_chan->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, ic->ic_ibss_chan);
	if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_FALSE, &status)) {
		if_printf(ifp, "unable to reset hardware; hal status %u\n",
			status);
		error = EIO;
		goto done;
	}

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	if (ic->ic_flags & IEEE80211_F_WEPON)
		ath_initkeytable(sc);
	if ((error = ath_startrecv(sc)) != 0) {
		if_printf(ifp, "unable to start recv logic\n");
		goto done;
	}

	/*
	 * Enable interrupts.
	 */
	sc->sc_imask = HAL_INT_RX | HAL_INT_TX
		  | HAL_INT_RXEOL | HAL_INT_RXORN
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;
	ath_hal_intrset(ah, sc->sc_imask);

	ifp->if_flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	/*
	 * The hardware should be ready to go now so it's safe
	 * to kick the 802.11 state machine as it's likely to
	 * immediately call back to us to send mgmt frames.
	 */
	ni = ic->ic_bss;
	ni->ni_chan = ic->ic_ibss_chan;
	mode = ieee80211_chan2mode(ic, ni->ni_chan);
	if (mode != sc->sc_curmode)
		ath_setcurmode(sc, mode);
	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
done:
	splx(s);
	return error;
}

void
ath_stop(struct ifnet *ifp)
{
	struct ieee80211com *ic = (struct ieee80211com *) ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	int s;

	DPRINTF(ATH_DEBUG_ANY, ("%s: invalid %u if_flags 0x%x\n",
		__func__, sc->sc_invalid, ifp->if_flags));

	s = splnet();
	if (ifp->if_flags & IFF_RUNNING) {
		/*
		 * Shutdown the hardware and driver:
		 *    disable interrupts
		 *    turn off timers
		 *    clear transmit machinery
		 *    clear receive machinery
		 *    drain and release tx queues
		 *    reclaim beacon resources
		 *    reset 802.11 state machine
		 *    power down hardware
		 *
		 * Note that some of this work is not possible if the
		 * hardware is gone (invalid).
		 */
		ifp->if_flags &= ~IFF_RUNNING;
		ifp->if_timer = 0;
		if (!sc->sc_invalid)
			ath_hal_intrset(ah, 0);
		ath_draintxq(sc);
		if (!sc->sc_invalid)
			ath_stoprecv(sc);
		else
			sc->sc_rxlink = NULL;
#ifdef __FreeBSD__
		IF_DRAIN(&ifp->if_snd);
#else
		IF_PURGE(&ifp->if_snd);
#endif
		ath_beacon_free(sc);
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		if (!sc->sc_invalid) {
			ath_hal_setpower(ah, HAL_PM_FULL_SLEEP, 0);
		}
		ath_disable(sc);
	}
	splx(s);
}

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from errors rx overrun
 * and to reset the hardware when rf gain settings must be reset.
 */
void
ath_reset(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_channel *c;
	HAL_STATUS status;
	HAL_CHANNEL hchan;

	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	hchan.channel = c->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, c);

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	ath_stoprecv(sc);		/* stop recv side */
	/* NB: indicate channel change so we do a full reset */
	if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status))
		if_printf(ifp, "%s: unable to reset hardware; hal status %u\n",
			__func__, status);
	ath_hal_intrset(ah, sc->sc_imask);
	if (ath_startrecv(sc) != 0)	/* restart recv */
		if_printf(ifp, "%s: unable to start recv logic\n", __func__);
	ath_start(ifp);			/* restart xmit */
	if (ic->ic_state == IEEE80211_S_RUN)
		ath_beacon_config(sc);	/* restart beacons */
}

void
ath_start(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	int s;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING || sc->sc_invalid)
		return;
	for (;;) {
		/*
		 * Grab a TX buffer and associated resources.
		 */
		s = splnet();
		bf = TAILQ_FIRST(&sc->sc_txbuf);
		if (bf != NULL)
			TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
		splx(s);
		if (bf == NULL) {
			DPRINTF(ATH_DEBUG_ANY, ("%s: out of xmit buffers\n",
				__func__));
			sc->sc_stats.ast_tx_qstop++;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/*
		 * Poll the management queue for frames; they
		 * have priority over normal data frames.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m == NULL) {
			/*
			 * No data frames go out unless we're associated.
			 */
			if (ic->ic_state != IEEE80211_S_RUN) {
				DPRINTF(ATH_DEBUG_ANY,
					("%s: ignore data packet, state %u\n",
					__func__, ic->ic_state));
				sc->sc_stats.ast_tx_discard++;
				s = splnet();
				TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				splx(s);
				break;
			}
			IF_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL) {
				s = splnet();
				TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				splx(s);
				break;
			}
			ifp->if_opackets++;

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m);
#endif

			/*
			 * Encapsulate the packet in prep for transmission.
			 */
			m = ieee80211_encap(ifp, m, &ni);
			if (m == NULL) {
				DPRINTF(ATH_DEBUG_ANY,
					("%s: encapsulation failure\n",
					__func__));
				sc->sc_stats.ast_tx_encap++;
				goto bad;
			}
			wh = mtod(m, struct ieee80211_frame *);
		} else {
			/*
			 * Hack!  The referenced node pointer is in the
			 * rcvif field of the packet header.  This is
			 * placed there by ieee80211_mgmt_output because
			 * we need to hold the reference with the frame
			 * and there's no other way (other than packet
			 * tags which we consider too expensive to use)
			 * to pass it along.
			 */
			ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;

			wh = mtod(m, struct ieee80211_frame *);
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			    IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				/* fill time stamp */
				u_int64_t tsf;
				u_int32_t *tstamp;

				tsf = ath_hal_gettsf64(ah);
				/* XXX: adjust 100us delay to xmit */
				tsf += 100;
				tstamp = (u_int32_t *)&wh[1];
				tstamp[0] = htole32(tsf & 0xffffffff);
				tstamp[1] = htole32(tsf >> 32);
			}
			sc->sc_stats.ast_tx_mgmt++;
		}

		if (ath_tx_start(sc, ni, bf, m)) {
	bad:
			s = splnet();
			TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
			splx(s);
			ifp->if_oerrors++;
			if (ni != NULL && ni != ic->ic_bss)
			          ieee80211_free_node(ic, ni);
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

int
ath_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
		    (IFF_RUNNING|IFF_UP))
			ath_init(ifp);		/* XXX lose error */
		error = 0;
	}
	return error;
}

void
ath_watchdog(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;
	if ((ifp->if_flags & IFF_RUNNING) == 0 || sc->sc_invalid)
		return;
	if (sc->sc_tx_timer) {
		if (--sc->sc_tx_timer == 0) {
			if_printf(ifp, "device timeout\n");
			ath_reset(sc);
			ifp->if_oerrors++;
			sc->sc_stats.ast_watchdog++;
			return;
		}
		ifp->if_timer = 1;
	}
	if (ic->ic_fixed_rate == -1) {
		/*
		 * Run the rate control algorithm if we're not
		 * locked at a fixed rate.
		 */
		if (ic->ic_opmode == IEEE80211_M_STA)
			ath_rate_ctl(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(ic, ath_rate_ctl, sc);
	}
	ieee80211_watchdog(ifp);
}

int
ath_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
   	struct ifaddr *ifa = (struct ifaddr *)data;
	int error = 0, s;

	s = splnet();
	switch (cmd) {
        case SIOCSIFMTU:
                if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
                        error = EINVAL;
                } else if (ifp->if_mtu != ifr->ifr_mtu) {
                        ifp->if_mtu = ifr->ifr_mtu;
                }
                break;
        case SIOCSIFADDR:
                ifp->if_flags |= IFF_UP;
#ifdef INET
                if (ifa->ifa_addr->sa_family == AF_INET) {
                        arp_ifinit(&ic->ic_ac, ifa);
                }
#endif  /* INET */
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING) {
				/*
				 * To avoid rescanning another access point,
				 * do not call ath_init() here.  Instead,
				 * only reflect promisc mode settings.
				 */
				ath_mode_init(sc);
			} else {
				/*
				 * Beware of being called during detach to
				 * reset promiscuous mode.  In that case we
				 * will still be marked UP but not RUNNING.
				 * However trying to re-init the interface
				 * is the wrong thing to do as we've already
				 * torn down much of our state.  There's
				 * probably a better way to deal with this.
				 */
				if (!sc->sc_invalid)
					ath_init(ifp);	/* XXX lose error */
			}
		} else
			ath_stop(ifp);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef __FreeBSD__
		/*
		 * The upper layer has already installed/removed
		 * the multicast address(es), just recalculate the
		 * multicast filter for the card.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			ath_mode_init(sc);
#endif
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ic.ic_ac) :
		    ether_delmulti(ifr, &sc->sc_ic.ic_ac);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				ath_mode_init(sc);
			error = 0;
		}
		break;
	case SIOCGATHSTATS:
		error = copyout(&sc->sc_stats,
				ifr->ifr_data, sizeof (sc->sc_stats));
		break;
	case SIOCGATHDIAG: {
#if 0	/* XXX punt */
		struct ath_diag *ad = (struct ath_diag *)data;
		struct ath_hal *ah = sc->sc_ah;
		void *data;
		u_int size;

		if (ath_hal_getdiagstate(ah, ad->ad_id, &data, &size)) {
			if (size < ad->ad_size)
				ad->ad_size = size;
			if (data)
				error = copyout(data, ad->ad_data, ad->ad_size);
		} else
			error = EINVAL;
#else
		error = EINVAL;
#endif
		break;
	}
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
			    (IFF_RUNNING|IFF_UP))
				ath_init(ifp);		/* XXX lose error */
			error = 0;
		}
		break;
	}
	splx(s);
	return error;
}

/*
 * Fill the hardware key cache with key entries.
 */
void
ath_initkeytable(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	int i;

	/* XXX maybe should reset all keys when !WEPON */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		struct ieee80211_wepkey *k = &ic->ic_nw_keys[i];
		if (k->wk_len == 0)
			ath_hal_keyreset(ah, i);
		else {
			HAL_KEYVAL hk;

			memset(&hk, 0, sizeof(hk));
			hk.wk_len = k->wk_len;
			memcpy(hk.wk_key, k->wk_key, k->wk_len);
			/* XXX return value */
			ath_hal_keyset(ah, i, &hk);
		}
	}
}

void
ath_mcastfilter_accum(caddr_t dl, u_int32_t (*mfilt)[2])
{
	u_int32_t val;
	u_int8_t pos;

	val = LE_READ_4(dl + 0);
	pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
	val = LE_READ_4(dl + 3);
	pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
	pos &= 0x3f;
	(*mfilt)[pos / 32] |= (1 << (pos % 32));
}

void
ath_mcastfilter_compute(struct ath_softc *sc, u_int32_t (*mfilt)[2])
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	struct ether_multi *enm;
	struct ether_multistep estep;

	ETHER_FIRST_MULTI(estep, &sc->sc_ic.ic_ac, enm);
	while (enm != NULL) {   
		/* XXX Punt on ranges. */
		if (!IEEE80211_ADDR_EQ(enm->enm_addrlo, enm->enm_addrhi)) {
			(*mfilt)[0] = (*mfilt)[1] = ~((u_int32_t)0);
			ifp->if_flags |= IFF_ALLMULTI;
			return;
		}
		ath_mcastfilter_accum(enm->enm_addrlo, mfilt);
		ETHER_NEXT_MULTI(estep, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
}

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o maintain current state of phy error reception
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, or monitor modes
 * o enable promiscuous mode according to the interface state
 * o accept beacons:
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when scanning
 */
u_int32_t
ath_calcrxfilter(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &ic->ic_if;
	u_int32_t rfilt;

	rfilt = (ath_hal_getrxfilter(ah) & HAL_RX_FILTER_PHYERR)
	      | HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	if (ic->ic_opmode != IEEE80211_M_AHDEMO)
		rfilt |= HAL_RX_FILTER_BEACON;
	if (ifp->if_flags & IFF_PROMISC)
		rfilt |= HAL_RX_FILTER_PROM;
	return rfilt;
}

void
ath_mode_init(struct ath_softc *sc)
{
#ifdef __FreeBSD__
	struct ieee80211com *ic = &sc->sc_ic;
#endif
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t rfilt, mfilt[2];

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(ah, rfilt);

	/* configure operational mode */
	ath_hal_setopmode(ah);

	/* calculate and install multicast filter */
#ifdef __FreeBSD__
	if ((ic->ic_if.if_flags & IFF_ALLMULTI) == 0) {
		mfilt[0] = mfilt[1] = 0;
		ath_mcastfilter_compute(sc, &mfilt);
	} else {
		mfilt[0] = mfilt[1] = ~0;
	}
#endif
	mfilt[0] = mfilt[1] = 0;
	ath_mcastfilter_compute(sc, &mfilt);
	ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);
	DPRINTF(ATH_DEBUG_MODE, ("%s: RX filter 0x%x, MC filter %08x:%08x\n",
		__func__, rfilt, mfilt[0], mfilt[1]));
}

#ifdef __FreeBSD__
void
ath_mbuf_load_cb(void *arg, bus_dma_segment_t *seg, int nseg, bus_size_t mapsize, int error)
{
	struct ath_buf *bf = arg;

	KASSERT(nseg <= ATH_MAX_SCATTER,
		("ath_mbuf_load_cb: too many DMA segments %u", nseg));
	bf->bf_mapsize = mapsize;
	bf->bf_nseg = nseg;
	bcopy(seg, bf->bf_segs, nseg * sizeof (seg[0]));
}
#endif /* __FreeBSD__ */

struct mbuf *
ath_getmbuf(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	KASSERT(pktlen <= MCLBYTES, ("802.11 packet too large: %u", pktlen));
#ifdef __FreeBSD__
	if (pktlen <= MHLEN)
		MGETHDR(m, flags, type);
	else
		m = m_getcl(flags, type, M_PKTHDR);
#else
	MGETHDR(m, flags, type);
	if (m != NULL && pktlen > MHLEN)
		MCLGET(m, flags);
#endif
	return m;
}

int
ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_frame *wh;
	struct ath_buf *bf;
	struct ath_desc *ds;
	struct mbuf *m;
	int error, pktlen;
	u_int8_t *frm, rate;
	u_int16_t capinfo;
	struct ieee80211_rateset *rs;
	const HAL_RATE_TABLE *rt;
	u_int flags;

	bf = sc->sc_bcbuf;
	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;
	}
	/*
	 * NB: the beacon data buffer must be 32-bit aligned;
	 * we assume the mbuf routines will return us something
	 * with this alignment (perhaps should assert).
	 */
	rs = &ni->ni_rates;
	pktlen = sizeof (struct ieee80211_frame)
	       + 8 + 2 + 2 + 2+ni->ni_esslen + 2+rs->rs_nrates + 3 + 6;
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		pktlen += 2;
	m = ath_getmbuf(M_DONTWAIT, MT_DATA, pktlen);
	if (m == NULL) {
		DPRINTF(ATH_DEBUG_BEACON,
			("%s: cannot get mbuf/cluster; size %u\n",
			__func__, pktlen));
		sc->sc_stats.ast_be_nombuf++;
		return ENOMEM;
	}

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	memcpy(wh->i_addr1, sc->sc_broadcast_addr, IEEE80211_ADDR_LEN);
	memcpy(wh->i_addr2, ic->ic_myaddr, IEEE80211_ADDR_LEN);
	memcpy(wh->i_addr3, ni->ni_bssid, IEEE80211_ADDR_LEN);
	*(u_int16_t *)wh->i_seq = 0;

	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (IBSS)
	 *	[tlv] extended supported rates
	 */
	frm = (u_int8_t *)&wh[1];
	memset(frm, 0, 8);	/* timestamp is set by hardware */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = ni->ni_esslen;
	memcpy(frm, ni->ni_essid, ni->ni_esslen);
	frm += ni->ni_esslen;
	frm = ieee80211_add_rates(frm, rs);
	*frm++ = IEEE80211_ELEMID_DSPARMS;
	*frm++ = 1;
	*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
	} else {
		/* TODO: TIM */
		*frm++ = IEEE80211_ELEMID_TIM;
		*frm++ = 4;	/* length */
		*frm++ = 0;	/* DTIM count */ 
		*frm++ = 1;	/* DTIM period */
		*frm++ = 0;	/* bitmap control */
		*frm++ = 0;	/* Partial Virtual Bitmap (variable length) */
	}
	frm = ieee80211_add_xrates(frm, rs);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
	KASSERT(m->m_pkthdr.len <= pktlen,
		("beacon bigger than expected, len %u calculated %u",
		m->m_pkthdr.len, pktlen));

	DPRINTF(ATH_DEBUG_BEACON, ("%s: m %p len %u\n", __func__, m, m->m_len));
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		m_freem(m);
		return error;
	}
	KASSERT(bf->bf_nseg == 1,
		("%s: multi-segment packet; nseg %u", __func__, bf->bf_nseg));
	bf->bf_m = m;

	/* setup descriptors */
	ds = bf->bf_desc;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		ds->ds_link = bf->bf_daddr;	/* link to self */
	else
		ds->ds_link = 0;
	ds->ds_data = bf->bf_segs[0].ds_addr;

	DPRINTF(ATH_DEBUG_ANY, ("%s: segaddr %p seglen %u\n", __func__,
	    (caddr_t)bf->bf_segs[0].ds_addr, (u_int)bf->bf_segs[0].ds_len));

	/*
	 * Calculate rate code.
	 * XXX everything at min xmit rate
	 */
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		rate = rt->info[0].rateCode | rt->info[0].shortPreamble;
	else
		rate = rt->info[0].rateCode;

	flags = HAL_TXDESC_NOACK;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		flags |= HAL_TXDESC_VEOL;

	if (!ath_hal_setuptxdesc(ah, ds
		, m->m_pkthdr.len + IEEE80211_CRC_LEN	/* packet length */
		, sizeof(struct ieee80211_frame)	/* header length */
		, HAL_PKT_TYPE_BEACON		/* Atheros packet type */
		, 0x20				/* txpower XXX */
		, rate, 1			/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID		/* no encryption */
		, 0				/* antenna mode */
		, flags				/* no ack for beacons */
		, 0				/* rts/cts rate */
		, 0				/* rts/cts duration */
	)) {
		printf("%s: ath_hal_setuptxdesc failed\n", __func__);
		return -1;
	}
	/* NB: beacon's BufLen must be a multiple of 4 bytes */
	/* XXX verify mbuf data area covers this roundup */
	if (!ath_hal_filltxdesc(ah, ds
		, roundup(bf->bf_segs[0].ds_len, 4)	/* buffer length */
		, AH_TRUE				/* first segment */
		, AH_TRUE				/* last segment */
	)) {
		printf("%s: ath_hal_filltxdesc failed\n", __func__);
		return -1;
	}

	/* XXX it is not appropriate to bus_dmamap_sync? -dcy */

	return 0;
}

void
ath_beacon_proc(struct ath_softc *sc, int pending)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_buf *bf = sc->sc_bcbuf;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(ATH_DEBUG_BEACON_PROC, ("%s: pending %u\n", __func__, pending));
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    bf == NULL || bf->bf_m == NULL) {
		DPRINTF(ATH_DEBUG_ANY, ("%s: ic_flags=%x bf=%p bf_m=%p\n",
			__func__, ic->ic_flags, bf, bf ? bf->bf_m : NULL));
		return;
	}
	/* TODO: update beacon to reflect PS poll state */
	if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
		DPRINTF(ATH_DEBUG_ANY, ("%s: beacon queue %u did not stop?\n",
			__func__, sc->sc_bhalq));
		/* NB: the HAL still stops DMA, so proceed */
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
	    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);

	ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
	ath_hal_txstart(ah, sc->sc_bhalq);
	DPRINTF(ATH_DEBUG_BEACON_PROC,
		("%s: TXDP%u = %p (%p)\n", __func__,
		sc->sc_bhalq, (caddr_t)bf->bf_daddr, bf->bf_desc));
}

void
ath_beacon_free(struct ath_softc *sc)
{
	struct ath_buf *bf = sc->sc_bcbuf;

	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;
	}
}

/*
 * Configure the beacon and sleep timers.
 *
 * When operating as an AP this resets the TSF and sets
 * up the hardware to notify us when we need to issue beacons.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 */
void
ath_beacon_config(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t nexttbtt, intval;

	nexttbtt = (LE_READ_4(ni->ni_tstamp + 4) << 22) |
	    (LE_READ_4(ni->ni_tstamp) >> 10);
	DPRINTF(ATH_DEBUG_BEACON, ("%s: nexttbtt=%u\n", __func__, nexttbtt));
	nexttbtt += ni->ni_intval;
	intval = ni->ni_intval & HAL_BEACON_PERIOD;
	if (ic->ic_opmode == IEEE80211_M_STA) {
		HAL_BEACON_STATE bs;
		u_int32_t bmisstime;

		/* NB: no PCF support right now */
		memset(&bs, 0, sizeof(bs));
		/*
		 * Reset our tsf so the hardware will update the
		 * tsf register to reflect timestamps found in
		 * received beacons.
		 */
		bs.bs_intval = intval | HAL_BEACON_RESET_TSF;
		bs.bs_nexttbtt = nexttbtt;
		bs.bs_dtimperiod = bs.bs_intval;
		bs.bs_nextdtim = nexttbtt;
		/*
		 * Calculate the number of consecutive beacons to miss
		 * before taking a BMISS interrupt.  The configuration
		 * is specified in ms, so we need to convert that to
		 * TU's and then calculate based on the beacon interval.
		 * Note that we clamp the result to at most 10 beacons.
		 */
		bmisstime = (ic->ic_bmisstimeout * 1000) / 1024;
		bs.bs_bmissthreshold = howmany(bmisstime,ni->ni_intval);
		if (bs.bs_bmissthreshold > 10)
			bs.bs_bmissthreshold = 10;
		else if (bs.bs_bmissthreshold <= 0)
			bs.bs_bmissthreshold = 1;

		/*
		 * Calculate sleep duration.  The configuration is
		 * given in ms.  We insure a multiple of the beacon
		 * period is used.  Also, if the sleep duration is
		 * greater than the DTIM period then it makes senses
		 * to make it a multiple of that.
		 *
		 * XXX fixed at 100ms
		 */
		bs.bs_sleepduration =
			roundup((100 * 1000) / 1024, bs.bs_intval);
		if (bs.bs_sleepduration > bs.bs_dtimperiod)
			bs.bs_sleepduration = roundup(bs.bs_sleepduration, bs.bs_dtimperiod);

		DPRINTF(ATH_DEBUG_BEACON, 
			("%s: intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u\n"
			, __func__
			, bs.bs_intval
			, bs.bs_nexttbtt
			, bs.bs_dtimperiod
			, bs.bs_nextdtim
			, bs.bs_bmissthreshold
			, bs.bs_sleepduration
		));
		ath_hal_intrset(ah, 0);
		ath_hal_beacontimers(ah, &bs, 0/*XXX*/, 0, 0);
		sc->sc_imask |= HAL_INT_BMISS;
		ath_hal_intrset(ah, sc->sc_imask);
	} else {
		ath_hal_intrset(ah, 0);
		sc->sc_imask |= HAL_INT_SWBA;	/* beacon prepare */
		intval |= HAL_BEACON_ENA;
		switch (ic->ic_opmode) {
		/* No beacons in monitor, ad hoc-demo modes. */
		case IEEE80211_M_MONITOR:
		case IEEE80211_M_AHDEMO:
			intval &= ~HAL_BEACON_ENA;
			/*FALLTHROUGH*/
		/* In IBSS mode, I am uncertain how SWBA interrupts
		 * work, so I just turn them off and use a self-linked
		 * descriptor.
		 */
		case IEEE80211_M_IBSS:
			sc->sc_imask &= ~HAL_INT_SWBA;
			nexttbtt = ni->ni_intval;
			/*FALLTHROUGH*/
		case IEEE80211_M_HOSTAP:
		default:
			if (nexttbtt == ni->ni_intval)
				intval |= HAL_BEACON_RESET_TSF;
			break;
		}
		DPRINTF(ATH_DEBUG_BEACON, ("%s: intval %u nexttbtt %u\n",
			__func__, ni->ni_intval, nexttbtt));
		ath_hal_beaconinit(ah, nexttbtt, intval);
		ath_hal_intrset(ah, sc->sc_imask);
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			ath_beacon_proc(sc, 0);
	}
}

#ifdef __FreeBSD__
void
ath_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *paddr = (bus_addr_t*) arg;
	*paddr = segs->ds_addr;
}
#endif

#ifdef __FreeBSD__
int
ath_desc_alloc(struct ath_softc *sc)
{
	int i, bsize, error;
	struct ath_desc *ds;
	struct ath_buf *bf;

	/* allocate descriptors */
	sc->sc_desc_len = sizeof(struct ath_desc) *
				(ATH_TXBUF * ATH_TXDESC + ATH_RXBUF + 1);
	error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &sc->sc_ddmamap);
	if (error != 0)
		return error;

	error = bus_dmamem_alloc(sc->sc_dmat, (void**) &sc->sc_desc,
				 BUS_DMA_NOWAIT, &sc->sc_ddmamap);

	if (error != 0)
		goto fail0;

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_ddmamap,
				sc->sc_desc, sc->sc_desc_len,
				ath_load_cb, &sc->sc_desc_paddr,
				BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail1;

	ds = sc->sc_desc;
	DPRINTF(ATH_DEBUG_ANY, ("%s: DMA map: %p (%lu) -> %p (%lu)\n",
	    __func__, ds, (u_long) sc->sc_desc_len, (caddr_t) sc->sc_desc_paddr,
	    /*XXX*/ (u_long) sc->sc_desc_len));

	/* allocate buffers */
	bsize = sizeof(struct ath_buf) * (ATH_TXBUF + ATH_RXBUF + 1);
	bf = malloc(bsize, M_DEVBUF, M_NOWAIT);
	if (bf == NULL) {
		printf("%s: unable to allocate Tx/Rx buffers\n",
		    sc->sc_dev.dv_xname);
		error = -1;
		goto fail2;
	}
	bzero(bf, bsize);
	sc->sc_bufptr = bf;

	TAILQ_INIT(&sc->sc_rxbuf);
	for (i = 0; i < ATH_RXBUF; i++, bf++, ds++) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
					  &bf->bf_dmamap);
		if (error != 0)
			break;
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}

	TAILQ_INIT(&sc->sc_txbuf);
	for (i = 0; i < ATH_TXBUF; i++, bf++, ds += ATH_TXDESC) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
					  &bf->bf_dmamap);
		if (error != 0)
			break;
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
	}
	TAILQ_INIT(&sc->sc_txq);

	/* beacon buffer */
	bf->bf_desc = ds;
	bf->bf_daddr = sc->sc_desc_paddr + ((caddr_t)ds - (caddr_t)sc->sc_desc);
	error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &bf->bf_dmamap);
	if (error != 0)
		return error;
	sc->sc_bcbuf = bf;
	return 0;

fail2:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
fail1:
	bus_dmamem_free(sc->sc_dmat, sc->sc_desc, sc->sc_ddmamap);
fail0:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	sc->sc_ddmamap = NULL;
	return error;
}
#else
int
ath_desc_alloc(struct ath_softc *sc)
{
	int i, bsize, error = -1;
	struct ath_desc *ds;
	struct ath_buf *bf;

	/* allocate descriptors */
	sc->sc_desc_len = sizeof(struct ath_desc) *
				(ATH_TXBUF * ATH_TXDESC + ATH_RXBUF + 1);
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sc->sc_desc_len, PAGE_SIZE,
	    0, &sc->sc_dseg, 1, &sc->sc_dnseg, 0)) != 0) {
		printf("%s: unable to allocate control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg,
	    sc->sc_desc_len, (caddr_t *)&sc->sc_desc, BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, sc->sc_desc_len, 1,
	    sc->sc_desc_len, 0, 0, &sc->sc_ddmamap)) != 0) {
		printf("%s: unable to create control data DMA map, "
		    "error = %d\n", sc->sc_dev.dv_xname, error);
		goto fail2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_ddmamap, sc->sc_desc,
	    sc->sc_desc_len, NULL, 0)) != 0) {
		printf("%s: unable to load control data DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail3;
	}

	ds = sc->sc_desc;
	sc->sc_desc_paddr = sc->sc_ddmamap->dm_segs[0].ds_addr;

	DPRINTF(ATH_DEBUG_XMIT_DESC|ATH_DEBUG_RECV_DESC,
	    ("ath_desc_alloc: DMA map: %p (%lu) -> %p (%lu)\n",
	    ds, (u_long)sc->sc_desc_len,
	    (caddr_t) sc->sc_desc_paddr, /*XXX*/ (u_long) sc->sc_desc_len));

	/* allocate buffers */
	bsize = sizeof(struct ath_buf) * (ATH_TXBUF + ATH_RXBUF + 1);
	bf = malloc(bsize, M_DEVBUF, M_NOWAIT);
	if (bf == NULL) {
		printf("%s: unable to allocate Tx/Rx buffers\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail3;
	}
	bzero(bf, bsize);
	sc->sc_bufptr = bf;

	TAILQ_INIT(&sc->sc_rxbuf);
	for (i = 0; i < ATH_RXBUF; i++, bf++, ds++) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &bf->bf_dmamap)) != 0) {
			printf("%s: unable to create Rx dmamap, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			goto fail4;
		}
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}

	TAILQ_INIT(&sc->sc_txbuf);
	for (i = 0; i < ATH_TXBUF; i++, bf++, ds += ATH_TXDESC) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    ATH_TXDESC, MCLBYTES, 0, 0, &bf->bf_dmamap)) != 0) {
			printf("%s: unable to create Tx dmamap, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			goto fail5;
		}
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
	}
	TAILQ_INIT(&sc->sc_txq);

	/* beacon buffer */
	bf->bf_desc = ds;
	bf->bf_daddr = sc->sc_desc_paddr + ((caddr_t)ds - (caddr_t)sc->sc_desc);
	if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0, 0,
	    &bf->bf_dmamap)) != 0) {
		printf("%s: unable to create beacon dmamap, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail5;
	}
	sc->sc_bcbuf = bf;
	return 0;

fail5:
	for (i = ATH_RXBUF; i < ATH_RXBUF + ATH_TXBUF; i++) {
		if (sc->sc_bufptr[i].bf_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_bufptr[i].bf_dmamap);
	}
fail4:
	for (i = 0; i < ATH_RXBUF; i++) {
		if (sc->sc_bufptr[i].bf_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_bufptr[i].bf_dmamap);
	}
fail3:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
fail2:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	sc->sc_ddmamap = NULL;
fail1:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_desc, sc->sc_desc_len);
fail0:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg);
	return error;
}
#endif

void
ath_desc_free(struct ath_softc *sc)
{
	struct ath_buf *bf;

#ifdef __FreeBSD__
	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
	bus_dmamem_free(sc->sc_dmat, sc->sc_desc, sc->sc_ddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
#else
	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg);
#endif

	TAILQ_FOREACH(bf, &sc->sc_txq, bf_list) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
	}
	TAILQ_FOREACH(bf, &sc->sc_txbuf, bf_list)
		bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
	TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		if (bf->bf_m) {
			bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
			m_freem(bf->bf_m);
			bf->bf_m = NULL;
		}
	}
	if (sc->sc_bcbuf != NULL) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_bcbuf->bf_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_bcbuf->bf_dmamap);
		sc->sc_bcbuf = NULL;
	}

	TAILQ_INIT(&sc->sc_rxbuf);
	TAILQ_INIT(&sc->sc_txbuf);
	TAILQ_INIT(&sc->sc_txq);
	free(sc->sc_bufptr, M_DEVBUF);
	sc->sc_bufptr = NULL;
}

struct ieee80211_node *
ath_node_alloc(struct ieee80211com *ic)
{
	struct ath_node *an =
		malloc(sizeof(struct ath_node), M_DEVBUF, M_NOWAIT);
	if (an) {
		int i;
		bzero(an, sizeof(struct ath_node));
		for (i = 0; i < ATH_RHIST_SIZE; i++)
			an->an_rx_hist[i].arh_ticks = ATH_RHIST_NOTIME;
		an->an_rx_hist_next = ATH_RHIST_SIZE-1;
		return &an->an_node;
	} else
		return NULL;
}

void
ath_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ath_softc *sc = ic->ic_if.if_softc;
	struct ath_buf *bf;

	TAILQ_FOREACH(bf, &sc->sc_txq, bf_list) {
		if (bf->bf_node == ni)
			bf->bf_node = NULL;
	}
	(*sc->sc_node_free)(ic, ni);
}

void
ath_node_copy(struct ieee80211com *ic,
	struct ieee80211_node *dst, const struct ieee80211_node *src)
{
        struct ath_softc *sc = ic->ic_if.if_softc;

	memcpy(&dst[1], &src[1],
		sizeof(struct ath_node) - sizeof(struct ieee80211_node));
	(*sc->sc_node_copy)(ic, dst, src);
}

u_int8_t
ath_node_getrssi(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ath_node *an = ATH_NODE(ni);
	int i, now, nsamples, rssi;

	/*
	 * Calculate the average over the last second of sampled data.
	 */
	now = ATH_TICKS();
	nsamples = 0;
	rssi = 0;
	i = an->an_rx_hist_next;
	do {
		struct ath_recv_hist *rh = &an->an_rx_hist[i];
		if (rh->arh_ticks == ATH_RHIST_NOTIME)
			goto done;
		if (now - rh->arh_ticks > hz)
			goto done;
		rssi += rh->arh_rssi;
		nsamples++;
		if (i == 0)
			i = ATH_RHIST_SIZE-1;
		else
			i--;
	} while (i != an->an_rx_hist_next);
done:
	/*
	 * Return either the average or the last known
	 * value if there is no recent data.
	 */
	return (nsamples ? rssi / nsamples : an->an_rx_hist[i].arh_rssi);
}

int
ath_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	int error;
	struct mbuf *m;
	struct ath_desc *ds;

	m = bf->bf_m;
	if (m == NULL) {
		/*
		 * NB: by assigning a page to the rx dma buffer we
		 * implicitly satisfy the Atheros requirement that
		 * this buffer be cache-line-aligned and sized to be
		 * multiple of the cache line size.  Not doing this
		 * causes weird stuff to happen (for the 5210 at least).
		 */
		m = ath_getmbuf(M_DONTWAIT, MT_DATA, MCLBYTES);
		if (m == NULL) {
			DPRINTF(ATH_DEBUG_ANY,
				("%s: no mbuf/cluster\n", __func__));
			sc->sc_stats.ast_rx_nombuf++;
			return ENOMEM;
		}
		bf->bf_m = m;
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
		    BUS_DMA_NOWAIT);	
		if (error != 0) {
			DPRINTF(ATH_DEBUG_ANY,
				("%s: ath_bus_dmamap_load_mbuf failed;"
				" error %d\n", __func__, error));
			sc->sc_stats.ast_rx_busdma++;
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("ath_rxbuf_init: multi-segment packet; nseg %u",
			bf->bf_nseg));
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
	    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY errors).
	 *
	 * To insure the last descriptor is self-linked we create
	 * each descriptor as self-linked and add it to the end.  As
	 * each additional descriptor is added the previous self-linked
	 * entry is ``fixed'' naturally.  This should be safe even
	 * if DMA is happening.  When processing RX interrupts we
	 * never remove/process the last, self-linked, entry on the
	 * descriptor list.  This insures the hardware always has
	 * someplace to write a new frame.
	 */
	ds = bf->bf_desc;
	ds->ds_link = bf->bf_daddr;	/* link to self */
	ds->ds_data = bf->bf_segs[0].ds_addr;
	ath_hal_setuprxdesc(ah, ds
		, m->m_len		/* buffer size */
		, 0
	);

	if (sc->sc_rxlink != NULL)
		*sc->sc_rxlink = bf->bf_daddr;
	sc->sc_rxlink = &ds->ds_link;
	return 0;
}

void
ath_rx_proc(void *arg, int npending)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_desc + \
		((_pa) - (_sc)->sc_desc_paddr)))
	struct ath_softc *sc = arg;
	struct ath_buf *bf;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct mbuf *m;
	struct ieee80211_frame *wh, whbuf;
	struct ieee80211_node *ni;
	struct ath_node *an;
	struct ath_recv_hist *rh;
	int len;
	u_int phyerr;
	HAL_STATUS status;

	DPRINTF(ATH_DEBUG_RX_PROC, ("%s: pending %u\n", __func__, npending));
	do {
		bf = TAILQ_FIRST(&sc->sc_rxbuf);
		if (bf == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "ath_rx_proc: no buffer!\n");
			break;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr) {
			/* NB: never process the self-linked entry at the end */
			break;
		}
		m = bf->bf_m;
		if (m == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "ath_rx_proc: no mbuf!\n");
			continue;
		}
		/* XXX sync descriptor memory */
		/*
		 * Must provide the virtual address of the current
		 * descriptor, the physical address, and the virtual
		 * address of the next descriptor in the h/w chain.
		 * This allows the HAL to look ahead to see if the
		 * hardware is done with a descriptor by checking the
		 * done bit in the following descriptor and the address
		 * of the current descriptor the DMA engine is working
		 * on.  All this is necessary because of our use of
		 * a self-linked list to avoid rx overruns.
		 */
		status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link));
#ifdef AR_DEBUG
		if (ath_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(bf, status == HAL_OK); 
#endif
		if (status == HAL_EINPROGRESS)
			break;
		TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);

		if (ds->ds_rxstat.rs_more) {
			/*
			 * Frame spans multiple descriptors; this
			 * cannot happen yet as we don't support
			 * jumbograms.  If not in monitor mode,
			 * discard the frame.
			 */

			/* enable this if you want to see error frames in Monitor mode */
#ifdef ERROR_FRAMES
			if (ic->ic_opmode != IEEE80211_M_MONITOR) {
				/* XXX statistic */
				goto rx_next;
			}
#endif
			/* fall thru for monitor mode handling... */

		} else if (ds->ds_rxstat.rs_status != 0) {
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
				sc->sc_stats.ast_rx_crcerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
				sc->sc_stats.ast_rx_fifoerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT)
				sc->sc_stats.ast_rx_badcrypt++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY) {
				sc->sc_stats.ast_rx_phyerr++;
				phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
				sc->sc_stats.ast_rx_phy[phyerr]++;
			}

			/*
			 * reject error frames, we normally don't want
			 * to see them in monitor mode.
			 */
			if ((ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT ) ||
			    (ds->ds_rxstat.rs_status & HAL_RXERR_PHY))
			    goto rx_next;

			/*
			 * In monitor mode, allow through packets that
			 * cannot be decrypted
			 */
			if ((ds->ds_rxstat.rs_status & ~HAL_RXERR_DECRYPT) ||
			    sc->sc_ic.ic_opmode != IEEE80211_M_MONITOR)
				goto rx_next;
		}

		len = ds->ds_rxstat.rs_datalen;
		if (len < IEEE80211_MIN_LEN) {
			DPRINTF(ATH_DEBUG_RECV, ("%s: short packet %d\n",
				__func__, len));
			sc->sc_stats.ast_rx_tooshort++;
			goto rx_next;
		}

		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0, 
		    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bf->bf_m = NULL;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

#if NBPFILTER > 0
		if (sc->sc_drvbpf) {
			struct mbuf mb;

			sc->sc_rx_th.wr_rate =
				sc->sc_hwmap[ds->ds_rxstat.rs_rate];
			sc->sc_rx_th.wr_antsignal = ds->ds_rxstat.rs_rssi;
			sc->sc_rx_th.wr_antenna = ds->ds_rxstat.rs_antenna;
			/* XXX TSF */
			M_DUP_PKTHDR(&mb, m);
			mb.m_data = (caddr_t)&sc->sc_rx_th;
			mb.m_len = sc->sc_rx_th_len;
			mb.m_next = m;
			mb.m_pkthdr.len += mb.m_len;
			bpf_mtap(sc->sc_drvbpf, &mb);
		}
#endif

		m_adj(m, -IEEE80211_CRC_LEN);
		wh = mtod(m, struct ieee80211_frame *);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			/*
			 * WEP is decrypted by hardware. Clear WEP bit
			 * and trim WEP header for ieee80211_input().
			 */
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
			memcpy(&whbuf, wh, sizeof(whbuf));
			m_adj(m, IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN);
			wh = mtod(m, struct ieee80211_frame *);
			memcpy(wh, &whbuf, sizeof(whbuf));
			/*
			 * Also trim WEP ICV from the tail.
			 */
			m_adj(m, -IEEE80211_WEP_CRCLEN);
			/*
			 * The header has probably moved.
			 */
			wh = mtod(m, struct ieee80211_frame *);
		}

		/*
		 * Locate the node for sender, track state, and
		 * then pass this node (referenced) up to the 802.11
		 * layer for its use.
		 */
		ni = ieee80211_find_rxnode(ic, wh);

		/*
		 * Record driver-specific state.
		 */
		an = ATH_NODE(ni);
		if (++(an->an_rx_hist_next) == ATH_RHIST_SIZE)
			an->an_rx_hist_next = 0;
		rh = &an->an_rx_hist[an->an_rx_hist_next];
		rh->arh_ticks = ATH_TICKS();
		rh->arh_rssi = ds->ds_rxstat.rs_rssi;
		rh->arh_antenna = ds->ds_rxstat.rs_antenna;

		/*
		 * Send frame up for processing.
		 */
		ieee80211_input(ifp, m, ni,
			ds->ds_rxstat.rs_rssi, ds->ds_rxstat.rs_tstamp);

		/*
		 * The frame may have caused the node to be marked for
		 * reclamation (e.g. in response to a DEAUTH message)
		 * so use release_node here instead of unref_node.
		 */
		if (ni == ic->ic_bss)
			ieee80211_unref_node(&ni);
		else
			ieee80211_free_node(ic, ni);

  rx_next:
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	ath_hal_rxmonitor(ah);			/* rx signal state monitoring */
	ath_hal_rxena(ah);			/* in case of RXEOL */

	if ((ifp->if_flags & IFF_OACTIVE) == 0 && !IFQ_IS_EMPTY(&ifp->if_snd))
		ath_start(ifp);
#undef PA2DESC
}

/*
 * XXX Size of an ACK control frame in bytes.
 */
#define	IEEE80211_ACK_SIZE	(2+2+IEEE80211_ADDR_LEN+4)

int
ath_tx_start(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_buf *bf,
    struct mbuf *m0)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int i, error, iswep, hdrlen, pktlen, s;
	u_int8_t rix, cix, txrate, ctsrate;
	struct ath_desc *ds;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	u_int32_t iv;
	u_int8_t *ivp;
	u_int8_t hdrbuf[sizeof(struct ieee80211_frame) +
	    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN];
	u_int subtype, flags, ctsduration, antenna;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;

	wh = mtod(m0, struct ieee80211_frame *);
	iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	hdrlen = sizeof(struct ieee80211_frame);
	pktlen = m0->m_pkthdr.len;

	if (iswep) {
		memcpy(hdrbuf, mtod(m0, caddr_t), hdrlen);
		m_adj(m0, hdrlen);
		M_PREPEND(m0, sizeof(hdrbuf), M_DONTWAIT);
		if (m0 == NULL) {
			sc->sc_stats.ast_tx_nombuf++;
			return ENOMEM;
		}
		ivp = hdrbuf + hdrlen;
		wh = mtod(m0, struct ieee80211_frame *);
		/*
		 * XXX
		 * IV must not duplicate during the lifetime of the key.
		 * But no mechanism to renew keys is defined in IEEE 802.11
		 * for WEP.  And the IV may be duplicated at other stations
		 * because the session key itself is shared.  So we use a
		 * pseudo random IV for now, though it is not the right way.
		 *
		 * NB: Rather than use a strictly random IV we select a
		 * random one to start and then increment the value for
		 * each frame.  This is an explicit tradeoff between
		 * overhead and security.  Given the basic insecurity of
		 * WEP this seems worthwhile.
		 */

		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 16 and 0 <= N <= 255
		 */
		iv = ic->ic_iv;
		if ((iv & 0xff00) == 0xff00) {
			int B = (iv & 0xff0000) >> 16;
			if (3 <= B && B < 16)
				iv = (B+1) << 16;
		}
		ic->ic_iv = iv + 1;

		/*
		 * NB: Preserve byte order of IV for packet
		 *     sniffers; it doesn't matter otherwise.
		 */
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
		ivp[0] = iv >> 0;
		ivp[1] = iv >> 8;
		ivp[2] = iv >> 16;
#else
		ivp[2] = iv >> 0;
		ivp[1] = iv >> 8;
		ivp[0] = iv >> 16;
#endif
		ivp[3] = ic->ic_wep_txkey << 6; /* Key ID and pad */
		memcpy(mtod(m0, caddr_t), hdrbuf, sizeof(hdrbuf));
		/*
		 * The ICV length must be included into hdrlen and pktlen.
		 */
		hdrlen = sizeof(hdrbuf) + IEEE80211_WEP_CRCLEN;
		pktlen = m0->m_pkthdr.len + IEEE80211_WEP_CRCLEN;
	}
	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
	    BUS_DMA_NOWAIT);
	/*
	 * Discard null packets and check for packets that
	 * require too many TX descriptors.  We try to convert
	 * the latter to a cluster.
	 */
	if (error == EFBIG) {		/* too many desc's, linearize */
		sc->sc_stats.ast_tx_linear++;
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			sc->sc_stats.ast_tx_nombuf++;
			m_freem(m0);
			return ENOMEM;
		}

		M_DUP_PKTHDR(m, m0);
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			sc->sc_stats.ast_tx_nomcl++;
			m_freem(m0);
			m_free(m);
			return ENOMEM;
		}
		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
		m_freem(m0);
		m->m_len = m->m_pkthdr.len;
		m0 = m;
		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			sc->sc_stats.ast_tx_busdma++;
			m_freem(m0);
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("ath_tx_start: packet not one segment; nseg %u",
			bf->bf_nseg));
	} else if (error != 0) {
		sc->sc_stats.ast_tx_busdma++;
		m_freem(m0);
		return error;
	} else if (bf->bf_nseg == 0) {		/* null packet, discard */
		sc->sc_stats.ast_tx_nodata++;
		m_freem(m0);
		return EIO;
	}
	DPRINTF(ATH_DEBUG_XMIT, ("%s: m %p len %u\n", __func__, m0, pktlen));
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
	    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bf->bf_m = m0;
	bf->bf_node = ni;			/* NB: held reference */

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * Calculate Atheros packet type from IEEE80211 packet header
	 * and setup for rate calculations.
	 */
	atype = HAL_PKT_TYPE_NORMAL;			/* default */
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_MGT:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
			atype = HAL_PKT_TYPE_BEACON;
		else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			atype = HAL_PKT_TYPE_PROBE_RESP;
		else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM)
			atype = HAL_PKT_TYPE_ATIM;
		rix = 0;			/* XXX lowest rate */
		break;
	case IEEE80211_FC0_TYPE_CTL:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL)
			atype = HAL_PKT_TYPE_PSPOLL;
		rix = 0;			/* XXX lowest rate */
		break;
	default:
		rix = sc->sc_rixmap[ni->ni_rates.rs_rates[ni->ni_txrate] &
				IEEE80211_RATE_VAL];
		if (rix == 0xff) {
			if_printf(ifp, "bogus xmit rate 0x%x\n",
				ni->ni_rates.rs_rates[ni->ni_txrate]);
			sc->sc_stats.ast_tx_badrate++;
			m_freem(m0);
			return EIO;
		}
		break;
	}
	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
		txrate = rt->info[rix].rateCode | rt->info[rix].shortPreamble;
		shortPreamble = AH_TRUE;
		sc->sc_stats.ast_tx_shortpre++;
	} else {
		txrate = rt->info[rix].rateCode;
		shortPreamble = AH_FALSE;
	}

	/*
	 * Calculate miscellaneous flags.
	 */
	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for wep errors */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
		sc->sc_stats.ast_tx_noack++;
	} else if (pktlen > ic->ic_rtsthreshold) {
		flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
		sc->sc_stats.ast_tx_rts++;
	}

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if ((flags & HAL_TXDESC_NOACK) == 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL) {
		u_int16_t dur;
		/*
		 * XXX not right with fragmentation.
		 */
		dur = ath_hal_computetxtime(ah, rt, IEEE80211_ACK_SIZE,
				rix, shortPreamble);
		*((u_int16_t*) wh->i_dur) = htole16(dur);
	}

	/*
	 * Calculate RTS/CTS rate and duration if needed.
	 */
	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		/*
		 * CTS transmit rate is derived from the transmit rate
		 * by looking in the h/w rate table.  We must also factor
		 * in whether or not a short preamble is to be used.
		 */
		cix = rt->info[rix].controlRate;
		ctsrate = rt->info[cix].rateCode;
		if (shortPreamble)
			ctsrate |= rt->info[cix].shortPreamble;
		/*
		 * Compute the transmit duration based on the size
		 * of an ACK frame.  We call into the HAL to do the
		 * computation since it depends on the characteristics
		 * of the actual PHY being used.
		 */
		if (flags & HAL_TXDESC_RTSENA) {	/* SIFS + CTS */
			ctsduration += ath_hal_computetxtime(ah,
				rt, IEEE80211_ACK_SIZE, cix, shortPreamble);
		}
		/* SIFS + data */
		ctsduration += ath_hal_computetxtime(ah,
			rt, pktlen, rix, shortPreamble);
		if ((flags & HAL_TXDESC_NOACK) == 0) {	/* SIFS + ACK */
			ctsduration += ath_hal_computetxtime(ah,
				rt, IEEE80211_ACK_SIZE, cix, shortPreamble);
		}
	} else
		ctsrate = 0;

	/*
	 * For now use the antenna on which the last good
	 * frame was received on.  We assume this field is
	 * initialized to 0 which gives us ``auto'' or the
	 * ``default'' antenna.
	 */
	an = (struct ath_node *) ni;
	if (an->an_tx_antenna)
		antenna = an->an_tx_antenna;
	else
		antenna = an->an_rx_hist[an->an_rx_hist_next].arh_antenna;

	if (ic->ic_rawbpf)
		bpf_mtap(ic->ic_rawbpf, m0);
	if (sc->sc_drvbpf) {
	        struct mbuf mb;

		sc->sc_tx_th.wt_flags = 0;
		if (shortPreamble)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		if (iswep)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		sc->sc_tx_th.wt_rate = ni->ni_rates.rs_rates[ni->ni_txrate];
		sc->sc_tx_th.wt_txpower = 60/2;		/* XXX */
		sc->sc_tx_th.wt_antenna = antenna;

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)&sc->sc_tx_th;
		mb.m_len = sc->sc_tx_th_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, 60			/* txpower XXX */
		, txrate, 1+10		/* series 0 rate/tries */
		, iswep ? sc->sc_ic.ic_wep_txkey : HAL_TXKEYIX_INVALID
		, antenna		/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
#ifdef notyet
	ath_hal_setupxtxdesc(ah, ds
		, AH_FALSE		/* short preamble */
		, 0, 0			/* series 1 rate/tries */
		, 0, 0			/* series 2 rate/tries */
		, 0, 0			/* series 3 rate/tries */
	);
#endif
	/*
	 * Fillin the remainder of the descriptor info.
	 */
	for (i = 0; i < bf->bf_nseg; i++, ds++) {
		ds->ds_data = bf->bf_segs[i].ds_addr;
		if (i == bf->bf_nseg - 1)
			ds->ds_link = 0;
		else
			ds->ds_link = bf->bf_daddr + sizeof(*ds) * (i + 1);
		ath_hal_filltxdesc(ah, ds
			, bf->bf_segs[i].ds_len	/* segment length */
			, i == 0		/* first segment */
			, i == bf->bf_nseg - 1	/* last segment */
		);
		DPRINTF(ATH_DEBUG_XMIT,
			("%s: %d: %08x %08x %08x %08x %08x %08x\n",
			__func__, i, ds->ds_link, ds->ds_data,
			ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]));
	}

	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	s = splnet();
	TAILQ_INSERT_TAIL(&sc->sc_txq, bf, bf_list);
	if (sc->sc_txlink == NULL) {
		ath_hal_puttxbuf(ah, sc->sc_txhalq, bf->bf_daddr);
		DPRINTF(ATH_DEBUG_XMIT, ("%s: TXDP0 = %p (%p)\n", __func__,
		    (caddr_t)bf->bf_daddr, bf->bf_desc));
	} else {
		*sc->sc_txlink = bf->bf_daddr;
		DPRINTF(ATH_DEBUG_XMIT, ("%s: link(%p)=%p (%p)\n", __func__,
		    sc->sc_txlink, (caddr_t)bf->bf_daddr, bf->bf_desc));
	}
	sc->sc_txlink = &bf->bf_desc[bf->bf_nseg - 1].ds_link;
	splx(s);

	ath_hal_txstart(ah, sc->sc_txhalq);
	return 0;
}

void
ath_tx_proc(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_desc *ds;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int sr, lr, s;
	HAL_STATUS status;

	DPRINTF(ATH_DEBUG_TX_PROC, ("%s: pending %u tx queue %p, link %p\n",
		__func__, npending,
		(caddr_t)(u_intptr_t) ath_hal_gettxbuf(sc->sc_ah, sc->sc_txhalq),
		sc->sc_txlink));
	for (;;) {
		s = splnet();
		bf = TAILQ_FIRST(&sc->sc_txq);
		if (bf == NULL) {
			sc->sc_txlink = NULL;
			splx(s);
			break;
		}
		/* only the last descriptor is needed */
		ds = &bf->bf_desc[bf->bf_nseg - 1];
		status = ath_hal_txprocdesc(ah, ds);
#ifdef AR_DEBUG
		if (ath_debug & ATH_DEBUG_XMIT_DESC)
			ath_printtxbuf(bf, status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS) {
			splx(s);
			break;
		}
		TAILQ_REMOVE(&sc->sc_txq, bf, bf_list);
		splx(s);

		ni = bf->bf_node;
		if (ni != NULL) {
			an = (struct ath_node *) ni;
			if (ds->ds_txstat.ts_status == 0) {
				an->an_tx_ok++;
				an->an_tx_antenna = ds->ds_txstat.ts_antenna;
			} else {
				an->an_tx_err++;
				ifp->if_oerrors++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_XRETRY)
					sc->sc_stats.ast_tx_xretries++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FIFO)
					sc->sc_stats.ast_tx_fifoerr++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FILT)
					sc->sc_stats.ast_tx_filtered++;
				an->an_tx_antenna = 0;	/* invalidate */
			}
			sr = ds->ds_txstat.ts_shortretry;
			lr = ds->ds_txstat.ts_longretry;
			sc->sc_stats.ast_tx_shortretry += sr;
			sc->sc_stats.ast_tx_longretry += lr;
			if (sr + lr)
				an->an_tx_retr++;
			/*
			 * Reclaim reference to node.
			 *
			 * NB: the node may be reclaimed here if, for example
			 *     this is a DEAUTH message that was sent and the
			 *     node was timed out due to inactivity.
			 */
			if(ni != NULL && ni != ic->ic_bss)
  			        ieee80211_free_node(ic, ni);
		}
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
		    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;

		s = splnet();
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		splx(s);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_tx_timer = 0;

	ath_start(ifp);
}

/*
 * Drain the transmit queue and reclaim resources.
 */
void
ath_draintxq(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	int s;

	/* XXX return value */
	if (!sc->sc_invalid) {
		/* don't touch the hardware if marked invalid */
		(void) ath_hal_stoptxdma(ah, sc->sc_txhalq);
		DPRINTF(ATH_DEBUG_RESET,
		    ("%s: tx queue %p, link %p\n", __func__,
		    (caddr_t)(u_intptr_t) ath_hal_gettxbuf(ah, sc->sc_txhalq),
		    sc->sc_txlink));
		(void) ath_hal_stoptxdma(ah, sc->sc_bhalq);
		DPRINTF(ATH_DEBUG_RESET,
		    ("%s: beacon queue %p\n", __func__,
		    (caddr_t)(u_intptr_t) ath_hal_gettxbuf(ah, sc->sc_bhalq)));
	}
	for (;;) {
		s = splnet();
		bf = TAILQ_FIRST(&sc->sc_txq);
		if (bf == NULL) {
			sc->sc_txlink = NULL;
			splx(s);
			break;
		}
		TAILQ_REMOVE(&sc->sc_txq, bf, bf_list);
		splx(s);
#ifdef AR_DEBUG
		if (ath_debug & ATH_DEBUG_RESET)
			ath_printtxbuf(bf,
				ath_hal_txprocdesc(ah, bf->bf_desc) == HAL_OK);
#endif /* AR_DEBUG */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		ni = bf->bf_node;
		bf->bf_node = NULL;
		s = splnet();
		if (ni != NULL && ni != ic->ic_bss) {
			/*
			 * Reclaim node reference.
			 */
			ieee80211_free_node(ic, ni);
		}
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		splx(s);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_tx_timer = 0;
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
void
ath_stoprecv(struct ath_softc *sc)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_desc + \
		((_pa) - (_sc)->sc_desc_paddr)))
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	DELAY(3000);			/* long enough for 1 frame */
#ifdef AR_DEBUG
	if (ath_debug & ATH_DEBUG_RESET) {
		struct ath_buf *bf;

		printf("%s: rx queue %p, link %p\n", __func__,
			(caddr_t)(u_intptr_t) ath_hal_getrxbuf(ah), sc->sc_rxlink);
		TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			struct ath_desc *ds = bf->bf_desc;
			if (ath_hal_rxprocdesc(ah, ds, bf->bf_daddr,
			    PA2DESC(sc, ds->ds_link)) == HAL_OK)
				ath_printrxbuf(bf, 1);
		}
	}
#endif
	sc->sc_rxlink = NULL;		/* just in case */
#undef PA2DESC
}

/*
 * Enable the receive h/w following a reset.
 */
int
ath_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;

	sc->sc_rxlink = NULL;
	TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(ATH_DEBUG_RECV,
				("%s: ath_rxbuf_init failed %d\n",
				__func__, error));
			return error;
		}
	}

	bf = TAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(sc);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
}

/*
 * Set/change channels.  If the channel is really being changed,
 * it's done by resetting the chip.  To accomplish this we must
 * first cleanup any pending DMA, then restart stuff after a la
 * ath_init.
 */
int
ath_chan_set(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(ATH_DEBUG_ANY, ("%s: %u (%u MHz) -> %u (%u MHz)\n", __func__,
	    ieee80211_chan2ieee(ic, ic->ic_ibss_chan),
		ic->ic_ibss_chan->ic_freq,
	    ieee80211_chan2ieee(ic, chan), chan->ic_freq));
	if (chan != ic->ic_ibss_chan) {
		HAL_STATUS status;
		HAL_CHANNEL hchan;
		enum ieee80211_phymode mode;

		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		ath_stoprecv(sc);		/* turn off frame recv */
		/*
		 * Convert to a HAL channel description with
		 * the flags constrained to reflect the current
		 * operating mode.
		 */
		hchan.channel = chan->ic_freq;
		hchan.channelFlags = ath_chan2flags(ic, chan);
		if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status)) {
			if_printf(&ic->ic_if, "ath_chan_set: unable to reset "
				"channel %u (%u Mhz)\n",
				ieee80211_chan2ieee(ic, chan), chan->ic_freq);
			return EIO;
		}
		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(&ic->ic_if,
				"ath_chan_set: unable to restart recv logic\n");
			return EIO;
		}

		/*
		 * Update BPF state.
		 */
		sc->sc_tx_th.wt_chan_freq = sc->sc_rx_th.wr_chan_freq =
			htole16(chan->ic_freq);
		sc->sc_tx_th.wt_chan_flags = sc->sc_rx_th.wr_chan_flags =
			htole16(chan->ic_flags);

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ic->ic_ibss_chan = chan;
		mode = ieee80211_chan2mode(ic, chan);
		if (mode != sc->sc_curmode)
			ath_setcurmode(sc, mode);

		/*
		 * Re-enable interrupts.
		 */
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
}

void
ath_next_scan(void *arg)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	/* don't call ath_start w/o network interrupts blocked */
	s = splnet();

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
	splx(s);
}

/*
 * Periodically recalibrate the PHY to account
 * for temperature/environment changes.
 */
void
ath_calibrate(void *arg)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;
	HAL_CHANNEL hchan;

	sc->sc_stats.ast_per_cal++;

	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	hchan.channel = c->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, c);

	DPRINTF(ATH_DEBUG_CALIBRATE,
		("%s: channel %u/%x\n", __func__, c->ic_freq, c->ic_flags));

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
		/*
		 * Rfgain is out of bounds, reset the chip
		 * to load new gain values.
		 */
		sc->sc_stats.ast_per_rfgain++;
		ath_reset(sc);
	}
	if (!ath_hal_calibrate(ah, &hchan)) {
		DPRINTF(ATH_DEBUG_ANY,
			("%s: calibration of channel %u failed\n",
			__func__, c->ic_freq));
		sc->sc_stats.ast_per_calfail++;
	}
	timeout_add(&sc->sc_cal_to, hz * ath_calinterval);
}

HAL_LED_STATE
ath_state_to_led(enum ieee80211_state state)
{
	switch (state) {
	case IEEE80211_S_INIT:
		return HAL_LED_INIT;
	case IEEE80211_S_SCAN:
		return HAL_LED_SCAN;
	case IEEE80211_S_AUTH:
		return HAL_LED_AUTH;
	case IEEE80211_S_ASSOC:
		return HAL_LED_ASSOC;
	case IEEE80211_S_RUN:
		return HAL_LED_RUN;
	default:
		panic("%s: unknown 802.11 state %d\n", __func__, state);
		return HAL_LED_INIT;
	}
}

int
ath_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	const u_int8_t *bssid;
	int i, error;
	u_int32_t rfilt;

	DPRINTF(ATH_DEBUG_ANY, ("%s: %s -> %s\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate]));

	ath_hal_setledstate(ah, ath_state_to_led(nstate));	/* set LED */

	if (nstate == IEEE80211_S_INIT) {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		ath_hal_intrset(ah, sc->sc_imask);
		timeout_del(&sc->sc_scan_to);
		timeout_del(&sc->sc_cal_to);
		return (*sc->sc_newstate)(ic, nstate, arg);
	}
	ni = ic->ic_bss;
	error = ath_chan_set(sc, ni->ni_chan);
	if (error != 0)
		goto bad;
	rfilt = ath_calcrxfilter(sc);
	if (nstate == IEEE80211_S_SCAN) {
	        timeout_add(&sc->sc_scan_to, (hz * ath_dwelltime) / 1000);
		bssid = sc->sc_broadcast_addr;
	} else {
		timeout_del(&sc->sc_scan_to);
		bssid = ni->ni_bssid;
	}
	ath_hal_setrxfilter(ah, rfilt);
	DPRINTF(ATH_DEBUG_ANY, ("%s: RX filter 0x%x bssid %s\n",
		 __func__, rfilt, ether_sprintf((u_char*)bssid)));

	if (nstate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA)
		ath_hal_setassocid(ah, bssid, ni->ni_associd);
	else
		ath_hal_setassocid(ah, bssid, 0);
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, bssid);
	}

	if (nstate == IEEE80211_S_RUN) {
		DPRINTF(ATH_DEBUG_ANY, ("%s(RUN): ic_flags=0x%08x iv=%d bssid=%s "
			"capinfo=0x%04x chan=%d\n"
			 , __func__
			 , ic->ic_flags
			 , ni->ni_intval
			 , ether_sprintf(ni->ni_bssid)
			 , ni->ni_capinfo
			 , ieee80211_chan2ieee(ic, ni->ni_chan)));

		/*
		 * Allocate and setup the beacon frame for AP or adhoc mode.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			error = ath_beacon_alloc(sc, ni);
			if (error != 0)
				goto bad;
		}

		/*
		 * Configure the beacon and sleep timers.
		 */
		ath_beacon_config(sc);

		/* start periodic recalibration timer */
	        timeout_add(&sc->sc_cal_to, hz * ath_calinterval);
	} else {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		ath_hal_intrset(ah, sc->sc_imask);
		timeout_del(&sc->sc_cal_to); /* no calibration */
	}
	/*
	 * Reset the rate control state.
	 */
	ath_rate_ctl_reset(sc, nstate);
	/*
	 * Invoke the parent method to complete the work.
	 */
	return (*sc->sc_newstate)(ic, nstate, arg);
bad:
	timeout_del(&sc->sc_scan_to);
	timeout_del(&sc->sc_cal_to);
	/* NB: do not invoke the parent */
	return error;
}

void
ath_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct ath_softc *sc = (struct ath_softc*)ic->ic_softc;
	struct ath_hal *ah = sc->sc_ah;

	(*sc->sc_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ic->ic_state != IEEE80211_S_RUN)
			break;
		if (ieee80211_ibss_merge(ic, ni, ath_hal_gettsf64(ah)) ==
		    ENETRESET)
			ath_hal_setassocid(ah, ic->ic_bss->ni_bssid, 0);
		break;
	default:
		break;
	}
	return;
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
void
ath_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	if (isnew) {
		struct ath_node *an = (struct ath_node *) ni;

		an->an_tx_ok = an->an_tx_err =
			an->an_tx_retr = an->an_tx_upper = 0;
		/* start with highest negotiated rate */
		/*
		 * XXX should do otherwise but only when
		 * the rate control algorithm is better.
		 */
		KASSERT(ni->ni_rates.rs_nrates > 0,
			("new association w/ no rates!"));
		ni->ni_txrate = ni->ni_rates.rs_nrates - 1;
	}
}

int
ath_getchannels(struct ath_softc *sc, u_int cc, HAL_BOOL outdoor,
    HAL_BOOL xchanmode)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *chans;
	int i, ix, nchan;

	chans = malloc(IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL),
			M_TEMP, M_NOWAIT);
	if (chans == NULL) {
		if_printf(ifp, "unable to allocate channel table\n");
		return ENOMEM;
	}
	if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, &nchan,
	    cc, HAL_MODE_ALL, outdoor, xchanmode)) {
		if_printf(ifp, "unable to collect channel list from hal\n");
		free(chans, M_TEMP);
		return EINVAL;
	}

	/*
	 * Convert HAL channels to ieee80211 ones and insert
	 * them in the table according to their channel number.
	 */
	for (i = 0; i < nchan; i++) {
		HAL_CHANNEL *c = &chans[i];
		ix = ath_hal_mhz2ieee(c->channel, c->channelFlags);
		if (ix > IEEE80211_CHAN_MAX) {
			if_printf(ifp, "bad hal channel %u (%u/%x) ignored\n",
				ix, c->channel, c->channelFlags);
			continue;
		}
		DPRINTF(ATH_DEBUG_ANY,
		    ("%s: HAL channel %d/%d freq %d flags %#04x idx %d\n",
		    sc->sc_dev.dv_xname, i, nchan, c->channel, c->channelFlags,
		    ix));
		/* NB: flags are known to be compatible */
		if (ic->ic_channels[ix].ic_freq == 0) {
			ic->ic_channels[ix].ic_freq = c->channel;
			ic->ic_channels[ix].ic_flags = c->channelFlags;
		} else {
			/* channels overlap; e.g. 11g and 11b */
			ic->ic_channels[ix].ic_flags |= c->channelFlags;
		}
	}
	free(chans, M_TEMP);
	return 0;
}

int
ath_rate_setup(struct ath_softc *sc, u_int mode)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	const HAL_RATE_TABLE *rt;
	struct ieee80211_rateset *rs;
	int i, maxrates;

	switch (mode) {
	case IEEE80211_MODE_11A:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11A);
		break;
	case IEEE80211_MODE_11B:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11B);
		break;
	case IEEE80211_MODE_11G:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11G);
		break;
	case IEEE80211_MODE_TURBO:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_TURBO);
		break;
	default:
		DPRINTF(ATH_DEBUG_ANY,
			("%s: invalid mode %u\n", __func__, mode));
		return 0;
	}
	rt = sc->sc_rates[mode];
	if (rt == NULL)
		return 0;
	if (rt->rateCount > IEEE80211_RATE_MAXSIZE) {
		DPRINTF(ATH_DEBUG_ANY,
			("%s: rate table too small (%u > %u)\n",
			__func__, rt->rateCount, IEEE80211_RATE_MAXSIZE));
		maxrates = IEEE80211_RATE_MAXSIZE;
	} else
		maxrates = rt->rateCount;
	rs = &ic->ic_sup_rates[mode];
	for (i = 0; i < maxrates; i++)
		rs->rs_rates[i] = rt->info[i].dot11Rate;
	rs->rs_nrates = maxrates;
	return 1;
}

void
ath_setcurmode(struct ath_softc *sc, enum ieee80211_phymode mode)
{
	const HAL_RATE_TABLE *rt;
	int i;

	memset(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
	rt = sc->sc_rates[mode];
	KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
	for (i = 0; i < rt->rateCount; i++)
		sc->sc_rixmap[rt->info[i].dot11Rate & IEEE80211_RATE_VAL] = i;
	memset(sc->sc_hwmap, 0, sizeof(sc->sc_hwmap));
	for (i = 0; i < 32; i++)
		sc->sc_hwmap[i] = rt->info[rt->rateCodeToIndex[i]].dot11Rate;
	sc->sc_currates = rt;
	sc->sc_curmode = mode;
}

/*
 * Reset the rate control state for each 802.11 state transition.
 */
void
ath_rate_ctl_reset(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_node *an;

	if (ic->ic_opmode != IEEE80211_M_STA) {
		/*
		 * When operating as a station the node table holds
		 * the AP's that were discovered during scanning.
		 * For any other operating mode we want to reset the
		 * tx rate state of each node.
		 */
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			ni->ni_txrate = 0;		/* use lowest rate */
			an = (struct ath_node *) ni;
			an->an_tx_ok = an->an_tx_err = an->an_tx_retr =
			    an->an_tx_upper = 0;
		}
	}
	/*
	 * Reset local xmit state; this is really only meaningful
	 * when operating in station or adhoc mode.
	 */
	ni = ic->ic_bss;
	an = (struct ath_node *) ni;
	an->an_tx_ok = an->an_tx_err = an->an_tx_retr = an->an_tx_upper = 0;
	if (state == IEEE80211_S_RUN) {
		/* start with highest negotiated rate */
		KASSERT(ni->ni_rates.rs_nrates > 0,
			("transition to RUN state w/ no rates!"));
		ni->ni_txrate = ni->ni_rates.rs_nrates - 1;
	} else {
		/* use lowest rate */
		ni->ni_txrate = 0;
	}
}

/* 
 * Examine and potentially adjust the transmit rate.
 */
void
ath_rate_ctl(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;
	struct ath_node *an = (struct ath_node *) ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int mod = 0, orate, enough;

	/*
	 * Rate control
	 * XXX: very primitive version.
	 */
	sc->sc_stats.ast_rate_calls++;

	enough = (an->an_tx_ok + an->an_tx_err >= 10);

	/* no packet reached -> down */
	if (an->an_tx_err > 0 && an->an_tx_ok == 0)
		mod = -1;

	/* all packets needs retry in average -> down */
	if (enough && an->an_tx_ok < an->an_tx_retr)
		mod = -1;

	/* no error and less than 10% of packets needs retry -> up */
	if (enough && an->an_tx_err == 0 && an->an_tx_ok > an->an_tx_retr * 10)
		mod = 1;

	orate = ni->ni_txrate;
	switch (mod) {
	case 0:
		if (enough && an->an_tx_upper > 0)
			an->an_tx_upper--;
		break;
	case -1:
		if (ni->ni_txrate > 0) {
			ni->ni_txrate--;
			sc->sc_stats.ast_rate_drop++;
		}
		an->an_tx_upper = 0;
		break;
	case 1:
		if (++an->an_tx_upper < 2)
			break;
		an->an_tx_upper = 0;
		if (ni->ni_txrate + 1 < rs->rs_nrates) {
			ni->ni_txrate++;
			sc->sc_stats.ast_rate_raise++;
		}
		break;
	}

	if (ni->ni_txrate != orate) {
		DPRINTF(ATH_DEBUG_RATE,
		    ("%s: %dM -> %dM (%d ok, %d err, %d retr)\n",
		    __func__,
		    (rs->rs_rates[orate] & IEEE80211_RATE_VAL) / 2,
		    (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL) / 2,
		    an->an_tx_ok, an->an_tx_err, an->an_tx_retr));
	}
	if (ni->ni_txrate != orate || enough)
		an->an_tx_ok = an->an_tx_err = an->an_tx_retr = 0;
}

#ifdef AR_DEBUG
#ifdef __FreeBSD__
int
sysctl_hw_ath_dump(SYSCTL_HANDLER_ARGS)
{
	char dmode[64];
	int error;

	strlcpy(dmode, "", sizeof(dmode) - 1);
	dmode[sizeof(dmode) - 1] = '\0';
	error = sysctl_handle_string(oidp, &dmode[0], sizeof(dmode), req);

	if (error == 0 && req->newptr != NULL) {
		struct ifnet *ifp;
		struct ath_softc *sc;

		ifp = ifunit("ath0");		/* XXX */
		if (!ifp)
			return EINVAL;
		sc = ifp->if_softc;
		if (strcmp(dmode, "hal") == 0)
			ath_hal_dumpstate(sc->sc_ah);
		else
			return EINVAL;
	}
	return error;
}
SYSCTL_PROC(_hw_ath, OID_AUTO, dump, CTLTYPE_STRING | CTLFLAG_RW,
	0, 0, sysctl_hw_ath_dump, "A", "Dump driver state");
#endif /* __FreeBSD__ */

#if 0 /* #ifdef __NetBSD__ */
int
sysctl_hw_ath_dump(SYSCTL_HANDLER_ARGS)
{
	char dmode[64];
	int error;

	strlcpy(dmode, "", sizeof(dmode) - 1);
	dmode[sizeof(dmode) - 1] = '\0';
	error = sysctl_handle_string(oidp, &dmode[0], sizeof(dmode), req);

	if (error == 0 && req->newptr != NULL) {
		struct ifnet *ifp;
		struct ath_softc *sc;

		ifp = ifunit("ath0");		/* XXX */
		if (!ifp)
			return EINVAL;
		sc = ifp->if_softc;
		if (strcmp(dmode, "hal") == 0)
			ath_hal_dumpstate(sc->sc_ah);
		else
			return EINVAL;
	}
	return error;
}
SYSCTL_PROC(_hw_ath, OID_AUTO, dump, CTLTYPE_STRING | CTLFLAG_RW,
	0, 0, sysctl_hw_ath_dump, "A", "Dump driver state");
#endif /* __NetBSD__ */

void
ath_printrxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("R%d (%p %p) %08x %08x %08x %08x %08x %08x %c\n",
		    i, ds, (struct ath_desc *)bf->bf_daddr + i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1],
		    !done ? ' ' : (ds->ds_rxstat.rs_status == 0) ? '*' : '!');
	}
}

void
ath_printtxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("T%d (%p %p) %08x %08x %08x %08x %08x %08x %08x %08x %c\n",
		    i, ds, (struct ath_desc *)bf->bf_daddr + i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1], ds->ds_hw[2], ds->ds_hw[3],
		    !done ? ' ' : (ds->ds_txstat.ts_status == 0) ? '*' : '!');
	}
}
#endif /* AR_DEBUG */

#if NGPIO > 0
int
ath_gpio_attach(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct gpiobus_attach_args gba;
	int i;

	if (ah->ah_gpio_npins < 1)
		return 0;
	
	/* Initialize gpio pins array */
	for (i = 0; i < ah->ah_gpio_npins; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT |
		    GPIO_PIN_OUTPUT;

		/* Set pin mode to input */
		ath_hal_gpiocfginput(ah, i);
		sc->sc_gpio_pins[i].pin_flags = GPIO_PIN_INPUT;

		/* Get pin input */
		sc->sc_gpio_pins[i].pin_state = ath_hal_gpioget(ah, i) ?
		    GPIO_PIN_HIGH : GPIO_PIN_LOW;
	}

	/* Create gpio controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = ath_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = ath_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = ath_gpio_pin_ctl;

	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = ah->ah_gpio_npins;

	if (config_found(&sc->sc_dev, &gba, gpiobus_print) == NULL)
		return (ENODEV);

	return (0);
}

int
ath_gpio_pin_read(void *arg, int pin)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	return (ath_hal_gpioget(ah, pin) ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

void
ath_gpio_pin_write(void *arg, int pin, int value)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	ath_hal_gpioset(ah, pin, value ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

void
ath_gpio_pin_ctl(void *arg, int pin, int flags)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;

	if (flags & GPIO_PIN_INPUT)
		ath_hal_gpiocfginput(ah, pin);
	else if (flags & GPIO_PIN_OUTPUT)
		ath_hal_gpiocfgoutput(ah, pin);
}
#endif /* NGPIO */
