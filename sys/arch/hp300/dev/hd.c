/*	$OpenBSD: hd.c,v 1.63 2010/09/22 01:18:57 matthew Exp $	*/
/*	$NetBSD: rd.c,v 1.33 1997/07/10 18:14:08 kleink Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rd.c 1.44 92/12/26$
 *
 *	@(#)rd.c	8.2 (Berkeley) 5/19/94
 */

/*
 * CS80/SS80 disk driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/dkio.h>

#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#include <hp300/dev/hpibvar.h>

#include <hp300/dev/hdreg.h>
#include <hp300/dev/hdvar.h>

#ifdef USELEDS
#include <hp300/hp300/leds.h>
#endif

#ifndef	HDRETRY
#define	HDRETRY		5
#endif

#ifndef	HDWAITC
#define HDWAITC		1	/* min time for timeout in seconds */
#endif

int	hderrthresh = HDRETRY - 1;	/* when to start reporting errors */

#ifdef DEBUG
/* error message tables */
const char *err_reject[16] = {
	NULL,
	NULL,
	"channel parity error",		/* 0x2000 */
	NULL,
	NULL,
	"illegal opcode",		/* 0x0400 */
	"module addressing",		/* 0x0200 */
	"address bounds",		/* 0x0100 */
	"parameter bounds",		/* 0x0080 */
	"illegal parameter",		/* 0x0040 */
	"message sequence",		/* 0x0020 */
	NULL,
	"message length",		/* 0x0008 */
	NULL,
	NULL,
	NULL
};

const char *err_fault[16] = {
	NULL,
	"cross unit",			/* 0x4000 */
	NULL,
	"controller fault",		/* 0x1000 */
	NULL,
	NULL,
	"unit fault",			/* 0x0200 */
	NULL,
	"diagnostic result",		/* 0x0080 */
	NULL,
	"operator release request",	/* 0x0020 */
	"diagnostic release request",	/* 0x0010 */
	"internal maintenance release request",	/* 0x0008 */
	NULL,
	"power fail",			/* 0x0002 */
	"retransmit"			/* 0x0001 */
};

const char *err_access[16] = {
	"illegal parallel operation",	/* 0x8000 */
	"uninitialized media",		/* 0x4000 */
	"no spares available",		/* 0x2000 */
	"not ready",			/* 0x1000 */
	"write protect",		/* 0x0800 */
	"no data found",		/* 0x0400 */
	NULL,
	NULL,
	"unrecoverable data overflow",	/* 0x0080 */
	"unrecoverable data",		/* 0x0040 */
	NULL,
	"end of file",			/* 0x0010 */
	"end of volume",		/* 0x0008 */
	NULL,
	NULL,
	NULL
};

const char *err_info[16] = {
	"operator release request",	/* 0x8000 */
	"diagnostic release request",	/* 0x4000 */
	"internal maintenance release request",	/* 0x2000 */
	"media wear",			/* 0x1000 */
	"latency induced",		/* 0x0800 */
	NULL,
	NULL,
	"auto sparing invoked",		/* 0x0100 */
	NULL,
	"recoverable data overflow",	/* 0x0040 */
	"marginal data",		/* 0x0020 */
	"recoverable data",		/* 0x0010 */
	NULL,
	"maintenance track overflow",	/* 0x0004 */
	NULL,
	NULL
};

#define HDB_FOLLOW	0x01
#define HDB_STATUS	0x02
#define HDB_IDENT	0x04
#define HDB_IO		0x08
#define HDB_ASYNC	0x10
#define HDB_ERROR	0x80
int	hddebug = HDB_ERROR | HDB_IDENT;
#endif

/*
 * Misc. HW description, indexed by sc_type.
 * Nothing really critical here, could do without it.
 */
const struct hdidentinfo hdidentinfo[] = {
	{ HD7946AID,	0,	"7945A",	NHD7945ABPT,
	  NHD7945ATRK,	968,	 108416 },

	{ HD9134DID,	1,	"9134D",	NHD9134DBPT,
	  NHD9134DTRK,	303,	  29088 },

	{ HD9134LID,	1,	"9122S",	NHD9122SBPT,
	  NHD9122STRK,	77,	   1232 },

	{ HD7912PID,	0,	"7912P",	NHD7912PBPT,
	  NHD7912PTRK,	572,	 128128 },

	{ HD7914PID,	0,	"7914P",	NHD7914PBPT,
	  NHD7914PTRK,	1152,	 258048 },

	{ HD7958AID,	0,	"7958A",	NHD7958ABPT,
	  NHD7958ATRK,	1013,	 255276 },

	{ HD7957AID,	0,	"7957A",	NHD7957ABPT,
	  NHD7957ATRK,	1036,	 159544 },

	{ HD7933HID,	0,	"7933H",	NHD7933HBPT,
	  NHD7933HTRK,	1321,	 789958 },

	{ HD9134LID,	1,	"9134L",	NHD9134LBPT,
	  NHD9134LTRK,	973,	  77840 },

	{ HD7936HID,	0,	"7936H",	NHD7936HBPT,
	  NHD7936HTRK,	698,	 600978 },

	{ HD7937HID,	0,	"7937H",	NHD7937HBPT,
	  NHD7937HTRK,	698,	1116102 },

	{ HD7914CTID,	0,	"7914CT",	NHD7914PBPT,
	  NHD7914PTRK,	1152,	 258048 },

	{ HD7946AID,	0,	"7946A",	NHD7945ABPT,
	  NHD7945ATRK,	968,	 108416 },

	{ HD9134LID,	1,	"9122D",	NHD9122SBPT,
	  NHD9122STRK,	77,	   1232 },

	{ HD7957BID,	0,	"7957B",	NHD7957BBPT,
	  NHD7957BTRK,	1269,	 159894 },

	{ HD7958BID,	0,	"7958B",	NHD7958BBPT,
	  NHD7958BTRK,	786,	 297108 },

	{ HD7959BID,	0,	"7959B",	NHD7959BBPT,
	  NHD7959BTRK,	1572,	 594216 },

	{ HD2200AID,	0,	"2200A",	NHD2200ABPT,
	  NHD2200ATRK,	1449,	 654948 },

	{ HD2203AID,	0,	"2203A",	NHD2203ABPT,
	  NHD2203ATRK,	1449,	1309896 }
};
const int numhdidentinfo = sizeof(hdidentinfo) / sizeof(hdidentinfo[0]);

