/*	$OpenBSD: installboot.c,v 1.66 2011/07/19 01:08:35 krw Exp $	*/
/*	$NetBSD: installboot.c,v 1.5 1995/11/17 23:23:50 gwr Exp $ */

/*
 * Copyright (c) 2011 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2010 Otto Moerbeek <otto@openbsd.org>
 * Copyright (c) 2003 Tom Cosgrove <tom.cosgrove@arches-consulting.com>
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

#include <dev/biovar.h>
#include <dev/softraidvar.h>

#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/biosvar.h>

#include <err.h>
#include <a.out.h>
#include <sys/exec_elf.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

struct	sym_data {
	char		*sym_name;		/* Must be initialised */
	int		sym_size;		/* And this one */
	int		sym_set;		/* Rest set at runtime */
	u_int32_t	sym_value;
};

extern	char *__progname;
int	verbose, nowrite = 0;
char	*boot, *proto, *dev, *realdev;
char	*protostore;
long	protosize;
struct sym_data pbr_symbols[] = {
	{"_fs_bsize_p",	2},
	{"_fs_bsize_s",	2},
	{"_fsbtodb",	1},
	{"_p_offset",	4},
	{"_inodeblk",	4},
	{"_inodedbl",	4},
	{"_nblocks",	2},
	{NULL}
};

#define INODESEG	0x07e0	/* where we will put /boot's inode's block */
#define BOOTSEG		0x07c0	/* biosboot loaded here */

#define INODEOFF  ((INODESEG-BOOTSEG) << 4)

#define SR_FS_BLOCKSIZE	(16 * 1024)

static char	*loadproto(char *, long *);
static int	getbootparams(char *, int, struct disklabel *);
static void	devread(int, void *, daddr64_t, size_t, char *);
static void	sym_set_value(struct sym_data *, char *, u_int32_t);
static void	pbr_set_symbols(char *, char *, struct sym_data *);
static void	usage(void);
static u_int	findopenbsd(int, struct disklabel *);
static void	write_bootblocks(int devfd, struct disklabel *);

static int	sr_volume(int, int *, int *);
static void	sr_installboot(int);
static void	sr_installpbr(int, int, int);

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-nv] boot biosboot device\n", __progname);
	exit(1);
}

/*
 * Read information about /boot's inode and filesystem parameters, then
 * put biosboot (partition boot record) on the target drive with these
 * parameters patched in.
 */
