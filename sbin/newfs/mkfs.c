/*	$OpenBSD: mkfs.c,v 1.43 2004/08/12 07:53:50 otto Exp $	*/
/*	$NetBSD: mkfs.c,v 1.25 1995/06/18 21:35:38 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkfs.c	8.3 (Berkeley) 2/3/94";
#else
static char rcsid[] = "$OpenBSD: mkfs.c,v 1.43 2004/08/12 07:53:50 otto Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef STANDALONE
#include <a.out.h>
#include <stdio.h>
#include <errno.h>
#endif

/*
 * make file system for cylinder-group style file systems
 */

/*
 * We limit the size of the inode map to be no more than a
 * third of the cylinder group space, since we must leave at
 * least an equal amount of space for the block map.
 *
 * N.B.: MAXIPG must be a multiple of INOPB(fs).
 */
#define MAXIPG(fs)	roundup((fs)->fs_bsize * NBBY / 3, INOPB(fs))

#define UMASK		0755
#define MAXINOPB	(MAXBSIZE / sizeof(struct ufs1_dinode))
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

/*
 * variables set up by front end.
 */
extern int	mfs;		/* run as the memory based filesystem */
extern int	Nflag;		/* run mkfs without writing file system */
extern int	Oflag;		/* format as an 4.3BSD file system */
extern int	fssize;		/* file system size */
extern int	ntracks;	/* # tracks/cylinder */
extern int	nsectors;	/* # sectors/track */
extern int	nphyssectors;	/* # sectors/track including spares */
extern int	secpercyl;	/* sectors per cylinder */
extern int	sectorsize;	/* bytes/sector */
extern int	rpm;		/* revolutions/minute of drive */
extern int	interleave;	/* hardware sector interleave */
extern int	trackskew;	/* sector 0 skew, per track */
extern int	fsize;		/* fragment size */
extern int	bsize;		/* block size */
extern int	cpg;		/* cylinders/cylinder group */
extern int	cpgflg;		/* cylinders/cylinder group flag was given */
extern int	minfree;	/* free space threshold */
extern int	opt;		/* optimization preference (space or time) */
extern int	density;	/* number of bytes per inode */
extern int	maxcontig;	/* max contiguous blocks to allocate */
extern int	rotdelay;	/* rotational delay between blocks */
extern int	maxbpg;		/* maximum blocks per file in a cyl group */
extern int	nrpos;		/* # of distinguished rotational positions */
extern int	bbsize;		/* boot block size */
extern int	sbsize;		/* superblock size */
extern int	avgfilesize;	/* expected average file size */
extern int	avgfilesperdir;	/* expected number of files per directory */
extern int	quiet;		/* quiet flag */
extern caddr_t	membase;	/* start address of memory based filesystem */

union fs_u {
	struct fs fs;
	char pad[SBSIZE];
} *fsun;
#define sblock	fsun->fs

struct	csum *fscs;

union cg_u {
	struct cg cg;
	char pad[MAXBSIZE];
} *cgun;
#define acg	cgun->cg

struct ufs1_dinode *zino;
char	*buf;

int	fsi, fso;

daddr_t		alloc(int, int);
static int	charsperline(void);
void		initcg(int, time_t);
void		wtfs(daddr_t, int, void *);
void		fsinit(time_t, mode_t, uid_t, gid_t);
int		makedir(struct direct *, int);
void		iput(struct ufs1_dinode *, ino_t);
void		setblock(struct fs *, unsigned char *, int);
void		clrblock(struct fs *, unsigned char *, int);
int		isblock(struct fs *, unsigned char *, int);
void		rdfs(daddr_t, int, void *);
void		mkfs(struct partition *, char *, int, int,
		    mode_t, uid_t, gid_t);

#ifndef STANDALONE
volatile sig_atomic_t cur_cylno;
volatile const char *cur_fsys;

void
siginfo(int sig)
{
	int save_errno = errno;
	char buf[128];

	snprintf(buf, sizeof(buf), "%s: initializing cg %ld/%d\n",
	    cur_fsys, (long)cur_cylno, sblock.fs_ncg);
	write(STDERR_FILENO, buf, strlen(buf));
	errno = save_errno;
}
#endif

