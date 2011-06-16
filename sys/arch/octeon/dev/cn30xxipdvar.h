/*	$OpenBSD: cn30xxipdvar.h,v 1.1 2011/06/16 11:22:30 syuu Exp $	*/

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

#ifndef _CN30XXIPDVAR_H_
#define _CN30XXIPDVAR_H_

#ifndef SET
#define	SET(t, f)	((t) |= (f))
#define	ISSET(t, f)	((t) & (f))
#define	CLR(t, f)	((t) &= ~(f))
#endif

/* XXX */
struct cn30xxipd_softc {
	int			sc_port;
	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;
	size_t			sc_first_mbuff_skip;
	size_t			sc_not_first_mbuff_skip;
#ifdef OCTEON_ETH_DEBUG
	struct evcnt		sc_ev_ipdbpsub;
	struct evcnt		sc_ev_ipdprcpar3;
	struct evcnt		sc_ev_ipdprcpar2;
	struct evcnt		sc_ev_ipdprcpar1;
	struct evcnt		sc_ev_ipdprcpar0;
#endif
};

/* XXX */
struct cn30xxipd_attach_args {
	int			aa_port;
	bus_space_tag_t		aa_regt;
	size_t			aa_first_mbuff_skip;
	size_t			aa_not_first_mbuff_skip;
};

void			cn30xxipd_init(struct cn30xxipd_attach_args *,
			    struct cn30xxipd_softc **);
int			cn30xxipd_enable(struct cn30xxipd_softc *);
int			cn30xxipd_config(struct cn30xxipd_softc *);
void			cn30xxipd_sub_port_fcs(struct cn30xxipd_softc *, int);
void			cn30xxipd_offload(uint64_t, caddr_t, uint16_t *);

#ifdef OCTEON_ETH_DEBUG
void			cn30xxipd_int_enable(struct cn30xxipd_softc *, int);
uint64_t		cn30xxipd_int_summary(struct cn30xxipd_softc *);
#endif /* OCTEON_ETH_DEBUG */
#ifdef OCTEON_ETH_DEBUG
void			cn30xxipd_int_enable(struct cn30xxipd_softc *, int);
#endif /* OCTEON_ETH_DEBUG */

#endif
