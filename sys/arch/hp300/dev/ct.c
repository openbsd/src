/*	$OpenBSD: ct.c,v 1.14 2005/11/18 00:16:48 miod Exp $	*/
/*	$NetBSD: ct.c,v 1.21 1997/04/02 22:37:23 scottr Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)ct.c	8.2 (Berkeley) 1/12/94
 */

/*
 * CS80 cartridge tape driver (9144, 88140, 9145)
 *
 * Reminder:
 *	C_CC bit (character count option) when used in the CS/80 command
 *	'set options' will cause the tape not to stream.
 *
 * TODO:
 *	make filesystem compatible
 *	make block mode work according to mtio(4) spec. (if possible)
 *	merge with cs80 disk driver
 *	finish support of 9145
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/proc.h>
#include <sys/tprintf.h>

#include <hp300/dev/hpibvar.h>

#include <hp300/dev/ctreg.h>

/* number of eof marks to remember */
#define EOFS	128

struct	ct_softc {
	struct	device sc_dev;
	int	sc_slave;		/* HP-IB slave ID */
	int	sc_punit;		/* physical unit */
	struct	ct_iocmd sc_ioc;
	struct	ct_rscmd sc_rsc;
	struct	ct_stat sc_stat;
	struct	ct_ssmcmd sc_ssmc;
	struct	ct_srcmd sc_src;
	struct	ct_soptcmd sc_soptc;
	struct	ct_ulcmd sc_ul;
	struct	ct_wfmcmd sc_wfm;
	struct	ct_clearcmd sc_clear;
	struct	buf sc_tab;
	struct	buf *sc_bp;
	struct	buf sc_bufstore;	/* XXX */
	int	sc_blkno;
	int	sc_cmd;
	int	sc_resid;
	char	*sc_addr;
	int	sc_flags;
	short	sc_type;
	tpr_t	sc_tpr;
	struct	hpibqueue sc_hq;	/* entry on hpib job queue */
	int	sc_eofp;
	int	sc_eofs[EOFS];
};

/* flags */
#define	CTF_OPEN	0x01
#define	CTF_ALIVE	0x02
#define	CTF_WRT		0x04
#define	CTF_CMD		0x08
#define	CTF_IO		0x10
#define	CTF_BEOF	0x20
#define	CTF_AEOF	0x40
#define	CTF_EOT		0x80
#define	CTF_STATWAIT	0x100
#define CTF_CANSTREAM	0x200
#define	CTF_WRTTN	0x400

int	ctmatch(struct device *, void *, void *);
void	ctattach(struct device *, struct device *, void *);

struct cfattach ct_ca = {
	sizeof(struct ct_softc), ctmatch, ctattach
};

struct cfdriver ct_cd = {
	NULL, "ct", DV_TAPE
};

int	ctident(struct device *, struct ct_softc *,
	    struct hpibbus_attach_args *);

void	ctreset(struct ct_softc *);
void	ctaddeof(struct ct_softc *);
void	ctustart(struct ct_softc *);
void	cteof(struct ct_softc *, struct buf *);
void	ctdone(struct ct_softc *, struct buf *);

void	ctstart(void *);
void	ctgo(void *);
void	ctintr(void *);

void	ctcommand(dev_t, int, int);

cdev_decl(ct);
bdev_decl(ct);

const struct	ctinfo {
	short	hwid;
	short	punit;
	char	*desc;
} ctinfo[] = {
	{ CT7946ID,	1,	"7946A"	},
	{ CT7912PID,	1,	"7912P"	},
	{ CT7914PID,	1,	"7914P"	},
	{ CT9144ID,	0,	"9144"	},
	{ CT9145ID,	0,	"9145"	},
};
int	nctinfo = sizeof(ctinfo) / sizeof(ctinfo[0]);

#define	CT_NOREW	4
#define	CT_STREAM	8
#define	UNIT(x)		(minor(x) & 3)
#define	ctpunit(x)	((x) & 7)

#ifdef DEBUG
int ctdebug = 0;
#define CDB_FILES	0x01
#define CT_BSF		0x02
#endif

int
ctmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct hpibbus_attach_args *ha = aux;

	return (ctident(parent, NULL, ha));
}