bdev_decl(hd);
cdev_decl(hd);

int	hdident(struct device *, struct hd_softc *,
	    struct hpibbus_attach_args *);
void	hdreset(int, int, int);
void	hdustart(struct hd_softc *);
int	hdgetdisklabel(dev_t, struct hd_softc *, struct disklabel *, int);
void	hdrestart(void *);
struct buf *hdfinish(struct hd_softc *, struct buf *);

void	hdstart(void *);
void	hdinterrupt(void *);
void	hdgo(void *);
int	hdstatus(struct hd_softc *);
int	hderror(int);
#ifdef DEBUG
void	hdprinterr(const char *, short, const char **);
#endif

int	hdmatch(struct device *, void *, void *);
void	hdattach(struct device *, struct device *, void *);

struct cfattach hd_ca = {
	sizeof(struct hd_softc), hdmatch, hdattach
};

struct cfdriver hd_cd = {
	NULL, "hd", DV_DISK
};

#define	hdlock(rs)	disk_lock(&(rs)->sc_dkdev)
#define	hdunlock(rs)	disk_unlock(&(rs)->sc_dkdev)
#define	hdlookup(unit)	(struct hd_softc *)device_lookup(&hd_cd, (unit))

int
hdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct hpibbus_attach_args *ha = aux;

	return (hdident(parent, NULL, ha));
}

void
hdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hd_softc *sc = (struct hd_softc *)self;
	struct hpibbus_attach_args *ha = aux;

	if (hdident(parent, sc, ha) == 0) {
		printf("\n%s: didn't respond to describe command!\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Initialize and attach the disk structure.
	 */
	sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dev, &sc->sc_dkdev);

	sc->sc_slave = ha->ha_slave;
	sc->sc_punit = ha->ha_punit;

	/* Initialize the hpib job queue entry */
	sc->sc_hq.hq_softc = sc;
	sc->sc_hq.hq_slave = sc->sc_slave;
	sc->sc_hq.hq_start = hdstart;
	sc->sc_hq.hq_go = hdgo;
	sc->sc_hq.hq_intr = hdinterrupt;

#ifdef DEBUG
	/* always report errors */
	if (hddebug & HDB_ERROR)
		hderrthresh = 0;
#endif

	/* Initialize timeout structure */
	timeout_set(&sc->sc_timeout, hdrestart, sc);
}

int
hdident(parent, sc, ha)
	struct device *parent;
	struct hd_softc *sc;
	struct hpibbus_attach_args *ha;
{
	struct cs80_describe desc;
	u_char stat, cmd[3];
	char name[7];
	int i, id, n, ctlr, slave;

	ctlr = parent->dv_unit;
	slave = ha->ha_slave;

	/* Verify that we have a CS80 device. */
	if ((ha->ha_id & 0x200) == 0)
		return (0);

	/* Is it one of the disks we support? */
	for (id = 0; id < numhdidentinfo; id++)
		if (ha->ha_id == hdidentinfo[id].ri_hwid &&
		    ha->ha_punit <= hdidentinfo[id].ri_maxunum)
			break;
	if (id == numhdidentinfo)
		return (0);

	/*
	 * Reset device and collect description
	 */
	bzero(&desc, sizeof(desc));
	stat = 0;
	hdreset(ctlr, slave, ha->ha_punit);
	cmd[0] = C_SUNIT(ha->ha_punit);
	cmd[1] = C_SVOL(0);
	cmd[2] = C_DESC;
	hpibsend(ctlr, slave, C_CMD, cmd, sizeof(cmd));
	hpibrecv(ctlr, slave, C_EXEC, &desc, sizeof(desc));
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	if (desc.d_name == 0 && stat != 0)
		return (0);

	/*
	 * If we're just probing for the device, that's all the
	 * work we need to do.
	 */
	if (sc == NULL)
		return (1);

