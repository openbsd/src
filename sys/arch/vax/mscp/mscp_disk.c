/*	$OpenBSD: mscp_disk.c,v 1.6 1998/10/03 21:18:59 millert Exp $	*/
/*	$NetBSD: mscp_disk.c,v 1.13 1997/06/24 01:12:40 thorpej Exp $	*/
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * RA disk device driver
 */

/*
 * TODO
 *	write bad block forwarding code
 *	split the file into a separate floppy file
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <machine/cpu.h>
#include <machine/rpb.h>

#include <ufs/ffs/fs.h> /* For some disklabel stuff */

#include <vax/mscp/mscp.h>
#include <vax/mscp/mscpvar.h>

/*
 * Drive status, per drive
 */
struct ra_softc {
	struct	device ra_dev;	/* Autoconf struct */
	struct	disk ra_disk;
	int	ra_state;	/* open/closed state */
	u_long	ra_mediaid;	/* media id */
	int	ra_hwunit;	/* Hardware unit number */
	int	ra_havelabel;	/* true if we have a label */
	int	ra_wlabel;	/* label sector is currently writable */
	int	ra_isafloppy;	/* unit is a floppy disk */
};

int	ramatch __P((struct device *, void *, void *));
void	raattach __P((struct device *, struct device *, void *));
void	radgram __P((struct device *, struct mscp *, struct mscp_softc *));
void	raiodone __P((struct device *, struct buf *));
int	raonline __P((struct device *, struct mscp *));
int	ragotstatus __P((struct device *, struct mscp *));
void	rareplace __P((struct device *, struct mscp *));
int	raioerror __P((struct device *, struct mscp *, struct buf *));
void	rafillin __P((struct buf *, struct mscp *));
void	rabb __P((struct device *, struct mscp *, struct buf *));
int	raopen __P((dev_t, int, int, struct proc *));
int	raclose __P((dev_t, int, int, struct proc *));
void	rastrategy __P((struct buf *));
void	rastrat1 __P((struct buf *));
int	raread __P((dev_t, struct uio *));
int	rawrite __P((dev_t, struct uio *));
int	raioctl __P((dev_t, int, caddr_t, int, struct proc *));
int	radump __P((dev_t, daddr_t, caddr_t, size_t));
int	rasize __P((dev_t));
int	ra_putonline __P((struct ra_softc *));


struct	mscp_device ra_device = {
	radgram,
	raiodone,
	raonline,
	ragotstatus,
	rareplace,
	raioerror,
	rabb,
	rafillin,
};

/*
 * Device to unit number and partition and back
 */
#define	UNITSHIFT	3
#define	UNITMASK	7
#define	raunit(dev)	(minor(dev) >> UNITSHIFT)
#define	rapart(dev)	(minor(dev) & UNITMASK)
#define	raminor(u, p)	(((u) << UNITSHIFT) | (p))

struct	cfdriver ra_cd = {
	NULL, "ra", DV_DISK
};

struct	cfattach ra_ca = {
	sizeof(struct ra_softc), ramatch, raattach
};

/*
 * Software state, per drive
 */
#define	RA_OFFLINE	0
#define	RA_WANTOPEN	1
#define	RA_ONLINE	3

/*
 * More driver definitions, for generic MSCP code.
 */
extern int cold;

int
ramatch(parent, match, aux)
	struct	device *parent;
	void	*match, *aux;
{
	struct	cfdata *cf = match;
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != mp->mscp_unit)
		return 0;
	return 1;
}

/*
 * The attach routine only checks and prints drive type.
 * Bringing the disk online is done when the disk is accessed
 * the first time. 
 */
