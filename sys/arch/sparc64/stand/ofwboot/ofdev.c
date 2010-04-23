/*	$OpenBSD: ofdev.c,v 1.13 2010/04/23 15:25:21 jsing Exp $	*/
/*	$NetBSD: ofdev.c,v 1.1 2000/08/20 14:58:41 mrg Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Device I/O routines using Open Firmware
 */
#include <sys/param.h>
#include <sys/disklabel.h>
#ifdef NETBOOT
#include <netinet/in.h>
#endif

#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/cd9660.h>
#ifdef NETBOOT
#include <lib/libsa/nfs.h>
#endif

#include <dev/sun/disklabel.h>
#include "ofdev.h"

extern char bootdev[];

/*
 * This is ugly.  A path on a sparc machine is something like this:
 *
 *	[device] [-<options] [path] [-options] [otherstuff] [-<more options]
 *
 */

static char *
filename(char *str, char *ppart)
{
	char *cp, *lp;
	char savec;
	int dhandle;
	char devtype[16];
	
	lp = str;
	devtype[0] = 0;
	*ppart = 0;
	for (cp = str; *cp; lp = cp) {
		/* For each component of the path name... */
		while (*++cp && *cp != '/');
		savec = *cp;
		*cp = 0;
		/* ...look whether there is a device with this name */
		dhandle = OF_finddevice(str);
		DNPRINTF(BOOT_D_OFDEV, "filename: OF_finddevice(%s) says %x\n",
		    str, dhandle);
		*cp = savec;
		if (dhandle == -1) {
			/* if not, lp is the delimiter between device and path */
			/* if the last component was a block device... */
			if (!strcmp(devtype, "block")) {
				/* search for arguments */
				DNPRINTF(BOOT_D_OFDEV, "filename: hunting for "
				    "arguments in %s\n", str);
				for (cp = lp;
				     --cp >= str && *cp != '/' && *cp != '-';);
				if (cp >= str && *cp == '-') {
					/* found arguments, make firmware ignore them */
					*cp = 0;
					for (cp = lp; *--cp && *cp != ',';);
					if (*++cp >= 'a' && *cp <= 'a' + MAXPARTITIONS)
						*ppart = *cp;
				}
			}
			DNPRINTF(BOOT_D_OFDEV, "filename: found %s\n", lp);
			return lp;
		} else if (OF_getprop(dhandle, "device_type", devtype, sizeof devtype) < 0)
			devtype[0] = 0;
	}
	DNPRINTF(BOOT_D_OFDEV, "filename: not found\n", lp);
	return 0;
}

static int
strategy(void *devdata, int rw, daddr_t blk, size_t size, void *buf,
    size_t *rsize)
{
	struct of_dev *dev = devdata;
	u_quad_t pos;
	int n;
	
	if (rw != F_READ)
		return EPERM;
	if (dev->type != OFDEV_DISK)
		panic("strategy");
	
	DNPRINTF(BOOT_D_OFDEV, "strategy: block %lx, partition offset %lx, "
	    "blksz %lx\n", (long)blk, (long)dev->partoff, (long)dev->bsize);
	DNPRINTF(BOOT_D_OFDEV, "strategy: seek position should be: %lx\n", 
	    (long)((blk + dev->partoff) * dev->bsize));
	pos = (u_quad_t)(blk + dev->partoff) * dev->bsize;
	
	for (;;) {
		DNPRINTF(BOOT_D_OFDEV, "strategy: seeking to %lx\n", (long)pos);
		if (OF_seek(dev->handle, pos) < 0)
			break;
		DNPRINTF(BOOT_D_OFDEV, "strategy: reading %lx at %p\n",
		    (long)size, buf);
		n = OF_read(dev->handle, buf, size);
		if (n == -2)
			continue;
		if (n < 0)
			break;
		*rsize = n;
		return 0;
	}
	return EIO;
}

static int
devclose(struct open_file *of)
{
	struct of_dev *op = of->f_devdata;

#ifdef NETBOOT	
	if (op->type == OFDEV_NET)
		net_close(op);
#endif
	OF_close(op->handle);
	op->handle = -1;
}

static struct devsw devsw[1] = {
	"OpenFirmware",
	strategy,
	(int (*)(struct open_file *, ...))nodev,
	devclose,
	noioctl
};
int ndevs = sizeof devsw / sizeof devsw[0];

#ifdef SPARC_BOOT_UFS
static struct fs_ops file_system_ufs = {
	ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat
};
#endif
#ifdef SPARC_BOOT_HSFS
static struct fs_ops file_system_cd9660 = {
	cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	    cd9660_stat
};
#endif
#ifdef NETBOOT
static struct fs_ops file_system_nfs = {
	nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat
};
#endif

struct fs_ops file_system[3];
int nfsys;

static struct of_dev ofdev = {
	-1,
};

char opened_name[256];

