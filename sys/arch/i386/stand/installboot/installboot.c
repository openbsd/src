/*	$OpenBSD: installboot.c,v 1.29 1998/04/02 10:50:31 deraadt Exp $	*/
/*	$NetBSD: installboot.c,v 1.5 1995/11/17 23:23:50 gwr Exp $ */

/*
 * Copyright (c) 1997 Michael Shalayeff
 * Copyright (c) 1994 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/biosvar.h>

#include <err.h>
#include <a.out.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

extern	char *__progname;
int	verbose, nowrite, nheads, nsectors, userspec = 0;
char	*boot, *proto, *dev, *realdev;
struct nlist nl[] = {
#define X_BLOCK_COUNT	0
	{{"_block_count"}},
#define X_BLOCK_TABLE	1
	{{"_block_table"}},
	{{NULL}}
};

u_int8_t *block_count_p;	/* block count var. in prototype image */
u_int8_t *block_table_p;	/* block number array in prototype image */
int	maxblocknum;		/* size of this array */

int biosdev;

char		*loadprotoblocks __P((char *, long *));
int		loadblocknums __P((char *, int, struct disklabel *));
static void	devread __P((int, void *, daddr_t, size_t, char *));
static void	usage __P((void));
static int	record_block
	__P((u_int8_t *, daddr_t, u_int, struct disklabel *));

