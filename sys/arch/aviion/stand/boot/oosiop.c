/* $OpenBSD: oosiop.c,v 1.2 2013/10/09 20:03:05 miod Exp $ */
/* OpenBSD: oosiop.c,v 1.20 2013/10/09 18:22:06 miod Exp */
/* OpenBSD: oosiopvar.h,v 1.5 2011/04/03 12:42:36 krw Exp */
/* $NetBSD: oosiop.c,v 1.4 2003/10/29 17:45:55 tsutsui Exp $ */
/* $NetBSD: oosiopvar.h,v 1.2 2003/05/03 18:11:23 wiz Exp $ */

/*
 * Copyright (c) 2001 Shuichiro URATA.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

#include <sys/param.h>
#define offsetof(s, e) ((size_t)&((s *)0)->e)

#include <stand.h>
#include "libsa.h"

#include "scsi.h"

#include <dev/ic/oosiopreg.h>

#include "oosiop.h"

#define	OOSIOP_NTGT	8		/* Max targets */

struct oosiop_xfer {
	/* script for DMA (move*nsg+jump) */
	uint32_t datain_scr[2 * 2];
	uint32_t dataout_scr[2 * 2];

	u_int8_t msgin[8];
	u_int8_t msgout[8];
	u_int8_t status;
};

#define	SCSI_OOSIOP_NOSTATUS	0xff	/* device didn't report status */

struct oosiop_cb {
	int flags;

	int id;				/* target scsi id */
	int lun;			/* target lun */

	int curdp;			/* current data pointer */
	int savedp;			/* saved data pointer */
	int msgoutlen;

	int xsflags;			/* copy of xs->flags */
	int datalen;			/* copy of xs->datalen */
	uint32_t databuf;
	int cmdlen;			/* copy of xs->cmdlen */
	uint32_t cmdbuf;

	struct oosiop_xfer *xfer;	/* DMA xfer block */

	int status;
	int error;
};

/* oosiop_cb flags */
#define	CBF_SELTOUT	0x01	/* Selection timeout */
#define	CBF_TIMEOUT	0x02	/* Command timeout */

struct oosiop_softc {
	uint32_t	sc_baseaddr;
	uint32_t *sc_scr;		/* ptr to script memory */

	int	sc_tgtid;
	int	sc_tgtlun;

	int		sc_id;		/* SCSI ID of this interface */
	int		sc_freq;	/* SCLK frequency */
	int		sc_ccf;		/* asynchronous divisor (*10) */
	u_int8_t	sc_dcntl;

	int sc_pending;

	struct oosiop_xfer sc_xfer;
	struct oosiop_cb sc_cb;

	struct oosiop_cb *sc_nexus;
	struct oosiop_cb *sc_curcb;	/* current command */

	uint32_t sc_reselbuf;		/* msgin buffer for reselection */
	int sc_resid;			/* reselected target id */

	int sc_nextdsp;

	uint8_t	sc_scntl0;
	uint8_t sc_dmode;
	uint8_t sc_dwt;
	uint8_t sc_ctest7;
};

#define	oosiop_read_1(sc, addr)						\
    *(volatile uint8_t *)((sc)->sc_baseaddr + (addr))
#define	oosiop_write_1(sc, addr, data)					\
    *(volatile uint8_t *)((sc)->sc_baseaddr + (addr)) = (data)
#define	oosiop_read_4(sc, addr)						\
    letoh32(*(volatile uint32_t *)((sc)->sc_baseaddr + (addr)))
#define	oosiop_write_4(sc, addr, data)					\
    *(volatile uint32_t *)((sc)->sc_baseaddr + (addr)) = htole32(data)

/* 53C700 script */
#include <dev/microcode/siop/oosiop.out>

static __inline void oosiop_relocate_io(struct oosiop_softc *, uint32_t);
static __inline void oosiop_relocate_tc(struct oosiop_softc *, uint32_t);
static __inline void oosiop_fixup_select(struct oosiop_softc *, uint32_t,
		         int);
static __inline void oosiop_fixup_jump(struct oosiop_softc *, uint32_t,
		         uint32_t);