void
mkfs(struct partition *pp, char *fsys, int fi, int fo,
    mode_t mfsmode, uid_t mfsuid, gid_t mfsgid)
{
	long i, mincpc, mincpg, inospercg;
	long cylno, rpos, blk, j, warn = 0;
	long used, mincpgcnt, bpcg;
	long mapcramped, inodecramped;
	long postblsize, rotblsize, totalsbsize;
	time_t utime;
	quad_t sizepb;
	int width;
	char tmpbuf[100];	/* XXX this will break in about 2,500 years */

	if ((fsun = calloc(1, sizeof (union fs_u))) == NULL ||
	    (cgun = calloc(1, sizeof (union cg_u))) == NULL ||
	    (zino = calloc(1, MAXBSIZE)) == NULL ||
	    (buf = calloc(1, MAXBSIZE)) == NULL) {
		err(1, "calloc");
	}

#ifndef STANDALONE
	time(&utime);
#endif
	if (mfs) {
		membase = mmap(NULL, fssize * sectorsize, PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, (off_t)0);
		if (membase == MAP_FAILED)
			err(12, "mmap");
		madvise(membase, fssize * sectorsize, MADV_RANDOM);
	}
	fsi = fi;
	fso = fo;
	if (Oflag) {
		sblock.fs_inodefmt = FS_42INODEFMT;
		sblock.fs_maxsymlinklen = 0;
	} else {
		sblock.fs_inodefmt = FS_44INODEFMT;
		sblock.fs_maxsymlinklen = MAXSYMLINKLEN;
	}
	/*
	 * Validate the given file system size.
	 * Verify that its last block can actually be accessed.
	 */
	if (fssize <= 0)
		errx(13, "preposterous size %d", fssize);
	wtfs(fssize - 1, sectorsize, (char *)&sblock);
recalc:
	/*
	 * collect and verify the sector and track info
	 */
	sblock.fs_nsect = nsectors;
	sblock.fs_ntrak = ntracks;
	if (sblock.fs_ntrak <= 0)
		errx(14, "preposterous ntrak %d", sblock.fs_ntrak);
	if (sblock.fs_nsect <= 0)
		errx(15, "preposterous nsect %d", sblock.fs_nsect);
	/*
	 * collect and verify the filesystem density info
	 */
	sblock.fs_avgfilesize = avgfilesize;
	sblock.fs_avgfpdir = avgfilesperdir;
	if (sblock.fs_avgfilesize <= 0)
		errx(14, "illegal expected average file size %d",
		    sblock.fs_avgfilesize);
	if (sblock.fs_avgfpdir <= 0)
		errx(15, "illegal expected number of files per directory %d",
		    sblock.fs_avgfpdir);
	/*
	 * collect and verify the block and fragment sizes
	 */
	sblock.fs_bsize = bsize;
	sblock.fs_fsize = fsize;
	if (!POWEROF2(sblock.fs_bsize)) {
		errx(16, "block size must be a power of 2, not %d",
		     sblock.fs_bsize);
	}
	if (!POWEROF2(sblock.fs_fsize)) {
		errx(17, "fragment size must be a power of 2, not %d",
		     sblock.fs_fsize);
	}
	if (sblock.fs_fsize < sectorsize) {
		errx(18, "fragment size %d is too small, minimum is %d",
		     sblock.fs_fsize, sectorsize);
	}
	if (sblock.fs_bsize < MINBSIZE) {
		errx(19, "block size %d is too small, minimum is %d",
		     sblock.fs_bsize, MINBSIZE);
	}
	if (sblock.fs_bsize > MAXBSIZE) {
		errx(19, "block size %d is too large, maximum is %d",
		     sblock.fs_bsize, MAXBSIZE);
	}
	if (sblock.fs_bsize < sblock.fs_fsize) {
		errx(20, "block size (%d) cannot be smaller than fragment size (%d)",
		     sblock.fs_bsize, sblock.fs_fsize);
	}
	sblock.fs_bmask = ~(sblock.fs_bsize - 1);
	sblock.fs_fmask = ~(sblock.fs_fsize - 1);
	sblock.fs_qbmask = ~sblock.fs_bmask;
	sblock.fs_qfmask = ~sblock.fs_fmask;
	for (sblock.fs_bshift = 0, i = sblock.fs_bsize; i > 1; i >>= 1)
		sblock.fs_bshift++;
	for (sblock.fs_fshift = 0, i = sblock.fs_fsize; i > 1; i >>= 1)
		sblock.fs_fshift++;
	sblock.fs_frag = numfrags(&sblock, sblock.fs_bsize);
	for (sblock.fs_fragshift = 0, i = sblock.fs_frag; i > 1; i >>= 1)
		sblock.fs_fragshift++;
	if (sblock.fs_frag > MAXFRAG) {
		errx(21, "fragment size %d is too small, minimum with block size %d is %d",
		    sblock.fs_fsize, sblock.fs_bsize,
		    sblock.fs_bsize / MAXFRAG);
	}
	sblock.fs_nrpos = nrpos;
	sblock.fs_nindir = sblock.fs_bsize / sizeof(daddr_t);
	sblock.fs_inopb = sblock.fs_bsize / sizeof(struct ufs1_dinode);
	sblock.fs_nspf = sblock.fs_fsize / sectorsize;
	for (sblock.fs_fsbtodb = 0, i = NSPF(&sblock); i > 1; i >>= 1)
		sblock.fs_fsbtodb++;
	sblock.fs_sblkno =
	    roundup(howmany(bbsize + sbsize, sblock.fs_fsize), sblock.fs_frag);
	sblock.fs_cblkno = (daddr_t)(sblock.fs_sblkno +
	    roundup(howmany(sbsize, sblock.fs_fsize), sblock.fs_frag));
	sblock.fs_iblkno = sblock.fs_cblkno + sblock.fs_frag;
	sblock.fs_cgoffset = roundup(
	    howmany(sblock.fs_nsect, NSPF(&sblock)), sblock.fs_frag);
	for (sblock.fs_cgmask = 0xffffffff, i = sblock.fs_ntrak; i > 1; i >>= 1)
		sblock.fs_cgmask <<= 1;
	if (!POWEROF2(sblock.fs_ntrak))
		sblock.fs_cgmask <<= 1;
	sblock.fs_maxfilesize = sblock.fs_bsize * NDADDR - 1;
	for (sizepb = sblock.fs_bsize, i = 0; i < NIADDR; i++) {
		sizepb *= NINDIR(&sblock);
		sblock.fs_maxfilesize += sizepb;
	}
	/*
	 * Validate specified/determined secpercyl
	 * and calculate minimum cylinders per group.
	 */
	sblock.fs_spc = secpercyl;
	for (sblock.fs_cpc = NSPB(&sblock), i = sblock.fs_spc;
	     sblock.fs_cpc > 1 && (i & 1) == 0;
	     sblock.fs_cpc >>= 1, i >>= 1)
		/* void */;
	mincpc = sblock.fs_cpc;
	bpcg = sblock.fs_spc * sectorsize;
	inospercg = roundup(bpcg / sizeof(struct ufs1_dinode), INOPB(&sblock));
	if (inospercg > MAXIPG(&sblock))
		inospercg = MAXIPG(&sblock);
	used = (sblock.fs_iblkno + inospercg / INOPF(&sblock)) * NSPF(&sblock);
	mincpgcnt = howmany(sblock.fs_cgoffset * (~sblock.fs_cgmask) + used,
	    sblock.fs_spc);
	mincpg = roundup(mincpgcnt, mincpc);
	/*
	 * Ensure that cylinder group with mincpg has enough space
	 * for block maps.
	 */
	sblock.fs_cpg = mincpg;
	sblock.fs_ipg = inospercg;
	if (maxcontig > 1)
		sblock.fs_contigsumsize = MIN(maxcontig, FS_MAXCONTIG);
	mapcramped = 0;
	/* A cylinder group *must* fit inside one block so force it if not. */
	if (CGSIZE(&sblock) > sblock.fs_bsize && secpercyl > 1024 && ntracks > 1) {
		secpercyl /= 2;
		ntracks /= 2;
		goto recalc;
	}
	while (CGSIZE(&sblock) > sblock.fs_bsize) {
		mapcramped = 1;
		if (sblock.fs_bsize < MAXBSIZE) {
			sblock.fs_bsize <<= 1;
			if ((i & 1) == 0) {
				i >>= 1;
			} else {
				sblock.fs_cpc <<= 1;
				mincpc <<= 1;
				mincpg = roundup(mincpgcnt, mincpc);
				sblock.fs_cpg = mincpg;
			}
			sblock.fs_frag <<= 1;
			sblock.fs_fragshift += 1;
			if (sblock.fs_frag <= MAXFRAG)
				continue;
		}
		if (sblock.fs_fsize == sblock.fs_bsize) {
			errx(22, "no block size to support this disk");
		}
		sblock.fs_frag >>= 1;
		sblock.fs_fragshift -= 1;
		sblock.fs_fsize <<= 1;
		sblock.fs_nspf <<= 1;
	}
	/*
	 * Ensure that cylinder group with mincpg has enough space for inodes.
	 */
	inodecramped = 0;
	used *= sectorsize;
	inospercg = roundup((mincpg * bpcg - used) / density, INOPB(&sblock));
	sblock.fs_ipg = inospercg;
	while (inospercg > MAXIPG(&sblock)) {
		inodecramped = 1;
		if (mincpc == 1 || sblock.fs_frag == 1 ||
		    sblock.fs_bsize == MINBSIZE)
			break;
		printf("With a block size of %d %s %ld\n",
		    sblock.fs_bsize, "minimum bytes per inode is",
		    (mincpg * bpcg - used) / MAXIPG(&sblock) + 1);
		sblock.fs_bsize >>= 1;
		sblock.fs_frag >>= 1;
		sblock.fs_fragshift -= 1;
		mincpc >>= 1;
		sblock.fs_cpg = roundup(mincpgcnt, mincpc);
		if (CGSIZE(&sblock) > sblock.fs_bsize) {
			sblock.fs_bsize <<= 1;
			break;
		}
		mincpg = sblock.fs_cpg;
		inospercg =
		    roundup((mincpg * bpcg - used) / density, INOPB(&sblock));
		sblock.fs_ipg = inospercg;
	}
	if (inodecramped) {
		if (inospercg > MAXIPG(&sblock)) {
			printf("Minimum bytes per inode is %ld\n",
			    (mincpg * bpcg - used) / MAXIPG(&sblock) + 1);
		} else if (!mapcramped) {
			printf("With %d bytes per inode, ", density);
			printf("minimum cylinders per group is %ld\n", mincpg);
		}
	}
	if (mapcramped) {
		printf("With %d sectors per cylinder, ", sblock.fs_spc);
		printf("minimum cylinders per group is %ld\n", mincpg);
	}
	if (inodecramped || mapcramped) {
		if (sblock.fs_bsize != bsize)
			printf("%s to be changed from %d to %d\n",
			    "This requires the block size",
			    bsize, sblock.fs_bsize);
		if (sblock.fs_fsize != fsize)
			printf("\t%s to be changed from %d to %d\n",
			    "and the fragment size",
			    fsize, sblock.fs_fsize);
		exit(23);
	}
	/*
	 * Calculate the number of cylinders per group
	 */
	sblock.fs_cpg = cpg;
	if (sblock.fs_cpg % mincpc != 0) {
		if (!quiet)
			printf("%s groups must have a multiple of %ld "
			    "cylinders\n", cpgflg ? "Cylinder" :
			    "Warning: cylinder", mincpc);
		sblock.fs_cpg = roundup(sblock.fs_cpg, mincpc);
		if (!cpgflg)
			cpg = sblock.fs_cpg;
	}
	/*
	 * Must ensure there is enough space for inodes.
	 */
	sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / density,
		INOPB(&sblock));
	while (sblock.fs_ipg > MAXIPG(&sblock)) {
		inodecramped = 1;
		sblock.fs_cpg -= mincpc;
		sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / density,
			INOPB(&sblock));
	}
	/*
	 * Must ensure there is enough space to hold block map.
	 */
	if (CGSIZE(&sblock) > sblock.fs_bsize && secpercyl > 1024 && ntracks > 1) {
		secpercyl /= 2;
		ntracks /= 2;
		goto recalc;
	}
	while (CGSIZE(&sblock) > sblock.fs_bsize) {
		mapcramped = 1;
		sblock.fs_cpg -= mincpc;
		sblock.fs_ipg = roundup((sblock.fs_cpg * bpcg - used) / density,
			INOPB(&sblock));
	}
	sblock.fs_fpg = (sblock.fs_cpg * sblock.fs_spc) / NSPF(&sblock);
	if ((sblock.fs_cpg * sblock.fs_spc) % NSPB(&sblock) != 0) {
		errx(24, "panic (fs_cpg * fs_spc) %% NSPF != 0");
	}
	if (sblock.fs_cpg < mincpg) {
		errx(25, "cylinder groups must have at least %ld cylinders",
			mincpg);
	} else if (cpgflg && sblock.fs_cpg != cpg) {
		if (!mapcramped && !inodecramped)
			errx(26, "!mapcramped && !inodecramped");
		if (mapcramped && inodecramped)
			printf("Block size and bytes per inode restrict");
		else if (mapcramped)
			printf("Block size restricts");
		else
			printf("Bytes per inode restrict");
		printf(" cylinders per group to %d.\n", sblock.fs_cpg);
		if (cpgflg)
			exit(27);
	}
	sblock.fs_cgsize = fragroundup(&sblock, CGSIZE(&sblock));
	/*
	 * Now have size for file system and nsect and ntrak.
	 * Determine number of cylinders and blocks in the file system.
	 */
	sblock.fs_size = dbtofsb(&sblock, fssize);
	sblock.fs_ncyl = sblock.fs_size * NSPF(&sblock) / sblock.fs_spc;
	if (sblock.fs_size * NSPF(&sblock) > sblock.fs_ncyl * sblock.fs_spc) {
		sblock.fs_ncyl++;
		warn = 1;
	}
	if (sblock.fs_ncyl < 1) {
		errx(28,
		    "file systems must have at least one cylinder (not %d)",
		     sblock.fs_ncyl);
	}
	/*
	 * Determine feasability/values of rotational layout tables.
	 *
	 * The size of the rotational layout tables is limited by the
	 * size of the superblock, SBSIZE. The amount of space available
	 * for tables is calculated as (SBSIZE - sizeof (struct fs)).
	 * The size of these tables is inversely proportional to the block
	 * size of the file system. The size increases if sectors per track
	 * are not powers of two, because more cylinders must be described
	 * by the tables before the rotational pattern repeats (fs_cpc).
	 */
	sblock.fs_interleave = interleave;
	sblock.fs_trackskew = trackskew;
	sblock.fs_npsect = nphyssectors;
	sblock.fs_postblformat = FS_DYNAMICPOSTBLFMT;
	sblock.fs_sbsize = fragroundup(&sblock, sizeof(struct fs));
	if (sblock.fs_ntrak == 1) {
		sblock.fs_cpc = 0;
		goto next;
	}
	postblsize = sblock.fs_nrpos * sblock.fs_cpc * sizeof(int16_t);
	rotblsize = sblock.fs_cpc * sblock.fs_spc / NSPB(&sblock);
	totalsbsize = sizeof(struct fs) + rotblsize;
	if (sblock.fs_nrpos == 8 && sblock.fs_cpc <= 16) {
		/* use old static table space */
		sblock.fs_postbloff = (char *)(&sblock.fs_opostbl[0][0]) -
		    (char *)(&sblock.fs_firstfield);
		sblock.fs_rotbloff = &sblock.fs_space[0] -
		    (u_char *)(&sblock.fs_firstfield);
	} else {
		/* use dynamic table space */
		sblock.fs_postbloff = &sblock.fs_space[0] -
		    (u_char *)(&sblock.fs_firstfield);
		sblock.fs_rotbloff = sblock.fs_postbloff + postblsize;
		totalsbsize += postblsize;
	}
	if (totalsbsize > SBSIZE ||
	    sblock.fs_nsect > (1 << NBBY) * NSPB(&sblock)) {
		printf("%s %s %d %s %d.%s",
		    "Warning: insufficient space in super block for\n",
		    "rotational layout tables with nsect", sblock.fs_nsect,
		    "and ntrak", sblock.fs_ntrak,
		    "\nFile system performance may be impaired.\n");
		sblock.fs_cpc = 0;
		goto next;
	}
	sblock.fs_sbsize = fragroundup(&sblock, totalsbsize);
	/*
	 * calculate the available blocks for each rotational position
	 */
	for (cylno = 0; cylno < sblock.fs_cpc; cylno++)
		for (rpos = 0; rpos < sblock.fs_nrpos; rpos++)
			fs_postbl(&sblock, cylno)[rpos] = -1;
	for (i = (rotblsize - 1) * sblock.fs_frag;
	     i >= 0; i -= sblock.fs_frag) {
		cylno = cbtocylno(&sblock, i);
		rpos = cbtorpos(&sblock, i);
		blk = fragstoblks(&sblock, i);
		if (fs_postbl(&sblock, cylno)[rpos] == -1)
			fs_rotbl(&sblock)[blk] = 0;
		else
			fs_rotbl(&sblock)[blk] =
			    fs_postbl(&sblock, cylno)[rpos] - blk;
		fs_postbl(&sblock, cylno)[rpos] = blk;
	}
