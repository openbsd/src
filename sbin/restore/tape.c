/*	$OpenBSD: tape.c,v 1.28 2004/04/13 21:51:18 henning Exp $	*/
/*	$NetBSD: tape.c,v 1.26 1997/04/15 07:12:25 lukem Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
static char sccsid[] = "@(#)tape.c	8.6 (Berkeley) 9/13/94";
#else
static const char rcsid[] = "$OpenBSD: tape.c,v 1.28 2004/04/13 21:51:18 henning Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/stat.h>

#include <ufs/ufs/dinode.h>
#include <protocols/dumprestore.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "restore.h"
#include "extern.h"

static long	fssize = MAXBSIZE;
static int	mt = -1;
static int	pipein = 0;
static char	magtape[BUFSIZ];
static int	blkcnt;
static int	numtrec;
static char	*tapebuf;
static union	u_spcl endoftapemark;
static long	blksread;		/* blocks read since last header */
static long	tpblksread = 0;		/* TP_BSIZE blocks read */
static long	tapesread;
static jmp_buf	restart;
static int	gettingfile = 0;	/* restart has a valid frame */
static char	*host = NULL;

static int	ofile;
static char	*map;
static char	lnkbuf[MAXPATHLEN + 1];
static size_t	pathlen;

int		oldinofmt;	/* old inode format conversion required */
int		Bcvt;		/* Swap Bytes (for CCI or sun) */
static int	Qcvt;		/* Swap quads (for sun) */

#define	FLUSHTAPEBUF()	blkcnt = ntrec + 1

static void	 accthdr(struct s_spcl *);
static int	 checksum(int *);
static void	 findinode(struct s_spcl *);
static void	 findtapeblksize(void);
static int	 gethead(struct s_spcl *);
static void	 readtape(char *);
static void	 setdumpnum(void);
static u_long	 swabl(u_long);
static u_char	*swablong(u_char *, int);
static u_char	*swabshort(u_char *, int);
static void	 terminateinput(void);
static void	 xtrfile(char *, size_t);
static void	 xtrlnkfile(char *, size_t);
static void	 xtrlnkskip(char *, size_t);
static void	 xtrmap(char *, size_t);
static void	 xtrmapskip(char *, size_t);
static void	 xtrskip(char *, size_t);

/*
 * Set up an input source
 */
void
setinput(source)
	char *source;
{
	FLUSHTAPEBUF();
	if (bflag)
		newtapebuf(ntrec);
	else
		newtapebuf(NTREC > HIGHDENSITYTREC ? NTREC : HIGHDENSITYTREC);
	terminal = stdin;

#ifdef RRESTORE
	if (strchr(source, ':')) {
		host = source;
		source = strchr(host, ':');
		*source++ = '\0';
		if (rmthost(host) == 0)
			exit(1);
	} else
#endif
	if (strcmp(source, "-") == 0) {
		/*
		 * Since input is coming from a pipe we must establish
		 * our own connection to the terminal.
		 */
		terminal = fopen(_PATH_TTY, "r");
		if (terminal == NULL) {
			warn("cannot open %s", _PATH_TTY);
			terminal = fopen(_PATH_DEVNULL, "r");
			if (terminal == NULL)
				err(1, "cannot open %s", _PATH_DEVNULL);
		}
		pipein++;
	}
	(void)strlcpy(magtape, source, sizeof magtape);
}

void
newtapebuf(size)
	long size;
{
	static long tapebufsize = -1;

	ntrec = size;
	if (size <= tapebufsize)
		return;
	if (tapebuf != NULL)
		free(tapebuf);
	tapebuf = malloc(size * TP_BSIZE);
	if (tapebuf == NULL)
		errx(1, "Cannot allocate space for tape buffer");
	tapebufsize = size;
}

/*
 * Verify that the tape drive can be accessed and
 * that it actually is a dump tape.
 */
