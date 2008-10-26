/*	$OpenBSD: safile.c,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
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
#include <libsa.h>

#include "conf.h"
#include "file.h"
#include "aproc.h"
#include "aparams.h"
#include "safile.h"
#include "dev.h"

struct safile {
	struct file file;
	struct sa_hdl *hdl;
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
	"libsa",
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
    unsigned *bufsz, unsigned *round)
{
	struct sa_par par;
	struct sa_hdl *hdl;
	struct safile *f;
	int mode;

	mode = 0;
	if (ipar)
		mode |= SA_REC;
	if (opar)
		mode |= SA_PLAY;
	if (!mode)
		fprintf(stderr, "%s: must at least play or record", path);
	hdl = sa_open(path, mode, 1);
	if (hdl == NULL) {
		fprintf(stderr, "safile_new: can't open device\n");
		return NULL;
	}
	sa_initpar(&par);
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
	if (*bufsz)
		par.bufsz = *bufsz;
	if (!sa_setpar(hdl, &par)) {
		fprintf(stderr, "safile_new: sa_setpar failed\n");
		exit(1);
	}
	if (!sa_getpar(hdl, &par)) {
		fprintf(stderr, "safile_new: sa_getpar failed\n");
		exit(1);
	}
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
	DPRINTF("safile_open: using %u(%u) fpb\n", *bufsz, *round);
	f = (struct safile *)file_new(ops, "hdl", sa_nfds(hdl));
	f->hdl = hdl;
	sa_onmove(f->hdl, safile_cb, f);
	return f;
}

void
safile_start(struct file *file)
{	
	struct safile *f = (struct safile *)file;

	if (!sa_start(f->hdl)) {
		fprintf(stderr, "safile_start: sa_start() failed\n");
		exit(1);
	}
	DPRINTF("safile_start: play/rec started\n");
}

void
safile_stop(struct file *file)
{
	struct safile *f = (struct safile *)file;

	if (!sa_stop(f->hdl)) {
		fprintf(stderr, "safile_stop: sa_start() filed\n");
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
	n = sa_read(f->hdl, data, count);
	if (n == 0) {
		f->file.state &= ~FILE_ROK;
		if (sa_eof(f->hdl)) {
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
	n = sa_write(f->hdl, data, count);
	if (n == 0) {
		f->file.state &= ~FILE_WOK;
		if (sa_eof(f->hdl)) {
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
	return sa_nfds(((struct safile *)file)->hdl);
}

int
safile_pollfd(struct file *file, struct pollfd *pfd, int events)
{
	return sa_pollfd(((struct safile *)file)->hdl, pfd, events);
}

int
safile_revents(struct file *file, struct pollfd *pfd)
{
	return sa_revents(((struct safile *)file)->hdl, pfd);
}

void
safile_close(struct file *file)
{
	return sa_close(((struct safile *)file)->hdl);
}
