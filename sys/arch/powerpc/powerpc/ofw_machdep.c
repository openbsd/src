/*	$NetBSD: ofw_machdep.c,v 1.1 1996/09/30 16:34:50 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>

#if defined(FFS) && defined(CD9660)
#include <ufs/ffs/fs.h>
#endif

#include <machine/powerpc.h>

#define	OFMEM_REGIONS	32
static struct mem_region OFmem[OFMEM_REGIONS + 1], OFavail[OFMEM_REGIONS + 3];

/*
 * This is called during initppc, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
mem_regions(memp, availp)
	struct mem_region **memp, **availp;
{
	int phandle, i, j, cnt;
	
	/*
	 * Get memory.
	 */
	if ((phandle = OF_finddevice("/memory")) == -1
	    || OF_getprop(phandle, "reg",
			  OFmem, sizeof OFmem[0] * OFMEM_REGIONS)
	       <= 0
	    || OF_getprop(phandle, "available",
			  OFavail, sizeof OFavail[0] * OFMEM_REGIONS)
	       <= 0)
		panic("no memory?");
	*memp = OFmem;
	*availp = OFavail;
}

void
ppc_exit()
{
	OF_exit();
}

void
ppc_boot(str)
	char *str;
{
	OF_boot(str);
}

/*
 * Establish a list of all available disks to allow specifying the root/swap/dump dev.
 */
struct ofb_disk {
	LIST_ENTRY(ofb_disk) ofb_list;
	struct disk *ofb_dk;
	struct device *ofb_dev;
	int ofb_phandle;
	int ofb_unit;
};

static LIST_HEAD(ofb_list, ofb_disk) ofb_head;	/* LIST_INIT?		XXX */

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	struct ofb_disk *od;
	struct ofb_softc *ofp = (void *)dev;

	MALLOC(od, struct ofb_disk *, sizeof *od, M_TEMP, M_NOWAIT);
	if (!od)
		panic("dk_establish");
	od->ofb_dk = dk;
	od->ofb_dev = dev;
	od->ofb_phandle = ofp->sc_phandle;
	if (dev->dv_class == DV_DISK)					/* XXX */
		od->ofb_unit = ofp->sc_unit;
	else
		od->ofb_unit = -1;
	LIST_INSERT_HEAD(&ofb_head, od, ofb_list);
}

/*
 * Cleanup the list.
 */
void
dk_cleanup()
{
	struct ofb_disk *od, *nd;

	for (od = ofb_head.lh_first; od; od = nd) {
		nd = od->ofb_list.le_next;
		LIST_REMOVE(od, ofb_list);
		FREE(od, M_TEMP);
	}
}

#if defined(FFS) && defined(CD9660)
static int
dk_match_ffs()
{
	int error;
	struct partinfo dpart;
	int bsize = DEV_BSIZE;
	int secpercyl = 1;
	struct buf *bp;
	struct fs *fs;

	if (bdevsw[major(rootdev)].d_ioctl(rootdev, DIOCGPART, (caddr_t)&dpart, FREAD, 0) == 0
	    && dpart.disklab->d_secsize != 0) {
		bsize = dpart.disklab->d_secsize;
		secpercyl = dpart.disklab->d_secpercyl;
	}

	bp = geteblk(SBSIZE);
	bp->b_dev = rootdev;

	/* Try to read the superblock */
	bp->b_blkno = SBOFF / bsize;
	bp->b_bcount = SBSIZE;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (bsize / DEV_BSIZE) / secpercyl;
	bdevsw[major(rootdev)].d_strategy(bp);

	if (error = biowait(bp))
		goto done;

	fs = (struct fs *)bp->b_data;
	if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE
	    || fs->fs_bsize < sizeof(struct fs))
		error = EIO;
done:
	brelse(bp);
	return error;
}
#endif

/* These should probably be somewhere else!				XXX */
extern int ffs_mountroot __P((void));
extern int cd9660_mountroot __P((void));
extern char *nfsbootdevname;
extern int nfs_mountroot __P((void));

extern int (*mountroot) __P((void));

