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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "pipe.h"
#ifdef DEBUG
#include "dbg.h"
#endif

struct fileops pipe_ops = {
	"pipe",
	sizeof(struct pipe),
	pipe_close,
	pipe_read,
	pipe_write,
	NULL, /* start */
	NULL, /* stop */
	pipe_nfds,
	pipe_pollfd,
	pipe_revents
};

struct pipe *
pipe_new(struct fileops *ops, int fd, char *name)
{
	struct pipe *f;

	f = (struct pipe *)file_new(ops, name, 1);
	if (f == NULL)
		return NULL;
	f->fd = fd;
	return f;
}

unsigned
pipe_read(struct file *file, unsigned char *data, unsigned count)
{
	struct pipe *f = (struct pipe *)file;
	int n;
	
	while ((n = read(f->fd, data, count)) < 0) {
		f->file.state &= ~FILE_ROK;
		if (errno == EAGAIN) {
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": reading blocked\n");
			}
#endif
		} else {
			warn("%s", f->file.name);
			file_eof(&f->file);
		}
		return 0;
	}
	if (n == 0) {
		f->file.state &= ~FILE_ROK; /* XXX: already cleared in file_eof */
		file_eof(&f->file);
		return 0;
	}
	return n;
}


unsigned
pipe_write(struct file *file, unsigned char *data, unsigned count)
{
	struct pipe *f = (struct pipe *)file;
	int n;

	while ((n = write(f->fd, data, count)) < 0) {
		f->file.state &= ~FILE_WOK;
		if (errno == EAGAIN) {
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": writing blocked\n");
			}
#endif
		} else {
			if (errno != EPIPE)
				warn("%s", f->file.name);
			file_hup(&f->file);
		}
		return 0;
	}
	return n;
}

int
pipe_nfds(struct file *file) {
	return 1;
}

int
pipe_pollfd(struct file *file, struct pollfd *pfd, int events)
{
	struct pipe *f = (struct pipe *)file;

	pfd->fd = f->fd;
	pfd->events = events;
	return (events != 0) ? 1 : 0;
}

int
pipe_revents(struct file *f, struct pollfd *pfd)
{
	return pfd->revents;
}

void
pipe_close(struct file *file)
{
	struct pipe *f = (struct pipe *)file;

	close(f->fd);
}

off_t
pipe_endpos(struct file *file)
{
	struct pipe *f = (struct pipe *)file;
	off_t pos;

	pos = lseek(f->fd, 0, SEEK_END);
	if (pos < 0) {
#ifdef DEBUG
		file_dbg(&f->file);
		dbg_puts(": couldn't get file size\n");
#endif
		return 0;
	}
	return pos;
}

int
pipe_seek(struct file *file, off_t pos)
{
	struct pipe *f = (struct pipe *)file;
	off_t newpos;
	
	newpos = lseek(f->fd, pos, SEEK_SET);
	if (newpos < 0) {
#ifdef DEBUG
		file_dbg(&f->file);
		dbg_puts(": couldn't seek\n");
#endif
		/* XXX: call eof() */
		return 0;
	}
	return 1;
}

int
pipe_trunc(struct file *file, off_t pos)
{
	struct pipe *f = (struct pipe *)file;

	if (ftruncate(f->fd, pos) < 0) {
#ifdef DEBUG
		file_dbg(&f->file);
		dbg_puts(": couldn't truncate file\n");
#endif
		/* XXX: call hup() */
		return 0;
	}
	return 1;
}
