/*	$OpenBSD: lofnvar.h,v 1.8 2002/09/24 18:33:26 jason Exp $	*/

/*
 * Copyright (c) 2001-2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#define	LOFN_RNGBUF_SIZE	8	/* size in 32 bit elements */

struct lofn_softc {
	struct device		sc_dv;
	void *			sc_ih;
	bus_space_handle_t	sc_sh;
	bus_space_tag_t		sc_st;
	bus_dma_tag_t		sc_dmat;
	u_int32_t		sc_rngbuf[LOFN_RNGBUF_SIZE], sc_ier;
	int32_t			sc_cid;
	union lofn_reg		sc_tmp;
	union lofn_reg		sc_zero;
	SIMPLEQ_HEAD(,lofn_q)	sc_queue;
	struct lofn_q		*sc_current;
};

struct lofn_q {
	SIMPLEQ_ENTRY(lofn_q) q_next;
	int (*q_start)(struct lofn_softc *, struct lofn_q *);
	void (*q_finish)(struct lofn_softc *, struct lofn_q *);
	struct cryptkop *q_krp;
};

#define	READ_REG(sc,r)		\
    bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))
#define	READ_REG_0(sc,r)	READ_REG((sc), (r) | LOFN_WIN_0)
#define	READ_REG_1(sc,r)	READ_REG((sc), (r) | LOFN_WIN_1)
#define	READ_REG_2(sc,r)	READ_REG((sc), (r) | LOFN_WIN_2)
#define	READ_REG_3(sc,r)	READ_REG((sc), (r) | LOFN_WIN_3)

#define	WRITE_REG(sc,r,v)	\
    bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (r), (v))
#define	WRITE_REG_0(sc,r,v)	WRITE_REG((sc), (r) | LOFN_WIN_0, (v))
#define	WRITE_REG_1(sc,r,v)	WRITE_REG((sc), (r) | LOFN_WIN_1, (v))
#define	WRITE_REG_2(sc,r,v)	WRITE_REG((sc), (r) | LOFN_WIN_2, (v))
#define	WRITE_REG_3(sc,r,v)	WRITE_REG((sc), (r) | LOFN_WIN_3, (v))

#ifndef LOFN_RNG_SCALAR
#define	LOFN_RNG_SCALAR		0x00000700
#endif

/* C = M ^ E mod N */
#define	LOFN_MODEXP_PAR_M	0
#define	LOFN_MODEXP_PAR_E	1
#define	LOFN_MODEXP_PAR_N	2
