/*	$OpenBSD: fxpvar.h,v 1.5 2001/06/13 23:19:17 jason Exp $	*/
/*	$NetBSD: if_fxpvar.h,v 1.1 1997/06/05 02:01:58 thorpej Exp $	*/

/*                  
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *              
 * Modifications to support NetBSD:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *                  
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:             
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.  
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      Id: if_fxpvar.h,v 1.6 1998/08/02 00:29:15 dg Exp
 */

/*
 * Misc. defintions for the Intel EtherExpress Pro/100B PCI Fast
 * Ethernet driver
 */
/*
 * NOTE: Elements are ordered for optimal cacheline behavior, and NOT
 *	 for functional grouping.
 */
struct fxp_softc {
	struct device sc_dev;		/* generic device structures */
	void *sc_ih;			/* interrupt handler cookie */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	struct arpcom arpcom;		/* per-interface network data */
	struct mii_data sc_mii;		/* MII media information */
	struct mbuf *rfa_headm;		/* first mbuf in receive frame area */
	struct mbuf *rfa_tailm;		/* last mbuf in receive frame area */
	struct fxp_cb_tx *cbl_first;	/* first active TxCB in list */
	int sc_flags;			/* misc. flags */
#define	FXPF_HAS_RESUME_BUG	0x08	/* has the resume bug */
#define	FXPF_FIX_RESUME_BUG	0x10	/* currently need to work-around
					   the resume bug */
	int tx_queued;			/* # of active TxCB's */
	int need_mcsetup;		/* multicast filter needs programming */
	struct fxp_cb_tx *cbl_last;	/* last active TxCB in list */
	struct fxp_stats *fxp_stats;	/* Pointer to interface stats */
	struct timeout stats_update_to; /* Pointer to timeout structure */
	int rx_idle_secs;		/* # of seconds RX has been idle */
	struct fxp_cb_tx *cbl_base;	/* base of TxCB list */
	struct fxp_cb_mcs *mcsp;	/* Pointer to mcast setup descriptor */
	int all_mcasts;			/* receive all multicasts */
	int phy_primary_addr;		/* address of primary PHY */
	int phy_primary_device;		/* device type of primary PHY */
	int phy_10Mbps_only;		/* PHY is 10Mbps-only device */
	int eeprom_size;		/* size of serial EEPROM */
	int not_82557;			/* yes if we are 82558/82559 */
	void *sc_sdhook;		/* shutdownhook */
	void *sc_powerhook;		/* powerhook */
};

/* Macros to ease CSR access. */
#define	CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

extern int fxp_intr __P((void *));
extern int fxp_attach_common __P((struct fxp_softc *, u_int8_t *, const char *));
extern int fxp_detach __P((struct fxp_softc *));
