/*	$OpenBSD: installboot.c,v 1.4 2004/05/05 04:33:56 mickey Exp $	*/
/*	$NetBSD: installboot.c,v 1.5 1995/11/17 23:23:50 gwr Exp $ */

/*
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

#define	ELFSIZE 32

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

#include "nlist.c"

struct	sym_data {
	char		*sym_name;		/* Must be initialised */
	int		sym_size;		/* And this one */
	int		sym_set;		/* Rest set at runtime */
	u_int32_t	sym_value;
};

extern	char *__progname;
int	verbose, nowrite = 0;
char	*boot, *proto, *dev, *realdev;
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

static char	*loadproto(char *, long *);
static int	getbootparams(char *, int, struct disklabel *);
static void	devread(int, void *, daddr_t, size_t, char *);
static void	sym_set_value(struct sym_data *, char *, u_int32_t);
static void	pbr_set_symbols(char *, char *, struct sym_data *);
static void	usage(void);

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
	int	c;
	int	devfd;
	char	*protostore;
	long	protosize;
	struct	stat sb;
	struct	disklabel dl;
	struct	dos_mbr mbr;
	struct	dos_partition *dp;
	off_t	startoff = 0;

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

	/* Open and check raw disk device. */
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

	/* Check disklabel. */
	if (dl.d_magic != DISKMAGIC)
		err(1, "bad disklabel magic=%0x8x", dl.d_magic);

	/* Warn on unknown disklabel types. */
	if (dl.d_type == 0)
		warnx("disklabel type unknown");

	/* Load proto blocks into core. */
	if ((protostore = loadproto(proto, &protosize)) == NULL)
		exit(1);

	/* XXX - Paranoia: Make sure size is aligned! */
	if (protosize & (DEV_BSIZE - 1))
		errx(1, "proto %s bad size=%ld", proto, protosize);

	/* Write patched proto bootblock(s) into the superblock. */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if (fstat(devfd, &sb) < 0)
		err(1, "stat: %s", realdev);

	if (!S_ISCHR(sb.st_mode))
		errx(1, "%s: not a character device", realdev);

	/* Get bootstrap parameters that are to be patched into proto. */
	if (getbootparams(boot, devfd, &dl) != 0)
		exit(1);

	/* Patch the parameters into the proto bootstrap sector. */
	pbr_set_symbols(proto, protostore, pbr_symbols);

	if (!nowrite) {
		/* Sync filesystems (to clean in-memory superblock?). */
		sync(); sleep(1);
	}

	if (dl.d_type != 0 && dl.d_type != DTYPE_FLOPPY &&
	    dl.d_type != DTYPE_VND) {
		if (lseek(devfd, (off_t)DOSBBSECTOR, SEEK_SET) < 0 ||
		    read(devfd, &mbr, sizeof(mbr)) < sizeof(mbr))
			err(4, "can't read master boot record");

		if (mbr.dmbr_sign != DOSMBR_SIGNATURE)
			errx(1, "broken MBR");

		/* Find OpenBSD partition. */
		for (dp = mbr.dmbr_parts; dp < &mbr.dmbr_parts[NDOSPART];
		    dp++) {
			if (dp->dp_size && dp->dp_typ == DOSPTYP_OPENBSD) {
				startoff = (off_t)dp->dp_start * dl.d_secsize;
				fprintf(stderr, "using MBR partition %ld: "
				    "type %d (0x%02x) offset %d (0x%x)\n",
				    (long)(dp - mbr.dmbr_parts),
				    dp->dp_typ, dp->dp_typ,
				    dp->dp_start, dp->dp_start);
				break;
			}
		}
		/* Don't check for old part number, that is ;-p */
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

	if (verbose) {
		fprintf(stderr, "%s: entry point %#x\n", fname, eh.e_entry);
		fprintf(stderr, "proto bootblock size %ld\n", *size);
	}

	close(fd);
	return bp;
}

static void
devread(int fd, void *buf, daddr_t blk, size_t size, char *msg)
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
	struct partition *pl;
	struct fs	*fs;
	char		*buf;
	daddr_t		blk, *ap;
	struct ufs1_dinode	*ip;
	int		ndb;
	int		mib[4];
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

	pl = &dl->d_partitions[DISKPART(statbuf.st_dev)];
	close(fd);

	/* Read superblock. */
	devread(devfd, sblock, pl->p_offset + SBLOCK, SBSIZE, "superblock");
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

	devread(devfd, buf, pl->p_offset + blk, fs->fs_bsize, "inode");
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
	sym_set_value(pbr_symbols, "_fs_bsize_s", (fs->fs_bsize / 512));
	sym_set_value(pbr_symbols, "_fsbtodb", fs->fs_fsbtodb);
	sym_set_value(pbr_symbols, "_p_offset", pl->p_offset);
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
		    "inode block %u, offset %u\n",
		    fs->fs_fsbtodb, pl->p_offset,
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

	if (p->sym_set)
		errx(1, "%s already set", p->sym_name);

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