	bzero(name, sizeof(name));
	n = desc.d_name;
	for (i = 5; i >= 0; i--) {
		name[i] = (n & 0xf) + '0';
		n >>= 4;
	}

#ifdef DEBUG
	if (hddebug & HDB_IDENT) {
		printf(": stat %d name: %x ('%s')\n", stat, desc.d_name, name);
		printf("  iuw %x, maxxfr %d, ctype %d\n",
		    desc.d_iuw, desc.d_cmaxxfr, desc.d_ctype);
		printf("  utype %d, bps %d, blkbuf %d, burst %d, blktime %d\n",
		    desc.d_utype, desc.d_sectsize,
		    desc.d_blkbuf, desc.d_burstsize, desc.d_blocktime);
		printf("  avxfr %d, ort %d, atp %d, maxint %d, fv %x, rv %x\n",
		    desc.d_uavexfr, desc.d_retry, desc.d_access,
		    desc.d_maxint, desc.d_fvbyte, desc.d_rvbyte);
		printf("  maxcyl/head/sect %d/%d/%d, maxvsect %d, inter %d\n",
		    desc.d_maxcyl, desc.d_maxhead, desc.d_maxsect,
		    desc.d_maxvsectl, desc.d_interleave);
		printf("%s:", sc->sc_dev.dv_xname);
	}
#endif

	/*
	 * Take care of a couple of anomalies:
	 * 1. 7945A and 7946A both return same HW id
	 * 2. 9122S and 9134D both return same HW id
	 * 3. 9122D and 9134L both return same HW id
	 */
	switch (ha->ha_id) {
	case HD7946AID:
		if (bcmp(name, "079450", 6) == 0)
			id = HD7945A;
		else
			id = HD7946A;
		break;

	case HD9134LID:
		if (bcmp(name, "091340", 6) == 0)
			id = HD9134L;
		else
			id = HD9122D;
		break;

	case HD9134DID:
		if (bcmp(name, "091220", 6) == 0)
			id = HD9122S;
		else
			id = HD9134D;
		break;
	}

	sc->sc_type = id;

	/*
	 * XXX We use DEV_BSIZE instead of the sector size value pulled
	 * XXX off the driver because all of this code assumes 512 byte
	 * XXX blocks.  ICK!
	 */
	printf(": %s\n", hdidentinfo[id].ri_desc);
	printf("%s: %luMB, %lu cyl, %lu head, %lu sec, %lu bytes/sec, %lu sec total\n",
	    sc->sc_dev.dv_xname,
	    hdidentinfo[id].ri_nblocks / (1048576 / DEV_BSIZE),
	    hdidentinfo[id].ri_ncyl, hdidentinfo[id].ri_ntpc,
	    hdidentinfo[id].ri_nbpt, DEV_BSIZE, hdidentinfo[id].ri_nblocks);

	return (1);
}

void
hdreset(ctlr, slave, punit)
	int ctlr, slave, punit;
{
	struct	hd_ssmcmd ssmc;
	struct	hd_srcmd src;
	struct	hd_clearcmd clear;
	u_char stat;

	bzero(&clear, sizeof(clear));
	clear.c_unit = C_SUNIT(punit);
	clear.c_cmd = C_CLEAR;
	hpibsend(ctlr, slave, C_TCMD, &clear, sizeof(clear));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	bzero(&src, sizeof(src));
	src.c_unit = C_SUNIT(HDCTLR);
	src.c_nop = C_NOP;
	src.c_cmd = C_SREL;
	src.c_param = C_REL;
	hpibsend(ctlr, slave, C_CMD, &src, sizeof(src));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));

	bzero(&ssmc, sizeof(ssmc));
	ssmc.c_unit = C_SUNIT(punit);
	ssmc.c_cmd = C_SSM;
	ssmc.c_refm = REF_MASK;
	ssmc.c_fefm = FEF_MASK;
	ssmc.c_aefm = AEF_MASK;
	ssmc.c_iefm = IEF_MASK;
	hpibsend(ctlr, slave, C_CMD, &ssmc, sizeof(ssmc));
	hpibswait(ctlr, slave);
	hpibrecv(ctlr, slave, C_QSTAT, &stat, sizeof(stat));
}

/*
 * Read or construct a disklabel
 */
int
hdgetdisklabel(dev, rs, lp, spoofonly)
	dev_t dev;
	struct hd_softc *rs;
	struct disklabel *lp;
	int spoofonly;
{
	bzero(lp, sizeof(struct disklabel));

	/*
	 * Create a default disk label based on geometry.
	 * This will get overridden if there is a real label on the disk.
	 */
	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = hdidentinfo[rs->sc_type].ri_ntpc;
	lp->d_nsectors = hdidentinfo[rs->sc_type].ri_nbpt;
	lp->d_ncylinders = hdidentinfo[rs->sc_type].ri_ncyl;
	lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it */
	}

	lp->d_type = DTYPE_HPIB;
	strncpy(lp->d_typename, hdidentinfo[rs->sc_type].ri_desc,
	    sizeof(lp->d_typename));
	strncpy(lp->d_packname, "fictitious", sizeof lp->d_packname);

	DL_SETDSIZE(lp, hdidentinfo[rs->sc_type].ri_nblocks);
	lp->d_flags = 0;
	lp->d_version = 1;

	/* XXX - these values for BBSIZE and SBSIZE assume ffs */
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Now try to read the disklabel
	 */
	return readdisklabel(DISKLABELDEV(dev), hdstrategy, lp, spoofonly);
}

