/*	$NetBSD: dir.c,v 1.1 1997/06/11 11:21:46 bouyer Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)dir.c	8.5 (Berkeley) 12/8/94";
#else
static char rcsid[] = "$NetBSD: dir.c,v 1.1 1997/06/11 11:21:46 bouyer Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs.h>

#include <ufs/ufs/dinode.h> /* for IFMT & friends */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsck.h"
#include "fsutil.h"
#include "extern.h"

char	*lfname = "lost+found";
int	lfmode = 01777;
/* XXX DIRBLKSIZ id bsize ! */
#define DIRBLKSIZ 0 /* just for now */
struct	ext2fs_dirtemplate emptydir = { 0, DIRBLKSIZ }; 
struct	ext2fs_dirtemplate dirhead = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};
#undef DIRBLKSIZ

static int expanddir __P((struct ext2fs_dinode *, char *));
static void freedir __P((ino_t, ino_t));
static struct ext2fs_direct *fsck_readdir __P((struct inodesc *));
static struct bufarea *getdirblk __P((daddr_t, long));
static int lftempname __P((char *, ino_t));
static int mkentry __P((struct inodesc *));
static int chgino __P((struct  inodesc *));

/*
 * Propagate connected state through the tree.
 */
void
propagate()
{
	register struct inoinfo **inpp, *inp, *pinp;
	struct inoinfo **inpend;

	/*
	 * Create a list of children for each directory.
	 */
	inpend = &inpsort[inplast];
	for (inpp = inpsort; inpp < inpend; inpp++) {
		inp = *inpp;
		if (inp->i_parent == 0 ||
		    inp->i_number == EXT2_ROOTINO)
			continue;
		pinp = getinoinfo(inp->i_parent);
		inp->i_parentp = pinp;
		inp->i_sibling = pinp->i_child;
		pinp->i_child = inp;
	}
	inp = getinoinfo(EXT2_ROOTINO);
	while (inp) {
		statemap[inp->i_number] = DFOUND;
		if (inp->i_child &&
		    statemap[inp->i_child->i_number] == DSTATE)
			inp = inp->i_child;
		else if (inp->i_sibling)
			inp = inp->i_sibling;
		else
			inp = inp->i_parentp;
	}
}

/*
 * Scan each entry in a directory block.
 */
int
dirscan(idesc)
	register struct inodesc *idesc;
{
	register struct ext2fs_direct *dp;
	register struct bufarea *bp;
	int dsize, n;
	long blksiz;
	char *dbuf = NULL;

	if ((dbuf = malloc(sblock.e2fs_bsize)) == NULL) {
		fprintf(stderr, "out of memory");
		exit(8);
	}

	if (idesc->id_type != DATA)
		errexit("wrong type to dirscan %d\n", idesc->id_type);
	if (idesc->id_entryno == 0 &&
	    (idesc->id_filesize & (sblock.e2fs_bsize - 1)) != 0)
		idesc->id_filesize = roundup(idesc->id_filesize, sblock.e2fs_bsize);
	blksiz = idesc->id_numfrags * sblock.e2fs_bsize;
	if (chkrange(idesc->id_blkno, idesc->id_numfrags)) {
		idesc->id_filesize -= blksiz;
		return (SKIP);
	}
	idesc->id_loc = 0;
	for (dp = fsck_readdir(idesc); dp != NULL; dp = fsck_readdir(idesc)) {
		dsize = dp->e2d_reclen;
		memcpy(dbuf, dp, (size_t)dsize);
		idesc->id_dirp = (struct ext2fs_direct *)dbuf;
		if ((n = (*idesc->id_func)(idesc)) & ALTERED) {
			bp = getdirblk(idesc->id_blkno, blksiz);
			memcpy(bp->b_un.b_buf + idesc->id_loc - dsize, dbuf,
			    (size_t)dsize);
			dirty(bp);
			sbdirty();
		}
		if (n & STOP) {
			free(dbuf);
			return (n);
		}
	}
	free(dbuf);
	return (idesc->id_filesize > 0 ? KEEPON : STOP);
}

/*
 * get next entry in a directory.
 */