static __inline void oosiop_fixup_move(struct oosiop_softc *, uint32_t,
		         uint32_t, uint32_t);

void	oosiop_load_script(struct oosiop_softc *);
void	oosiop_setup_sgdma(struct oosiop_softc *, struct oosiop_cb *);
void	oosiop_setup_dma(struct oosiop_softc *);
void	oosiop_flush_fifo(struct oosiop_softc *);
void	oosiop_clear_fifo(struct oosiop_softc *);
void	oosiop_phasemismatch(struct oosiop_softc *);
static __inline void oosiop_setup_syncxfer(struct oosiop_softc *);
void	oosiop_done(struct oosiop_softc *, struct oosiop_cb *);
void	oosiop_reset(struct oosiop_softc *);
void	oosiop_reset_bus(struct oosiop_softc *);
void	oosiop_scriptintr(struct oosiop_softc *);
void	oosiop_msgin(struct oosiop_softc *, struct oosiop_cb *);
void	oosiop_setup(struct oosiop_softc *, struct oosiop_cb *);
void	oosiop_processintr(struct oosiop_softc *, u_int8_t);

/* Trap interrupt code for unexpected data I/O */
#define	DATAIN_TRAP	0xdead0001
#define	DATAOUT_TRAP	0xdead0002

void *
oosiop_attach(uint32_t addr, int id, int lun)
{
	struct oosiop_softc *sc;

	sc = (struct oosiop_softc *)alloc(sizeof *sc);
	if (sc == NULL)
		return NULL;

	/* XXX run badaddr? */
	memset(sc, 0, sizeof *sc);
	sc->sc_baseaddr = addr;
	sc->sc_scr = (uint32_t *)alloc(sizeof(oosiop_script));

	/* XXX */
	sc->sc_id = 7;
	sc->sc_freq = 33333333;
	sc->sc_scntl0 = OOSIOP_SCNTL0_EPC | OOSIOP_SCNTL0_EPG;
	sc->sc_dmode = OOSIOP_DMODE_BL_4;
	sc->sc_dwt = 0x4f;
	sc->sc_ctest7 = OOSIOP_CTEST7_DC;
	/* XXX */

	/* Setup asynchronous clock divisor parameters */
	if (sc->sc_freq <= 25000000) {
		sc->sc_ccf = 10;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_1;
	} else if (sc->sc_freq <= 37500000) {
		sc->sc_ccf = 15;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_1_5;
	} else if (sc->sc_freq <= 50000000) {
		sc->sc_ccf = 20;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_2;
	} else {
		sc->sc_ccf = 30;
		sc->sc_dcntl = OOSIOP_DCNTL_CF_3;
	}

	sc->sc_cb.xfer = &sc->sc_xfer;
	sc->sc_reselbuf = (uint32_t)&sc->sc_xfer.msgin;

	/*
	 * Reset all
	 */
	oosiop_reset(sc);
	oosiop_reset_bus(sc);

	/*
	 * Start SCRIPTS processor
	 */
	oosiop_load_script(sc);
	oosiop_write_4(sc, OOSIOP_DSP, (uint32_t)sc->sc_scr + Ent_wait_reselect);

	sc->sc_tgtid = id;
	sc->sc_tgtlun = lun;

	return sc;
}

void
oosiop_detach(void *cookie)
{
	struct oosiop_softc *sc = cookie;

	free(sc->sc_scr, sizeof(oosiop_script));
	free(sc, sizeof *sc);
}

static __inline void
oosiop_relocate_io(struct oosiop_softc *sc, uint32_t addr)
{
	uint32_t dcmd;
	int32_t dsps;

	dcmd = letoh32(sc->sc_scr[addr / 4 + 0]);
	dsps = letoh32(sc->sc_scr[addr / 4 + 1]);

	/* convert relative to absolute */
	if (dcmd & 0x04000000) {
		dcmd &= ~0x04000000;
#if 0
		/*
		 * sign extension isn't needed here because
		 * ncr53cxxx.c generates 32 bit dsps.
		 */
		dsps <<= 8;
		dsps >>= 8;
#endif
		sc->sc_scr[addr / 4 + 0] = htole32(dcmd);
		dsps += addr + 8;
	}

	sc->sc_scr[addr / 4 + 1] = htole32(dsps + (uint32_t)sc->sc_scr);
}