static void
dk_setroot(od, part)
	struct ofb_disk *od;
	int part;
{
	char type[8];
	int maj, min;
	struct partinfo dpart;
	char *cp;

	if (OF_getprop(od->ofb_phandle, "device_type", type, sizeof type) < 0)
		panic("OF_getproperty");
#if defined(FFS) || defined(CD9660)
	if (!strcmp(type, "block")) {
		for (maj = 0; maj < nblkdev; maj++) {
			if (bdevsw[maj].d_strategy == od->ofb_dk->dk_driver->d_strategy)
				break;
		}
		if (maj >= nblkdev)
			panic("dk_setroot");

		/*
		 * Find the unit.
		 */
		min = 0;
		for (cp = od->ofb_dk->dk_name; *cp; cp++) {
			if (*cp >= '0' && *cp <= '9')
				min = min * 10 + *cp - '0';
			else
				/* Start anew */
				min = 0;
		}

		if (part == -1) {
			/* Try to open partition 'a' first */
			rootdev = makedev(maj, min * MAXPARTITIONS);

			if (bdevsw[maj].d_open(rootdev, FREAD, S_IFBLK, 0) < 0)
				panic("dk_setroot");
			dpart.part = 0;
			bdevsw[major(rootdev)].d_ioctl(rootdev, DIOCGPART,
						       (caddr_t)&dpart, FREAD, 0);
			bdevsw[maj].d_close(rootdev, FREAD, S_IFBLK, 0);
			if (!dpart.part || dpart.part->p_size <= 0) {
				/* Now we use the whole disk */
				rootdev = makedev(maj, min * MAXPARTITIONS + RAW_PART);
			}
		} else
			rootdev = makedev(maj, min * MAXPARTITIONS + part);
#if defined(FFS) && defined(CD9660)
		/*
		 * Find out whether to use ffs or cd9660.
		 */
		if (bdevsw[maj].d_open(rootdev, FREAD, S_IFBLK, 0) < 0)
			panic("dk_setroot");
		if (!dk_match_ffs())
			mountroot = ffs_mountroot;
		else							/* XXX */
			mountroot = cd9660_mountroot;
		bdevsw[maj].d_close(rootdev, FREAD, S_IFBLK, 0);
#elif defined(FFS)
		mountroot = ffs_mountroot;
#elif defined(CD9660)
		mountroot = cd9660_mountroot;
#else				/* Cannot occur */
		panic("dk_setroot: No disk filesystem");
#endif

		/*
		 * Now setup the swap/dump device
		 */
		dumpdev = makedev(major(rootdev), min * MAXPARTITIONS + 1);
		swdevt[0].sw_dev = dumpdev;
		swdevt[1].sw_dev = NODEV;

		return;
	}
#endif	/* defined(FFS) || defined(CD9660) */
#ifdef	NFSCLIENT
	if (!strcmp(type, "network")) {
		mountroot = nfs_mountroot;
		nfsbootdevname = od->ofb_dev->dv_xname;
		rootdev = NODEV;
		dumpdev = NODEV;
		swdevt[0].sw_dev = NODEV;
		return;
	}
#endif
	panic("Where were we booted from?");
}

/*
 * Try to find a disk with the given name.
 * This allows either the OpenFirmware device name,
 * or the NetBSD device name, both with optional trailing partition.
 */
int
dk_match(name)
	char *name;
{
	struct ofb_disk *od;
	char *cp;
	int phandle;
	int part, unit;
	int l;

	for (od = ofb_head.lh_first; od; od = od->ofb_list.le_next) {
		/*
		 * First try the NetBSD name.
		 */
		l = strlen(od->ofb_dev->dv_xname);
		if (!bcmp(name, od->ofb_dev->dv_xname, l)) {
			if (!name[l]) {
				/* Default partition, (or none at all) */
				dk_setroot(od, -1);
				return 0;
			}
			if (!name[l + 1]) {
				switch (name[l]) {
				case '*':
					/* Default partition */
					dk_setroot(od, -1);
					return 0;
				default:
					if (name[l] >= 'a'
					    && name[l] < 'a' + MAXPARTITIONS) {
						/* specified partition */
						dk_setroot(od, name[l] - 'a');
						return 0;
					}
					break;
				}
			}
		}
	}
	/*
	 * Now try the OpenFirmware name
	 */
	l = strlen(name);
	for (cp = name + l; --cp >= name;)
		if (*cp == '/' || *cp == ':')
			break;
	if (cp >= name && *cp == ':')
		*cp++ = 0;
	else
		cp = name + l;
	part = *cp >= 'a' && *cp < 'a' + MAXPARTITIONS
		? *cp - 'a'
		: -1;
	while (--cp >= name && *cp != '@');
	if (cp - 4 < name || bcmp(cp - 4, "disk", 4))
		unit = -1;
	else {
		for (unit = 0; *++cp >= '0' && *cp <= '9';)
			unit = unit * 10 + *cp - '0';
	}

	if ((phandle = OF_finddevice(name)) != -1) {
		for (od = ofb_head.lh_first; od; od = od->ofb_list.le_next) {
			if (phandle == od->ofb_phandle) {
				/* Check for matching units */
				if (od->ofb_dk
				    && od->ofb_unit != unit)
					continue;
				dk_setroot(od, part);
				return 0;
			}
		}
	}
	return ENODEV;
}

void
ofrootfound()
{
	int node;
	struct ofprobe probe;

	if (!(node = OF_peer(0)))
		panic("No PROM root");
	probe.phandle = node;
	if (!config_rootfound("ofroot", &probe))
		panic("ofroot not configured");
}
