/*	$OpenBSD: siofile.c,v 1.28 2024/12/20 07:35:56 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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

#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "dev_sioctl.h"
#include "dsp.h"
#include "fdpass.h"
#include "file.h"
#include "siofile.h"
#include "utils.h"

#define WATCHDOG_USEC	4000000		/* 4 seconds */

void dev_sio_onmove(void *, int);
void dev_sio_timeout(void *);
int dev_sio_pollfd(void *, struct pollfd *);
int dev_sio_revents(void *, struct pollfd *);
void dev_sio_run(void *);
void dev_sio_hup(void *);

extern struct fileops dev_sioctl_ops;

struct fileops dev_sio_ops = {
	"sio",
	dev_sio_pollfd,
	dev_sio_revents,
	dev_sio_run,
	dev_sio_run,
	dev_sio_hup
};

void
dev_sio_onmove(void *arg, int delta)
{
	struct dev *d = arg;

#ifdef DEBUG
	logx(4, "%s: tick, delta = %d", d->path, delta);

	d->sio.sum_utime += file_utime - d->sio.utime;
	d->sio.sum_wtime += file_wtime - d->sio.wtime;
	d->sio.wtime = file_wtime;
	d->sio.utime = file_utime;
	if (d->mode & MODE_PLAY)
		d->sio.pused -= delta;
	if (d->mode & MODE_REC)
		d->sio.rused += delta;
#endif
	dev_onmove(d, delta);
}

void
dev_sio_timeout(void *arg)
{
	struct dev *d = arg;

	logx(1, "%s: watchdog timeout", d->path);
	dev_migrate(d);
	dev_abort(d);
}

/*
 * open the device.
 */
int
dev_sio_open(struct dev *d)
{
	struct sio_par par;
	unsigned int rate, mode = d->reqmode & (SIO_PLAY | SIO_REC);

	d->sio.hdl = fdpass_sio_open(d->num, mode);
	if (d->sio.hdl == NULL) {
		if (mode != (SIO_PLAY | SIO_REC))
			return 0;
		d->sio.hdl = fdpass_sio_open(d->num, SIO_PLAY);
		if (d->sio.hdl != NULL)
			mode = SIO_PLAY;
		else {
			d->sio.hdl = fdpass_sio_open(d->num, SIO_REC);
			if (d->sio.hdl != NULL)
				mode = SIO_REC;
			else
				return 0;
		}
		logx(1, "%s: warning, device opened in %s mode",
		    d->path, mode == SIO_PLAY ? "play-only" : "rec-only");
	}
	d->mode = mode;

	d->sioctl.hdl = fdpass_sioctl_open(d->num, SIOCTL_READ | SIOCTL_WRITE);
	if (d->sioctl.hdl == NULL)
		logx(1, "%s: no control device", d->path);

	sio_initpar(&par);
	par.bits = d->par.bits;
	par.bps = d->par.bps;
	par.sig = d->par.sig;
	par.le = d->par.le;
	par.msb = d->par.msb;
	if (d->mode & SIO_PLAY)
		par.pchan = d->pchan;
	if (d->mode & SIO_REC)
		par.rchan = d->rchan;
	par.appbufsz = d->bufsz;
	par.round = d->round;
	par.rate = d->rate;
	if (!sio_setpar(d->sio.hdl, &par))
		goto bad_close;
	if (!sio_getpar(d->sio.hdl, &par))
		goto bad_close;

	/*
	 * If the requested rate is not supported by the device,
	 * use the new one, but retry using a block size that would
	 * match the requested one
	 */
	rate = par.rate;
	if (rate != d->rate) {
		sio_initpar(&par);
		par.bits = d->par.bits;
		par.bps = d->par.bps;
		par.sig = d->par.sig;
		par.le = d->par.le;
		par.msb = d->par.msb;
		if (mode & SIO_PLAY)
			par.pchan = d->reqpchan;
		if (mode & SIO_REC)
			par.rchan = d->reqrchan;
		par.appbufsz = d->bufsz * rate / d->rate;
		par.round = d->round * rate / d->rate;
		par.rate = rate;
		if (!sio_setpar(d->sio.hdl, &par))
			goto bad_close;
		if (!sio_getpar(d->sio.hdl, &par))
			goto bad_close;
	}

#ifdef DEBUG
	/*
	 * We support any parameter combination exposed by the kernel,
	 * and we have no other choice than trusting the kernel for
	 * returning correct parameters. But let's check parameters
	 * early and nicely report kernel bugs rather than crashing
	 * later in memset(), malloc() or alike.
	 */

	if (par.bits > BITS_MAX) {
		logx(0, "%s: %u: unsupported number of bits", d->path, par.bits);
		goto bad_close;
	}
	if (par.bps > SIO_BPS(BITS_MAX)) {
		logx(0, "%s: %u: unsupported sample size", d->path, par.bps);
		goto bad_close;
	}
	if ((d->mode & SIO_PLAY) && par.pchan > NCHAN_MAX) {
		logx(0, "%s: %u: unsupported number of play channels", d->path, par.pchan);
		goto bad_close;
	}
	if ((d->mode & SIO_REC) && par.rchan > NCHAN_MAX) {
		logx(0, "%s: %u: unsupported number of rec channels", d->path, par.rchan);
		goto bad_close;
	}
	if (par.bufsz == 0 || par.bufsz > RATE_MAX) {
		logx(0, "%s: %u: unsupported buffer size", d->path, par.bufsz);
		goto bad_close;
	}
	if (par.round == 0 || par.round > par.bufsz ||
	    par.bufsz % par.round != 0) {
		logx(0, "%s: %u: unsupported block size", d->path, par.round);
		goto bad_close;
	}
	if (par.rate == 0 || par.rate > RATE_MAX) {
		logx(0, "%s: %u: unsupported rate", d->path, par.rate);
		goto bad_close;
	}
#endif
	d->par.bits = par.bits;
	d->par.bps = par.bps;
	d->par.sig = par.sig;
	d->par.le = par.le;
	d->par.msb = par.msb;
	if (d->mode & SIO_PLAY)
		d->pchan = par.pchan;
	if (d->mode & SIO_REC)
		d->rchan = par.rchan;
	d->bufsz = par.bufsz;
	d->round = par.round;
	d->rate = par.rate;
	if (d->mode & MODE_PLAY)
		d->mode |= MODE_MON;
	sio_onmove(d->sio.hdl, dev_sio_onmove, d);
	d->sio.file = file_new(&dev_sio_ops, d, "dev", sio_nfds(d->sio.hdl));
	if (d->sioctl.hdl) {
		d->sioctl.file = file_new(&dev_sioctl_ops, d, "mix",
		    sioctl_nfds(d->sioctl.hdl));
	}
	timo_set(&d->sio.watchdog, dev_sio_timeout, d);
	dev_sioctl_open(d);
	return 1;
 bad_close:
	sio_close(d->sio.hdl);
	if (d->sioctl.hdl) {
		sioctl_close(d->sioctl.hdl);
		d->sioctl.hdl = NULL;
	}
	return 0;
}

