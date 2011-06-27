/*	$OpenBSD: siofile.c,v 1.7 2011/06/27 07:22:00 ratchov Exp $	*/
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
#include "abuf.h"
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
	unsigned wtickets, wbpf;
	unsigned rtickets, rbpf;
	unsigned bufsz;
	int started;
#ifdef DEBUG
	long long wtime, utime;
#endif
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

int wsio_out(struct aproc *, struct abuf *);
int rsio_in(struct aproc *, struct abuf *);

struct aproc_ops rsio_ops = {
	"rsio",
	rsio_in,
	rfile_out,
	rfile_eof,
	rfile_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	rfile_done
};

struct aproc_ops wsio_ops = {
	"wsio",
	wfile_in,
	wsio_out,
	wfile_eof,
	wfile_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	wfile_done
};

struct aproc *
rsio_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rsio_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;
	f->rproc = p;
	return p;
}

struct aproc *
wsio_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wsio_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;
	f->wproc = p;
	return p;
}

int
wsio_out(struct aproc *p, struct abuf *obuf)
{
	struct siofile *f = (struct siofile *)p->u.io.file;

	if (f->wtickets == 0) {
#ifdef DEBUG
		if (debug_level >= 4) {
			file_dbg(&f->file);
			dbg_puts(": no more write tickets\n");
		}
#endif
		f->file.state &= ~FILE_WOK;
		return 0;
	}
	return wfile_out(p, obuf);
}

int
rsio_in(struct aproc *p, struct abuf *ibuf)
{
	struct siofile *f = (struct siofile *)p->u.io.file;

	if (f->rtickets == 0) {
#ifdef DEBUG
		if (debug_level >= 4) {
			file_dbg(&f->file);
			dbg_puts(": no more read tickets\n");
		}
#endif
		f->file.state &= ~FILE_ROK;
		return 0;
	}
	return rfile_in(p, ibuf);
}

void
siofile_cb(void *addr, int delta)
{
	struct siofile *f = (struct siofile *)addr;
	struct aproc *p;

#ifdef DEBUG
	if (delta < 0 || delta > (60 * RATE_MAX)) {
		file_dbg(&f->file);
		dbg_puts(": ");
		dbg_puti(delta);
		dbg_puts(": bogus sndio delta");
		dbg_panic();
	}
	if (debug_level >= 4) {
		file_dbg(&f->file);
		dbg_puts(": tick, delta = ");
		dbg_puti(delta);
		dbg_puts(", load = ");
		dbg_puti((file_utime - f->utime) / 1000);
		dbg_puts(" + ");
		dbg_puti((file_wtime - f->wtime) / 1000);
		dbg_puts("\n");
	}
	f->wtime = file_wtime;
	f->utime = file_utime;
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
	f->wtickets += delta * f->wbpf;
	f->rtickets += delta * f->rbpf;
}

/*
 * Open the device.
 */
struct siofile *
siofile_new(struct fileops *ops, char *path, unsigned *rmode,
    struct aparams *ipar, struct aparams *opar,
    unsigned *bufsz, unsigned *round)
{
	char *siopath;
	struct sio_par par;
	struct sio_hdl *hdl;
	struct siofile *f;
	unsigned mode = *rmode;

	siopath = (strcmp(path, "default") == 0) ? NULL : path;
	hdl = sio_open(siopath, mode, 1);
	if (hdl == NULL) {
		if (mode != (SIO_PLAY | SIO_REC))
			return NULL;
		hdl = sio_open(siopath, SIO_PLAY, 1);
		if (hdl != NULL)
			mode = SIO_PLAY;
		else {
			hdl = sio_open(siopath, SIO_REC, 1);
			if (hdl != NULL)
				mode = SIO_REC;
			else
				return NULL;
		}
#ifdef DEBUG
		if (debug_level >= 1) {
			dbg_puts("warning, device opened in ");
			dbg_puts(mode == SIO_PLAY ? "play-only" : "rec-only");
			dbg_puts(" mode\n");
		}
#endif
	}

	sio_initpar(&par);
	if (mode & SIO_REC) {
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
	if (mode & SIO_PLAY)
		par.pchan = opar->cmax - opar->cmin + 1;
	par.appbufsz = *bufsz;
	par.round = *round;
	if (!sio_setpar(hdl, &par))
		goto bad_close;
	if (!sio_getpar(hdl, &par))
		goto bad_close;
	if (mode & SIO_REC) {
		ipar->bits = par.bits;
		ipar->bps = par.bps;
		ipar->sig = par.sig;
		ipar->le = par.le;
		ipar->msb = par.msb;
		ipar->rate = par.rate;
		ipar->cmax = ipar->cmin + par.rchan - 1;
	}
	if (mode & SIO_PLAY) {
		opar->bits = par.bits;
		opar->bps = par.bps;
		opar->sig = par.sig;
		opar->le = par.le;
		opar->msb = par.msb;
		opar->rate = par.rate;
		opar->cmax = opar->cmin + par.pchan - 1;
	}
	*rmode = mode;
	*bufsz = par.bufsz;
	*round = par.round;
	f = (struct siofile *)file_new(ops, path, sio_nfds(hdl));
	if (f == NULL)
		goto bad_close;
	f->hdl = hdl;
	f->started = 0;
	f->wtickets = 0;
	f->rtickets = 0;
	f->wbpf = par.pchan * par.bps;
	f->rbpf = par.rchan * par.bps;
	f->bufsz = par.bufsz;
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
	f->wtickets = f->bufsz * f->wbpf;
	f->rtickets = 0;
#ifdef DEBUG
	f->wtime = file_wtime;
	f->utime = file_utime;
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

#ifdef DEBUG
	if (f->rtickets == 0) {
		file_dbg(&f->file);
		dbg_puts(": called with no read tickets\n");
	}
#endif
	if (count > f->rtickets)
		count = f->rtickets;
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
	} else {
		f->rtickets -= n;
		if (f->rtickets == 0) {
			f->file.state &= ~FILE_ROK;
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": read tickets exhausted\n");
			}
#endif
		}
	}
	return n;

}

unsigned
siofile_write(struct file *file, unsigned char *data, unsigned count)
{
	struct siofile *f = (struct siofile *)file;
	unsigned n;

#ifdef DEBUG
	if (f->wtickets == 0) {
		file_dbg(&f->file);
		dbg_puts(": called with no write tickets\n");
	}
#endif
	if (count > f->wtickets)
		count = f->wtickets;
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
	} else {
		f->wtickets -= n;
		if (f->wtickets == 0) {
			f->file.state &= ~FILE_WOK;
#ifdef DEBUG
			if (debug_level >= 4) {
				file_dbg(&f->file);
				dbg_puts(": write tickets exhausted\n");
			}
#endif
		}
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