static __inline void
oosiop_relocate_tc(struct oosiop_softc *sc, uint32_t addr)
{
	uint32_t dcmd;
	int32_t dsps;

	dcmd = letoh32(sc->sc_scr[addr / 4 + 0]);
	dsps = letoh32(sc->sc_scr[addr / 4 + 1]);

	/* convert relative to absolute */
	if (dcmd & 0x00800000) {
		dcmd &= ~0x00800000;
		sc->sc_scr[addr / 4] = htole32(dcmd);
#if 0
		/*
		 * sign extension isn't needed here because
		 * ncr53cxxx.c generates 32 bit dsps.
		 */
		dsps <<= 8;
		dsps >>= 8;
#endif
		dsps += addr + 8;
	}

	sc->sc_scr[addr / 4 + 1] = htole32(dsps + (uint32_t)sc->sc_scr);
}

static __inline void
oosiop_fixup_select(struct oosiop_softc *sc, uint32_t addr, int id)
{
	uint32_t dcmd;

	dcmd = letoh32(sc->sc_scr[addr / 4]);
	dcmd &= 0xff00ffff;
	dcmd |= 0x00010000 << id;
	sc->sc_scr[addr / 4] = htole32(dcmd);
}

static __inline void
oosiop_fixup_jump(struct oosiop_softc *sc, uint32_t addr, uint32_t dst)
{

	sc->sc_scr[addr / 4 + 1] = htole32(dst);
}

static __inline void
oosiop_fixup_move(struct oosiop_softc *sc, uint32_t addr, uint32_t dbc,
    uint32_t dsps)
{
	uint32_t dcmd;

	dcmd = letoh32(sc->sc_scr[addr / 4]);
	dcmd &= 0xff000000;
	dcmd |= dbc & 0x00ffffff;
	sc->sc_scr[addr / 4 + 0] = htole32(dcmd);
	sc->sc_scr[addr / 4 + 1] = htole32(dsps);
}

void
oosiop_load_script(struct oosiop_softc *sc)
{
	int i;

	/* load script */
	for (i = 0; i < sizeof(oosiop_script) / sizeof(oosiop_script[0]); i++)
		sc->sc_scr[i] = htole32(oosiop_script[i]);

	/* relocate script */
	for (i = 0; i < (sizeof(oosiop_script) / 8); i++) {
		switch (oosiop_script[i * 2] >> 27) {
		case 0x08:	/* select */
		case 0x0a:	/* wait reselect */
			oosiop_relocate_io(sc, i * 8);
			break;
		case 0x10:	/* jump */
		case 0x11:	/* call */
			oosiop_relocate_tc(sc, i * 8);
			break;
		}
	}

	oosiop_fixup_move(sc, Ent_p_resel_msgin_move, 1, sc->sc_reselbuf);
}

void
oosiop_setup_sgdma(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	struct oosiop_xfer *xfer = cb->xfer;
	int n, off;

	off = cb->curdp;

	if (cb->xsflags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		/* build MOVE block */
		if (cb->xsflags & SCSI_DATA_IN) {
			n = 0;
			xfer->datain_scr[n * 2 + 0] =
			    htole32(0x09000000 | (cb->datalen - off));
			xfer->datain_scr[n * 2 + 1] =
			    htole32(cb->databuf + off);
			n++;
			xfer->datain_scr[n * 2 + 0] = htole32(0x80080000);
			xfer->datain_scr[n * 2 + 1] =
			    htole32((uint32_t)sc->sc_scr + Ent_phasedispatch);
		}
		if (cb->xsflags & SCSI_DATA_OUT) {
			n = 0;
			xfer->dataout_scr[n * 2 + 0] =
			    htole32(0x08000000 | (cb->datalen - off));
			xfer->dataout_scr[n * 2 + 1] =
			    htole32(cb->databuf + off);
			n++;
			xfer->dataout_scr[n * 2 + 0] = htole32(0x80080000);
			xfer->dataout_scr[n * 2 + 1] =
			    htole32((uint32_t)sc->sc_scr + Ent_phasedispatch);
		}
	}
	if ((cb->xsflags & SCSI_DATA_IN) == 0) {
		xfer->datain_scr[0] = htole32(0x98080000);
		xfer->datain_scr[1] = htole32(DATAIN_TRAP);
	}
	if ((cb->xsflags & SCSI_DATA_OUT) == 0) {
		xfer->dataout_scr[0] = htole32(0x98080000);
		xfer->dataout_scr[1] = htole32(DATAOUT_TRAP);
	}
}