next:
	/*
	 * Compute/validate number of cylinder groups.
	 */
	sblock.fs_ncg = sblock.fs_ncyl / sblock.fs_cpg;
	if (sblock.fs_ncyl % sblock.fs_cpg)
		sblock.fs_ncg++;
	sblock.fs_dblkno = sblock.fs_iblkno + sblock.fs_ipg / INOPF(&sblock);
	i = MIN(~sblock.fs_cgmask, sblock.fs_ncg - 1);
	if (cgdmin(&sblock, i) - cgbase(&sblock, i) >= sblock.fs_fpg) {
		printf("inode blocks/cyl group (%ld) >= data blocks (%d)\n",
		    cgdmin(&sblock, i) - cgbase(&sblock, i) / sblock.fs_frag,
		    sblock.fs_fpg / sblock.fs_frag);
		printf("number of cylinders per cylinder group (%d) %s.\n",
		    sblock.fs_cpg, "must be increased");
		exit(29);
	}
	j = sblock.fs_ncg - 1;
	if ((i = sblock.fs_size - j * sblock.fs_fpg) < sblock.fs_fpg &&
	    cgdmin(&sblock, j) - cgbase(&sblock, j) > i) {
		if (j == 0) {
			errx(30, "filesystem must have at least %d sectors",
			     NSPF(&sblock) *
			    (cgdmin(&sblock, 0) + 3 * sblock.fs_frag));
		}
		printf("Warning: inode blocks/cyl group (%ld) >= data blocks (%ld) in last\n",
		    (cgdmin(&sblock, j) - cgbase(&sblock, j)) / sblock.fs_frag,
		    i / sblock.fs_frag);
		printf("    cylinder group. This implies %ld sector(s) cannot be allocated.\n",
		    i * NSPF(&sblock));
		sblock.fs_ncg--;
		sblock.fs_ncyl -= sblock.fs_ncyl % sblock.fs_cpg;
		sblock.fs_size = sblock.fs_ncyl * sblock.fs_spc / NSPF(&sblock);
		fssize = fsbtodb(&sblock, sblock.fs_size);
		warn = 0;
	}
	if (!quiet && warn && !mfs) {
		printf("Warning: %d sector(s) in last cylinder unallocated\n",
		    sblock.fs_spc -
		    (dbtofsb(&sblock, fssize) * NSPF(&sblock) -
		    (sblock.fs_ncyl - 1) * sblock.fs_spc));
	}
	/*
	 * fill in remaining fields of the super block
	 */
	sblock.fs_csaddr = cgdmin(&sblock, 0);
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof(struct csum));

	/*
	 * The superblock fields 'fs_csmask' and 'fs_csshift' are no
	 * longer used. However, we still initialise them so that the
	 * filesystem remains compatible with old kernels.
	 */
	i = sblock.fs_bsize / sizeof(struct csum);
	sblock.fs_csmask = ~(i - 1);
	for (sblock.fs_csshift = 0; i > 1; i >>= 1)
		sblock.fs_csshift++;
	if ((fscs = calloc(1, sblock.fs_cssize)) == NULL)
		err(1, "cg summary");
	sblock.fs_magic = FS_MAGIC;
	sblock.fs_rotdelay = rotdelay;
	sblock.fs_minfree = minfree;
	sblock.fs_maxcontig = maxcontig;
	sblock.fs_maxbpg = maxbpg;
	sblock.fs_rps = rpm / 60;
	sblock.fs_optim = opt;
	sblock.fs_cgrotor = 0;
	sblock.fs_cstotal.cs_ndir = 0;
	sblock.fs_cstotal.cs_nbfree = 0;
	sblock.fs_cstotal.cs_nifree = 0;
	sblock.fs_cstotal.cs_nffree = 0;
	sblock.fs_fmod = 0;
	sblock.fs_ronly = 0;
	sblock.fs_clean = FS_ISCLEAN;
	sblock.fs_id[0] = (u_int32_t)utime;
	sblock.fs_id[1] = (u_int32_t)arc4random();

	/*
	 * Dump out summary information about file system.
	 */
	if (!mfs) {
		printf("%s:\t%d sectors in %d %s of %d tracks, %d sectors\n",
		    fsys, sblock.fs_size * NSPF(&sblock), sblock.fs_ncyl,
		    "cylinders", sblock.fs_ntrak, sblock.fs_nsect);
#define B2MBFACTOR (1 / (1024.0 * 1024.0))
		printf("\t%.1fMB in %d cyl groups (%d c/g, %.2fMB/g, %d i/g)\n",
		    (float)sblock.fs_size * sblock.fs_fsize * B2MBFACTOR,
		    sblock.fs_ncg, sblock.fs_cpg,
		    (float)sblock.fs_fpg * sblock.fs_fsize * B2MBFACTOR,
		    sblock.fs_ipg);
#undef B2MBFACTOR
	}
	/*
	 * Now build the cylinders group blocks and
	 * then print out indices of cylinder groups.
	 */
	if (!quiet)
		printf("super-block backups (for fsck -b #) at:\n");