static struct ext2fs_direct *
fsck_readdir(idesc)
	register struct inodesc *idesc;
{
	register struct ext2fs_direct *dp, *ndp;
	register struct bufarea *bp;
	long size, blksiz, fix, dploc;

	blksiz = idesc->id_numfrags * sblock.e2fs_bsize;
	bp = getdirblk(idesc->id_blkno, blksiz);
	if (idesc->id_loc % sblock.e2fs_bsize == 0 && idesc->id_filesize > 0 &&
	    idesc->id_loc < blksiz) {
		dp = (struct ext2fs_direct *)(bp->b_un.b_buf + idesc->id_loc);
		if (dircheck(idesc, dp))
			goto dpok;
		if (idesc->id_fix == IGNORE)
			return (0);
		fix = dofix(idesc, "DIRECTORY CORRUPTED");
		bp = getdirblk(idesc->id_blkno, blksiz);
		dp = (struct ext2fs_direct *)(bp->b_un.b_buf + idesc->id_loc);
		dp->e2d_reclen = sblock.e2fs_bsize;
		dp->e2d_ino = 0;
		dp->e2d_namlen = 0;
		dp->e2d_name[0] = '\0';
		if (fix)
			dirty(bp);
		idesc->id_loc += sblock.e2fs_bsize;
		idesc->id_filesize -= sblock.e2fs_bsize;
		return (dp);
	}
dpok:
	if (idesc->id_filesize <= 0 || idesc->id_loc >= blksiz)
		return NULL;
	dploc = idesc->id_loc;
	dp = (struct ext2fs_direct *)(bp->b_un.b_buf + dploc);
	idesc->id_loc += dp->e2d_reclen;
	idesc->id_filesize -= dp->e2d_reclen;
	if ((idesc->id_loc % sblock.e2fs_bsize) == 0)
		return (dp);
	ndp = (struct ext2fs_direct *)(bp->b_un.b_buf + idesc->id_loc);
	if (idesc->id_loc < blksiz && idesc->id_filesize > 0 &&
	    dircheck(idesc, ndp) == 0) {
		size = sblock.e2fs_bsize - (idesc->id_loc % sblock.e2fs_bsize);
		idesc->id_loc += size;
		idesc->id_filesize -= size;
		if (idesc->id_fix == IGNORE)
			return (0);
		fix = dofix(idesc, "DIRECTORY CORRUPTED");
		bp = getdirblk(idesc->id_blkno, blksiz);
		dp = (struct ext2fs_direct *)(bp->b_un.b_buf + dploc);
		dp->e2d_reclen += size;
		if (fix)
			dirty(bp);
	}
	return (dp);
}

/*
 * Verify that a directory entry is valid.
 * This is a superset of the checks made in the kernel.
 */
int
dircheck(idesc, dp)
	struct inodesc *idesc;
	struct ext2fs_direct *dp;
{
	int size;
	char *cp;
	u_char namlen;
	int spaceleft;

	spaceleft = sblock.e2fs_bsize - (idesc->id_loc % sblock.e2fs_bsize);
	if (dp->e2d_ino > maxino ||
	    dp->e2d_reclen == 0 ||
	    dp->e2d_reclen > spaceleft ||
	    (dp->e2d_reclen & 0x3) != 0)
		return (0);
	if (dp->e2d_ino == 0)
		return (1);
	size = EXT2FS_DIRSIZ(dp->e2d_namlen);
	namlen = dp->e2d_namlen;
	if (dp->e2d_reclen < size ||
	    idesc->id_filesize < size ||
	    namlen > EXT2FS_MAXNAMLEN)
		return (0);
	for (cp = dp->e2d_name, size = 0; size < namlen; size++)
		if (*cp == '\0' || (*cp++ == '/'))
			return (0);
	return (1);
}

void
direrror(ino, errmesg)
	ino_t ino;
	char *errmesg;
{

	fileerror(ino, ino, errmesg);
}

void
fileerror(cwd, ino, errmesg)
	ino_t cwd, ino;
	char *errmesg;
{
	register struct ext2fs_dinode *dp;
	char pathbuf[MAXPATHLEN + 1];

	pwarn("%s ", errmesg);
	pinode(ino);
	printf("\n");
	getpathname(pathbuf, cwd, ino);
	if ((ino < EXT2_FIRSTINO && ino != EXT2_ROOTINO) || ino > maxino) {
		pfatal("NAME=%s\n", pathbuf);
		return;
	}
	dp = ginode(ino);
	if (ftypeok(dp))
		pfatal("%s=%s\n",
		    (dp->e2di_mode & IFMT) == IFDIR ? "DIR" : "FILE", pathbuf);
	else
		pfatal("NAME=%s\n", pathbuf);
}