int
hdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = DISKUNIT(dev);
	struct hd_softc *rs;
	int mask, part;
	int error;

	rs = hdlookup(unit);
	if (rs == NULL)
		return (ENXIO);

	/*
	 * Fail open if we tried to attach but the disk did not answer.
	 */
	if (!ISSET(rs->sc_dkdev.dk_flags, DKF_CONSTRUCTED)) {
		device_unref(&rs->sc_dev);
		return (error);
	}

	if ((error = hdlock(rs)) != 0) {
		device_unref(&rs->sc_dev);
		return (error);
	}

	/*
	 * On first open, get label and partition info.
	 * We may block reading the label, so be careful
	 * to stop any other opens.
	 */
	if (rs->sc_dkdev.dk_openmask == 0) {
		rs->sc_flags |= HDF_OPENING;
		error = hdgetdisklabel(dev, rs, rs->sc_dkdev.dk_label, 0);
		rs->sc_flags &= ~HDF_OPENING;
		if (error == EIO)
			goto out;
	}

	part = DISKPART(dev);
	mask = 1 << part;

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part > rs->sc_dkdev.dk_label->d_npartitions ||
	     rs->sc_dkdev.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto out;
	}

	/* Ensure only one open at a time. */
	switch (mode) {
	case S_IFCHR:
		rs->sc_dkdev.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		rs->sc_dkdev.dk_bopenmask |= mask;
		break;
	}
	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;

	error = 0;
out:
	hdunlock(rs);
	device_unref(&rs->sc_dev);
	return (error);
}

int
hdclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = DISKUNIT(dev);
	struct hd_softc *rs;
	struct disk *dk;
	int mask, s;
	int error;

	rs = hdlookup(unit);
	if (rs == NULL)
		return (ENXIO);

	if ((error = hdlock(rs)) != 0) {
		device_unref(&rs->sc_dev);
		return (error);
	}

	mask = 1 << DISKPART(dev);
 	dk = &rs->sc_dkdev;
	switch (mode) {
	case S_IFCHR:
		dk->dk_copenmask &= ~mask;
		break;
	case S_IFBLK:
		dk->dk_bopenmask &= ~mask;
		break;
	}
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;

	/*
	 * On last close, we wait for all activity to cease since
	 * the label/parition info will become invalid.
	 * Note we don't have to about other closes since we know
	 * we are the last one.
	 */
	if (dk->dk_openmask == 0) {
		rs->sc_flags |= HDF_CLOSING;
		s = splbio();
		while (rs->sc_tab.b_active) {
			rs->sc_flags |= HDF_WANTED;
			tsleep((caddr_t)&rs->sc_tab, PRIBIO, "hdclose", 0);
		}
		splx(s);
		rs->sc_flags &= ~(HDF_CLOSING);
	}

	hdunlock(rs);
	device_unref(&rs->sc_dev);
	return (0);
}

void
hdstrategy(bp)
	struct buf *bp;
{
	int unit = DISKUNIT(bp->b_dev);
	struct hd_softc *rs;
	struct buf *dp;
	struct disklabel *lp;
	int s;

	rs = hdlookup(unit);
	if (rs == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

#ifdef DEBUG
	if (hddebug & HDB_FOLLOW)
		printf("hdstrategy(%x): dev %x, bn %x, bcount %lx, %c\n",
		       bp, bp->b_dev, bp->b_blkno, bp->b_bcount,
		       (bp->b_flags & B_READ) ? 'R' : 'W');
#endif

	lp = rs->sc_dkdev.dk_label;

	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % lp->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/*
	 * Do bounds checking, adjust transfer. if error, process;
	 * If end of partition, just return.
	 */
	if (bounds_check_with_label(bp, lp,
	    (rs->sc_flags & HDF_WLABEL) != 0) <= 0)
			goto done;

	s = splbio();
 	dp = &rs->sc_tab;
	disksort(dp, bp);
	if (dp->b_active == 0) {
		dp->b_active = 1;
		hdustart(rs);
	}
	splx(s);

	device_unref(&rs->sc_dev);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	s = splbio();
	biodone(bp);
	splx(s);
	if (rs != NULL)
		device_unref(&rs->sc_dev);
}

/*
 * Called via timeout(9) when handling maintenance releases
 */
void
hdrestart(arg)
	void *arg;
{
	int s = splbio();
	hdustart((struct hd_softc *)arg);
	splx(s);
}

void
hdustart(rs)
	struct hd_softc *rs;
{
	struct buf *bp;