void
raattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux; 
{
	struct	ra_softc *ra = (void *)self;
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;
	struct	mscp_softc *mi = (void *)parent;
	struct	disklabel *dl;

	ra->ra_mediaid = mp->mscp_guse.guse_mediaid;
	ra->ra_state = RA_OFFLINE;
	ra->ra_havelabel = 0;
	ra->ra_hwunit = mp->mscp_unit;
	mi->mi_dp[mp->mscp_unit] = self;

	ra->ra_disk.dk_name = ra->ra_dev.dv_xname;
	disk_attach((struct disk *)&ra->ra_disk);

	/* Fill in what we know. The actual size is gotten later */
	dl = ra->ra_disk.dk_label;

	dl->d_secsize = DEV_BSIZE;
	dl->d_nsectors = mp->mscp_guse.guse_nspt;
	dl->d_ntracks = mp->mscp_guse.guse_ngpc;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	disk_printtype(mp->mscp_unit, mp->mscp_guse.guse_mediaid);
	/*
	 * Find out if we booted from this disk.
	 */
	if ((B_TYPE(bootdev) == BDEV_UDA) && (ra->ra_hwunit == B_UNIT(bootdev))
	    && (mi->mi_ctlrnr == B_CONTROLLER(bootdev))
	    && (mi->mi_adapnr == B_ADAPTOR(bootdev)))
		booted_from = self;
}

/* 
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it har fallen offline.
 */
int
ra_putonline(ra)
	struct ra_softc *ra;
{
	struct	mscp *mp;
	struct	mscp_softc *mi = (struct mscp_softc *)ra->ra_dev.dv_parent;
	struct	disklabel *dl;
	volatile int i;
	char *msg;

	dl = ra->ra_disk.dk_label;

	ra->ra_state = RA_WANTOPEN;
	mp = mscp_getcp(mi, MSCP_WAIT);
	mp->mscp_opcode = M_OP_ONLINE;
	mp->mscp_unit = ra->ra_hwunit;
	mp->mscp_cmdref = (long)&ra->ra_state;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;

	/* Poll away */
	i = *mi->mi_ip;
	if (tsleep(&ra->ra_state, PRIBIO, "raonline", 100*100)) {
		ra->ra_state = RA_OFFLINE;
		return MSCP_FAILED;
	}

	if (ra->ra_state == RA_OFFLINE)
		return MSCP_FAILED;
	if (ra->ra_isafloppy)
		return MSCP_DONE;

	printf("%s", ra->ra_dev.dv_xname);
	if ((msg = readdisklabel(raminor(ra->ra_dev.dv_unit, 0),
	    rastrategy, dl, NULL, 0)) != NULL)
		printf(": %s", msg);
	else
		ra->ra_havelabel = 1;
	ra->ra_state = RA_ONLINE;

	printf(": size %d sectors\n", dl->d_secperunit);

	return MSCP_DONE;
}
/*
 * Open a drive.
 */
/*ARGSUSED*/
int
raopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct	proc *p;
{
	register struct ra_softc *ra;
	int part, unit, mask;

	/*
	 * Make sure this is a reasonable open request.
	 */
	unit = raunit(dev);
	if (unit >= ra_cd.cd_ndevs)
		return ENXIO;
	ra = ra_cd.cd_devs[unit];
	if (ra == 0)
		return ENXIO;

	/*
	 * If this is the first open; we must first try to put
	 * the disk online (and read the label).
	 */
	if (ra->ra_state == RA_OFFLINE)
		if (ra_putonline(ra) == MSCP_FAILED)
			return EIO;

	/* If the disk has no label; allow writing everywhere */
	if (ra->ra_havelabel == 0)
		ra->ra_wlabel = 1;

	part = rapart(dev);
	if (ra->ra_isafloppy == 0)
	        if (part >= ra->ra_disk.dk_label->d_npartitions)
			return ENXIO;

	/*
	 * Wait for the state to settle
	 */
#if notyet
	while (ra->ra_state != RA_ONLINE)
		if ((error = tsleep((caddr_t)ra, (PZERO + 1) | PCATCH,
		    devopn, 0))) {
			splx(s);
			return (error);
		}
#endif

	mask = 1 << part;

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask |= mask;
		break;
	}
	ra->ra_disk.dk_openmask |= mask;
	return 0;
}