static u_long
get_long(const void *p)
{
	const unsigned char *cp = p;
	
	return cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
}

/************************************************************************
 *
 * The rest of this was taken from arch/sparc64/scsi/sun_disklabel.c
 * and then substantially rewritten by Gordon W. Ross
 *
 ************************************************************************/

/* What partition types to assume for Sun disklabels: */
static u_char
sun_fstypes[8] = {
	FS_BSDFFS,	/* a */
	FS_SWAP,	/* b */
	FS_OTHER,	/* c - whole disk */
	FS_BSDFFS,	/* d */
	FS_BSDFFS,	/* e */
	FS_BSDFFS,	/* f */
	FS_BSDFFS,	/* g */
	FS_BSDFFS,	/* h */
};

/*
 * Given a SunOS disk label, set lp to a BSD disk label.
 * Returns NULL on success, else an error string.
 *
 * The BSD label is cleared out before this is called.
 */
static char *
disklabel_sun_to_bsd(char *cp, struct disklabel *lp)
{
	struct sun_disklabel *sl;
	struct partition *npp;
	struct sun_dkpart *spp;
	int i, secpercyl;
	u_short cksum, *sp1, *sp2;

	sl = (struct sun_disklabel *)cp;

	/* Verify the XOR check. */
	sp1 = (u_short *)sl;
	sp2 = (u_short *)(sl + 1);
	cksum = 0;
	while (sp1 < sp2)
		cksum ^= *sp1++;
	if (cksum != 0)
		return("SunOS disk label, bad checksum");

	/* Format conversion. */
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	memcpy(lp->d_packname, sl->sl_text, sizeof(lp->d_packname));

	lp->d_secsize = 512;
	lp->d_nsectors   = sl->sl_nsectors;
	lp->d_ntracks    = sl->sl_ntracks;
	lp->d_ncylinders = sl->sl_ncylinders;

	secpercyl = sl->sl_nsectors * sl->sl_ntracks;
	lp->d_secpercyl  = secpercyl;
	lp->d_secperunit = secpercyl * sl->sl_ncylinders;

	lp->d_acylinders   = sl->sl_acylinders;

	lp->d_npartitions = MAXPARTITIONS;
	/* These are as defined in <ufs/ffs/fs.h> */
	lp->d_bbsize = 8192;	/* XXX */
	lp->d_sbsize = 8192;	/* XXX */

	for (i = 0; i < 8; i++) {
		spp = &sl->sl_part[i];
		npp = &lp->d_partitions[i];
		npp->p_offset = spp->sdkp_cyloffset * secpercyl;
		npp->p_size = spp->sdkp_nsectors;
		DNPRINTF(BOOT_D_OFDEV, "partition %d start %x size %x\n",
		    i, (int)npp->p_offset, (int)npp->p_size);
		if (npp->p_size == 0) {
			npp->p_fstype = FS_UNUSED;
		} else {
			npp->p_fstype = sun_fstypes[i];
			if (npp->p_fstype == FS_BSDFFS) {
				/*
				 * The sun label does not store the FFS fields,
				 * so just set them with default values here.
				 */
				npp->p_fragblock =
				    DISKLABELV1_FFS_FRAGBLOCK(1024, 8);
				npp->p_cpg = 16;
			}
		}
	}

	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	DNPRINTF(BOOT_D_OFDEV, "disklabel_sun_to_bsd: success!\n");
	return (NULL);
}

/*
 * Find a valid disklabel.
 */
static char *
search_label(struct of_dev *devp, u_long off, char *buf, struct disklabel *lp,
    u_long off0)
{
	size_t read;
	struct mbr_partition *p;
	int i;
	u_long poff;
	static int recursion;

	struct disklabel *dlp;
	struct sun_disklabel *slp;
	int error;
	
	/* minimal requirements for archetypal disk label */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = MAXPARTITIONS;
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;

	if (strategy(devp, F_READ, LABELSECTOR, DEV_BSIZE, buf, &read)
	    || read != DEV_BSIZE)
		return ("Cannot read label");
	/* Check for a NetBSD disk label. */
	dlp = (struct disklabel *) (buf + LABELOFFSET);
	if (dlp->d_magic == DISKMAGIC) {
		if (dkcksum(dlp))
			return ("NetBSD disk label corrupted");
		*lp = *dlp;
		DNPRINTF(BOOT_D_OFDEV, "search_label: found NetBSD label\n");
		return (NULL);
	}

	/* Check for a Sun disk label (for PROM compatibility). */
	slp = (struct sun_disklabel *) buf;
	if (slp->sl_magic == SUN_DKMAGIC)
		return (disklabel_sun_to_bsd(buf, lp));


	bzero(buf, sizeof(buf));
	return ("no disk label");
}

