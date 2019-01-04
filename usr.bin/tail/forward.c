/*	$OpenBSD: forward.c,v 1.32 2019/01/04 15:04:28 martijn Exp $	*/
/*	$NetBSD: forward.c,v 1.7 1996/02/13 16:49:10 ghudson Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/event.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static int rlines(struct tailfile *, off_t);
static inline void tfprint(FILE *fp);
static int tfqueue(struct tailfile *tf);
static const struct timespec *tfreopen(struct tailfile *tf);

static int kq = -1;

/*
 * forward -- display the file, from an offset, forward.
 *
 * There are eight separate cases for this -- regular and non-regular
 * files, by bytes or lines and from the beginning or end of the file.
 *
 * FBYTES	byte offset from the beginning of the file
 *	REG	seek
 *	NOREG	read, counting bytes
 *
 * FLINES	line offset from the beginning of the file
 *	REG	read, counting lines
 *	NOREG	read, counting lines
 *
 * RBYTES	byte offset from the end of the file
 *	REG	seek
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * RLINES
 *	REG	step back until the correct offset is reached.
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 */
void
forward(struct tailfile *tf, int nfiles, enum STYLE style, off_t origoff)
{
	int ch;
	struct tailfile *ctf, *ltf;
	struct kevent ke;
	const struct timespec *ts = NULL;
	int i;
	int nevents;

	if (nfiles < 1)
		return;

	if (fflag && (kq = kqueue()) < 0)
		warn("kqueue");

	for (i = 0; i < nfiles; i++) {
		off_t off = origoff;
		if (nfiles > 1)
			printfname(tf[i].fname);

		switch(style) {
		case FBYTES:
			if (off == 0)
				break;
			if (S_ISREG(tf[i].sb.st_mode)) {
				if (tf[i].sb.st_size < off)
					off = tf[i].sb.st_size;
				if (fseeko(tf[i].fp, off, SEEK_SET) == -1) {
					ierr(tf[i].fname);
					return;
				}
			} else while (off--)
				if ((ch = getc(tf[i].fp)) == EOF) {
					if (ferror(tf[i].fp)) {
						ierr(tf[i].fname);
						return;
					}
					break;
				}
			break;
		case FLINES:
			if (off == 0)
				break;
			for (;;) {
				if ((ch = getc(tf[i].fp)) == EOF) {
					if (ferror(tf[i].fp)) {
						ierr(tf[i].fname);
						return;
					}
					break;
				}
				if (ch == '\n' && !--off)
					break;
			}
			break;
		case RBYTES:
			if (S_ISREG(tf[i].sb.st_mode)) {
				if (tf[i].sb.st_size >= off &&
				    fseeko(tf[i].fp, -off, SEEK_END) == -1) {
					ierr(tf[i].fname);
					return;
				}
			} else if (off == 0) {
				while (getc(tf[i].fp) != EOF)
					;
				if (ferror(tf[i].fp)) {
					ierr(tf[i].fname);
					return;
				}
			} else {
				if (bytes(&(tf[i]), off))
					return;
			}
			break;
		case RLINES:
			if (S_ISREG(tf[i].sb.st_mode)) {
				if (!off) {
					if (fseeko(tf[i].fp, (off_t)0,
					    SEEK_END) == -1) {
						ierr(tf[i].fname);
						return;
					}
				} else if (rlines(&(tf[i]), off) != 0)
					lines(&(tf[i]), off);
			} else if (off == 0) {
				while (getc(tf[i].fp) != EOF)
					;
				if (ferror(tf[i].fp)) {
					ierr(tf[i].fname);
					return;
				}
			} else {
				if (lines(&(tf[i]), off))
					return;
			}
			break;
		default:
			err(1, "Unsupported style");
		}

		tfprint(tf[i].fp);
		if (fflag && tfqueue(&(tf[i])) == -1)
			warn("Unable to follow %s", tf[i].fname);

	}
	ltf = &(tf[i-1]);

	(void)fflush(stdout);
	if (!fflag || kq < 0)
		return;

	while (1) {
		if ((nevents = kevent(kq, NULL, 0, &ke, 1, ts)) <= 0) {
			if (errno == EINTR) {
				close(kq);
				return;
			}
		}

		ctf = ke.udata;
		if (nevents > 0) {
			if (ke.filter == EVFILT_READ) {
				if (ctf != ltf) {
					printfname(ctf->fname);
					ltf = ctf;
				}
				clearerr(ctf->fp);
				tfprint(ctf->fp);
				if (ferror(ctf->fp)) {
					ierr(ctf->fname);
					fclose(ctf->fp);
					warn("Lost file %s", ctf->fname);
					continue;
				}
				(void)fflush(stdout);
				clearerr(ctf->fp);
			} else if (ke.filter == EVFILT_VNODE) {
				if (ke.fflags & (NOTE_DELETE | NOTE_RENAME)) {
					/*
					 * File was deleted or renamed.
					 *
					 * Continue to look at it until
					 * a new file reappears with
					 * the same name. 
					 */
					(void) tfreopen(ctf);
				} else if (ke.fflags & NOTE_TRUNCATE) {
					warnx("%s has been truncated, "
					    "resetting.", ctf->fname);
					fpurge(ctf->fp);
					rewind(ctf->fp);
				}
			}
		}
		ts = tfreopen(NULL);
	}
}