/* ARGSUSED */
int
raclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct	proc *p;
{
	register int unit = raunit(dev);
	register struct ra_softc *ra = ra_cd.cd_devs[unit];
	int mask = (1 << rapart(dev));

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask &= ~mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask &= ~mask;
		break;
	}
	ra->ra_disk.dk_openmask =
	    ra->ra_disk.dk_copenmask | ra->ra_disk.dk_bopenmask;

	/*
	 * Should wait for I/O to complete on this partition even if
	 * others are open, but wait for work on blkflush().
	 */
#if notyet
	if (ra->ra_openpart == 0) {
		s = splbio();
		while (udautab[unit].b_actf)
			sleep((caddr_t)&udautab[unit], PZERO - 1);
		splx(s);
		ra->ra_state = CLOSED;
		ra->ra_wlabel = 0;
	}
#endif
	return (0);
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 *
 * This routine is broken into two so that the internal version
 * udastrat1() can be called by the (nonexistent, as yet) bad block
 * revectoring routine.
 */
void
rastrategy(bp)
	register struct buf *bp;
{
	register int unit;
	register struct ra_softc *ra;
	int p;
	/*
	 * Make sure this is a reasonable drive to use.
	 */
	unit = raunit(bp->b_dev);
	if (unit > ra_cd.cd_ndevs || (ra = ra_cd.cd_devs[unit]) == NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}
	/*
	 * If drive is open `raw' or reading label, let it at it.
	 */
	if (ra->ra_state < RA_ONLINE) {
		mscp_strategy(bp, ra->ra_dev.dv_parent);
		return;
	}
	p = rapart(bp->b_dev);

	/*
	 * Determine the size of the transfer, and make sure it is
	 * within the boundaries of the partition.
	 */
	if (ra->ra_isafloppy) {
		if (bp->b_blkno >= ra->ra_disk.dk_label->d_secperunit) {
			bp->b_resid = bp->b_bcount;
			goto done;
		}
	} else
		if (bounds_check_with_label(bp, ra->ra_disk.dk_label,
		    ra->ra_disk.dk_cpulabel, ra->ra_wlabel) <= 0)
			goto done;

	/* Make some statistics... /bqt */
	ra->ra_disk.dk_xfer++;
	ra->ra_disk.dk_bytes += bp->b_bcount;
	mscp_strategy(bp, ra->ra_dev.dv_parent);
	return;

done:
	biodone(bp);
}

int
raread(dev, uio)
        dev_t dev;
        struct uio *uio;
{

        return (physio(rastrategy, NULL, dev, B_READ, minphys, uio));
}

int
rawrite(dev, uio)
        dev_t dev;
        struct uio *uio;
{

        return (physio(rastrategy, NULL, dev, B_WRITE, minphys, uio));
}

void
raiodone(usc, bp)
	struct device *usc;
	struct buf *bp;
{

	biodone(bp);
}

/*
 * Fill in disk addresses in a mscp packet waiting for transfer.
 */
void
rafillin(bp, mp)
	struct buf *bp;
	struct mscp *mp;
{
	int unit = raunit(bp->b_dev);
	int part = rapart(bp->b_dev);
	struct ra_softc *ra = ra_cd.cd_devs[unit];
	struct disklabel *lp = ra->ra_disk.dk_label;

	
	/* XXX more checks needed */
	mp->mscp_unit = ra->ra_hwunit;
	mp->mscp_seq.seq_lbn = bp->b_blkno + lp->d_partitions[part].p_offset;
	mp->mscp_seq.seq_bytecount = bp->b_bcount;
}

/*
 * Handle an error datagram.
 * This can come from an unconfigured drive as well.
 */