void
ctattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ct_softc *sc = (struct ct_softc *)self;
	struct hpibbus_attach_args *ha = aux;

	if (ctident(parent, sc, ha) == 0) {
		printf("\n%s: didn't respond to describe command!\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_slave = ha->ha_slave;
	sc->sc_punit = ha->ha_punit;

	sc->sc_tab.b_actb = &sc->sc_tab.b_actf;

	/* Initialize hpib job queue entry. */
	sc->sc_hq.hq_softc = sc;
	sc->sc_hq.hq_slave = sc->sc_slave;
	sc->sc_hq.hq_start = ctstart;
	sc->sc_hq.hq_go = ctgo;
	sc->sc_hq.hq_intr = ctintr;

	ctreset(sc);
	sc->sc_flags |= CTF_ALIVE;
}

int
ctident(parent, sc, ha)
	struct device *parent;
	struct ct_softc *sc;
	struct hpibbus_attach_args *ha;
{
	struct cs80_describe desc;
	u_char stat, cmd[3];
	char name[7];
	int i, id, n, type, canstream;

	type = canstream = 0;

	/* Verify that we have a CS80 device. */
	if ((ha->ha_id & 0x200) == 0)
		return (0);

	/* Is it one of the tapes we support? */
	for (id = 0; id < nctinfo; id++)
		if (ha->ha_id == ctinfo[id].hwid &&
		    ha->ha_punit == ctinfo[id].punit)
			break;
	if (id == nctinfo)
		return (0);

	/*
	 * So far, so good.  Get drive parameters.  Note command
	 * is always issued to unit 0.
	 */
	cmd[0] = C_SUNIT(0);
	cmd[1] = C_SVOL(0);
	cmd[2] = C_DESC;
	hpibsend(parent->dv_unit, ha->ha_slave, C_CMD, cmd, sizeof(cmd));
	hpibrecv(parent->dv_unit, ha->ha_slave, C_EXEC, &desc, sizeof(desc));
	hpibrecv(parent->dv_unit, ha->ha_slave, C_QSTAT, &stat, sizeof(stat));

	bzero(name, sizeof(name));
	if (stat == 0) {
		n = desc.d_name;
		for (i = 5; i >= 0; i--) {
			name[i] = (n & 0xf) + '0';
			n >>= 4;
		}
	}

	switch (ha->ha_id) {
	case CT7946ID:
		if (bcmp(name, "079450", 6) == 0)
			return (0);		/* not really a 7946 */
		/* fall into... */
	case CT9144ID:
	case CT9145ID:
		type = CT9144;
		canstream = 1;
		break;

	case CT7912PID:
	case CT7914PID:
		type = CT88140;
		break;
	}

	if (sc != NULL) {
		sc->sc_type = type;
		sc->sc_flags = canstream ? CTF_CANSTREAM : 0;
		printf(": %s %stape\n", ctinfo[id].desc,
		    canstream ? "streaming " : "");
	}

	return (1);
}

void
ctreset(sc)
	struct ct_softc *sc;
{
	int ctlr, slave;
	u_char stat;

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_slave;

	sc->sc_clear.unit = C_SUNIT(sc->sc_punit);
	sc->sc_clear.cmd = C_CLEAR;
	hpibsend(ctlr, slave, C_TCMD, &sc->sc_clear, sizeof(sc->sc_clear));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	sc->sc_src.unit = C_SUNIT(CTCTLR);
	sc->sc_src.nop = C_NOP;
	sc->sc_src.cmd = C_SREL;
	sc->sc_src.param = C_REL;
	hpibsend(ctlr, slave, C_CMD, &sc->sc_src, sizeof(sc->sc_src));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	sc->sc_ssmc.unit = C_SUNIT(sc->sc_punit);
	sc->sc_ssmc.cmd = C_SSM;
	sc->sc_ssmc.refm = REF_MASK;
	sc->sc_ssmc.fefm = FEF_MASK;
	sc->sc_ssmc.aefm = AEF_MASK;
	sc->sc_ssmc.iefm = IEF_MASK;
	hpibsend(ctlr, slave, C_CMD, &sc->sc_ssmc, sizeof(sc->sc_ssmc));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	sc->sc_soptc.unit = C_SUNIT(sc->sc_punit);
	sc->sc_soptc.nop = C_NOP;
	sc->sc_soptc.cmd = C_SOPT;
	sc->sc_soptc.opt = C_SPAR;
	hpibsend(ctlr, slave, C_CMD, &sc->sc_soptc, sizeof(sc->sc_soptc));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));
}

