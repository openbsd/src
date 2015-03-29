/*	$OpenBSD: qecvar.h,v 1.12 2015/03/29 10:59:47 mpi Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct qec_softc {
	struct	device	sc_dev;		/* us as a device */
	struct	qecregs	*sc_regs;	/* QEC registers */
	int		sc_node;	/* PROM node ID */
	int		sc_burst;	/* DVMA burst size in effect */
	int		sc_bufsiz;	/* Size of buffer */
	int		sc_pri;		/* interrupt priority */
	int		sc_nchannels;	/* number of channels on board */
	int		sc_nrange;	/* number of ranges */
	struct	rom_range *sc_range;	/* array of ranges */
	void		*sc_paddr;

	/*
	 * For use by children:
	 */
	u_int32_t	sc_msize;	/* qec buffer offset per channel */
	u_int32_t	sc_rsize;	/* qec buffer size for receive */
};

void	qec_reset(struct qec_softc *);
int	qec_put(u_int8_t *, struct mbuf *);
struct mbuf *qec_get(u_int8_t *, int);