/*
 * Setup DMA pointer into script.
 */
void
oosiop_setup_dma(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t xferbase;

	cb = sc->sc_curcb;
	xferbase = (uint32_t)cb->xfer;

	oosiop_fixup_select(sc, Ent_p_select, cb->id);
	oosiop_fixup_jump(sc, Ent_p_datain_jump, xferbase + 
	    offsetof(struct oosiop_xfer, datain_scr[0]));
	oosiop_fixup_jump(sc, Ent_p_dataout_jump, xferbase +
	    offsetof(struct oosiop_xfer, dataout_scr[0]));
	oosiop_fixup_move(sc, Ent_p_msgin_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, msgin[0]));
	oosiop_fixup_move(sc, Ent_p_extmsglen_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, msgin[1]));
	oosiop_fixup_move(sc, Ent_p_msgout_move, cb->msgoutlen, xferbase +
	    offsetof(struct oosiop_xfer, msgout[0]));
	oosiop_fixup_move(sc, Ent_p_status_move, 1, xferbase +
	    offsetof(struct oosiop_xfer, status));
	oosiop_fixup_move(sc, Ent_p_cmdout_move, cb->cmdlen, cb->cmdbuf);
}

void
oosiop_flush_fifo(struct oosiop_softc *sc)
{

	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) |
	    OOSIOP_DFIFO_FLF);
	while ((oosiop_read_1(sc, OOSIOP_CTEST1) & OOSIOP_CTEST1_FMT) !=
	    OOSIOP_CTEST1_FMT)
		;
	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) &
	    ~OOSIOP_DFIFO_FLF);
}

void
oosiop_clear_fifo(struct oosiop_softc *sc)
{

	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) |
	    OOSIOP_DFIFO_CLF);
	while ((oosiop_read_1(sc, OOSIOP_CTEST1) & OOSIOP_CTEST1_FMT) !=
	    OOSIOP_CTEST1_FMT)
		;
	oosiop_write_1(sc, OOSIOP_DFIFO, oosiop_read_1(sc, OOSIOP_DFIFO) &
	    ~OOSIOP_DFIFO_CLF);
}

void
oosiop_phasemismatch(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t dsp, dbc, n, i, len;
	u_int8_t dfifo, sstat1;

	cb = sc->sc_curcb;
	if (cb == NULL)
		return;

	dsp = oosiop_read_4(sc, OOSIOP_DSP);
	dbc = oosiop_read_4(sc, OOSIOP_DBC) & OOSIOP_DBC_MAX;
	len = 0;

	n = dsp - (uint32_t)cb->xfer - 8;
	if (n >= offsetof(struct oosiop_xfer, datain_scr[0]) &&
	    n < offsetof(struct oosiop_xfer, datain_scr[1 * 2])) {
		n -= offsetof(struct oosiop_xfer, datain_scr[0]);
		n >>= 3;
		for (i = 0; i <= n; i++)
			len += letoh32(cb->xfer->datain_scr[i * 2]) &
			    0x00ffffff;
		/* All data in the chip are already flushed */
	} else if (n >= offsetof(struct oosiop_xfer, dataout_scr[0]) &&
	    n < offsetof(struct oosiop_xfer, dataout_scr[1 * 2])) {
		n -= offsetof(struct oosiop_xfer, dataout_scr[0]);
		n >>= 3;
		for (i = 0; i <= n; i++)
			len += letoh32(cb->xfer->dataout_scr[i * 2]) &
			    0x00ffffff;

		dfifo = oosiop_read_1(sc, OOSIOP_DFIFO);
		dbc += ((dfifo & OOSIOP_DFIFO_BO) - (dbc & OOSIOP_DFIFO_BO)) &
		    OOSIOP_DFIFO_BO;

		sstat1 = oosiop_read_1(sc, OOSIOP_SSTAT1);
		if (sstat1 & OOSIOP_SSTAT1_OLF)
			dbc++;

		oosiop_clear_fifo(sc);
	} else {
#if 0	/* happens too many times in the polling driver */
		printf("ncsc: phase mismatch addr=%p\n",
		    oosiop_read_4(sc, OOSIOP_DSP) - 8);
#endif
		oosiop_clear_fifo(sc);
		return;
	}

	len -= dbc;
	if (len) {
		cb->curdp += len;
		oosiop_setup_sgdma(sc, cb);
	}
}