/*ARGSUSED*/
int
ctopen(dev, flag, type, p)
	dev_t dev;
	int flag, type;
	struct proc *p;
{
	struct ct_softc *sc;
	u_char stat;
	int cc, ctlr, slave;

	if (UNIT(dev) >= ct_cd.cd_ndevs ||
	    (sc = ct_cd.cd_devs[UNIT(dev)]) == NULL ||
	    (sc->sc_flags & CTF_ALIVE) == 0)
		return (ENXIO);

	if (sc->sc_flags & CTF_OPEN)
		return (EBUSY);

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_slave;

	sc->sc_soptc.unit = C_SUNIT(sc->sc_punit);
	sc->sc_soptc.nop = C_NOP;
	sc->sc_soptc.cmd = C_SOPT;
	if ((dev & CT_STREAM) && (sc->sc_flags & CTF_CANSTREAM))
		sc->sc_soptc.opt = C_SPAR | C_IMRPT;
	else
		sc->sc_soptc.opt = C_SPAR;

	/*
	 * Check the return of hpibsend() and hpibswait().
	 * Drive could be loading/unloading a tape. If not checked,
	 * driver hangs.
	 */
	cc = hpibsend(ctlr, slave, C_CMD, &sc->sc_soptc, sizeof(sc->sc_soptc));
	if (cc != sizeof(sc->sc_soptc))
		return (EBUSY);

	hpibswait(ctlr, slave);
	cc = hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));
	if (cc != sizeof(stat))
		return(EBUSY);

	sc->sc_tpr = tprintf_open(p);
	sc->sc_flags |= CTF_OPEN;
	return(0);
}

/*ARGSUSED*/
int
ctclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct ct_softc *sc = ct_cd.cd_devs[UNIT(dev)];

	if ((sc->sc_flags & (CTF_WRT|CTF_WRTTN)) == (CTF_WRT|CTF_WRTTN) &&
	    (sc->sc_flags & CTF_EOT) == 0 ) { /* XXX return error if EOT ?? */
		ctcommand(dev, MTWEOF, 2);
		ctcommand(dev, MTBSR, 1);
		if (sc->sc_eofp == EOFS - 1)
			sc->sc_eofs[EOFS - 1]--;
		else
			sc->sc_eofp--;
#ifdef DEBUG
		if(ctdebug & CT_BSF)
			printf("%s: ctclose backup eofs prt %d blk %d\n",
			       sc->sc_dev.dv_xname, sc->sc_eofp,
			       sc->sc_eofs[sc->sc_eofp]);
#endif
	}
	if ((minor(dev) & CT_NOREW) == 0)
		ctcommand(dev, MTREW, 1);
	sc->sc_flags &= ~(CTF_OPEN | CTF_WRT | CTF_WRTTN);
	tprintf_close(sc->sc_tpr);
#ifdef DEBUG
	if (ctdebug & CDB_FILES)
		printf("ctclose: flags %x\n", sc->sc_flags);
#endif
	return(0);	/* XXX */
}

void
ctcommand(dev, cmd, cnt)
	dev_t dev;
	int cmd;
	int cnt;
{
	struct ct_softc *sc = ct_cd.cd_devs[UNIT(dev)];
	struct buf *bp = &sc->sc_bufstore;
	struct buf *nbp = 0;

	if (cmd == MTBSF && sc->sc_eofp == EOFS - 1) {
		cnt = sc->sc_eofs[EOFS - 1] - cnt;
		ctcommand(dev, MTREW, 1);
		ctcommand(dev, MTFSF, cnt);
		cnt = 2;
		cmd = MTBSR;
	}

	if (cmd == MTBSF && sc->sc_eofp - cnt < 0) {
		cnt = 1;
		cmd = MTREW;
	}

	sc->sc_flags |= CTF_CMD;
	sc->sc_bp = bp;
	sc->sc_cmd = cmd;
	bp->b_dev = dev;
	if (cmd == MTFSF) {
		nbp = (struct buf *)geteblk(MAXBSIZE);
		bp->b_un.b_addr = nbp->b_un.b_addr;
		bp->b_bcount = MAXBSIZE;
	}

	while (cnt-- > 0) {
		bp->b_flags = B_BUSY;
		if (cmd == MTBSF) {
			sc->sc_blkno = sc->sc_eofs[sc->sc_eofp];
			sc->sc_eofp--;
#ifdef DEBUG
			if (ctdebug & CT_BSF)
				printf("%s: backup eof pos %d blk %d\n",
				    sc->sc_dev.dv_xname, sc->sc_eofp,
				    sc->sc_eofs[sc->sc_eofp]);
#endif
		}
		ctstrategy(bp);
		biowait(bp);
	}
	bp->b_flags = 0;
	sc->sc_flags &= ~CTF_CMD;
	if (nbp)
		brelse(nbp);
}