	bp = rs->sc_tab.b_actf;
	rs->sc_addr = bp->b_data;
	rs->sc_resid = bp->b_bcount;
	if (hpibreq(rs->sc_dev.dv_parent, &rs->sc_hq))
		hdstart(rs);
}

struct buf *
hdfinish(rs, bp)
	struct hd_softc *rs;
	struct buf *bp;
{
	struct buf *dp = &rs->sc_tab;
	int s;

	rs->sc_errcnt = 0;
	dp->b_actf = bp->b_actf;
	bp->b_resid = 0;
	s = splbio();
	biodone(bp);
	splx(s);
	hpibfree(rs->sc_dev.dv_parent, &rs->sc_hq);
	if (dp->b_actf)
		return (dp->b_actf);
	dp->b_active = 0;
	if (rs->sc_flags & HDF_WANTED) {
		rs->sc_flags &= ~HDF_WANTED;
		wakeup((caddr_t)dp);
	}
	return (NULL);
}

void
hdstart(arg)
	void *arg;
{
	struct hd_softc *rs = arg;
	struct disklabel *lp;
	struct buf *bp = rs->sc_tab.b_actf;
	int ctlr, slave;
	daddr64_t bn;

	ctlr = rs->sc_dev.dv_parent->dv_unit;
	slave = rs->sc_slave;

again:
#ifdef DEBUG
	if (hddebug & HDB_FOLLOW)
		printf("hdstart(%s): bp %p, %c\n", rs->sc_dev.dv_xname, bp,
		       (bp->b_flags & B_READ) ? 'R' : 'W');
#endif
	lp = rs->sc_dkdev.dk_label;
	bn = bp->b_blkno +
	    DL_GETPOFFSET(&lp->d_partitions[DISKPART(bp->b_dev)]);

	rs->sc_flags |= HDF_SEEK;
	rs->sc_ioc.c_unit = C_SUNIT(rs->sc_punit);
	rs->sc_ioc.c_volume = C_SVOL(0);
	rs->sc_ioc.c_saddr = C_SADDR;
	rs->sc_ioc.c_hiaddr = 0;
	rs->sc_ioc.c_addr = HDBTOS(bn);
	rs->sc_ioc.c_nop2 = C_NOP;
	rs->sc_ioc.c_slen = C_SLEN;
	rs->sc_ioc.c_len = rs->sc_resid;
	rs->sc_ioc.c_cmd = bp->b_flags & B_READ ? C_READ : C_WRITE;
#ifdef DEBUG
	if (hddebug & HDB_IO)
		printf("hdstart: hpibsend(%x, %x, %x, %p, %x)\n",
		       ctlr, slave, C_CMD,
		       &rs->sc_ioc.c_unit, sizeof(rs->sc_ioc)-2);
#endif
	if (hpibsend(ctlr, slave, C_CMD, &rs->sc_ioc.c_unit,
		     sizeof(rs->sc_ioc)-2) == sizeof(rs->sc_ioc)-2) {

		/* Instrumentation. */
		disk_busy(&rs->sc_dkdev);
		rs->sc_dkdev.dk_seek++;

#ifdef DEBUG
		if (hddebug & HDB_IO)
			printf("hdstart: hpibawait(%x)\n", ctlr);
#endif
		hpibawait(ctlr);
		return;
	}
	/*
	 * Experience has shown that the hpibwait in this hpibsend will
	 * occasionally timeout.  It appears to occur mostly on old 7914
	 * drives with full maintenance tracks.  We should probably
	 * integrate this with the backoff code in hderror.
	 */
#ifdef DEBUG
	if (hddebug & HDB_ERROR)
		printf("%s: hdstart: cmd %x adr %lx blk %d len %d ecnt %ld\n",
		       rs->sc_dev.dv_xname, rs->sc_ioc.c_cmd, rs->sc_ioc.c_addr,
		       bp->b_blkno, rs->sc_resid, rs->sc_errcnt);
	rs->sc_stats.hdretries++;
#endif
	rs->sc_flags &= ~HDF_SEEK;
	hdreset(rs->sc_dev.dv_parent->dv_unit, rs->sc_slave, rs->sc_punit);
	if (rs->sc_errcnt++ < HDRETRY)
		goto again;
	printf("%s: hdstart err: err: cmd 0x%x sect %ld blk %d len %d\n",
	       rs->sc_dev.dv_xname, rs->sc_ioc.c_cmd, rs->sc_ioc.c_addr,
	       bp->b_blkno, rs->sc_resid);
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	bp = hdfinish(rs, bp);
	if (bp) {
		rs->sc_addr = bp->b_data;
		rs->sc_resid = bp->b_bcount;
		if (hpibreq(rs->sc_dev.dv_parent, &rs->sc_hq))
			goto again;
	}
}

void
hdgo(arg)
	void *arg;
{
	struct hd_softc *rs = arg;
	struct buf *bp = rs->sc_tab.b_actf;
	int rw, ctlr, slave;

	ctlr = rs->sc_dev.dv_parent->dv_unit;
	slave = rs->sc_slave;

	rw = bp->b_flags & B_READ;

	/* Instrumentation. */
	disk_busy(&rs->sc_dkdev);

#ifdef USELEDS
	ledcontrol(0, 0, LED_DISK);
#endif
	hpibgo(ctlr, slave, C_EXEC, rs->sc_addr, rs->sc_resid, rw, rw != 0);
}

/* ARGSUSED */
void
hdinterrupt(arg)
	void *arg;
{
	struct hd_softc *rs = arg;
	int unit = rs->sc_dev.dv_unit;
	struct buf *bp = rs->sc_tab.b_actf;
	u_char stat = 13;	/* in case hpibrecv fails */
	int rv, restart, ctlr, slave;

	ctlr = rs->sc_dev.dv_parent->dv_unit;
	slave = rs->sc_slave;

#ifdef DEBUG
	if (hddebug & HDB_FOLLOW)
		printf("hdinterrupt(%d): bp %p, %c, flags %x\n", unit, bp,
		       (bp->b_flags & B_READ) ? 'R' : 'W', rs->sc_flags);
	if (bp == NULL) {
		printf("%s: bp == NULL\n", rs->sc_dev.dv_xname);
		return;
	}
#endif
	disk_unbusy(&rs->sc_dkdev, (bp->b_bcount - bp->b_resid),
	    (bp->b_flags & B_READ));

	if (rs->sc_flags & HDF_SEEK) {
		rs->sc_flags &= ~HDF_SEEK;
		if (hpibustart(ctlr))
			hdgo(rs);
		return;
	}
	if ((rs->sc_flags & HDF_SWAIT) == 0) {
#ifdef DEBUG
		rs->sc_stats.hdpolltries++;
#endif
		if (hpibpptest(ctlr, slave) == 0) {
#ifdef DEBUG
			rs->sc_stats.hdpollwaits++;
#endif

			/* Instrumentation. */
			disk_busy(&rs->sc_dkdev);
			rs->sc_flags |= HDF_SWAIT;
			hpibawait(ctlr);
			return;
		}
	} else
		rs->sc_flags &= ~HDF_SWAIT;
	rv = hpibrecv(ctlr, slave, C_QSTAT, &stat, 1);
	if (rv != 1 || stat) {
#ifdef DEBUG
		if (hddebug & HDB_ERROR)
			printf("hdinterrupt: recv failed or bad stat %d\n", stat);
#endif
		restart = hderror(unit);
#ifdef DEBUG
		rs->sc_stats.hdretries++;
#endif
		if (rs->sc_errcnt++ < HDRETRY) {
			if (restart)
				hdstart(rs);
			return;
		}
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
	if (hdfinish(rs, bp))
		hdustart(rs);
}

int
hdstatus(rs)
	struct hd_softc *rs;
{
	int c, s;
	u_char stat;
	int rv;

	c = rs->sc_dev.dv_parent->dv_unit;
	s = rs->sc_slave;
	rs->sc_rsc.c_unit = C_SUNIT(rs->sc_punit);
	rs->sc_rsc.c_sram = C_SRAM;
	rs->sc_rsc.c_ram = C_RAM;
	rs->sc_rsc.c_cmd = C_STATUS;
	bzero((caddr_t)&rs->sc_stat, sizeof(rs->sc_stat));
	rv = hpibsend(c, s, C_CMD, &rs->sc_rsc, sizeof(rs->sc_rsc));
	if (rv != sizeof(rs->sc_rsc)) {
#ifdef DEBUG
		if (hddebug & HDB_STATUS)
			printf("hdstatus: send C_CMD failed %d != %d\n",
			       rv, sizeof(rs->sc_rsc));
#endif
		return(1);
	}
	rv = hpibrecv(c, s, C_EXEC, &rs->sc_stat, sizeof(rs->sc_stat));
	if (rv != sizeof(rs->sc_stat)) {
#ifdef DEBUG
		if (hddebug & HDB_STATUS)
			printf("hdstatus: send C_EXEC failed %d != %d\n",
			       rv, sizeof(rs->sc_stat));
#endif
		return(1);
	}
	rv = hpibrecv(c, s, C_QSTAT, &stat, 1);
	if (rv != 1 || stat) {
#ifdef DEBUG
		if (hddebug & HDB_STATUS)
			printf("hdstatus: recv failed %d or bad stat %d\n",
			       rv, stat);
#endif
		return(1);
	}
	return(0);
}

/*
 * Deal with errors.
 * Returns 1 if request should be restarted,
 * 0 if we should just quietly give up.
 */
int
hderror(unit)
	int unit;
{
	struct hd_softc *rs = hd_cd.cd_devs[unit];
	struct hd_stat *sp;
	struct buf *bp;
	daddr64_t hwbn, pbn;

	if (hdstatus(rs)) {
#ifdef DEBUG
		printf("%s: couldn't get status\n", rs->sc_dev.dv_xname);
#endif
		hdreset(rs->sc_dev.dv_parent->dv_unit,
		    rs->sc_slave, rs->sc_punit);
		return(1);
	}
	sp = &rs->sc_stat;
	if (sp->c_fef & FEF_REXMT)
		return(1);
	if (sp->c_fef & FEF_PF) {
		hdreset(rs->sc_dev.dv_parent->dv_unit,
		    rs->sc_slave, rs->sc_punit);
		return(1);
	}
	/*
	 * Unit requests release for internal maintenance.
	 * We just delay a while and try again later.  Use exponentially
	 * increasing backoff a la ethernet drivers since we don't really
	 * know how long the maintenance will take.  With HDWAITC and
	 * HDRETRY as defined, the range is 1 to 32 seconds.
	 */
	if (sp->c_fef & FEF_IMR) {
		int hdtimo = HDWAITC << rs->sc_errcnt;
#ifdef DEBUG
		printf("%s: internal maintenance, %d second timeout\n",
		       rs->sc_dev.dv_xname, hdtimo);
		rs->sc_stats.hdtimeouts++;
#endif
		hpibfree(rs->sc_dev.dv_parent, &rs->sc_hq);
		timeout_add_sec(&rs->sc_timeout, hdtimo);
		return(0);
	}
	/*
	 * Only report error if we have reached the error reporting
	 * threshold.  By default, this will only report after the
	 * retry limit has been exceeded.
	 */
	if (rs->sc_errcnt < hderrthresh)
		return(1);

	/*
	 * First conjure up the block number at which the error occurred.
	 * Note that not all errors report a block number, in that case
	 * we just use b_blkno.
 	 */
	bp = rs->sc_tab.b_actf;
	pbn = DL_GETPOFFSET(&rs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)]);
	if ((sp->c_fef & FEF_CU) || (sp->c_fef & FEF_DR) ||
	    (sp->c_ief & IEF_RRMASK)) {
		hwbn = HDBTOS(pbn + bp->b_blkno);
		pbn = bp->b_blkno;
	} else {
		hwbn = sp->c_blk;
		pbn = HDSTOB(hwbn) - pbn;
	}