int
main(int argc, char *argv[])
{
	int	vol = -1, ndisks = 0, disk;
	int	c;
	int	devfd;
	struct	disklabel dl;

	while ((c = getopt(argc, argv, "vn")) != -1) {
		switch (c) {
		case 'n':
			/* Do not actually write the bootblock to disk. */
			nowrite = 1;
			break;
		case 'v':
			/* Give more information. */
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind < 3)
		usage();

	boot = argv[optind];
	proto = argv[optind + 1];
	realdev = dev = argv[optind + 2];

	/* Open raw disk device. */
	if ((devfd = opendev(dev, (nowrite? O_RDONLY:O_RDWR),
	    OPENDEV_PART, &realdev)) < 0)
		err(1, "open: %s", realdev);

	if (verbose)
		fprintf(stderr, "boot: %s proto: %s device: %s\n",
		    boot, proto, realdev);

	/* Load proto blocks into core. */
	if ((protostore = loadproto(proto, &protosize)) == NULL)
		exit(1);

	/* XXX - Paranoia: Make sure size is aligned! */
	if (protosize & (DEV_BSIZE - 1))
		errx(1, "proto %s bad size=%ld", proto, protosize);

	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if (sr_volume(devfd, &vol, &ndisks)) {

		/* Install boot loader into softraid volume. */
		sr_installboot(devfd);

		/* Install biosboot on each disk that is part of this volume. */
		for (disk = 0; disk < ndisks; disk++)
			sr_installpbr(devfd, vol, disk);

	} else {

		/* Get and check disklabel. */
		if (ioctl(devfd, DIOCGDINFO, &dl) != 0)
			err(1, "disklabel: %s", realdev);
		if (dl.d_magic != DISKMAGIC)
			err(1, "bad disklabel magic=0x%08x", dl.d_magic);

		/* Warn on unknown disklabel types. */
		if (dl.d_type == 0)
			warnx("disklabel type unknown");

		/* Get bootstrap parameters to patch into proto. */
		if (getbootparams(boot, devfd, &dl) != 0)
			exit(1);

		/* Write boot blocks to device. */
		write_bootblocks(devfd, &dl);

	}

	(void)close(devfd);

	return 0;
}

void
write_bootblocks(int devfd, struct disklabel *dl)
{
	struct stat	sb;
	u_int8_t	*secbuf;
	u_int		start = 0;

	/* Write patched proto bootblock(s) into the superblock. */
	if (fstat(devfd, &sb) < 0)
		err(1, "stat: %s", realdev);

	if (!S_ISCHR(sb.st_mode))
		errx(1, "%s: not a character device", realdev);

	/* Patch the parameters into the proto bootstrap sector. */
	pbr_set_symbols(proto, protostore, pbr_symbols);

	if (!nowrite) {
		/* Sync filesystems (to clean in-memory superblock?). */
		sync(); sleep(1);
	}

	/*
	 * Find OpenBSD partition. Floppies are special, getting an
	 * everything-in-one /boot starting at sector 0.
	 */
	if (dl->d_type != DTYPE_FLOPPY) {
		start = findopenbsd(devfd, dl);
		if (start == (u_int)-1)
 			errx(1, "no OpenBSD partition");
	}

	if (verbose)
		fprintf(stderr, "/boot will be written at sector %u\n", start);

	if (start + (protosize / dl->d_secsize) > BOOTBIOS_MAXSEC)
		warnx("/boot extends beyond sector %u. OpenBSD might not boot.",
		    BOOTBIOS_MAXSEC);

	if (!nowrite) {
		if (lseek(devfd, (off_t)start * dl->d_secsize, SEEK_SET) < 0)
			err(1, "seek bootstrap");
		secbuf = calloc(1, dl->d_secsize);
		bcopy(protostore, secbuf, protosize);
		if (write(devfd, secbuf, dl->d_secsize) != dl->d_secsize)
			err(1, "write bootstrap");
		free(secbuf);
	}
}

u_int
findopenbsd(int devfd, struct disklabel *dl)
{
	struct		dos_mbr mbr;
	u_int		mbroff = DOSBBSECTOR;
	u_int		mbr_eoff = DOSBBSECTOR; /* Offset of extended part. */
	struct		dos_partition *dp;
	u_int8_t	*secbuf;
	int		i, maxebr = DOS_MAXEBR, nextebr;

again:
	if (!maxebr--) {
		if (verbose)
			fprintf(stderr, "Traversed more than %d Extended Boot "
			    "Records (EBRs)\n",
			    DOS_MAXEBR);
		return ((u_int)-1);
	}
		
	if (verbose)
		fprintf(stderr, "%s boot record (%cBR) at sector %u\n",
		    (mbroff == DOSBBSECTOR) ? "master" : "extended",
		    (mbroff == DOSBBSECTOR) ? 'M' : 'E', mbroff);

	secbuf = malloc(dl->d_secsize); 
	if (lseek(devfd, (off_t)mbroff * dl->d_secsize, SEEK_SET) < 0 ||
	    read(devfd, secbuf, dl->d_secsize) < sizeof(mbr))
		err(4, "can't read boot record");
	bcopy(secbuf, &mbr, sizeof(mbr));
	free(secbuf);

	if (mbr.dmbr_sign != DOSMBR_SIGNATURE)
		errx(1, "invalid boot record signature (0x%04X) @ sector %u",
		    mbr.dmbr_sign, mbroff);

	nextebr = 0;
	for (i = 0; i < NDOSPART; i++) {
		dp = &mbr.dmbr_parts[i];
		if (!dp->dp_size)
			continue;

		if (verbose)
			fprintf(stderr,
			    "\tpartition %d: type 0x%02X offset %u size %u\n",
			    i, dp->dp_typ, dp->dp_start, dp->dp_size);

		if (dp->dp_typ == DOSPTYP_OPENBSD) {
			if (dp->dp_start > (dp->dp_start + mbroff))
				continue;
			return (dp->dp_start + mbroff);
		}

		if (!nextebr && (dp->dp_typ == DOSPTYP_EXTEND ||
		    dp->dp_typ == DOSPTYP_EXTENDL)) {
			nextebr = dp->dp_start + mbr_eoff;
			if (nextebr < dp->dp_start)
				nextebr = (u_int)-1;
			if (mbr_eoff == DOSBBSECTOR)
				mbr_eoff = dp->dp_start;
		}
	}

	if (nextebr && nextebr != (u_int)-1) {
		mbroff = nextebr;
		goto again;
	}

	return ((u_int)-1);
}

/*
 * Load the prototype boot sector (biosboot) into memory.
 */
static char *
loadproto(char *fname, long *size)
{
	int	fd;
	size_t	tdsize;		/* text+data size */
	char	*bp;
	Elf_Ehdr eh;
	Elf_Word phsize;
	Elf_Phdr *ph;

	if ((fd = open(fname, O_RDONLY)) < 0)
		err(1, "%s", fname);

	if (read(fd, &eh, sizeof(eh)) != sizeof(eh))
		errx(1, "%s: read failed", fname);

	if (!IS_ELF(eh))
		errx(1, "%s: bad magic: 0x%02x%02x%02x%02x",
		    boot,
		    eh.e_ident[EI_MAG0], eh.e_ident[EI_MAG1],
		    eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]);

	/*
	 * We have to include the exec header in the beginning of
	 * the buffer, and leave extra space at the end in case
	 * the actual write to disk wants to skip the header.
	 */

	/* Program load header. */
	if (eh.e_phnum != 1)
		errx(1, "%s: %u ELF load sections (only support 1)",
		    boot, eh.e_phnum);

	phsize = eh.e_phnum * sizeof(Elf_Phdr);
	ph = malloc(phsize);
	if (ph == NULL)
		err(1, NULL);

	lseek(fd, eh.e_phoff, SEEK_SET);

	if (read(fd, ph, phsize) != phsize)
		errx(1, "%s: can't read header", boot);

	tdsize = ph->p_filesz;

	/*
	 * Allocate extra space here because the caller may copy
	 * the boot block starting at the end of the exec header.
	 * This prevents reading beyond the end of the buffer.
	 */
	if ((bp = calloc(tdsize, 1)) == NULL) {
		err(1, NULL);
	}

	/* Read the rest of the file. */
	lseek(fd, ph->p_offset, SEEK_SET);
	if (read(fd, bp, tdsize) != tdsize) {
		errx(1, "%s: read failed", fname);
	}

	*size = tdsize;	/* not aligned to DEV_BSIZE */

	close(fd);
	return bp;
}

