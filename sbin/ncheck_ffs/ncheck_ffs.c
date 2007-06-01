/*	$OpenBSD: ncheck_ffs.c,v 1.29 2007/06/01 06:41:35 deraadt Exp $	*/

/*-
 * Copyright (c) 1995, 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1980, 1988, 1991, 1993
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
static const char rcsid[] = "$OpenBSD: ncheck_ffs.c,v 1.29 2007/06/01 06:41:35 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ufs/ffs/fs.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fstab.h>
#include <errno.h>
#include <err.h>

#define DIP(dp, field) \
    ((sblock->fs_magic == FS_UFS1_MAGIC) ? \
    ((struct ufs1_dinode *)(dp))->field : \
    ((struct ufs2_dinode *)(dp))->field)

char	*disk;		/* name of the disk file */
int	diskfd;		/* disk file descriptor */
struct	fs *sblock;	/* the file system super block */
char	sblock_buf[MAXBSIZE];
int	sblock_try[] = SBLOCKSEARCH; /* possible superblock locations */
long	dev_bsize;	/* block size of underlying disk device */
int	dev_bshift;	/* log2(dev_bsize) */
ino_t	*ilist;		/* list of inodes to check */
int	ninodes;	/* number of inodes in list */
int	sflag;		/* only suid and special files */
int	aflag;		/* print the . and .. entries too */
int	mflag;		/* verbose output */
int	iflag;		/* specific inode */
char	*format;	/* output format */

struct icache_s {
	ino_t		ino;
	union {
		struct ufs1_dinode dp1;
		struct ufs2_dinode dp2;
	} di;
} *icache;
int	nicache;

void addinode(ino_t inum);
void *getino(ino_t inum);
void findinodes(ino_t);
void bread(daddr64_t, char *, int);
__dead void usage(void);
void scanonedir(ino_t, const char *);
void dirindir(ino_t, daddr64_t, int, off_t, const char *);
void searchdir(ino_t, daddr64_t, long, off_t, const char *);
int matchino(const void *, const void *);
int matchcache(const void *, const void *);
void cacheino(ino_t, void *);
void *cached(ino_t);
int main(int, char *[]);
char *rawname(char *);
void format_entry(const char *, struct direct *);

/*
 * Check to see if the indicated inodes are the same
 */
int
matchino(const void *key, const void *val)
{
	ino_t k = *(ino_t *)key;
	ino_t v = *(ino_t *)val;

	if (k < v)
		return -1;
	else if (k > v)
		return 1;
	return 0;
}

/*
 * Check if the indicated inode match the entry in the cache
 */
int
matchcache(const void *key, const void *val)
{
	ino_t		ino = *(ino_t *)key;
	struct icache_s	*ic = (struct icache_s *)val;

	if (ino < ic->ino)
		return -1;
	else if (ino > ic->ino)
		return 1;
	return 0;
}

/*
 * Add an inode to the cached entries
 */
void
cacheino(ino_t ino, void *dp)
{
	struct icache_s *newicache;

	newicache = realloc(icache, (nicache + 1) * sizeof(struct icache_s));
	if (newicache == NULL) {
		if (icache)
			free(icache);
		icache = NULL;
		errx(1, "malloc");
	}
	icache = newicache;
	icache[nicache].ino = ino;
	if (sblock->fs_magic == FS_UFS1_MAGIC)
		icache[nicache++].di.dp1 = *(struct ufs1_dinode *)dp;
	else
		icache[nicache++].di.dp2 = *(struct ufs2_dinode *)dp;
}

/*
 * Get a cached inode
 */
void *
cached(ino_t ino)
{
	struct icache_s *ic;
	void *dp = NULL;

	ic = (struct icache_s *)bsearch(&ino, icache, nicache,
	    sizeof(struct icache_s), matchcache);
	if (ic != NULL) {
		if (sblock->fs_magic == FS_UFS1_MAGIC)
			dp = &ic->di.dp1;
		else
			dp = &ic->di.dp2;
	}
	return (dp);
}

/*
 * Walk the inode list for a filesystem to find all allocated inodes
 * Remember inodes we want to give information about and cache all
 * inodes pointing to directories
 */
void
findinodes(ino_t maxino)
{
	ino_t ino;
	void *dp;
	mode_t mode;

	for (ino = ROOTINO; ino < maxino; ino++) {
		dp = getino(ino);
		mode = DIP(dp, di_mode) & IFMT;
		if (!mode)
			continue;
		if (mode == IFDIR)
			cacheino(ino, dp);
		if (iflag ||
		    (sflag && (mode == IFDIR ||
		     ((DIP(dp, di_mode) & (ISGID | ISUID)) == 0 &&
		      (mode == IFREG || mode == IFLNK)))))
			continue;
		addinode(ino);
	}
}