void
radgram(usc, mp, mi)
	struct device *usc;
	struct mscp *mp;
	struct mscp_softc *mi;
{
	if (mscp_decodeerror(usc == NULL?"unconf ra" : usc->dv_xname, mp, mi))
		return;
	/*
	 * SDI status information bytes 10 and 11 are the microprocessor
	 * error code and front panel code respectively.  These vary per
	 * drive type and are printed purely for field service information.
	 */
	if (mp->mscp_format == M_FM_SDI)
		printf("\tsdi uproc error code 0x%x, front panel code 0x%x\n",
			mp->mscp_erd.erd_sdistat[10],
			mp->mscp_erd.erd_sdistat[11]);
}

/*
 * A drive came on line.  Check its type and size.  Return DONE if
 * we think the drive is truly on line.  In any case, awaken anyone
 * sleeping on the drive on-line-ness.
 */
int
raonline(usc, mp)
	struct device *usc;
	struct mscp *mp;
{
	register struct ra_softc *ra = (void *)usc;
	struct disklabel *dl;
	int p = 0, d, n;

	wakeup((caddr_t)&ra->ra_state);
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		printf("%s: attempt to bring on line failed: ", 
		    ra->ra_dev.dv_xname);
		mscp_printevent(mp);
		ra->ra_state = RA_OFFLINE;
		return (MSCP_FAILED);
	}

	/*
	 * Fill in the rest of disk size.
	 */
	ra->ra_state = RA_WANTOPEN;
	dl = ra->ra_disk.dk_label;
	dl->d_secperunit = (daddr_t)mp->mscp_onle.onle_unitsize;

	if (dl->d_secpercyl != 0)
		dl->d_ncylinders = dl->d_secperunit/dl->d_secpercyl;
	else
		ra->ra_isafloppy = 1;
	dl->d_type = DTYPE_MSCP;
	dl->d_rpm = 3600;
	dl->d_bbsize = BBSIZE;
	dl->d_sbsize = SBSIZE;

	/* Create the disk name for disklabel. Phew... */
	d = ra->ra_mediaid;
	dl->d_typename[p++] = MSCP_MID_CHAR(2, d);
	dl->d_typename[p++] = MSCP_MID_CHAR(1, d);
	if (MSCP_MID_ECH(0, d))
		dl->d_typename[p++] = MSCP_MID_CHAR(0, d);
	n = MSCP_MID_NUM(d);
	if (n > 99) {
		dl->d_typename[p++] = '1';
		n -= 100;
	}
	if (n > 9) {
		dl->d_typename[p++] = (n / 10) + '0';
		n %= 10;
	}
	dl->d_typename[p++] = n + '0';
	dl->d_typename[p] = 0;
	dl->d_npartitions = MAXPARTITIONS;
	dl->d_partitions[0].p_size = dl->d_partitions[2].p_size =
	    dl->d_secperunit;
	dl->d_partitions[0].p_offset = dl->d_partitions[2].p_offset = 0;
	dl->d_interleave = dl->d_headswitch = 1;
	dl->d_magic = dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);

	return (MSCP_DONE);
}

/*
 * We got some (configured) unit's status.  Return DONE if it succeeded.
 */
int
ragotstatus(usc, mp)
	register struct device *usc;
	register struct mscp *mp;
{
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		printf("%s: attempt to get status failed: ", usc->dv_xname);
		mscp_printevent(mp);
		return (MSCP_FAILED);
	}
	/* record for (future) bad block forwarding and whatever else */
#ifdef notyet
	uda_rasave(ui->ui_unit, mp, 1);
#endif
	return (MSCP_DONE);
}

/*
 * A transfer failed.  We get a chance to fix or restart it.
 * Need to write the bad block forwaring code first....
 */