void
ctstrategy(bp)
	struct buf *bp;
{
	struct buf *dp;
	int s, unit;
	struct ct_softc *sc;

	unit = UNIT(bp->b_dev);
	sc = ct_cd.cd_devs[unit];

	dp = &sc->sc_tab;
	bp->b_actf = NULL;
	s = splbio();
	bp->b_actb = dp->b_actb;
	*(dp->b_actb) = bp;
	dp->b_actb = &bp->b_actf;
	if (dp->b_active == 0) {
		dp->b_active = 1;
		ctustart(sc);
	}
	splx(s);
}

void
ctustart(sc)
	struct ct_softc *sc;
{
	struct buf *bp;

	bp = sc->sc_tab.b_actf;
	sc->sc_addr = bp->b_un.b_addr;
	sc->sc_resid = bp->b_bcount;
	if (hpibreq(sc->sc_dev.dv_parent, &sc->sc_hq))
		ctstart(sc);
}

void
ctstart(arg)
	void *arg;
{
	struct ct_softc *sc = arg;
	struct buf *bp;
	int i, ctlr, slave;

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_slave;

	bp = sc->sc_tab.b_actf;
	if ((sc->sc_flags & CTF_CMD) && sc->sc_bp == bp) {
		switch(sc->sc_cmd) {
		case MTFSF:
			bp->b_flags |= B_READ;
			goto mustio;

		case MTBSF:
			goto gotaddr;

		case MTOFFL:
			sc->sc_blkno = 0;
			sc->sc_ul.unit = C_SUNIT(sc->sc_punit);
			sc->sc_ul.cmd = C_UNLOAD;
			hpibsend(ctlr, slave, C_CMD, &sc->sc_ul,
			    sizeof(sc->sc_ul));
			break;

		case MTWEOF:
			sc->sc_blkno++;
			sc->sc_flags |= CTF_WRT;
			sc->sc_wfm.unit = C_SUNIT(sc->sc_punit);
			sc->sc_wfm.cmd = C_WFM;
			hpibsend(ctlr, slave, C_CMD, &sc->sc_wfm,
			    sizeof(sc->sc_wfm));
			ctaddeof(sc);
			break;

		case MTBSR:
			sc->sc_blkno--;
			goto gotaddr;

		case MTFSR:
			sc->sc_blkno++;
			goto gotaddr;

		case MTREW:
			sc->sc_blkno = 0;
#ifdef DEBUG
			if(ctdebug & CT_BSF)
				printf("%s: clearing eofs\n",
				    sc->sc_dev.dv_xname);
#endif
			for (i=0; i<EOFS; i++)
				sc->sc_eofs[i] = 0;
			sc->sc_eofp = 0;

gotaddr:
			sc->sc_ioc.saddr = C_SADDR;
			sc->sc_ioc.addr0 = 0;
			sc->sc_ioc.addr = sc->sc_blkno;
			sc->sc_ioc.unit = C_SUNIT(sc->sc_punit);
			sc->sc_ioc.nop2 = C_NOP;
			sc->sc_ioc.slen = C_SLEN;
			sc->sc_ioc.len = 0;
			sc->sc_ioc.nop3 = C_NOP;
			sc->sc_ioc.cmd = C_READ;
			hpibsend(ctlr, slave, C_CMD, &sc->sc_ioc,
			    sizeof(sc->sc_ioc));
			break;
		}
	} else {
mustio:
		if ((bp->b_flags & B_READ) &&
		    sc->sc_flags & (CTF_BEOF|CTF_EOT)) {
#ifdef DEBUG
			if (ctdebug & CDB_FILES)
				printf("ctstart: before flags %x\n",
				    sc->sc_flags);
#endif
			if (sc->sc_flags & CTF_BEOF) {
				sc->sc_flags &= ~CTF_BEOF;
				sc->sc_flags |= CTF_AEOF;
#ifdef DEBUG
				if (ctdebug & CDB_FILES)
					printf("ctstart: after flags %x\n",
					    sc->sc_flags);
#endif
			}
			bp->b_resid = bp->b_bcount;
			ctdone(sc, bp);
			return;
		}
		sc->sc_flags |= CTF_IO;
		sc->sc_ioc.unit = C_SUNIT(sc->sc_punit);
		sc->sc_ioc.saddr = C_SADDR;
		sc->sc_ioc.addr0 = 0;
		sc->sc_ioc.addr = sc->sc_blkno;
		sc->sc_ioc.nop2 = C_NOP;
		sc->sc_ioc.slen = C_SLEN;
		sc->sc_ioc.len = sc->sc_resid;
		sc->sc_ioc.nop3 = C_NOP;
		if (bp->b_flags & B_READ)
			sc->sc_ioc.cmd = C_READ;
		else {
			sc->sc_ioc.cmd = C_WRITE;
			sc->sc_flags |= (CTF_WRT | CTF_WRTTN);
		}
		hpibsend(ctlr, slave, C_CMD, &sc->sc_ioc, sizeof(sc->sc_ioc));
	}
	hpibawait(ctlr);
}