static void
devread(int fd, void *buf, daddr64_t blk, size_t size, char *msg)
{
	if (lseek(fd, dbtob((off_t)blk), SEEK_SET) != dbtob((off_t)blk))
		err(1, "%s: devread: lseek", msg);

	if (read(fd, buf, size) != size)
		err(1, "%s: devread: read", msg);
}

static char sblock[SBSIZE];

/*
 * Read information about /boot's inode, then put this and filesystem
 * parameters from the superblock into pbr_symbols.
 */
static int
getbootparams(char *boot, int devfd, struct disklabel *dl)
{
	int		fd;
	struct stat	statbuf, sb;
	struct statfs	statfsbuf;
	struct partition *pp;
	struct fs	*fs;
	char		*buf;
	u_int		blk, *ap;
	struct ufs1_dinode	*ip;
	int		ndb;
	int		mib[3];
	size_t		size;
	dev_t		dev;

	/*
	 * Open 2nd-level boot program and record enough details about
	 * where it is on the filesystem represented by `devfd'
	 * (inode block, offset within that block, and various filesystem
	 * parameters essentially taken from the superblock) for biosboot
	 * to be able to load it later.
	 */

	/* Make sure the (probably new) boot file is on disk. */
	sync(); sleep(1);

	if ((fd = open(boot, O_RDONLY)) < 0)
		err(1, "open: %s", boot);

	if (fstatfs(fd, &statfsbuf) != 0)
		err(1, "statfs: %s", boot);

	if (strncmp(statfsbuf.f_fstypename, "ffs", MFSNAMELEN) &&
	    strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN) )
		errx(1, "%s: not on an FFS filesystem", boot);

#if 0
	if (read(fd, &eh, sizeof(eh)) != sizeof(eh))
		errx(1, "read: %s", boot);

	if (!IS_ELF(eh)) {
		errx(1, "%s: bad magic: 0x%02x%02x%02x%02x",
		    boot,
		    eh.e_ident[EI_MAG0], eh.e_ident[EI_MAG1],
		    eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]);
	}
