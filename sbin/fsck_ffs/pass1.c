/*	$OpenBSD: pass1.c,v 1.24 2007/06/01 06:41:33 deraadt Exp $	*/
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)pass1.c	8.1 (Berkeley) 6/5/93";
#else
static const char rcsid[] = "$OpenBSD: pass1.c,v 1.24 2007/06/01 06:41:33 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsck.h"
#include "extern.h"
#include "fsutil.h"

static daddr_t badblk;
static daddr_t dupblk;
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
	struct inodesc idesc;
	ino_t inumber, inosused;
	int c, i, cgd;

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
		if (sblock.fs_magic == FS_UFS2_MAGIC)
			inosused = cgrp.cg_initediblk;
		else
			inosused = sblock.fs_ipg;
		cginosused[c] = inosused;
		for (i = 0; i < inosused; i++, inumber++) {
			info_inumber = inumber;
			if (inumber < ROOTINO)
				continue;
			checkinode(inumber, &idesc);
		}
	}
	info_fn = NULL;
	freeinodebuf();
}

static void
checkinode(ino_t inumber, struct inodesc *idesc)
{
	union dinode *dp;
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
			NDADDR * sizeof(ufs1_daddr_t)) ||
		      memcmp(dp->dp1.di_ib, ufs1_zino.di_ib,
			NIADDR * sizeof(ufs1_daddr_t)) ||
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
		statemap[inumber] = USTATE;
		return;
	}
	lastino = inumber;
	if (/* DIP(dp, di_size) < 0 || */
	    DIP(dp, di_size) + sblock.fs_bsize - 1 < DIP(dp, di_size)) {
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
		 * Note that the old fastlink format always had di_blocks set
		 * to 0.  Other than that we no longer use the `spare' field
		 * (which is now the extended uid) for sanity checking, the
		 * new format is the same as the old.  We simply ignore the
		 * conversion altogether.  - mycroft, 19MAY1994
		 */
		if (sblock.fs_magic == FS_UFS1_MAGIC && doinglevel2 &&
		    DIP(dp, di_size) > 0 &&
		    DIP(dp, di_size) < MAXSYMLINKLEN_UFS1 &&
		    DIP(dp, di_blocks) != 0) {
			symbuf = alloca(secsize);
			if (bread(fsreadfd, symbuf,
			    fsbtodb(&sblock, DIP(dp, di_db[0])),
			    (long)secsize) != 0)
				errexit("cannot read symlink\n");
			if (debug) {
				symbuf[DIP(dp, di_size)] = 0;
				printf("convert symlink %d(%s) of size %llu\n",
					inumber, symbuf,
					(unsigned long long)DIP(dp, di_size));
			}
			dp = ginode(inumber);
			memcpy(dp->dp1.di_shortlink, symbuf,
			    (long)DIP(dp, di_size));
			DIP_SET(dp, di_blocks, 0);
			inodirty();
		}
		/*
		 * Fake ndb value so direct/indirect block checks below
		 * will detect any garbage after symlink string.
		 */
		if (DIP(dp, di_size) < sblock.fs_maxsymlinklen ||
		    (sblock.fs_maxsymlinklen == 0 && DIP(dp, di_blocks) == 0)) {
			if (sblock.fs_magic == FS_UFS1_MAGIC)
				ndb = howmany(DIP(dp, di_size),
				    sizeof(ufs1_daddr_t));
			else
				ndb = howmany(DIP(dp, di_size),
				    sizeof(daddr64_t));
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
	lncntp[inumber] = DIP(dp, di_nlink);
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
			statemap[inumber] = DCLEAR;
		else
			statemap[inumber] = DSTATE;
		cacheino(dp, inumber);
	} else
		statemap[inumber] = FSTATE;
	typemap[inumber] = IFTODT(mode);
	if (sblock.fs_magic == FS_UFS1_MAGIC && doinglevel2 &&
	   (dp->dp1.di_ouid != (u_short)-1 ||
	    dp->dp1.di_ogid != (u_short)-1)) {
		dp = ginode(inumber);
		DIP_SET(dp, di_uid, dp->dp1.di_ouid);
		dp->dp1.di_ouid = -1;
		DIP_SET(dp, di_gid, dp->dp1.di_ogid);
		dp->dp1.di_ogid = -1;
		inodirty();
	}
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
	statemap[inumber] = FCLEAR;
	if (reply("CLEAR") == 1) {
		statemap[inumber] = USTATE;
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
	daddr_t blkno = idesc->id_blkno;
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