#ifndef STANDALONE
	else if (!mfs && isatty(STDIN_FILENO)) {
		signal(SIGINFO, siginfo);
		cur_fsys = fsys;
	}
#endif
	i = 0;
	width = charsperline();
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++) {
		cur_cylno = (sig_atomic_t)cylno;
		initcg(cylno, utime);
		if (quiet)
			continue;
		j = snprintf(tmpbuf, sizeof tmpbuf, " %ld,",
		    fsbtodb(&sblock, cgsblock(&sblock, cylno)));
		if (i+j >= width) {
			printf("\n");
			i = 0;
		}
		i += j;
		printf("%s", tmpbuf);
		fflush(stdout);
	}
	if (!quiet)
		printf("\n");
	if (Nflag && !mfs)
		exit(0);
	/*
	 * Now construct the initial file system,
	 * then write out the super-block.
	 */
	fsinit(utime, mfsmode, mfsuid, mfsgid);
	sblock.fs_time = utime;
	/* don't write magic until we are done */
	sblock.fs_magic = 0;
	wtfs((int)SBOFF / sectorsize, sbsize, (char *)&sblock);
	sblock.fs_magic = FS_MAGIC;
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize)
		wtfs(fsbtodb(&sblock, sblock.fs_csaddr + numfrags(&sblock, i)),
			sblock.fs_cssize - i < sblock.fs_bsize ?
			    sblock.fs_cssize - i : sblock.fs_bsize,
			((char *)fscs) + i);
	/*
	 * Write out the duplicate super blocks
	 */
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
		wtfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    sbsize, (char *)&sblock);
	/* done, can write with magic now */
	wtfs((int)SBOFF / sectorsize, sbsize, (char *)&sblock);
	/*
	 * Update information about this partion in pack
	 * label, to that it may be updated on disk.
	 */
	pp->p_fstype = FS_BSDFFS;
	pp->p_fsize = sblock.fs_fsize;
	pp->p_frag = sblock.fs_frag;
	pp->p_cpg = sblock.fs_cpg;
}