#endif

	if (fsync(fd) != 0)
		err(1, "fsync: %s", boot);

	if (fstat(fd, &statbuf) != 0)
		err(1, "fstat: %s", boot);

	if (fstat(devfd, &sb) != 0)
		err(1, "fstat: %s", realdev);

	/* Check devices. */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CHR2BLK;
	mib[2] = sb.st_rdev;
	size = sizeof(dev);
	if (sysctl(mib, 3, &dev, &size, NULL, 0) >= 0) {
		if (statbuf.st_dev / MAXPARTITIONS != dev / MAXPARTITIONS)
			errx(1, "cross-device install");
	}

	pp = &dl->d_partitions[DISKPART(statbuf.st_dev)];
	close(fd);

	/* Read superblock. */
	devread(devfd, sblock, DL_SECTOBLK(dl, pp->p_offset) + SBLOCK,
	    SBSIZE, "superblock");
	fs = (struct fs *)sblock;

	/* Sanity-check super-block. */
	if (fs->fs_magic != FS_MAGIC)
		errx(1, "Bad magic number in superblock");
	if (fs->fs_inopb <= 0)
		err(1, "Bad inopb=%d in superblock", fs->fs_inopb);

	/* Read inode. */
	if ((buf = malloc(fs->fs_bsize)) == NULL)
		err(1, NULL);

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));

	devread(devfd, buf, DL_SECTOBLK(dl, pp->p_offset) + blk,
	    fs->fs_bsize, "inode");
	ip = (struct ufs1_dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Have the inode.  Figure out how many filesystem blocks (not disk
	 * sectors) there are for biosboot to load.
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb <= 0)
		errx(1, "No blocks to load");

	/*
	 * Now set the values that will need to go into biosboot
	 * (the partition boot record, a.k.a. the PBR).
	 */
	sym_set_value(pbr_symbols, "_fs_bsize_p", (fs->fs_bsize / 16));
	sym_set_value(pbr_symbols, "_fs_bsize_s", (fs->fs_bsize / 
	    dl->d_secsize));

	/*
	 * fs_fsbtodb is the shift to convert fs_fsize to DEV_BSIZE. The
	 * ino_to_fsba() return value is the number of fs_fsize units.
	 * Calculate the shift to convert fs_fsize into physical sectors,
	 * which are added to p_offset to get the sector address BIOS
	 * will use.
	 *
	 * N.B.: ASSUMES fs_fsize is a power of 2 of d_secsize.
	 */
	sym_set_value(pbr_symbols, "_fsbtodb",
	    ffs(fs->fs_fsize / dl->d_secsize) - 1);

	sym_set_value(pbr_symbols, "_p_offset", pp->p_offset);
	sym_set_value(pbr_symbols, "_inodeblk",
	    ino_to_fsba(fs, statbuf.st_ino));
	ap = ip->di_db;
	sym_set_value(pbr_symbols, "_inodedbl",
	    ((((char *)ap) - buf) + INODEOFF));
	sym_set_value(pbr_symbols, "_nblocks", ndb);

	if (verbose) {
		fprintf(stderr, "%s is %d blocks x %d bytes\n",
		    boot, ndb, fs->fs_bsize);
		fprintf(stderr, "fs block shift %u; part offset %u; "
		    "inode block %lld, offset %u\n",
		    ffs(fs->fs_fsize / dl->d_secsize) - 1,
		    pp->p_offset,
		    ino_to_fsba(fs, statbuf.st_ino),
		    (unsigned int)((((char *)ap) - buf) + INODEOFF));
	}

	return 0;
}

static void
sym_set_value(struct sym_data *sym_list, char *sym, u_int32_t value)
{
	struct sym_data *p;

	for (p = sym_list; p->sym_name != NULL; p++) {
		if (strcmp(p->sym_name, sym) == 0)
			break;
	}

	if (p->sym_name == NULL)
		errx(1, "%s: no such symbol", sym);

	p->sym_value = value;
	p->sym_set = 1;
}

/*
 * Write the parameters stored in sym_list into the in-memory copy of
 * the prototype biosboot (proto), ready for it to be written to disk.
 */
