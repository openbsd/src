/*	$OpenBSD: mio_rmidi.c,v 1.6 2010/04/24 06:15:54 ratchov Exp $	*/
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mio_priv.h"

#define RMIDI_PATH "/dev/rmidi0"

struct rmidi_hdl {
	struct mio_hdl mio;
	int fd;
};

static void rmidi_close(struct mio_hdl *);
static size_t rmidi_read(struct mio_hdl *, void *, size_t);
static size_t rmidi_write(struct mio_hdl *, const void *, size_t);
static int rmidi_pollfd(struct mio_hdl *, struct pollfd *, int);
static int rmidi_revents(struct mio_hdl *, struct pollfd *);

static struct mio_ops rmidi_ops = {
	rmidi_close,
	rmidi_write,
	rmidi_read,
	rmidi_pollfd,
	rmidi_revents,
};

struct mio_hdl *
mio_open_rmidi(const char *str, unsigned mode, int nbio)
{
	int fd, flags;
	struct rmidi_hdl *hdl;
	char path[PATH_MAX];

	hdl = malloc(sizeof(struct rmidi_hdl));
	if (hdl == NULL)
		return NULL;
	mio_create(&hdl->mio, &rmidi_ops, mode, nbio);

	snprintf(path, sizeof(path), "/dev/rmidi%s", str);
	if (mode == (MIO_OUT | MIO_IN))
		flags = O_RDWR;
	else
		flags = (mode & MIO_OUT) ? O_WRONLY : O_RDONLY;
	if (nbio)
		flags |= O_NONBLOCK;
	while ((fd = open(path, flags)) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR(path);
		goto bad_free;
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		DPERROR("FD_CLOEXEC");
		goto bad_close;
	}
	hdl->fd = fd;
	return (struct mio_hdl *)hdl;
 bad_close:
	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

static void
rmidi_close(struct mio_hdl *sh)
{
	struct rmidi_hdl *hdl = (struct rmidi_hdl *)sh;
	int rc;

	do {
		rc = close(hdl->fd);
	} while (rc < 0 && errno == EINTR);
	free(hdl);
}

static size_t
rmidi_read(struct mio_hdl *sh, void *buf, size_t len)
{
	struct rmidi_hdl *hdl = (struct rmidi_hdl *)sh;
	ssize_t n;

	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("rmidi_read: read");
			hdl->mio.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("rmidi_read: eof\n");
		hdl->mio.eof = 1;
		return 0;
	}
	return n;
}

static size_t
rmidi_write(struct mio_hdl *sh, const void *buf, size_t len)
{
	struct rmidi_hdl *hdl = (struct rmidi_hdl *)sh;
	ssize_t n;

	while ((n = write(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("rmidi_write: write");
			hdl->mio.eof = 1;
			return 0;
		}
 		return 0;
	}
	return n;
}

static int
rmidi_pollfd(struct mio_hdl *sh, struct pollfd *pfd, int events)
{
	struct rmidi_hdl *hdl = (struct rmidi_hdl *)sh;

	pfd->fd = hdl->fd;
	pfd->events = events;
	return 1;
}

static int
rmidi_revents(struct mio_hdl *sh, struct pollfd *pfd)
{
	return pfd->revents;
}