/*
 * Get a specified inode from disk.  Attempt to minimize reads to once
 * per cylinder group
 */
void *
getino(ino_t inum)
{
	static char *itab = NULL;
	static daddr_t iblk = -1;
	void *dp;
	size_t dsize;

	if (inum < ROOTINO || inum >= sblock->fs_ncg * sblock->fs_ipg)
		return NULL;
	if ((dp = cached(inum)) != NULL)
		return dp;
	if (sblock->fs_magic == FS_UFS1_MAGIC)
		dsize = sizeof(struct ufs1_dinode);
	else
		dsize = sizeof(struct ufs2_dinode);
	if ((inum / sblock->fs_ipg) != iblk || itab == NULL) {
		iblk = inum / sblock->fs_ipg;
		if (itab == NULL &&
		    (itab = calloc(sblock->fs_ipg, dsize)) == NULL)
			errx(1, "no memory for inodes");
		bread(fsbtodb(sblock, cgimin(sblock, iblk)), itab,
		      sblock->fs_ipg * dsize);
	}
	return itab + (inum % sblock->fs_ipg) * dsize;
}

/*
 * Read a chunk of data from the disk.
 * Try to recover from hard errors by reading in sector sized pieces.
 * Error recovery is attempted at most BREADEMAX times before seeking
 * consent from the operator to continue.
 */
int	breaderrors = 0;
#define	BREADEMAX 32

void
bread(daddr64_t blkno, char *buf, int size)
{
	int cnt, i;

loop:
	if (lseek(diskfd, ((off_t)blkno << dev_bshift), SEEK_SET) < 0)
		warnx("bread: lseek fails");
	if ((cnt = read(diskfd, buf, size)) == size)
		return;
	if (blkno + (size / dev_bsize) > fsbtodb(sblock, sblock->fs_ffs1_size)) {
		/*
		 * Trying to read the final fragment.
		 *
		 * NB - dump only works in TP_BSIZE blocks, hence
		 * rounds `dev_bsize' fragments up to TP_BSIZE pieces.
		 * It should be smarter about not actually trying to
		 * read more than it can get, but for the time being
		 * we punt and scale back the read only when it gets
		 * us into trouble. (mkm 9/25/83)
		 */
		size -= dev_bsize;
		goto loop;
	}
	if (cnt == -1)
		warnx("read error from %s: %s: [block %lld]: count=%d",
		    disk, strerror(errno), (long long)blkno, size);
	else
		warnx("short read error from %s: [block %lld]: count=%d, got=%d",
		    disk, (long long)blkno, size, cnt);
	if (++breaderrors > BREADEMAX)
		errx(1, "More than %d block read errors from %s", BREADEMAX, disk);
	/*
	 * Zero buffer, then try to read each sector of buffer separately.
	 */
	memset(buf, 0, size);
	for (i = 0; i < size; i += dev_bsize, buf += dev_bsize, blkno++) {
		if (lseek(diskfd, ((off_t)blkno << dev_bshift), SEEK_SET) < 0)
			warnx("bread: lseek2 fails!");
		if ((cnt = read(diskfd, buf, (int)dev_bsize)) == dev_bsize)
			continue;
		if (cnt == -1) {
			warnx("read error from %s: %s: [sector %lld]: count=%ld",
			    disk, strerror(errno), (long long)blkno, dev_bsize);
			continue;
		}
		warnx("short read error from %s: [sector %lld]: count=%ld, got=%d",
		    disk, (long long)blkno, dev_bsize, cnt);
	}
}

/*
 * Add an inode to the in-memory list of inodes to dump
 */
void
addinode(ino_t ino)
{
	ino_t *newilist;

	newilist = realloc(ilist, sizeof(ino_t) * (ninodes + 1));
	if (newilist == NULL) {
		if (ilist)
			free(ilist);
		ilist = NULL;
		errx(4, "not enough memory to allocate tables");
	}
	ilist = newilist;
	ilist[ninodes] = ino;
	ninodes++;
}

/*
 * Scan the directory pointer at by ino
 */
