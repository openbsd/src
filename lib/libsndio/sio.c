/*	$OpenBSD: sio.c,v 1.6 2011/05/09 17:34:14 ratchov Exp $	*/
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "sio_priv.h"

#define SIO_PAR_MAGIC	0x83b905a4

struct sio_backend {
	char *prefix;
	struct sio_hdl *(*open)(const char *, unsigned, int);
};

static struct sio_backend backends[] = {
	{ "aucat", sio_aucat_open },
	{ "sun", sio_sun_open },
	{ NULL, NULL }
};

void
sio_initpar(struct sio_par *par)
{
	memset(par, 0xff, sizeof(struct sio_par));
	par->__magic = SIO_PAR_MAGIC;
}

struct sio_hdl *
sio_open(const char *str, unsigned mode, int nbio)
{
	struct sio_backend *b;
	struct sio_hdl *hdl;
	char *sep;
	int len;

#ifdef DEBUG
	sndio_debug_init();
#endif
	if ((mode & (SIO_PLAY | SIO_REC)) == 0)
		return NULL;
	if (str == NULL && !issetugid())
		str = getenv("AUDIODEVICE");
	if (str == NULL) {
		for (b = backends; b->prefix != NULL; b++) {
			hdl = b->open(NULL, mode, nbio);
			if (hdl != NULL)
				return hdl;
		}
		return NULL;
	}
	sep = strchr(str, ':');
	if (sep == NULL) {
		DPRINTF("sio_open: %s: ':' missing in device name\n", str);
		return NULL;
	}
	len = sep - str;
	for (b = backends; b->prefix != NULL; b++) {
		if (strlen(b->prefix) == len && memcmp(b->prefix, str, len) == 0)
			return b->open(sep + 1, mode, nbio);
	}
	DPRINTF("sio_open: %s: unknown device type\n", str);
	return NULL;
}

void
sio_create(struct sio_hdl *hdl, struct sio_ops *ops, unsigned mode, int nbio)
{
	hdl->ops = ops;
	hdl->mode = mode;
	hdl->nbio = nbio;
	hdl->started = 0;
	hdl->eof = 0;
	hdl->move_cb = NULL;
	hdl->vol_cb = NULL;
}

void
sio_close(struct sio_hdl *hdl)
{
	hdl->ops->close(hdl);
}

int
sio_start(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		DPRINTF("sio_start: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_start: already started\n");
		hdl->eof = 1;
		return 0;
	}
#ifdef DEBUG
	if (!sio_getpar(hdl, &hdl->par))
		return 0;
	hdl->pollcnt = hdl->wcnt = hdl->rcnt = hdl->realpos = 0;
	gettimeofday(&hdl->tv, NULL);
#endif
	if (!hdl->ops->start(hdl))
		return 0;
	hdl->started = 1;
	return 1;
}

int
sio_stop(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		DPRINTF("sio_stop: eof\n");
		return 0;
	}
	if (!hdl->started) {
		DPRINTF("sio_stop: not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->stop(hdl))
		return 0;
#ifdef DEBUG
	DPRINTF("libsndio: polls: %llu, written = %llu, read: %llu\n",
	    hdl->pollcnt, hdl->wcnt, hdl->rcnt);
#endif
	hdl->started = 0;
	return 1;
}

int
sio_setpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		DPRINTF("sio_setpar: eof\n");
		return 0;
	}
	if (par->__magic != SIO_PAR_MAGIC) {
		DPRINTF("sio_setpar: use of uninitialized sio_par structure\n");
		hdl->eof = 1;
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_setpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (par->bufsz != ~0U) {
		DPRINTF("sio_setpar: setting bufsz is deprecated\n");
		par->appbufsz = par->bufsz;
	}
	if (par->rate != ~0U && par->appbufsz == ~0U)
		par->appbufsz = par->rate * 200 / 1000;
	return hdl->ops->setpar(hdl, par);
}

int
sio_getpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		DPRINTF("sio_getpar: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_getpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->getpar(hdl, par)) {
		par->__magic = 0;
		return 0;
	}
	par->__magic = 0;
	return 1;
}

int
sio_getcap(struct sio_hdl *hdl, struct sio_cap *cap)
{
	if (hdl->eof) {
		DPRINTF("sio_getcap: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_getcap: already started\n");
		hdl->eof = 1;
		return 0;
	}
	return hdl->ops->getcap(hdl, cap);
}

static int
sio_psleep(struct sio_hdl *hdl, int event)
{
	struct pollfd pfd[SIO_MAXNFDS];
	int revents;
	nfds_t nfds;

	nfds = sio_nfds(hdl);
	for (;;) {
		sio_pollfd(hdl, pfd, event);
		while (poll(pfd, nfds, -1) < 0) {
			if (errno == EINTR)
				continue;
			DPERROR("sio_psleep: poll");
			hdl->eof = 1;
			return 0;
		}
		revents = sio_revents(hdl, pfd);
		if (revents & POLLHUP) {
			DPRINTF("sio_psleep: hang-up\n");
			return 0;
		}
		if (revents & event)
			break;
	}
	return 1;
}

