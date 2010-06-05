/*	$OpenBSD: dev.c,v 1.59 2010/06/05 16:14:44 ratchov Exp $	*/
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
/*
 * Device abstraction module
 *
 * This module exposes a ``enhanced device'' that uses aproc
 * structures framework; it does conversions on the fly and can
 * handle multiple streams.  The enhanced device starts and stops
 * automatically, when streams are attached, and provides
 * primitives for MIDI control
 *
 * From the main loop, the device is used as follows:
 *
 *   1. create the device using dev_new_xxx()
 *   2. call dev_run() in the event loop
 *   3. destroy the device using dev_del()
 *   4. continue running the event loop to drain
 *
 * The device is used as follows from aproc context:
 *
 *   1. open the device with dev_ref()
 *   2. negociate parameters (mode, rate, ...)
 *   3. create your stream (ie allocate and fill abufs)
 *   4. attach your stream atomically:
 * 	  - first call dev_wakeup() to ensure device is not suspended
 *	  - possibly fetch dynamic parameters (eg. dev_getpos())
 *	  - attach your buffers with dev_attach()
 *   5. close your stream, ie abuf_eof() or abuf_hup()
 *   6. close the device with dev_unref()
 *
 * The device has the following states:
 *
 * CLOSED	sio_open() is not called, it's not ready and
 *		no streams can be attached; dev_ref() must
 *		be called to open the device
 *
 * INIT		device is opened, processing chain is ready, but
 *		DMA is not started yet. Streams can attach,
 *		in which case device will automatically switch
 *		to the START state
 *
 * START	at least one stream is attached, play buffers
 *		are primed (if necessary) DMA is ready and
 *		will start immeadiately (next cycle)
 *
 * RUN		DMA is started. New streams can attach. If the
 *		device is idle (all streams are closed and
 *		finished draining), then the device
 *		automatically switches to INIT or CLOSED
 */
/*
 * TODO:
 *
 * priming buffer is not ok, because it will insert silence and
 * break synchronization to other programs.
 *
 * priming buffer in server mode is required, because f->bufsz may
 * be smaller than the server buffer and may cause underrun in the
 * dev_bufsz part of the buffer, in turn causing apps to break. It
 * doesn't hurt because we care only in synchronization between
 * clients.
 *
 * Priming is not required in non-server mode, because streams
 * actually start when they are in the READY state, and their
 * buffer is large enough to never cause underruns of dev_bufsz.
 *
 * Fix sock.c to allocate dev_bufsz, but to use only appbufsz --
 * or whatever -- but to avoid underruns in dev_bufsz. Then remove
 * this ugly hack.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "pipe.h"
#include "miofile.h"
#include "siofile.h"
#include "midi.h"
#include "opt.h"
#ifdef DEBUG
#include "dbg.h"
#endif

int  dev_open(struct dev *);
void dev_close(struct dev *);
void dev_start(struct dev *);
void dev_stop(struct dev *);
void dev_clear(struct dev *);

struct dev *dev_list = NULL;

/*
 * Create a sndio device
 */
struct dev *
dev_new_sio(char *path,
    unsigned mode, struct aparams *dipar, struct aparams *dopar,
    unsigned bufsz, unsigned round, unsigned hold)
{
	struct dev *d;

	d = malloc(sizeof(struct dev));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	d->path = path;
	d->reqmode = mode;
	if (mode & MODE_PLAY)
		d->reqopar = *dopar;
	if (mode & MODE_RECMASK)
		d->reqipar = *dipar;
	d->reqbufsz = bufsz;
	d->reqround = round;
	d->hold = hold;
	d->pstate = DEV_CLOSED;
	d->next = dev_list;
	dev_list = d;
	if (d->hold && !dev_open(d)) {
		dev_del(d);
		return NULL;
	}
	return d;
}

/*
 * Create a loopback synchronous device
 */