void
adjust(idesc, lcnt)
	register struct inodesc *idesc;
	short lcnt;
{
	register struct ext2fs_dinode *dp;

	dp = ginode(idesc->id_number);
	if (dp->e2di_nlink == lcnt) {
		if (linkup(idesc->id_number, (ino_t)0) == 0)
			clri(idesc, "UNREF", 0);
	} else {
		pwarn("LINK COUNT %s", (lfdir == idesc->id_number) ? lfname :
			((dp->e2di_mode & IFMT) == IFDIR ? "DIR" : "FILE"));
		pinode(idesc->id_number);
		printf(" COUNT %d SHOULD BE %d",
			dp->e2di_nlink, dp->e2di_nlink - lcnt);
		if (preen) {
			if (lcnt < 0) {
				printf("\n");
				pfatal("LINK COUNT INCREASING");
			}
			printf(" (ADJUSTED)\n");
		}
		if (preen || reply("ADJUST") == 1) {
			dp->e2di_nlink -= lcnt;
			inodirty();
		}
	}
}

static int
mkentry(idesc)
	struct inodesc *idesc;
{
	register struct ext2fs_direct *dirp = idesc->id_dirp;
	struct ext2fs_direct newent;
	int newlen, oldlen;

	newent.e2d_namlen = strlen(idesc->id_name);
	newlen = EXT2FS_DIRSIZ(newent.e2d_namlen);
	if (dirp->e2d_ino != 0)
		oldlen = EXT2FS_DIRSIZ(dirp->e2d_namlen);
	else
		oldlen = 0;
	if (dirp->e2d_reclen - oldlen < newlen)
		return (KEEPON);
	newent.e2d_reclen = dirp->e2d_reclen - oldlen;
	dirp->e2d_reclen = oldlen;
	dirp = (struct ext2fs_direct *)(((char *)dirp) + oldlen);
	dirp->e2d_ino = idesc->id_parent;	/* ino to be entered is in id_parent */
	dirp->e2d_reclen = newent.e2d_reclen;
	dirp->e2d_namlen = newent.e2d_namlen;
	memcpy(dirp->e2d_name, idesc->id_name, (size_t)dirp->e2d_namlen);
	return (ALTERED|STOP);
}

static int
chgino(idesc)
	struct inodesc *idesc;
{
	register struct ext2fs_direct *dirp = idesc->id_dirp;

	if (strlen(idesc->id_name) != dirp->e2d_namlen ||
		strncmp(dirp->e2d_name, idesc->id_name, (int)dirp->e2d_namlen))
		return (KEEPON);
	dirp->e2d_ino = idesc->id_parent;
	return (ALTERED|STOP);
}

int
linkup(orphan, parentdir)
	ino_t orphan;
	ino_t parentdir;
{
	register struct ext2fs_dinode *dp;
	int lostdir;
	ino_t oldlfdir;
	struct inodesc idesc;
	char tempname[BUFSIZ];

	memset(&idesc, 0, sizeof(struct inodesc));
	dp = ginode(orphan);
	lostdir = (dp->e2di_mode & IFMT) == IFDIR;
	pwarn("UNREF %s ", lostdir ? "DIR" : "FILE");
	pinode(orphan);
	if (preen && dp->e2di_size == 0)
		return (0);
	if (preen)
		printf(" (RECONNECTED)\n");
	else
		if (reply("RECONNECT") == 0)
			return (0);
	if (lfdir == 0) {
		dp = ginode(EXT2_ROOTINO);
		idesc.id_name = lfname;
		idesc.id_type = DATA;
		idesc.id_func = findino;
		idesc.id_number = EXT2_ROOTINO;
		if ((ckinode(dp, &idesc) & FOUND) != 0) {
			lfdir = idesc.id_parent;
		} else {
			pwarn("NO lost+found DIRECTORY");
			if (preen || reply("CREATE")) {
				lfdir = allocdir(EXT2_ROOTINO, (ino_t)0, lfmode);
				if (lfdir != 0) {
					if (makeentry(EXT2_ROOTINO, lfdir, lfname) != 0) {
						if (preen)
							printf(" (CREATED)\n");
					} else {
						freedir(lfdir, EXT2_ROOTINO);
						lfdir = 0;
						if (preen)
							printf("\n");
					}
				}
			}
		}
		if (lfdir == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY");
			printf("\n\n");
			return (0);
		}
	}
	dp = ginode(lfdir);
	if ((dp->e2di_mode & IFMT) != IFDIR) {
		pfatal("lost+found IS NOT A DIRECTORY");
		if (reply("REALLOCATE") == 0)
			return (0);
		oldlfdir = lfdir;
		if ((lfdir = allocdir(EXT2_ROOTINO, (ino_t)0, lfmode)) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			return (0);
		}
		if ((changeino(EXT2_ROOTINO, lfname, lfdir) & ALTERED) == 0) {
			pfatal("SORRY. CANNOT CREATE lost+found DIRECTORY\n\n");
			return (0);
		}
		inodirty();
		idesc.id_type = ADDR;
		idesc.id_func = pass4check;
		idesc.id_number = oldlfdir;
		adjust(&idesc, lncntp[oldlfdir] + 1);
		lncntp[oldlfdir] = 0;
		dp = ginode(lfdir);
	}
	if (statemap[lfdir] != DFOUND) {
		pfatal("SORRY. NO lost+found DIRECTORY\n\n");
		return (0);
	}
	(void)lftempname(tempname, orphan);
	if (makeentry(lfdir, orphan, tempname) == 0) {
		pfatal("SORRY. NO SPACE IN lost+found DIRECTORY");
		printf("\n\n");
		return (0);
	}
	lncntp[orphan]--;
	if (lostdir) {
		if ((changeino(orphan, "..", lfdir) & ALTERED) == 0 &&
		    parentdir != (ino_t)-1)
			(void)makeentry(orphan, lfdir, "..");
		dp = ginode(lfdir);
		dp->e2di_nlink++;
		inodirty();
		lncntp[lfdir]++;
		pwarn("DIR I=%u CONNECTED. ", orphan);
		if (parentdir != (ino_t)-1)
			printf("PARENT WAS I=%u\n", parentdir);
		if (preen == 0)
			printf("\n");
	}
	return (1);
}