void
ctgo(arg)
	void *arg;
{
	struct ct_softc *sc = arg;
	struct buf *bp;
	int rw;

	bp = sc->sc_tab.b_actf;
	rw = bp->b_flags & B_READ;
	hpibgo(sc->sc_dev.dv_parent->dv_unit, sc->sc_slave, C_EXEC,
	    sc->sc_addr, sc->sc_resid, rw, rw != 0);
}

/*
 * Hideous grue to handle EOF/EOT (mostly for reads)
 */
void
cteof(sc, bp)
	struct ct_softc *sc;
	struct buf *bp;
{
	long blks;

	/*
	 * EOT on a write is an error.
	 */
	if ((bp->b_flags & B_READ) == 0) {
		bp->b_resid = bp->b_bcount;
		bp->b_flags |= B_ERROR;
		bp->b_error = ENOSPC;
		sc->sc_flags |= CTF_EOT;
		return;
	}
	/*
	 * Use returned block position to determine how many blocks
	 * we really read and update b_resid.
	 */
	blks = sc->sc_stat.c_blk - sc->sc_blkno - 1;
#ifdef DEBUG
	if (ctdebug & CDB_FILES)
		printf("cteof: bc %ld oblk %d nblk %ld read %ld, resid %ld\n",
		       bp->b_bcount, sc->sc_blkno, sc->sc_stat.c_blk,
		       blks, bp->b_bcount - CTKTOB(blks));
#endif
	if (blks == -1) { /* 9145 on EOF does not change sc_stat.c_blk */
		blks = 0;
		sc->sc_blkno++;
	}
	else {
		sc->sc_blkno = sc->sc_stat.c_blk;
	}
	bp->b_resid = bp->b_bcount - CTKTOB(blks);
	/*
	 * If we are at physical EOV or were after an EOF,
	 * we are now at logical EOT.
	 */
	if ((sc->sc_stat.c_aef & AEF_EOV) ||
	    (sc->sc_flags & CTF_AEOF)) {
		sc->sc_flags |= CTF_EOT;
		sc->sc_flags &= ~(CTF_AEOF|CTF_BEOF);
	}
	/*
	 * If we were before an EOF or we have just completed a FSF,
	 * we are now after EOF.
	 */
	else if ((sc->sc_flags & CTF_BEOF) ||
		 ((sc->sc_flags & CTF_CMD) && sc->sc_cmd == MTFSF)) {
		sc->sc_flags |= CTF_AEOF;
		sc->sc_flags &= ~CTF_BEOF;
	}
	/*
	 * Otherwise if we read something we are now before EOF
	 * (and no longer after EOF).
	 */
	else if (blks) {
		sc->sc_flags |= CTF_BEOF;
		sc->sc_flags &= ~CTF_AEOF;
	}
	/*
	 * Finally, if we didn't read anything we just passed an EOF
	 */
	else
		sc->sc_flags |= CTF_AEOF;
#ifdef DEBUG
	if (ctdebug & CDB_FILES)
		printf("cteof: leaving flags %x\n", sc->sc_flags);
#endif
}