void
setup()
{
	int i, j, *ip;
	struct stat stbuf;

	Vprintf(stdout, "Verify tape and initialize maps\n");
#ifdef RRESTORE
	if (host)
		mt = rmtopen(magtape, 0);
	else
#endif
	if (pipein)
		mt = 0;
	else
		mt = open(magtape, O_RDONLY);
	if (mt < 0)
		err(1, "%s", magtape);
	volno = 1;
	setdumpnum();
	FLUSHTAPEBUF();
	if (!pipein && !bflag)
		findtapeblksize();
	if (gethead(&spcl) == FAIL) {
		blkcnt--; /* push back this block */
		blksread--;
		tpblksread--;
		cvtflag++;
		if (gethead(&spcl) == FAIL)
			errx(1, "Tape is not a dump tape");
		(void)fputs("Converting to new file system format.\n", stderr);
	}
	if (pipein) {
		endoftapemark.s_spcl.c_magic = cvtflag ? OFS_MAGIC : NFS_MAGIC;
		endoftapemark.s_spcl.c_type = TS_END;
		ip = (int *)&endoftapemark;
		j = sizeof(union u_spcl) / sizeof(int);
		i = 0;
		do
			i += *ip++;
		while (--j);
		endoftapemark.s_spcl.c_checksum = CHECKSUM - i;
	}
	if (vflag || command == 't')
		printdumpinfo();
	dumptime = spcl.c_ddate;
	dumpdate = spcl.c_date;
	if (stat(".", &stbuf) < 0)
		err(1, "cannot stat .");
	if (stbuf.st_blksize > 0 && stbuf.st_blksize < TP_BSIZE )
		fssize = TP_BSIZE;
	if (stbuf.st_blksize >= TP_BSIZE && stbuf.st_blksize <= MAXBSIZE)
		fssize = stbuf.st_blksize;
	if (((fssize - 1) & fssize) != 0)
		errx(1, "bad block size %ld", fssize);
	if (spcl.c_volume != 1)
		errx(1, "Tape is not volume 1 of the dump");
	if (gethead(&spcl) == FAIL) {
		Dprintf(stdout, "header read failed at %ld blocks\n", blksread);
		panic("no header after volume mark!\n");
	}
	findinode(&spcl);
	if (spcl.c_type != TS_CLRI)
		errx(1, "Cannot find file removal list");
	maxino = (spcl.c_count * TP_BSIZE * NBBY) + 1;
	Dprintf(stdout, "maxino = %d\n", maxino);
	map = calloc((unsigned)1, (unsigned)howmany(maxino, NBBY));
	if (map == NULL)
		panic("no memory for active inode map\n");
	usedinomap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	if (spcl.c_type != TS_BITS)
		errx(1, "Cannot find file dump list");
	map = calloc((size_t)1, (size_t)howmany(maxino, NBBY));
	if (map == NULL)
		panic("no memory for file dump list\n");
	dumpmap = map;
	curfile.action = USING;
	getfile(xtrmap, xtrmapskip);
	/*
	 * If there may be whiteout entries on the tape, pretend that the
	 * whiteout inode exists, so that the whiteout entries can be
	 * extracted.
	 */
	if (oldinofmt == 0)
		SETINO(WINO, dumpmap);
}

/*
 * Prompt user to load a new dump volume.
 * "Nextvol" is the next suggested volume to use.
 * This suggested volume is enforced when doing full
 * or incremental restores, but can be overrridden by
 * the user when only extracting a subset of the files.
 */
