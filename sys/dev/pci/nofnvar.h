/*	$OpenBSD: nofnvar.h,v 1.5 2002/09/24 18:33:26 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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

struct nofn_softc {
	struct device sc_dev;
	void *sc_ih;
	bus_space_handle_t sc_sh;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_pk_h;
	bus_space_tag_t sc_pk_t;
	bus_dma_tag_t sc_dmat;
	int32_t sc_cid;
	int sc_flags;
	int sc_rngskip, sc_rngtick;
	struct timeout sc_rngto;
	u_int32_t sc_intrmask, sc_revid;
	SIMPLEQ_HEAD(,nofn_pk_q) sc_pk_queue;
	struct nofn_pk_q *sc_pk_current;
	union nofn_pk_reg sc_pk_tmp, sc_pk_zero;
};

struct nofn_pk_q {
	SIMPLEQ_ENTRY(nofn_pk_q)	q_next;
	int (*q_start)(struct nofn_softc *, struct nofn_pk_q *);
	void (*q_finish)(struct nofn_softc *, struct nofn_pk_q *);
	struct cryptkop *q_krp;
};

#define	NOFN_FLAGS_RNG		0x01
#define	NOFN_FLAGS_PK		0x02

#define	REG_WRITE_4(sc,r,v) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (r), (v))
#define	REG_READ_4(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define	PK_WRITE_4(sc,r,v) \
	bus_space_write_4((sc)->sc_pk_t, (sc)->sc_pk_h, (r), (v))
#define	PK_READ_4(sc,r) \
	bus_space_read_4((sc)->sc_pk_t, (sc)->sc_pk_h, (r))

#ifndef PK_RNC_SCALER
#define PK_RNC_SCALER		0x00000700
#endif

/* C = M ^ E mod N */
#define	NOFN_MODEXP_PAR_M	0
#define	NOFN_MODEXP_PAR_E	1
#define	NOFN_MODEXP_PAR_N	2
