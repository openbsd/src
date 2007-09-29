/*	$OpenBSD: forward.c,v 1.22 2007/09/29 12:31:28 otto Exp $	*/
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
static char rcsid[] = "$OpenBSD: forward.c,v 1.22 2007/09/29 12:31:28 otto Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/event.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
	struct stat nsb;
	int kq, queue;
	struct kevent ke;

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

	kq = -1;
kq_retry:
	if (fflag && ((kq = kqueue()) >= 0)) {
		ke.ident = fileno(fp);
		ke.flags = EV_ENABLE|EV_ADD|EV_CLEAR;
		ke.filter = EVFILT_READ;
		ke.fflags = ke.data = 0;
		ke.udata = NULL;
		if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0) {
			close(kq);
			kq = -1;
		} else if (S_ISREG(sbp->st_mode)) {
			ke.ident = fileno(fp);
			ke.flags = EV_ENABLE|EV_ADD|EV_CLEAR;
			ke.filter = EVFILT_VNODE;
			ke.fflags = NOTE_DELETE | NOTE_RENAME | NOTE_TRUNCATE;
			if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0) {
				close(kq);
				kq = -1;
			}
		}
	}

	for (;;) {
		while (!feof(fp) && (ch = getc(fp)) != EOF)
			if (putchar(ch) == EOF)
				oerr();
		if (ferror(fp)) {
			ierr();
			if (kq != -1)
				close(kq);
			return;
		}
		(void)fflush(stdout);
		if (!fflag)
			break;
		clearerr(fp);
		queue = 1;
		if (kq < 0 || kevent(kq, NULL, 0, &ke, 1, NULL) <= 0) {
			queue = 0;
			sleep(1);
		} else if (ke.filter == EVFILT_READ) {
			continue;
		} else if ((ke.fflags & NOTE_TRUNCATE) == 0) {
			/*
			 * File was renamed or deleted.
			 *
			 * Continue to look at it until a new file reappears
			 * with the same name. 
			 * Fall back to the old algorithm for that.
			 */
			close(kq);
			kq = -1;
		}

		if (is_stdin || stat(fname, &nsb) != 0)
			continue;
		/* Reopen file if the inode changes or file was truncated */
		if (nsb.st_ino != sbp->st_ino) {
			warnx("%s has been replaced, reopening.", fname);
			if ((fp = freopen(fname, "r", fp)) == NULL) {
				ierr();
				if (kq >= 0)
					close(kq);
				return;
			}
			(void)memcpy(sbp, &nsb, sizeof(nsb));
			goto kq_retry;
		} else if ((queue && (ke.fflags & NOTE_TRUNCATE)) ||
		    (!queue && nsb.st_size < sbp->st_size)) {
			warnx("%s has been truncated, resetting.", fname);
			fpurge(fp);
			rewind(fp);
		}
		(void)memcpy(sbp, &nsb, sizeof(nsb));
	}
	if (kq >= 0)
		close(kq);
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
