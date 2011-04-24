/*	$OpenBSD: pass1.c,v 1.35 2011/04/24 07:07:03 otto Exp $	*/
/*	$NetBSD: pass1.c,v 1.16 1996/09/27 22:45:15 christos Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1993
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

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fsck.h"
#include "extern.h"
#include "fsutil.h"

static daddr64_t badblk;
static daddr64_t dupblk;
static void checkinode(ino_t, struct inodesc *);

static ino_t info_inumber;

static int
pass1_info(char *buf, size_t buflen)
{
	return (snprintf(buf, buflen, "phase 1, inode %d/%d",
	    info_inumber, sblock.fs_ipg * sblock.fs_ncg) > 0);
}

void
pass1(void)
{
	ino_t inumber, inosused, ninosused;
	size_t inospace;
	struct inostat *info;
	int c;
	struct inodesc idesc;
	daddr64_t i, cgd;
	u_int8_t *cp;

	/*
	 * Set file system reserved blocks in used block map.
	 */
	for (c = 0; c < sblock.fs_ncg; c++) {
		cgd = cgdmin(&sblock, c);
		if (c == 0)
			i = cgbase(&sblock, c);
		else
			i = cgsblock(&sblock, c);
		for (; i < cgd; i++)
			setbmap(i);
	}
	i = sblock.fs_csaddr;
	cgd = i + howmany(sblock.fs_cssize, sblock.fs_fsize);
	for (; i < cgd; i++)
		setbmap(i);
	/*
	 * Find all allocated blocks.
	 */
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = pass1check;
	n_files = n_blks = 0;
	info_inumber = 0;
	info_fn = pass1_info;
	for (c = 0; c < sblock.fs_ncg; c++) {
		inumber = c * sblock.fs_ipg;
		setinodebuf(inumber);
		getblk(&cgblk, cgtod(&sblock, c), sblock.fs_cgsize);
		if (sblock.fs_magic == FS_UFS2_MAGIC) {
			inosused = cgrp.cg_initediblk;
			if (inosused > sblock.fs_ipg)
				inosused = sblock.fs_ipg;
		} else
			inosused = sblock.fs_ipg;

		/*
		 * If we are using soft updates, then we can trust the
		 * cylinder group inode allocation maps to tell us which
		 * inodes are allocated. We will scan the used inode map
		 * to find the inodes that are really in use, and then
		 * read only those inodes in from disk.
		 */
		if (preen && usedsoftdep) {
			cp = &cg_inosused(&cgrp)[(inosused - 1) / CHAR_BIT];
			for ( ; inosused > 0; inosused -= CHAR_BIT, cp--) {
				if (*cp == 0)
					continue;
				for (i = 1 << (CHAR_BIT - 1); i > 0; i >>= 1) {
					if (*cp & i)
						break;
					inosused--;
				}
				break;
			}
			if (inosused < 0)
				inosused = 0;
		}
		/*
 		 * Allocate inoinfo structures for the allocated inodes.
		 */
		inostathead[c].il_numalloced = inosused;
		if (inosused == 0) {
			inostathead[c].il_stat = 0;
			continue;
		}
		info = calloc((unsigned)inosused, sizeof(struct inostat));
		inospace = (unsigned)inosused * sizeof(struct inostat);
		if (info == NULL)
			errexit("cannot alloc %u bytes for inoinfo",
			    (unsigned)(sizeof(struct inostat) * inosused));
		inostathead[c].il_stat = info;
		/*
		 * Scan the allocated inodes.
		 */
		for (i = 0; i < inosused; i++, inumber++) {
			info_inumber = inumber;
			if (inumber < ROOTINO) {
				(void)getnextinode(inumber);
				continue;
			}
			checkinode(inumber, &idesc);
		}
		lastino += 1;
		if (inosused < sblock.fs_ipg || inumber == lastino)
			continue;
		/*
		 * If we were not able to determine in advance which inodes
		 * were in use, then reduce the size of the inoinfo structure
		 * to the size necessary to describe the inodes that we
		 * really found.
		 */
		if (lastino < (c * sblock.fs_ipg))
			ninosused = 0;
		else
			ninosused = lastino - (c * sblock.fs_ipg);
		inostathead[c].il_numalloced = ninosused;
		if (ninosused == 0) {
			free(inostathead[c].il_stat);
			inostathead[c].il_stat = 0;
			continue;
		}
		if (ninosused != inosused) {
			struct inostat *ninfo;
			size_t ninospace = ninosused * sizeof(*ninfo);
			if (ninospace / sizeof(*info) != ninosused) {
				pfatal("too many inodes %llu\n",
				    (unsigned long long)ninosused);
				exit(8);
			}
			ninfo = realloc(info, ninospace);
			if (ninfo == NULL) {
				pfatal("cannot realloc %zu bytes to %zu "
				    "for inoinfo\n", inospace, ninospace);
				exit(8);
			}
			if (ninosused > inosused)
				(void)memset(&ninfo[inosused], 0, ninospace - inospace);
			inostathead[c].il_stat = ninfo;
		}
	}
	info_fn = NULL;
	freeinodebuf();
}