void
getvol(nextvol)
	long nextvol;
{
	long newvol = 0, savecnt, wantnext, i;
	union u_spcl tmpspcl;
#	define tmpbuf tmpspcl.s_spcl
	char buf[TP_BSIZE];

	if (nextvol == 1) {
		tapesread = 0;
		gettingfile = 0;
	}
	if (pipein) {
		if (nextvol != 1)
			panic("Changing volumes on pipe input?\n");
		if (volno == 1)
			return;
		goto gethdr;
	}
	savecnt = blksread;
again:
	if (pipein)
		exit(1); /* pipes do not get a second chance */
	if (command == 'R' || command == 'r' || curfile.action != SKIP) {
		newvol = nextvol;
		wantnext = 1;
	} else {
		newvol = 0;
		wantnext = 0;
	}
	while (newvol <= 0) {
		if (tapesread == 0) {
			fprintf(stderr, "%s%s%s%s%s",
			    "You have not read any tapes yet.\n",
			    "Unless you know which volume your",
			    " file(s) are on you should start\n",
			    "with the last volume and work",
			    " towards the first.\n");
		} else {
			fprintf(stderr, "You have read volumes");
			strlcpy(buf, ": ", sizeof buf);
			for (i = 1; i < 32; i++)
				if (tapesread & (1 << i)) {
					fprintf(stderr, "%s%ld", buf, i);
					strlcpy(buf, ", ", sizeof buf);
				}
			fprintf(stderr, "\n");
		}
		do	{
			fprintf(stderr, "Specify next volume #: ");
			(void)fflush(stderr);
			(void)fgets(buf, TP_BSIZE, terminal);
		} while (!feof(terminal) && buf[0] == '\n');
		if (feof(terminal))
			exit(1);
		newvol = atoi(buf);
		if (newvol <= 0) {
			fprintf(stderr,
			    "Volume numbers are positive numerics\n");
		}
	}
	if (newvol == volno) {
		tapesread |= 1 << volno;
		return;
	}
	closemt();
	fprintf(stderr, "Mount tape volume %ld\n", newvol);
	fprintf(stderr, "Enter ``none'' if there are no more tapes\n");
	fprintf(stderr, "otherwise enter tape name (default: %s) ", magtape);
	(void)fflush(stderr);
	if (fgets(buf, TP_BSIZE, terminal) == NULL || feof(terminal))
		exit(1);
	i = strlen(buf);
	if (i > 0 && buf[--i] == '\n')
		buf[i] = '\0';
	if (strcmp(buf, "none") == 0) {
		terminateinput();
		return;
	}
	if (buf[0] != '\0')
		(void)strlcpy(magtape, buf, sizeof magtape);

#ifdef RRESTORE
	if (host)
		mt = rmtopen(magtape, 0);
	else
#endif
		mt = open(magtape, O_RDONLY);

	if (mt == -1) {
		fprintf(stderr, "Cannot open %s\n", magtape);
		volno = -1;
		goto again;
	}
gethdr:
	volno = newvol;
	setdumpnum();
	FLUSHTAPEBUF();
	if (gethead(&tmpbuf) == FAIL) {
		Dprintf(stdout, "header read failed at %ld blocks\n", blksread);
		fprintf(stderr, "tape is not dump tape\n");
		volno = 0;
		goto again;
	}
	if (tmpbuf.c_volume != volno) {
		fprintf(stderr, "Wrong volume (%d)\n", tmpbuf.c_volume);
		volno = 0;
		goto again;
	}
	if (tmpbuf.c_date != dumpdate || tmpbuf.c_ddate != dumptime) {
		fprintf(stderr, "Wrong dump date\n\tgot: %s",
			ctime(&tmpbuf.c_date));
		fprintf(stderr, "\twanted: %s", ctime(&dumpdate));
		volno = 0;
		goto again;
	}
	tapesread |= 1 << volno;
	blksread = savecnt;
 	/*
 	 * If continuing from the previous volume, skip over any
 	 * blocks read already at the end of the previous volume.
 	 *
 	 * If coming to this volume at random, skip to the beginning
 	 * of the next record.
 	 */
	Dprintf(stdout, "read %ld recs, tape starts with %d\n",
		tpblksread, tmpbuf.c_firstrec);
 	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER)) {
 		if (!wantnext) {
 			tpblksread = tmpbuf.c_firstrec;
 			for (i = tmpbuf.c_count; i > 0; i--)
 				readtape(buf);
 		} else if (tmpbuf.c_firstrec > 0 &&
			   tmpbuf.c_firstrec < tpblksread - 1) {
			/*
			 * -1 since we've read the volume header
			 */
 			i = tpblksread - tmpbuf.c_firstrec - 1;
			Dprintf(stderr, "Skipping %ld duplicate record%s.\n",
				i, (i == 1) ? "" : "s");
 			while (--i >= 0)
 				readtape(buf);
 		}
 	}
	if (curfile.action == USING) {
		if (volno == 1)
			panic("active file into volume 1\n");
		return;
	}
	/*
	 * Skip up to the beginning of the next record
	 */
	if (tmpbuf.c_type == TS_TAPE && (tmpbuf.c_flags & DR_NEWHEADER))
		for (i = tmpbuf.c_count; i > 0; i--)
			readtape(buf);
	(void)gethead(&spcl);
	findinode(&spcl);
	if (gettingfile) {
		gettingfile = 0;
		longjmp(restart, 1);
	}
}

/*
 * Handle unexpected EOF.
 */
static void
terminateinput()
{

	if (gettingfile && curfile.action == USING) {
		printf("Warning: %s %s\n",
		    "End-of-input encountered while extracting", curfile.name);
	}
	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = maxino;
	if (gettingfile) {
		gettingfile = 0;
		longjmp(restart, 1);
	}
}

/*
 * handle multiple dumps per tape by skipping forward to the
 * appropriate one.
 */
static void
setdumpnum()
{
	struct mtop tcom;

	if (dumpnum == 1 || volno != 1)
		return;
	if (pipein)
		errx(1, "Cannot have multiple dumps on pipe input");
	tcom.mt_op = MTFSF;
	tcom.mt_count = dumpnum - 1;
#ifdef RRESTORE
	if (host)
		rmtioctl(MTFSF, dumpnum - 1);
	else
#endif
		if (ioctl(mt, MTIOCTOP, (char *)&tcom) < 0)
			warn("ioctl MTFSF");
}

void
printdumpinfo()
{
	fprintf(stdout, "Dump   date: %s", ctime(&spcl.c_date));
	fprintf(stdout, "Dumped from: %s",
	    (spcl.c_ddate == 0) ? "the epoch\n" : ctime(&spcl.c_ddate));
	if (spcl.c_host[0] == '\0')
		return;
	fprintf(stderr, "Level %d dump of %s on %s:%s\n",
		spcl.c_level, spcl.c_filesys, spcl.c_host, spcl.c_dev);
	fprintf(stderr, "Label: %s\n", spcl.c_label);
}