	diskerr(bp, hd_cd.cd_name, "hard error", LOG_PRINTF,
	    pbn - bp->b_blkno, rs->sc_dkdev.dk_label);
	printf("\n%s%c: ", rs->sc_dev.dv_xname, 'a' + DISKPART(bp->b_dev));
	
#ifdef DEBUG
	if (hddebug & HDB_ERROR) {
		/* status info */
		printf("volume: %d, unit: %d\n",
		       (sp->c_vu>>4)&0xF, sp->c_vu&0xF);
		hdprinterr("reject", sp->c_ref, err_reject);
		hdprinterr("fault", sp->c_fef, err_fault);
		hdprinterr("access", sp->c_aef, err_access);
		hdprinterr("info", sp->c_ief, err_info);
		printf("    block: %lld, P1-P10: ", hwbn);
		printf("0x%04x", *(u_int *)&sp->c_raw[0]);
		printf("%04x", *(u_int *)&sp->c_raw[4]);
		printf("%02x\n", *(u_short *)&sp->c_raw[8]);
		/* command */
		printf("    ioc: ");
		printf("0x%x", *(u_int *)&rs->sc_ioc.c_pad);
		printf("0x%x", *(u_short *)&rs->sc_ioc.c_hiaddr);
		printf("0x%x", *(u_int *)&rs->sc_ioc.c_addr);
		printf("0x%x", *(u_short *)&rs->sc_ioc.c_nop2);
		printf("0x%x", *(u_int *)&rs->sc_ioc.c_len);
		printf("0x%x\n", *(u_short *)&rs->sc_ioc.c_cmd);
	} else
#endif
	{
		printf("v%d u%d, R0x%x F0x%x A0x%x I0x%x",
		    (sp->c_vu>>4)&0xF, sp->c_vu&0xF,
		    sp->c_ref, sp->c_fef, sp->c_aef, sp->c_ief);
		printf(" P1-P10: 0x%04x%04x%02x\n",
		    *(u_int *)&sp->c_raw[0], *(u_int *)&sp->c_raw[4],
		    *(u_short *)&sp->c_raw[8]);
	}
	return (1);
}