/*
 * Initialize a cylinder group.
 */
void
initcg(int cylno, time_t utime)
{
	daddr_t cbase, d, dlower, dupper, dmax, blkno;
	long i, j;
	struct csum *cs;

	/*
	 * Determine block bounds for cylinder group.
	 * Allow space for super block summary information in first
	 * cylinder group.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0)
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	cs = fscs + cylno;
	memset(&acg, 0, sblock.fs_cgsize);
	acg.cg_time = utime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	if (cylno == sblock.fs_ncg - 1)
		acg.cg_ncyl = sblock.fs_ncyl % sblock.fs_cpg;
	else
		acg.cg_ncyl = sblock.fs_cpg;
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_ndblk = dmax - cbase;
	if (sblock.fs_contigsumsize > 0)
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	acg.cg_btotoff = &acg.cg_space[0] - (u_char *)(&acg.cg_firstfield);
	acg.cg_boff = acg.cg_btotoff + sblock.fs_cpg * sizeof(int32_t);
	acg.cg_iusedoff = acg.cg_boff +
		sblock.fs_cpg * sblock.fs_nrpos * sizeof(int16_t);
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, NBBY);
	if (sblock.fs_contigsumsize <= 0) {
		acg.cg_nextfreeoff = acg.cg_freeoff +
		   howmany(sblock.fs_cpg * sblock.fs_spc / NSPF(&sblock), NBBY);
	} else {
		acg.cg_clustersumoff = acg.cg_freeoff + howmany
		    (sblock.fs_cpg * sblock.fs_spc / NSPF(&sblock), NBBY) -
		    sizeof(int32_t);
		acg.cg_clustersumoff =
		    roundup(acg.cg_clustersumoff, sizeof(int32_t));
		acg.cg_clusteroff = acg.cg_clustersumoff +
		    (sblock.fs_contigsumsize + 1) * sizeof(int32_t);
		acg.cg_nextfreeoff = acg.cg_clusteroff + howmany
		    (sblock.fs_cpg * sblock.fs_spc / NSPB(&sblock), NBBY);
	}
	if (acg.cg_nextfreeoff > sblock.fs_cgsize)
		errx(37, "panic: cylinder group too big");
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; i < ROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	for (i = 0; i < sblock.fs_ipg / INOPF(&sblock); i += sblock.fs_frag) {
		for (j = 0; j < sblock.fs_bsize / sizeof(struct ufs1_dinode); j++)
			zino[j].di_gen = (u_int32_t)arc4random();
		wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
		    sblock.fs_bsize, (char *)zino);
	}
	if (cylno > 0) {
		/*
		 * In cylno 0, beginning space is reserved
		 * for boot and super blocks.
		 */
		for (d = 0; d < dlower; d += sblock.fs_frag) {
			blkno = d / sblock.fs_frag;
			setblock(&sblock, cg_blksfree(&acg), blkno);
			if (sblock.fs_contigsumsize > 0)
				setbit(cg_clustersfree(&acg), blkno);
			acg.cg_cs.cs_nbfree++;
			cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
			cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
			    [cbtorpos(&sblock, d)]++;
		}
		sblock.fs_dsize += dlower;
	}
	sblock.fs_dsize += acg.cg_ndblk - dupper;
	if ((i = dupper % sblock.fs_frag)) {
		acg.cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(&acg), dupper);
			acg.cg_cs.cs_nffree++;
		}
	}
	for (d = dupper; d + sblock.fs_frag <= dmax - cbase; ) {
		blkno = d / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), blkno);
		if (sblock.fs_contigsumsize > 0)
			setbit(cg_clustersfree(&acg), blkno);
		acg.cg_cs.cs_nbfree++;
		cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
		cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
		    [cbtorpos(&sblock, d)]++;
		d += sblock.fs_frag;
	}
	if (d < dmax - cbase) {
		acg.cg_frsum[dmax - cbase - d]++;
		for (; d < dmax - cbase; d++) {
			setbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree++;
		}
	}
	if (sblock.fs_contigsumsize > 0) {
		int32_t *sump = cg_clustersum(&acg);
		u_char *mapp = cg_clustersfree(&acg);
		int map = *mapp++;
		int bit = 1;
		int run = 0;

		for (i = 0; i < acg.cg_nclusterblks; i++) {
			if ((map & bit) != 0) {
				run++;
			} else if (run != 0) {
				if (run > sblock.fs_contigsumsize)
					run = sblock.fs_contigsumsize;
				sump[run]++;
				run = 0;
			}
			if ((i & (NBBY - 1)) != (NBBY - 1)) {
				bit <<= 1;
			} else {
				map = *mapp++;
				bit = 1;
			}
		}
		if (run != 0) {
			if (run > sblock.fs_contigsumsize)
				run = sblock.fs_contigsumsize;
			sump[run]++;
		}
	}
	sblock.fs_cstotal.cs_ndir += acg.cg_cs.cs_ndir;
	sblock.fs_cstotal.cs_nffree += acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree += acg.cg_cs.cs_nbfree;
	sblock.fs_cstotal.cs_nifree += acg.cg_cs.cs_nifree;
	*cs = acg.cg_cs;
	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
	    sblock.fs_bsize, (char *)&acg);
}

