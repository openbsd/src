/*	$OpenBSD: forward.c,v 1.24 2008/10/17 11:38:20 landry Exp $	*/
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)forward.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: forward.c,v 1.24 2008/10/17 11:38:20 landry Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/event.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "extern.h"

static int rlines(FILE *, off_t, struct stat *);

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
forward(FILE *fp, enum STYLE style, off_t off, struct stat *sbp)
{
	int ch;

	switch(style) {
	case FBYTES:
		if (off == 0)
			break;
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size < off)
				off = sbp->st_size;
			if (fseeko(fp, off, SEEK_SET) == -1) {
				ierr();
				return;
			}
		} else while (off--)
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
		break;
	case FLINES:
		if (off == 0)
			break;
		for (;;) {
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
			if (ch == '\n' && !--off)
				break;
		}
		break;
	case RBYTES:
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size >= off &&
			    fseeko(fp, -off, SEEK_END) == -1) {
				ierr();
				return;
			}
		} else if (off == 0) {
			while (getc(fp) != EOF)
				;
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (bytes(fp, off))
				return;
		}
		break;
	case RLINES:
		if (S_ISREG(sbp->st_mode)) {
			if (!off) {
				if (fseeko(fp, (off_t)0, SEEK_END) == -1) {
					ierr();
					return;
				}
			} else if (rlines(fp, off, sbp) != 0)
				lines(fp, off);
		} else if (off == 0) {
			while (getc(fp) != EOF)
				;
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (lines(fp, off))
				return;
		}
		break;
	}

	while (!feof(fp) && (ch = getc(fp)) != EOF)
		if (putchar(ch) == EOF)
			oerr();
	if (ferror(fp)) {
		ierr();
		return;
	}
	(void)fflush(stdout);
}

struct file_info {
	FILE *fp;
	char *fname;
	struct stat fst;
};

/*
 * follow one or multiple files, i.e don't stop when end-of-file is reached,
 * but rather wait for additional data to be appended to the input.
 * this implements -f switch.
 */
