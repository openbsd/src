/*	$OpenBSD: file.h,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef FILE_H
#define FILE_H

#include <sys/queue.h>
#include <sys/types.h>

#include <poll.h>

struct aparams;
struct aproc;
struct abuf;

struct file {
	int fd;				/* file descriptor */
	struct pollfd *pfd;		/* arg to poll(2) syscall */
	off_t rbytes;			/* bytes to read, -1 if no limit */
	off_t wbytes;			/* bytes to write, -1 if no limit */
	int events;			/* events for poll(2) */
#define FILE_ROK	0x1		/* file readable */
#define FILE_WOK	0x2		/* file writable */
#define FILE_EOF	0x4		/* eof on the read end */
#define FILE_HUP	0x8		/* eof on the write end */
#define FILE_RFLOW	0x10		/* has flow control on read() */
#define FILE_WFLOW	0x20		/* has flow control on write() */
	int state;			/* one of above */
	char *name;			/* for debug purposes */
	struct aproc *rproc, *wproc;	/* reader and/or writer */
	LIST_ENTRY(file) entry;
};

LIST_HEAD(filelist,file);

extern struct filelist file_list;

void file_start(void);
void file_stop(void);
struct file *file_new(int, char *);
void file_del(struct file *);
void file_attach(struct file *, struct aproc *, struct aproc *);
unsigned file_read(struct file *, unsigned char *, unsigned);
unsigned file_write(struct file *, unsigned char *, unsigned);
int file_poll(void);
void file_eof(struct file *);
void file_hup(struct file *);

/*
 * max data of a .wav file. The total file size must be smaller than
 * 2^31, and we also have to leave some space for the headers (around 40
 * bytes)
 */ 
#define WAV_DATAMAX	(0x7fff0000)

int wav_readhdr(int, struct aparams *, off_t *);
int wav_writehdr(int, struct aparams *);

/* legacy */
int legacy_play(char *, char *);

#endif /* !defined(FILE_H) */