size_t
sio_read(struct sio_hdl *hdl, void *buf, size_t len)
{
	unsigned n;
	char *data = buf;
	size_t todo = len;

	if (hdl->eof) {
		DPRINTF("sio_read: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_REC)) {
		DPRINTF("sio_read: recording not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		DPRINTF("sio_read: zero length read ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->read(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof || todo < len)
				break;
			if (!sio_psleep(hdl, POLLIN))
				break;
			continue;
		}
		data += n;
		todo -= n;
#ifdef DEBUG
		hdl->rcnt += n;
#endif
	}
	return len - todo;
}

size_t
sio_write(struct sio_hdl *hdl, const void *buf, size_t len)
{
	unsigned n;
	const unsigned char *data = buf;
	size_t todo = len;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (sndio_debug >= 2)
		gettimeofday(&tv0, NULL);
#endif

	if (hdl->eof) {
		DPRINTF("sio_write: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_PLAY)) {
		DPRINTF("sio_write: playback not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		DPRINTF("sio_write: zero length write ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->write(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof)
				break;
			if (!sio_psleep(hdl, POLLOUT))
				break;
			continue;
		}
		data += n;
		todo -= n;
#ifdef DEBUG
		hdl->wcnt += n;
#endif
	}
#ifdef DEBUG
	if (sndio_debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec;
		DPRINTF(
		    "sio_write: wrote %d bytes of %d in %uus\n",
		    (int)(len - todo), (int)len, us);
	}
#endif
	return len - todo;
}

int
sio_nfds(struct sio_hdl *hdl)
{
	return hdl->ops->nfds(hdl);
}

int
sio_pollfd(struct sio_hdl *hdl, struct pollfd *pfd, int events)
{
	if (hdl->eof)
		return 0;
	if (!hdl->started)
		events = 0;
	return hdl->ops->pollfd(hdl, pfd, events);
}

int
sio_revents(struct sio_hdl *hdl, struct pollfd *pfd)
{
	int revents;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (sndio_debug >= 2)
		gettimeofday(&tv0, NULL);
#endif
	if (hdl->eof)
		return POLLHUP;
#ifdef DEBUG
	hdl->pollcnt++;
#endif
	revents = hdl->ops->revents(hdl, pfd);
	if (!hdl->started)
		return revents & POLLHUP;
#ifdef DEBUG
	if (sndio_debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec;
		DPRINTF("sio_revents: revents = 0x%x, complete in %uus\n",
		    revents, us);
	}
#endif
	return revents;
}

int
sio_eof(struct sio_hdl *hdl)
{
	return hdl->eof;
}

void
sio_onmove(struct sio_hdl *hdl, void (*cb)(void *, int), void *addr)
{
	if (hdl->started) {
		DPRINTF("sio_onmove: already started\n");
		hdl->eof = 1;
		return;
	}
	hdl->move_cb = cb;
	hdl->move_addr = addr;
}

void
sio_onmove_cb(struct sio_hdl *hdl, int delta)
{
#ifdef DEBUG
	struct timeval tv0, dtv;
	long long playpos;

	if (sndio_debug >= 3 && (hdl->mode & SIO_PLAY)) {
		gettimeofday(&tv0, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);
		hdl->realpos += delta;
		playpos = hdl->wcnt / (hdl->par.bps * hdl->par.pchan);
		DPRINTF("sio_onmove_cb: delta = %+7d, "
		    "plat = %+7lld, "
		    "realpos = %+7lld, "
		    "bufused = %+7lld\n",
		    delta,
		    playpos - hdl->realpos,
		    hdl->realpos,
		    hdl->realpos < 0 ? playpos : playpos - hdl->realpos);
	}
#endif
	if (hdl->move_cb)
		hdl->move_cb(hdl->move_addr, delta);
}

int
sio_setvol(struct sio_hdl *hdl, unsigned ctl)
{
	if (hdl->eof)
		return 0;
	if (!hdl->ops->setvol)
		return 1;
	if (!hdl->ops->setvol(hdl, ctl))
		return 0;
	hdl->ops->getvol(hdl);
	return 1;
}

int
sio_onvol(struct sio_hdl *hdl, void (*cb)(void *, unsigned), void *addr)
{
	if (hdl->started) {
		DPRINTF("sio_onvol: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->setvol)
		return 0;
	hdl->vol_cb = cb;
	hdl->vol_addr = addr;
	hdl->ops->getvol(hdl);
	return 1;
}

void
sio_onvol_cb(struct sio_hdl *hdl, unsigned ctl)
{
	if (hdl->vol_cb)
		hdl->vol_cb(hdl->vol_addr, ctl);
}