static void
pbr_set_symbols(char *fname, char *proto, struct sym_data *sym_list)
{
	struct sym_data *sym;
	struct nlist	*nl;
	char		*vp;
	u_int32_t	*lp;
	u_int16_t	*wp;
	u_int8_t	*bp;

	for (sym = sym_list; sym->sym_name != NULL; sym++) {
		if (!sym->sym_set)
			errx(1, "%s not set", sym->sym_name);

		/* Allocate space for 2; second is null-terminator for list. */
		nl = calloc(2, sizeof(struct nlist));
		if (nl == NULL)
			err(1, NULL);

		nl->n_un.n_name = sym->sym_name;

		if (nlist(fname, nl) != 0)
			errx(1, "%s: symbol %s not found",
			    fname, sym->sym_name);

		if (nl->n_type != (N_TEXT))
			errx(1, "%s: %s: wrong type (%x)",
			    fname, sym->sym_name, nl->n_type);

		/* Get a pointer to where the symbol's value needs to go. */
		vp = proto + nl->n_value;

		switch (sym->sym_size) {
		case 4:					/* u_int32_t */
			lp = (u_int32_t *) vp;
			*lp = sym->sym_value;
			break;
		case 2:					/* u_int16_t */
			if (sym->sym_value >= 0x10000)	/* out of range */
				errx(1, "%s: symbol out of range (%u)",
				    sym->sym_name, sym->sym_value);
			wp = (u_int16_t *) vp;
			*wp = (u_int16_t) sym->sym_value;
			break;
		case 1:					/* u_int16_t */
			if (sym->sym_value >= 0x100)	/* out of range */
				errx(1, "%s: symbol out of range (%u)",
				    sym->sym_name, sym->sym_value);
			bp = (u_int8_t *) vp;
			*bp = (u_int8_t) sym->sym_value;
			break;
		default:
			errx(1, "%s: bad symbol size %d",
			    sym->sym_name, sym->sym_size);
			/* NOTREACHED */
		}

		free(nl);
	}
}

int
sr_volume(int devfd, int *vol, int *disks)
{
	struct	bioc_inq bi;
	struct	bioc_vol bv;
	int	rv, i;

	/* Get volume information. */
	memset(&bi, 0, sizeof(bi));
	rv = ioctl(devfd, BIOCINQ, &bi);
	if (rv == -1)
		return 0;

	/* XXX - softraid volumes will always have a "softraid0" controller. */
	if (strncmp(bi.bi_dev, "softraid0", sizeof("softraid0")))
		return 0;

	/* Locate specific softraid volume. */
	for (i = 0; i < bi.bi_novol; i++) {

		memset(&bv, 0, sizeof(bv));
		bv.bv_volid = i;
		rv = ioctl(devfd, BIOCVOL, &bv);
		if (rv == -1)
			err(1, "BIOCVOL");

		if (strncmp(dev, bv.bv_dev, sizeof(bv.bv_dev)) == 0) {
			*vol = i;
			*disks = bv.bv_nodisk;
			break;
		}

	}

	if (verbose)
		fprintf(stderr, "%s: softraid volume with %i disk(s)\n",
		    dev, *disks);

	return 1;
}