struct dev *
dev_new_loop(struct aparams *dipar, struct aparams *dopar, unsigned bufsz)
{
	struct aparams par;
	unsigned cmin, cmax, rate;
	struct dev *d;

	d = malloc(sizeof(struct dev));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	cmin = (dipar->cmin < dopar->cmin) ? dipar->cmin : dopar->cmin;
	cmax = (dipar->cmax > dopar->cmax) ? dipar->cmax : dopar->cmax;
	rate = (dipar->rate > dopar->rate) ? dipar->rate : dopar->rate;
	aparams_init(&par, cmin, cmax, rate);
	d->reqipar = par;
	d->reqopar = par;
	d->rate = rate;
	d->reqround = (bufsz + 1) / 2;
	d->reqbufsz = d->reqround * 2;
	d->reqmode = MODE_PLAY | MODE_REC | MODE_LOOP;
	d->pstate = DEV_CLOSED;
	d->hold = 0;
	d->next = dev_list;
	dev_list = d;
	return d;
}

/*
 * Create a MIDI thru box device
 */
struct dev *
dev_new_thru(void)
{
	struct dev *d;

	d = malloc(sizeof(struct dev));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	d->reqmode = 0;
	d->pstate = DEV_CLOSED;
	d->hold = 0;
	d->next = dev_list;
	dev_list = d;
	return d;
}

/*
 * Open the device with the dev_reqxxx capabilities. Setup a mixer, demuxer,
 * monitor, midi control, and any necessary conversions.
 */
int
dev_open(struct dev *d)
{
	struct file *f;
	struct aparams par;
	struct aproc *conv;
	struct abuf *buf;
	unsigned siomode;
	
	d->mode = d->reqmode;
	d->round = d->reqround;
	d->bufsz = d->reqbufsz;
	d->ipar = d->reqipar;
	d->opar = d->reqopar;
	d->rec = NULL;
	d->play = NULL;
	d->mon = NULL;
	d->submon = NULL;
	d->rate = 0;

	/*
	 * If needed, open the device (ie create dev_rec and dev_play)
	 */
	if ((d->mode & (MODE_PLAY | MODE_REC)) && !(d->mode & MODE_LOOP)) {
		siomode = d->mode & (MODE_PLAY | MODE_REC);
		f = (struct file *)siofile_new(&siofile_ops,
		    d->path,
		    &siomode,
		    &d->ipar,
		    &d->opar,
		    &d->bufsz,
		    &d->round);
		if (f == NULL) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(d->path ? d->path : "default");
				dbg_puts(": failed to open audio device\n");
			}
#endif
			return 0;
		}
		if (!(siomode & MODE_PLAY))
			d->mode &= ~(MODE_PLAY | MODE_MON);
		if (!(siomode & MODE_REC))
			d->mode &= ~MODE_REC;
		if ((d->mode & (MODE_PLAY | MODE_REC)) == 0) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(d->path ? d->path : "default");
				dbg_puts(": mode not supported by device\n");
			}
#endif
			return 0;
		}
		d->rate = d->mode & MODE_REC ? d->ipar.rate : d->opar.rate;
#ifdef DEBUG
		if (debug_level >= 2) {
			if (d->mode & MODE_REC) {
				dbg_puts("hw recording ");
				aparams_dbg(&d->ipar);
				dbg_puts("\n");
			}
			if (d->mode & MODE_PLAY) {
				dbg_puts("hw playing ");
				aparams_dbg(&d->opar);
				dbg_puts("\n");
			}
		}
