/*	$OpenBSD: siofile.c,v 1.1 2010/01/13 10:02:52 ratchov Exp $	*/
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

#include <poll.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aparams.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "file.h"
#include "siofile.h"
#ifdef DEBUG
#include "dbg.h"
#endif

struct siofile {
	struct file file;
	struct sio_hdl *hdl;
	int started;
};

void siofile_close(struct file *);
unsigned siofile_read(struct file *, unsigned char *, unsigned);
unsigned siofile_write(struct file *, unsigned char *, unsigned);
void siofile_start(struct file *);
void siofile_stop(struct file *);
int siofile_nfds(struct file *);
int siofile_pollfd(struct file *, struct pollfd *, int);
int siofile_revents(struct file *, struct pollfd *);

struct fileops siofile_ops = {
	"sio",
	sizeof(struct siofile),
	siofile_close,
	siofile_read,
	siofile_write,
	siofile_start,
	siofile_stop,
	siofile_nfds,
	siofile_pollfd,
	siofile_revents
};

void
siofile_cb(void *addr, int delta)
{
	struct siofile *f = (struct siofile *)addr;
	struct aproc *p;

#ifdef DEBUG
	if (delta < 0 || delta > (60 * RATE_MAX)) {
		dbg_puts(f->file.name);
		dbg_puts(": ");
		dbg_puti(delta);
		dbg_puts(": bogus sndio delta");
		dbg_panic();
	}
#endif
	if (delta != 0) {
		p = f->file.wproc;
		if (p && p->ops->opos)
			p->ops->opos(p, NULL, delta);
	}
	if (delta != 0) {
		p = f->file.rproc;
		if (p && p->ops->ipos)
			p->ops->ipos(p, NULL, delta);
	}
}

/*
 * Open the device.
 */
struct siofile *
siofile_new(struct fileops *ops, char *path,
    struct aparams *ipar, struct aparams *opar,
    unsigned *bufsz, unsigned *round)
{
	struct sio_par par;
	struct sio_hdl *hdl;
	struct siofile *f;
	int mode;

	mode = 0;
	if (ipar)
		mode |= SIO_REC;
	if (opar)
		mode |= SIO_PLAY;
	hdl = sio_open(path, mode, 1);
	if (hdl == NULL)
		return NULL;
	sio_initpar(&par);
	if (ipar) {
		par.bits = ipar->bits;
		par.bps = ipar->bps;
		par.sig = ipar->sig;
		par.le = ipar->le;
		par.msb = ipar->msb;
		par.rate = ipar->rate;
		par.rchan = ipar->cmax - ipar->cmin + 1;
	} else {
		par.bits = opar->bits;
		par.bps = opar->bps;
		par.sig = opar->sig;
		par.le = opar->le;
		par.msb = opar->msb;
		par.rate = opar->rate;
	}
	if (opar)
		par.pchan = opar->cmax - opar->cmin + 1;
	par.appbufsz = *bufsz;
	par.round = *round;
	if (!sio_setpar(hdl, &par))
		goto bad_close;
	if (!sio_getpar(hdl, &par))
		goto bad_close;
	if (ipar) {
		ipar->bits = par.bits;
		ipar->bps = par.bps;
		ipar->sig = par.sig;
		ipar->le = par.le;
		ipar->msb = par.msb;
		ipar->rate = par.rate;
		ipar->cmax = ipar->cmin + par.rchan - 1;
	}
	if (opar) {
		opar->bits = par.bits;
		opar->bps = par.bps;
		opar->sig = par.sig;
		opar->le = par.le;
		opar->msb = par.msb;
		opar->rate = par.rate;
		opar->cmax = opar->cmin + par.pchan - 1;
	}
	*bufsz = par.bufsz;
	*round = par.round;
	if (path == NULL)
		path = "default";
	f = (struct siofile *)file_new(ops, path, sio_nfds(hdl));
	if (f == NULL)
		goto bad_close;
	f->hdl = hdl;
	f->started = 0;
	sio_onmove(f->hdl, siofile_cb, f);
	return f;
 bad_close:
	sio_close(hdl);
	return NULL;
}

void
siofile_start(struct file *file)
{
	struct siofile *f = (struct siofile *)file;

	if (!sio_start(f->hdl)) {
#ifdef DEBUG
		dbg_puts(f->file.name);
		dbg_puts(": failed to start device\n");
#endif
		file_close(file);
		return;
	}
	f->started = 1;
#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(&f->file);
		dbg_puts(": started\n");
	}
#endif
}

void
siofile_stop(struct file *file)
{
	struct siofile *f = (struct siofile *)file;

	f->started = 0;
	if (!sio_eof(f->hdl) && !sio_stop(f->hdl)) {
#ifdef DEBUG
		dbg_puts(f->file.name);
		dbg_puts(": failed to stop device\n");
#endif
		file_close(file);
		return;
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		file_dbg(&f->file);
		dbg_puts(": stopped\n");
	}
#endif
}

unsigned
siofile_read(struct file *file, unsigned char *data, unsigned count)
{
	struct siofile *f = (struct siofile *)file;
	unsigned n;

	n = f->started ? sio_read(f->hdl, data, count) : 0;
	if (n == 0) {
		f->file.state &= ~FILE_ROK;
		if (sio_eof(f->hdl)) {
#ifdef DEBUG
			dbg_puts(f->file.name);
			dbg_puts(": failed to read from device\n");
#endif
			file_eof(&f->file);
		} else {
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": reading blocked\n");
			}
#endif
		}
		return 0;
	}
	return n;

}

unsigned
siofile_write(struct file *file, unsigned char *data, unsigned count)
{
	struct siofile *f = (struct siofile *)file;
	unsigned n;

	n = f->started ? sio_write(f->hdl, data, count) : 0;
	if (n == 0) {
		f->file.state &= ~FILE_WOK;
		if (sio_eof(f->hdl)) {
#ifdef DEBUG
			dbg_puts(f->file.name);
			dbg_puts(": failed to write on device\n");
#endif
			file_hup(&f->file);
		} else {
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": writing blocked\n");
			}
#endif
		}
		return 0;
	}
	return n;
}

int
siofile_nfds(struct file *file)
{
	return sio_nfds(((struct siofile *)file)->hdl);
}

int
siofile_pollfd(struct file *file, struct pollfd *pfd, int events)
{
	struct siofile *f = (struct siofile *)file;

	if (!f->started)
		events &= ~(POLLIN | POLLOUT);
	return sio_pollfd(((struct siofile *)file)->hdl, pfd, events);
}

int
siofile_revents(struct file *file, struct pollfd *pfd)
{
	return sio_revents(((struct siofile *)file)->hdl, pfd);
}

void
siofile_close(struct file *file)
{
	struct siofile *f = (struct siofile *)file;

	if (f->started)
		siofile_stop(&f->file);
	return sio_close(((struct siofile *)file)->hdl);
}
