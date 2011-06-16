/*	$OpenBSD: cn30xxpipvar.h,v 1.1 2011/06/16 11:22:30 syuu Exp $	*/

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

#ifndef _CN30XXPIPVAR_H_
#define _CN30XXPIPVAR_H_

/* XXX */
struct cn30xxpip_softc {
	int			sc_port;
	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;
	int			sc_tag_type;
	int			sc_receive_group;
	size_t			sc_ip_offset;
#ifdef OCTEON_ETH_DEBUG
	struct evcnt		sc_ev_pipbeperr;
	struct evcnt		sc_ev_pipfeperr;
	struct evcnt		sc_ev_pipskprunt;
	struct evcnt		sc_ev_pipbadtag;
	struct evcnt		sc_ev_pipprtnxa;
	struct evcnt		sc_ev_pippktdrp;
#endif
};

/* XXX */
struct cn30xxpip_attach_args {
	int			aa_port;
	bus_space_tag_t		aa_regt;
	int			aa_tag_type;
	int			aa_receive_group;
	size_t			aa_ip_offset;
};

void			cn30xxpip_init(struct cn30xxpip_attach_args *,
			    struct cn30xxpip_softc **);
void			cn30xxpip_gbl_ctl_debug(struct cn30xxpip_softc *);
int			cn30xxpip_port_config(struct cn30xxpip_softc *);
void			cn30xxpip_prt_cfg_enable(struct cn30xxpip_softc *,
			    uint64_t, int);
void			cn30xxpip_stats(struct cn30xxpip_softc *,
			    struct ifnet *, int);
#ifdef OCTEON_ETH_DEBUG
void			cn30xxpip_int_enable(struct cn30xxpip_softc *, int);
void			cn30xxpip_dump(void);
void			cn30xxpip_dump_regs(void);
void			cn30xxpip_dump_stats(void);
#endif /* OCTEON_ETH_DEBUG */

#ifdef OCTEON_ETH_DEBUG
void			cn30xxpip_int_enable(struct cn30xxpip_softc *, int);
uint64_t		cn30xxpip_int_summary(struct cn30xxpip_softc *);
#endif /* OCTEON_ETH_DEBUG */


#endif
