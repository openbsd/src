/*	$NetBSD: setup.c,v 1.1 1997/06/11 11:22:01 bouyer Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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
static char sccsid[] = "@(#)setup.c	8.5 (Berkeley) 11/23/94";
#else
static char rcsid[] = "$NetBSD: setup.c,v 1.1 1997/06/11 11:22:01 bouyer Exp $";
#endif
#endif /* not lint */

#define DKTYPENAMES
#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext2fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/file.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fsck.h"
#include "extern.h"
#include "fsutil.h"

struct bufarea asblk;
#define altsblock (*asblk.b_un.b_fs)
#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

void badsb __P((int, char *));
/* int calcsb __P((char *, int, struct m_ext2fs *)); */
static struct disklabel *getdisklabel __P((char *, int));
static int readsb __P((int));

int
setup(dev)
	char *dev;
{
	long cg, size, asked, i, j;
	long bmapsize;
	struct disklabel *lp;
	off_t sizepb;
	struct stat statb;
	struct m_ext2fs proto;
	int doskipclean;
	u_int64_t maxfilesize;

	havesb = 0;
	fswritefd = -1;
	doskipclean = skipclean;
	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (!S_ISCHR(statb.st_mode)) {
		pfatal("%s is not a character device", dev);
		if (reply("CONTINUE") == 0)
			return (0);
	}
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (preen == 0)
		printf("** %s", dev);
	if (nflag || (fswritefd = open(dev, O_WRONLY)) < 0) {
		fswritefd = -1;
		if (preen)
			pfatal("NO WRITE ACCESS");
		printf(" (NO WRITE)");
	}
	if (preen == 0)
		printf("\n");
	fsmodified = 0;
	lfdir = 0;
	initbarea(&sblk);
	initbarea(&asblk);
	sblk.b_un.b_buf = malloc(SBSIZE);
	asblk.b_un.b_buf = malloc(SBSIZE);
	if (sblk.b_un.b_buf == NULL || asblk.b_un.b_buf == NULL)
		errexit("cannot allocate space for superblock\n");
	if ((lp = getdisklabel((char *)NULL, fsreadfd)) != NULL)
		dev_bsize = secsize = lp->d_secsize;
	else
		dev_bsize = secsize = DEV_BSIZE;
	/*
	 * Read in the superblock, looking for alternates if necessary
	 */
	if (readsb(1) == 0) {
		if (bflag || preen || calcsb(dev, fsreadfd, &proto) == 0)
			return(0);
		if (reply("LOOK FOR ALTERNATE SUPERBLOCKS") == 0)
			return (0);
		for (cg = 1; cg < proto.e2fs_ncg; cg++) {
			bflag = fsbtodb(&proto,
				cg * proto.e2fs.e2fs_bpg + proto.e2fs.e2fs_first_dblock);
			if (readsb(0) != 0)
				break;
		}
		if (cg >= proto.e2fs_ncg) {
			printf("%s %s\n%s %s\n%s %s\n",
				"SEARCH FOR ALTERNATE SUPER-BLOCK",
				"FAILED. YOU MUST USE THE",
				"-b OPTION TO FSCK_FFS TO SPECIFY THE",
				"LOCATION OF AN ALTERNATE",
				"SUPER-BLOCK TO SUPPLY NEEDED",
				"INFORMATION; SEE fsck_ext2fs(8).");
			return(0);
		}
		doskipclean = 0;
		pwarn("USING ALTERNATE SUPERBLOCK AT %d\n", bflag);
	}
	if (debug)
		printf("state = %d\n", sblock.e2fs.e2fs_state);
	if (sblock.e2fs.e2fs_state == E2FS_ISCLEAN) {
		if (doskipclean) {
			pwarn("%sile system is clean; not checking\n",
				preen ? "f" : "** F");
			return (-1);
		}
		if (!preen)
			pwarn("** File system is already clean\n");
	}
	maxfsblock = sblock.e2fs.e2fs_bcount;
	maxino = sblock.e2fs_ncg * sblock.e2fs.e2fs_ipg;
	sizepb = sblock.e2fs_bsize;
	maxfilesize = sblock.e2fs_bsize * NDADDR - 1;
	for (i = 0; i < NIADDR; i++) {
		sizepb *= NINDIR(&sblock);
		maxfilesize += sizepb;
	}
	/*
	 * Check and potentially fix certain fields in the super block.
	 */
	if ((sblock.e2fs.e2fs_rbcount < 0) ||
			(sblock.e2fs.e2fs_rbcount > sblock.e2fs.e2fs_bcount)) {
		pfatal("IMPOSSIBLE RESERVED BLOCK COUNT=%d IN SUPERBLOCK",
			sblock.e2fs.e2fs_rbcount);
		if (reply("SET TO DEFAULT") == 1) {
			sblock.e2fs.e2fs_rbcount = sblock.e2fs.e2fs_bcount * 0.1;
			sbdirty();
		}
	}
	if (sblock.e2fs.e2fs_bpg != sblock.e2fs.e2fs_fpg) {
		pfatal("WRONG FPG=%d (BPG=%d) IN SUPERBLOCK",
			sblock.e2fs.e2fs_fpg, sblock.e2fs.e2fs_bpg);
		return 0;
	}
	if (asblk.b_dirty && !bflag) {
		memcpy(&altsblock, &sblock, (size_t)SBSIZE);
		flush(fswritefd, &asblk);
	}
	/*
	 * read in the summary info.
	 */

	sblock.e2fs_gd = malloc(sblock.e2fs_ngdb * sblock.e2fs_bsize);
	asked = 0;
	for (i=0; i < sblock.e2fs_ngdb; i++) {
		if (bread(fsreadfd,(char *)
			&sblock.e2fs_gd[i* sblock.e2fs_bsize / sizeof(struct ext2_gd)],
			fsbtodb(&sblock, ((sblock.e2fs_bsize>1024)?0:1)+i+1),
			sblock.e2fs_bsize) != 0 && !asked) {
			pfatal("BAD SUMMARY INFORMATION");
			if (reply("CONTINUE") == 0)
				errexit("%s\n", "");
			asked++;
		}
	}
	/*
	 * allocate and initialize the necessary maps
	 */
	bmapsize = roundup(howmany(maxfsblock, NBBY), sizeof(int16_t));
	blockmap = calloc((unsigned)bmapsize, sizeof (char));
	if (blockmap == NULL) {
		printf("cannot alloc %u bytes for blockmap\n",
			(unsigned)bmapsize);
		goto badsblabel;
	}
	statemap = calloc((unsigned)(maxino + 2), sizeof(char));
	if (statemap == NULL) {
		printf("cannot alloc %u bytes for statemap\n",
			(unsigned)(maxino + 1));
		goto badsblabel;
	}
	lncntp = (int16_t *)calloc((unsigned)(maxino + 1), sizeof(int16_t));
	if (lncntp == NULL) {
		printf("cannot alloc %u bytes for lncntp\n", 
			(unsigned)(maxino + 1) * sizeof(int16_t));
		goto badsblabel;
	}
	for (numdirs = 0, cg = 0; cg < sblock.e2fs_ncg; cg++) {
		numdirs += sblock.e2fs_gd[cg].ext2bgd_ndirs;
	}
	inplast = 0;
	listmax = numdirs + 10;
	inpsort = (struct inoinfo **)calloc((unsigned)listmax,
		sizeof(struct inoinfo *));
	inphead = (struct inoinfo **)calloc((unsigned)numdirs,
		sizeof(struct inoinfo *));
	if (inpsort == NULL || inphead == NULL) {
		printf("cannot alloc %u bytes for inphead\n", 
			(unsigned)numdirs * sizeof(struct inoinfo *));
		goto badsblabel;
	}
	bufinit();
	return (1);

badsblabel:
	ckfini(0);
	return (0);
}