void
follow(char **fnames, int nbfiles, enum STYLE style, off_t off)
{
	int ch, first, i;
	int kq;
	struct kevent ke;
	struct stat cst;
	FILE *fp;
	struct file_info *files;

	kq = -1;
	if ((kq = kqueue()) < 0)
		err(2, "kqueue() failed");

	if ((files = calloc(nbfiles, sizeof(struct file_info))) == NULL)
		err(1, "calloc() failed");

	for (first = 1, i = 0; (files[i].fname = *fnames++); i++) {
		if ((fp = fopen(files[i].fname, "r")) == NULL ||
		    fstat(fileno(fp), &(files[i].fst))) {
			warn("%s",files[i].fname);
			nbfiles--;
			i--;
			continue;
		}
		if (S_ISDIR(files[i].fst.st_mode)) {
			warnx("%s is a directory, skipping.",files[i].fname);
			nbfiles--;
			i--;
			continue;
		}
		files[i].fp = fp;
		if (nbfiles > 1) {
			(void)printf("%s==> %s <==\n",
			    first ? "" : "\n", files[i].fname);
			first = 0;
		}

		/* print from the given offset to the end */
		if (off != 0) {
			if (style == RBYTES) {
				if (S_ISREG(files[i].fst.st_mode)) {
					if (files[i].fst.st_size >= off &&
					    fseeko(fp, -off, SEEK_END) == -1) {
						ierr();
						goto cleanup;
					}
				}
				if (bytes(fp, off))
					goto cleanup;
			} else if (rlines(fp, off, &(files[i].fst)) != 0)
				lines(fp, off);
		}
		(void)fflush(stdout);

		/* one event to see if there is data to read */
		ke.ident = fileno(fp);
		EV_SET(&ke, fileno(fp), EVFILT_READ,
		    EV_ENABLE | EV_ADD | EV_CLEAR,
		    NULL, 0, NULL);
		if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
			goto cleanup;
		else if (S_ISREG(files[i].fst.st_mode)) {
			/* one event to detect if inode changed */
			EV_SET(&ke, fileno(fp), EVFILT_VNODE,
			    EV_ENABLE | EV_ADD | EV_CLEAR,
			    NOTE_DELETE | NOTE_RENAME | NOTE_TRUNCATE,
			    0, NULL);
			if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
				goto cleanup;
		}
	}

	/* no files to read */
	if (nbfiles == 0)
		goto cleanup;

	if (fp == NULL)
		fp = files[nbfiles - 1].fp;

	for (;;) {
		while (!feof(fp) && (ch = getc(fp)) != EOF)
			if (putchar(ch) == EOF)
				oerr();
		if (ferror(fp))
			goto cleanup;

		(void)fflush(stdout);
		clearerr(fp);
		/* give it a chance to fail.. */
		if (kevent(kq, NULL, 0, &ke, 1, NULL) <= 0) {
			sleep(1);
			continue;
		} else {
			/* an event occured on file #i */
			for (i = 0 ; i < nbfiles ; i++)
				if (fileno(files[i].fp) == ke.ident)
					break;

			/* EVFILT_READ event, check that it's on the current fp */
			if (ke.filter == EVFILT_READ) {
				if (fp != files[i].fp) {
					(void)printf("\n==> %s <==\n",files[i].fname);
					fp = files[i].fp;
					clearerr(fp);
				}
			/* EVFILT_VNODE event and File was renamed or deleted */
			} else if (ke.fflags & (NOTE_DELETE | NOTE_RENAME)) {
				/* file didn't reappear */
				if (stat(files[i].fname, &cst) != 0) {
					warnx("%s has been renamed or deleted.", files[i].fname);
					if (--nbfiles == 0)
						goto cleanup;
					/* overwrite with the latest file_info */
					fp = files[nbfiles].fp;
					(void)memcpy(&files[i], &files[nbfiles], sizeof(struct file_info));
				} else {
					/* Reopen file if the inode changed */
					if (cst.st_ino != files[i].fst.st_ino) {
						warnx("%s has been replaced, reopening.", files[i].fname);
						if ((fp = freopen(files[i].fname, "r", files[i].fp)) == NULL) {
							ierr();
							goto cleanup;
						}
						/*
						 * on freopen(), events corresponding to the fp
						 * were deleted from kqueue, we readd them
						*/
						ke.ident = fileno(fp);
						EV_SET(&ke, fileno(fp), EVFILT_READ,
						    EV_ENABLE | EV_ADD | EV_CLEAR,
						    NULL, 0, NULL);
						if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
							goto cleanup;
						else if (S_ISREG(files[i].fst.st_mode)) {
							EV_SET(&ke, fileno(fp), EVFILT_VNODE,
							    EV_ENABLE | EV_ADD | EV_CLEAR,
							    NOTE_DELETE | NOTE_RENAME | NOTE_TRUNCATE,
							    0, NULL);
							if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0)
								goto cleanup;
						}
						files[i].fp = fp;
					}
					(void)memcpy(&(files[i].fst), &cst, sizeof(cst));
				}
			} else if (ke.fflags & NOTE_TRUNCATE) {
				/* reset file if it was truncated */
				warnx("%s has been truncated, resetting.", files[i].fname);
				fpurge(files[i].fp);
				rewind(files[i].fp);
				continue;
			}
		}
	}

cleanup:
	if (kq >= 0)
		close(kq);
	free(files);
	return;
}

/*
 * rlines -- display the last offset lines of the file.
 */
static int
rlines(FILE *fp, off_t off, struct stat *sbp)
{
	off_t pos;
	int ch;

	pos = sbp->st_size;
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
		if (fseeko(fp, pos, SEEK_SET) == -1) {
			ierr();
			return (1);
		}
		if ((ch = getc(fp)) == '\n')
			off--;
		else if (ch == EOF) {
			if (ferror(fp)) {
				ierr();
				return (1);
			}
			break;
		}
	}
	/* If we read until start of file, put back last read char */
	if (pos < 0 && off > 0 && ch != EOF && ungetc(ch, fp) == EOF) {
		ierr();
		return (1);
	}

	while (!feof(fp) && (ch = getc(fp)) != EOF)
		if (putchar(ch) == EOF)
			oerr();
	if (ferror(fp)) {
		ierr();
		return (1);
	}

	return (0);
}