#endif
		if (d->mode & MODE_REC) {
			d->rec = rsio_new(f);
			d->rec->refs++;
		}
		if (d->mode & MODE_PLAY) {
			d->play = wsio_new(f);
			d->play->refs++;
		}
	}

	/*
	 * Create the midi control end, or a simple thru box
	 * if there's no device
	 */
	d->midi = (d->mode == 0) ? thru_new("thru") : ctl_new("ctl", d);
	d->midi->refs++;

	/*
	 * Create mixer, demuxer and monitor
	 */
	if (d->mode & MODE_PLAY) {
		d->mix = mix_new("play", d->bufsz, d->round);
		d->mix->refs++;
		d->mix->u.mix.ctl = d->midi;
	}
	if (d->mode & MODE_REC) {
		d->sub = sub_new("rec", d->bufsz, d->round);
		d->sub->refs++;
		/*
		 * If not playing, use the record end as clock source
		 */
		if (!(d->mode & MODE_PLAY))
			d->sub->u.sub.ctl = d->midi;
	}
	if (d->mode & MODE_LOOP) {
		/*
		 * connect mixer out to demuxer in
		 */
		buf = abuf_new(d->bufsz, &d->opar);
		aproc_setout(d->mix, buf);
		aproc_setin(d->sub, buf);

		d->mix->flags |= APROC_QUIT;
		d->sub->flags |= APROC_QUIT;
		d->rate = d->opar.rate;
	}
	if (d->rec) {
		aparams_init(&par, d->ipar.cmin, d->ipar.cmax, d->rate);

		/*
		 * Create device <-> demuxer buffer
		 */
		buf = abuf_new(d->bufsz, &d->ipar);
		aproc_setout(d->rec, buf);

		/*
		 * Insert a converter, if needed.
		 */
		if (!aparams_eqenc(&d->ipar, &par)) {
			conv = dec_new("rec", &d->ipar);
			aproc_setin(conv, buf);
			buf = abuf_new(d->round, &par);
			aproc_setout(conv, buf);
		}
		d->ipar = par;
		aproc_setin(d->sub, buf);
	}
	if (d->play) {
		aparams_init(&par, d->opar.cmin, d->opar.cmax, d->rate);

		/*
		 * Create device <-> mixer buffer
		 */
		buf = abuf_new(d->bufsz, &d->opar);
		aproc_setin(d->play, buf);

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(&par, &d->opar)) {
			conv = enc_new("play", &d->opar);
			aproc_setout(conv, buf);
			buf = abuf_new(d->round, &par);
			aproc_setin(conv, buf);
		}
		d->opar = par;
		aproc_setout(d->mix, buf);
	}
	if (d->mode & MODE_MON) {
		d->mon = mon_new("mon", d->bufsz);
		d->mon->refs++;
		buf = abuf_new(d->bufsz, &d->opar);
		aproc_setout(d->mon, buf);

		/*
		 * Append a "sub" to which clients will connect.
		 */
		d->submon = sub_new("mon", d->bufsz, d->round);
		d->submon->refs++;
		aproc_setin(d->submon, buf);

		/*
		 * Attach to the mixer
		 */
		d->mix->u.mix.mon = d->mon;
		d->mon->refs++;
	}
#ifdef DEBUG
	if (debug_level >= 2) { 
		if (d->mode & (MODE_PLAY | MODE_RECMASK)) {
			dbg_puts("device block size is ");
			dbg_putu(d->round);
			dbg_puts(" frames, using ");
			dbg_putu(d->bufsz / d->round);
			dbg_puts(" blocks\n");
		}
	}
#endif
	d->pstate = DEV_INIT;
	return 1;
}

/*
 * Cleanly stop and drain everything and close the device
 * once both play chain and record chain are gone.
 */