/*
 * fix an entry in a directory.
 */
int
changeino(dir, name, newnum)
	ino_t dir;
	char *name;
	ino_t newnum;
{
	struct inodesc idesc;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = chgino;
	idesc.id_number = dir;
	idesc.id_fix = DONTKNOW;
	idesc.id_name = name;
	idesc.id_parent = newnum;	/* new value for name */
	return (ckinode(ginode(dir), &idesc));
}

/*
 * make an entry in a directory
 */
int
makeentry(parent, ino, name)
	ino_t parent, ino;
	char *name;
{
	struct ext2fs_dinode *dp;
	struct inodesc idesc;
	char pathbuf[MAXPATHLEN + 1];
	
	if ((parent < EXT2_FIRSTINO && parent != EXT2_ROOTINO)
		|| parent >= maxino ||
	    (ino < EXT2_FIRSTINO && ino < EXT2_ROOTINO) || ino >= maxino)
		return (0);
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_func = mkentry;
	idesc.id_number = parent;
	idesc.id_parent = ino;	/* this is the inode to enter */
	idesc.id_fix = DONTKNOW;
	idesc.id_name = name;
	dp = ginode(parent);
	if (dp->e2di_size % sblock.e2fs_bsize) {
		dp->e2di_size = roundup(dp->e2di_size, sblock.e2fs_bsize);
		inodirty();
	}
	if ((ckinode(dp, &idesc) & ALTERED) != 0)
		return (1);
	getpathname(pathbuf, parent, parent);
	dp = ginode(parent);
	if (expanddir(dp, pathbuf) == 0)
		return (0);
	return (ckinode(dp, &idesc) & ALTERED);
}

/*
 * Attempt to expand the size of a directory
 */