/*
 * rlines -- display the last offset lines of the file.
 */
static int
rlines(struct tailfile *tf, off_t off)
{
	off_t pos;
	int ch;

	pos = tf->sb.st_size;
	if (pos == 0)
		return (0);

	/*
	 * Position before char.
	 * Last char is special, ignore it whether newline or not.
	 */
	pos -= 2;
	ch = EOF;
	for (; off > 0 && pos >= 0; pos--) {
		/* A seek per char isn't a problem with a smart stdio */
		if (fseeko(tf[0].fp, pos, SEEK_SET) == -1) {
			ierr(tf->fname);
			return (1);
		}
		if ((ch = getc(tf[0].fp)) == '\n')
			off--;
		else if (ch == EOF) {
			if (ferror(tf[0].fp)) {
				ierr(tf->fname);
				return (1);
			}
			break;
		}
	}
	/* If we read until start of file, put back last read char */
	if (pos < 0 && off > 0 && ch != EOF && ungetc(ch, tf[0].fp) == EOF) {
		ierr(tf->fname);
		return (1);
	}

	while (!feof(tf[0].fp) && (ch = getc(tf[0].fp)) != EOF)
		if (putchar(ch) == EOF)
			oerr();
	if (ferror(tf[0].fp)) {
		ierr(tf->fname);
		return (1);
	}

	return (0);
}

static inline void
tfprint(FILE *fp)
{
	int ch;

	while (!feof(fp) && (ch = getc(fp)) != EOF)
		if (putchar(ch) == EOF)
			oerr();
}

static int
tfqueue(struct tailfile *tf)
{
	struct kevent ke[2];
	int i = 1;

	if (kq < 0) {
		errno = EBADF;
		return -1;
	}

	EV_SET(&(ke[0]), fileno(tf->fp), EVFILT_READ,
	    EV_ENABLE | EV_ADD | EV_CLEAR, 0, 0, tf);

	if (S_ISREG(tf->sb.st_mode)) {
		i = 2;
		EV_SET(&(ke[1]), fileno(tf->fp), EVFILT_VNODE,
		    EV_ENABLE | EV_ADD | EV_CLEAR,
		    NOTE_DELETE | NOTE_RENAME | NOTE_TRUNCATE,
		    0, tf);
	}
	if (kevent(kq, ke, i, NULL, 0, NULL) < 0) {
		ierr(tf->fname);
		return -1;
	}
	return 0;
}

#define AFILESINCR 8
static const struct timespec *
tfreopen(struct tailfile *tf) {
	static struct tailfile		**reopen = NULL;
	static int			  nfiles = 0, afiles = 0;
	static const struct timespec	  ts = {1, 0};

	struct stat			  sb;
	struct tailfile			**treopen, *ttf;
	int				  i;

	if (tf && !(tf->fp == stdin) &&
	    ((stat(tf->fname, &sb) != 0) || sb.st_ino != tf->sb.st_ino)) {
		if (afiles < ++nfiles) {
			afiles += AFILESINCR;
			treopen = reallocarray(reopen, afiles, sizeof(*reopen));
			if (treopen)
				reopen = treopen;
			else
				afiles -= AFILESINCR;
		}
		if (nfiles <= afiles) {
			for (i = 0; i < nfiles - 1; i++)
				if (strcmp(reopen[i]->fname, tf->fname) == 0)
					break;
			if (i < nfiles - 1)
				nfiles--;
			else
				reopen[nfiles-1] = tf;
		} else {
			warnx("Lost track of %s", tf->fname);
			nfiles--;
		}
	}

	for (i = 0; i < nfiles; i++) {
		ttf = reopen[i];
		if (stat(ttf->fname, &sb) == -1)
			continue;
		if (sb.st_ino != ttf->sb.st_ino) {
			(void) memcpy(&(ttf->sb), &sb, sizeof(ttf->sb));
			ttf->fp = freopen(ttf->fname, "r", ttf->fp);
			if (ttf->fp == NULL)
				ierr(ttf->fname);
			else {
				warnx("%s has been replaced, reopening.",
				    ttf->fname);
				tfqueue(ttf);
			}
		}
		reopen[i] = reopen[--nfiles];
	}

	return nfiles ? &ts : NULL;
}
