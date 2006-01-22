/*	$OpenBSD: fd.c,v 1.43 2006/01/22 00:40:02 miod Exp $	*/
/*	$NetBSD: fd.c,v 1.51 1997/05/24 20:16:19 pk Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
/*-
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Portions Copyright (c) 1993, 1994 by
 *  jc@irbs.UUCP (John Capo)
 *  vak@zebub.msk.su (Serge Vakulenko)
 *  ache@astral.msk.su (Andrew A. Chernov)
 *  joerg_wunsch@uriah.sax.de (Joerg Wunsch)
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
 *	@(#)fd.c	7.4 (Berkeley) 5/25/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/disk.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/mtio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/ioctl_fd.h>

#include <sparc/sparc/auxioreg.h>
#include <sparc/dev/fdreg.h>
#include <sparc/dev/fdvar.h>

#define FDUNIT(dev)	((dev & 0x80) >> 7)
#define FDTYPE(dev)	((minor(dev) & 0x70) >> 4)
#define FDPART(dev)	(minor(dev) & 0x0f)

/* XXX misuse a flag to identify format operation */
#define B_FORMAT B_XXX

#ifdef FD_DEBUG
int	fdc_debug = 0;
#endif

enum fdc_state {
	DEVIDLE = 0,
	MOTORWAIT,	/*  1 */
	DOSEEK,		/*  2 */
	SEEKWAIT,	/*  3 */
	SEEKTIMEDOUT,	/*  4 */
	SEEKCOMPLETE,	/*  5 */
	DOIO,		/*  6 */
	IOCOMPLETE,	/*  7 */
	IOTIMEDOUT,	/*  8 */
	IOCLEANUPWAIT,	/*  9 */
	IOCLEANUPTIMEDOUT,/*10 */
	DORESET,	/* 11 */
	RESETCOMPLETE,	/* 12 */
	RESETTIMEDOUT,	/* 13 */
	DORECAL,	/* 14 */
	RECALWAIT,	/* 15 */
	RECALTIMEDOUT,	/* 16 */
	RECALCOMPLETE,	/* 17 */
};

/* software state, per controller */
struct fdc_softc {
	struct device	sc_dev;		/* boilerplate */
	struct intrhand sc_sih;
	caddr_t		sc_reg;
	struct fd_softc *sc_fd[4];	/* pointers to children */
	TAILQ_HEAD(drivehead, fd_softc) sc_drives;
	enum fdc_state	sc_state;
	int		sc_flags;
#define FDC_82077		0x01
#define FDC_NEEDHEADSETTLE	0x02
#define FDC_EIS			0x04
#define FDC_NEEDMOTORWAIT	0x08
	int		sc_errors;		/* number of retries so far */
	int		sc_overruns;		/* number of DMA overruns */
	int		sc_cfg;			/* current configuration */
	struct fdcio	sc_io;
#define sc_reg_msr	sc_io.fdcio_reg_msr
#define sc_reg_fifo	sc_io.fdcio_reg_fifo
#define sc_reg_dor	sc_io.fdcio_reg_dor
#define sc_reg_drs	sc_io.fdcio_reg_msr
#define sc_itask	sc_io.fdcio_itask
#define sc_istatus	sc_io.fdcio_istatus
#define sc_data		sc_io.fdcio_data
#define sc_tc		sc_io.fdcio_tc
#define sc_nstat	sc_io.fdcio_nstat
#define sc_status	sc_io.fdcio_status
#define	sc_hih		sc_io.fdcio_ih
	struct timeout	fdctimeout_to;
	struct timeout	fdcpseudointr_to;
};

#ifndef FDC_C_HANDLER
extern	struct fdcio *fdciop;
#endif

/* controller driver configuration */
int	fdcmatch(struct device *, void *, void *);
void	fdcattach(struct device *, struct device *, void *);

struct cfattach fdc_ca = {
	sizeof(struct fdc_softc), fdcmatch, fdcattach
};

struct cfdriver fdc_cd = {
	NULL, "fdc", DV_DULL
};

__inline struct fd_type *fd_dev_to_type(struct fd_softc *, dev_t);

/* The order of entries in the following table is important -- BEWARE! */
struct fd_type fd_types[] = {
	{ 18,2,36,2,0xff,0xcf,0x1b,0x54,80,2880,1,FDC_500KBPS, "1.44MB"    }, /* 1.44MB diskette */
	{  9,2,18,2,0xff,0xdf,0x2a,0x50,80,1440,1,FDC_250KBPS, "720KB"    }, /* 3.5" 720kB diskette */
	{  9,2,18,2,0xff,0xdf,0x2a,0x50,40, 720,2,FDC_250KBPS, "360KB/x"  }, /* 360kB in 720kB drive */
	{  8,2,16,3,0xff,0xdf,0x35,0x74,77,1232,1,FDC_500KBPS, "1.2MB/NEC" } /* 1.2 MB japanese format */
};

/* software state, per disk (with up to 4 disks per ctlr) */
struct fd_softc {
	struct device	sc_dv;		/* generic device info */
	struct disk	sc_dk;		/* generic disk info */

	struct fd_type *sc_deftype;	/* default type descriptor */
	struct fd_type *sc_type;	/* current type descriptor */

	daddr_t	sc_blkno;	/* starting block number */
	int sc_bcount;		/* byte count left */
	int sc_skip;		/* bytes already transferred */
	int sc_nblks;		/* number of blocks currently transferring */
	int sc_nbytes;		/* number of bytes currently transferring */

	int sc_drive;		/* physical unit number */
	int sc_flags;
#define	FD_OPEN		0x01		/* it's open */
#define	FD_MOTOR	0x02		/* motor should be on */
#define	FD_MOTOR_WAIT	0x04		/* motor coming up */
	int sc_cylin;		/* where we think the head is */
	int sc_opts;		/* user-set options */

	void	*sc_sdhook;	/* shutdownhook cookie */

	TAILQ_ENTRY(fd_softc) sc_drivechain;
	int sc_ops;		/* I/O ops since last switch */
	struct buf sc_q;	/* head of buf chain */

	struct timeout fd_motor_on_to;
	struct timeout fd_motor_off_to;
};

/* floppy driver configuration */
int	fdmatch(struct device *, void *, void *);
void	fdattach(struct device *, struct device *, void *);

struct cfattach fd_ca = {
	sizeof(struct fd_softc), fdmatch, fdattach
};

struct cfdriver fd_cd = {
	NULL, "fd", DV_DISK
};

void fdgetdisklabel(dev_t);
int fd_get_parms(struct fd_softc *);
void fdstrategy(struct buf *);
void fdstart(struct fd_softc *);
int fdprint(void *, const char *);

struct dkdriver fddkdriver = { fdstrategy };

struct	fd_type *fd_nvtotype(char *, int, int);
void	fd_set_motor(struct fdc_softc *fdc);
void	fd_motor_off(void *arg);
void	fd_motor_on(void *arg);
int	fdcresult(struct fdc_softc *fdc);
int	fdc_wrfifo(struct fdc_softc *fdc, u_char x);
void	fdcstart(struct fdc_softc *fdc);
void	fdcstatus(struct fdc_softc *fdc, char *s);
void	fdc_reset(struct fdc_softc *fdc);
void	fdctimeout(void *arg);
void	fdcpseudointr(void *arg);
#ifdef FDC_C_HANDLER
int	fdc_c_hwintr(struct fdc_softc *);
#else
void	fdchwintr(void);
#endif
int	fdcswintr(struct fdc_softc *);
int	fdcstate(struct fdc_softc *);
void	fdcretry(struct fdc_softc *fdc);
void	fdfinish(struct fd_softc *fd, struct buf *bp);
int	fdformat(dev_t, struct fd_formb *, struct proc *);
void	fd_do_eject(struct fd_softc *);
static int fdconf(struct fdc_softc *);

