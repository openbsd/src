/*	$OpenBSD: file.h,v 1.2 2015/07/17 09:51:18 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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

#include <sys/types.h>

struct file;
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
	int (*pollfd)(void *, struct pollfd *);
	int (*revents)(void *, struct pollfd *);
	/* 
	 * we have to handle POLLIN and POLLOUT events
	 * in separate handles, since handling POLLIN can
	 * close the file, and continuing (to handle POLLOUT)
	 * would make use of the free()'ed file structure
	 */
	void (*in)(void *);
	void (*out)(void *);
	void (*hup)(void *);
};

struct file {
	struct file *next;		/* next in file_list */
	struct pollfd *pfd;		/* arg to poll(2) syscall */
	struct fileops *ops;		/* event handlers */
	void *arg;			/* argument to event handlers */
#define FILE_INIT	0		/* ready */
#define FILE_ZOMB	1		/* closed, but not free()d yet */
	unsigned int state;		/* one of above */
	unsigned int max_nfds;		/* max number of descriptors */	
	char *name;			/* for debug purposes */
};

extern struct file *file_list;
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

struct file *file_new(struct fileops *, void *, char *, unsigned int);
void file_del(struct file *);
void file_log(struct file *);

int file_poll(void);

#endif /* !defined(FILE_H) */
