/*	$NetBSD: sys.c,v 1.13 1995/01/09 22:13:12 ws Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include "boot.h"
#include <sys/dirent.h>
#include <sys/reboot.h>

char mapbuf[MAXBSIZE], iobuf[MAXBSIZE], fsbuf[SBSIZE];
int mapblock = 0;
char pathname[MAXPATHLEN + 1];

void bcopy(), pcpy();

read(buffer, count)
	char *buffer;
	int count;
{
	_read(buffer, count, bcopy);
}

xread(buffer, count)
	char *buffer;
	int count;
{
	_read(buffer, count, pcpy);
}

_read(buffer, count, copy)
	char *buffer;
	int count;
	void (*copy)();
{
	int logno, off, size;
	int cnt2;

#ifdef DOSREAD
      extern short doshandle;
      if (doshandle>=0)
        return __dosread(buffer,count,copy);
#endif
	while (count) {
		off = blkoff(fs, poff);
		logno = lblkno(fs, poff);
		cnt2 = size = blksize(fs, &inode, logno);
		bnum = fsbtodb(fs, block_map(logno)) + boff;
		cnt = cnt2;
		iodest = iobuf;
		devread();
		size -= off;
		if (size > count)
			size = count;
		copy(iodest + off, buffer, size);
		buffer += size;
		count -= size;
		poff += size;
	}
}

find(path)
	char *path;
{
	char *rest, ch;
	int block, off, loc, ino = ROOTINO, parent;
	struct dirent *dp;
	int nlinks = 0;
	int list_only = 0;

	if (strcmp(path, "?") == 0)
		list_only = 1;
	
loop:
	iodest = iobuf;
	cnt = fs->fs_bsize;
	bnum = fsbtodb(fs, ino_to_fsba(fs,ino)) + boff;
	devread();
	bcopy(&((struct dinode *)iodest)[ino_to_fsbo(fs,ino)],
	      &inode.i_din,
	      sizeof(struct dinode));
	if ((inode.i_mode & IFMT) == IFLNK) {
		int link_len = inode.i_size;
		int len = strlen(path);
		
		if (link_len + len > MAXPATHLEN ||
		    ++ nlinks > MAXSYMLINKS)
			return 0;
		bcopy(path, &pathname[link_len], len + 1);
		if (link_len < fs->fs_maxsymlinklen)
			bcopy(inode.i_shortlink, pathname, link_len);
		else {
			poff = 0;
			read(pathname,link_len);
		}
		path = pathname;
		if (*pathname == '/')
			ino = ROOTINO;
		else
			ino = parent;
		goto loop;
	}
	if (!*path)
		return 1;
	while (*path == '/')
		path++;
	if (!inode.i_size || ((inode.i_mode & IFMT) != IFDIR))
		return 0;
	parent = ino;
	for (rest = path; (ch = *rest) && ch != '/'; rest++);
	*rest = 0;
	loc = 0;
	do {
		if (loc >= inode.i_size)
			if (list_only) {
				putchar('\n');
				return -1;
			} else
				return 0;
		if (!(off = blkoff(fs, loc))) {
			int cnt2;
			block = lblkno(fs, loc);
			cnt2 = blksize(fs, &inode, block);
			bnum = fsbtodb(fs, block_map(block)) + boff;
			cnt = cnt2;
			iodest = iobuf;
			devread();
		}
		dp = (struct dirent *)(iodest + off);
		if (dp->d_reclen < 8) {
			printf("dir corrupt (geom. mismatch?)\n");
			return 0;
		}
		loc += dp->d_reclen;
		if (dp->d_fileno && list_only &&
		    dp->d_type == DT_REG && dp->d_name[0] != '.') {
			printf("%s", dp->d_name);
			putchar(' ');
		}
	} while (!dp->d_fileno || strcmp(path, dp->d_name));
	ino = dp->d_fileno;
	*(path = rest) = ch;
	goto loop;
}

block_map(file_block)
	int file_block;
{
	if (file_block < NDADDR)
		return(inode.i_db[file_block]);
	if ((bnum = fsbtodb(fs, inode.i_ib[0]) + boff) != mapblock) {
		iodest = mapbuf;
		cnt = fs->fs_bsize;
		devread();
		mapblock = bnum;
	}
	return (((int *)mapbuf)[(file_block - NDADDR) % NINDIR(fs)]);
}

openrd()
{
	char **devp, *cp = name;
	/*******************************************************\
	* If bracket given look for preceding device name	*
	\*******************************************************/
	while (*cp && *cp!='(')
		cp++;
	if (!*cp) {
		cp = name;
	} else {
		if (cp++ != name) {
			for (devp = devs; *devp; devp++)
				if (name[0] == (*devp)[0] &&
				    name[1] == (*devp)[1])
					break;
			if (!*devp) {
				printf("Unknown device\n");
				return 1;
			}
			maj = devp-devs;
		}
		/*******************************************************\
		* Look inside brackets for unit number, and partition	*
		\*******************************************************/
		if (*cp >= '0' && *cp <= '9')
			unit = *cp++ - '0';	/* enough for both wd and sd */
		else {
			printf("Bad unit\n");
			return 1;
		}
		if (!*cp || (*cp == ',' && !*++cp))
			return 1;
		if (*cp >= 'a' && *cp <= 'p')
			part = *cp++ - 'a';
		while (*cp && *cp++!=')') ;
		if (!*cp)
			return 1;
	}
	if (maj == 1) {
		dosdev = unit | 0x80;
		unit = 0;
	} else if (maj == 0 || maj == 4)
		dosdev = unit | 0x80;
	else if (maj == 2)
		dosdev = unit;
	else if (maj == 3) {
		printf("Wangtek unsupported\n");
		return 1;
	}
#ifdef DOSREAD
      else if (maj == 5) {
        return dosopenrd(cp);
      }
#endif
	inode.i_dev = dosdev;
	/***********************************************\
	* Now we know the disk unit and part,		*
	* Load disk info, (open the device)		*
	\***********************************************/
	if (devopen())
		return 1;

	/***********************************************\
	* Load Filesystem info (mount the device)	*
	\***********************************************/
	iodest = (char *)(fs = (struct fs *)fsbuf);
	cnt = SBSIZE;
	bnum = SBLOCK + boff;
	devread();

	/*
	 * Deal with old file system format.  This is borrowed from
	 * ffs_oldfscompat() in ufs/ffs/ffs_vfsops.c.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {			/* XXX */
		fs->fs_qbmask = ~fs->fs_bmask;			/* XXX */
		fs->fs_qfmask = ~fs->fs_fmask;			/* XXX */
	}							/* XXX */

	/***********************************************\
	* Find the actual FILE on the mounted device	*
	\***********************************************/
	switch(find(cp)) {
	case -1:
		return -1;
	case 0:
		return 1;
	}

	poff = 0;
	name = cp;
	return 0;
}
