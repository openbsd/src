/*	$OpenBSD: installboot.c,v 1.6 2011/03/12 19:40:34 deraadt Exp $	*/
/*	$NetBSD: installboot.c,v 1.1 1997/06/01 03:39:45 mrg Exp $	*/

/*
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
#include <sys/sysctl.h>
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

int	verbose, nowrite, hflag;
char	*boot, *proto, *dev;

struct nlist nl[] = {
#define X_BLOCKTABLE	0
	{"_block_table"},
#define X_BLOCKCOUNT	1
	{"_block_count"},
#define X_BLOCKSIZE	2
	{"_block_size"},
	{NULL}
};
daddr_t	*block_table;		/* block number array in prototype image */
int32_t	*block_count_p;		/* size of this array */
int32_t	*block_size_p;		/* filesystem block size */
int32_t	max_block_count;

char	*karch;
char	cpumodel[130];

int	isofsblk = 0;
int	isofseblk = 0;

char		*loadprotoblocks(char *, long *);
int		loadblocknums(char *, int);
static void	devread(int, void *, daddr_t, size_t, char *);
static void	usage(void);
int 		main(int, char *[]);


static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] [-h] [-s isofsblk -e isofseblk] [-a <karch>] <boot> <proto> <device>\n");
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
	int	mib[2];
	size_t	size;

	while ((c = getopt(argc, argv, "a:vnhs:e:")) != -1) {
		switch (c) {
		case 'a':
			karch = optarg;
			break;
		case 'h':	/* Note: for backwards compatibility */
			/* Don't strip a.out header */
			hflag = 1;
			break;
		case 'n':
			/* Do not actually write the bootblock to disk */
			nowrite = 1;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		case 's':
			isofsblk = atoi(optarg);
			break;
		case 'e':
			isofseblk = atoi(optarg);
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
	dev = argv[optind + 2];

	if (karch == NULL) {
		mib[0] = CTL_HW;
		mib[1] = HW_MODEL;
		size = sizeof(cpumodel);
		if (sysctl(mib, 2, cpumodel, &size, NULL, 0) == -1)
			err(1, "sysctl");

		if (size < 5 || strncmp(cpumodel, "SUN-4", 5) != 0) /*XXX*/ 
			/* Assume a sun4c/sun4m */
			karch = "sun4c";
		else
			karch = "sun4";
	}

	if (verbose) {
		printf("boot: %s\n", boot);
		printf("proto: %s\n", proto);
		printf("device: %s\n", dev);
		printf("architecture: %s\n", karch);
	}

	if (strcmp(karch, "sun4") == 0) {
		hflag = 0;
	} else if (strcmp(karch, "sun4c") == 0) {
		hflag = 1;
	} else if (strcmp(karch, "sun4m") == 0) {
		hflag = 1;
	} else
		errx(1, "Unsupported architecture");

	/* Load proto blocks into core */
	if ((protostore = loadprotoblocks(proto, &protosize)) == NULL)
		exit(1);

	/* Open and check raw disk device */
	if ((devfd = open(dev, O_RDONLY, 0)) < 0)
		err(1, "open: %s", dev);

	/* Extract and load block numbers */
	if (loadblocknums(boot, devfd) != 0)
		exit(1);

	(void)close(devfd);

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
	long *size;
{
	int	fd, sz;
	char	*bp;
	struct	stat statbuf;
	struct	exec *hp;
	long	off;

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

	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (fstat(fd, &statbuf) != 0) {
		warn("fstat: %s", fname);
		close(fd);
		return NULL;
	}
	if (statbuf.st_size == 0) {
		warn("%s is empty", fname);
		close(fd);
		return NULL;
	}

	if ((bp = calloc(roundup(statbuf.st_size, DEV_BSIZE), 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		close(fd);
		return NULL;
	}
	if (read(fd, bp, statbuf.st_size) != statbuf.st_size) {
		warn("read: %s", fname);
		free(bp);
		close(fd);
		return NULL;
	}
	close(fd);

	hp = (struct exec *)bp;
	sz = (hflag ? sizeof(*hp) : 0) + hp->a_text + hp->a_data;
	sz = roundup(sz, DEV_BSIZE);

	/* Calculate the symbols' location within the proto file */
	off = N_DATOFF(*hp) - N_DATADDR(*hp) - (hp->a_entry - N_TXTADDR(*hp));
	block_table = (daddr_t *) (bp + nl[X_BLOCKTABLE].n_value + off);
	block_count_p = (int32_t *)(bp + nl[X_BLOCKCOUNT].n_value + off);
	block_size_p = (int32_t *) (bp + nl[X_BLOCKSIZE].n_value + off);
	if ((int)block_table & 3) {
		warn("%s: invalid address: block_table = %x",
		     fname, block_table);
		free(bp);
		close(fd);
		return NULL;
	}
	if ((int)block_count_p & 3) {
		warn("%s: invalid address: block_count_p = %x",
		     fname, block_count_p);
		free(bp);
		close(fd);
		return NULL;
	}
	if ((int)block_size_p & 3) {
		warn("%s: invalid address: block_size_p = %x",
		     fname, block_size_p);
		free(bp);
		close(fd);
		return NULL;
	}
	max_block_count = *block_count_p;

	if (verbose) {
		printf("%s: entry point %#x\n", fname, hp->a_entry);
		printf("%s: a.out header %s\n", fname,
			hflag?"left on":"stripped off");
		printf("proto bootblock size %ld\n", sz);
		printf("room for %d filesystem blocks at %#x\n",
			max_block_count, nl[X_BLOCKTABLE].n_value);
	}

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
	*((int *)bp) = SUN_MAGIC;
	*((int *)bp + 1) = SUN4_BASTART;

	*size = sz;
	return (hflag ? bp : (bp + sizeof(struct exec)));
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

	if (isofsblk) {
		int i;

		*block_size_p = 512;
		*block_count_p = (isofseblk - isofsblk + 1) * (2048/512);
		if (*block_count_p > max_block_count)
			errx(1, "%s: Too many blocks", boot);
		if (verbose)
			printf("%s: %d block numbers: ", boot, *block_count_p);
		for (i = 0; i < *block_count_p; i++) {
			blk = (isofsblk * (2048/512)) + i;
			block_table[i] = blk;
			if (verbose)
				printf("%d ", blk);
		}
		if (verbose)
			printf("\n");
		return 0;
	}

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