static void
checkinode(ino_t inumber, struct inodesc *idesc)
{
	union dinode *dp;
	off_t kernmaxfilesize;
	struct zlncnt *zlnp;
	int ndb, j;
	mode_t mode;
	char *symbuf;
	u_int64_t lndb;

	dp = getnextinode(inumber);
	mode = DIP(dp, di_mode) & IFMT;
	if (mode == 0) {
		if ((sblock.fs_magic == FS_UFS1_MAGIC &&
		     (memcmp(dp->dp1.di_db, ufs1_zino.di_db,
			NDADDR * sizeof(int32_t)) ||
		      memcmp(dp->dp1.di_ib, ufs1_zino.di_ib,
			NIADDR * sizeof(int32_t)) ||
		      dp->dp1.di_mode || dp->dp1.di_size)) ||
		    (sblock.fs_magic == FS_UFS2_MAGIC &&
		     (memcmp(dp->dp2.di_db, ufs2_zino.di_db,
			NDADDR * sizeof(daddr64_t)) ||
		      memcmp(dp->dp2.di_ib, ufs2_zino.di_ib,
			NIADDR * sizeof(daddr64_t)) ||
		      dp->dp2.di_mode || dp->dp2.di_size))) {
			pfatal("PARTIALLY ALLOCATED INODE I=%u", inumber);
			if (reply("CLEAR") == 1) {
				dp = ginode(inumber);
				clearinode(dp);
				inodirty();
			}
		}
		SET_ISTATE(inumber, USTATE);
		return;
	}
	lastino = inumber;
	/* This should match the file size limit in ffs_mountfs(). */
	kernmaxfilesize = FS_KERNMAXFILESIZE(getpagesize(), &sblock);
	if (DIP(dp, di_size) > kernmaxfilesize ||
	    DIP(dp, di_size) > sblock.fs_maxfilesize ||
	    (mode == IFDIR && DIP(dp, di_size) > MAXDIRSIZE)) {
		if (debug)
			printf("bad size %llu:",
			    (unsigned long long)DIP(dp, di_size));
		goto unknown;
	}
	if (!preen && mode == IFMT && reply("HOLD BAD BLOCK") == 1) {
		dp = ginode(inumber);
		DIP_SET(dp, di_size, sblock.fs_fsize);
		DIP_SET(dp, di_mode, IFREG|0600);
		inodirty();
	}
	lndb = howmany(DIP(dp, di_size), sblock.fs_bsize);
	ndb = lndb > (u_int64_t)INT_MAX ? -1 : (int)lndb;
	if (ndb < 0) {
		if (debug)
			printf("bad size %llu ndb %d:",
			    (unsigned long long)DIP(dp, di_size), ndb);
		goto unknown;
	}
	if (mode == IFBLK || mode == IFCHR)
		ndb++;
	if (mode == IFLNK) {
		/*
		 * Fake ndb value so direct/indirect block checks below
		 * will detect any garbage after symlink string.
		 */
		if (DIP(dp, di_size) < sblock.fs_maxsymlinklen ||
		    (sblock.fs_maxsymlinklen == 0 && DIP(dp, di_blocks) == 0)) {
			if (sblock.fs_magic == FS_UFS1_MAGIC)
				ndb = howmany(DIP(dp, di_size),
				    sizeof(int32_t));
			else
				ndb = howmany(DIP(dp, di_size),
				    sizeof(int64_t));
			if (ndb > NDADDR) {
				j = ndb - NDADDR;
				for (ndb = 1; j > 1; j--)
					ndb *= NINDIR(&sblock);
				ndb += NDADDR;
			}
		}
	}
	for (j = ndb; j < NDADDR; j++)
		if (DIP(dp, di_db[j]) != 0) {
			if (debug)
				printf("bad direct addr: %ld\n",
				    (long)DIP(dp, di_db[j]));
			goto unknown;
		}
	for (j = 0, ndb -= NDADDR; ndb > 0; j++)
		ndb /= NINDIR(&sblock);
	for (; j < NIADDR; j++)
		if (DIP(dp, di_ib[j]) != 0) {
			if (debug)
				printf("bad indirect addr: %ld\n",
				    (long)DIP(dp, di_ib[j]));
			goto unknown;
		}
	if (ftypeok(dp) == 0)
		goto unknown;
	n_files++;
	ILNCOUNT(inumber) = DIP(dp, di_nlink);
	if (DIP(dp, di_nlink) <= 0) {
		zlnp =  malloc(sizeof *zlnp);
		if (zlnp == NULL) {
			pfatal("LINK COUNT TABLE OVERFLOW");
			if (reply("CONTINUE") == 0) {
				ckfini(0);
				errexit("%s", "");
			}
		} else {
			zlnp->zlncnt = inumber;
			zlnp->next = zlnhead;
			zlnhead = zlnp;
		}
	}
	if (mode == IFDIR) {
		if (DIP(dp, di_size) == 0)
			SET_ISTATE(inumber, DCLEAR);
		else
			SET_ISTATE(inumber, DSTATE);
		cacheino(dp, inumber);
	} else
		SET_ISTATE(inumber, FSTATE);
	SET_ITYPE(inumber, IFTODT(mode));
	badblk = dupblk = 0;
	idesc->id_number = inumber;
	(void)ckinode(dp, idesc);
	idesc->id_entryno *= btodb(sblock.fs_fsize);
	if (DIP(dp, di_blocks) != idesc->id_entryno) {
		pwarn("INCORRECT BLOCK COUNT I=%u (%ld should be %d)",
		    inumber, (long)DIP(dp, di_blocks), idesc->id_entryno);
		if (preen)
			printf(" (CORRECTED)\n");
		else if (reply("CORRECT") == 0)
			return;
		dp = ginode(inumber);
		DIP_SET(dp, di_blocks, idesc->id_entryno);
		inodirty();
	}
	return;
unknown:
	pfatal("UNKNOWN FILE TYPE I=%u", inumber);
	SET_ISTATE(inumber, FCLEAR);
	if (reply("CLEAR") == 1) {
		SET_ISTATE(inumber, USTATE);
		dp = ginode(inumber);
		clearinode(dp);
		inodirty();
	}
}