#if IPL_FDSOFT == 4
#define IE_FDSOFT	IE_L4
#else
#error 4
#endif

#ifdef FDC_C_HANDLER
#if defined(SUN4M)
#define FD_SET_SWINTR do {		\
	if (CPU_ISSUN4M)		\
		raise(0, IPL_FDSOFT);	\
	else				\
		ienab_bis(IE_FDSOFT);	\
} while(0)
#else
#define FD_SET_SWINTR ienab_bis(IE_FDSOFT)
#endif /* defined(SUN4M) */
#endif /* FDC_C_HANDLER */

#define OBP_FDNAME	(CPU_ISSUN4M ? "SUNW,fdtwo" : "fd")

int
fdcmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	/*
	 * Floppy doesn't exist on sun4.
	 */
	if (CPU_ISSUN4)
		return (0);

	/*
	 * Floppy controller is on mainbus on sun4c.
	 */
	if ((CPU_ISSUN4C) && (ca->ca_bustype != BUS_MAIN))
		return (0);

	/*
	 * Floppy controller is on obio on sun4m.
	 */
	if ((CPU_ISSUN4M) && (ca->ca_bustype != BUS_OBIO))
		return (0);

	/* Sun PROMs call the controller an "fd" or "SUNW,fdtwo" */
	if (strcmp(OBP_FDNAME, ra->ra_name))
		return (0);

	if (ca->ca_ra.ra_vaddr &&
	    probeget(ca->ca_ra.ra_vaddr, 1) == -1) {
		return (0);
	}

	return (1);
}

/*
 * Arguments passed between fdcattach and fdprobe.
 */
struct fdc_attach_args {
	int fa_drive;
	struct bootpath *fa_bootpath;
	struct fd_type *fa_deftype;
};

/*
 * Print the location of a disk drive (called just before attaching the
 * the drive).  If `fdc' is not NULL, the drive was found but was not
 * in the system config file; print the drive name as well.
 * Return QUIET (config_find ignores this if the device was configured) to
 * avoid printing `fdN not configured' messages.
 */
int
fdprint(aux, fdc)
	void *aux;
	const char *fdc;
{
	register struct fdc_attach_args *fa = aux;

	if (!fdc)
		printf(" drive %d", fa->fa_drive);
	return (QUIET);
}

/*
 * Configure several parameters and features on the FDC.
 * Return 0 on success.
 */
static int
fdconf(fdc)
	struct fdc_softc *fdc;
{
	int	vroom;

	if (fdc_wrfifo(fdc, NE7CMD_DUMPREG) || fdcresult(fdc) != 10)
		return (-1);

	/*
	 * dumpreg[7] seems to be a motor-off timeout; set it to whatever
	 * the PROM thinks is appropriate.
	 */
	if ((vroom = fdc->sc_status[7]) == 0)
		vroom = 0x64;

	/* Configure controller to use FIFO and Implied Seek */
	if (fdc_wrfifo(fdc, NE7CMD_CFG) != 0)
		return (-1);
	if (fdc_wrfifo(fdc, vroom) != 0)
		return (-1);
	if (fdc_wrfifo(fdc, fdc->sc_cfg) != 0)
		return (-1);
	if (fdc_wrfifo(fdc, 0) != 0)	/* PRETRK */
		return (-1);
	/* No result phase for the NE7CMD_CFG command */

	if ((fdc->sc_flags & FDC_82077) != 0) {
		/* Lock configuration accross soft resets. */
		if (fdc_wrfifo(fdc, NE7CMD_LOCK | CFG_LOCK) != 0 ||
		    fdcresult(fdc) != 1) {
#ifdef FD_DEBUG
			printf("fdconf: CFGLOCK failed");
#endif
			return (-1);
		}
	}
	return (0);
#if 0
	if (fdc_wrfifo(fdc, NE7CMD_VERSION) == 0 &&
	    fdcresult(fdc) == 1 && fdc->sc_status[0] == 0x90) {
		if (fdc_debug)
			printf("[version cmd]");
	}
#endif

}

void
fdcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	struct fdc_softc *fdc = (void *)self;
	struct fdc_attach_args fa;
	struct bootpath *bp;
	int pri;
	char code;

	if (ca->ca_ra.ra_vaddr)
		fdc->sc_reg = (caddr_t)ca->ca_ra.ra_vaddr;
	else
		fdc->sc_reg = (caddr_t)mapiodev(ca->ca_ra.ra_reg, 0,
						ca->ca_ra.ra_len);

	fdc->sc_state = DEVIDLE;
	fdc->sc_itask = FDC_ITASK_NONE;
	fdc->sc_istatus = FDC_ISTATUS_NONE;
	fdc->sc_flags |= FDC_EIS;
	TAILQ_INIT(&fdc->sc_drives);

	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d, softpri %d: ", pri, IPL_FDSOFT);
#ifdef FDC_C_HANDLER
	fdc->sc_hih.ih_fun = (void *)fdc_c_hwintr;
	fdc->sc_hih.ih_arg = fdc;
	intr_establish(pri, &fdc->sc_hih, IPL_FD, self->dv_xname);
#else
	fdciop = &fdc->sc_io;
	fdc->sc_hih.ih_vec = pri;
	if (intr_fasttrap(pri, fdchwintr, NULL, NULL) != 0) {
		printf("unable to register fast trap handler\n");
		return;
	}
	evcount_attach(&fdc->sc_hih.ih_count, self->dv_xname,
	    &fdc->sc_hih.ih_vec, &evcount_intr);
#endif
	fdc->sc_sih.ih_fun = (void *)fdcswintr;
	fdc->sc_sih.ih_arg = fdc;
	intr_establish(IPL_FDSOFT, &fdc->sc_sih, IPL_BIO, self->dv_xname);

	/* Assume a 82077 */
	fdc->sc_reg_msr = &((struct fdreg_77 *)fdc->sc_reg)->fd_msr;
	fdc->sc_reg_fifo = &((struct fdreg_77 *)fdc->sc_reg)->fd_fifo;
	fdc->sc_reg_dor = &((struct fdreg_77 *)fdc->sc_reg)->fd_dor;

	code = '7';
	if (*fdc->sc_reg_dor == NE7_RQM) {
		/*
		 * This hack from Chris Torek: apparently DOR really
		 * addresses MSR/DRS on a 82072.
		 * We used to rely on the VERSION command to tell the
		 * difference (which did not work).
		 */
		*fdc->sc_reg_dor = FDC_250KBPS;
		if (*fdc->sc_reg_dor == NE7_RQM)
			code = '2';
	}
	if (code == '7') {
		fdc->sc_flags |= FDC_82077;
		fdc->sc_flags |= FDC_NEEDMOTORWAIT;
	} else {
		fdc->sc_reg_msr = &((struct fdreg_72 *)fdc->sc_reg)->fd_msr;
		fdc->sc_reg_fifo = &((struct fdreg_72 *)fdc->sc_reg)->fd_fifo;
		fdc->sc_reg_dor = 0;
	}

	/*
	 * Configure controller; enable FIFO, Implied seek, no POLL mode?.
	 * Note: CFG_EFIFO is active-low, initial threshold value: 8
	 */
	fdc->sc_cfg = CFG_EIS|/*CFG_EFIFO|*/CFG_POLL|(8 & CFG_THRHLD_MASK);
	if (fdconf(fdc) != 0) {
		printf("%s: no drives attached\n", fdc->sc_dev.dv_xname);
		return;
	}

	if ((fdc->sc_flags & FDC_82077) != 0) {
		/* Lock configuration across soft resets. */
		fdc_wrfifo(fdc, NE7CMD_LOCK | CFG_LOCK);
		if (fdcresult(fdc) != 1)
			printf(" CFGLOCK: unexpected response");
	}

	printf("chip 8207%c\n", code);

	/*
	 * Controller and drives are represented by one and the same
	 * Openprom node, so we can as well check for the floppy boots here.
	 */
	fa.fa_bootpath = 0;
	if ((bp = ca->ca_ra.ra_bp) && strcmp(bp->name, OBP_FDNAME) == 0) {

		switch (ca->ca_bustype) {
		case BUS_MAIN:
			/*
			 * We can get the bootpath in several different
			 * formats! The faked v1 bootpath looks like /fd@0,0.
			 * The v2 bootpath is either just /fd0, in which case
			 * `bp->val[0]' will have been set to -1, or /fd@x,y
			 * where <x,y> is the prom address specifier.
			 */
			if (((bp->val[0] == ca->ca_ra.ra_iospace) &&
			     (bp->val[1] == (int)ca->ca_ra.ra_paddr)) ||

			    ((bp->val[0] == -1) &&	/* v2: /fd0 */
			     (bp->val[1] == 0)) ||

			    ((bp->val[0] == 0) &&	/* v1: /fd@0,0 */
			     (bp->val[1] == 0))
			   )
				fa.fa_bootpath = bp;
			break;

		case BUS_OBIO:
			/*
			 * floppy controller on obio (such as on the sun4m),
			 * e.g.: `/obio0/SUNW,fdtwo@0,700000'.
			 * We use "slot, offset" to determine if this is the
			 * right one.
			 */
			if ((bp->val[0] == ca->ca_slot) &&
			    (bp->val[1] == ca->ca_offset))
				fa.fa_bootpath = bp;
			break;
		}

	}

	timeout_set(&fdc->fdctimeout_to, fdctimeout, fdc);
	timeout_set(&fdc->fdcpseudointr_to, fdcpseudointr, fdc);

	/*
	 * physical limit: four drives per controller, but the dev_t
	 * only has room for 2
	 */
	for (fa.fa_drive = 0; fa.fa_drive < 2; fa.fa_drive++) {
		fa.fa_deftype = NULL;		/* unknown */
		fa.fa_deftype = &fd_types[0];	/* XXX */
		(void)config_found(self, (void *)&fa, fdprint);
	}

	bootpath_store(1, NULL);
}