void
scanonedir(ino_t ino, const char *path)
{
	void *dp;
	off_t filesize;
	int i;

	if ((dp = cached(ino)) == NULL)
		return;
	filesize = (off_t)DIP(dp, di_size);
	for (i = 0; filesize > 0 && i < NDADDR; i++) {
		if (DIP(dp, di_db[i]) != 0) {
			searchdir(ino, DIP(dp, di_db[i]),
			    sblksize(sblock, DIP(dp, di_size), i),
			    filesize, path);
		}
		filesize -= sblock->fs_bsize;
	}
	for (i = 0; filesize > 0 && i < NIADDR; i++) {
		if (DIP(dp, di_ib[i]))
			dirindir(ino, DIP(dp, di_ib[i]), i, filesize, path);
	}
}

/*
 * Read indirect blocks, and pass the data blocks to be searched
 * as directories. Quit as soon as any entry is found that will
 * require the directory to be dumped.
 */
void
dirindir(ino_t ino, daddr64_t blkno, int ind_level, off_t filesize,
    const char *path)
{
	int i;
	static void *idblk; 

	if (idblk == NULL && (idblk = malloc(sblock->fs_bsize)) == NULL)
		errx(1, "dirindir: cannot allocate indirect memory.\n");
	bread(fsbtodb(sblock, blkno), idblk, (int)sblock->fs_bsize);
	if (ind_level <= 0) {
		for (i = 0; filesize > 0 && i < NINDIR(sblock); i++) {
			if (sblock->fs_magic == FS_UFS1_MAGIC)
				blkno = ((ufs1_daddr_t *)idblk)[i];
			else
				blkno = ((daddr64_t *)idblk)[i];
			if (blkno != 0)
				searchdir(ino, blkno, sblock->fs_bsize,
				    filesize, path);
		}
		return;
	}
	ind_level--;
	for (i = 0; filesize > 0 && i < NINDIR(sblock); i++) {
		if (sblock->fs_magic == FS_UFS1_MAGIC)
			blkno = ((ufs1_daddr_t *)idblk)[i];
		else
			blkno = ((daddr64_t *)idblk)[i];
		if (blkno != 0)
			dirindir(ino, blkno, ind_level, filesize, path);
	}
}

/*
 * Scan a disk block containing directory information looking to see if
 * any of the entries are on the dump list and to see if the directory
 * contains any subdirectories.
 */
void
searchdir(ino_t ino, daddr64_t blkno, long size, off_t filesize,
    const char *path)
{
	char *dblk;
	struct direct *dp;
	void *di;
	mode_t mode;
	char *npath;
	long loc;

	if ((dblk = malloc(sblock->fs_bsize)) == NULL)
		errx(1, "searchdir: cannot allocate indirect memory.");
	bread(fsbtodb(sblock, blkno), dblk, (int)size);
	if (filesize < size)
		size = filesize;
	for (loc = 0; loc < size; ) {
		dp = (struct direct *)(dblk + loc);
		if (dp->d_reclen == 0) {
			warnx("corrupted directory, inode %u", ino);
			break;
		}
		loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		if (dp->d_name[0] == '.') {
			if (!aflag && (dp->d_name[1] == '\0' ||
			    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
				continue;
		}
		di = getino(dp->d_ino);
		mode = DIP(di, di_mode) & IFMT;
		if (bsearch(&dp->d_ino, ilist, ninodes, sizeof(*ilist), matchino)) {
			if (format) {
				format_entry(path, dp);
			} else {
				if (mflag)
					printf("mode %-6o uid %-5u gid %-5u ino ",
					    DIP(di, di_mode), DIP(di, di_uid),
					    DIP(di, di_gid));
				printf("%-7u %s/%s%s\n", dp->d_ino, path,
				    dp->d_name, mode == IFDIR ? "/." : "");
			}
		}
		if (mode == IFDIR) {
			if (dp->d_name[0] == '.') {
				if (dp->d_name[1] == '\0' ||
				    (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))
				continue;
			}
			if (asprintf(&npath, "%s/%s", path, dp->d_name) == -1)
				errx(1, "malloc");
			scanonedir(dp->d_ino, npath);
			free(npath);
		}
	}
}

char *
rawname(char *name)
{
	static char newname[MAXPATHLEN];
	char *p;

	if ((p = strrchr(name, '/')) == NULL)
		return name;
	*p = '\0';
	strlcpy(newname, name, sizeof newname - 2);
	*p++ = '/';
	strlcat(newname, "/r", sizeof newname);
	strlcat(newname, p, sizeof newname);
	return(newname);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-ams] [-f format] [-i number [...]] filesystem\n",
	    __progname);
	exit(3);
}