static void
usage()
{
	fprintf(stderr, "usage: %s [-n] [-v] [-s sec-per-track] [-h track-per-cyl] "
	    "boot biosboot device\n", __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	c;
	int	devfd;
	char	*protostore;
	long	protosize;
	struct stat sb;
	struct disklabel dl;
	struct dos_mbr mbr;
	struct dos_partition *dp;
	off_t startoff = 0;
	int mib[4], size;
	dev_t devno;
	bios_diskinfo_t di;

	nsectors = nheads = -1;
	while ((c = getopt(argc, argv, "vnh:s:")) != EOF) {
		switch (c) {
		case 'h':
			nheads = atoi(optarg);
			userspec = 1;
			break;
		case 's':
			nsectors = atoi(optarg);
			userspec = 1;
			break;
		case 'n':
			/* Do not actually write the bootblock to disk */
			nowrite = 1;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind < 3) {
		usage();
	}

	boot = argv[optind];
	proto = argv[optind + 1];
	realdev = dev = argv[optind + 2];

	/* Open and check raw disk device */
	if ((devfd = opendev(dev, (nowrite? O_RDONLY:O_RDWR),
			     OPENDEV_PART, &realdev)) < 0)
		err(1, "open: %s", realdev);

	if (verbose) {
		fprintf(stderr, "boot: %s\n", boot);
		fprintf(stderr, "proto: %s\n", proto);
		fprintf(stderr, "device: %s\n", realdev);
	}

	if (ioctl(devfd, DIOCGDINFO, &dl) != 0)
		err(1, "disklabel: %s", realdev);

	/* check disklabel */
	if (dl.d_magic != DISKMAGIC)
		err(1, "bad disklabel magic=%0x8x", dl.d_magic);

	/* warn on unknown disklabel types */
	if (dl.d_type == 0)
		warnx("disklabel type unknown");

	/* Load proto blocks into core */
	if ((protostore = loadprotoblocks(proto, &protosize)) == NULL)
		exit(1);

	/* XXX - Paranoia: Make sure size is aligned! */
	if (protosize & (DEV_BSIZE - 1))
		err(1, "proto bootblock bad size=%ld", protosize);

	/* Write patched proto bootblocks into the superblock */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if (fstat(devfd, &sb) < 0)
		err(1, "stat: %s", realdev);

	if (!S_ISCHR(sb.st_mode))
		errx(1, "%s: Not a character device", realdev);

	if (nheads == -1 || nsectors == -1) {
		mib[0] = CTL_MACHDEP;
		mib[1] = CPU_CHR2BLK;
		mib[2] = sb.st_rdev;
		size = sizeof(devno);
		if(sysctl(mib, 3, &devno, &size, NULL, 0) >= 0) {
			devno = MAKEBOOTDEV(major(devno), 0, 0,
			    DISKUNIT(devno), RAW_PART);
			mib[0] = CTL_MACHDEP;
			mib[1] = CPU_BIOS;
			mib[2] = BIOS_DISKINFO;
			mib[3] = devno;
			size = sizeof(di);
			if(sysctl(mib, 4, &di, &size, NULL, 0) >= 0) {
				nheads = di.bios_heads;
				nsectors = di.bios_sectors;
			}
		}
	}

	if (nheads == -1 || nsectors == -1)
		errx(1, "Unable to get BIOS geometry, must specify -h and -s");

	/* Extract and load block numbers */
	if (loadblocknums(boot, devfd, &dl) != 0)
		exit(1);

	/* Sync filesystems (to clean in-memory superblock?) */
	sync(); sleep(1);

	if (dl.d_type != 0 && dl.d_type != DTYPE_FLOPPY &&
	    dl.d_type != DTYPE_VND) {
		if (lseek(devfd, (off_t)DOSBBSECTOR, SEEK_SET) < 0 ||
		    read(devfd, &mbr, sizeof(mbr)) < sizeof(mbr))
			err(4, "can't read master boot record");

		if (mbr.dmbr_sign != DOSMBR_SIGNATURE)
			errx(1, "broken MBR");

		/* Find OpenBSD partition. */
		for (dp = mbr.dmbr_parts; dp < &mbr.dmbr_parts[NDOSPART]; dp++) {
			if (dp->dp_size && dp->dp_typ == DOSPTYP_OPENBSD) {
				startoff = dp->dp_start * dl.d_secsize;
				fprintf(stderr, "using MBR partition %d: "
					"type %d (0x%02x) offset %d (0x%x)\n",
					dp - mbr.dmbr_parts,
					dp->dp_typ, dp->dp_typ,
					dp->dp_start, dp->dp_start);
				break;
			}
		}
		/* don't check for old part number, that is ;-p */
		if (dp >= &mbr.dmbr_parts[NDOSPART])
			errx(1, "no OpenBSD partition");
	}

	if (!nowrite) {
		if (lseek(devfd, startoff, SEEK_SET) < 0 ||
		    write(devfd, protostore, protosize) != protosize)
			err(1, "write bootstrap");
	}

	(void)close(devfd);

	return 0;
}

char *
loadprotoblocks(fname, size)
	char *fname;
	long *size;
{
	int	fd;
	size_t	tdsize;		/* text+data size */
	char	*bp;
	struct	nlist *nlp;
	struct	exec eh;

	fd = -1;
	bp = NULL;

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
	/* Validate symbol types (global data). */
	for (nlp = nl; nlp->n_un.n_name; nlp++) {
		if (nlp->n_type != (N_TEXT | N_EXT)) {
			warnx("nlist: %s: wrong type", nlp->n_un.n_name);
			return NULL;
		}
	}

	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (read(fd, &eh, sizeof(eh)) != sizeof(eh)) {
		warn("read: %s", fname);
		goto bad;
	}
	if (N_GETMAGIC(eh) != OMAGIC) {
		warn("bad magic: 0x%lx", eh.a_midmag);
		goto bad;
	}
	/*
	 * We have to include the exec header in the beginning of
	 * the buffer, and leave extra space at the end in case
	 * the actual write to disk wants to skip the header.
	 */
	tdsize = eh.a_text + eh.a_data;

	/*
	 * Allocate extra space here because the caller may copy
	 * the boot block starting at the end of the exec header.
	 * This prevents reading beyond the end of the buffer.
	 */
	if ((bp = calloc(tdsize, 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		goto bad;
	}
	/* Read the rest of the file. */
	if (read(fd, bp, tdsize) != tdsize) {
		warn("read: %s", fname);
		goto bad;
	}

	*size = tdsize;	/* not aligned to DEV_BSIZE */

	/* Calculate the symbols' locations within the proto file */
	block_count_p = (u_int8_t *) (bp + nl[X_BLOCK_COUNT].n_value);
	block_table_p = (u_int8_t *) (bp + nl[X_BLOCK_TABLE].n_value);
	maxblocknum = *block_count_p;

	if (verbose) {
		fprintf(stderr, "%s: entry point %#lx\n", fname, eh.a_entry);
		fprintf(stderr, "proto bootblock size %ld\n", *size);
		fprintf(stderr, "room for %d filesystem blocks at %#lx\n",
			maxblocknum, nl[X_BLOCK_TABLE].n_value);
	}

	close(fd);
	return bp;

 bad:
	if (bp)
		free(bp);
	if (fd >= 0)
		close(fd);
	return NULL;
}

static void
devread(fd, buf, blk, size, msg)
	int	fd;
	void	*buf;
	daddr_t	blk;
	size_t	size;
	char	*msg;
{
	if (lseek(fd, dbtob((off_t)blk), SEEK_SET) != dbtob((off_t)blk))
		err(1, "%s: devread: lseek", msg);

	if (read(fd, buf, size) != size)
		err(1, "%s: devread: read", msg);
}

static char sblock[SBSIZE];

int
loadblocknums(boot, devfd, dl)
	char	*boot;
	int	devfd;
	struct disklabel *dl;
{
	int		i, fd;
	struct stat	statbuf, sb;
	struct statfs	statfsbuf;
	struct partition *pl;
	struct fs	*fs;
	char		*buf;
	daddr_t		blk, *ap;
	struct dinode	*ip;
	int		ndb;
	u_int8_t	*bt;
	struct exec	eh;
	int mib[4], size;
	dev_t dev;

	/*
	 * Open 2nd-level boot program and record the block numbers
	 * it occupies on the filesystem represented by `devfd'.
	 */

	/* Make sure the (probably new) boot file is on disk. */
	sync(); sleep(1);

	if ((fd = open(boot, O_RDONLY)) < 0)
		err(1, "open: %s", boot);

	if (fstatfs(fd, &statfsbuf) != 0)
		err(1, "statfs: %s", boot);

	if (strncmp(statfsbuf.f_fstypename, "ffs", MFSNAMELEN) &&
	    strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN) ) {
		errx(1, "%s: must be on an FFS filesystem", boot);
	}

	if (read(fd, &eh, sizeof(eh)) != sizeof(eh)) {
		errx(1, "read: %s", boot);
	}

	if (N_GETMAGIC(eh) != ZMAGIC) {
		errx(1, "%s: bad magic: 0x%lx", boot, eh.a_midmag);
	}

	if (fsync(fd) != 0)
		err(1, "fsync: %s", boot);

	if (fstat(fd, &statbuf) != 0)
		err(1, "fstat: %s", boot);

	if (fstat(devfd, &sb) != 0)
		err(1, "fstat: %s", realdev);

	/* check devices */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CHR2BLK;
	mib[2] = sb.st_rdev;
	size = sizeof(dev);
	if (sysctl(mib, 3, &dev, &size, NULL, 0) >= 0)
		if (statbuf.st_dev / MAXPARTITIONS != dev / MAXPARTITIONS)
			errx(1, "cross-device install");

	pl = &dl->d_partitions[DISKPART(statbuf.st_dev)];
	close(fd);

	/* Read superblock */
	devread(devfd, sblock, pl->p_offset + SBLOCK, SBSIZE, "superblock");
	fs = (struct fs *)sblock;

	/* Sanity-check super-block. */
	if (fs->fs_magic != FS_MAGIC)
		errx(1, "Bad magic number in superblock");
	if (fs->fs_inopb <= 0)
		err(1, "Bad inopb=%d in superblock", fs->fs_inopb);

	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL)
		errx(1, "No memory for filesystem block");

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));
	devread(devfd, buf, pl->p_offset + blk, fs->fs_bsize, "inode");
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Have the inode.  Figure out how many blocks we need.
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb <= 0)
		errx(1, "No blocks to load");
	if (ndb > maxblocknum)
		errx(1, "Too many blocks");
	if (verbose)
		fprintf(stderr, "Will load %d blocks of size %d each.\n",
			ndb, fs->fs_bsize);

	if ((dl->d_type != 0 && dl->d_type != DTYPE_FLOPPY &&
	    dl->d_type != DTYPE_VND) || userspec ) {
		/* adjust disklabel w/ synthetic geometry */
		dl->d_nsectors = nsectors;
		dl->d_secpercyl = dl->d_nsectors * nheads;
	}

	if (verbose)
		fprintf(stderr, "Using disk geometry of %u sectors and %u heads.\n",
			dl->d_nsectors, dl->d_secpercyl/dl->d_nsectors);
	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ap = ip->di_db;
	bt = block_table_p;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--)
		bt += record_block(bt, pl->p_offset + fsbtodb(fs, *ap),
					    fs->fs_bsize / 512, dl);
	if (ndb != 0) {

		/*
		 * Just one level of indirections; there isn't much room
		 * for more in the 2nd-level /boot anyway.
		 */
		blk = fsbtodb(fs, ip->di_ib[0]);
		devread(devfd, buf, pl->p_offset + blk, fs->fs_bsize,
			"indirect block");
		ap = (daddr_t *)buf;
		for (; i < NINDIR(fs) && *ap && ndb; i++, ap++, ndb--)
			bt += record_block(bt, pl->p_offset + fsbtodb(fs, *ap),
					   fs->fs_bsize / 512, dl);
	}

	/* write out remaining piece */
	bt += record_block(bt, 0, 0, dl);
	/* and again */
	bt += record_block(bt, 0, 0, dl);
	*block_count_p = (bt - block_table_p) / 4;

	if (verbose)
		fprintf(stderr, "%s: %d entries total\n",
			boot, *block_count_p);

	return 0;
}