int
extractfile(name)
	char *name;
{
	u_int flags;
	mode_t mode;
	struct timeval timep[2];
	struct entry *ep;

	curfile.name = name;
	curfile.action = USING;
	timep[0].tv_sec = curfile.dip->di_atime;
	timep[0].tv_usec = curfile.dip->di_atimensec / 1000;
	timep[1].tv_sec = curfile.dip->di_mtime;
	timep[1].tv_usec = curfile.dip->di_mtimensec / 1000;
	mode = curfile.dip->di_mode;
	flags = curfile.dip->di_flags;
	switch (mode & IFMT) {

	default:
		fprintf(stderr, "%s: unknown file mode 0%o\n", name, mode);
		skipfile();
		return (FAIL);

	case IFSOCK:
		Vprintf(stdout, "skipped socket %s\n", name);
		skipfile();
		return (GOOD);

	case IFDIR:
		if (mflag) {
			ep = lookupname(name);
			if (ep == NULL || ep->e_flags & EXTRACT)
				panic("unextracted directory %s\n", name);
			skipfile();
			return (GOOD);
		}
		Vprintf(stdout, "extract file %s\n", name);
		return (genliteraldir(name, curfile.ino));

	case IFLNK: {
			/* Gotta save these, linkit() changes curfile. */
			uid_t luid = curfile.dip->di_uid;
			gid_t lgid = curfile.dip->di_gid;

			lnkbuf[0] = '\0';
			pathlen = 0;
			getfile(xtrlnkfile, xtrlnkskip);
			if (pathlen == 0) {
				Vprintf(stdout,
				    "%s: zero length symbolic link (ignored)\n",
				     name);
				return (GOOD);
			}
			if (linkit(lnkbuf, name, SYMLINK) == FAIL)
				return (FAIL);
			(void)lchown(name, luid, lgid);
			return (GOOD);
		}

	case IFCHR:
	case IFBLK:
		Vprintf(stdout, "extract special file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (mknod(name, mode, (int)curfile.dip->di_rdev) < 0) {
			warn("%s: cannot create special file", name);
			skipfile();
			return (FAIL);
		}
		(void)chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void)chmod(name, mode);
		(void)chflags(name, flags);
		skipfile();
		utimes(name, timep);
		return (GOOD);

	case IFIFO:
		Vprintf(stdout, "extract fifo %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if (mkfifo(name, mode) < 0) {
			warn("%s: cannot create fifo", name);
			skipfile();
			return (FAIL);
		}
		(void)chown(name, curfile.dip->di_uid, curfile.dip->di_gid);
		(void)chmod(name, mode);
		(void)chflags(name, flags);
		skipfile();
		utimes(name, timep);
		return (GOOD);

	case IFREG:
		Vprintf(stdout, "extract file %s\n", name);
		if (Nflag) {
			skipfile();
			return (GOOD);
		}
		if ((ofile = open(name, O_WRONLY | O_CREAT | O_TRUNC,
		    0666)) < 0) {
			warn("%s: cannot create file", name);
			skipfile();
			return (FAIL);
		}
		(void)fchown(ofile, curfile.dip->di_uid, curfile.dip->di_gid);
		(void)fchmod(ofile, mode);
		(void)fchflags(ofile, flags);
		getfile(xtrfile, xtrskip);
		(void)close(ofile);
		utimes(name, timep);
		return (GOOD);
	}
	/* NOTREACHED */
}

/*
 * skip over bit maps on the tape
 */
void
skipmaps()
{

	while (spcl.c_type == TS_BITS || spcl.c_type == TS_CLRI)
		skipfile();
}

/*
 * skip over a file on the tape
 */
void
skipfile()
{

	curfile.action = SKIP;
	getfile(xtrnull, xtrnull);
}

/*
 * Extract a file from the tape.
 * When an allocated block is found it is passed to the fill function;
 * when an unallocated block (hole) is found, a zeroed buffer is passed
 * to the skip function.
 *
 * For some block types (TS_BITS, TS_CLRI), the c_addr map is not meaningful
 * and no blocks should be skipped.
 */
