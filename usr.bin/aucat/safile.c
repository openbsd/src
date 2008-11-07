/*	$OpenBSD: safile.c,v 1.3 2008/11/07 21:01:15 ratchov Exp $	*/
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
#include <sys/time.h>

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndio.h>

#include "conf.h"
#include "file.h"
#include "aproc.h"
#include "aparams.h"
#include "safile.h"
#include "dev.h"

struct safile {
	struct file file;
	struct sio_hdl *hdl;
#ifdef DEBUG	
	struct timeval itv, otv;
#endif
};

void safile_close(struct file *);
unsigned safile_read(struct file *, unsigned char *, unsigned);
unsigned safile_write(struct file *, unsigned char *, unsigned);
void safile_start(struct file *);
void safile_stop(struct file *);
int safile_nfds(struct file *);
int safile_pollfd(struct file *, struct pollfd *, int);
int safile_revents(struct file *, struct pollfd *);

struct fileops safile_ops = {
	"sndio",
	sizeof(struct safile),
	safile_close,
	safile_read,
	safile_write,
	safile_start,
	safile_stop,
	safile_nfds,
	safile_pollfd,
	safile_revents
};

/*
 * list of (rate, block-size) pairs ordered by frequency preference and
 * then by block size preference (except for jumbo block sizes that are
 * less prefered than anything else).
 */
struct blkdesc {
	unsigned rate;		/* sample rate */
	unsigned round;		/* usable block sizes */
} blkdesc[] = {
	{ 44100,	 882 },
	{ 44100,	 840 },
	{ 44100,	 441 },
	{ 44100,	 420 },
	{ 44100,	1764 },
	{ 44100,	1680 },
	{ 48000,	 960 },
	{ 48000,	 768 },
	{ 48000,	 480 },
	{ 48000,	 384 },
	{ 48000,	1920 },
	{ 48000,	1536 },
	{ 32000,	 640 },
	{ 32000,	 512 },
	{ 32000,	 320 },
	{ 32000,	 256 },
	{ 32000,	1280 },
	{ 32000,	1024 },
	{ 44100,	2940 },
	{ 48000,	2976 },
	{ 32000,	3200 },
	{  8000,	 320 },
	{  8000,	 256 },
	{     0,	   0 }
};


int
safile_trypar(struct sio_hdl *hdl, struct sio_par *par, int blkio)
{
	struct blkdesc *d;
	struct sio_par np;
	unsigned rate = par->rate;
	unsigned round = par->round;

	if (!blkio) {
		fprintf(stderr, "not setting block size\n");
		if (!sio_setpar(hdl, par))
			return 0;
		if (!sio_getpar(hdl, par))
			return 0;
		return 1;
	}

	/*
	 * find the rate we want to use
	 */
	for (d = blkdesc;; d++) {
		if (d->rate == 0) {
			d = blkdesc;
			break;
		}
		if (d->rate == rate)
			break;
	}

	/*
	 * find the first matching entry, (the blkdesc array is)
	 * sorted by order of preference)
	 */
	for (;; d++) {
		if (d->rate == 0)
			break;
		if (d->round > round)
			continue;
		par->rate = d->rate;
		par->round = d->round;
		if (!sio_setpar(hdl, par))
			return 0;
		if (!sio_getpar(hdl, &np))
			return 0;
		if (np.rate == d->rate && np.round == d->round) {
			*par = np;
			if (d->round >= d->rate / 15)
				fprintf(stderr,
				    "Warning: using jumbo block size, "
				    "try to use another sample rate.\n");
			return 1;
		}
		DPRINTF("safile_trypar: %uHz/%ufr failed, got %uHz/%ufr\n",
		    d->rate, d->round, np.rate, np.round);
	}
	fprintf(stderr, "Couldn't set block size to <%u frames.\n", round);
	return 0;
}