static int
record_block(bt, blk, bs, dl)
	u_int8_t *bt;
	daddr_t blk;
	u_int bs;
	struct disklabel *dl;
{
	static u_int ss = 0, l = 0, i = 0; /* start and len of group */
	int ret = 0;

	if (ss == 0) { /* very beginning */
		ss = blk;
		l = bs;
		return 0;
	} else if (l == 0)
		return 0;

	/* record on track boundary or non-contig blocks or last call */
	if ((ss + l) != blk ||
	    (ss % dl->d_nsectors + l) >= dl->d_nsectors) {
		register u_int c = ss / dl->d_secpercyl,
			s = ss % dl->d_nsectors + 1;

		/* nsectors */
		if ((ss % dl->d_nsectors + l) >= dl->d_nsectors)
			bt[3] = dl->d_nsectors - s + 1;
		else
			bt[3] = l; /* non-contig or last block piece */

		bt[2] = (ss % dl->d_secpercyl) / dl->d_nsectors; /* head */
		*(u_int16_t *)bt = (s & 0x3f) | /* sect, cyl */
			((c & 0xff) << 8) | ((c & 0x300) >> 2);

		if (verbose)
			fprintf(stderr, "%2d: %2d @(%d %d %d) (%d-%d)\n",
				i, bt[3], c, bt[2], s, ss, ss + bt[3] - 1);

		if ((ss % dl->d_nsectors + l) >= dl->d_nsectors) {
			ss += bt[3];
			l -= bt[3];
			l += bs;
		} else {
			ss = blk;
			l = bs;
		}

		i++;
		ret = 4;
	} else {
		l += bs;
	}

	return ret;
}
