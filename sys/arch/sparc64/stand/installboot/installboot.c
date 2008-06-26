/*	$OpenBSD: installboot.c,v 1.7 2008/06/26 05:42:13 ray Exp $	*/
/*	$NetBSD: installboot.c,v 1.8 2001/02/19 22:48:59 cgd Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <err.h>
#include <a.out.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "loadfile.h"

int	verbose, nowrite, sparc64, uflag, hflag = 1;
char	*boot, *proto, *dev;

#if 0
#ifdef __ELF__
#define SYMNAME(a)	a
#else
#define SYMNAME(a)	__CONCAT("_",a)
#endif
#else
/* XXX: Hack in libc nlist works with both formats */
#define SYMNAME(a)	"_"a
#endif

struct nlist nl[] = {
#define X_BLOCKTABLE	0
	{ {SYMNAME("block_table")} },
#define X_BLOCKCOUNT	1
	{ {SYMNAME("block_count")} },
#define X_BLOCKSIZE	2
	{ {SYMNAME("block_size")} },
	{ {NULL} }
};
daddr_t	*block_table;		/* block number array in prototype image */
int32_t	*block_count_p;		/* size of this array */
int32_t	*block_size_p;		/* filesystem block size */
int32_t	max_block_count;

char		*loadprotoblocks(char *, size_t *);
int		loadblocknums(char *, int);
static void	devread(int, void *, daddr_t, size_t, char *);
static void	usage(void);
int 		main(int, char *[]);

static void
usage()
{
	extern char *__progname;
	const char *progname = __progname;

	if (sparc64)
		(void)fprintf(stderr,
		    "Usage: %s [-nv] <bootblk> <device>\n"
		    "       %s -U [-nv] <boot> <proto> <device>\n",
		    progname, progname);
	else
		(void)fprintf(stderr,
		    "Usage: %s [-nv] <boot> <proto> <device>\n"
		    "       %s -u [-n] [-v] <bootblk> <device>\n",
		    progname, progname);
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
	size_t	protosize;
	struct	utsname utsname;

	/*
	 * For UltraSPARC machines, we turn on the uflag by default.
	 */
	if (uname(&utsname) == -1)
		err(1, "uname");
	if (strcmp(utsname.machine, "sparc64") == 0)
		sparc64 = uflag = 1;

	while ((c = getopt(argc, argv, "a:nhuUv")) != -1) {
		switch (c) {
		case 'a':
			warnx("-a option is obsolete");
			break;
		case 'h':	/* Note: for backwards compatibility */
			/* Don't strip a.out header */
			warnx("-h option is obsolete");
			break;
		case 'n':
			/* Do not actually write the bootblock to disk */
			nowrite = 1;
			break;
		case 'u':
			/* UltraSPARC boot block */
			uflag = 1;
			break;
		case 'U':
			/* Force non-ultrasparc */
			uflag = 0;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (uflag) {
		if (argc - optind < 2)
			usage();
	} else {
		if (argc - optind < 3)
			usage();
		boot = argv[optind++];
	}

	proto = argv[optind++];
	dev = argv[optind];

	if (verbose) {
		if (!uflag)
			printf("boot: %s\n", boot);
		printf("proto: %s\n", proto);
		printf("device: %s\n", dev);
	}

	/* Load proto blocks into core */
	if (uflag == 0) {
		if ((protostore = loadprotoblocks(proto, &protosize)) == NULL)
			exit(1);

		/* Open and check raw disk device */
		if ((devfd = open(dev, O_RDONLY, 0)) < 0)
			err(1, "open: %s", dev);

		/* Extract and load block numbers */
		if (loadblocknums(boot, devfd) != 0)
			exit(1);

		(void)close(devfd);
	} else {
		struct stat sb;
		int protofd;
		size_t blanklen;

		if ((protofd = open(proto, O_RDONLY)) < 0)
			err(1, "open: %s", proto);

		if (fstat(protofd, &sb) < 0)
			err(1, "fstat: %s", proto);

		/* there must be a better way */
		blanklen = DEV_BSIZE - ((sb.st_size + DEV_BSIZE) & (DEV_BSIZE - 1));
		protosize = sb.st_size + blanklen;
		if ((protostore = mmap(0, (size_t)protosize,
		    PROT_READ|PROT_WRITE, MAP_PRIVATE,
		    protofd, 0)) == MAP_FAILED)
			err(1, "mmap: %s", proto);
		/* and provide the rest of the block */
		if (blanklen)
			memset(protostore + sb.st_size, 0, blanklen);
	}

	if (nowrite)
		return 0;

	/* Write patched proto bootblocks into the superblock */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if ((devfd = open(dev, O_RDWR, 0)) < 0)
		err(1, "open: %s", dev);

	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek bootstrap");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();

	if (write(devfd, protostore, protosize) != protosize)
		err(1, "write bootstrap");
	(void)close(devfd);
	return 0;
}

char *
loadprotoblocks(fname, size)
	char *fname;
	size_t *size;
{
	int	fd, sz;
	u_long	ap, bp, st, en;
	u_long	marks[MARK_MAX];

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
	if (nl[X_BLOCKTABLE].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKTABLE].n_un.n_name);
		return NULL;
	}
	if (nl[X_BLOCKCOUNT].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKCOUNT].n_un.n_name);
		return NULL;
	}
	if (nl[X_BLOCKSIZE].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKSIZE].n_un.n_name);
		return NULL;
	}

	marks[MARK_START] = 0;
	if ((fd = loadfile(fname, marks, COUNT_TEXT|COUNT_DATA)) == -1)
		return NULL;
	(void)close(fd);

	sz = (marks[MARK_END] - marks[MARK_START]) + (hflag ? 32 : 0);
	sz = roundup(sz, DEV_BSIZE);
	st = marks[MARK_START];
	en = marks[MARK_ENTRY];

	if ((ap = (u_long)malloc(sz)) == NULL) {
		warn("malloc: %s", "");
		return NULL;
	}

	bp = ap + (hflag ? 32 : 0);
	marks[MARK_START] = bp - st;
	if ((fd = loadfile(fname, marks, LOAD_TEXT|LOAD_DATA)) == -1) {
		free((void *)ap);
		return NULL;
	}
	(void)close(fd);

	block_table = (daddr_t *) (bp + nl[X_BLOCKTABLE].n_value - st);
	block_count_p = (int32_t *)(bp + nl[X_BLOCKCOUNT].n_value - st);
	block_size_p = (int32_t *) (bp + nl[X_BLOCKSIZE].n_value - st);
	if ((int)(u_long)block_table & 3) {
		warn("%s: invalid address: block_table = %p",
		     fname, block_table);
		free((void *)ap);
		return NULL;
	}
	if ((int)(u_long)block_count_p & 3) {
		warn("%s: invalid address: block_count_p = %p",
		     fname, block_count_p);
		free((void *)ap);
		return NULL;
	}
	if ((int)(u_long)block_size_p & 3) {
		warn("%s: invalid address: block_size_p = %p",
		     fname, block_size_p);
		free((void *)ap);
		return NULL;
	}
	max_block_count = *block_count_p;

	if (verbose) {
		printf("%s: entry point %#lx\n", fname, en);
		printf("%s: a.out header %s\n", fname,
		    hflag ? "left on" : "stripped off");
		printf("proto bootblock size %d\n", sz);
		printf("room for %d filesystem blocks at %#lx\n",
		    max_block_count, nl[X_BLOCKTABLE].n_value);
	}

	if (hflag) {
		/*
		 * We convert the a.out header in-vitro into something that
		 * Sun PROMs understand.
		 * Old-style (sun4) ROMs do not expect a header at all, so
		 * we turn the first two words into code that gets us past
		 * the 32-byte header where the actual code begins. In assembly
		 * speak:
		 *	.word	MAGIC		! a NOP
		 *	ba,a	start		!
		 *	.skip	24		! pad
		 * start:
		 */
#define SUN_MAGIC	0x01030107
#define SUN4_BASTART	0x30800007	/* i.e.: ba,a `start' */
		*((int *)ap) = SUN_MAGIC;
		*((int *)ap + 1) = SUN4_BASTART;
	}

	*size = sz;
	return (char *)ap;
}