void
safile_cb(void *addr, int delta)
{
	struct safile *f = (struct safile *)addr;
	struct aproc *p;

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
 * open the device
 */
struct safile *
safile_new(struct fileops *ops, char *path,
    struct aparams *ipar, struct aparams *opar,
    unsigned *bufsz, unsigned *round, int blkio)
{
	struct sio_par par;
	struct sio_hdl *hdl;
	struct safile *f;
	int mode;

	mode = 0;
	if (ipar)
		mode |= SIO_REC;
	if (opar)
		mode |= SIO_PLAY;
	if (!mode)
		fprintf(stderr, "%s: must at least play or record", path);
	hdl = sio_open(path, mode, 1);
	if (hdl == NULL) {
		fprintf(stderr, "safile_new: can't open device\n");
		return NULL;
	}
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
	par.bufsz = *bufsz;
	par.round = *round;
	if (!safile_trypar(hdl, &par, blkio))
		exit(1);
	if (ipar) {
		ipar->bits = par.bits;
		ipar->bps = par.bps;
		ipar->sig = par.sig;
		ipar->le = par.le;
		ipar->msb = par.msb;
		ipar->rate = par.rate;
		ipar->cmax = par.rchan - 1;
		ipar->cmin = 0;
	}
	if (opar) {
		opar->bits = par.bits;
		opar->bps = par.bps;
		opar->sig = par.sig;
		opar->le = par.le;
		opar->msb = par.msb;
		opar->rate = par.rate;
		opar->cmax = par.pchan - 1;
		opar->cmin = 0;
	}
	*bufsz = par.bufsz;
	*round = par.round;
	DPRINTF("safile_new: using %u(%u) fpb\n", *bufsz, *round);
	f = (struct safile *)file_new(ops, "hdl", sio_nfds(hdl));
	f->hdl = hdl;
	sio_onmove(f->hdl, safile_cb, f);
	return f;
}

void
safile_start(struct file *file)
{	
	struct safile *f = (struct safile *)file;

	if (!sio_start(f->hdl)) {
		fprintf(stderr, "safile_start: sio_start() failed\n");
		exit(1);
	}
	DPRINTF("safile_start: play/rec started\n");
}

void
safile_stop(struct file *file)
{
	struct safile *f = (struct safile *)file;

	if (!sio_stop(f->hdl)) {
		fprintf(stderr, "safile_stop: sio_start() filed\n");
		exit(1);
	}
	DPRINTF("safile_stop: play/rec stopped\n");
}

unsigned
safile_read(struct file *file, unsigned char *data, unsigned count)
{
	struct safile *f = (struct safile *)file;
	unsigned n;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (!(f->file.state & FILE_ROK)) {
		DPRINTF("file_read: %s: bad state\n", f->file.name);
		abort();
	}
	gettimeofday(&tv0, NULL);
#endif
	n = sio_read(f->hdl, data, count);
	if (n == 0) {
		f->file.state &= ~FILE_ROK;
		if (sio_eof(f->hdl)) {
			fprintf(stderr, "safile_read: eof\n");
			file_eof(&f->file);
		} else {
			DPRINTFN(3, "safile_read: %s: blocking...\n",
			    f->file.name);
		}
		return 0;
	}
#ifdef DEBUG
	gettimeofday(&tv1, NULL);
	timersub(&tv1, &tv0, &dtv);
	us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
	DPRINTFN(us < 5000 ? 4 : 1,
	    "safile_read: %s: got %d bytes in %uus\n", 
	    f->file.name, n, us);
#endif
	return n;

}

unsigned
safile_write(struct file *file, unsigned char *data, unsigned count)
{
	struct safile *f = (struct safile *)file;
	unsigned n;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;
	
	if (!(f->file.state & FILE_WOK)) {
		DPRINTF("safile_write: %s: bad state\n", f->file.name);
		abort();
	}
	gettimeofday(&tv0, NULL);
#endif
	n = sio_write(f->hdl, data, count);
	if (n == 0) {
		f->file.state &= ~FILE_WOK;
		if (sio_eof(f->hdl)) {
			fprintf(stderr, "safile_write: %s: hup\n", f->file.name);
			file_hup(&f->file);
		} else {
			DPRINTFN(3, "safile_write: %s: blocking...\n",
			    f->file.name);
		}
		return 0;
	}
#ifdef DEBUG
	gettimeofday(&tv1, NULL);
	timersub(&tv1, &tv0, &dtv);
	us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
	DPRINTFN(us < 5000 ? 4 : 1,
	    "safile_write: %s: wrote %d bytes in %uus\n",
	    f->file.name, n, us);
#endif
	return n;
}

int
safile_nfds(struct file *file)
{
	return sio_nfds(((struct safile *)file)->hdl);
}

int
safile_pollfd(struct file *file, struct pollfd *pfd, int events)
{
	return sio_pollfd(((struct safile *)file)->hdl, pfd, events);
}

int
safile_revents(struct file *file, struct pollfd *pfd)
{
	return sio_revents(((struct safile *)file)->hdl, pfd);
}

void
safile_close(struct file *file)
{
	return sio_close(((struct safile *)file)->hdl);
}