int
devopen(struct open_file *of, const char *name, char **file)
{
	char *cp;
	char partition;
	char fname[256];
	char buf[DEV_BSIZE];
	struct disklabel label;
	int handle, part;
	size_t read;
	char *errmsg = NULL;
	int error = 0;

	if (ofdev.handle != -1)
		panic("devopen");
	if (of->f_flags != F_READ)
		return EPERM;
	DNPRINTF(BOOT_D_OFDEV, "devopen: you want %s\n", name);
	strlcpy(fname, name, sizeof fname);
	cp = filename(fname, &partition);
	if (cp) {
		strlcpy(buf, cp, sizeof buf);
		*cp = 0;
	}
	if (!cp || !*buf)
		strlcpy(buf, DEFAULT_KERNEL, sizeof buf);
	if (!*fname)
		strlcpy(fname, bootdev, sizeof fname);
	strlcpy(opened_name, fname, sizeof opened_name);
	if (partition) {
		cp = opened_name + strlen(opened_name);
		*cp++ = ':';
		*cp++ = partition;
		*cp = 0;
	}
	if (*buf != '/')
		strlcat(opened_name, "/", sizeof opened_name);
	strlcat(opened_name, buf, sizeof opened_name);
	*file = opened_name + strlen(fname) + 1;
	DNPRINTF(BOOT_D_OFDEV, "devopen: trying %s\n", fname);
	if ((handle = OF_finddevice(fname)) == -1)
		return ENOENT;
	DNPRINTF(BOOT_D_OFDEV, "devopen: found %s\n", fname);
	if (OF_getprop(handle, "name", buf, sizeof buf) < 0)
		return ENXIO;
	DNPRINTF(BOOT_D_OFDEV, "devopen: %s is called %s\n", fname, buf);
	if (OF_getprop(handle, "device_type", buf, sizeof buf) < 0)
		return ENXIO;
	DNPRINTF(BOOT_D_OFDEV, "devopen: %s is a %s device\n", fname, buf);
	DNPRINTF(BOOT_D_OFDEV, "devopen: opening %s\n", fname);
	if ((handle = OF_open(fname)) == -1) {
		DNPRINTF(BOOT_D_OFDEV, "devopen: open of %s failed\n", fname);
		return ENXIO;
	}
	DNPRINTF(BOOT_D_OFDEV, "devopen: %s is now open\n", fname);
	bzero(&ofdev, sizeof ofdev);
	ofdev.handle = handle;
	if (!strcmp(buf, "block")) {
		ofdev.type = OFDEV_DISK;
		ofdev.bsize = DEV_BSIZE;
		/* First try to find a disklabel without MBR partitions */
		DNPRINTF(BOOT_D_OFDEV, "devopen: trying to read disklabel\n");
		if (strategy(&ofdev, F_READ,
			     LABELSECTOR, DEV_BSIZE, buf, &read) != 0
		    || read != DEV_BSIZE
		    || (errmsg = getdisklabel(buf, &label))) {
#ifdef BOOT_DEBUG
			if (errmsg)
				DNPRINTF(BOOT_D_OFDEV,
				    "devopen: getdisklabel says %s\n", errmsg);
#endif
			/* Else try MBR partitions */
			errmsg = search_label(&ofdev, 0, buf, &label, 0);
			if (errmsg) { 
				printf("devopen: search_label says %s\n", errmsg);
				error = ERDLAB;
			}
			if (error && error != ERDLAB)
				goto bad;
		}

		if (error == ERDLAB) {
			if (partition)
				/* User specified a parititon, but there is none */
				goto bad;
			/* No, label, just use complete disk */
			ofdev.partoff = 0;
		} else {
			part = partition ? partition - 'a' : 0;
			ofdev.partoff = label.d_partitions[part].p_offset;
			DNPRINTF(BOOT_D_OFDEV, "devopen: setting partition %d "
			    "offset %x\n", part, ofdev.partoff);
		}
		
		of->f_dev = devsw;
		of->f_devdata = &ofdev;
#ifdef SPARC_BOOT_UFS
		bcopy(&file_system_ufs, &file_system[nfsys++], sizeof file_system[0]);
#endif
#ifdef SPARC_BOOT_HSFS
		bcopy(&file_system_cd9660, &file_system[nfsys++],
		    sizeof file_system[0]);
#endif
		DNPRINTF(BOOT_D_OFDEV, "devopen: return 0\n");
		return 0;
	}
#ifdef NETBOOT
	if (!strcmp(buf, "network")) {
		ofdev.type = OFDEV_NET;
		of->f_dev = devsw;
		of->f_devdata = &ofdev;
		bcopy(&file_system_nfs, file_system, sizeof file_system[0]);
		nfsys = 1;
		if (error = net_open(&ofdev))
			goto bad;
		return 0;
	}
#endif
	error = EFTYPE;
bad:
	DNPRINTF(BOOT_D_OFDEV, "devopen: error %d, cannot open device\n",
	    error);
	OF_close(handle);
	ofdev.handle = -1;
	return error;
}