/*
 * Read in the super block and its summary info.
 */
static int
readsb(listerr)
	int listerr;
{
	daddr_t super = bflag ? bflag : SBOFF / dev_bsize;

	if (bread(fsreadfd, (char *)&sblock.e2fs, super, (long)SBSIZE) != 0)
		return (0);
	sblk.b_bno = super;
	sblk.b_size = SBSIZE;
	/*
	 * run a few consistency checks of the super block
	 */
	if (sblock.e2fs.e2fs_magic != E2FS_MAGIC) {
		badsb(listerr, "MAGIC NUMBER WRONG"); return (0);
	}
	if (sblock.e2fs.e2fs_log_bsize > 2) {
		badsb(listerr, "BAD LOG_BSIZE"); return (0);
	}

	/* compute the dynamic fields of the in-memory sb */
	/* compute dynamic sb infos */
	sblock.e2fs_ncg =
		howmany(sblock.e2fs.e2fs_bcount - sblock.e2fs.e2fs_first_dblock,
		sblock.e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	sblock.e2fs_fsbtodb = sblock.e2fs.e2fs_log_bsize + 1;
	sblock.e2fs_bsize = 1024 << sblock.e2fs.e2fs_log_bsize;
	sblock.e2fs_bshift = LOG_MINBSIZE + sblock.e2fs.e2fs_log_bsize;
	sblock.e2fs_qbmask = sblock.e2fs_bsize - 1;
	sblock.e2fs_bmask = ~sblock.e2fs_qbmask;
	sblock.e2fs_ngdb = howmany(sblock.e2fs_ncg,
		sblock.e2fs_bsize / sizeof(struct ext2_gd));
	sblock.e2fs_ipb = sblock.e2fs_bsize / sizeof(struct ext2fs_dinode);
	sblock.e2fs_itpg = sblock.e2fs.e2fs_ipg/sblock.e2fs_ipb;

	cgoverhead =	1 /* super block */ +
					sblock.e2fs_ngdb +
					1 /* block bitmap */ +
					1 /* inode bitmap */ +
					sblock.e2fs_itpg;

	if (debug) /* DDD */
		printf("cg overhead %d blocks \n", cgoverhead);
	/*
	 * Compute block size that the filesystem is based on,
	 * according to fsbtodb, and adjust superblock block number
	 * so we can tell if this is an alternate later.
	 */
	super *= dev_bsize;
	dev_bsize = sblock.e2fs_bsize / fsbtodb(&sblock, 1);
	sblk.b_bno = super / dev_bsize;
	if (bflag) {
		havesb = 1;
		return (1);
	}

	/*
	 * Set all possible fields that could differ, then do check
	 * of whole super block against an alternate super block.
	 * When an alternate super-block is specified this check is skipped.
	 */
	getblk(&asblk, 1 * sblock.e2fs.e2fs_bpg + sblock.e2fs.e2fs_first_dblock,
		(long)SBSIZE);
	if (asblk.b_errs)
		return (0);
	altsblock.e2fs.e2fs_rbcount = sblock.e2fs.e2fs_rbcount;
	altsblock.e2fs.e2fs_fbcount = sblock.e2fs.e2fs_fbcount;
	altsblock.e2fs.e2fs_ficount = sblock.e2fs.e2fs_ficount;
	altsblock.e2fs.e2fs_mtime = sblock.e2fs.e2fs_mtime;
	altsblock.e2fs.e2fs_wtime = sblock.e2fs.e2fs_wtime;
	altsblock.e2fs.e2fs_mnt_count = sblock.e2fs.e2fs_mnt_count;
	altsblock.e2fs.e2fs_max_mnt_count = sblock.e2fs.e2fs_max_mnt_count;
	altsblock.e2fs.e2fs_state = sblock.e2fs.e2fs_state;
	altsblock.e2fs.e2fs_beh = sblock.e2fs.e2fs_beh;
	altsblock.e2fs.e2fs_lastfsck = sblock.e2fs.e2fs_lastfsck;
	altsblock.e2fs.e2fs_fsckintv = sblock.e2fs.e2fs_fsckintv;
	altsblock.e2fs.e2fs_ruid = sblock.e2fs.e2fs_ruid;
	altsblock.e2fs.e2fs_rgid = sblock.e2fs.e2fs_rgid;
	if (memcmp(&(sblock.e2fs), &(altsblock.e2fs), (int)SBSIZE)) {
		if (debug) {
			long *nlp, *olp, *endlp;

			printf("superblock mismatches\n");
			nlp = (long *)&altsblock;
			olp = (long *)&sblock;
			endlp = olp + (SBSIZE / sizeof *olp);
			for ( ; olp < endlp; olp++, nlp++) {
				if (*olp == *nlp)
					continue;
				printf("offset %d, original %ld, alternate %ld\n",
					olp - (long *)&sblock, *olp, *nlp);
			}
		}
		badsb(listerr,
		"VALUES IN SUPER BLOCK DISAGREE WITH THOSE IN FIRST ALTERNATE");
		return (0);
	}
	havesb = 1;
	return (1);
}

void
badsb(listerr, s)
	int listerr;
	char *s;
{

	if (!listerr)
		return;
	if (preen)
		printf("%s: ", cdevname());
	pfatal("BAD SUPER BLOCK: %s\n", s);
}

/*
 * Calculate a prototype superblock based on information in the disk label.
 * When done the cgsblock macro can be calculated and the fs_ncg field
 * can be used. Do NOT attempt to use other macros without verifying that
 * their needed information is available!
 */

int
calcsb(dev, devfd, fs)
	char *dev;
	int devfd;
	register struct m_ext2fs *fs;
{
	register struct disklabel *lp;
	register struct partition *pp;
	register char *cp;
	int i;

	cp = strchr(dev, '\0') - 1;
	if ((cp == (char *)-1 || (*cp < 'a' || *cp > 'h')) && !isdigit(*cp)) {
		pfatal("%s: CANNOT FIGURE OUT FILE SYSTEM PARTITION\n", dev);
		return (0);
	}
	lp = getdisklabel(dev, devfd);
	if (isdigit(*cp))
		pp = &lp->d_partitions[0];
	else
		pp = &lp->d_partitions[*cp - 'a'];
	if (pp->p_fstype != FS_BSDFFS) {
		pfatal("%s: NOT LABELED AS A BSD FILE SYSTEM (%s)\n",
			dev, pp->p_fstype < FSMAXTYPES ?
			fstypenames[pp->p_fstype] : "unknown");
		return (0);
	}
	memset(fs, 0, sizeof(struct m_ext2fs));
	fs->e2fs_bsize = 1024; /* XXX to look for altenate SP */
	fs->e2fs.e2fs_log_bsize = 0;
	fs->e2fs.e2fs_bcount = (pp->p_size * DEV_BSIZE) / fs->e2fs_bsize;
	fs->e2fs.e2fs_first_dblock = 1;
	fs->e2fs.e2fs_bpg = fs->e2fs_bsize * NBBY;
	fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs.e2fs_log_bsize;
	fs->e2fs_qbmask = fs->e2fs_bsize - 1;
	fs->e2fs_bmask = ~fs->e2fs_qbmask;
	fs->e2fs_ncg =
		howmany(fs->e2fs.e2fs_bcount - fs->e2fs.e2fs_first_dblock,
		fs->e2fs.e2fs_bpg);
	fs->e2fs_fsbtodb = fs->e2fs.e2fs_log_bsize + 1;
	fs->e2fs_ngdb = howmany(fs->e2fs_ncg,
		fs->e2fs_bsize / sizeof(struct ext2_gd));



	return (1);
}

static struct disklabel *
getdisklabel(s, fd)
	char *s;
	int	fd;
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *)&lab) < 0) {
		if (s == NULL)
			return ((struct disklabel *)NULL);
		pwarn("ioctl (GCINFO): %s\n", strerror(errno));
		errexit("%s: can't read disk label\n", s);
	}
	return (&lab);
}