/*
 * initialize the file system
 */
struct ufs1_dinode node;

#ifdef LOSTDIR
#define PREDEFDIR 3
#else
#define PREDEFDIR 2
#endif

struct direct root_dir[] = {
	{ ROOTINO, sizeof(struct direct), DT_DIR, 1, "." },
	{ ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
#ifdef LOSTDIR
	{ LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 10, "lost+found" },
#endif
};
struct odirect {
	u_int32_t d_ino;
	u_int16_t d_reclen;
	u_int16_t d_namlen;
	u_char	d_name[MAXNAMLEN + 1];
} oroot_dir[] = {
	{ ROOTINO, sizeof(struct direct), 1, "." },
	{ ROOTINO, sizeof(struct direct), 2, ".." },
#ifdef LOSTDIR
	{ LOSTFOUNDINO, sizeof(struct direct), 10, "lost+found" },
#endif
};
#ifdef LOSTDIR
struct direct lost_found_dir[] = {
	{ LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 1, "." },
	{ ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
	{ 0, DIRBLKSIZ, 0, 0, 0 },
};
struct odirect olost_found_dir[] = {
	{ LOSTFOUNDINO, sizeof(struct direct), 1, "." },
	{ ROOTINO, sizeof(struct direct), 2, ".." },
	{ 0, DIRBLKSIZ, 0, 0 },
};
#endif

void
fsinit(time_t utime, mode_t mfsmode, uid_t mfsuid, gid_t mfsgid)
{
	/*
	 * initialize the node
	 */
	node.di_atime = utime;
	node.di_mtime = utime;
	node.di_ctime = utime;
#ifdef LOSTDIR
	/*
	 * create the lost+found directory
	 */
	if (Oflag) {
		int i;

		(void)makedir((struct direct *)olost_found_dir, 2);
		for (i = DIRBLKSIZ; i < sblock.fs_bsize; i += DIRBLKSIZ)
			memcpy(&buf[i], &olost_found_dir[2],
			    DIRSIZ(0, &olost_found_dir[2]));
	} else {
		int i;

		(void)makedir(lost_found_dir, 2);
		for (i = DIRBLKSIZ; i < sblock.fs_bsize; i += DIRBLKSIZ)
			memcpy(&buf[i], &lost_found_dir[2],
			    DIRSIZ(0, &lost_found_dir[2]));
	}
	node.di_mode = IFDIR | 1700;
	node.di_nlink = 2;
	node.di_size = sblock.fs_bsize;
	node.di_db[0] = alloc(node.di_size, node.di_mode);
	node.di_blocks = btodb(fragroundup(&sblock, node.di_size));
	wtfs(fsbtodb(&sblock, node.di_db[0]), node.di_size, buf);
	iput(&node, LOSTFOUNDINO);
#endif
	/*
	 * create the root directory
	 */
	if (mfs) {
		node.di_mode = IFDIR | mfsmode;
		node.di_uid = mfsuid;
		node.di_gid = mfsgid;
	} else {
		node.di_mode = IFDIR | UMASK;
		node.di_uid = geteuid();
		node.di_gid = getegid();
	}
	node.di_nlink = PREDEFDIR;
	if (Oflag)
		node.di_size = makedir((struct direct *)oroot_dir, PREDEFDIR);
	else
		node.di_size = makedir(root_dir, PREDEFDIR);
	node.di_db[0] = alloc(sblock.fs_fsize, node.di_mode);
	node.di_blocks = btodb(fragroundup(&sblock, node.di_size));
	wtfs(fsbtodb(&sblock, node.di_db[0]), sblock.fs_fsize, buf);
	iput(&node, ROOTINO);
}

/*
 * construct a set of directory entries in "buf".
 * return size of directory.
 */
int
makedir(struct direct *protodir, int entries)
{
	char *cp;
	int i, spcleft;

	spcleft = DIRBLKSIZ;
	for (cp = buf, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(0, &protodir[i]);
		memcpy(cp, &protodir[i], protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		spcleft -= protodir[i].d_reclen;
	}
	protodir[i].d_reclen = spcleft;
	memcpy(cp, &protodir[i], DIRSIZ(0, &protodir[i]));
	return (DIRBLKSIZ);
}

/*
 * allocate a block or frag
 */
daddr_t
alloc(int size, int mode)
{
	int i, frag;
	daddr_t d, blkno;

	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		printf("cg 0: bad magic number\n");
		return (0);
	}
	if (acg.cg_cs.cs_nbfree == 0) {
		printf("first cylinder group ran out of space\n");
		return (0);
	}
	for (d = 0; d < acg.cg_ndblk; d += sblock.fs_frag)
		if (isblock(&sblock, cg_blksfree(&acg), d / sblock.fs_frag))
			goto goth;
	printf("internal error: can't find block in cyl 0\n");
	return (0);
goth:
	blkno = fragstoblks(&sblock, d);
	clrblock(&sblock, cg_blksfree(&acg), blkno);
	if (sblock.fs_contigsumsize > 0)
		clrbit(cg_clustersfree(&acg), blkno);
	acg.cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	fscs[0].cs_nbfree--;
	if (mode & IFDIR) {
		acg.cg_cs.cs_ndir++;
		sblock.fs_cstotal.cs_ndir++;
		fscs[0].cs_ndir++;
	}
	cg_blktot(&acg)[cbtocylno(&sblock, d)]--;
	cg_blks(&sblock, &acg, cbtocylno(&sblock, d))[cbtorpos(&sblock, d)]--;
	if (size != sblock.fs_bsize) {
		frag = howmany(size, sblock.fs_fsize);
		fscs[0].cs_nffree += sblock.fs_frag - frag;
		sblock.fs_cstotal.cs_nffree += sblock.fs_frag - frag;
		acg.cg_cs.cs_nffree += sblock.fs_frag - frag;
		acg.cg_frsum[sblock.fs_frag - frag]++;
		for (i = frag; i < sblock.fs_frag; i++)
			setbit(cg_blksfree(&acg), d + i);
	}
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	return (d);
}

/*
 * Allocate an inode on the disk
 */
void
iput(struct ufs1_dinode *ip, ino_t ino)
{
	struct ufs1_dinode buf[MAXINOPB];
	daddr_t d;

	ip->di_gen = (u_int32_t)arc4random();
	rdfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	if (acg.cg_magic != CG_MAGIC) {
		errx(41, "cg 0: bad magic number (0x%06x != 0x%06x)",
		     acg.cg_magic, CG_MAGIC);
	}
	acg.cg_cs.cs_nifree--;
	setbit(cg_inosused(&acg), ino);
	wtfs(fsbtodb(&sblock, cgtod(&sblock, 0)), sblock.fs_cgsize,
	    (char *)&acg);
	sblock.fs_cstotal.cs_nifree--;
	fscs[0].cs_nifree--;
	if (ino >= sblock.fs_ipg * sblock.fs_ncg) {
		errx(32, "fsinit: inode value %d out of range", ino);
	}
	d = fsbtodb(&sblock, ino_to_fsba(&sblock, ino));
	rdfs(d, sblock.fs_bsize, buf);
	buf[ino_to_fsbo(&sblock, ino)] = *ip;
	wtfs(d, sblock.fs_bsize, buf);
}

/*
 * read a block from the file system
 */
void
rdfs(daddr_t bno, int size, void *bf)
{
	int n;

	if (mfs) {
		memcpy(bf, membase + bno * sectorsize, size);
		return;
	}
	n = pread(fsi, bf, size, (off_t)bno * sectorsize);
	if (n != size) {
		err(34, "rdfs: read error on block %d", bno);
	}
}

/*
 * write a block to the file system
 */
void
wtfs(daddr_t bno, int size, void *bf)
{
	int n;

	if (mfs) {
		memcpy(membase + bno * sectorsize, bf, size);
		return;
	}
	if (Nflag)
		return;
	n = pwrite(fso, bf, size, (off_t)bno * sectorsize);
	if (n != size) {
		err(36, "wtfs: write error on block %d", bno);
	}
}

/*
 * check if a block is available
 */
int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	unsigned char mask;