int
main(int argc, char *argv[])
{
	struct stat stblock;
	struct fstab *fsp;
	unsigned long ulval;
	ssize_t n;
	char *ep;
	int c, i;

	while ((c = getopt(argc, argv, "af:i:ms")) != -1)
		switch (c) {
		case 'a':
			aflag++;
			break;
		case 'i':
			iflag++;

			errno = 0;
			ulval = strtoul(optarg, &ep, 10);
			if (optarg[0] == '\0' || *ep != '\0')
				errx(1, "%s is not a number",
				    optarg);
			if (errno == ERANGE && ulval == ULONG_MAX)
				errx(1, "%s is out or range",
				    optarg);
			addinode((ino_t)ulval);

			while (optind < argc) {
				errno = 0;
				ulval = strtoul(argv[optind], &ep, 10);
				if (argv[optind][0] == '\0' || *ep != '\0')
					break;
				if (errno == ERANGE && ulval == ULONG_MAX)
					errx(1, "%s is out or range",
					    argv[optind]);
				addinode((ino_t)ulval);
				optind++;
			}
			break;
		case 'f':
			format = optarg;
			break;
		case 'm':
			mflag++;
			break;
		case 's':
			sflag++;
			break;
		default:
			usage();
			exit(2);
		}
	if (optind != argc - 1 || (mflag && format))
		usage();

	disk = argv[optind];

	if (stat(disk, &stblock) < 0)
		err(1, "cannot stat %s", disk);

        if (S_ISBLK(stblock.st_mode)) {
		disk = rawname(disk);
	} else if (!S_ISCHR(stblock.st_mode)) {
		if ((fsp = getfsfile(disk)) == NULL)
			err(1, "cound not find file system %s", disk);
                disk = rawname(fsp->fs_spec);
        }

	if ((diskfd = open(disk, O_RDONLY)) < 0)
		err(1, "cannot open %s", disk);
	sblock = (struct fs *)sblock_buf;
	for (i = 0; sblock_try[i] != -1; i++) {
		n = pread(diskfd, sblock, SBLOCKSIZE, (off_t)sblock_try[i]);
		if (n == SBLOCKSIZE && (sblock->fs_magic == FS_UFS1_MAGIC ||
		     (sblock->fs_magic == FS_UFS2_MAGIC &&
		      sblock->fs_sblockloc == sblock_try[i])) &&
		    sblock->fs_bsize <= MAXBSIZE &&
		    sblock->fs_bsize >= sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1)
		errx(1, "cannot find filesystem superblock");

	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		errx(2, "blocksize (%ld) not a power of 2", dev_bsize);
	findinodes(sblock->fs_ipg * sblock->fs_ncg);
	if (!format)
		printf("%s:\n", disk);
	scanonedir(ROOTINO, "");
	close(diskfd);
	exit (0);
}

void
format_entry(const char *path, struct direct *dp)
{
	static size_t size;
	static char *buf;
	size_t len, nsize;
	char *src, *dst, *newbuf;

	if (buf == NULL) {
		if ((buf = malloc(LINE_MAX)) == NULL)
			err(1, "malloc");
		size = LINE_MAX;
	}

	for (src = format, dst = buf; *src; src++) {
		/* Need room for at least one character in buf. */
		if (size <= dst - buf) {
		    expand_buf:
			nsize = size << 1;

			if ((newbuf = realloc(buf, nsize)) == NULL)
				err(1, "realloc");
			buf = newbuf;
			size = nsize;
		}
		if (src[0] =='\\') {
			switch (src[1]) {
			case 'I':
				len = snprintf(dst, size - (dst - buf), "%u",
				    dp->d_ino);
				if (len == -1 || len >= size - (dst - buf))
					goto expand_buf;
				dst += len;
				break;
			case 'P':
				len = snprintf(dst, size - (dst - buf), "%s/%s",
				    path, dp->d_name);
				if (len == -1 || len >= size - (dst - buf))
					goto expand_buf;
				dst += len;
				break;
			case '\\':
				*dst++ = '\\';
				break;
			case '0':
				/* XXX - support other octal numbers? */
				*dst++ = '\0';
				break;
			case 'a':
				*dst++ = '\a';
				break;
			case 'b':
				*dst++ = '\b';
				break;
			case 'e':
				*dst++ = '\e';
				break;
			case 'f':
				*dst++ = '\f';
				break;
			case 'n':
				*dst++ = '\n';
				break;
			case 'r':
				*dst++ = '\r';
				break;
			case 't':
				*dst++ = '\t';
				break;
			case 'v':
				*dst++ = '\v';
				break;
			default:
				*dst++ = src[1];
				break;
			}
			src++;
		} else
			*dst++ = *src;
	}
	fwrite(buf, dst - buf, 1, stdout);
}
