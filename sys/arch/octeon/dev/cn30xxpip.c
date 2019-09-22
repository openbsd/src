/*	$OpenBSD: cn30xxpip.c,v 1.9 2019/09/22 05:16:05 visa Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_var.h>

#include <machine/octeonvar.h>

#include <octeon/dev/cn30xxgmxreg.h>
#include <octeon/dev/cn30xxpipreg.h>
#include <octeon/dev/cn30xxpipvar.h>

/* XXX */
void
cn30xxpip_init(struct cn30xxpip_attach_args *aa,
    struct cn30xxpip_softc **rsc)
{
	struct cn30xxpip_softc *sc;
	int status;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc == NULL)
		panic("can't allocate memory: %s", __func__);

	sc->sc_port = aa->aa_port;
	sc->sc_regt = aa->aa_regt;
	sc->sc_tag_type = aa->aa_tag_type;
	sc->sc_receive_group = aa->aa_receive_group;
	sc->sc_ip_offset = aa->aa_ip_offset;

	status = bus_space_map(sc->sc_regt, PIP_BASE, PIP_SIZE, 0,
	    &sc->sc_regh);
	if (status != 0)
		panic("can't map %s space", "pip register");

	*rsc = sc;
}

#define	_PIP_RD8(sc, off) \
	bus_space_read_8((sc)->sc_regt, (sc)->sc_regh, (off))
#define	_PIP_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_regt, (sc)->sc_regh, (off), (v))

int
cn30xxpip_port_config(struct cn30xxpip_softc *sc)
{
	uint64_t prt_cfg;
	uint64_t prt_tag;
	uint64_t ip_offset;

	/*
	 * Process the headers and place the IP header in the work queue
	 */
	prt_cfg = 0;
	/* RAWDRP=0; don't allow raw packet drop */
	/* TAGINC=0 */
	/* DYN_RS=0; disable dynamic short buffering */
	/* INST_HDR=0 */
	/* GRP_WAT=0 */
	SET(prt_cfg, (sc->sc_port << 24) & PIP_PRT_CFGN_QOS);
	/* QOS_WAT=0 */
	/* SPARE=0 */
	/* QOS_DIFF=0 */
	/* QOS_VLAN=0 */
	SET(prt_cfg, PIP_PRT_CFGN_CRC_EN);
	SET(prt_cfg, (PIP_PORT_CFG_MODE_L2) & PIP_PRT_CFGN_MODE);
	/* SKIP=0 */

	prt_tag = 0;
	SET(prt_tag, PIP_PRT_TAGN_INC_PRT);
	CLR(prt_tag, PIP_PRT_TAGN_IP6_DPRT);
	CLR(prt_tag, PIP_PRT_TAGN_IP4_DPRT);
	CLR(prt_tag, PIP_PRT_TAGN_IP6_SPRT);
	CLR(prt_tag, PIP_PRT_TAGN_IP4_SPRT);
	CLR(prt_tag, PIP_PRT_TAGN_IP6_NXTH);
	CLR(prt_tag, PIP_PRT_TAGN_IP4_PCTL);
	CLR(prt_tag, PIP_PRT_TAGN_IP6_DST);
	CLR(prt_tag, PIP_PRT_TAGN_IP4_SRC);
	CLR(prt_tag, PIP_PRT_TAGN_IP6_SRC);
	CLR(prt_tag, PIP_PRT_TAGN_IP4_DST);
	SET(prt_tag, PIP_PRT_TAGN_TCP6_TAG_ORDERED);
	SET(prt_tag, PIP_PRT_TAGN_TCP4_TAG_ORDERED);
	SET(prt_tag, PIP_PRT_TAGN_IP6_TAG_ORDERED);
	SET(prt_tag, PIP_PRT_TAGN_IP4_TAG_ORDERED);
	SET(prt_tag, PIP_PRT_TAGN_NON_TAG_ORDERED);
	SET(prt_tag, sc->sc_receive_group & PIP_PRT_TAGN_GRP);

	ip_offset = 0;
	SET(ip_offset, (sc->sc_ip_offset / 8) & PIP_IP_OFFSET_MASK_OFFSET);

	_PIP_WR8(sc, PIP_PRT_CFG0_OFFSET + (8 * sc->sc_port), prt_cfg);
	_PIP_WR8(sc, PIP_PRT_TAG0_OFFSET + (8 * sc->sc_port), prt_tag);
	_PIP_WR8(sc, PIP_IP_OFFSET_OFFSET, ip_offset);

	return 0;
}

void
cn30xxpip_prt_cfg_enable(struct cn30xxpip_softc *sc, uint64_t prt_cfg,
    int enable)
{
	uint64_t tmp;

	tmp = _PIP_RD8(sc, PIP_PRT_CFG0_OFFSET + (8 * sc->sc_port));
	if (enable)
		tmp |= prt_cfg;
	else
		tmp &= ~prt_cfg;
	_PIP_WR8(sc, PIP_PRT_CFG0_OFFSET + (8 * sc->sc_port), tmp);
}

void
cn30xxpip_stats(struct cn30xxpip_softc *sc, struct ifnet *ifp, int gmx_port)
{
	uint64_t tmp, pkts;
	uint64_t pip_stat_ctl;

	if (sc == NULL || ifp == NULL)
		panic("%s: invalid argument. sc=%p, ifp=%p\n", __func__,
			sc, ifp);

	if (gmx_port < 0 || gmx_port >= GMX_PORT_NUNITS) {
		printf("%s: invalid gmx_port %d\n", __func__, gmx_port);
		return;
	}

	pip_stat_ctl = _PIP_RD8(sc, PIP_STAT_CTL_OFFSET);
	_PIP_WR8(sc, PIP_STAT_CTL_OFFSET, pip_stat_ctl | PIP_STAT_CTL_RDCLR);
	tmp = _PIP_RD8(sc, PIP_STAT0_PRT_OFFSET(gmx_port));
	pkts = (tmp & 0xffffffff00000000ULL) >> 32;
	ifp->if_iqdrops += pkts;

	_PIP_WR8(sc, PIP_STAT_CTL_OFFSET, pip_stat_ctl);
}