void
getfile(fill, skip)
	void	(*fill)(char *, size_t);
	void	(*skip)(char *, size_t);
{
	int i;
	volatile int curblk = 0;
	volatile off_t size = spcl.c_dinode.di_size;
	static char clearedbuf[MAXBSIZE];
	char buf[MAXBSIZE / TP_BSIZE][TP_BSIZE];
	char junk[TP_BSIZE];
	volatile int noskip = (spcl.c_type == TS_BITS || spcl.c_type == TS_CLRI);

	if (spcl.c_type == TS_END)
		panic("ran off end of tape\n");
	if (spcl.c_magic != NFS_MAGIC)
		panic("not at beginning of a file\n");
	if (!gettingfile && setjmp(restart) != 0)
		return;
	gettingfile++;
loop:
	for (i = 0; i < spcl.c_count; i++) {
		if (noskip || spcl.c_addr[i]) {
			readtape(&buf[curblk++][0]);
			if (curblk == fssize / TP_BSIZE) {
				(*fill)((char *)buf, size > TP_BSIZE ?
				     fssize :
				     ((off_t)curblk - 1) * TP_BSIZE + size);
				curblk = 0;
			}
		} else {
			if (curblk > 0) {
				(*fill)((char *)buf, size > TP_BSIZE ?
				     (curblk * TP_BSIZE) :
				     ((off_t)curblk - 1) * TP_BSIZE + size);
				curblk = 0;
			}
			(*skip)(clearedbuf, size > TP_BSIZE ?
				TP_BSIZE : size);
		}
		if ((size -= TP_BSIZE) <= 0) {
			for (i++; i < spcl.c_count; i++)
				if (noskip || spcl.c_addr[i])
					readtape(junk);
			break;
		}
	}
	if (gethead(&spcl) == GOOD && size > 0) {
		if (spcl.c_type == TS_ADDR)
			goto loop;
		Dprintf(stdout,
			"Missing address (header) block for %s at %ld blocks\n",
			curfile.name, blksread);
	}
	if (curblk > 0)
		(*fill)((char *)buf, ((off_t)curblk * TP_BSIZE) + size);
	findinode(&spcl);
	gettingfile = 0;
}

/*
 * Write out the next block of a file.
 */
static void
xtrfile(buf, size)
	char	*buf;
	size_t	size;
{

	if (Nflag)
		return;
	if (write(ofile, buf, size) == -1)
		err(1, "write error extracting inode %d, name %s\nwrite",
		    curfile.ino, curfile.name);
}

/*
 * Skip over a hole in a file.
 */
/* ARGSUSED */
static void
xtrskip(buf, size)
	char *buf;
	size_t size;
{

	if (lseek(ofile, (off_t)size, SEEK_CUR) == -1)
		err(1, "seek error extracting inode %d, name %s\nlseek",
		    curfile.ino, curfile.name);
}

/*
 * Collect the next block of a symbolic link.
 */
static void
xtrlnkfile(buf, size)
	char	*buf;
	size_t	size;
{

	pathlen += size;
	if (pathlen > MAXPATHLEN)
		errx(1, "symbolic link name: %s->%s%s; too long %lu",
		    curfile.name, lnkbuf, buf, (u_long)pathlen);
	(void)strlcat(lnkbuf, buf, sizeof(lnkbuf));
}

/*
 * Skip over a hole in a symbolic link (should never happen).
 */
/* ARGSUSED */
static void
xtrlnkskip(buf, size)
	char *buf;
	size_t size;
{

	errx(1, "unallocated block in symbolic link %s", curfile.name);
}

/*
 * Collect the next block of a bit map.
 */
static void
xtrmap(buf, size)
	char	*buf;
	size_t	size;
{

	memcpy(map, buf, size);
	map += size;
}

/*
 * Skip over a hole in a bit map (should never happen).
 */
/* ARGSUSED */
static void
xtrmapskip(buf, size)
	char *buf;
	size_t size;
{

	panic("hole in map\n");
	map += size;
}

/*
 * Noop, when an extraction function is not needed.
 */
/* ARGSUSED */
void
xtrnull(buf, size)
	char *buf;
	size_t size;
{

	return;
}

/*
 * Read TP_BSIZE blocks from the input.
 * Handle read errors, and end of media.
 */
