/*	$OpenBSD: cn30xxgmxvar.h,v 1.1 2011/06/16 11:22:30 syuu Exp $	*/

/*
 * Copyright (c) 2007 Internet Initiative Japan, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CN30XXGMXVAR_H_
#define _CN30XXGMXVAR_H_

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#define GMX_MII_PORT	1
#define GMX_GMII_PORT	2
#define GMX_RGMII_PORT	3
#define GMX_SPI42_PORT	4

#define GMX_FRM_MAX_SIZ	0x600

/* Disable 802.3x flow-control when AutoNego is enabled */
#define GMX_802_3X_DISABLE_AUTONEG

#if 1
	/* XXX */
	enum OCTEON_ETH_QUIRKS {
		OCTEON_ETH_QUIRKS_SEILX = 1 << 1,
		OCTEON_ETH_QUIRKS_SEILX1 = 1 << 2,
		OCTEON_ETH_QUIRKS_SEILX2 = 1 << 3,
		OCTEON_ETH_QUIRKS_SEILX2PORT0 = 1 << 4,
		OCTEON_ETH_QUIRKS_L2SWPORT = 1 << 5,
		OCTEON_ETH_QUIRKS_FCS = 1 << 6,
		OCTEON_ETH_QUIRKS_SEILX1_REVB = 1 << 7,
	};
#endif

#if 1
struct cn30xxgmx_softc;
struct cn30xxgmx_port_softc;

struct cn30xxgmx_port_softc {
	struct cn30xxgmx_softc	*sc_port_gmx;
	bus_space_handle_t	sc_port_regh;
	int			sc_port_no;	/* GMX0:0, GMX0:1, ... */
	int			sc_port_type;
	uint64_t		sc_mac;
	int			sc_quirks;
	uint64_t		sc_link;
	struct mii_data		*sc_port_mii;
	struct arpcom		*sc_port_ac;
	struct cn30xxgmx_port_ops
				*sc_port_ops;
	struct cn30xxasx_softc	*sc_port_asx;
	struct cn30xxipd_softc	*sc_ipd;
	int			sc_port_flowflags;

#if defined(OCTEON_DEBUG) || defined(OCTEON_ETH_DEBUG)
#if 0
	/* XXX */
	struct evcnt		sc_ev_pausedrp;
	struct evcnt		sc_ev_phydupx;
	struct evcnt		sc_ev_physpd;
	struct evcnt		sc_ev_phylink;
#endif
	struct evcnt		sc_ev_ifgerr;
	struct evcnt		sc_ev_coldet;
	struct evcnt		sc_ev_falerr;
	struct evcnt		sc_ev_rcverr;
	struct evcnt		sc_ev_rsverr;
	struct evcnt		sc_ev_pckterr;
	struct evcnt		sc_ev_ovrerr;
	struct evcnt		sc_ev_niberr;
	struct evcnt		sc_ev_skperr;
	struct evcnt		sc_ev_lenerr;
	struct evcnt		sc_ev_alnerr;
	struct evcnt		sc_ev_fcserr;
	struct evcnt		sc_ev_jabber;
	struct evcnt		sc_ev_maxerr;
	struct evcnt		sc_ev_carext;
	struct evcnt		sc_ev_minerr;
#endif
};

struct cn30xxgmx_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;
	int			sc_unitno;	/* GMX0, GMX1, ... */
	int			sc_nports;
	int			sc_port_types[4/* XXX */];

	struct cn30xxgmx_port_softc
				*sc_ports;

#if defined(OCTEON_DEBUG) || defined(OCTEON_ETH_DEBUG)
	struct evcnt		sc_ev_latecol;
	struct evcnt		sc_ev_xsdef;
	struct evcnt		sc_ev_xscol;
	struct evcnt		sc_ev_undflw;
	struct evcnt		sc_ev_pkonxa;
#endif
};
#endif

struct cn30xxgmx_attach_args {
        bus_space_tag_t         ga_regt;
	bus_addr_t		ga_addr;
	bus_dma_tag_t		ga_dmat;
	const char		*ga_name;
	int			ga_portno;
	int			ga_port_type;

	struct cn30xxgmx_softc *ga_gmx;
	struct cn30xxgmx_port_softc
				*ga_gmx_port;
};

#define	CN30XXGMX_FILTER_NADDRS_MAX	8	/* XXX elsewhere */

enum CN30XXGMX_FILTER_POLICY {
	CN30XXGMX_FILTER_POLICY_ACCEPT_ALL,
	CN30XXGMX_FILTER_POLICY_ACCEPT,
	CN30XXGMX_FILTER_POLICY_REJECT,
	CN30XXGMX_FILTER_POLICY_REJECT_ALL
};

int		cn30xxgmx_link_enable(struct cn30xxgmx_port_softc *, int);
int		cn30xxgmx_tx_stats_rd_clr(struct cn30xxgmx_port_softc *, int);
int		cn30xxgmx_rx_stats_rd_clr(struct cn30xxgmx_port_softc *, int);
void		cn30xxgmx_rx_stats_dec_bad(struct cn30xxgmx_port_softc *);
int		cn30xxgmx_stats_init(struct cn30xxgmx_port_softc *);
void		cn30xxgmx_tx_int_enable(struct cn30xxgmx_port_softc *, int);
void		cn30xxgmx_rx_int_enable(struct cn30xxgmx_port_softc *, int);
int		cn30xxgmx_setfilt(struct cn30xxgmx_port_softc *,
		    enum CN30XXGMX_FILTER_POLICY, size_t, uint8_t **);
int		cn30xxgmx_rx_frm_ctl_enable(struct cn30xxgmx_port_softc *,
		    uint64_t rx_frm_ctl);
int		cn30xxgmx_rx_frm_ctl_disable(struct cn30xxgmx_port_softc *,
		    uint64_t rx_frm_ctl);
int		cn30xxgmx_tx_thresh(struct cn30xxgmx_port_softc *, int);
int		cn30xxgmx_set_mac_addr(struct cn30xxgmx_port_softc *, uint8_t *);
int		cn30xxgmx_set_filter(struct cn30xxgmx_port_softc *);
int		cn30xxgmx_port_enable(struct cn30xxgmx_port_softc *, int);
int		cn30xxgmx_reset_speed(struct cn30xxgmx_port_softc *);
int		cn30xxgmx_reset_flowctl(struct cn30xxgmx_port_softc *);
int		cn30xxgmx_reset_timing(struct cn30xxgmx_port_softc *);
int		cn30xxgmx_reset_board(struct cn30xxgmx_port_softc *);
void		cn30xxgmx_stats(struct cn30xxgmx_port_softc *);
uint64_t	cn30xxgmx_get_rx_int_reg(struct cn30xxgmx_port_softc *sc);
uint64_t	cn30xxgmx_get_tx_int_reg(struct cn30xxgmx_port_softc *sc);
static inline int	cn30xxgmx_link_status(struct cn30xxgmx_port_softc *);

/* XXX RGMII specific */
static inline int
cn30xxgmx_link_status(struct cn30xxgmx_port_softc *sc)
{
	return (sc->sc_link & RXN_RX_INBND_STATUS) ? 1 : 0;
}

#endif