static void
devread(fd, buf, blk, size, msg)
	int	fd;
	void	*buf;
	daddr_t	blk;
	size_t	size;
	char	*msg;
{
	if (lseek(fd, dbtob(blk), SEEK_SET) != dbtob(blk))
		err(1, "%s: devread: lseek", msg);

	if (read(fd, buf, size) != size)
		err(1, "%s: devread: read", msg);
}

static char sblock[SBSIZE];

int
loadblocknums(boot, devfd)
char	*boot;
int	devfd;
{
	int		i, fd;
	struct	stat	statbuf;
	struct	statfs	statfsbuf;
	struct fs	*fs;
	char		*buf;
	daddr_t		blk, *ap;
	struct ufs1_dinode	*ip;
	int		ndb;

	/*
	 * Open 2nd-level boot program and record the block numbers
	 * it occupies on the filesystem represented by `devfd'.
	 */
	if ((fd = open(boot, O_RDONLY)) < 0)
		err(1, "open: %s", boot);

	if (fstatfs(fd, &statfsbuf) != 0)
		err(1, "statfs: %s", boot);

	if (strncmp(statfsbuf.f_fstypename, "ffs", MFSNAMELEN) &&
	    strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN)) {
		errx(1, "%s: must be on an FFS filesystem", boot);
	}

	if (fsync(fd) != 0)
		err(1, "fsync: %s", boot);

	if (fstat(fd, &statbuf) != 0)
		err(1, "fstat: %s", boot);

	close(fd);

	/* Read superblock */
	devread(devfd, sblock, btodb(SBOFF), SBSIZE, "superblock");
	fs = (struct fs *)sblock;

	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL)
		errx(1, "No memory for filesystem block");

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));
	devread(devfd, buf, blk, fs->fs_bsize, "inode");
	ip = (struct ufs1_dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Register filesystem block size.
	 */
	*block_size_p = fs->fs_bsize;

	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb > max_block_count)
		errx(1, "%s: Too many blocks", boot);

	/*
	 * Register block count.
	 */
	*block_count_p = ndb;

	if (verbose)
		printf("%s: block numbers: ", boot);
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		block_table[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb == 0)
		return 0;

	/*
	 * Just one level of indirections; there isn't much room
	 * for more in the 1st-level bootblocks anyway.
	 */
	if (verbose)
		printf("%s: block numbers (indirect): ", boot);
	blk = ip->di_ib[0];
	devread(devfd, buf, blk, fs->fs_bsize, "indirect block");
	ap = (daddr_t *)buf;
	for (; i < NINDIR(fs) && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		block_table[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb)
		errx(1, "%s: Too many blocks", boot);
	return 0;
}
