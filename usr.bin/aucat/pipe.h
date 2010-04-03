/*	$OpenBSD: pipe.h,v 1.4 2010/04/03 17:59:17 ratchov Exp $	*/
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
#ifndef PIPE_H
#define PIPE_H

#include "file.h"

struct pipe {
	struct file file;
	int fd;			/* file descriptor */
};

extern struct fileops pipe_ops;

struct pipe *pipe_new(struct fileops *, int, char *);
void pipe_close(struct file *);
unsigned pipe_read(struct file *, unsigned char *, unsigned);
unsigned pipe_write(struct file *, unsigned char *, unsigned);
int pipe_nfds(struct file *);
int pipe_pollfd(struct file *, struct pollfd *, int);
int pipe_revents(struct file *, struct pollfd *);

#endif /* !defined(PIPE_H) */
