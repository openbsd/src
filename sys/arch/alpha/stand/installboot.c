/*	$NetBSD: installboot.c,v 1.1 1995/11/23 02:39:02 cgd Exp $ */

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
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bbinfo.h"

int	verbose, nowrite, hflag;
char	*boot, *proto, *dev;

struct bbinfoloc *bbinfolocp;
struct bbinfo *bbinfop;
int	max_block_count;


char		*loadprotoblocks __P((char *, long *));
int		loadblocknums __P((char *, int));
static void	devread __P((int, void *, daddr_t, size_t, char *));
static void	usage __P((void));
int 		main __P((int, char *[]));


static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] <boot> <proto> <device>\n");
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

	while ((c = getopt(argc, argv, "vn")) != EOF) {
		switch (c) {
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

	/* Open and check raw disk device */
	if ((devfd = open(dev, O_RDONLY, 0)) < 0)
		err(1, "open: %s", dev);

	/* Extract and load block numbers */
	if (loadblocknums(boot, devfd) != 0)
		exit(1);

	(void)close(devfd);

	if (nowrite)
		return 0;

#if 0
	/* Write patched proto bootblocks into the superblock */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");
#endif

	if ((devfd = open(dev, O_RDWR, 0)) < 0)
		err(1, "open: %s", dev);

	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek bootstrap");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();
	sleep(3);

	if (write(devfd, protostore, protosize) != protosize)
		err(1, "write bootstrap");

	{

#define BBPAD   0x1e0
	struct bb {
		char    bb_pad[BBPAD];  /* disklabel lives in here, actually */
		long    bb_secsize;     /* size of secondary boot block */
		long    bb_secstart;    /* start of secondary boot block */
		long    bb_flags;       /* unknown; always zero */
		long    bb_cksum;       /* checksum of the the boot block, as longs. */
	} bb;
	long *lp, *ep;

	if (lseek(devfd, 0, SEEK_SET) != 0)
		err(1, "lseek label");

	if (read(devfd, &bb, sizeof (bb)) != sizeof (bb)) 
		err(1, "read label");

        bb.bb_secsize = 15;
        bb.bb_secstart = 1;
        bb.bb_flags = 0;
        bb.bb_cksum = 0;

        for (lp = (long *)&bb, ep = &bb.bb_cksum; lp < ep; lp++)
                bb.bb_cksum += *lp;

	if (lseek(devfd, 0, SEEK_SET) != 0)
		err(1, "lseek label 2");

        if (write(devfd, &bb, sizeof bb) != sizeof bb)
		err(1, "write label ");
	}

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
	u_int64_t *matchp;

	/*
	 * Read the prototype boot block into memory.
	 */
	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (fstat(fd, &statbuf) != 0) {
		warn("fstat: %s", fname);
		close(fd);
		return NULL;
	}
	sz = roundup(statbuf.st_size, DEV_BSIZE);
	if ((bp = calloc(sz, 1)) == NULL) {
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

	/*
	 * Find the magic area of the program, and figure out where
	 * the 'blocks' struct is, from that.
	 */
	bbinfolocp = NULL;
	for (matchp = (u_int64_t *)bp; (char *)matchp < bp + sz; matchp++) {
		if (*matchp != 0xbabefacedeadbeef)
			continue;
		bbinfolocp = (struct bbinfoloc *)matchp;
		if (bbinfolocp->magic1 == 0xbabefacedeadbeef &&
		    bbinfolocp->magic2 == 0xdeadbeeffacebabe)
			break;
		bbinfolocp = NULL;
	}

	if (bbinfolocp == NULL) {
		warnx("%s: not a valid boot block?", fname);
		return NULL;
	}

	bbinfop = (struct bbinfo *)(bp + bbinfolocp->end - bbinfolocp->start);	
	memset(bbinfop, 0, sz - (bbinfolocp->end - bbinfolocp->start));
	max_block_count =
	    ((char *)bbinfop->blocks - bp) / sizeof (bbinfop->blocks[0]);

	if (verbose) {
		printf("boot block info locator at offset 0x%x\n",
			(char *)bbinfolocp - bp);
		printf("boot block info at offset 0x%x\n",
			(char *)bbinfop - bp);
		printf("max number of blocks: %d\n", max_block_count);
	}

	*size = sz;
	return (bp);
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
	int32_t		cksum;

	/*
	 * Open 2nd-level boot program and record the block numbers
	 * it occupies on the filesystem represented by `devfd'.
	 */
	if ((fd = open(boot, O_RDONLY)) < 0)
		err(1, "open: %s", boot);

	if (fstatfs(fd, &statfsbuf) != 0)
		err(1, "statfs: %s", boot);

	if (strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN))
		errx(1, "%s: must be on a UFS filesystem", boot);

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
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Register filesystem block size.
	 */
	bbinfop->bsize = fs->fs_bsize;

	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb > max_block_count)
		errx(1, "%s: Too many blocks", boot);

	/*
	 * Register block count.
	 */
	bbinfop->nblocks = ndb;

	if (verbose)
		printf("%s: block numbers: ", boot);
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		bbinfop->blocks[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb == 0)
		goto checksum;

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
		bbinfop->blocks[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb)
		errx(1, "%s: Too many blocks", boot);

checksum:
	cksum = 0;
	for (i = 0; i < bbinfop->nblocks +
	    (sizeof (*bbinfop) / sizeof (bbinfop->blocks[0])) - 1; i++) {
		cksum += ((int32_t *)bbinfop)[i];
	}
	bbinfop->cksum = -cksum;

	return 0;
}
