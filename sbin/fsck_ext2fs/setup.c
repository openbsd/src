/*	$OpenBSD: setup.c,v 1.17 2010/05/18 04:41:14 dlg Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

#define DKTYPENAMES
#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext2fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/disklabel.h>
#include <sys/file.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fsck.h"
#include "extern.h"
#include "fsutil.h"

#define POWEROF2(num)	(((num) & ((num) - 1)) == 0)

void badsb(int, char *);
int calcsb(char *, int, struct m_ext2fs *);
static struct disklabel *getdisklabel(char *, int);
static int readsb(int);

int
setup(char *dev)
{
	long cg, asked, i;
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
			dirty(&asblk);
		}
	}
	if (sblock.e2fs.e2fs_bpg != sblock.e2fs.e2fs_fpg) {
		pfatal("WRONG FPG=%d (BPG=%d) IN SUPERBLOCK",
			sblock.e2fs.e2fs_fpg, sblock.e2fs.e2fs_bpg);
		return 0;
	}
	if (asblk.b_dirty && !bflag) {
		copyback_sb(&asblk);
		flush(fswritefd, &asblk);
	}
	/*
	 * read in the summary info.
	 */

	sblock.e2fs_gd = calloc(sblock.e2fs_ngdb, sblock.e2fs_bsize);
	if (sblock.e2fs_gd == NULL)
		errexit("out of memory\n");
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
	typemap = calloc((unsigned)(maxino + 1), sizeof(char));
	if (typemap == NULL) {
		printf("cannot alloc %u bytes for typemap\n",
		    (unsigned)(maxino + 1));
		goto badsblabel;
	}
	lncntp = (int16_t *)calloc((unsigned)(maxino + 1), sizeof(int16_t));
	if (lncntp == NULL) {
		printf("cannot alloc %u bytes for lncntp\n",
			(unsigned)((maxino + 1) * sizeof(int16_t)));
		goto badsblabel;
	}
	for (numdirs = 0, cg = 0; cg < sblock.e2fs_ncg; cg++) {
		numdirs += fs2h16(sblock.e2fs_gd[cg].ext2bgd_ndirs);
	}
	inplast = 0;
	listmax = numdirs + 10;
	inpsort = (struct inoinfo **)calloc((unsigned)listmax,
		sizeof(struct inoinfo *));
	inphead = (struct inoinfo **)calloc((unsigned)numdirs,
		sizeof(struct inoinfo *));
	if (inpsort == NULL || inphead == NULL) {
		printf("cannot alloc %u bytes for inphead\n",
			(unsigned)(numdirs * sizeof(struct inoinfo *)));
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
readsb(int listerr)
{
	daddr_t super = bflag ? bflag : SBOFF / dev_bsize;

	if (bread(fsreadfd, (char *)sblk.b_un.b_fs, super, (long)SBSIZE) != 0)
		return (0);
	sblk.b_bno = super;
	sblk.b_size = SBSIZE;

	/* Copy the superblock in memory */
	e2fs_sbload(sblk.b_un.b_fs, &sblock.e2fs);

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

	/*
	 * Compute block size that the filesystem is based on,
	 * according to fsbtodb, and adjust superblock block number
	 * so we can tell if this is an alternate later.
	 */
	super *= dev_bsize;
	dev_bsize = sblock.e2fs_bsize / fsbtodb(&sblock, 1);
	sblk.b_bno = super / dev_bsize;

	if (sblock.e2fs_ncg == 1) {
		/* no alternate superblock; assume it's okey */
		havesb = 1;
		return 1;
	}

	getblk(&asblk, 1 * sblock.e2fs.e2fs_bpg + sblock.e2fs.e2fs_first_dblock,
		(long)SBSIZE);
	if (asblk.b_errs)
		return (0);
	if (bflag) {
		havesb = 1;
		return (1);
	}

	/*
	 * Set all possible fields that could differ, then do check
	 * of whole super block against an alternate super block.
	 * When an alternate super-block is specified this check is skipped.
	 */
	asblk.b_un.b_fs->e2fs_rbcount = sblk.b_un.b_fs->e2fs_rbcount;
	asblk.b_un.b_fs->e2fs_fbcount = sblk.b_un.b_fs->e2fs_fbcount;
	asblk.b_un.b_fs->e2fs_ficount = sblk.b_un.b_fs->e2fs_ficount;
	asblk.b_un.b_fs->e2fs_mtime = sblk.b_un.b_fs->e2fs_mtime;
	asblk.b_un.b_fs->e2fs_wtime = sblk.b_un.b_fs->e2fs_wtime;
	asblk.b_un.b_fs->e2fs_mnt_count = sblk.b_un.b_fs->e2fs_mnt_count;
	asblk.b_un.b_fs->e2fs_max_mnt_count = sblk.b_un.b_fs->e2fs_max_mnt_count;
	asblk.b_un.b_fs->e2fs_state = sblk.b_un.b_fs->e2fs_state;
	asblk.b_un.b_fs->e2fs_beh = sblk.b_un.b_fs->e2fs_beh;
	asblk.b_un.b_fs->e2fs_lastfsck = sblk.b_un.b_fs->e2fs_lastfsck;
	asblk.b_un.b_fs->e2fs_fsckintv = sblk.b_un.b_fs->e2fs_fsckintv;
	asblk.b_un.b_fs->e2fs_ruid = sblk.b_un.b_fs->e2fs_ruid;
	asblk.b_un.b_fs->e2fs_rgid = sblk.b_un.b_fs->e2fs_rgid;
	asblk.b_un.b_fs->e2fs_block_group_nr =
	    sblk.b_un.b_fs->e2fs_block_group_nr;
	asblk.b_un.b_fs->e2fs_features_rocompat &= ~EXT2F_ROCOMPAT_LARGEFILE;
	asblk.b_un.b_fs->e2fs_features_rocompat |=
	    sblk.b_un.b_fs->e2fs_features_rocompat & EXT2F_ROCOMPAT_LARGEFILE;
	if (sblock.e2fs.e2fs_rev > E2FS_REV0 &&
	    ((sblock.e2fs.e2fs_features_incompat & ~EXT2F_INCOMPAT_SUPP) ||
	    (sblock.e2fs.e2fs_features_rocompat & ~EXT2F_ROCOMPAT_SUPP))) {
		if (debug) {
			printf("compat 0x%08x, incompat 0x%08x, compat_ro "
			    "0x%08x\n",
			    sblock.e2fs.e2fs_features_compat,
			    sblock.e2fs.e2fs_features_incompat,
			    sblock.e2fs.e2fs_features_rocompat);
		}
		badsb(listerr,"INCOMPATIBLE FEATURE BITS IN SUPER BLOCK");
		return 0;
	}
	if (memcmp(sblk.b_un.b_fs, asblk.b_un.b_fs, SBSIZE)) {
		if (debug) {
			u_int32_t *nlp, *olp, *endlp;

			printf("superblock mismatches\n");
			nlp = (u_int32_t *)asblk.b_un.b_fs;
			olp = (u_int32_t *)sblk.b_un.b_fs;
			endlp = olp + (SBSIZE / sizeof *olp);
			for ( ; olp < endlp; olp++, nlp++) {
				if (*olp == *nlp)
					continue;
				printf("offset %ld, original %ld, alternate %ld\n",
					(long)(olp - (u_int32_t *)sblk.b_un.b_fs),
					(long)fs2h32(*olp),
					(long)fs2h32(*nlp));
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
copyback_sb(struct bufarea *bp)
{
	/* Copy the in-memory superblock back to buffer */
	bp->b_un.b_fs->e2fs_icount = fs2h32(sblock.e2fs.e2fs_icount);
	bp->b_un.b_fs->e2fs_bcount = fs2h32(sblock.e2fs.e2fs_bcount);
	bp->b_un.b_fs->e2fs_rbcount = fs2h32(sblock.e2fs.e2fs_rbcount);
	bp->b_un.b_fs->e2fs_fbcount = fs2h32(sblock.e2fs.e2fs_fbcount);
	bp->b_un.b_fs->e2fs_ficount = fs2h32(sblock.e2fs.e2fs_ficount);
	bp->b_un.b_fs->e2fs_first_dblock =
					fs2h32(sblock.e2fs.e2fs_first_dblock);
	bp->b_un.b_fs->e2fs_log_bsize = fs2h32(sblock.e2fs.e2fs_log_bsize);
	bp->b_un.b_fs->e2fs_fsize = fs2h32(sblock.e2fs.e2fs_fsize);
	bp->b_un.b_fs->e2fs_bpg = fs2h32(sblock.e2fs.e2fs_bpg);
	bp->b_un.b_fs->e2fs_fpg = fs2h32(sblock.e2fs.e2fs_fpg);
	bp->b_un.b_fs->e2fs_ipg = fs2h32(sblock.e2fs.e2fs_ipg);
	bp->b_un.b_fs->e2fs_mtime = fs2h32(sblock.e2fs.e2fs_mtime);
	bp->b_un.b_fs->e2fs_wtime = fs2h32(sblock.e2fs.e2fs_wtime);
	bp->b_un.b_fs->e2fs_lastfsck = fs2h32(sblock.e2fs.e2fs_lastfsck);
	bp->b_un.b_fs->e2fs_fsckintv = fs2h32(sblock.e2fs.e2fs_fsckintv);
	bp->b_un.b_fs->e2fs_creator = fs2h32(sblock.e2fs.e2fs_creator);
	bp->b_un.b_fs->e2fs_rev = fs2h32(sblock.e2fs.e2fs_rev);
	bp->b_un.b_fs->e2fs_mnt_count = fs2h16(sblock.e2fs.e2fs_mnt_count);
	bp->b_un.b_fs->e2fs_max_mnt_count =
					fs2h16(sblock.e2fs.e2fs_max_mnt_count);
	bp->b_un.b_fs->e2fs_magic = fs2h16(sblock.e2fs.e2fs_magic);
	bp->b_un.b_fs->e2fs_state = fs2h16(sblock.e2fs.e2fs_state);
	bp->b_un.b_fs->e2fs_beh = fs2h16(sblock.e2fs.e2fs_beh);
	bp->b_un.b_fs->e2fs_ruid = fs2h16(sblock.e2fs.e2fs_ruid);
	bp->b_un.b_fs->e2fs_rgid = fs2h16(sblock.e2fs.e2fs_rgid);
}

void
badsb(int listerr, char *s)
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
calcsb(char *dev, int devfd, struct m_ext2fs *fs)
{
	struct disklabel *lp;
	struct partition *pp;
	char *cp;

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
	if (pp->p_fstype != FS_EXT2FS) {
		pfatal("%s: NOT LABELED AS A EXT2 FILE SYSTEM (%s)\n",
			dev, pp->p_fstype < FSMAXTYPES ?
			fstypenames[pp->p_fstype] : "unknown");
		return (0);
	}
	memset(fs, 0, sizeof(struct m_ext2fs));
	fs->e2fs_bsize = DISKLABELV1_FFS_FSIZE(pp->p_fragblock); /* XXX */
	fs->e2fs.e2fs_log_bsize = fs->e2fs_bsize / 1024;
	fs->e2fs.e2fs_bcount = (pp->p_size * DEV_BSIZE) / fs->e2fs_bsize;
	fs->e2fs.e2fs_first_dblock = (fs->e2fs.e2fs_log_bsize == 0) ? 1 : 0;
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
getdisklabel(char *s, int fd)
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

daddr_t
cgoverhead(int c)
{
	int overh;
	overh =	1 /* block bitmap */ +
		1 /* inode bitmap */ +
		sblock.e2fs_itpg;
	if (sblock.e2fs.e2fs_rev > E2FS_REV0 &&
	    sblock.e2fs.e2fs_features_rocompat & EXT2F_ROCOMPAT_SPARSESUPER) {
		if (cg_has_sb(c) == 0)
			return overh;
	}
	overh += 1 + sblock.e2fs_ngdb;
	return overh;
}