void
dev_close(struct dev *d)
{
	struct file *f;

	/*
	 * if the device is starting, ensure it actually starts
	 * so buffers are drained, else clear any buffers
	 */
	switch (d->pstate) {
	case DEV_START:
#ifdef DEBUG
		if (debug_level >= 3) 
			dbg_puts("draining device\n");
#endif
		dev_start(d);
		break;
	case DEV_INIT:
#ifdef DEBUG
		if (debug_level >= 3) 
			dbg_puts("flushing device\n");
#endif
		dev_clear(d);
		break;
	}
#ifdef DEBUG
	if (debug_level >= 2) 
		dbg_puts("closing device\n");
#endif

	if (d->mix) {
		/*
		 * Put the mixer in ``autoquit'' state and generate
		 * EOF on all inputs connected it. Once buffers are
		 * drained the mixer will terminate and shutdown the
		 * device.
		 *
		 * NOTE: since file_eof() can destroy the file and
		 * reorder the file_list, we have to restart the loop
		 * after each call to file_eof().
		 */
		if (APROC_OK(d->mix))
			mix_quit(d->mix);

		/*
		 * XXX: handle this in mix_done()
		 */
		if (APROC_OK(d->mix->u.mix.mon)) {
			d->mix->u.mix.mon->refs--;
			aproc_del(d->mix->u.mix.mon);
			d->mix->u.mix.mon = NULL;
		}
	restart_mix:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL &&
			    aproc_depend(d->mix, f->rproc)) {
				file_eof(f);
				goto restart_mix;
			}
		}
	} else if (d->sub) {
		/*
		 * Same as above, but since there's no mixer, 
		 * we generate EOF on the record-end of the
		 * device.
		 */	
	restart_sub:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL &&
			    aproc_depend(d->sub, f->rproc)) {
				file_eof(f);
				goto restart_sub;
			}
		}
	} else if (d->submon) {
		/*
		 * Same as above
		 */	
	restart_submon:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL &&
			    aproc_depend(d->submon, f->rproc)) {
				file_eof(f);
				goto restart_submon;
			}
		}
	}
	if (d->midi) {
		d->midi->flags |= APROC_QUIT;
		if (LIST_EMPTY(&d->midi->ins))
			aproc_del(d->midi);
 	restart_midi:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc &&
			    aproc_depend(d->midi, f->rproc)) {
				file_eof(f);
				goto restart_midi;
			}
		}
	}
	if (d->mix) {
		if (--d->mix->refs == 0 && (d->mix->flags & APROC_ZOMB))
			aproc_del(d->mix);
		d->mix = NULL;
	}
	if (d->play) {
		if (--d->play->refs == 0 && (d->play->flags & APROC_ZOMB))
			aproc_del(d->play);
		d->play = NULL;
	}
	if (d->sub) {
		if (--d->sub->refs == 0 && (d->sub->flags & APROC_ZOMB))
			aproc_del(d->sub);
		d->sub = NULL;
	}
	if (d->rec) {
		if (--d->rec->refs == 0 && (d->rec->flags & APROC_ZOMB))
			aproc_del(d->rec);
		d->rec = NULL;
	}
	if (d->submon) {
		if (--d->submon->refs == 0 && (d->submon->flags & APROC_ZOMB))
			aproc_del(d->submon);
		d->submon = NULL;
	}
	if (d->mon) {
		if (--d->mon->refs == 0 && (d->mon->flags & APROC_ZOMB))
			aproc_del(d->mon);
		d->mon = NULL;
	}
	if (d->midi) {
		if (--d->midi->refs == 0 && (d->midi->flags & APROC_ZOMB))
			aproc_del(d->midi);
		d->midi = NULL;
	}
	d->pstate = DEV_CLOSED;
}

/*
 * Free the device
 */
void
dev_del(struct dev *d)
{
	struct dev **p;

	if (d->pstate != DEV_CLOSED)
		dev_close(d);
	for (p = &dev_list; *p != d; p = &(*p)->next) {
#ifdef DEBUG
		if (*p == NULL) {
			dbg_puts("device to delete not on the list\n");
			dbg_panic();
		}
#endif
	}
	*p = d->next;
	free(d);
}

/*
 * Open a MIDI device and connect it to the thru box
 */
int
dev_thruadd(struct dev *d, char *name, int in, int out)
{
	struct file *f;
	struct abuf *rbuf = NULL, *wbuf = NULL;
	struct aproc *rproc, *wproc;

	if (!dev_ref(d))
		return 0;
	f = (struct file *)miofile_new(&miofile_ops, name, in, out);
	if (f == NULL)
		return 0;
	if (in) {
		rproc = rfile_new(f);
		rbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setout(rproc, rbuf);
	}
	if (out) {
		wproc = wfile_new(f);
		wbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setin(wproc, wbuf);
	}
	dev_midiattach(d, rbuf, wbuf);
	return 1;
}