int
fdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct fdc_softc *fdc = (void *)parent;
	struct fdc_attach_args *fa = aux;
	int drive = fa->fa_drive;
	int n, ok;

	if (drive > 0)
		/* XXX - for now, punt on more than one drive */
		return (0);

	if ((fdc->sc_flags & FDC_82077) != 0) {
		/* select drive and turn on motor */
		*fdc->sc_reg_dor = drive | FDO_FRST | FDO_MOEN(drive);
		/* wait for motor to spin up */
		delay(250000);
	} else {
		auxregbisc(AUXIO4C_FDS, 0);
	}

	fdc->sc_nstat = 0;
	fdc_wrfifo(fdc, NE7CMD_RECAL);
	fdc_wrfifo(fdc, drive);

	/* wait for recalibrate */
	for (n = 0; n < 10000; n++) {
		delay(1000);
		if ((*fdc->sc_reg_msr & (NE7_RQM|NE7_DIO|NE7_CB)) == NE7_RQM) {
			/* wait a bit longer till device *really* is ready */
			delay(100000);
			if (fdc_wrfifo(fdc, NE7CMD_SENSEI))
				break;
			if (fdcresult(fdc) == 1 && fdc->sc_status[0] == 0x80)
				/*
				 * Got `invalid command'; we interpret it
				 * to mean that the re-calibrate hasn't in
				 * fact finished yet
				 */
				continue;
			break;
		}
	}
	n = fdc->sc_nstat;
#ifdef FD_DEBUG
	if (fdc_debug) {
		int i;
		printf("fdprobe: %d stati:", n);
		for (i = 0; i < n; i++)
			printf(" 0x%x", fdc->sc_status[i]);
		printf("\n");
	}
#endif
	ok = (n == 2 && (fdc->sc_status[0] & 0xf8) == 0x20) ? 1 : 0;

	/* turn off motor */
	if ((fdc->sc_flags & FDC_82077) != 0) {
		/* deselect drive and turn motor off */
		*fdc->sc_reg_dor = FDO_FRST | FDO_DS;
	} else {
		auxregbisc(0, AUXIO4C_FDS);
	}

	return (ok);
}

/*
 * Controller is working, and drive responded.  Attach it.
 */
void
fdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fdc_softc *fdc = (void *)parent;
	struct fd_softc *fd = (void *)self;
	struct fdc_attach_args *fa = aux;
	struct fd_type *type = fa->fa_deftype;
	int drive = fa->fa_drive;

	/* Setup timeouts */
	timeout_set(&fd->fd_motor_on_to, fd_motor_on, fd);
	timeout_set(&fd->fd_motor_off_to, fd_motor_off, fd);

	/* XXX Allow `flags' to override device type? */

	if (type)
		printf(": %s %d cyl, %d head, %d sec\n", type->name,
		    type->tracks, type->heads, type->sectrac);
	else
		printf(": density unknown\n");

	fd->sc_cylin = -1;
	fd->sc_drive = drive;
	fd->sc_deftype = type;
	fdc->sc_fd[drive] = fd;

	fdc_wrfifo(fdc, NE7CMD_SPECIFY);
	fdc_wrfifo(fdc, type->steprate);
	/* XXX head load time == 6ms */
	fdc_wrfifo(fdc, 6 | NE7_SPECIFY_NODMA);

	/*
	 * Initialize and attach the disk structure.
	 */
	fd->sc_dk.dk_name = fd->sc_dv.dv_xname;
	fd->sc_dk.dk_driver = &fddkdriver;
	disk_attach(&fd->sc_dk);

	/*
	 * We're told if we're the boot device in fdcattach().
	 */
	if (fa->fa_bootpath)
		fa->fa_bootpath->dev = &fd->sc_dv;

	/* Make sure the drive motor gets turned off at shutdown time. */
	fd->sc_sdhook = shutdownhook_establish(fd_motor_off, fd);

	/* XXX Need to do some more fiddling with sc_dk. */
	dk_establish(&fd->sc_dk, &fd->sc_dv);
}

__inline struct fd_type *
fd_dev_to_type(fd, dev)
	struct fd_softc *fd;
	dev_t dev;
{
	int type = FDTYPE(dev);

	if (type > (sizeof(fd_types) / sizeof(fd_types[0])))
		return (NULL);
	return (type ? &fd_types[type - 1] : fd->sc_deftype);
}