static __inline void
oosiop_setup_syncxfer(struct oosiop_softc *sc)
{
	oosiop_write_1(sc, OOSIOP_SXFER, 0);
}

int
oosiop_scsicmd(void *cookie, void *cmdbuf, size_t cmdlen, void *databuf,
    size_t datalen, size_t *resid)
{
	struct oosiop_softc *sc = cookie;
	struct oosiop_cb *cb = &sc->sc_cb;
	struct oosiop_xfer *xfer;

	if (resid != NULL)
		*resid = 0;
	cb->xsflags = datalen != 0 ? SCSI_DATA_IN : 0;	/* XXX */
	cb->cmdbuf = (uint32_t)cmdbuf;
	cb->cmdlen = cmdlen;
	cb->datalen = 0;
	cb->flags = 0;
	cb->id = sc->sc_tgtid;
	cb->lun = sc->sc_tgtlun;
	xfer = cb->xfer;

	/* Setup data buffer DMA */
	if (cb->xsflags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		cb->datalen = datalen;
		cb->databuf = (uint32_t)databuf;
	}

	xfer->status = SCSI_OOSIOP_NOSTATUS;

	oosiop_setup(sc, cb);
	sc->sc_pending = 1;

	/* Abort script to start selection */
	oosiop_write_1(sc, OOSIOP_ISTAT, OOSIOP_ISTAT_ABRT);

	for (;;) {
		u_int8_t istat;

		while (((istat = oosiop_read_1(sc, OOSIOP_ISTAT)) &
		    (OOSIOP_ISTAT_SIP | OOSIOP_ISTAT_DIP)) == 0) {
			delay(1000);
		}
		oosiop_processintr(sc, istat);

		if (cb->xsflags & ITSDONE)
			break;
	}

	if (resid != NULL && cb->error == 0)
		*resid = datalen;
	return cb->error;
}

void
oosiop_setup(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	struct oosiop_xfer *xfer = cb->xfer;

	cb->curdp = 0;
	cb->savedp = 0;

	oosiop_setup_sgdma(sc, cb);

	/* Setup msgout buffer */
	xfer->msgout[0] = MSG_IDENTIFY(cb->lun,
	    (((struct scsi_generic *)cb->cmdbuf)->opcode != REQUEST_SENSE));
	cb->msgoutlen = 1;
}

void
oosiop_done(struct oosiop_softc *sc, struct oosiop_cb *cb)
{

	cb->status = cb->xfer->status;

	if (cb->flags & CBF_SELTOUT)
		cb->error = XS_SELTIMEOUT;
	else if (cb->flags & CBF_TIMEOUT)
		cb->error = XS_TIMEOUT;
	else switch (cb->status) {
	case SCSI_OK:
		cb->error = XS_NOERROR;
		break;

	case SCSI_BUSY:
		cb->error = XS_BUSY;
		break;
	case SCSI_CHECK:
		cb->error = XS_DRIVER_STUFFUP;
		break;
	case SCSI_OOSIOP_NOSTATUS:
		/* the status byte was not updated, cmd was aborted. */
		cb->error = XS_SELTIMEOUT;
		break;

	default:
		cb->error = XS_RESET;
		break;
	}

	/* Put it on the free list. */
	cb->xsflags |= ITSDONE;

	if (cb == sc->sc_curcb)
		sc->sc_curcb = NULL;
	sc->sc_nexus = NULL;
}