/*
 * Attach a bi-directional MIDI stream to the MIDI device
 */
void
dev_midiattach(struct dev *d, struct abuf *ibuf, struct abuf *obuf)
{
	if (ibuf)
		aproc_setin(d->midi, ibuf);
	if (obuf) {
		aproc_setout(d->midi, obuf);
		if (ibuf) {
			ibuf->duplex = obuf;
			obuf->duplex = ibuf;
		}
	}
}

unsigned
dev_roundof(struct dev *d, unsigned newrate)
{
	return (d->round * newrate + d->rate / 2) / d->rate;
}

/*
 * Start the (paused) device. By default it's paused.
 */
void
dev_start(struct dev *d)
{
	struct file *f;

#ifdef DEBUG
	if (debug_level >= 2)
		dbg_puts("starting device\n");
#endif
	d->pstate = DEV_RUN;
	if (d->mode & MODE_LOOP)
		return;
	if (APROC_OK(d->mix))
		d->mix->flags |= APROC_DROP;
	if (APROC_OK(d->sub))
		d->sub->flags |= APROC_DROP;
	if (APROC_OK(d->submon))
		d->submon->flags |= APROC_DROP;
	if (APROC_OK(d->play) && d->play->u.io.file) {
		f = d->play->u.io.file;
		f->ops->start(f);
	} else if (APROC_OK(d->rec) && d->rec->u.io.file) {
		f = d->rec->u.io.file;
		f->ops->start(f);
	}
}

/*
 * Pause the device. This may trigger context switches,
 * so it shouldn't be called from aproc methods
 */
void
dev_stop(struct dev *d)
{
	struct file *f;

#ifdef DEBUG
	if (debug_level >= 2)
		dbg_puts("device stopped\n");
#endif
	d->pstate = DEV_INIT;
	if (d->mode & MODE_LOOP)
		return;
	if (APROC_OK(d->play) && d->play->u.io.file) {
		f = d->play->u.io.file;
		f->ops->stop(f);
	} else if (APROC_OK(d->rec) && d->rec->u.io.file) {
		f = d->rec->u.io.file;
		f->ops->stop(f);
	}
	if (APROC_OK(d->mix))
		d->mix->flags &= ~APROC_DROP;
	if (APROC_OK(d->sub))
		d->sub->flags &= ~APROC_DROP;
	if (APROC_OK(d->submon))
		d->submon->flags &= ~APROC_DROP;
}

int
dev_ref(struct dev *d)
{
#ifdef DEBUG
	if (debug_level >= 3)
		dbg_puts("device requested\n");
#endif
	if (d->pstate == DEV_CLOSED && !dev_open(d)) {
		if (d->hold)
			dev_del(d);
		return 0;
	}
	d->refcnt++;
	return 1;
}

void
dev_unref(struct dev *d)
{
#ifdef DEBUG
	if (debug_level >= 3)
		dbg_puts("device released\n");
#endif
	d->refcnt--;
	if (d->refcnt == 0 && d->pstate == DEV_INIT && !d->hold)
		dev_close(d);
}

/*
 * There are actions (like start/stop/close ... ) that may trigger aproc
 * operations, a thus cannot be started from aproc context.
 * To avoid problems, aprocs only change the s!tate of the device,
 * and actual operations are triggered from the main loop,
 * outside the aproc code path.
 *
 * The following routine invokes pending actions, returns 0
 * on fatal error
 */