void
fdstrategy(bp)
	register struct buf *bp;	/* IO operation to perform */
{
	struct fd_softc *fd;
	int unit = FDUNIT(bp->b_dev);
	int sz;
 	int s;

	/* Valid unit, controller, and request? */
	if (unit >= fd_cd.cd_ndevs ||
	    (fd = fd_cd.cd_devs[unit]) == 0 ||
	    bp->b_blkno < 0 ||
	    (((bp->b_bcount % FD_BSIZE(fd)) != 0 ||
	      (bp->b_blkno * DEV_BSIZE) % FD_BSIZE(fd) != 0) &&
	     (bp->b_flags & B_FORMAT) == 0)) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	sz = howmany(bp->b_bcount, DEV_BSIZE);

	if (bp->b_blkno + sz > (fd->sc_type->size * DEV_BSIZE) / FD_BSIZE(fd)) {
		sz = (fd->sc_type->size * DEV_BSIZE) / FD_BSIZE(fd)
		    - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

 	bp->b_cylinder = (bp->b_blkno * DEV_BSIZE) /
	    (FD_BSIZE(fd) * fd->sc_type->seccyl);

#ifdef FD_DEBUG
	if (fdc_debug > 1)
	    printf("fdstrategy: b_blkno %d b_bcount %ld blkno %d cylin %ld\n",
		    bp->b_blkno, bp->b_bcount, fd->sc_blkno, bp->b_cylinder);
#endif

	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	disksort(&fd->sc_q, bp);
	timeout_del(&fd->fd_motor_off_to); /* a good idea */
	if (!fd->sc_q.b_active)
		fdstart(fd);
#ifdef DIAGNOSTIC
	else {
		struct fdc_softc *fdc = (void *)fd->sc_dv.dv_parent;
		if (fdc->sc_state == DEVIDLE) {
			printf("fdstrategy: controller inactive\n");
			fdcstart(fdc);
		}
	}
#endif
	splx(s);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	s = splbio();
	biodone(bp);
	splx(s);
}

void
fdstart(fd)
	struct fd_softc *fd;
{
	struct fdc_softc *fdc = (void *)fd->sc_dv.dv_parent;
	int active = !TAILQ_EMPTY(&fdc->sc_drives);

	/* Link into controller queue. */
	fd->sc_q.b_active = 1;
	TAILQ_INSERT_TAIL(&fdc->sc_drives, fd, sc_drivechain);

	/* If controller not already active, start it. */
	if (!active)
		fdcstart(fdc);
}

void
fdfinish(fd, bp)
	struct fd_softc *fd;
	struct buf *bp;
{
	struct fdc_softc *fdc = (void *)fd->sc_dv.dv_parent;

	/*
	 * Move this drive to the end of the queue to give others a `fair'
	 * chance.  We only force a switch if N operations are completed while
	 * another drive is waiting to be serviced, since there is a long motor
	 * startup delay whenever we switch.
	 */
	if (TAILQ_NEXT(fd, sc_drivechain) != NULL && ++fd->sc_ops >= 8) {
		fd->sc_ops = 0;
		TAILQ_REMOVE(&fdc->sc_drives, fd, sc_drivechain);
		if (bp->b_actf) {
			TAILQ_INSERT_TAIL(&fdc->sc_drives, fd, sc_drivechain);
		} else
			fd->sc_q.b_active = 0;
	}
	bp->b_resid = fd->sc_bcount;
	fd->sc_skip = 0;
	fd->sc_q.b_actf = bp->b_actf;

	biodone(bp);
	/* turn off motor 5s from now */
	timeout_add(&fd->fd_motor_off_to, 5 * hz);
	fdc->sc_state = DEVIDLE;
}

void
fdc_reset(fdc)
	struct fdc_softc *fdc;
{
	if (fdc->sc_flags & FDC_82077) {
		*fdc->sc_reg_dor = FDO_FDMAEN | FDO_MOEN(0);
	}

	*fdc->sc_reg_drs = DRS_RESET;
	delay(10);
	*fdc->sc_reg_drs = 0;

	if (fdc->sc_flags & FDC_82077) {
		*fdc->sc_reg_dor = FDO_FRST | FDO_FDMAEN | FDO_DS;
	}
#ifdef FD_DEBUG
	if (fdc_debug)
		printf("fdc reset\n");
#endif
}

void
fd_set_motor(fdc)
	struct fdc_softc *fdc;
{
	struct fd_softc *fd;
	u_char status;
	int n;

	if (fdc->sc_flags & FDC_82077) {
		status = FDO_FRST | FDO_FDMAEN;
		if ((fd = TAILQ_FIRST(&fdc->sc_drives)) != NULL)
			status |= fd->sc_drive;

		for (n = 0; n < 4; n++)
			if ((fd = fdc->sc_fd[n]) && (fd->sc_flags & FD_MOTOR))
				status |= FDO_MOEN(n);
		*fdc->sc_reg_dor = status;
	} else {
		for (n = 0; n < 4; n++) {
			if ((fd = fdc->sc_fd[n]) != NULL &&
			    (fd->sc_flags & FD_MOTOR) != 0) {
				auxregbisc(AUXIO4C_FDS, 0);
				return;
			}
		}
		auxregbisc(0, AUXIO4C_FDS);
	}
}

void
fd_motor_off(arg)
	void *arg;
{
	struct fd_softc *fd = arg;
	int s;

	s = splbio();
	fd->sc_flags &= ~(FD_MOTOR | FD_MOTOR_WAIT);
	fd_set_motor((struct fdc_softc *)fd->sc_dv.dv_parent);
	splx(s);
}

void
fd_motor_on(arg)
	void *arg;
{
	struct fd_softc *fd = arg;
	struct fdc_softc *fdc = (void *)fd->sc_dv.dv_parent;
	int s;

	s = splbio();
	fd->sc_flags &= ~FD_MOTOR_WAIT;
	if (fd == TAILQ_FIRST(&fdc->sc_drives) && fdc->sc_state == MOTORWAIT)
		(void) fdcstate(fdc);
	splx(s);
}

/*
 * Get status bytes off the FDC after a command has finished
 * Returns the number of status bytes read; -1 on error.
 * The return value is also stored in `sc_nstat'.
 */
int
fdcresult(fdc)
	struct fdc_softc *fdc;
{
	u_char i;
	int j, n = 0;

	for (j = 100000; j; j--) {
		i = *fdc->sc_reg_msr & (NE7_DIO | NE7_RQM | NE7_CB);
		if (i == NE7_RQM)
			return (fdc->sc_nstat = n);
		if (i == (NE7_DIO | NE7_RQM | NE7_CB)) {
			if (n >= sizeof(fdc->sc_status)) {
				log(LOG_ERR, "fdcresult: overrun\n");
				return (-1);
			}
			fdc->sc_status[n++] = *fdc->sc_reg_fifo;
		} else
			delay(1);
	}

	log(LOG_ERR, "fdcresult: timeout\n");
	return (fdc->sc_nstat = -1);
}

/*
 * Write a command byte to the FDC.
 * Returns 0 on success; -1 on failure (i.e. timeout)
 */
int
fdc_wrfifo(fdc, x)
	struct fdc_softc *fdc;
	u_char x;
{
	int i;

	for (i = 100000; i-- > 0;) {
		if ((*fdc->sc_reg_msr & (NE7_DIO|NE7_RQM)) == NE7_RQM) {
			/* The chip is ready */
			*fdc->sc_reg_fifo = x;
			return (0);
		}
		delay(1);
	}
	return (-1);
}

int
fdopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
 	int unit, pmask;
	struct fd_softc *fd;
	struct fd_type *type;

	unit = FDUNIT(dev);
	if (unit >= fd_cd.cd_ndevs)
		return (ENXIO);
	fd = fd_cd.cd_devs[unit];
	if (fd == NULL)
		return (ENXIO);
	type = fd_dev_to_type(fd, dev);
	if (type == NULL)
		return (ENXIO);

	if ((fd->sc_flags & FD_OPEN) != 0 &&
	    fd->sc_type != type)
		return (EBUSY);

	fd->sc_type = type;
	fd->sc_cylin = -1;
	fd->sc_flags |= FD_OPEN;

	/*
	 * Only update the disklabel if we're not open anywhere else.
	 */
	if (fd->sc_dk.dk_openmask == 0)
		fdgetdisklabel(dev);

	pmask = (1 << FDPART(dev));

	switch (fmt) {
	case S_IFCHR:
		fd->sc_dk.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		fd->sc_dk.dk_bopenmask |= pmask;
		break;
	}
	fd->sc_dk.dk_openmask =
	    fd->sc_dk.dk_copenmask | fd->sc_dk.dk_bopenmask;

	return (0);
}

int
fdclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	int pmask = (1 << FDPART(dev));

	fd->sc_flags &= ~FD_OPEN;
	fd->sc_opts &= ~(FDOPT_NORETRY|FDOPT_SILENT);

	switch (fmt) {
	case S_IFCHR:
		fd->sc_dk.dk_copenmask &= ~pmask;
		break;

	case S_IFBLK:
		fd->sc_dk.dk_bopenmask &= ~pmask;
		break;
	}
	fd->sc_dk.dk_openmask =
	    fd->sc_dk.dk_copenmask | fd->sc_dk.dk_bopenmask;

	return (0);
}