int
pass1check(struct inodesc *idesc)
{
	int res = KEEPON;
	int anyout, nfrags;
	daddr64_t blkno = idesc->id_blkno;
	struct dups *dlp;
	struct dups *new;

	if ((anyout = chkrange(blkno, idesc->id_numfrags)) != 0) {
		blkerror(idesc->id_number, "BAD", blkno);
		if (badblk++ >= MAXBAD) {
			pwarn("EXCESSIVE BAD BLKS I=%u",
				idesc->id_number);
			if (preen)
				printf(" (SKIPPING)\n");
			else if (reply("CONTINUE") == 0) {
				ckfini(0);
				errexit("%s", "");
			}
			return (STOP);
		}
	}
	for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
		if (anyout && chkrange(blkno, 1)) {
			res = SKIP;
		} else if (!testbmap(blkno)) {
			n_blks++;
			setbmap(blkno);
		} else {
			blkerror(idesc->id_number, "DUP", blkno);
			if (dupblk++ >= MAXDUP) {
				pwarn("EXCESSIVE DUP BLKS I=%u",
					idesc->id_number);
				if (preen)
					printf(" (SKIPPING)\n");
				else if (reply("CONTINUE") == 0) {
					ckfini(0);
					errexit("%s", "");
				}
				return (STOP);
			}
			new = malloc(sizeof(struct dups));
			if (new == NULL) {
				pfatal("DUP TABLE OVERFLOW.");
				if (reply("CONTINUE") == 0) {
					ckfini(0);
					errexit("%s", "");
				}
				return (STOP);
			}
			new->dup = blkno;
			if (muldup == 0) {
				duplist = muldup = new;
				new->next = 0;
			} else {
				new->next = muldup->next;
				muldup->next = new;
			}
			for (dlp = duplist; dlp != muldup; dlp = dlp->next)
				if (dlp->dup == blkno)
					break;
			if (dlp == muldup && dlp->dup != blkno)
				muldup = new;
		}
		/*
		 * count the number of blocks found in id_entryno
		 */
		idesc->id_entryno++;
	}
	return (res);
}