void
oosiop_reset(struct oosiop_softc *sc)
{
	/* Stop SCRIPTS processor */
	oosiop_write_1(sc, OOSIOP_ISTAT, OOSIOP_ISTAT_ABRT);
	delay(100);
	oosiop_write_1(sc, OOSIOP_ISTAT, 0);

	/* Reset the chip */
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl | OOSIOP_DCNTL_RST);
	delay(100);
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl);
	delay(10000);

	/* Set up various chip parameters */
	oosiop_write_1(sc, OOSIOP_SCNTL0, OOSIOP_ARB_FULL | sc->sc_scntl0);
	oosiop_write_1(sc, OOSIOP_SCNTL1, OOSIOP_SCNTL1_ESR);
	oosiop_write_1(sc, OOSIOP_DCNTL, sc->sc_dcntl);
	oosiop_write_1(sc, OOSIOP_DMODE, sc->sc_dmode);
	oosiop_write_1(sc, OOSIOP_SCID, OOSIOP_SCID_VALUE(sc->sc_id));
	oosiop_write_1(sc, OOSIOP_DWT, sc->sc_dwt);
	oosiop_write_1(sc, OOSIOP_CTEST7, sc->sc_ctest7);
	oosiop_write_1(sc, OOSIOP_SXFER, 0);

	/* Clear all interrupts */
	(void)oosiop_read_1(sc, OOSIOP_SSTAT0);
	(void)oosiop_read_1(sc, OOSIOP_SSTAT1);
	(void)oosiop_read_1(sc, OOSIOP_DSTAT);

	/* Enable interrupts */
	oosiop_write_1(sc, OOSIOP_SIEN,
	    OOSIOP_SIEN_M_A | OOSIOP_SIEN_STO | OOSIOP_SIEN_SGE |
	    OOSIOP_SIEN_UDC | OOSIOP_SIEN_RST | OOSIOP_SIEN_PAR);
	oosiop_write_1(sc, OOSIOP_DIEN,
	    OOSIOP_DIEN_ABRT | OOSIOP_DIEN_SSI | OOSIOP_DIEN_SIR |
	    OOSIOP_DIEN_WTD | OOSIOP_DIEN_IID);
}

void
oosiop_reset_bus(struct oosiop_softc *sc)
{
	/* Assert SCSI RST */
	oosiop_write_1(sc, OOSIOP_SCNTL1, OOSIOP_SCNTL1_RST);
	delay(25);	/* Reset hold time (25us) */
	oosiop_write_1(sc, OOSIOP_SCNTL1, 0);

	/* Remove all nexuses */
	if (sc->sc_nexus) {
		sc->sc_nexus->xfer->status =
		    SCSI_OOSIOP_NOSTATUS; /* XXX */
		oosiop_done(sc, sc->sc_nexus);
	}

	sc->sc_curcb = NULL;

	delay(250000);	/* Reset to selection (250ms) */
}