int
fdread(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
	int flag;
{

        return (physio(fdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
fdwrite(dev, uio, flag)
        dev_t dev;
        struct uio *uio;
	int flag;
{

        return (physio(fdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

void
fdcstart(fdc)
	struct fdc_softc *fdc;
{

#ifdef DIAGNOSTIC
	/* only got here if controller's drive queue was inactive; should
	   be in idle state */
	if (fdc->sc_state != DEVIDLE) {
		printf("fdcstart: not idle\n");
		return;
	}
#endif
	(void) fdcstate(fdc);
}

void
fdcstatus(fdc, s)
	struct fdc_softc *fdc;
	char *s;
{
	struct fd_softc *fd = TAILQ_FIRST(&fdc->sc_drives);
	int n;

	/* Just print last status */
	n = fdc->sc_nstat;

#if 0
	/*
	 * A 82072 seems to return <invalid command> on
	 * gratuitous Sense Interrupt commands.
	 */
	if (n == 0 && (fdc->sc_flags & FDC_82077) != 0) {
		fdc_wrfifo(fdc, NE7CMD_SENSEI);
		(void) fdcresult(fdc);
		n = 2;
	}
#endif

	printf("%s: %s: state %d",
	    fd ? fd->sc_dv.dv_xname : "fdc", s, fdc->sc_state);

	switch (n) {
	case 0:
		printf("\n");
		break;
	case 2:
		printf(" (st0 %b cyl %d)\n",
		    fdc->sc_status[0], NE7_ST0BITS,
		    fdc->sc_status[1]);
		break;
	case 7:
		printf(" (st0 %b st1 %b st2 %b cyl %d head %d sec %d)\n",
		    fdc->sc_status[0], NE7_ST0BITS,
		    fdc->sc_status[1], NE7_ST1BITS,
		    fdc->sc_status[2], NE7_ST2BITS,
		    fdc->sc_status[3], fdc->sc_status[4], fdc->sc_status[5]);
		break;
#ifdef DIAGNOSTIC
	default:
		printf(" fdcstatus: weird size: %d\n", n);
		break;
#endif
	}
}

void
fdctimeout(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	struct fd_softc *fd;
	int s;

	s = splbio();
	fd = TAILQ_FIRST(&fdc->sc_drives);
	if (fd == NULL) {
		printf("%s: timeout but no I/O pending: statu %d, istatus=%d\n",
		    fdc->sc_dev.dv_xname, fdc->sc_state, fdc->sc_istatus);
		fdc->sc_state = DEVIDLE;
		goto out;
	}

	if (fd->sc_q.b_actf)
		fdc->sc_state++;
	else
		fdc->sc_state = DEVIDLE;

	(void) fdcstate(fdc);
out:
	splx(s);
}

void
fdcpseudointr(arg)
	void *arg;
{
	struct fdc_softc *fdc = arg;
	int s;

	/* Just ensure it has the right spl. */
	s = splbio();
	(void) fdcstate(fdc);
	splx(s);
}


#ifdef FDC_C_HANDLER
/*
 * hardware interrupt entry point: must be converted to `fast'
 * (in-window) handler.
 */
int
fdc_c_hwintr(fdc)
	struct fdc_softc *fdc;
{

	switch (fdc->sc_itask) {
	case FDC_ITASK_NONE:
		return (0);
	case FDC_ITASK_SENSEI:
		if (fdc_wrfifo(fdc, NE7CMD_SENSEI) != 0 || fdcresult(fdc) == -1)
			fdc->sc_istatus = FDC_ISTATUS_ERROR;
		else
			fdc->sc_istatus = FDC_ISTATUS_DONE;
		FD_SET_SWINTR;
		goto done;
	case FDC_ITASK_RESULT:
		if (fdcresult(fdc) == -1)
			fdc->sc_istatus = FDC_ISTATUS_ERROR;
		else
			fdc->sc_istatus = FDC_ISTATUS_DONE;
		FD_SET_SWINTR;
		goto done;
	case FDC_ITASK_DMA:
		/* Proceed with pseudo-DMA below */
		break;
	default:
		printf("fdc: stray hard interrupt: itask=%d\n", fdc->sc_itask);
		fdc->sc_istatus = FDC_ISTATUS_SPURIOUS;
		FD_SET_SWINTR;
		goto done;
	}

	/*
	 * Pseudo DMA in progress
	 */
	for (;;) {
		register int msr;

		msr = *fdc->sc_reg_msr;

		if ((msr & NE7_RQM) == 0)
			/* That's all this round */
			break;

		if ((msr & NE7_NDM) == 0) {
			fdcresult(fdc);
			fdc->sc_istatus = FDC_ISTATUS_DONE;
			FD_SET_SWINTR;
#ifdef FD_DEBUG
			if (fdc_debug > 1)
				printf("fdc: overrun: tc = %d\n", fdc->sc_tc);
#endif
			break;
		}

		/* Another byte can be transferred */
		if ((msr & NE7_DIO) != 0)
			*fdc->sc_data++ = *fdc->sc_reg_fifo;
		else
			*fdc->sc_reg_fifo = *fdc->sc_data++;
		if (--fdc->sc_tc == 0) {
			fdc->sc_istatus = FDC_ISTATUS_DONE;
			FTC_FLIP;
			fdcresult(fdc);
			FD_SET_SWINTR;
			break;
		}
	}
done:
	return (1);
}
#endif

int
fdcswintr(fdc)
	struct fdc_softc *fdc;
{
	int s;

	if (fdc->sc_istatus == FDC_ISTATUS_NONE)
		/* This (software) interrupt is not for us */
		return (0);

	switch (fdc->sc_istatus) {
	case FDC_ISTATUS_ERROR:
		printf("fdc: ierror status: state %d\n", fdc->sc_state);
		break;
	case FDC_ISTATUS_SPURIOUS:
		printf("fdc: spurious interrupt: state %d\n", fdc->sc_state);
		break;
	}

	s = splbio();
	fdcstate(fdc);
	splx(s);
	return (1);
}

int
fdcstate(fdc)
	struct fdc_softc *fdc;
{
#define	st0	fdc->sc_status[0]
#define	st1	fdc->sc_status[1]
#define	cyl	fdc->sc_status[1]
#define FDC_WRFIFO(fdc, c) \
	do { \
		if (fdc_wrfifo(fdc, (c))) { \
			goto xxx; \
		} \
	} while (0)

	struct fd_softc *fd;
	struct buf *bp;
	int read, head, sec, nblks;
	struct fd_type *type;
	struct fd_formb *finfo = NULL;


	if (fdc->sc_istatus == FDC_ISTATUS_ERROR) {
		/* Prevent loop if the reset sequence produces errors */
		if (fdc->sc_state != RESETCOMPLETE &&
		    fdc->sc_state != RECALWAIT &&
		    fdc->sc_state != RECALCOMPLETE)
			fdc->sc_state = DORESET;
	}

	/* Clear I task/status field */
	fdc->sc_istatus = FDC_ISTATUS_NONE;
	fdc->sc_itask = FDC_ITASK_NONE;

loop:
	/* Is there a drive for the controller to do a transfer with? */
	fd = TAILQ_FIRST(&fdc->sc_drives);
	if (fd == NULL) {
		fdc->sc_state = DEVIDLE;
 		return (0);
	}

	/* Is there a transfer to this drive?  If not, deactivate drive. */
	bp = fd->sc_q.b_actf;
	if (bp == NULL) {
		fd->sc_ops = 0;
		TAILQ_REMOVE(&fdc->sc_drives, fd, sc_drivechain);
		fd->sc_q.b_active = 0;
		goto loop;
	}

	if (bp->b_flags & B_FORMAT)
		finfo = (struct fd_formb *)bp->b_data;

	switch (fdc->sc_state) {
	case DEVIDLE:
		fdc->sc_errors = 0;
		fd->sc_skip = 0;
		fd->sc_bcount = bp->b_bcount;
		fd->sc_blkno = (bp->b_blkno * DEV_BSIZE) / FD_BSIZE(fd);
		timeout_del(&fd->fd_motor_off_to);
		if ((fd->sc_flags & FD_MOTOR_WAIT) != 0) {
			fdc->sc_state = MOTORWAIT;
			return (1);
		}
		if ((fd->sc_flags & FD_MOTOR) == 0) {
			/* Turn on the motor, being careful about pairing. */
			struct fd_softc *ofd = fdc->sc_fd[fd->sc_drive ^ 1];
			if (ofd && ofd->sc_flags & FD_MOTOR) {
				timeout_del(&ofd->fd_motor_off_to);
				ofd->sc_flags &= ~(FD_MOTOR | FD_MOTOR_WAIT);
			}
			fd->sc_flags |= FD_MOTOR | FD_MOTOR_WAIT;
			fd_set_motor(fdc);
			fdc->sc_state = MOTORWAIT;
			if ((fdc->sc_flags & FDC_NEEDMOTORWAIT) != 0) { /* XXX */
				/* Allow .25s for motor to stabilize. */
				timeout_add(&fd->fd_motor_on_to, hz / 4);
			} else {
				fd->sc_flags &= ~FD_MOTOR_WAIT;
				goto loop;
			}
			return (1);
		}
		/* Make sure the right drive is selected. */
		fd_set_motor(fdc);

		/*FALLTHROUGH*/
	case DOSEEK:
	doseek:
		if ((fdc->sc_flags & FDC_EIS) &&
		    (bp->b_flags & B_FORMAT) == 0) {
			fd->sc_cylin = bp->b_cylinder;
			/* We use implied seek */
			goto doio;
		}

		if (fd->sc_cylin == bp->b_cylinder)
			goto doio;

		fd->sc_cylin = -1;
		fdc->sc_state = SEEKWAIT;
		fdc->sc_nstat = 0;

		fd->sc_dk.dk_seek++;

		disk_busy(&fd->sc_dk);
		timeout_add(&fdc->fdctimeout_to, 4 * hz);

		/* specify command */
		FDC_WRFIFO(fdc, NE7CMD_SPECIFY);
		FDC_WRFIFO(fdc, fd->sc_type->steprate);
		/* XXX head load time == 6ms */
		FDC_WRFIFO(fdc, 6 | NE7_SPECIFY_NODMA);

		fdc->sc_itask = FDC_ITASK_SENSEI;
		/* seek function */
		FDC_WRFIFO(fdc, NE7CMD_SEEK);
		FDC_WRFIFO(fdc, fd->sc_drive); /* drive number */
		FDC_WRFIFO(fdc, bp->b_cylinder * fd->sc_type->step);

		return (1);

	case DOIO:
	doio:
		if (finfo != NULL)
			fd->sc_skip = (char *)&(finfo->fd_formb_cylno(0)) -
				      (char *)finfo;
		type = fd->sc_type;
		sec = fd->sc_blkno % type->seccyl;
		nblks = type->seccyl - sec;
		nblks = min(nblks, fd->sc_bcount / FD_BSIZE(fd));
		nblks = min(nblks, FDC_MAXIOSIZE / FD_BSIZE(fd));
		fd->sc_nblks = nblks;
		fd->sc_nbytes = finfo ? bp->b_bcount : nblks * FD_BSIZE(fd);
		head = sec / type->sectrac;
		sec -= head * type->sectrac;
#ifdef DIAGNOSTIC
		{int block;
		 block = (fd->sc_cylin * type->heads + head) * type->sectrac + sec;
		 if (block != fd->sc_blkno) {
			 printf("fdcintr: block %d != blkno %d\n", block, fd->sc_blkno);
#if defined(FD_DEBUG) && defined(DDB)
			 Debugger();
#endif
		 }}
#endif
		read = bp->b_flags & B_READ;

		/* Setup for pseudo DMA */
		fdc->sc_data = bp->b_data + fd->sc_skip;
		fdc->sc_tc = fd->sc_nbytes;

		*fdc->sc_reg_drs = type->rate;
#ifdef FD_DEBUG
		if (fdc_debug > 1)
			printf("fdcstate: doio: %s drive %d "
			       "track %d head %d sec %d nblks %d\n",
				finfo ? "format" :
					(read ? "read" : "write"),
				fd->sc_drive, fd->sc_cylin, head, sec, nblks);
#endif
		fdc->sc_state = IOCOMPLETE;
		fdc->sc_itask = FDC_ITASK_DMA;
		fdc->sc_nstat = 0;

		disk_busy(&fd->sc_dk);

		/* allow 3 seconds for operation */
		timeout_add(&fdc->fdctimeout_to, 3 * hz);

		if (finfo != NULL) {
			/* formatting */
			FDC_WRFIFO(fdc, NE7CMD_FORMAT);
			FDC_WRFIFO(fdc, (head << 2) | fd->sc_drive);
			FDC_WRFIFO(fdc, finfo->fd_formb_secshift);
			FDC_WRFIFO(fdc, finfo->fd_formb_nsecs);
			FDC_WRFIFO(fdc, finfo->fd_formb_gaplen);
			FDC_WRFIFO(fdc, finfo->fd_formb_fillbyte);
		} else {
			if (read)
				FDC_WRFIFO(fdc, NE7CMD_READ);
			else
				FDC_WRFIFO(fdc, NE7CMD_WRITE);
			FDC_WRFIFO(fdc, (head << 2) | fd->sc_drive);
			FDC_WRFIFO(fdc, fd->sc_cylin);	/*track*/
			FDC_WRFIFO(fdc, head);
			FDC_WRFIFO(fdc, sec + 1);	/*sector+1*/
			FDC_WRFIFO(fdc, type->secsize);	/*sector size*/
			FDC_WRFIFO(fdc, type->sectrac);	/*secs/track*/
			FDC_WRFIFO(fdc, type->gap1);	/*gap1 size*/
			FDC_WRFIFO(fdc, type->datalen);	/*data length*/
		}

		return (1);				/* will return later */

	case SEEKWAIT:
		timeout_del(&fdc->fdctimeout_to);
		fdc->sc_state = SEEKCOMPLETE;
		if (fdc->sc_flags & FDC_NEEDHEADSETTLE) {
			/* allow 1/50 second for heads to settle */
			timeout_add(&fdc->fdcpseudointr_to, hz / 50);
			return (1);		/* will return later */
		}
		/*FALLTHROUGH*/
	case SEEKCOMPLETE:
		disk_unbusy(&fd->sc_dk, 0, 0);	/* no data on seek */

		/* Make sure seek really happened. */
		if (fdc->sc_nstat != 2 || (st0 & 0xf8) != 0x20 ||
		    cyl != bp->b_cylinder * fd->sc_type->step) {
#ifdef FD_DEBUG
			if (fdc_debug)
				fdcstatus(fdc, "seek failed");
#endif
			fdcretry(fdc);
			goto loop;
		}
		fd->sc_cylin = bp->b_cylinder;
		goto doio;

	case IOTIMEDOUT:
		/*
		 * Try to abort the I/O operation without resetting
		 * the chip first.  Poke TC and arrange to pick up
		 * the timed out I/O command's status.
		 */
		fdc->sc_itask = FDC_ITASK_RESULT;
		fdc->sc_state = IOCLEANUPWAIT;
		fdc->sc_nstat = 0;
		/* 1/10 second should be enough */
		timeout_add(&fdc->fdctimeout_to, hz / 10);
		return (1);

	case IOCLEANUPTIMEDOUT:
	case SEEKTIMEDOUT:
	case RECALTIMEDOUT:
	case RESETTIMEDOUT:
		fdcstatus(fdc, "timeout");

		/* All other timeouts always roll through a chip reset */
		fdcretry(fdc);

		/* Force reset, no matter what fdcretry() says */
		fdc->sc_state = DORESET;
		goto loop;

	case IOCLEANUPWAIT: /* IO FAILED, cleanup succeeded */
		timeout_del(&fdc->fdctimeout_to);
		disk_unbusy(&fd->sc_dk, (bp->b_bcount - bp->b_resid),
		    (bp->b_flags & B_READ));
		fdcretry(fdc);
		goto loop;

	case IOCOMPLETE: /* IO DONE, post-analyze */
		timeout_del(&fdc->fdctimeout_to);

		disk_unbusy(&fd->sc_dk, (bp->b_bcount - bp->b_resid),
		    (bp->b_flags & B_READ));

		if (fdc->sc_nstat != 7 || st1 != 0 ||
		    ((st0 & 0xf8) != 0 &&
		     ((st0 & 0xf8) != 0x20 || (fdc->sc_cfg & CFG_EIS) == 0))) {
#ifdef FD_DEBUG
			if (fdc_debug) {
				fdcstatus(fdc,
					bp->b_flags & B_READ
					? "read failed" : "write failed");
				printf("blkno %d nblks %d nstat %d tc %d\n",
				       fd->sc_blkno, fd->sc_nblks,
				       fdc->sc_nstat, fdc->sc_tc);
			}
#endif
			if (fdc->sc_nstat == 7 &&
			    (st1 & ST1_OVERRUN) == ST1_OVERRUN) {

				/*
				 * Silently retry overruns if no other
				 * error bit is set. Adjust threshold.
				 */
				int thr = fdc->sc_cfg & CFG_THRHLD_MASK;
				if (thr < 15) {
					thr++;
					fdc->sc_cfg &= ~CFG_THRHLD_MASK;
					fdc->sc_cfg |= (thr & CFG_THRHLD_MASK);
#ifdef FD_DEBUG
					if (fdc_debug)
						printf("fdc: %d -> threshold\n", thr);
#endif
					fdconf(fdc);
					fdc->sc_overruns = 0;
				}
				if (++fdc->sc_overruns < 3) {
					fdc->sc_state = DOIO;
					goto loop;
				}
			}
			fdcretry(fdc);
			goto loop;
		}
		if (fdc->sc_errors) {
			diskerr(bp, "fd", "soft error", LOG_PRINTF,
			    fd->sc_skip / FD_BSIZE(fd),
			    (struct disklabel *)NULL);
			printf("\n");
			fdc->sc_errors = 0;
		} else {
			if (--fdc->sc_overruns < -20) {
				int thr = fdc->sc_cfg & CFG_THRHLD_MASK;
				if (thr > 0) {
					thr--;
					fdc->sc_cfg &= ~CFG_THRHLD_MASK;
					fdc->sc_cfg |= (thr & CFG_THRHLD_MASK);
#ifdef FD_DEBUG
					if (fdc_debug)
						printf("fdc: %d -> threshold\n", thr);
#endif
					fdconf(fdc);
				}
				fdc->sc_overruns = 0;
			}
		}
		fd->sc_blkno += fd->sc_nblks;
		fd->sc_skip += fd->sc_nbytes;
		fd->sc_bcount -= fd->sc_nbytes;
		if (finfo == NULL && fd->sc_bcount > 0) {
			bp->b_cylinder = fd->sc_blkno / fd->sc_type->seccyl;
			goto doseek;
		}
		fdfinish(fd, bp);
		goto loop;

	case DORESET:
		/* try a reset, keep motor on */
		fd_set_motor(fdc);
		delay(100);
		fdc->sc_nstat = 0;
		fdc->sc_itask = FDC_ITASK_SENSEI;
		fdc->sc_state = RESETCOMPLETE;
		timeout_add(&fdc->fdctimeout_to, hz / 2);
		fdc_reset(fdc);
		return (1);			/* will return later */

	case RESETCOMPLETE:
		timeout_del(&fdc->fdctimeout_to);
		fdconf(fdc);

		/* FALLTHROUGH */
	case DORECAL:
		fdc->sc_state = RECALWAIT;
		fdc->sc_itask = FDC_ITASK_SENSEI;
		fdc->sc_nstat = 0;
		timeout_add(&fdc->fdctimeout_to, 5 * hz);
		/* recalibrate function */
		FDC_WRFIFO(fdc, NE7CMD_RECAL);
		FDC_WRFIFO(fdc, fd->sc_drive);
		return (1);			/* will return later */

	case RECALWAIT:
		timeout_del(&fdc->fdctimeout_to);
		fdc->sc_state = RECALCOMPLETE;
		if (fdc->sc_flags & FDC_NEEDHEADSETTLE) {
			/* allow 1/30 second for heads to settle */
			timeout_add(&fdc->fdcpseudointr_to, hz / 30);
			return (1);		/* will return later */
		}

	case RECALCOMPLETE:
		if (fdc->sc_nstat != 2 || (st0 & 0xf8) != 0x20 || cyl != 0) {
#ifdef FD_DEBUG
			if (fdc_debug)
				fdcstatus(fdc, "recalibrate failed");
#endif
			fdcretry(fdc);
			goto loop;
		}
		fd->sc_cylin = 0;
		goto doseek;

	case MOTORWAIT:
		if (fd->sc_flags & FD_MOTOR_WAIT)
			return (1);		/* time's not up yet */
		goto doseek;

	default:
		fdcstatus(fdc, "stray interrupt");
		return (1);
	}
#ifdef DIAGNOSTIC
	panic("fdcintr: impossible");
#endif

xxx:
	/*
	 * We get here if the chip locks up in FDC_WRFIFO()
	 * Cancel any operation and schedule a reset.
	 */
	timeout_del(&fdc->fdctimeout_to);
	fdcretry(fdc);
	fdc->sc_state = DORESET;
	goto loop;

#undef	st0
#undef	st1
#undef	cyl
}

void
fdcretry(fdc)
	struct fdc_softc *fdc;
{
	struct fd_softc *fd;
	struct buf *bp;
	int error = EIO;

	fd = TAILQ_FIRST(&fdc->sc_drives);
	bp = fd->sc_q.b_actf;

	fdc->sc_overruns = 0;
	if (fd->sc_opts & FDOPT_NORETRY)
		goto fail;

	switch (fdc->sc_errors) {
	case 0:
		if (fdc->sc_nstat == 7 &&
		    (fdc->sc_status[0] & 0xd8) == 0x40 &&
		    (fdc->sc_status[1] & 0x2) == 0x2) {
			printf("%s: read-only medium\n", fd->sc_dv.dv_xname);
			error = EROFS;
			goto failsilent;
		}
		/* try again */
		fdc->sc_state =
			(fdc->sc_flags & FDC_EIS) ? DOIO : DOSEEK;
		break;

	case 1: case 2: case 3:
		/* didn't work; try recalibrating */
		fdc->sc_state = DORECAL;
		break;

	case 4:
		if (fdc->sc_nstat == 7 &&
		    fdc->sc_status[0] == 0 &&
		    fdc->sc_status[1] == 0 &&
		    fdc->sc_status[2] == 0) {
			/*
			 * We've retried a few times and we've got
			 * valid status and all three status bytes
			 * are zero.  Assume this condition is the
			 * result of no disk loaded into the drive.
			 */
			printf("%s: no medium?\n", fd->sc_dv.dv_xname);
			error = ENODEV;
			goto failsilent;
		}

		/* still no go; reset the bastard */
		fdc->sc_state = DORESET;
		break;

	default:
	fail:
		if ((fd->sc_opts & FDOPT_SILENT) == 0) {
			diskerr(bp, "fd", "hard error", LOG_PRINTF,
				fd->sc_skip / FD_BSIZE(fd),
				(struct disklabel *)NULL);
			printf("\n");
			fdcstatus(fdc, "controller status");
		}

	failsilent:
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
		fdfinish(fd, bp);
	}
	fdc->sc_errors++;
}

int
fdsize(dev)
	dev_t dev;
{

	/* Swapping to floppies would not make sense. */
	return (-1);
}

int
fddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return (EINVAL);
}

int
fdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	int error;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *(fd->sc_dk.dk_label);
		return 0;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		/* XXX do something */
		return (0);

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);

		error = setdisklabel(fd->sc_dk.dk_label,
				    (struct disklabel *)addr, 0,
				    fd->sc_dk.dk_cpulabel);
		if (error)
			return (error);

		error = writedisklabel(dev, fdstrategy,
				       fd->sc_dk.dk_label,
				       fd->sc_dk.dk_cpulabel);
		return (error);

	case DIOCLOCK:
		/*
		 * Nothing to do here, really.
		 */
		return (0);

	case MTIOCTOP:
		if (((struct mtop *)addr)->mt_op != MTOFFL)
			return EIO;

#ifdef COMPAT_SUNOS
	case SUNOS_FDIOCEJECT:
#endif
	case DIOCEJECT:
		fd_do_eject(fd);
		return (0);

	case FD_FORM:
		if((flag & FWRITE) == 0)
			return EBADF;	/* must be opened for writing */
		else if(((struct fd_formb *)addr)->format_version !=
		    FD_FORMAT_VERSION)
			return EINVAL;	/* wrong version of formatting prog */
		else
			return fdformat(dev, (struct fd_formb *)addr, p);
		break;

	case FD_GTYPE:			/* get drive type */
		*(struct fd_type *)addr = *fd->sc_type;
		return (0);

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd->sc_opts;
		return (0);

	case FD_SOPTS:			/* set drive options */
		fd->sc_opts = *(int *)addr;
		return (0);

#ifdef FD_DEBUG
	case _IO('f', 100):
		{
		int i;
		struct fdc_softc *fdc = (struct fdc_softc *)
					fd->sc_dv.dv_parent;

		fdc_wrfifo(fdc, NE7CMD_DUMPREG);
		fdcresult(fdc);
		printf("dumpreg(%d regs): <", fdc->sc_nstat);
		for (i = 0; i < fdc->sc_nstat; i++)
			printf(" 0x%x", fdc->sc_status[i]);
		printf(">\n");
		}

		return (0);
	case _IOW('f', 101, int):
		{
		struct fdc_softc *fdc = (struct fdc_softc *)
					fd->sc_dv.dv_parent;
		fdc->sc_cfg &= ~CFG_THRHLD_MASK;
		fdc->sc_cfg |= (*(int *)addr & CFG_THRHLD_MASK);
		fdconf(fdc);
		}
		return (0);
	case _IO('f', 102):
		{
		int i;
		struct fdc_softc *fdc = (struct fdc_softc *)
					fd->sc_dv.dv_parent;
		fdc_wrfifo(fdc, NE7CMD_SENSEI);
		fdcresult(fdc);
		printf("sensei(%d regs): <", fdc->sc_nstat);
		for (i = 0; i < fdc->sc_nstat; i++)
			printf(" 0x%x", fdc->sc_status[i]);
		}
		printf(">\n");
		return (0);
#endif
	default:
		return (ENOTTY);
	}
}

int
fdformat(dev, finfo, p)
	dev_t dev;
	struct fd_formb *finfo;
	struct proc *p;
{
	int rv = 0;
	struct fd_softc *fd = fd_cd.cd_devs[FDUNIT(dev)];
	struct fd_type *type = fd->sc_type;
	struct buf *bp;

	/* set up a buffer header for fdstrategy() */
	bp = (struct buf *)malloc(sizeof(struct buf), M_TEMP, M_NOWAIT);
	if (bp == 0)
		return (ENOBUFS);

	PHOLD(p);
	bzero((void *)bp, sizeof(struct buf));
	bp->b_flags = B_BUSY | B_PHYS | B_FORMAT;
	bp->b_proc = p;
	bp->b_dev = dev;

	/*
	 * Calculate a fake blkno, so fdstrategy() would initiate a
	 * seek to the requested cylinder.
	 */
	bp->b_blkno = ((finfo->cyl * (type->sectrac * type->heads)
		       + finfo->head * type->sectrac) * FD_BSIZE(fd))
		       / DEV_BSIZE;

	bp->b_bcount = sizeof(struct fd_idfield_data) * finfo->fd_formb_nsecs;
	bp->b_data = (caddr_t)finfo;

#ifdef FD_DEBUG
	if (fdc_debug) {
		int i;

		printf("fdformat: blkno 0x%x count %ld\n",
			bp->b_blkno, bp->b_bcount);

		printf("\tcyl:\t%d\n", finfo->cyl);
		printf("\thead:\t%d\n", finfo->head);
		printf("\tnsecs:\t%d\n", finfo->fd_formb_nsecs);
		printf("\tsshft:\t%d\n", finfo->fd_formb_secshift);
		printf("\tgaplen:\t%d\n", finfo->fd_formb_gaplen);
		printf("\ttrack data:");
		for (i = 0; i < finfo->fd_formb_nsecs; i++) {
			printf(" [c%d h%d s%d]",
					finfo->fd_formb_cylno(i),
					finfo->fd_formb_headno(i),
					finfo->fd_formb_secno(i) );
			if (finfo->fd_formb_secsize(i) != 2)
				printf("<sz:%d>", finfo->fd_formb_secsize(i));
		}
		printf("\n");
	}
#endif

	/* now do the format */
	fdstrategy(bp);

	/* ...and wait for it to complete */
	rv = biowait(bp);
	PRELE(p);
	free(bp, M_TEMP);
	return (rv);
}

void
fdgetdisklabel(dev)
	dev_t dev;
{
	int unit = FDUNIT(dev);
	struct fd_softc *fd = fd_cd.cd_devs[unit];
	struct disklabel *lp = fd->sc_dk.dk_label;
	struct cpu_disklabel *clp = fd->sc_dk.dk_cpulabel;
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(clp, sizeof(struct cpu_disklabel));

	lp->d_type = DTYPE_FLOPPY;
	lp->d_secsize = FD_BSIZE(fd);
	lp->d_secpercyl = fd->sc_type->seccyl;
	lp->d_nsectors = fd->sc_type->sectrac;
	lp->d_ncylinders = fd->sc_type->tracks;
	lp->d_ntracks = fd->sc_type->heads;	/* Go figure... */
	lp->d_secperunit = fd->sc_type->size;
	lp->d_rpm = 300;	/* XXX like it matters... */

	strncpy(lp->d_typename, "floppy disk", sizeof(lp->d_typename));
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secpercyl * lp->d_ncylinders;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	errstring = readdisklabel(dev, fdstrategy, lp, clp, 0);
	if (errstring) {
		printf("%s: %s\n", fd->sc_dv.dv_xname, errstring);
	}
}

void
fd_do_eject(fd)
	struct fd_softc *fd;
{
	struct fdc_softc *fdc = (void *)fd->sc_dv.dv_parent;

	if (CPU_ISSUN4C) {
		auxregbisc(AUXIO4C_FDS, AUXIO4C_FEJ);
		delay(10);
		auxregbisc(AUXIO4C_FEJ, AUXIO4C_FDS);
		return;
	}
	if (CPU_ISSUN4M && (fdc->sc_flags & FDC_82077) != 0) {
		int dor = FDO_FRST | FDO_FDMAEN | FDO_MOEN(0);
		*fdc->sc_reg_dor = dor | FDO_EJ;
		delay(10);
		*fdc->sc_reg_dor = FDO_FRST | FDO_DS;
		return;
	}
}