int
dev_run(struct dev *d)
{
	if (d->pstate == DEV_CLOSED)
		return 1;
	/*
	 * check if device isn't gone
	 */
	if (((d->mode & MODE_PLAY) && !APROC_OK(d->mix)) ||
	    ((d->mode & MODE_REC)  && !APROC_OK(d->sub)) ||
	    ((d->mode & MODE_MON)  && !APROC_OK(d->submon))) {
#ifdef DEBUG
		if (debug_level >= 1)
			dbg_puts("device disappeared\n");
#endif
		if (d->hold) {
			dev_del(d);
			return 0;
		}
		dev_close(d);
		return 1;
	}
	switch (d->pstate) {
	case DEV_INIT:
		/* nothing */
		break;
	case DEV_START:
		dev_start(d);
		/* PASSTHROUGH */
	case DEV_RUN:
		/*
		 * if the device is not used, then stop it
		 */
		if ((!APROC_OK(d->mix) ||
			d->mix->u.mix.idle > 2 * d->bufsz) &&
		    (!APROC_OK(d->sub) ||
			d->sub->u.sub.idle > 2 * d->bufsz) &&
		    (!APROC_OK(d->submon) ||
			d->submon->u.sub.idle > 2 * d->bufsz) &&
		    (!APROC_OK(d->midi) ||
			d->midi->u.ctl.tstate != CTL_RUN)) {
#ifdef DEBUG
			if (debug_level >= 3)
				dbg_puts("device idle, suspending\n");
#endif
			dev_stop(d);
			if (d->refcnt == 0 && !d->hold)
				dev_close(d);
			else
				dev_clear(d);
		}
		break;
	}
	return 1;
}

/*
 * If the device is paused, then resume it.
 * This routine can be called from aproc context.
 */
void
dev_wakeup(struct dev *d)
{
	if (d->pstate == DEV_INIT)
		d->pstate = DEV_START;
}

/*
 * Find the end points connected to the mix/sub.
 */
int
dev_getep(struct dev *d,
    unsigned mode, struct abuf **sibuf, struct abuf **sobuf)
{
	struct abuf *ibuf, *obuf;

	if (mode & MODE_PLAY) {
		if (!APROC_OK(d->mix))
			return 0;
		ibuf = *sibuf;
		for (;;) {
			if (!ibuf || !ibuf->rproc) {
#ifdef DEBUG
				if (debug_level >= 3) {
					abuf_dbg(*sibuf);
					dbg_puts(": not connected to device\n");
				}
#endif
				return 0;
			}
			if (ibuf->rproc == d->mix)
				break;
			ibuf = LIST_FIRST(&ibuf->rproc->outs);
		}
		*sibuf = ibuf;
	}
	if (mode & MODE_REC) {
		if (!APROC_OK(d->sub))
			return 0;
		obuf = *sobuf;
		for (;;) {
			if (!obuf || !obuf->wproc) {
#ifdef DEBUG
				if (debug_level >= 3) {
					abuf_dbg(*sobuf);
					dbg_puts(": not connected to device\n");
				}
#endif
				return 0;
			}
			if (obuf->wproc == d->sub)
				break;
			obuf = LIST_FIRST(&obuf->wproc->ins);
		}
		*sobuf = obuf;
	}
	if (mode & MODE_MON) {
		if (!APROC_OK(d->submon))
			return 0;
		obuf = *sobuf;
		for (;;) {
			if (!obuf || !obuf->wproc) {
#ifdef DEBUG
				if (debug_level >= 3) {
					abuf_dbg(*sobuf);
					dbg_puts(": not connected to device\n");
				}
#endif
				return 0;
			}
			if (obuf->wproc == d->submon)
				break;
			obuf = LIST_FIRST(&obuf->wproc->ins);
		}
		*sobuf = obuf;
	}
	return 1;
}

/*
 * Sync play buffer to rec buffer (for instance when one of
 * them underruns/overruns).
 */
