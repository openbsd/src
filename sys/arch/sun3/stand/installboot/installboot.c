/*	$NetBSD: installboot.c,v 1.4 1995/11/07 23:01:40 gwr Exp $ */

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
#define X_BLOCK_SIZE	0
	{"_block_size"},
#define X_BLOCK_COUNT	1
	{"_block_count"},
#define X_BLOCK_TABLE	2
	{"_block_table"},
	{NULL}
};

int *block_size_p;		/* block size var. in prototype image */
int *block_count_p;		/* block count var. in prototype image */
daddr_t	*block_table;	/* block number array in prototype image */
int	maxblocknum;		/* size of this array */


char		*loadprotoblocks __P((char *, long *));
int		loadblocknums __P((char *, int));
static void	devread __P((int, void *, daddr_t, size_t, char *));
static void	usage __P((void));
int 		main __P((int, char *[]));


static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] [-h] <boot> <proto> <device>\n");
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

	while ((c = getopt(argc, argv, "vnh")) != EOF) {
		switch (c) {
		case 'h':
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

	if (verbose) {
		printf("boot: %s\n", boot);
		printf("proto: %s\n", proto);
		printf("device: %s\n", dev);
	}

	/* Load proto blocks into core */
	if ((protostore = loadprotoblocks(proto, &protosize)) == NULL)
		exit(1);

	/* XXX - Paranoia: Make sure size is aligned! */
	if (protosize & (DEV_BSIZE - 1))
		err(1, "proto bootblock bad size=%d", protosize);

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
	int	fd;
	size_t	tdsize;		/* text+data size */
	size_t	bbsize;		/* boot block size (block aligned) */
	char	*bp;
	struct	nlist *nlp;
	struct	exec eh;
	long	off;

	fd = -1;
	bp = NULL;

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
	/* Validate symbol types (global data). */
	for (nlp = nl; nlp->n_un.n_name; nlp++) {
		if (nlp->n_type != (N_DATA | N_EXT)) {
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
		warn("bad magic: 0x%x", eh.a_midmag);
		goto bad;
	}
	/*
	 * We have to include the exec header in the beginning of
	 * the buffer, and leave extra space at the end in case
	 * the actual write to disk wants to skip the header.
	 */
	tdsize = eh.a_text + eh.a_data;
	bbsize = tdsize + sizeof(eh);
	bbsize = roundup(bbsize, DEV_BSIZE);

	/*
	 * Allocate extra space here because the caller may copy
	 * the boot block starting at the end of the exec header.
	 * This prevents reading beyond the end of the buffer.
	 */
	if ((bp = calloc(bbsize + sizeof(eh), 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		goto bad;
	}
	/* Copy the exec header and read the rest of the file. */
	memcpy(bp, &eh, sizeof(eh));
	if (read(fd, bp+sizeof(eh), tdsize) != tdsize) {
		warn("read: %s", fname);
		goto bad;
	}

	*size = bbsize;	/* aligned to DEV_BSIZE */

	/* Calculate the symbols' locations within the proto file */
	off = N_DATOFF(eh) - N_DATADDR(eh) - (eh.a_entry - N_TXTADDR(eh));
	block_size_p  =   (int *) (bp + nl[X_BLOCK_SIZE ].n_value + off);
	block_count_p =   (int *) (bp + nl[X_BLOCK_COUNT].n_value + off);
	block_table = (daddr_t *) (bp + nl[X_BLOCK_TABLE].n_value + off);
	maxblocknum = *block_count_p;

	if (verbose) {
		printf("%s: entry point %#x\n", fname, eh.a_entry);
		printf("proto bootblock size %ld\n", *size);
		printf("room for %d filesystem blocks at %#x\n",
			maxblocknum, nl[X_BLOCK_TABLE].n_value);
	}

	close(fd);
	if (!hflag)
		bp += sizeof(struct exec);
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
	struct dinode	*ip;
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
	    strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN) ) {
		errx(1, "%s: must be on an FFS filesystem", boot);
	}

	if (fsync(fd) != 0)
		err(1, "fsync: %s", boot);

	if (fstat(fd, &statbuf) != 0)
		err(1, "fstat: %s", boot);

	close(fd);

	/* Read superblock */
	devread(devfd, sblock, SBLOCK, SBSIZE, "superblock");
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
	devread(devfd, buf, blk, fs->fs_bsize, "inode");
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Have the inode.  Figure out how many blocks we need.
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb > maxblocknum)
		errx(1, "Too many blocks");
	*block_count_p = ndb;
	*block_size_p = fs->fs_bsize;
	if (verbose)
		printf("Will load %d blocks of size %d each.\n",
			   ndb, fs->fs_bsize);

	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		if (verbose)
			printf("%d: %d\n", i, blk);
		block_table[i] = blk;
	}
	if (ndb == 0)
		return 0;

	/*
	 * Just one level of indirections; there isn't much room
	 * for more in the 1st-level bootblocks anyway.
	 */
	blk = fsbtodb(fs, ip->di_ib[0]);
	devread(devfd, buf, blk, fs->fs_bsize, "indirect block");
	ap = (daddr_t *)buf;
	for (; i < NINDIR(fs) && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		if (verbose)
			printf("%d: %d\n", i, blk);
		block_table[i] = blk;
	}

	return 0;
}