static void
readtape(buf)
	char *buf;
{
	long rd, newvol, i;
	int cnt, seek_failed;

	if (blkcnt < numtrec) {
		memcpy(buf, &tapebuf[(blkcnt++ * TP_BSIZE)], (long)TP_BSIZE);
		blksread++;
		tpblksread++;
		return;
	}
	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	if (numtrec == 0)
		numtrec = ntrec;
	cnt = ntrec * TP_BSIZE;
	rd = 0;
getmore:
#ifdef RRESTORE
	if (host)
		i = rmtread(&tapebuf[rd], cnt);
	else
#endif
		i = read(mt, &tapebuf[rd], cnt);
	/*
	 * Check for mid-tape short read error.
	 * If found, skip rest of buffer and start with the next.
	 */
	if (!pipein && numtrec < ntrec && i > 0) {
		Dprintf(stdout, "mid-media short read error.\n");
		numtrec = ntrec;
	}
	/*
	 * Handle partial block read.
	 */
	if (pipein && i == 0 && rd > 0)
		i = rd;
	else if (i > 0 && i != ntrec * TP_BSIZE) {
		if (pipein) {
			rd += i;
			cnt -= i;
			if (cnt > 0)
				goto getmore;
			i = rd;
		} else {
			/*
			 * Short read. Process the blocks read.
			 */
			if (i % TP_BSIZE != 0)
				Vprintf(stdout,
				    "partial block read: %ld should be %ld\n",
				    i, ntrec * TP_BSIZE);
			numtrec = i / TP_BSIZE;
		}
	}
	/*
	 * Handle read error.
	 */
	if (i < 0) {
		fprintf(stderr, "Tape read error while ");
		switch (curfile.action) {
		default:
			fprintf(stderr, "trying to set up tape\n");
			break;
		case UNKNOWN:
			fprintf(stderr, "trying to resynchronize\n");
			break;
		case USING:
			fprintf(stderr, "restoring %s\n", curfile.name);
			break;
		case SKIP:
			fprintf(stderr, "skipping over inode %d\n",
				curfile.ino);
			break;
		}
		if (!yflag && !reply("continue"))
			exit(1);
		i = ntrec * TP_BSIZE;
		memset(tapebuf, 0, i);
#ifdef RRESTORE
		if (host)
			seek_failed = (rmtseek(i, 1) < 0);
		else
#endif
			seek_failed = (lseek(mt, i, SEEK_CUR) == (off_t)-1);

		if (seek_failed)
			err(1, "continuation failed");
	}
	/*
	 * Handle end of tape.
	 */
	if (i == 0) {
		Vprintf(stdout, "End-of-tape encountered\n");
		if (!pipein) {
			newvol = volno + 1;
			volno = 0;
			numtrec = 0;
			getvol(newvol);
			readtape(buf);
			return;
		}
		if (rd % TP_BSIZE != 0)
			panic("partial block read: %d should be %d\n",
				rd, ntrec * TP_BSIZE);
		terminateinput();
		memcpy(&tapebuf[rd], &endoftapemark, (long)TP_BSIZE);
	}
	blkcnt = 0;
	memcpy(buf, &tapebuf[(blkcnt++ * TP_BSIZE)], (long)TP_BSIZE);
	blksread++;
	tpblksread++;
}

static void
findtapeblksize()
{
	long i;

	for (i = 0; i < ntrec; i++)
		((struct s_spcl *)&tapebuf[i * TP_BSIZE])->c_magic = 0;
	blkcnt = 0;
#ifdef RRESTORE
	if (host)
		i = rmtread(tapebuf, ntrec * TP_BSIZE);
	else
#endif
		i = read(mt, tapebuf, ntrec * TP_BSIZE);

	if (i <= 0)
		err(1, "tape read error");
	if (i % TP_BSIZE != 0) {
		errx(1,
		    "Tape block size (%ld) is not a multiple of dump block size (%d)",
		    i, TP_BSIZE);
	}
	ntrec = i / TP_BSIZE;
	numtrec = ntrec;
	Vprintf(stdout, "Tape block size is %ld\n", ntrec);
}

void
closemt()
{

	if (mt < 0)
		return;
#ifdef RRESTORE
	if (host)
		rmtclose();
	else
#endif
		(void)close(mt);
}

/*
 * Read the next block from the tape.
 * Check to see if it is one of several vintage headers.
 * If it is an old style header, convert it to a new style header.
 * If it is not any valid header, return an error.
 */
static int
gethead(buf)
	struct s_spcl *buf;
{
	long i;
	union {
		quad_t	qval;
		int32_t	val[2];
	} qcvt;
	union u_ospcl {
		char dummy[TP_BSIZE];
		struct	s_ospcl {
			int32_t   c_type;
			int32_t   c_date;
			int32_t   c_ddate;
			int32_t   c_volume;
			int32_t   c_tapea;
			u_int16_t c_inumber;
			int32_t   c_magic;
			int32_t   c_checksum;
			struct odinode {
				unsigned short odi_mode;
				u_int16_t odi_nlink;
				u_int16_t odi_uid;
				u_int16_t odi_gid;
				int32_t   odi_size;
				int32_t   odi_rdev;
				char	  odi_addr[36];
				int32_t   odi_atime;
				int32_t   odi_mtime;
				int32_t   odi_ctime;
			} c_dinode;
			int32_t c_count;
			char	c_addr[256];
		} s_ospcl;
	} u_ospcl;

