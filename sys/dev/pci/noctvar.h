/*	$OpenBSD: noctvar.h,v 1.5 2002/07/16 03:59:17 jason Exp $	*/

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

#define	NOCT_RNG_QLEN		15
#define	NOCT_RNG_ENTRIES	(1 << NOCT_RNG_QLEN)
#define	NOCT_RNG_BUFSIZE	(NOCT_RNG_ENTRIES * sizeof(u_int64_t))

#define	NOCT_PKH_QLEN		15
#define	NOCT_PKH_ENTRIES	(1 << NOCT_PKH_QLEN)
#define	NOCT_PKH_BUFSIZE	(NOCT_PKH_ENTRIES * sizeof(union noct_pkh_cmd))

#define	NOCT_EA_QLEN		15
#define	NOCT_EA_ENTRIES		(1 << NOCT_EA_QLEN)
#define	NOCT_EA_BUFSIZE		(NOCT_EA_ENTRIES * sizeof(struct noct_ea_cmd))

#define	NOCT_BN_CACHE_SIZE	((256) * (128 / 8))

struct noct_workq {
	SIMPLEQ_ENTRY(noct_workq)	q_next;
	struct cryptop			*q_crp;
	bus_dmamap_t			q_dmamap;
	bus_dma_segment_t		q_dmaseg;
	caddr_t				q_buf;
	u_int8_t			q_macbuf[20];
};

struct noct_softc;

struct noct_bnc_sw {
	u_long bn_off;			/* cache offset */
	u_long bn_siz;			/* cache size */
	void (*bn_callback)(struct noct_softc *, u_int32_t, int);
	struct cryptkop *bn_krp;
};

struct noct_softc {
	struct device sc_dv;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_dma_tag_t sc_dmat;
	void *sc_ih;
	u_int sc_ramsize;
	int32_t sc_cid;			/* cryptodev id */

	u_int64_t *sc_rngbuf;
	bus_dmamap_t sc_rngmap;
	struct timeout sc_rngto;
	int sc_rngtick;

	bus_dmamap_t sc_pkhmap;		/* pkh buffer map */
	bus_dmamap_t sc_bnmap;		/* bignumber cache map */
	union noct_pkh_cmd *sc_pkhcmd;	/* pkh command buffers */
	u_int8_t *sc_bncache;		/* bignumber cache buffer */
	u_int32_t sc_pkhwp;		/* pkh write pointer */
	u_int32_t sc_pkhrp;		/* pkh read pointer */
	struct extent *sc_pkh_bn;	/* pkh big number cache usage */
	struct noct_bnc_sw	sc_pkh_bnsw[NOCT_PKH_ENTRIES];

	bus_dmamap_t sc_eamap;		/* ea buffer map */
	u_int32_t sc_eawp;		/* ea write pointer */
	u_int32_t sc_earp;		/* ea read pointer */
	struct noct_ea_cmd *sc_eacmd;	/* ea command buffers */

	SIMPLEQ_HEAD(,noct_workq)	sc_inq;
	SIMPLEQ_HEAD(,noct_workq)	sc_chipq;
	SIMPLEQ_HEAD(,noct_workq)	sc_outq;
};

#define	NOCT_READ_4(sc,r) \
    bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))
#define	NOCT_WRITE_4(sc,r,v) \
    bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (r), (v))

#define	NOCT_READ_8(sc,r) noct_read_8(sc, r)
#define	NOCT_WRITE_8(sc,r,v) noct_write_8(sc, r, v)

#define	NOCT_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define	NOCT_SESSION(sid)	( (sid) & 0x0fffffff)
#define	NOCT_SID(crd, sesn)	(((crd) << 28) | ((sesn) & 0x0fffffff))

#define	NOCT_WAKEUP(sc)	wakeup(&(sc)->sc_eawp)
#define	NOCT_SLEEP(sc)	tsleep(&(sc)->sc_eawp, PWAIT, "noctea", 0)