int
hdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(hdstrategy, dev, B_READ, minphys, uio));
}

int
hdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(hdstrategy, dev, B_WRITE, minphys, uio));
}

int
hdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DISKUNIT(dev);
	struct disklabel *lp;
	struct hd_softc *sc;
	int error = 0;

	sc = hdlookup(unit);
	if (sc == NULL)
		return (ENXIO);

	switch (cmd) {
	case DIOCRLDINFO:
		lp = malloc(sizeof(*lp), M_TEMP, M_WAITOK);
		hdgetdisklabel(dev, sc, lp, 0);
		*(sc->sc_dkdev.dk_label) = *lp;
		free(lp, M_TEMP);
		return 0;

	case DIOCGPDINFO:
		hdgetdisklabel(dev, sc, (struct disklabel *)data, 1);
		goto exit;

	case DIOCGDINFO:
		*(struct disklabel *)data = *sc->sc_dkdev.dk_label;
		goto exit;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = sc->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
			&sc->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		goto exit;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}
		if (*(int *)data)
			sc->sc_flags |= HDF_WLABEL;
		else
			sc->sc_flags &= ~HDF_WLABEL;
		goto exit;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}

		if ((error = hdlock(sc)) != 0)
			goto exit;
		sc->sc_flags |= HDF_WLABEL;

		error = setdisklabel(sc->sc_dkdev.dk_label,
		    (struct disklabel *)data, /* sc->sc_dkdev.dk_openmask */ 0);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    hdstrategy, sc->sc_dkdev.dk_label);
		}

		sc->sc_flags &= ~HDF_WLABEL;
		hdunlock(sc);
		goto exit;

	default:
		error = EINVAL;
		break;
	}

