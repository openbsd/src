/*	$OpenBSD: isesvar.h,v 1.5 2001/09/21 19:41:13 ho Exp $	*/

/*
 * Copyright (c) 2000 Håkan Olsson (ho@crt.se)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct ises_softc {
	struct device		sc_dv;		/* generic device */
	void			*sc_ih;		/* interrupt handler cookie */
	bus_space_handle_t	sc_memh;	/* memory handle */
	bus_space_tag_t		sc_memt;	/* memory tag */
	bus_dma_tag_t		sc_dmat;	/* dma tag */
	bus_dmamap_t		sc_dmamap;	/* dma xfer map */
	caddr_t			sc_dma_data;	/* data area */

	int32_t			sc_cid;		/* crypto tag */
	u_int32_t		sc_intrmask;	/* interrupt mask */
	u_int32_t		sc_dma_mask;	/* DMA running mask */
	SIMPLEQ_HEAD(,ises_q)	sc_queue;	/* packet queue */
	int			sc_nqueue;	/* count enqueued */
	SIMPLEQ_HEAD(,ises_q)	sc_qchip;	/* on chip */
	struct timeout		sc_timeout;	/* init + hrng timeout */
	int			sc_nsessions;	/* nr of sessions */
	struct ises_session	*sc_sessions;	/* sessions */
	int			sc_cursession;	/* current session */
	int			sc_switching;	/* we're switching sessions */
	int			sc_initstate;	/* card initialization state */

	SIMPLEQ_HEAD(,ises_cmd) sc_cmdq;	/* Commands in A-queue */
	u_int32_t		sc_lnau1_r[64];	/* LNAU 1 result (2048 bits) */
	int			sc_lnau1_rlen;	/* LNAU 1 result len (bytes) */
	u_int32_t		sc_lnau2_r[64];	/* LNAU 2 result (2048 bits) */
	int			sc_lnau2_rlen;	/* LNAU 2 result len (bytes) */
};

union ises_q_u {
	struct mbuf		*mbuf;
	struct uio		*uio;
	/* XXX more ? */
};

#define ISES_MAX_SCATTER	64

struct ises_q {
	SIMPLEQ_ENTRY(ises_q)	q_next;
	struct cryptop		*q_crp;
	struct ises_softc	*q_sc;

	union ises_q_u		q_src, q_dst;	/* src/dst data bufs */

	bus_dma_segment_t	q_src_ds, q_dst_ds;

	struct ises_session	q_session;
	u_int16_t		q_offset;	/* crypto offset */
	int			q_sesn;

#if 0
	long			q_src_packp[ISES_MAX_SCATTER];
	int			q_src_packl[ISES_MAX_SCATTER];
	int			q_src_npa, q_src_l;
#endif

	long			q_dst_packp;
	int			q_dst_packl;
	int			q_dst_npa, q_dst_l;
	u_int32_t		q_macbuf[5];
};

struct ises_cmd {
	SIMPLEQ_ENTRY(ises_cmd)	cmd_next;
	u_int32_t		cmd_code;	/* Command code */
	u_int32_t		cmd_rlen;	/* Response length */
	u_int32_t		cmd_session;	/* Current ises_session */
	u_int32_t		(*cmd_cb)(struct ises_softc *, 
					  struct ises_cmd *); /* Callback */
};

/* Maximum queue length */
#ifndef ISES_MAX_NQUEUE
#define ISES_MAX_NQUEUE		24
#endif
