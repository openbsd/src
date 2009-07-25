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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "pipe.h"

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
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (!(f->file.state & FILE_ROK)) {
		DPRINTF("pipe_read: %s: bad state\n", f->file.name);
		abort();
	}
	gettimeofday(&tv0, NULL);
#endif
	while ((n = read(f->fd, data, count)) < 0) {
		f->file.state &= ~FILE_ROK;
		if (errno == EAGAIN) {
			DPRINTFN(3, "pipe_read: %s: blocking...\n",
			    f->file.name);
		} else {
			warn("%s", f->file.name);
			file_eof(&f->file);
		}
		return 0;
	}
	if (n == 0) {
		DPRINTFN(2, "pipe_read: %s: eof\n", f->file.name);
		f->file.state &= ~FILE_ROK;
		file_eof(&f->file);
		return 0;
	}
#ifdef DEBUG
	gettimeofday(&tv1, NULL);
	timersub(&tv1, &tv0, &dtv);
	us = dtv.tv_sec * 1000000 + dtv.tv_usec;
	DPRINTFN(us < 5000 ? 4 : 2,
	    "pipe_read: %s: got %d bytes in %uus\n",
	    f->file.name, n, us);
#endif
	return n;
}


unsigned
pipe_write(struct file *file, unsigned char *data, unsigned count)
{
	struct pipe *f = (struct pipe *)file;
	int n;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (!(f->file.state & FILE_WOK)) {
		DPRINTF("pipe_write: %s: bad state\n", f->file.name);
		abort();
	}
	gettimeofday(&tv0, NULL);
#endif
	while ((n = write(f->fd, data, count)) < 0) {
		f->file.state &= ~FILE_WOK;
		if (errno == EAGAIN) {
			DPRINTFN(3, "pipe_write: %s: blocking...\n",
			    f->file.name);
		} else {
			if (errno != EPIPE)
				warn("%s", f->file.name);
			file_hup(&f->file);
		}
		return 0;
	}
#ifdef DEBUG
	gettimeofday(&tv1, NULL);
	timersub(&tv1, &tv0, &dtv);
	us = dtv.tv_sec * 1000000 + dtv.tv_usec;
	DPRINTFN(us < 5000 ? 4 : 2,
	    "pipe_write: %s: wrote %d bytes in %uus\n",
	    f->file.name, n, us);
#endif
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