/* ARGSUSED */
void
ctintr(arg)
	void *arg;
{
	struct ct_softc *sc = arg;
	struct buf *bp;
	u_char stat;
	int ctlr, slave, unit;

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_slave;
	unit = sc->sc_dev.dv_unit;

	bp = sc->sc_tab.b_actf;
	if (bp == NULL) {
		printf("%s: bp == NULL\n", sc->sc_dev.dv_xname);
		return;
	}
	if (sc->sc_flags & CTF_IO) {
		sc->sc_flags &= ~CTF_IO;
		if (hpibustart(ctlr))
			ctgo(sc);
		return;
	}
	if ((sc->sc_flags & CTF_STATWAIT) == 0) {
		if (hpibpptest(ctlr, slave) == 0) {
			sc->sc_flags |= CTF_STATWAIT;
			hpibawait(ctlr);
			return;
		}
	} else
		sc->sc_flags &= ~CTF_STATWAIT;
	hpibrecv(ctlr, slave, C_QSTAT, &stat, 1);
#ifdef DEBUG
	if (ctdebug & CDB_FILES)
		printf("ctintr: before flags %x\n", sc->sc_flags);
#endif
	if (stat) {
		sc->sc_rsc.unit = C_SUNIT(sc->sc_punit);
		sc->sc_rsc.cmd = C_STATUS;
		hpibsend(ctlr, slave, C_CMD, &sc->sc_rsc, sizeof(sc->sc_rsc));
		hpibrecv(ctlr, slave, C_EXEC, &sc->sc_stat,
		    sizeof(sc->sc_stat));
		hpibrecv(ctlr, slave, C_QSTAT, &stat, 1);
#ifdef DEBUG
		if (ctdebug & CDB_FILES)
			printf("ctintr: return stat 0x%x, A%x F%x blk %ld\n",
			       stat, sc->sc_stat.c_aef,
			       sc->sc_stat.c_fef, sc->sc_stat.c_blk);
#endif
		if (stat == 0) {
			if (sc->sc_stat.c_aef & (AEF_EOF | AEF_EOV)) {
				cteof(sc, bp);
				ctaddeof(sc);
				goto done;
			}
			if (sc->sc_stat.c_fef & FEF_PF) {
				ctreset(sc);
				ctstart(sc);
				return;
			}
			if (sc->sc_stat.c_fef & FEF_REXMT) {
				ctstart(sc);
				return;
			}
			if (sc->sc_stat.c_aef & 0x5800) {
				if (sc->sc_stat.c_aef & 0x4000)
					tprintf(sc->sc_tpr,
						"%s: uninitialized media\n",
						sc->sc_dev.dv_xname);
				if (sc->sc_stat.c_aef & 0x1000)
					tprintf(sc->sc_tpr,
						"%s: not ready\n",
						sc->sc_dev.dv_xname);
				if (sc->sc_stat.c_aef & 0x0800)
					tprintf(sc->sc_tpr,
						"%s: write protect\n",
						sc->sc_dev.dv_xname);
			} else {
				printf("%s err: v%d u%d ru%d bn%ld, ",
				       sc->sc_dev.dv_xname,
				       (sc->sc_stat.c_vu>>4)&0xF,
				       sc->sc_stat.c_vu&0xF,
				       sc->sc_stat.c_pend,
				       sc->sc_stat.c_blk);
				printf("R0x%x F0x%x A0x%x I0x%x\n",
				       sc->sc_stat.c_ref,
				       sc->sc_stat.c_fef,
				       sc->sc_stat.c_aef,
				       sc->sc_stat.c_ief);
			}
		} else
			printf("%s: request status failed\n",
			    sc->sc_dev.dv_xname);
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		goto done;
	} else
		bp->b_resid = 0;
	if (sc->sc_flags & CTF_CMD) {
		switch (sc->sc_cmd) {
		case MTFSF:
			sc->sc_flags &= ~(CTF_BEOF|CTF_AEOF);
			sc->sc_blkno += CTBTOK(sc->sc_resid);
			ctstart(sc);
			return;
		case MTBSF:
			sc->sc_flags &= ~(CTF_AEOF|CTF_BEOF|CTF_EOT);
			break;
		case MTBSR:
			sc->sc_flags &= ~CTF_BEOF;
			if (sc->sc_flags & CTF_EOT) {
				sc->sc_flags |= CTF_AEOF;
				sc->sc_flags &= ~CTF_EOT;
			} else if (sc->sc_flags & CTF_AEOF) {
				sc->sc_flags |= CTF_BEOF;
				sc->sc_flags &= ~CTF_AEOF;
			}
			break;
		case MTWEOF:
			sc->sc_flags &= ~CTF_BEOF;
			if (sc->sc_flags & (CTF_AEOF|CTF_EOT)) {
				sc->sc_flags |= CTF_EOT;
				sc->sc_flags &= ~CTF_AEOF;
			} else
				sc->sc_flags |= CTF_AEOF;
			break;
		case MTREW:
		case MTOFFL:
			sc->sc_flags &= ~(CTF_BEOF|CTF_AEOF|CTF_EOT);
			break;
		}
	} else {
		sc->sc_flags &= ~CTF_AEOF;
		sc->sc_blkno += CTBTOK(sc->sc_resid);
	}
done:
#ifdef DEBUG
	if (ctdebug & CDB_FILES)
		printf("ctintr: after flags %x\n", sc->sc_flags);
#endif
	ctdone(sc, bp);
}