void
dev_sync(struct dev *d, unsigned mode, struct abuf *ibuf, struct abuf *obuf)
{
	int delta, offs;
	struct abuf *mbuf = NULL;

	if (!dev_getep(d, mode, &ibuf, &obuf))
		return;
	/*
	 * Calculate delta, the number of frames the play chain is ahead
	 * of the record chain. It's necessary to schedule silences (or
	 * drops) in order to start playback and record in sync.
	 */
	offs = 0;
	delta = 0;
	if (APROC_OK(d->mix)) {
		mbuf = LIST_FIRST(&d->mix->outs);
		offs += mbuf->w.mix.todo;
		delta += d->mix->u.mix.lat;
	}
	if (APROC_OK(d->sub))
		delta += d->sub->u.sub.lat;
#ifdef DEBUG
	if (debug_level >= 3) {
		dbg_puts("syncing device");
		if (APROC_OK(d->mix)) {
			dbg_puts(", ");
			aproc_dbg(d->mix);
			dbg_puts(": todo = ");
			dbg_putu(mbuf->w.mix.todo);
			dbg_puts(": lat = ");
			dbg_putu(d->mix->u.mix.lat);
		}
		if (APROC_OK(d->sub)) {
			dbg_puts(", ");
			aproc_dbg(d->sub);
			dbg_puts(": lat = ");
			dbg_putu(d->sub->u.sub.lat);
		}
		dbg_puts("\n");
	}
#endif
	if (mode & MODE_PLAY)
	 	mix_drop(ibuf, -offs);
	if (mode & MODE_RECMASK)
		sub_silence(obuf, -(offs + delta));
}

/*
 * return the current latency (in frames), ie the latency that
 * a stream would have if dev_attach() is called on it.
 */
int
dev_getpos(struct dev *d)
{
	struct abuf *mbuf = NULL;

	if (APROC_OK(d->mix)) {
		mbuf = LIST_FIRST(&d->mix->outs);
		return -(mbuf->w.mix.todo + d->mix->u.mix.lat);
	} else
		return 0;
}

/*
 * Attach the given input and output buffers to the mixer and the
 * multiplexer respectively. The operation is done synchronously, so
 * both buffers enter in sync. If buffers do not match play
 * and rec.
 */
