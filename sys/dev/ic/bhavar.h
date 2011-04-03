/*	$OpenBSD: bhavar.h,v 1.5 2011/04/03 12:42:36 krw Exp $	*/
/*	$NetBSD: bhavar.h,v 1.12 1998/11/19 21:53:00 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>

/*
 * Mail box defs  etc.
 * these could be bigger but we need the bha_softc to fit on a single page..
 */
#define BHA_MBX_SIZE	32	/* mail box size  (MAX 255 MBxs) */
				/* don't need that many really */
#define BHA_CCB_MAX	32	/* store up to 32 CCBs at one time */
#define	CCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define CCB_HASH(x)	((((long)(x))>>CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define bha_nextmbx(wmb, mbx, mbio) \
	if ((wmb) == &(mbx)->mbio[BHA_MBX_SIZE - 1])	\
		(wmb) = &(mbx)->mbio[0];		\
	else						\
		(wmb)++;

struct bha_mbx {
	struct bha_mbx_out mbo[BHA_MBX_SIZE];
	struct bha_mbx_in mbi[BHA_MBX_SIZE];
	struct bha_mbx_out *cmbo;	/* Collection Mail Box out */
	struct bha_mbx_out *tmbo;	/* Target Mail Box out */
	struct bha_mbx_in *tmbi;	/* Target Mail Box in */
};

struct bha_control {
	struct bha_mbx bc_mbx;		/* all our mailboxes */
	struct bha_ccb bc_ccbs[BHA_CCB_MAX]; /* all our control blocks */
};

struct bha_softc {
	struct device sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap_control;	/* maps the control structures */
	int sc_dmaflags;		/* bus-specific dma map flags */
	void *sc_ih;

	struct bha_control *sc_control;	/* control structures */
	int sc_iswide;

#define	wmbx	(&sc->sc_control->bc_mbx)

	struct bha_ccb *sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, bha_ccb) sc_free_ccb, sc_waiting_ccb;
	int sc_mbofull;
	struct scsi_link sc_link;	/* prototype for devs */
	struct scsi_adapter sc_adapter;

	struct mutex		sc_ccb_mtx;
	struct scsi_iopool	sc_iopool;

	char sc_model[7];
	char sc_firmware[6];
};

/*
 * Offset of a Mail Box In from the beginning of the control DMA mapping.
 */
#define	BHA_MBI_OFF(m)	(offsetof(struct bha_control, bc_mbx.mbi[0]) +	\
			    (((u_long)(m)) - ((u_long)&wmbx->mbi[0])))

/*
 * Offset of a Mail Box Out from the beginning of the control DMA mapping.
 */
#define	BHA_MBO_OFF(m)	(offsetof(struct bha_control, bc_mbx.mbo[0]) +	\
			    (((u_long)(m)) - ((u_long)&wmbx->mbo[0])))

/*
 * Offset of a CCB from the beginning of the control DMA mapping.
 */
#define	BHA_CCB_OFF(c)	(offsetof(struct bha_control, bc_ccbs[0]) +	\
		    (((u_long)(c)) - ((u_long)&sc->sc_control->bc_ccbs[0])))

struct bha_probe_data {
	int sc_irq, sc_drq;
	int sc_scsi_dev;		/* adapters scsi id */
	int sc_iswide;			/* adapter is wide */
};

/*#define	ISWIDE(sc)	(sc->sc_link.max_target >= 8)*/

int	bha_cmd(bus_space_tag_t, bus_space_handle_t, struct bha_softc *,
	    int, u_char *, int, u_char *);
int	bha_find(bus_space_tag_t, bus_space_handle_t,
	    struct bha_probe_data *);
void	bha_attach(struct bha_softc *, struct bha_probe_data *);
int	bha_intr(void *);

int	bha_disable_isacompat(struct bha_softc *);
void	bha_inquire_setup_information(struct bha_softc *);