/*ARGSUSED*/
int
raioerror(usc, mp, bp)
	register struct device *usc;
	register struct mscp *mp;
	struct buf *bp;
{
printf("raioerror\n");
#if 0
	if (mp->mscp_flags & M_EF_BBLKR) {
		/*
		 * A bad block report.  Eventually we will
		 * restart this transfer, but for now, just
		 * log it and give up.
		 */
		log(LOG_ERR, "ra%d: bad block report: %d%s\n",
			ui->ui_unit, (int)mp->mscp_seq.seq_lbn,
			mp->mscp_flags & M_EF_BBLKU ? " + others" : "");
	} else {
		/*
		 * What the heck IS a `serious exception' anyway?
		 * IT SURE WOULD BE NICE IF DEC SOLD DOCUMENTATION
		 * FOR THEIR OWN CONTROLLERS.
		 */
		if (mp->mscp_flags & M_EF_SEREX)
			log(LOG_ERR, "ra%d: serious exception reported\n",
				ui->ui_unit);
	}
#endif
	return (MSCP_FAILED);
}

/*
 * A replace operation finished.
 */
/*ARGSUSED*/
void
rareplace(usc, mp)
	struct device *usc;
	struct mscp *mp;
{

	panic("udareplace");
}

/*
 * A bad block related operation finished.
 */
/*ARGSUSED*/
void
rabb(usc, mp, bp)
	struct device *usc;
	struct mscp *mp;
	struct buf *bp;
{

	panic("udabb");
}


/*
 * I/O controls.
 */
