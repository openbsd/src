/*	$OpenBSD: mio_rmidi.c,v 1.15 2015/02/16 06:07:56 ratchov Exp $	*/
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

#include "debug.h"
#include "mio_priv.h"

struct mio_rmidi_hdl {
	struct mio_hdl mio;
	int fd;
};

static void mio_rmidi_close(struct mio_hdl *);
static size_t mio_rmidi_read(struct mio_hdl *, void *, size_t);
static size_t mio_rmidi_write(struct mio_hdl *, const void *, size_t);
static int mio_rmidi_nfds(struct mio_hdl *);
static int mio_rmidi_pollfd(struct mio_hdl *, struct pollfd *, int);
static int mio_rmidi_revents(struct mio_hdl *, struct pollfd *);

static struct mio_ops mio_rmidi_ops = {
	mio_rmidi_close,
	mio_rmidi_write,
	mio_rmidi_read,
	mio_rmidi_nfds,
	mio_rmidi_pollfd,
	mio_rmidi_revents
};

struct mio_hdl *
_mio_rmidi_open(const char *str, unsigned int mode, int nbio)
{
	int fd, flags;
	struct mio_rmidi_hdl *hdl;
	char path[PATH_MAX];

	switch (*str) {
	case '/':
		str++;
		break;
	default:
		DPRINTF("_sio_sun_open: %s: '/<devnum>' expected\n", str);
		return NULL;
	}
	hdl = malloc(sizeof(struct mio_rmidi_hdl));
	if (hdl == NULL)
		return NULL;
	_mio_create(&hdl->mio, &mio_rmidi_ops, mode, nbio);

	snprintf(path, sizeof(path), "/dev/rmidi%s", str);
	if (mode == (MIO_OUT | MIO_IN))
		flags = O_RDWR;
	else
		flags = (mode & MIO_OUT) ? O_WRONLY : O_RDONLY;
	while ((fd = open(path, flags | O_NONBLOCK | O_CLOEXEC)) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR(path);
		goto bad_free;
	}
	hdl->fd = fd;
	return (struct mio_hdl *)hdl;
 bad_free:
	free(hdl);
	return NULL;
}

static void
mio_rmidi_close(struct mio_hdl *sh)
{
	struct mio_rmidi_hdl *hdl = (struct mio_rmidi_hdl *)sh;
	int rc;

	do {
		rc = close(hdl->fd);
	} while (rc < 0 && errno == EINTR);
	free(hdl);
}

static size_t
mio_rmidi_read(struct mio_hdl *sh, void *buf, size_t len)
{
	struct mio_rmidi_hdl *hdl = (struct mio_rmidi_hdl *)sh;
	ssize_t n;

	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("mio_rmidi_read: read");
			hdl->mio.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("mio_rmidi_read: eof\n");
		hdl->mio.eof = 1;
		return 0;
	}
	return n;
}

static size_t
mio_rmidi_write(struct mio_hdl *sh, const void *buf, size_t len)
{
	struct mio_rmidi_hdl *hdl = (struct mio_rmidi_hdl *)sh;
	ssize_t n;

	while ((n = write(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("mio_rmidi_write: write");
			hdl->mio.eof = 1;
		}
 		return 0;
	}
	return n;
}

static int
mio_rmidi_nfds(struct mio_hdl *sh)
{
	return 1;
}

static int
mio_rmidi_pollfd(struct mio_hdl *sh, struct pollfd *pfd, int events)
{
	struct mio_rmidi_hdl *hdl = (struct mio_rmidi_hdl *)sh;

	pfd->fd = hdl->fd;
	pfd->events = events;
	return 1;
}

static int
mio_rmidi_revents(struct mio_hdl *sh, struct pollfd *pfd)
{
	return pfd->revents;
}
