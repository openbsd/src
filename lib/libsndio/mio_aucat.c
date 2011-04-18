/*	$OpenBSD: mio_aucat.c,v 1.4 2011/04/18 23:57:35 ratchov Exp $	*/
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
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aucat.h"
#include "debug.h"
#include "mio_priv.h"

struct mio_aucat_hdl {
	struct mio_hdl mio;
	struct aucat aucat;
	int events;
};

static void mio_aucat_close(struct mio_hdl *);
static size_t mio_aucat_read(struct mio_hdl *, void *, size_t);
static size_t mio_aucat_write(struct mio_hdl *, const void *, size_t);
static int mio_aucat_pollfd(struct mio_hdl *, struct pollfd *, int);
static int mio_aucat_revents(struct mio_hdl *, struct pollfd *);

static struct mio_ops mio_aucat_ops = {
	mio_aucat_close,
	mio_aucat_write,
	mio_aucat_read,
	mio_aucat_pollfd,
	mio_aucat_revents,
};

static struct mio_hdl *
mio_xxx_open(const char *str, unsigned mode, int nbio, int isaudio)
{
	struct mio_aucat_hdl *hdl;

	hdl = malloc(sizeof(struct mio_aucat_hdl));
	if (hdl == NULL)
		return NULL;
	if (!aucat_open(&hdl->aucat, str, mode, isaudio))
		goto bad;
	mio_create(&hdl->mio, &mio_aucat_ops, mode, nbio);
	if (!aucat_setfl(&hdl->aucat, nbio, &hdl->mio.eof))
		goto bad;
	return (struct mio_hdl *)hdl;
bad:
	free(hdl);
	return NULL;
}

struct mio_hdl *
mio_midithru_open(const char *str, unsigned mode, int nbio)
{
	return mio_xxx_open(str, mode, nbio, 0);
}

struct mio_hdl *
mio_aucat_open(const char *str, unsigned mode, int nbio)
{
	return mio_xxx_open(str, mode, nbio, 1);
}

static void
mio_aucat_close(struct mio_hdl *sh)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;

	if (!hdl->mio.eof)
		aucat_setfl(&hdl->aucat, 0, &hdl->mio.eof);
	aucat_close(&hdl->aucat, hdl->mio.eof);
	free(hdl);
}

static size_t
mio_aucat_read(struct mio_hdl *sh, void *buf, size_t len)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;

	while (hdl->aucat.rstate == RSTATE_MSG) {
		if (!aucat_rmsg(&hdl->aucat, &hdl->mio.eof))
			return 0;
	}
	return aucat_rdata(&hdl->aucat, buf, len, &hdl->mio.eof);
}

static size_t
mio_aucat_write(struct mio_hdl *sh, const void *buf, size_t len)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;

	return aucat_wdata(&hdl->aucat, buf, len, 1, &hdl->mio.eof);
}

static int
mio_aucat_pollfd(struct mio_hdl *sh, struct pollfd *pfd, int events)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;

	hdl->events = events;
	return aucat_pollfd(&hdl->aucat, pfd, events);
}

static int
mio_aucat_revents(struct mio_hdl *sh, struct pollfd *pfd)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;
	int revents = pfd->revents;

	if (revents & POLLIN) {
		while (hdl->aucat.rstate == RSTATE_MSG) {
			if (!aucat_rmsg(&hdl->aucat, &hdl->mio.eof))
				break;
		}
		if (hdl->aucat.rstate != RSTATE_DATA)
			revents &= ~POLLIN;
	}
	if (hdl->mio.eof)
		return POLLHUP;
	return revents & (hdl->events | POLLHUP);
}