void
dev_sio_close(struct dev *d)
{
	dev_sioctl_close(d);
#ifdef DEBUG
	logx(3, "%s: closed", d->path);
#endif
	timo_del(&d->sio.watchdog);
	file_del(d->sio.file);
	sio_close(d->sio.hdl);
	if (d->sioctl.hdl) {
		file_del(d->sioctl.file);
		sioctl_close(d->sioctl.hdl);
		d->sioctl.hdl = NULL;
	}
}

void
dev_sio_start(struct dev *d)
{
	if (!sio_start(d->sio.hdl)) {
		logx(1, "%s: failed to start device", d->path);
		return;
	}
	if (d->mode & MODE_PLAY) {
		d->sio.cstate = DEV_SIO_CYCLE;
		d->sio.todo = 0;
	} else {
		d->sio.cstate = DEV_SIO_READ;
		d->sio.todo = d->round * d->rchan * d->par.bps;
	}
#ifdef DEBUG
	d->sio.pused = 0;
	d->sio.rused = 0;
	d->sio.sum_utime = 0;
	d->sio.sum_wtime = 0;
	d->sio.wtime = file_wtime;
	d->sio.utime = file_utime;
	logx(3, "%s: started", d->path);
#endif
	timo_add(&d->sio.watchdog, WATCHDOG_USEC);
}

void
dev_sio_stop(struct dev *d)
{
	if (!sio_eof(d->sio.hdl) && !sio_flush(d->sio.hdl)) {
		logx(1, "%s: failed to stop device", d->path);
		return;
	}
#ifdef DEBUG
	logx(3, "%s: stopped, load avg = %lld / %lld",
	    d->path, d->sio.sum_utime / 1000, d->sio.sum_wtime / 1000);
#endif
	timo_del(&d->sio.watchdog);
}

int
dev_sio_pollfd(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;
	int events;

	events = (d->sio.cstate == DEV_SIO_READ) ? POLLIN : POLLOUT;
	return sio_pollfd(d->sio.hdl, pfd, events);
}

int
dev_sio_revents(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;
	int events;

	events = sio_revents(d->sio.hdl, pfd);
#ifdef DEBUG
	d->sio.events = events;
#endif
	return events;
}