static int
expanddir(dp, name)
	register struct ext2fs_dinode *dp;
	char *name;
{
	daddr_t lastbn, newblk;
	register struct bufarea *bp;
	char *cp, *firstblk;

	if ((firstblk = malloc(sblock.e2fs_bsize)) == NULL) {
		fprintf(stderr, "out of memory");
		exit(8);
	}

	lastbn = lblkno(&sblock, dp->e2di_size);
	if (lastbn >= NDADDR - 1 || dp->e2di_blocks[lastbn] == 0 ||
		dp->e2di_size == 0)
		return (0);
	if ((newblk = allocblk()) == 0)
		return (0);
	dp->e2di_blocks[lastbn + 1] = dp->e2di_blocks[lastbn];
	dp->e2di_blocks[lastbn] = newblk;
	dp->e2di_size += sblock.e2fs_bsize;
	dp->e2di_nblock += 1;
	bp = getdirblk(dp->e2di_blocks[lastbn + 1],
		sblock.e2fs_bsize);
	if (bp->b_errs)
		goto bad;
	memcpy(firstblk, bp->b_un.b_buf, sblock.e2fs_bsize);
	bp = getdirblk(newblk, sblock.e2fs_bsize);
	if (bp->b_errs)
		goto bad;
	memcpy(bp->b_un.b_buf, firstblk, sblock.e2fs_bsize);
	emptydir.dot_reclen = sblock.e2fs_bsize;
	for (cp = &bp->b_un.b_buf[sblock.e2fs_bsize];
	     cp < &bp->b_un.b_buf[sblock.e2fs_bsize];
	     cp += sblock.e2fs_bsize)
		memcpy(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	bp = getdirblk(dp->e2di_blocks[lastbn + 1],
		sblock.e2fs_bsize);
	if (bp->b_errs)
		goto bad;
	memcpy(bp->b_un.b_buf, &emptydir, sizeof emptydir);
	pwarn("NO SPACE LEFT IN %s", name);
	if (preen)
		printf(" (EXPANDED)\n");
	else if (reply("EXPAND") == 0)
		goto bad;
	dirty(bp);
	inodirty();
	return (1);
bad:
	dp->e2di_blocks[lastbn] = dp->e2di_blocks[lastbn + 1];
	dp->e2di_blocks[lastbn + 1] = 0;
	dp->e2di_size -= sblock.e2fs_bsize;
	dp->e2di_nblock -= sblock.e2fs_bsize;
	freeblk(newblk);
	return (0);
}

/*
 * allocate a new directory
 */
int
allocdir(parent, request, mode)
	ino_t parent, request;
	int mode;
{
	ino_t ino;
	char *cp;
	struct ext2fs_dinode *dp;
	register struct bufarea *bp;
	struct ext2fs_dirtemplate *dirp;

	ino = allocino(request, IFDIR|mode);
	dirhead.dot_reclen = 12; /* XXX */
	dirhead.dotdot_reclen = sblock.e2fs_bsize - 12; /* XXX */
	dirp = &dirhead;
	dirp->dot_ino = ino;
	dirp->dotdot_ino = parent;
	dp = ginode(ino);
	bp = getdirblk(dp->e2di_blocks[0], sblock.e2fs_bsize);
	if (bp->b_errs) {
		freeino(ino);
		return (0);
	}
	emptydir.dot_reclen = sblock.e2fs_bsize;
	memcpy(bp->b_un.b_buf, dirp, sizeof(struct ext2fs_dirtemplate));
	for (cp = &bp->b_un.b_buf[sblock.e2fs_bsize];
	     cp < &bp->b_un.b_buf[sblock.e2fs_bsize];
	     cp += sblock.e2fs_bsize)
		memcpy(cp, &emptydir, sizeof emptydir);
	dirty(bp);
	dp->e2di_nlink = 2;
	inodirty();
	if (ino == EXT2_ROOTINO) {
		lncntp[ino] = dp->e2di_nlink;
		cacheino(dp, ino);
		return(ino);
	}
	if (statemap[parent] != DSTATE && statemap[parent] != DFOUND) {
		freeino(ino);
		return (0);
	}
	cacheino(dp, ino);
	statemap[ino] = statemap[parent];
	if (statemap[ino] == DSTATE) {
		lncntp[ino] = dp->e2di_nlink;
		lncntp[parent]++;
	}
	dp = ginode(parent);
	dp->e2di_nlink++;
	inodirty();
	return (ino);
}

/*
 * free a directory inode
 */
static void
freedir(ino, parent)
	ino_t ino, parent;
{
	struct ext2fs_dinode *dp;

	if (ino != parent) {
		dp = ginode(parent);
		dp->e2di_nlink--;
		inodirty();
	}
	freeino(ino);
}

/*
 * generate a temporary name for the lost+found directory.
 */
static int
lftempname(bufp, ino)
	char *bufp;
	ino_t ino;
{
	register ino_t in;
	register char *cp;
	int namlen;

	cp = bufp + 2;
	for (in = maxino; in > 0; in /= 10)
		cp++;
	*--cp = 0;
	namlen = cp - bufp;
	in = ino;
	while (cp > bufp) {
		*--cp = (in % 10) + '0';
		in /= 10;
	}
	*cp = '#';
	return (namlen);
}

/*
 * Get a directory block.
 * Insure that it is held until another is requested.
 */
static struct bufarea *
getdirblk(blkno, size)
	daddr_t blkno;
	long size;
{

	if (pdirbp != 0)
		pdirbp->b_flags &= ~B_INUSE;
	pdirbp = getdatablk(blkno, size);
	return (pdirbp);
}
