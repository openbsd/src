/* $NetBSD: awivar.h,v 1.4 1999/11/09 14:58:07 sommerfeld Exp $ */
/* $OpenBSD: awivar.h,v 1.1 1999/12/16 02:56:56 deraadt Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


enum awi_state {
	AWI_ST_OFF,		/* powered off */
	AWI_ST_SELFTEST,		/* waiting for selftest to complete*/
	AWI_ST_IFTEST,		/* waiting for interface to respond */
	AWI_ST_MIB_GET,		/* fetching MIB variables */
	AWI_ST_MIB_SET,		/* stuffing MIB variables */
	AWI_ST_TXINIT,		/* initializing TX side */
	AWI_ST_RXINIT,		/* initializing RX side */
	AWI_ST_SCAN,		/* hunting for a BSS */
	AWI_ST_SYNCED,		/* synced?  trying to auth.. */
	/* there are probably some missing 802.11 states here.. */
	AWI_ST_AUTHED,		/* authenticated */
	AWI_ST_RUNNING,		/* ready to send user data.. */
	AWI_ST_INSANE,		/* failed to respond.. */
};

#define AWI_FL_CMD_INPROG 		0x0001

#define AWI_SSID_LEN 33

struct awi_bss_binding 
{
	u_int8_t	chanset; /* channel set to use */
	u_int8_t	pattern; /* hop pattern to use */
	u_int8_t	index;	/* index to use */
	u_int8_t	rssi;	/* strenght of this beacon */
	u_int16_t	dwell_time; /* dwell time */
	u_int8_t	bss_timestamp[8]; /* timestamp of this bss */
	u_int8_t	bss_id[6];
	u_int32_t	rxtime;	/* unit's local time */
	u_int8_t	sslen;
	u_int8_t	ssid[AWI_SSID_LEN];
};

#define NBND 4
#define NTXD 4

struct awi_txbd 
{
	u_int32_t	descr;	/* offset to descriptor */
	u_int32_t	frame;	/* offset to frame */
	u_int32_t	len;	/* frame length */	
};

struct awi_softc 
{
	struct device 		sc_dev;
	struct am79c930_softc 	sc_chip;
	struct arpcom		sc_ec;
	int 			sc_enabled;
	enum awi_state		sc_state;
	int			sc_flags;
	void			*sc_ih; /* interrupt handler */
	struct ifnet		*sc_ifp;	/* XXX */
	int			(*sc_enable) __P((struct awi_softc *));
	void			(*sc_disable) __P((struct awi_softc *));
	void			(*sc_completion) __P((struct awi_softc *,
	    u_int8_t));

	struct ifqueue		sc_mgtq;
	
	u_int32_t		sc_txbase;
	u_int32_t		sc_txlen;
	u_int32_t		sc_rxbase;
	u_int32_t		sc_rxlen;

	u_int32_t		sc_rx_data_desc;
	u_int32_t		sc_rx_mgt_desc;

	u_int16_t		sc_scan_duration;
	u_int8_t		sc_scan_chanset;
	u_int8_t		sc_scan_pattern;

	int			sc_nbindings;

	u_int8_t		sc_my_addr[6];
	
	int			sc_new_bss;
	struct awi_bss_binding	sc_active_bss;
	/*
	 * BSS's found during a scan.. XXX doesn't need to be in-line
	 */
	struct awi_bss_binding	sc_bindings[NBND];
	
	int			sc_txpending;
	int			sc_ntxd;
	int			sc_txnext; /* next txd to be given to driver */
	int			sc_txfirst; /* first unsent txd dev has */
	struct awi_txbd	sc_txd[NTXD];
	u_int8_t		sc_curmib;

	int			sc_scan_timer;
	int			sc_tx_timer;
	int			sc_mgt_timer;
	int			sc_cmd_timer;
	int			sc_selftest_tries;

	/*
	 * packet parsing state.
	 */

	struct mbuf		*sc_nextpkt;
	struct mbuf		*sc_m;
	u_int8_t		*sc_mptr;
	u_int32_t		sc_mleft;
	int			sc_flushpkt;
};

extern int awi_activate __P((struct device *, enum devact));
extern int awi_attach __P((struct awi_softc *, u_int8_t *macaddr));
extern void awi_init __P((struct awi_softc *));
extern void awi_stop __P((struct awi_softc *));

#define awi_read_1(sc, off) ((sc)->sc_chip.sc_ops->read_1)(&sc->sc_chip, off)
#define awi_read_2(sc, off) ((sc)->sc_chip.sc_ops->read_2)(&sc->sc_chip, off)
#define awi_read_4(sc, off) ((sc)->sc_chip.sc_ops->read_4)(&sc->sc_chip, off)
#define awi_read_bytes(sc, off, ptr, len) ((sc)->sc_chip.sc_ops->read_bytes)(&sc->sc_chip, off, ptr, len)

#define awi_write_1(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_1)(&sc->sc_chip, off, val)
#define awi_write_2(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_2)(&sc->sc_chip, off, val)
#define awi_write_4(sc, off, val) \
	((sc)->sc_chip.sc_ops->write_4)(&sc->sc_chip, off, val)
#define awi_write_bytes(sc, off, ptr, len) \
	((sc)->sc_chip.sc_ops->write_bytes)(&sc->sc_chip, off, ptr, len)

#define awi_drvstate(sc, state) \
	awi_write_1(sc, AWI_DRIVERSTATE, \
	    ((state) | AWI_DRV_AUTORXLED|AWI_DRV_AUTOTXLED));

/* Number of trips around the loop waiting for the device.. */

#define AWI_LOCKOUT_SPIN	10000 /* 10ms */

/* 24-byte mac header + 8 byte SNAP header + 1500-byte ether MTU */
#define AWI_FRAME_SIZE		1532

/* refresh associations every 300s */
  
#define AWI_ASSOC_REFRESH	300

extern int awi_intr __P((void *));