	switch (fs->fs_frag) {
	case 8:
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		return ((cp[h >> 3] & mask) == mask);
	default:
#ifdef STANDALONE
		printf("isblock bad fs_frag %d\n", fs->fs_frag);
#else
		fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
#endif
		return (0);
	}
}

/*
 * take a block out of the map
 */
void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		return;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		return;
	default:
#ifdef STANDALONE
		printf("clrblock bad fs_frag %d\n", fs->fs_frag);
#else
		fprintf(stderr, "clrblock bad fs_frag %d\n", fs->fs_frag);
#endif
		return;
	}
}

/*
 * put a block into the map
 */
void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		return;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		return;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		return;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		return;
	default:
#ifdef STANDALONE
		printf("setblock bad fs_frag %d\n", fs->fs_frag);
#else
		fprintf(stderr, "setblock bad fs_frag %d\n", fs->fs_frag);
#endif
		return;
	}
}

/*
 * Determine the number of characters in a
 * single line.
 */
static int
charsperline(void)
{
	int columns;
	char *cp;
	struct winsize ws;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1)
		columns = ws.ws_col;
	if (columns == 0 && (cp = getenv("COLUMNS")))
		columns = atoi(cp);
	if (columns == 0)
		columns = 80;   /* last resort */
	return columns;
}