void
oosiop_processintr(struct oosiop_softc *sc, u_int8_t istat)
{
	struct oosiop_cb *cb;
	uint32_t dcmd;
	u_int8_t dstat, sstat0;

	sc->sc_nextdsp = Ent_wait_reselect;

	/* DMA interrupts */
	if (istat & OOSIOP_ISTAT_DIP) {
		oosiop_write_1(sc, OOSIOP_ISTAT, 0);

		dstat = oosiop_read_1(sc, OOSIOP_DSTAT);

		if (dstat & OOSIOP_DSTAT_ABRT) {
			sc->sc_nextdsp = oosiop_read_4(sc, OOSIOP_DSP) -
			    (uint32_t)sc->sc_scr - 8;

			if (sc->sc_nextdsp == Ent_p_resel_msgin_move &&
			    (oosiop_read_1(sc, OOSIOP_SBCL) & OOSIOP_ACK)) {
				if ((dstat & OOSIOP_DSTAT_DFE) == 0)
					oosiop_flush_fifo(sc);
				sc->sc_nextdsp += 8;
			}
		}

		if (dstat & OOSIOP_DSTAT_SSI) {
			sc->sc_nextdsp = oosiop_read_4(sc, OOSIOP_DSP) -
			    (uint32_t)sc->sc_scr;
			printf("ncsc: single step %p\n", sc->sc_nextdsp);
		}

		if (dstat & OOSIOP_DSTAT_SIR) {
			if ((dstat & OOSIOP_DSTAT_DFE) == 0)
				oosiop_flush_fifo(sc);
			oosiop_scriptintr(sc);
		}

		if (dstat & OOSIOP_DSTAT_WTD) {
			printf("ncsc: DMA time out\n");
			oosiop_reset(sc);
		}

		if (dstat & OOSIOP_DSTAT_IID) {
			dcmd = oosiop_read_4(sc, OOSIOP_DBC);
			if ((dcmd & 0xf8000000) == 0x48000000) {
				printf("ncsc: REQ asserted on WAIT DISCONNECT\n");
				sc->sc_nextdsp = Ent_phasedispatch; /* XXX */
			} else {
				printf("ncsc: invalid SCRIPTS instruction "
				    "addr=%p dcmd=%p dsps=%p\n",
				    oosiop_read_4(sc, OOSIOP_DSP) - 8, dcmd,
				    oosiop_read_4(sc, OOSIOP_DSPS));
				oosiop_reset(sc);
				oosiop_load_script(sc);
			}
		}

		if ((dstat & OOSIOP_DSTAT_DFE) == 0)
			oosiop_clear_fifo(sc);
	}

	/* SCSI interrupts */
	if (istat & OOSIOP_ISTAT_SIP) {
		if (istat & OOSIOP_ISTAT_DIP)
			delay(1);
		sstat0 = oosiop_read_1(sc, OOSIOP_SSTAT0);

		if (sstat0 & OOSIOP_SSTAT0_M_A) {
			/* SCSI phase mismatch during MOVE operation */
			oosiop_phasemismatch(sc);
			sc->sc_nextdsp = Ent_phasedispatch;
		}

		if (sstat0 & OOSIOP_SSTAT0_STO) {
			if (sc->sc_curcb) {
				sc->sc_curcb->flags |= CBF_SELTOUT;
				oosiop_done(sc, sc->sc_curcb);
			}
		}

		if (sstat0 & OOSIOP_SSTAT0_SGE) {
			printf("ncsc: SCSI gross error\n");
			oosiop_reset(sc);
		}

		if (sstat0 & OOSIOP_SSTAT0_UDC) {
			/* XXX */
			if (sc->sc_curcb) {
				printf("ncsc: unexpected disconnect\n");
				oosiop_done(sc, sc->sc_curcb);
			}
		}

		if (sstat0 & OOSIOP_SSTAT0_RST)
			oosiop_reset(sc);

		if (sstat0 & OOSIOP_SSTAT0_PAR)
			printf("ncsc: parity error\n");
	}

	/* Start next command if available */
	if (sc->sc_nextdsp == Ent_wait_reselect && sc->sc_pending != 0) {
		sc->sc_pending = 0;
		cb = sc->sc_curcb = &sc->sc_cb;
		sc->sc_nexus = cb;
		oosiop_setup_dma(sc);
		oosiop_setup_syncxfer(sc);
		sc->sc_nextdsp = Ent_start_select;
	}

	/* Restart script */
	oosiop_write_4(sc, OOSIOP_DSP, sc->sc_nextdsp + (uint32_t)sc->sc_scr);
}