	if (!cvtflag) {
		readtape((char *)buf);
		if (buf->c_magic != NFS_MAGIC) {
			if (swabl(buf->c_magic) != NFS_MAGIC)
				return (FAIL);
			if (!Bcvt) {
				Vprintf(stdout, "Note: Doing Byte swapping\n");
				Bcvt = 1;
			}
		}
		if (checksum((int *)buf) == FAIL)
			return (FAIL);
		if (Bcvt)
			swabst((u_char *)"8l4s31l528b1l192b2l", (u_char *)buf);
		goto good;
	}
	readtape((char *)(&u_ospcl.s_ospcl));
	memset(buf, 0, (long)TP_BSIZE);
	buf->c_type = u_ospcl.s_ospcl.c_type;
	buf->c_date = u_ospcl.s_ospcl.c_date;
	buf->c_ddate = u_ospcl.s_ospcl.c_ddate;
	buf->c_volume = u_ospcl.s_ospcl.c_volume;
	buf->c_tapea = u_ospcl.s_ospcl.c_tapea;
	buf->c_inumber = u_ospcl.s_ospcl.c_inumber;
	buf->c_checksum = u_ospcl.s_ospcl.c_checksum;
	buf->c_magic = u_ospcl.s_ospcl.c_magic;
	buf->c_dinode.di_mode = u_ospcl.s_ospcl.c_dinode.odi_mode;
	buf->c_dinode.di_nlink = u_ospcl.s_ospcl.c_dinode.odi_nlink;
	buf->c_dinode.di_uid = u_ospcl.s_ospcl.c_dinode.odi_uid;
	buf->c_dinode.di_gid = u_ospcl.s_ospcl.c_dinode.odi_gid;
	buf->c_dinode.di_size = u_ospcl.s_ospcl.c_dinode.odi_size;
	buf->c_dinode.di_rdev = u_ospcl.s_ospcl.c_dinode.odi_rdev;
	buf->c_dinode.di_atime = u_ospcl.s_ospcl.c_dinode.odi_atime;
	buf->c_dinode.di_mtime = u_ospcl.s_ospcl.c_dinode.odi_mtime;
	buf->c_dinode.di_ctime = u_ospcl.s_ospcl.c_dinode.odi_ctime;
	buf->c_count = u_ospcl.s_ospcl.c_count;
	memcpy(buf->c_addr, u_ospcl.s_ospcl.c_addr, (long)256);
	if (u_ospcl.s_ospcl.c_magic != OFS_MAGIC ||
	    checksum((int *)(&u_ospcl.s_ospcl)) == FAIL)
		return(FAIL);
	buf->c_magic = NFS_MAGIC;

good:
	if ((buf->c_dinode.di_size == 0 || buf->c_dinode.di_size > 0xfffffff) &&
	    (buf->c_dinode.di_mode & IFMT) == IFDIR && Qcvt == 0) {
		qcvt.qval = buf->c_dinode.di_size;
		if (qcvt.val[0] || qcvt.val[1]) {
			printf("Note: Doing Quad swapping\n");
			Qcvt = 1;
		}
	}
	if (Qcvt) {
		qcvt.qval = buf->c_dinode.di_size;
		i = qcvt.val[1];
		qcvt.val[1] = qcvt.val[0];
		qcvt.val[0] = i;
		buf->c_dinode.di_size = qcvt.qval;
	}

	switch (buf->c_type) {

	case TS_CLRI:
	case TS_BITS:
		/*
		 * Have to patch up missing information in bit map headers
		 */
		buf->c_inumber = 0;
		buf->c_dinode.di_size = buf->c_count * TP_BSIZE;
		break;

	case TS_TAPE:
		if ((buf->c_flags & DR_NEWINODEFMT) == 0)
			oldinofmt = 1;
		/* fall through */
	case TS_END:
		buf->c_inumber = 0;
		break;

	case TS_INODE:
	case TS_ADDR:
		break;

	default:
		panic("gethead: unknown inode type %d\n", buf->c_type);
		break;
	}
	/*
	 * If we are restoring a filesystem with old format inodes,
	 * copy the uid/gid to the new location.
	 */
	if (oldinofmt) {
		buf->c_dinode.di_uid = buf->c_dinode.di_ouid;
		buf->c_dinode.di_gid = buf->c_dinode.di_ogid;
	}
	if (dflag)
		accthdr(buf);
	return(GOOD);
}

/*
 * Check that a header is where it belongs and predict the next header
 */
static void
accthdr(header)
	struct s_spcl *header;
{
	static ino_t previno = 0x7fffffff;
	static int prevtype;
	static long predict;
	long blks, i;