exit:
	device_unref(&sc->sc_dev);
	return (error);
}

daddr64_t
hdsize(dev)
	dev_t dev;
{
	struct hd_softc *rs;
	int unit = DISKUNIT(dev);
	int part, omask;
	int size;

	rs = hdlookup(unit);
	if (rs == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = rs->sc_dkdev.dk_openmask & (1 << part);

	/*
	 * We get called very early on (via swapconf)
	 * without the device being open so we may need
	 * to handle it here.
	 */
	if (omask == 0 && hdopen(dev, FREAD | FWRITE, S_IFBLK, NULL) != 0) {
		size = -1;
		goto out;
	}

	if (rs->sc_dkdev.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = DL_GETPSIZE(&rs->sc_dkdev.dk_label->d_partitions[part]) *
		    (rs->sc_dkdev.dk_label->d_secsize / DEV_BSIZE);

	if (hdclose(dev, FREAD | FWRITE, S_IFBLK, NULL) != 0)
		size = -1;

out:
	device_unref(&rs->sc_dev);
	return (size);
}

#ifdef DEBUG
void
hdprinterr(str, err, tab)
	const char *str;
	short err;
	const char **tab;
{
	int i;
	int printed;

	if (err == 0)
		return;
	printf("    %s error %d field:", str, err);
	printed = 0;
	for (i = 0; i < 16; i++)
		if (err & (0x8000 >> i))
			printf("%s%s", printed++ ? " + " : " ", tab[i]);
	printf("\n");
}
#endif

static int hddoingadump;	/* simple mutex */

/*
 * Non-interrupt driven, non-dma dump routine.
 */
int
hddump(dev, blkno, va, size)
	dev_t dev;
	daddr64_t blkno;
	caddr_t va;
	size_t size;
{
	int sectorsize;		/* size of a disk sector */
	daddr64_t nsects;	/* number of sectors in partition */
	daddr64_t sectoff;	/* sector offset of partition */
	int totwrt;		/* total number of sectors left to write */
	int nwrt;		/* current number of sectors to write */
	int unit, part;
	int ctlr, slave;
	struct hd_softc *rs;
	struct disklabel *lp;
	char stat;

	/* Check for recursive dump; if so, punt. */
	if (hddoingadump)
		return (EFAULT);
	hddoingadump = 1;

	/* Decompose unit and partition. */
	unit = DISKUNIT(dev);
	part = DISKPART(dev);

	/* Make sure dump device is ok. */
	rs = hdlookup(unit);
	if (rs == NULL)
		return (ENXIO);
	device_unref(&rs->sc_dev);

	ctlr = rs->sc_dev.dv_parent->dv_unit;
	slave = rs->sc_slave;

	/*
	 * Convert to disk sectors.  Request must be a multiple of size.
	 */
	lp = rs->sc_dkdev.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return (EFAULT);
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = DL_GETPSIZE(&lp->d_partitions[part]);
	sectoff = DL_GETPOFFSET(&lp->d_partitions[part]);

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || (blkno + totwrt) > nsects)
		return (EINVAL);

	/* Offset block number to start of partition. */
	blkno += sectoff;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef HD_DUMP_NOT_TRUSTED
		/*
		 * Fill out and send HPIB command.
		 */
		rs->sc_ioc.c_unit = C_SUNIT(rs->sc_punit);
		rs->sc_ioc.c_volume = C_SVOL(0);
		rs->sc_ioc.c_saddr = C_SADDR;
		rs->sc_ioc.c_hiaddr = 0;
		rs->sc_ioc.c_addr = HDBTOS(blkno);
		rs->sc_ioc.c_nop2 = C_NOP;
		rs->sc_ioc.c_slen = C_SLEN;
		rs->sc_ioc.c_len = nwrt * sectorsize;
		rs->sc_ioc.c_cmd = C_WRITE;
		hpibsend(ctlr, slave, C_CMD, &rs->sc_ioc.c_unit,
		    sizeof(rs->sc_ioc)-2);
		if (hpibswait(ctlr, slave))
			return (EIO);

		/*
		 * Send the data.
		 */
		hpibsend(ctlr, slave, C_EXEC, va, nwrt * sectorsize);
		(void) hpibswait(ctlr, slave);
		hpibrecv(ctlr, slave, C_QSTAT, &stat, 1);
		if (stat)
			return (EIO);
#else /* HD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("%s: dump addr %p, blk %d\n", sc->sc_dev.dv_xname,
		    va, blkno);
		delay(500 * 1000);	/* half a second */
#endif /* HD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va += sectorsize * nwrt;
	}
	hddoingadump = 0;
	return (0);
}