void
dev_attach(struct dev *d, char *name, unsigned mode,
    struct abuf *ibuf, struct aparams *sipar, unsigned inch,
    struct abuf *obuf, struct aparams *sopar, unsigned onch,
    unsigned xrun, int vol)
{
	struct abuf *pbuf = NULL, *rbuf = NULL;
	struct aparams ipar, opar;
	struct aproc *conv;
	unsigned round, nblk, nch;

#ifdef DEBUG
	if ((!APROC_OK(d->mix)    && (mode & MODE_PLAY)) ||
	    (!APROC_OK(d->sub)    && (mode & MODE_REC)) ||
	    (!APROC_OK(d->submon) && (mode & MODE_MON))) {
	    	dbg_puts("mode beyond device mode, not attaching\n");
		return;
	}
#endif
	if (mode & MODE_PLAY) {
		ipar = *sipar;
		pbuf = LIST_FIRST(&d->mix->outs);
		nblk = (d->bufsz / d->round + 3) / 4;
		round = dev_roundof(d, ipar.rate);
		nch = ipar.cmax - ipar.cmin + 1;
		if (!aparams_eqenc(&ipar, &d->opar)) {
			conv = dec_new(name, &ipar);
			ipar.bps = d->opar.bps;
			ipar.bits = d->opar.bits;
			ipar.sig = d->opar.sig;
			ipar.le = d->opar.le;
			ipar.msb = d->opar.msb;
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		if (inch > 0 && nch >= inch * 2) {
			conv = join_new(name);
			aproc_setin(conv, ibuf);
			ipar.cmax = ipar.cmin + inch - 1;
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		if (!aparams_eqrate(&ipar, &d->opar)) {
			conv = resamp_new(name, round, d->round);
			ipar.rate = d->opar.rate;
			round = d->round;
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		if (inch > 0 && nch * 2 <= inch) {
			conv = join_new(name);
			aproc_setin(conv, ibuf);
			ipar.cmax = ipar.cmin + inch - 1;
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		aproc_setin(d->mix, ibuf);
		ibuf->r.mix.xrun = xrun;
		ibuf->r.mix.maxweight = vol;
		mix_setmaster(d->mix);
	}
	if (mode & MODE_REC) {
		opar = *sopar;
		rbuf = LIST_FIRST(&d->sub->ins);
		round = dev_roundof(d, opar.rate);
		nblk = (d->bufsz / d->round + 3) / 4;
		nch = opar.cmax - opar.cmin + 1;
		if (!aparams_eqenc(&opar, &d->ipar)) {
			conv = enc_new(name, &opar);
			opar.bps = d->ipar.bps;
			opar.bits = d->ipar.bits;
			opar.sig = d->ipar.sig;
			opar.le = d->ipar.le;
			opar.msb = d->ipar.msb;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (onch > 0 && nch >= onch * 2) {
			conv = join_new(name);
			aproc_setout(conv, obuf);
			opar.cmax = opar.cmin + onch - 1;
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (!aparams_eqrate(&opar, &d->ipar)) {
			conv = resamp_new(name, d->round, round);
			opar.rate = d->ipar.rate;
			round = d->round;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (onch > 0 && nch * 2 <= onch) {
			conv = join_new(name);
			aproc_setout(conv, obuf);
			opar.cmax = opar.cmin + onch - 1;
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		aproc_setout(d->sub, obuf);
		obuf->w.sub.xrun = xrun;
	}
	if (mode & MODE_MON) {
		opar = *sopar;
		rbuf = LIST_FIRST(&d->submon->ins);
		round = dev_roundof(d, opar.rate);
		nblk = (d->bufsz / d->round + 3) / 4;
		nch = opar.cmax - opar.cmin + 1;
		if (!aparams_eqenc(&opar, &d->opar)) {
			conv = enc_new(name, &opar);
			opar.bps = d->opar.bps;
			opar.bits = d->opar.bits;
			opar.sig = d->opar.sig;
			opar.le = d->opar.le;
			opar.msb = d->opar.msb;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (onch > 0 && nch >= onch * 2) {
			conv = join_new(name);
			aproc_setout(conv, obuf);
			opar.cmax = opar.cmin + onch - 1;
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (!aparams_eqrate(&opar, &d->opar)) {
			conv = resamp_new(name, d->round, round);
			opar.rate = d->opar.rate;
			round = d->round;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (onch > 0 && nch * 2 <= onch) {
			conv = join_new(name);
			aproc_setout(conv, obuf);
			opar.cmax = opar.cmin + onch - 1;
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		aproc_setout(d->submon, obuf);
		obuf->w.sub.xrun = xrun;
	}

	/*
	 * Sync play to record.
	 */
	if ((mode & MODE_PLAY) && (mode & MODE_RECMASK)) {
		ibuf->duplex = obuf;
		obuf->duplex = ibuf;
	}
	dev_sync(d, mode, ibuf, obuf);
}

/*
 * Change the playback volume of the given stream.
 */
void
dev_setvol(struct dev *d, struct abuf *ibuf, int vol)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(ibuf);
		dbg_puts(": setting volume to ");
		dbg_putu(vol);
		dbg_puts("\n");
	}
#endif
	if (!dev_getep(d, MODE_PLAY, &ibuf, NULL)) {
		return;
	}
	ibuf->r.mix.vol = vol;
}

/*
 * Clear buffers of the play and record chains so that when the device
 * is started, playback and record start in sync.
 */
void
dev_clear(struct dev *d)
{
	struct abuf *buf;

	if (APROC_OK(d->mix)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&d->mix->ins)) {
			dbg_puts("play end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&d->mix->outs);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->rproc->outs);
		}
		mix_clear(d->mix);
	}
	if (APROC_OK(d->sub)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&d->sub->outs)) {
			dbg_puts("record end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&d->sub->ins);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ins);
		}
		sub_clear(d->sub);
	}
	if (APROC_OK(d->submon)) {
#ifdef DEBUG
		dbg_puts("clearing monitor\n");
		if (!LIST_EMPTY(&d->submon->outs)) {
			dbg_puts("monitoring end not idle, can't clear device\n");
			dbg_panic();
		}
#endif
		buf = LIST_FIRST(&d->submon->ins);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ins);
		}
		sub_clear(d->submon);
		mon_clear(d->mon);
	}
}
