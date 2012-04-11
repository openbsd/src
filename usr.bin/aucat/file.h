/*	$OpenBSD: file.h,v 1.14 2012/04/11 06:05:43 ratchov Exp $	*/
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

struct file;
struct aproc;
struct pollfd;

struct timo {
	struct timo *next;
	unsigned int val;		/* time to wait before the callback */
	unsigned int set;		/* true if the timeout is set */
	void (*cb)(void *arg);		/* routine to call on expiration */
	void *arg;			/* argument to give to 'cb' */
};

struct fileops {
	char *name;
	size_t size;
	void (*close)(struct file *);
	unsigned int (*read)(struct file *, unsigned char *, unsigned int);
	unsigned int (*write)(struct file *, unsigned char *, unsigned int);
	void (*start)(struct file *, void (*)(void *, int), void *);
	void (*stop)(struct file *);
	int (*nfds)(struct file *);
	int (*pollfd)(struct file *, struct pollfd *, int);
	int (*revents)(struct file *, struct pollfd *);
};

struct file {
	struct fileops *ops;
	struct pollfd *pfd;		/* arg to poll(2) syscall */
#define FILE_ROK	0x1		/* file readable */
#define FILE_WOK	0x2		/* file writable */
#define FILE_EOF	0x4		/* eof on the read end */
#define FILE_HUP	0x8		/* hang-up on the write end */
#define FILE_ZOMB	0x10		/* closed, but struct not freed */
#define FILE_RINUSE	0x20		/* inside rproc->ops->in() */
#define FILE_WINUSE	0x40		/* inside wproc->ops->out() */
	unsigned int state;		/* one of above */
#ifdef DEBUG
#define FILE_MAXCYCLES	20
	unsigned int cycles;		/* number of POLLIN/POLLOUT events */
#endif
	char *name;			/* for debug purposes */
	struct aproc *rproc, *wproc;	/* reader and/or writer */
	LIST_ENTRY(file) entry;
};

LIST_HEAD(filelist,file);

extern struct filelist file_list;
extern int file_slowaccept;

#ifdef DEBUG
extern long long file_wtime, file_utime;
#endif

void timo_set(struct timo *, void (*)(void *), void *);
void timo_add(struct timo *, unsigned int);
void timo_del(struct timo *);

void filelist_init(void);
void filelist_done(void);
void filelist_unlisten(void);

struct file *file_new(struct fileops *, char *, unsigned int);
void file_del(struct file *);
void file_dbg(struct file *);

void file_attach(struct file *, struct aproc *, struct aproc *);
unsigned int file_read(struct file *, unsigned char *, unsigned int);
unsigned int file_write(struct file *, unsigned char *, unsigned int);
int file_poll(void);
void file_eof(struct file *);
void file_hup(struct file *);
void file_close(struct file *);

#endif /* !defined(FILE_H) */