void
ctdone(sc, bp)
	struct ct_softc *sc;
	struct buf *bp;
{
	struct buf *dp;
	int s;

	if ((dp = bp->b_actf) != NULL)
		dp->b_actb = bp->b_actb;
	else
		sc->sc_tab.b_actb = bp->b_actb;
	*bp->b_actb = dp;
	s = splbio();
	biodone(bp);
	splx(s);
	hpibfree(sc->sc_dev.dv_parent, &sc->sc_hq);
	if (sc->sc_tab.b_actf == NULL) {
		sc->sc_tab.b_active = 0;
		return;
	}
	ctustart(sc);
}

int
ctread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (physio(ctstrategy, NULL, dev, B_READ, minphys, uio));
}

int
ctwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	/* XXX: check for hardware write-protect? */
	return (physio(ctstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*ARGSUSED*/
int
ctioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	int flag;
	caddr_t data;
	struct proc *p;
{
	struct mtop *op;
	int cnt;

	switch (cmd) {

	case MTIOCTOP:
		op = (struct mtop *)data;
		switch(op->mt_op) {

		case MTWEOF:
		case MTFSF:
		case MTBSR:
		case MTBSF:
		case MTFSR:
			cnt = op->mt_count;
			break;

		case MTREW:
		case MTOFFL:
			cnt = 1;
			break;

		default:
			return(EINVAL);
		}
		ctcommand(dev, op->mt_op, cnt);
		break;

	case MTIOCGET:
		break;

	default:
		return(EINVAL);
	}
	return(0);
}

/* ARGSUSED */
int
ctdump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	return (ENODEV);
}

void
ctaddeof(sc)
	struct ct_softc *sc;
{

	if (sc->sc_eofp == EOFS - 1)
		sc->sc_eofs[EOFS - 1]++;
	else {
		sc->sc_eofp++;
		if (sc->sc_eofp == EOFS - 1)
			sc->sc_eofs[EOFS - 1] = EOFS;
		else
			/* save blkno */
			sc->sc_eofs[sc->sc_eofp] = sc->sc_blkno - 1;
	}
#ifdef DEBUG
	if (ctdebug & CT_BSF)
		printf("%s: add eof pos %d blk %d\n",
		       sc->sc_dev.dv_xname, sc->sc_eofp,
		       sc->sc_eofs[sc->sc_eofp]);
#endif
}