	if (header->c_type == TS_TAPE) {
		fprintf(stderr, "Volume header (%s inode format) ",
		    oldinofmt ? "old" : "new");
 		if (header->c_firstrec)
 			fprintf(stderr, "begins with record %d",
 				header->c_firstrec);
 		fprintf(stderr, "\n");
		previno = 0x7fffffff;
		return;
	}
	if (previno == 0x7fffffff)
		goto newcalc;
	switch (prevtype) {
	case TS_BITS:
		fprintf(stderr, "Dumped inodes map header");
		break;
	case TS_CLRI:
		fprintf(stderr, "Used inodes map header");
		break;
	case TS_INODE:
		fprintf(stderr, "File header, ino %d", previno);
		break;
	case TS_ADDR:
		fprintf(stderr, "File continuation header, ino %d", previno);
		break;
	case TS_END:
		fprintf(stderr, "End of tape header");
		break;
	}
	if (predict != blksread - 1)
		fprintf(stderr, "; predicted %ld blocks, got %ld blocks",
			predict, blksread - 1);
	fprintf(stderr, "\n");
newcalc:
	blks = 0;
	switch (header->c_type) {

	case TS_BITS:
	case TS_CLRI:
		blks = header->c_count;
		break;

	case TS_END:
		break;

	default:
		for (i = 0; i < header->c_count; i++)
			if (header->c_addr[i] != 0)
				blks++;
	}
	predict = blks;
	blksread = 0;
	prevtype = header->c_type;
	previno = header->c_inumber;
}

/*
 * Find an inode header.
 * Complain if had to skip, and complain is set.
 */
static void
findinode(header)
	struct s_spcl *header;
{
	static long skipcnt = 0;
	long i;
	char buf[TP_BSIZE];

	curfile.name = "<name unknown>";
	curfile.action = UNKNOWN;
	curfile.dip = NULL;
	curfile.ino = 0;
	do {
		if (header->c_magic != NFS_MAGIC) {
			skipcnt++;
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
		}
		switch (header->c_type) {

		case TS_ADDR:
			/*
			 * Skip up to the beginning of the next record
			 */
			for (i = 0; i < header->c_count; i++)
				if (header->c_addr[i])
					readtape(buf);
			while (gethead(header) == FAIL ||
			    header->c_date != dumpdate)
				skipcnt++;
			break;

		case TS_INODE:
			curfile.dip = &header->c_dinode;
			curfile.ino = header->c_inumber;
			break;

		case TS_END:
			curfile.ino = maxino;
			break;

		case TS_CLRI:
			curfile.name = "<file removal list>";
			break;

		case TS_BITS:
			curfile.name = "<file dump list>";
			break;

		case TS_TAPE:
			panic("unexpected tape header\n");
			/* NOTREACHED */

		default:
			panic("unknown tape header type %d\n", spcl.c_type);
			/* NOTREACHED */

		}
	} while (header->c_type == TS_ADDR);
	if (skipcnt > 0)
		fprintf(stderr, "resync restore, skipped %ld blocks\n", skipcnt);
	skipcnt = 0;
}

static int
checksum(buf)
	int *buf;
{
	int i, j;

	j = sizeof(union u_spcl) / sizeof(int);
	i = 0;
	if(!Bcvt) {
		do
			i += *buf++;
		while (--j);
	} else {
		/* What happens if we want to read restore tapes
			for a 16bit int machine??? */
		do
			i += swabl(*buf++);
		while (--j);
	}

	if (i != CHECKSUM) {
		fprintf(stderr, "Checksum error %o, inode %d file %s\n", i,
			curfile.ino, curfile.name);
		return(FAIL);
	}
	return(GOOD);
}

#ifdef RRESTORE
#include <stdarg.h>

void
msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif /* RRESTORE */

static u_char *
swabshort(sp, n)
	u_char *sp;
	int n;
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[1]; sp[1] = c;
		sp += 2;
	}
	return (sp);
}

static u_char *
swablong(sp, n)
	u_char *sp;
	int n;
{
	char c;

	while (--n >= 0) {
		c = sp[0]; sp[0] = sp[3]; sp[3] = c;
		c = sp[2]; sp[2] = sp[1]; sp[1] = c;
		sp += 4;
	}
	return (sp);
}

void
swabst(cp, sp)
	u_char *cp, *sp;
{
	int n = 0;

	while (*cp) {
		switch (*cp) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = (n * 10) + (*cp++ - '0');
			continue;

		case 's': case 'w': case 'h':
			if (n == 0)
				n = 1;
			sp = swabshort(sp, n);
			break;

		case 'l':
			if (n == 0)
				n = 1;
			sp = swablong(sp, n);
			break;

		default: /* Any other character, like 'b' counts as byte. */
			if (n == 0)
				n = 1;
			sp += n;
			break;
		}
		cp++;
		n = 0;
	}
}

static u_long
swabl(x)
	u_long x;
{
	swabst((u_char *)"l", (u_char *)&x);
	return (x);
}