int
raioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register int unit = raunit(dev);
	register struct disklabel *lp;
	register struct ra_softc *ra = ra_cd.cd_devs[unit];
	int error = 0;

	lp = ra->ra_disk.dk_label;

	switch (cmd) {

	case DIOCGDINFO:
		bcopy(lp, data, sizeof (struct disklabel));
		break;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = lp;
		((struct partinfo *)data)->part =
		    &lp->d_partitions[rapart(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			error = setdisklabel(lp, (struct disklabel *)data,0,0);
			if ((error == 0) && (cmd == DIOCWDINFO)) {
				ra->ra_wlabel = 1;
				error = writedisklabel(dev, rastrategy, lp,0);
				ra->ra_wlabel = 0;
			}
		}
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			ra->ra_wlabel = 1;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

#if 0
/*
 * Do a panic dump.  We set up the controller for one command packet
 * and one response packet, for which we use `struct uda1'.
 */
struct	uda1 {
	struct	uda1ca uda1_ca;	/* communications area */
	struct	mscp uda1_rsp;	/* response packet */
	struct	mscp uda1_cmd;	/* command packet */
} uda1;
#endif

#define	DBSIZE	32		/* dump 16K at a time */

int
radump(dev, blkno, va, size)
	dev_t	dev;
	daddr_t	blkno;
	caddr_t	va;
	size_t	size;
{
#if 0
	struct udadevice *udaddr;
	struct uda1 *ud_ubaddr;
	char *start;
	int num, blk, unit, maxsz, blkoff, reg;
	struct partition *pp;
	struct uba_regs *uba;
	struct uba_device *ui;
	struct uda1 *ud;
	struct pte *io;
	int i;

	/*
	 * Make sure the device is a reasonable place on which to dump.
	 */
	unit = udaunit(dev);
	if (unit >= NRA)
		return (ENXIO);
#define	phys(cast, addr)	((cast) ((int)addr & 0x7fffffff))
	ui = phys(struct uba_device *, udadinfo[unit]);
	if (ui == NULL || ui->ui_alive == 0)
		return (ENXIO);

	/*
	 * Find and initialise the UBA; get the physical address of the
	 * device registers, and of communications area and command and
	 * response packet.
	 */
	uba = phys(struct uba_softc *, ui->ui_hd)->uh_physuba;
	ubainit(ui->ui_hd);
	udaddr = (struct udadevice *)ui->ui_physaddr;
	ud = phys(struct uda1 *, &uda1);
	/*
	 * Map the ca+packets into Unibus I/O space so the UDA50 can get
	 * at them.  Use the registers at the end of the Unibus map (since
	 * we will use the registers at the beginning to map the memory
	 * we are dumping).
	 */
	num = btoc(sizeof(struct uda1)) + 1;
	reg = NUBMREG - num;
	io = (void *)&uba->uba_map[reg];
	for (i = 0; i < num; i++)
		*(int *)io++ = UBAMR_MRV | (btop(ud) + i);
	ud_ubaddr = (struct uda1 *)(((int)ud & PGOFSET) | (reg << 9));

	/*
	 * Initialise the controller, with one command and one response
	 * packet.
	 */
	udaddr->udaip = 0;
	if (udadumpwait(udaddr, UDA_STEP1))
		return (EFAULT);
	udaddr->udasa = UDA_ERR;
	if (udadumpwait(udaddr, UDA_STEP2))
		return (EFAULT);
	udaddr->udasa = (int)&ud_ubaddr->uda1_ca.ca_rspdsc;
	if (udadumpwait(udaddr, UDA_STEP3))
		return (EFAULT);
	udaddr->udasa = ((int)&ud_ubaddr->uda1_ca.ca_rspdsc) >> 16;
	if (udadumpwait(udaddr, UDA_STEP4))
		return (EFAULT);
	((struct uda_softc *)uda_cd.cd_devs[ui->ui_ctlr])->sc_micro = udaddr->udasa & 0xff;
	udaddr->udasa = UDA_GO;

	/*
	 * Set up the command and response descriptor, then set the
	 * controller characteristics and bring the drive on line.
	 * Note that all uninitialised locations in uda1_cmd are zero.
	 */
	ud->uda1_ca.ca_rspdsc = (long)&ud_ubaddr->uda1_rsp.mscp_cmdref;
	ud->uda1_ca.ca_cmddsc = (long)&ud_ubaddr->uda1_cmd.mscp_cmdref;
	/* ud->uda1_cmd.mscp_sccc.sccc_ctlrflags = 0; */
	/* ud->uda1_cmd.mscp_sccc.sccc_version = 0; */
	if (udadumpcmd(M_OP_SETCTLRC, ud, ui))
		return (EFAULT);
	ud->uda1_cmd.mscp_unit = ui->ui_slave;
	if (udadumpcmd(M_OP_ONLINE, ud, ui))
		return (EFAULT);

	pp = phys(struct partition *,
	    &udalabel[unit].d_partitions[udapart(dev)]);
	maxsz = pp->p_size;
	blkoff = pp->p_offset;

	/*
	 * Dump all of physical memory, or as much as will fit in the
	 * space provided.
	 */
	start = 0;
	printf("Dumpar {r inte implementerade {n :) \n");
	asm("halt");
/*	num = maxfree; */
	if (dumplo + num >= maxsz)
		num = maxsz - dumplo;
	blkoff += dumplo;

	/*
	 * Write out memory, DBSIZE pages at a time.
	 * N.B.: this code depends on the fact that the sector
	 * size == the page size.
	 */
	while (num > 0) {
		blk = num > DBSIZE ? DBSIZE : num;
		io = (void *)uba->uba_map;
		/*
		 * Map in the pages to write, leaving an invalid entry
		 * at the end to guard against wild Unibus transfers.
		 * Then do the write.
		 */
		for (i = 0; i < blk; i++)
			*(int *)io++ = UBAMR_MRV | (btop(start) + i);
		*(int *)io = 0;
		ud->uda1_cmd.mscp_unit = ui->ui_slave;
		ud->uda1_cmd.mscp_seq.seq_lbn = btop(start) + blkoff;
		ud->uda1_cmd.mscp_seq.seq_bytecount = blk << PGSHIFT;
		if (udadumpcmd(M_OP_WRITE, ud, ui))
			return (EIO);
		start += blk << PGSHIFT;
		num -= blk;
	}
	return (0);		/* made it! */
}

/*
 * Wait for some of the bits in `bits' to come on.  If the error bit
 * comes on, or ten seconds pass without response, return true (error).
 */
int
udadumpwait(udaddr, bits)
	struct udadevice *udaddr;
	register int bits;
{
	register int timo = todr() + 1000;

	while ((udaddr->udasa & bits) == 0) {
		if (udaddr->udasa & UDA_ERR) {
			char bits[64];
			printf("udasa=%s\ndump ",
			    bitmask_snprintf(udaddr->udasa, udasr_bits,
			    bits, sizeof(bits)));
			return (1);
		}
		if (todr() >= timo) {
			printf("timeout\ndump ");
			return (1);
		}
	}
	return (0);
}

/*
 * Feed a command to the UDA50, wait for its response, and return
 * true iff something went wrong.
 */
int
udadumpcmd(op, ud, ui)
	int op;
	struct uda1 *ud;
	struct uba_device *ui;
{
	volatile struct udadevice *udaddr;
	volatile int n;
#define mp (&ud->uda1_rsp)

	udaddr = (struct udadevice *)ui->ui_physaddr;
	ud->uda1_cmd.mscp_opcode = op;
	ud->uda1_cmd.mscp_msglen = MSCP_MSGLEN;
	ud->uda1_rsp.mscp_msglen = MSCP_MSGLEN;
	ud->uda1_ca.ca_rspdsc |= MSCP_OWN | MSCP_INT;
	ud->uda1_ca.ca_cmddsc |= MSCP_OWN | MSCP_INT;
	if (udaddr->udasa & UDA_ERR) {
		char bits[64];
		printf("udasa=%s\ndump ", bitmask_snprintf(udaddr->udasa,
		    udasr_bits, bits, sizeof(bits)));
		return (1);
	}
	n = udaddr->udaip;
	n = todr() + 1000;
	for (;;) {
		if (todr() > n) {
			printf("timeout\ndump ");
			return (1);
		}
		if (ud->uda1_ca.ca_cmdint)
			ud->uda1_ca.ca_cmdint = 0;
		if (ud->uda1_ca.ca_rspint == 0)
			continue;
		ud->uda1_ca.ca_rspint = 0;
		if (mp->mscp_opcode == (op | M_OP_END))
			break;
		printf("\n");
		switch (MSCP_MSGTYPE(mp->mscp_msgtc)) {

		case MSCPT_SEQ:
			printf("sequential");
			break;

		case MSCPT_DATAGRAM:
			mscp_decodeerror("uda", ui->ui_ctlr, mp);
			printf("datagram");
			break;

		case MSCPT_CREDITS:
			printf("credits");
			break;

		case MSCPT_MAINTENANCE:
			printf("maintenance");
			break;

		default:
			printf("unknown (type 0x%x)",
				MSCP_MSGTYPE(mp->mscp_msgtc));
			break;
		}
		printf(" ignored\ndump ");
		ud->uda1_ca.ca_rspdsc |= MSCP_OWN | MSCP_INT;
	}
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		printf("error: op 0x%x => 0x%x status 0x%x\ndump ", op,
			mp->mscp_opcode, mp->mscp_status);
		return (1);
	}
#endif
	return (0);
#undef mp
}

/*
 * Return the size of a partition, if known, or -1 if not.
 */
int
rasize(dev)
	dev_t dev;
{
	register int unit = raunit(dev);
	struct ra_softc *ra;

	if (unit >= ra_cd.cd_ndevs || ra_cd.cd_devs[unit] == 0)
		return -1;

	ra = ra_cd.cd_devs[unit];

	if (ra->ra_state == RA_OFFLINE)
		if (ra_putonline(ra) == MSCP_FAILED)
                        return -1;

	return ra->ra_disk.dk_label->d_partitions[rapart(dev)].p_size;
}

int
ra_getdev(adaptor, controller, unit, uname)
	int adaptor, controller, unit;
	char **uname;
{
	struct mscp_softc *mi;
	struct ra_softc *ra;
	int i;

	for (i = 0; i < ra_cd.cd_ndevs; i++) {
		if ((ra = ra_cd.cd_devs[i]) == 0)
			continue;

		mi = (void *)ra->ra_dev.dv_parent;
		if (mi->mi_ctlrnr == controller && mi->mi_adapnr == adaptor &&
		    ra->ra_hwunit == unit) {
			*uname = ra->ra_dev.dv_xname;
			return i;
		}
	}
	return -1;
}