void
sr_installboot(int devfd)
{
	struct bioc_installboot bb;
	struct stat sb;
	struct ufs1_dinode *ino_p;
	uint32_t bootsize, inodeblk, inodedbl;
	uint16_t bsize = SR_FS_BLOCKSIZE;
	uint16_t nblocks;
	uint8_t bshift = 5;		/* fragsize == blocksize */
	int fd, i, rv;
	u_char *p;

	/*
	 * Install boot loader into softraid boot loader storage area.
	 *
	 * In order to allow us to reuse the existing biosboot we construct
	 * a fake FFS filesystem with a single inode, which points to the
	 * boot loader.
	 */

	nblocks = howmany(SR_BOOT_LOADER_SIZE, SR_FS_BLOCKSIZE / DEV_BSIZE);
	inodeblk = nblocks - 1;
	bootsize = nblocks * SR_FS_BLOCKSIZE;

	p = malloc(bootsize);
	if (p == NULL)
		err(1, NULL);
	
	memset(p, 0, bootsize);
	fd = open(boot, O_RDONLY, 0);
	if (fd == -1)
		err(1, NULL);

	if (fstat(fd, &sb) == -1)
		err(1, NULL);

	nblocks = howmany(sb.st_blocks, SR_FS_BLOCKSIZE / DEV_BSIZE);
	if (sb.st_blocks * S_BLKSIZE > bootsize - sizeof(struct ufs1_dinode))
		errx(1, "boot code will not fit");

	/* We only need to fill the direct block array. */
	ino_p = (struct ufs1_dinode *)&p[bootsize - sizeof(struct ufs1_dinode)];

	ino_p->di_mode = sb.st_mode;
	ino_p->di_nlink = 1;
	ino_p->di_inumber = 0xfeebfaab;
	ino_p->di_size = read(fd, p, sb.st_blocks * S_BLKSIZE);
	ino_p->di_blocks = nblocks;
	for (i = 0; i < nblocks; i++)
		ino_p->di_db[i] = i;

	inodedbl = ((u_char*)&ino_p->di_db[0] -
	    &p[bootsize - SR_FS_BLOCKSIZE]) + INODEOFF;

	bb.bb_bootldr = p;
	bb.bb_bootldr_size = bootsize;
	bb.bb_bootblk = "XXX";
	bb.bb_bootblk_size = sizeof("XXX");
	strncpy(bb.bb_dev, dev, sizeof(bb.bb_dev));
	if (!nowrite) {
		if (verbose)
			fprintf(stderr, "%s: installing boot loader on "
			    "softraid volume\n", dev);
		rv = ioctl(devfd, BIOCINSTALLBOOT, &bb);
		if (rv != 0)
			errx(1, "softraid installboot failed");
	}

	/*
	 * Set the values that will need to go into biosboot
	 * (the partition boot record, a.k.a. the PBR).
	 */
	sym_set_value(pbr_symbols, "_fs_bsize_p", (bsize / 16));
	sym_set_value(pbr_symbols, "_fs_bsize_s", (bsize / 512));
	sym_set_value(pbr_symbols, "_fsbtodb", bshift);
	sym_set_value(pbr_symbols, "_inodeblk", inodeblk);
	sym_set_value(pbr_symbols, "_inodedbl", inodedbl);
	sym_set_value(pbr_symbols, "_nblocks", nblocks);

	if (verbose)
		fprintf(stderr, "%s is %d blocks x %d bytes\n",
		    boot, nblocks, bsize);

	close(fd);
}

void
sr_installpbr(int devfd, int vol, int disk)
{
	struct bioc_disk bd;
	struct disklabel dl;
	struct partition *pp;
	uint32_t poffset;
	char *realdiskdev;
	char part;
	int diskfd;
	int rv;

	/* Get device name for this disk/chunk. */
	memset(&bd, 0, sizeof(bd));
	bd.bd_volid = vol;
	bd.bd_diskid = disk;
	rv = ioctl(devfd, BIOCDISK, &bd);
	if (rv == -1)
		err(1, "BIOCDISK");

	/* Check disk status. */
	if (bd.bd_status != BIOC_SDONLINE && bd.bd_status != BIOC_SDREBUILD) {
		fprintf(stderr, "softraid disk %s not online - skipping...\n",
		    bd.bd_vendor);
		return;	
	}

	if (strlen(bd.bd_vendor) < 1)
		errx(1, "invalid disk name %s", bd.bd_vendor);
	part = bd.bd_vendor[strlen(bd.bd_vendor) - 1];
	if (part < 'a' || part >= 'a' + MAXPARTITIONS)
		errx(1, "invalid partition %c\n", part);
	bd.bd_vendor[strlen(bd.bd_vendor) - 1] = '\0';

	/* Open this device and check its disklabel. */
	if ((diskfd = opendev(bd.bd_vendor, (nowrite? O_RDONLY:O_RDWR),
	    OPENDEV_PART, &realdiskdev)) < 0)
		err(1, "open: %s", realdiskdev);

	/* Get and check disklabel. */
	if (ioctl(diskfd, DIOCGDINFO, &dl) != 0)
		err(1, "disklabel: %s", realdev);
	if (dl.d_magic != DISKMAGIC)
		err(1, "bad disklabel magic=0x%08x", dl.d_magic);

	/* Warn on unknown disklabel types. */
	if (dl.d_type == 0)
		warnx("disklabel type unknown");

	/* Determine poffset and set symbol value. */
	pp = &dl.d_partitions[part - 'a'];
	if (pp->p_offseth != 0)
		errx(1, "partition offset too high");
	poffset = pp->p_offset; 		/* Offset of RAID partition. */
	poffset += SR_BOOT_LOADER_OFFSET;	/* SR boot loader area. */
	sym_set_value(pbr_symbols, "_p_offset", poffset);

	if (verbose)
		fprintf(stderr, "%s%c: installing boot blocks on %s, "
		    "part offset %u\n", bd.bd_vendor, part, realdiskdev,
		    poffset);

	/* Write boot blocks to device. */
	write_bootblocks(diskfd, &dl);

	close(diskfd);
}