void
dev_sio_run(void *arg)
{
	struct dev *d = arg;
	unsigned char *data, *base;
	unsigned int n;

	/*
	 * sio_read() and sio_write() would block at the end of the
	 * cycle so we *must* return and restart poll()'ing. Otherwise
	 * we may trigger dev_cycle() which would make all clients
	 * underrun (ex, on a play-only device)
	 */
	for (;;) {
		if (d->pstate != DEV_RUN)
			return;
		switch (d->sio.cstate) {
		case DEV_SIO_READ:
#ifdef DEBUG
			if (!(d->sio.events & POLLIN)) {
				logx(0, "%s: recording, but POLLIN not set", d->path);
				panic();
			}
			if (d->sio.todo == 0) {
				logx(0, "%s: can't read data", d->path);
				panic();
			}
			if (d->prime > 0) {
				logx(0, "%s: unexpected data", d->path);
				panic();
			}
#endif
			base = d->decbuf ? d->decbuf : (unsigned char *)d->rbuf;
			data = base +
			    d->rchan * d->round * d->par.bps -
			    d->sio.todo;
			n = sio_read(d->sio.hdl, data, d->sio.todo);
			d->sio.todo -= n;
#ifdef DEBUG
			logx(4, "%s: read %u bytes, todo %u / %u", d->path,
			    n, d->sio.todo, d->round * d->rchan * d->par.bps);
#endif
			if (d->sio.todo > 0)
				return;
#ifdef DEBUG
			d->sio.rused -= d->round;
			if (d->sio.rused >= d->round) {
				logx(2, "%s: rec hw xrun, rused = %d / %d",
				    d->path, d->sio.rused, d->bufsz);
			}
#endif
			d->sio.cstate = DEV_SIO_CYCLE;
			break;
		case DEV_SIO_CYCLE:
			timo_del(&d->sio.watchdog);
			timo_add(&d->sio.watchdog, WATCHDOG_USEC);

#ifdef DEBUG
			/*
			 * check that we're called at cycle boundary:
			 * either after a recorded block, or when POLLOUT is
			 * raised
			 */
			if (!((d->mode & MODE_REC) && d->prime == 0) &&
			    !(d->sio.events & POLLOUT)) {
				logx(0, "%s: cycle not at block boundary", d->path);
				panic();
			}
#endif
			dev_cycle(d);
			if (d->mode & MODE_PLAY) {
				d->sio.cstate = DEV_SIO_WRITE;
				d->sio.todo = d->round * d->pchan * d->par.bps;
				break;
			} else {
				d->sio.cstate = DEV_SIO_READ;
				d->sio.todo = d->round * d->rchan * d->par.bps;
				return;
			}
		case DEV_SIO_WRITE:
#ifdef DEBUG
			if (d->sio.todo == 0) {
				logx(0, "%s: can't write data", d->path);
				panic();
			}
#endif
			base = d->encbuf ? d->encbuf : (unsigned char *)DEV_PBUF(d);
			data = base +
			    d->pchan * d->round * d->par.bps -
			    d->sio.todo;
			n = sio_write(d->sio.hdl, data, d->sio.todo);
			d->sio.todo -= n;
#ifdef DEBUG
			logx(4, "%s: wrote %u bytes, todo %u / %u",
			    d->path, n, d->sio.todo, d->round * d->pchan * d->par.bps);
#endif
			if (d->sio.todo > 0)
				return;
#ifdef DEBUG
			d->sio.pused += d->round;
			if (d->prime == 0 &&
			    d->sio.pused <= d->bufsz - d->round) {
				logx(2, "%s: play hw xrun, pused = %d / %d",
				    d->path, d->sio.pused, d->bufsz);
			}
			if (d->sio.pused < 0 ||
			    d->sio.pused > d->bufsz) {
				/* device driver or libsndio bug */
				logx(2, "%s: out of bounds pused = %d / %d",
				    d->path, d->sio.pused, d->bufsz);
			}
#endif
			d->poffs += d->round;
			if (d->poffs == d->psize)
				d->poffs = 0;
			if ((d->mode & MODE_REC) && d->prime == 0) {
				d->sio.cstate = DEV_SIO_READ;
				d->sio.todo = d->round * d->rchan * d->par.bps;
			} else
				d->sio.cstate = DEV_SIO_CYCLE;
			return;
		}
	}
}

void
dev_sio_hup(void *arg)
{
	struct dev *d = arg;

#ifdef DEBUG
	logx(2, "%s: disconnected", d->path);
#endif
	dev_migrate(d);
	dev_abort(d);
}