void
oosiop_scriptintr(struct oosiop_softc *sc)
{
	struct oosiop_cb *cb;
	uint32_t icode;
	uint32_t dsp;
	int i;
	u_int8_t sfbr, resid, resmsg;

	cb = sc->sc_curcb;
	icode = oosiop_read_4(sc, OOSIOP_DSPS);

	switch (icode) {
	case A_int_done:
		if (cb)
			oosiop_done(sc, cb);
		break;

	case A_int_msgin:
		if (cb)
			oosiop_msgin(sc, cb);
		break;

	case A_int_extmsg:
		/* extended message in DMA setup request */
		sfbr = oosiop_read_1(sc, OOSIOP_SFBR);
		oosiop_fixup_move(sc, Ent_p_extmsgin_move, sfbr,
		    (uint32_t)cb->xfer +
		    offsetof(struct oosiop_xfer, msgin[2]));
		sc->sc_nextdsp = Ent_rcv_extmsg;
		break;

	case A_int_resel:
		/* reselected */
		resid = oosiop_read_1(sc, OOSIOP_SFBR);
		for (i = 0; i < OOSIOP_NTGT; i++)
			if (resid & (1 << i))
				break;
		if (i == OOSIOP_NTGT) {
			printf("ncsc: missing reselection target id\n");
			break;
		}
		sc->sc_resid = i;
		sc->sc_nextdsp = Ent_wait_resel_identify;

		if (cb) {
			/* Current command was lost arbitration */
			sc->sc_nexus = NULL;
			sc->sc_curcb = NULL;
		}

		break;

	case A_int_res_id:
		cb = sc->sc_nexus;
		resmsg = oosiop_read_1(sc, OOSIOP_SFBR);
		if (MSG_ISIDENTIFY(resmsg) && cb &&
		    (resmsg & MSG_IDENTIFY_LUNMASK) == cb->lun) {
			sc->sc_curcb = cb;
			/* might be overkill */
			oosiop_setup_dma(sc);
			oosiop_setup_syncxfer(sc);
			if (cb->curdp != cb->savedp) {
				cb->curdp = cb->savedp;
				oosiop_setup_sgdma(sc, cb);
			}
			sc->sc_nextdsp = Ent_ack_msgin;
		} else {
			/* Reselection from invalid target */
			oosiop_reset_bus(sc);
		}
		break;

	case A_int_resfail:
		/* reselect failed */
		break;

	case A_int_disc:
		/* disconnected */
		sc->sc_curcb = NULL;
		break;

	case A_int_err:
		/* generic error */
		dsp = oosiop_read_4(sc, OOSIOP_DSP);
		printf("ncsc: script error at %p\n",
		    dsp - 8);
		sc->sc_curcb = NULL;
		break;

	case DATAIN_TRAP:
		printf("ncsc: unexpected datain\n");
		/* XXX: need to reset? */
		break;

	case DATAOUT_TRAP:
		printf("ncsc: unexpected dataout\n");
		/* XXX: need to reset? */
		break;

	default:
		printf("ncsc: unknown intr code %p\n",
		    icode);
		break;
	}
}

void
oosiop_msgin(struct oosiop_softc *sc, struct oosiop_cb *cb)
{
	struct oosiop_xfer *xfer;
	int msgout;

	xfer = cb->xfer;
	sc->sc_nextdsp = Ent_ack_msgin;
	msgout = 0;

	switch (xfer->msgin[0]) {
	case MSG_SAVEDATAPOINTER:
		cb->savedp = cb->curdp;
		break;

	case MSG_RESTOREPOINTERS:
		if (cb->curdp != cb->savedp) {
			cb->curdp = cb->savedp;
			oosiop_setup_sgdma(sc, cb);
		}
		break;

	case MSG_MESSAGE_REJECT:
		break;

	default:
		/* Reject message */
		xfer->msgout[0] = MSG_MESSAGE_REJECT;
		cb->msgoutlen = 1;
		msgout = 1;
	}

	if (msgout) {
		oosiop_fixup_move(sc, Ent_p_msgout_move, cb->msgoutlen,
		    (uint32_t)cb->xfer +
		    offsetof(struct oosiop_xfer, msgout[0]));
		sc->sc_nextdsp = Ent_sendmsg;
	}
}
