/*	$NetBSD: ofdev.c,v 1.1 1997/04/16 20:29:20 thorpej Exp $	*/

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
#include <netinet/in.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/cd9660.h>
#include <lib/libsa/nfs.h>

#include <powerpc/stand/ofwboot/ofdev.h>

extern char bootdev[];

static char *
filename(str, ppart)
	char *str;
	char *ppart;
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
		*cp = savec;
		if (dhandle == -1) {
			/* if not, lp is the delimiter between device and path */
			/* if the last component was a block device... */
			if (!strcmp(devtype, "block")) {
				/* search for arguments */
				for (cp = lp;
				     --cp >= str && *cp != '/' && *cp != ':';);
				if (cp >= str && *cp == ':') {
					/* found arguments, make firmware ignore them */
					*cp = 0;
					for (cp = lp; *--cp && *cp != ',';);
					if (*++cp >= 'a' && *cp <= 'a' + MAXPARTITIONS)
						*ppart = *cp;
				}
			}
			return lp;
		} else if (OF_getprop(dhandle, "device_type", devtype, sizeof devtype) < 0)
			devtype[0] = 0;
	}
	return 0;
}

static int
strategy(devdata, rw, blk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t blk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	struct of_dev *dev = devdata;
	u_quad_t pos;
	int n;
	
	if (rw != F_READ)
		return EPERM;
	if (dev->type != OFDEV_DISK)
		panic("strategy");
	
	pos = (u_quad_t)(blk + dev->partoff) * dev->bsize;
	
	for (;;) {
		if (OF_seek(dev->handle, pos) < 0)
			break;
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
devclose(of)
	struct open_file *of;
{
	struct of_dev *op = of->f_devdata;
	
	if (op->type == OFDEV_NET)
		net_close(op);
	OF_close(op->handle);
	op->handle = -1;
}

static struct devsw devsw[1] = {
	"OpenFirmware",
	strategy,
	(int (*)__P((struct open_file *, ...)))nodev,
	devclose,
	noioctl
};
int ndevs = sizeof devsw / sizeof devsw[0];

static struct fs_ops file_system_ufs = {
	ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat
};
static struct fs_ops file_system_cd9660 = {
	cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	    cd9660_stat
};
static struct fs_ops file_system_nfs = {
	nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat
};

struct fs_ops file_system[3];
int nfsys;

static struct of_dev ofdev = {
	-1,
};

char opened_name[256];
int floppyboot;

static u_long
get_long(p)
	const void *p;
{
	const unsigned char *cp = p;
	
	return cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
}

/*
 * Find a valid disklabel.
 */
static int
search_label(devp, off, buf, lp, off0)
	struct of_dev *devp;
	u_long off;
	char *buf;
	struct disklabel *lp;
	u_long off0;
{
	size_t read;
	struct mbr_partition *p;
	int i;
	u_long poff;
	static int recursion;
	
	if (strategy(devp, F_READ, off, DEV_BSIZE, buf, &read)
	    || read != DEV_BSIZE)
		return ERDLAB;
	
	if (buf[510] != 0x55 || buf[511] != 0xaa)
		return ERDLAB;

	if (recursion++ <= 1)
		off0 += off;
	for (p = (struct mbr_partition *)(buf + MBRPARTOFF), i = 4;
	     --i >= 0; p++) {
		if (p->mbr_type == MBR_NETBSD) {
			poff = get_long(&p->mbr_start) + off0;
			if (strategy(devp, F_READ, poff + LABELSECTOR,
				     DEV_BSIZE, buf, &read) == 0
			    && read == DEV_BSIZE) {
				if (!getdisklabel(buf, lp)) {
					recursion--;
					return 0;
				}
			}
			if (strategy(devp, F_READ, off, DEV_BSIZE, buf, &read)
			    || read != DEV_BSIZE) {
				recursion--;
				return ERDLAB;
			}
		} else if (p->mbr_type == MBR_EXTENDED) {
			poff = get_long(&p->mbr_start);
			if (!search_label(devp, poff, buf, lp, off0)) {
				recursion--;
				return 0;
			}
			if (strategy(devp, F_READ, off, DEV_BSIZE, buf, &read)
			    || read != DEV_BSIZE) {
				recursion--;
				return ERDLAB;
			}
		}
	}
	recursion--;
	return ERDLAB;
}

int
devopen(of, name, file)
	struct open_file *of;
	const char *name;
	char **file;
{
	char *cp;
	char partition;
	char fname[256];
	char buf[DEV_BSIZE];
	struct disklabel label;
	int handle, part;
	size_t read;
	int error = 0;

	if (ofdev.handle != -1)
		panic("devopen");
	if (of->f_flags != F_READ)
		return EPERM;
	strcpy(fname, name);
	cp = filename(fname, &partition);
	if (cp) {
		strcpy(buf, cp);
		*cp = 0;
	}
	if (!cp || !*buf)
		strcpy(buf, DEFAULT_KERNEL);
	if (!*fname)
		strcpy(fname, bootdev);
	strcpy(opened_name, fname);
	if (partition) {
		cp = opened_name + strlen(opened_name);
		*cp++ = ':';
		*cp++ = partition;
		*cp = 0;
	}
	if (*buf != '/')
		strcat(opened_name, "/");
	strcat(opened_name, buf);
	*file = opened_name + strlen(fname) + 1;
	if ((handle = OF_finddevice(fname)) == -1)
		return ENOENT;
	if (OF_getprop(handle, "name", buf, sizeof buf) < 0)
		return ENXIO;
	floppyboot = !strcmp(buf, "floppy");
	if (OF_getprop(handle, "device_type", buf, sizeof buf) < 0)
		return ENXIO;
	if (!strcmp(buf, "block"))
		/* For block devices, indicate raw partition (:0 in OpenFirmware) */
		strcat(fname, ":0");
	if ((handle = OF_open(fname)) == -1)
		return ENXIO;
	bzero(&ofdev, sizeof ofdev);
	ofdev.handle = handle;
	if (!strcmp(buf, "block")) {
		ofdev.type = OFDEV_DISK;
		ofdev.bsize = DEV_BSIZE;
		/* First try to find a disklabel without MBR partitions */
		if (strategy(&ofdev, F_READ,
			     LABELSECTOR, DEV_BSIZE, buf, &read) != 0
		    || read != DEV_BSIZE
		    || getdisklabel(buf, &label)) {
			/* Else try MBR partitions */
			error = search_label(&ofdev, 0, buf, &label, 0);
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
		}
		
		of->f_dev = devsw;
		of->f_devdata = &ofdev;
		bcopy(&file_system_ufs, file_system, sizeof file_system[0]);
		bcopy(&file_system_cd9660, file_system + 1,
		    sizeof file_system[0]);
		nfsys = 2;
		return 0;
	}
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
	error = EFTYPE;
bad:
	OF_close(handle);
	ofdev.handle = -1;
	return error;
}
