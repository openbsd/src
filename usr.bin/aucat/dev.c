/*	$OpenBSD: dev.c,v 1.85 2014/08/10 10:25:35 ratchov Exp $	*/
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "pipe.h"
#include "miofile.h"
#include "siofile.h"
#include "midi.h"
#ifdef DEBUG
#include "dbg.h"
#endif

int  dev_open(struct dev *);
void dev_close(struct dev *);
void dev_start(struct dev *);
void dev_stop(struct dev *);
void dev_clear(struct dev *);
void dev_onmove(void *, int);
int  devctl_open(struct dev *, struct devctl *);
int  dev_getep(struct dev *, unsigned int, struct abuf **, struct abuf **);
void dev_sync(struct dev *, unsigned int, struct abuf *, struct abuf *);
int  dev_mkslot(struct dev *, char *);
int  dev_try(struct dev *, int);


struct dev *dev_list = NULL;
unsigned int dev_sndnum = 0, dev_thrnum = 0;

#ifdef DEBUG
void
dev_dbg(struct dev *d)
{
	if (d->num >= DEV_NMAX) {
		dbg_puts("thr");
		dbg_putu(d->num - DEV_NMAX);
	} else {
		dbg_puts("snd");
		dbg_putu(d->num);
	}
}
#endif

/*
 * Create a sndio device
 */
struct dev *
dev_new(char *path, unsigned int mode, unsigned int bufsz, unsigned int round,
    unsigned int hold, unsigned int autovol)
{
	struct dev *d;
	unsigned int *pnum, i;

	pnum = (mode & MODE_THRU) ? &dev_thrnum : &dev_sndnum;
	if (*pnum == DEV_NMAX) {
#ifdef DEBUG
		if (debug_level >= 1)
			dbg_puts("too many devices\n");
#endif
		return NULL;
	}
	d = malloc(sizeof(struct dev));
	if (d == NULL) {
		perror("malloc");
		exit(1);
	}
	d->num = (*pnum)++;
	if (mode & MODE_THRU)
		d->num += DEV_NMAX;
	d->ctl_list = NULL;
	d->path = path;
	d->reqmode = mode;
	aparams_init(&d->reqopar, NCHAN_MAX, 0, 0);
	aparams_init(&d->reqipar, NCHAN_MAX, 0, 0);
	d->reqbufsz = bufsz;
	d->reqround = round;
	d->hold = hold;
	d->autovol = autovol;
	d->autostart = 0;
	d->refcnt = 0;
	d->pstate = DEV_CLOSED;
	d->serial = 0;
	for (i = 0; i < CTL_NSLOT; i++) {
		d->slot[i].unit = i;
		d->slot[i].ops = NULL;
		d->slot[i].vol = MIDI_MAXCTL;
		d->slot[i].tstate = CTL_OFF;
		d->slot[i].serial = d->serial++;
		d->slot[i].name[0] = '\0';
	}
	d->master = MIDI_MAXCTL;
	d->origin = 0;
	d->tstate = CTL_STOP;
	d->next = dev_list;
	dev_list = d;      
	return d;
}

/*
 * adjust device parameters and mode
 */
void
dev_adjpar(struct dev *d, unsigned int mode,
    struct aparams *ipar, struct aparams *opar)
{
	d->reqmode |= (mode | MODE_MIDIMASK);
	if (mode & MODE_REC)
		aparams_grow(&d->reqipar, ipar);
	if (mode & MODE_PLAY)
		aparams_grow(&d->reqopar, opar);
}

/*
 * Initialize the device with the current parameters
 */
int
dev_init(struct dev *d)
{
	if ((d->reqmode & (MODE_AUDIOMASK | MODE_MIDIMASK)) == 0) {
#ifdef DEBUG
		    dev_dbg(d);
		    dbg_puts(": has no streams, skipped\n");
#endif		    		    
		    return 1;
	}
	if (d->hold && d->pstate == DEV_CLOSED && !dev_open(d)) {
		dev_del(d);
		return 0;
	}
	return 1;
}

/*
 * Add a MIDI port to the device
 */
int
devctl_add(struct dev *d, char *path, unsigned int mode)
{
	struct devctl *c;

	c = malloc(sizeof(struct devctl));
	if (c == NULL) {
		perror("malloc");
		exit(1);
	}
	c->path = path;
	c->mode = mode;
	c->next = d->ctl_list;
	d->ctl_list = c;
	if (d->pstate != DEV_CLOSED && !devctl_open(d, c))
		return 0;
	return 1;
}

/*
 * Open a MIDI device and connect it to the thru box
 */
int
devctl_open(struct dev *d, struct devctl *c)
{
	struct file *f;
	struct abuf *rbuf = NULL, *wbuf = NULL;
	struct aproc *rproc, *wproc;

	f = (struct file *)miofile_new(&miofile_ops, c->path, c->mode);
	if (f == NULL)
		return 0;
	if (c->mode & MODE_MIDIIN) {
		rproc = rfile_new(f);
		rbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setout(rproc, rbuf);
	}
	if (c->mode & MODE_MIDIOUT) {
		wproc = wfile_new(f);
		wbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setin(wproc, wbuf);
	}
	dev_midiattach(d, rbuf, wbuf);
	return 1;
}

/*
 * Open the device with the dev_reqxxx capabilities. Setup a mixer, demuxer,
 * monitor, midi control, and any necessary conversions.
 */
int
dev_open(struct dev *d)
{
	struct file *f;
	struct devctl *c;
	struct aparams par;
	struct aproc *conv;
	struct abuf *buf;
	unsigned int siomode, cmin, cmax, rate;
	
	d->mode = d->reqmode;
	d->round = d->reqround;
	d->bufsz = d->reqbufsz;
	d->ipar = d->reqipar;
	d->opar = d->reqopar;
	d->rec = NULL;
	d->play = NULL;
	d->mon = NULL;
	d->mix = NULL;
	d->sub = NULL;
	d->submon = NULL;
	d->midi = NULL;
	d->rate = 0;

	if (d->opar.cmin > d->opar.cmax) {
		d->opar.cmin = 0;
		d->opar.cmax = 1;
	}
	if (d->ipar.cmin > d->ipar.cmax) {
		d->ipar.cmin = 0;
		d->ipar.cmax = 1;
	}
	if (d->opar.rate > d->ipar.rate)
		d->ipar.rate = d->opar.rate;
	else
		d->opar.rate = d->ipar.rate;
	if (d->opar.rate == 0)
		d->opar.rate = d->ipar.rate = 48000; /* XXX */

	if (d->mode & MODE_THRU)
		d->mode &= ~MODE_AUDIOMASK;

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
				dev_dbg(d);
				dbg_puts(": ");
				dbg_puts(d->path);
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
				dev_dbg(d);
				dbg_puts(": mode not supported by device\n");
			}
#endif
			return 0;
		}
		d->rate = d->mode & MODE_REC ? d->ipar.rate : d->opar.rate;
		if (d->mode & MODE_REC) {
			d->rec = rsio_new(f);
			d->rec->refs++;
		}
		if (d->mode & MODE_PLAY) {
			d->play = wsio_new(f);
			d->play->refs++;
		}
	}
	if (d->mode & MODE_LOOP) {
		if (d->mode & MODE_MON) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts("monitoring not allowed "
				    "in loopback mode\n");
			}
#endif
			return 0;
		}
		if ((d->mode & MODE_PLAYREC) != MODE_PLAYREC) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts("both play and record streams "
				    "required in loopback mode\n");
			}
#endif
			return 0;
		}
		if (d->ctl_list) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts("MIDI control not allowed "
				    "in loopback mode\n");
			}
#endif
			return 0;
		}
		cmin = (d->ipar.cmin < d->opar.cmin) ?
		    d->ipar.cmin : d->opar.cmin;
		cmax = (d->ipar.cmax > d->opar.cmax) ?
		    d->ipar.cmax : d->opar.cmax;
		rate = (d->ipar.rate > d->opar.rate) ?
		    d->ipar.rate : d->opar.rate;
		aparams_init(&par, cmin, cmax, rate);
		d->ipar = par;
		d->opar = par;
		d->rate = rate;
		/* 
		 * block sizes in the resampling code are limited to
		 * 2^15, so use 1/15 of the rate, since all standard
		 * sample rates are multiple of 15
		 */
		d->round = rate / 15;
		d->bufsz = 2 * d->round;
	}
#ifdef DEBUG
	if (debug_level >= 2) {
		if (d->mode & MODE_REC) {
			dev_dbg(d);
			dbg_puts(": recording ");
			aparams_dbg(&d->ipar);
			dbg_puts("\n");
		}
		if (d->mode & MODE_PLAY) {
			dev_dbg(d);
			dbg_puts(": playing ");
			aparams_dbg(&d->opar);
			dbg_puts("\n");
		}
	}
#endif
	/*
	 * Create the midi control end, or a simple thru box
	 * if there's no device
	 */
	if (d->mode & MODE_MIDIMASK) {
		d->midi = midi_new("midi", (d->mode & MODE_THRU) ? NULL : d);
		d->midi->refs++;
	}

	/*
	 * Create mixer, demuxer and monitor
	 */
	if (d->mode & MODE_PLAY) {
		d->mix = mix_new("play", d->bufsz, d->round,
		    d->autovol, MIDI_TO_ADATA(d->master));
		d->mix->refs++;
	}
	if (d->mode & MODE_REC) {
		d->sub = sub_new("rec", d->bufsz, d->round);
		d->sub->refs++;
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
			dev_dbg(d);
			dbg_puts(": block size is ");
			dbg_putu(d->round);
			dbg_puts(" frames, using ");
			dbg_putu(d->bufsz / d->round);
			dbg_puts(" blocks\n");
		}
	}
#endif
	d->pstate = DEV_INIT;
	for (c = d->ctl_list; c != NULL; c = c->next) {
		if (!devctl_open(d, c)) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(c->path);
				dbg_puts(": couldn't open MIDI port\n");
			}
#endif
			dev_close(d);
			return 0;
		}
	}
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
		if (debug_level >= 3) {
			dev_dbg(d);
			dbg_puts(": draining device\n");
		}
#endif
		dev_start(d);
		break;
	case DEV_INIT:
#ifdef DEBUG
		if (debug_level >= 3) {
			dev_dbg(d);
			dbg_puts(": flushing device\n");
		}
#endif
		dev_clear(d);
		break;
	}
#ifdef DEBUG
	if (debug_level >= 2) {
		dev_dbg(d);
		dbg_puts(": closing device\n");
	}
#endif
	d->pstate = DEV_CLOSED;
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
}

/*
 * Unless the device is already in process of closing, request it to close
 */
void
dev_drain(struct dev *d)
{
	unsigned int i;
	struct ctl_slot *s;

	for (i = 0, s = d->slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops)
			s->ops->quit(s->arg);
	}
	if (d->pstate != DEV_CLOSED)
		dev_close(d);
}

/*
 * Free the device
 */
void
dev_del(struct dev *d)
{
	struct dev **p;

	dev_drain(d);
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

unsigned int
dev_roundof(struct dev *d, unsigned int newrate)
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
		f->ops->start(f, dev_onmove, d);
	} else if (APROC_OK(d->rec) && d->rec->u.io.file) {
		f = d->rec->u.io.file;
		f->ops->start(f, dev_onmove, d);
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
	if (debug_level >= 2) {
		dev_dbg(d);
		dbg_puts(": device stopped\n");
	}
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
	if (debug_level >= 3) {
		dev_dbg(d);
		dbg_puts(": device requested\n");
	}
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
	if (debug_level >= 3) {
		dev_dbg(d);
		dbg_puts(": device released\n");
	}
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
		if (debug_level >= 2) {
			dev_dbg(d);
			dbg_puts(": device disappeared\n");
		}
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
			d->tstate != CTL_RUN)) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dev_dbg(d);
				dbg_puts(": device idle, suspending\n");
			}
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
    unsigned int mode, struct abuf **sibuf, struct abuf **sobuf)
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
dev_sync(struct dev *d, unsigned int mode,
    struct abuf *ibuf, struct abuf *obuf)
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
		dev_dbg(d);
		dbg_puts(": syncing device");
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
 *
 * XXX: return a "unsigned int", since result is always positive, isn't it?
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
dev_attach(struct dev *d, char *name, unsigned int mode,
    struct abuf *ibuf, struct aparams *sipar, unsigned int inch,
    struct abuf *obuf, struct aparams *sopar, unsigned int onch,
    unsigned int xrun, int vol)
{
	struct aparams ipar, opar;
	struct aproc *conv;
	unsigned int round, nblk, nch;

#ifdef DEBUG
	if ((!APROC_OK(d->mix)    && (mode & MODE_PLAY)) ||
	    (!APROC_OK(d->sub)    && (mode & MODE_REC)) ||
	    (!APROC_OK(d->submon) && (mode & MODE_MON))) {
		dev_dbg(d);
	    	dbg_puts(": mode beyond device mode, not attaching\n");
		return;
	}
#endif
	if (mode & MODE_PLAY) {
		ipar = *sipar;
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
			dev_dbg(d);
			dbg_puts(": play end not idle, can't clear device\n");
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
			dev_dbg(d);
			dbg_puts(": record end not idle, can't clear device\n");
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
		if (!LIST_EMPTY(&d->submon->outs)) {
			dev_dbg(d);
			dbg_puts(": monitoring end not idle, can't clear device\n");
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

#ifdef DEBUG
void
dev_slotdbg(struct dev *d, int slot)
{
	struct ctl_slot *s;

	if (slot < 0) {
		dbg_puts("none");
	} else {
		s = d->slot + slot;
		dbg_puts(s->name);
		dbg_putu(s->unit);
		dbg_puts("(");
		dbg_putu(s->vol);
		dbg_puts(")/");
		switch (s->tstate) {
		case CTL_OFF:
			dbg_puts("off");
			break;
		case CTL_RUN:
			dbg_puts("run");
			break;
		case CTL_START:
			dbg_puts("sta");
			break;
		case CTL_STOP:
			dbg_puts("stp");
			break;
		default:
			dbg_puts("unk");
			break;
		}
	}
}
#endif

/*
 * find the best matching free slot index (ie midi channel).
 * return -1, if there are no free slots anymore
 */
int
dev_mkslot(struct dev *d, char *who)
{
	char *s;
	struct ctl_slot *slot;
	char name[CTL_NAMEMAX];
	unsigned int i, unit, umap = 0;
	unsigned int ser, bestser, bestidx;

	/*
	 * create a ``valid'' control name (lowcase, remove [^a-z], trucate)
	 */
	for (i = 0, s = who; ; s++) {
		if (i == CTL_NAMEMAX - 1 || *s == '\0') {
			name[i] = '\0';
			break;
		} else if (*s >= 'A' && *s <= 'Z') {
			name[i++] = *s + 'a' - 'A';
		} else if (*s >= 'a' && *s <= 'z')
			name[i++] = *s;
	}
	if (i == 0)
		strlcpy(name, "noname", CTL_NAMEMAX);

	/*
	 * find the instance number of the control name
	 */
	for (i = 0, slot = d->slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops == NULL)
			continue;
		if (strcmp(slot->name, name) == 0)
			umap |= (1 << slot->unit);
	} 
	for (unit = 0; ; unit++) {
		if (unit == CTL_NSLOT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(name);
				dbg_puts(": too many instances\n");
			}
#endif
			return -1;
		}
		if ((umap & (1 << unit)) == 0)
			break;
	}

	/*
	 * find a free controller slot with the same name/unit
	 */
	for (i = 0, slot = d->slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops == NULL &&
		    strcmp(slot->name, name) == 0 &&
		    slot->unit == unit) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dbg_puts(name);
				dbg_putu(unit);
				dbg_puts(": found slot ");
				dbg_putu(i);
				dbg_puts("\n");
			}
#endif
			return i;
		}
	}

	/*
	 * couldn't find a matching slot, pick oldest free slot
	 * and set its name/unit
	 */
	bestser = 0;
	bestidx = CTL_NSLOT;
	for (i = 0, slot = d->slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops != NULL)
			continue;
		ser = d->serial - slot->serial;
		if (ser > bestser) {
			bestser = ser;
			bestidx = i;
		}
	}
	if (bestidx == CTL_NSLOT) {
#ifdef DEBUG
		if (debug_level >= 1) {
			dbg_puts(name);
			dbg_putu(unit);
			dbg_puts(": out of mixer slots\n");
		}
#endif
		return -1;
	}
	slot = d->slot + bestidx;
	if (slot->name[0] != '\0')
		slot->vol = MIDI_MAXCTL;
	strlcpy(slot->name, name, CTL_NAMEMAX);
	slot->serial = d->serial++;
	slot->unit = unit;
#ifdef DEBUG
	if (debug_level >= 3) {
		dbg_puts(name);
		dbg_putu(unit);
		dbg_puts(": overwritten slot ");
		dbg_putu(bestidx);
		dbg_puts("\n");
	}
#endif
	return bestidx;
}

/*
 * allocate a new slot and register the given call-backs
 */
int
dev_slotnew(struct dev *d, char *who, struct ctl_ops *ops, void *arg, int mmc)
{
	int slot;
	struct ctl_slot *s;

	slot = dev_mkslot(d, who);
	if (slot < 0)
		return -1;

	s = d->slot + slot;
	s->ops = ops;
	s->arg = arg;
	s->tstate = mmc ? CTL_STOP : CTL_OFF;
	s->ops->vol(s->arg, s->vol);

	if (APROC_OK(d->midi)) {
		midi_send_slot(d->midi, slot);
		midi_send_vol(d->midi, slot, s->vol);
		midi_flush(d->midi);
	} else {
#ifdef DEBUG
		if (debug_level >= 2) {
			dev_slotdbg(d, slot);
			dbg_puts(": MIDI control not available\n");
		}
#endif
	}
	return slot;
}

/*
 * release the given slot
 */
void
dev_slotdel(struct dev *d, int slot)
{
	struct ctl_slot *s;

	s = d->slot + slot;
	s->ops = NULL;
}

/*
 * notifty the mixer that volume changed, called by whom allocad the slot using
 * ctl_slotnew(). Note: it doesn't make sens to call this from within the
 * call-back.
 *
 * XXX: set actual volume here and use only this interface. Now, this
 *	can work because all streams have a slot
 */
void
dev_slotvol(struct dev *d, int slot, unsigned int vol)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		dev_slotdbg(d, slot);
		dbg_puts(": changing volume to ");
		dbg_putu(vol);
		dbg_puts("\n");
	}
#endif
	d->slot[slot].vol = vol;
	if (APROC_OK(d->midi)) {
		midi_send_vol(d->midi, slot, vol);
		midi_flush(d->midi);
	}
}

/*
 * check that all clients controlled by MMC are ready to start,
 * if so, start them all but the caller
 */
int
dev_try(struct dev *d, int slot)
{
	unsigned int i;
	struct ctl_slot *s;

	if (d->tstate != CTL_START) {
#ifdef DEBUG
		if (debug_level >= 3) {
			dev_slotdbg(d, slot);
			dbg_puts(": server not started, delayed\n");
		}
#endif
		return 0;
	}
	for (i = 0, s = d->slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops || i == slot)
			continue;
		if (s->tstate != CTL_OFF && s->tstate != CTL_START) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dev_slotdbg(d, i);
				dbg_puts(": not ready, server delayed\n");
			}
#endif
			return 0;
		}
	}
	for (i = 0, s = d->slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops || i == slot)
			continue;
		if (s->tstate == CTL_START) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dev_slotdbg(d, i);
				dbg_puts(": started\n");
			}
#endif
			s->tstate = CTL_RUN;
			s->ops->start(s->arg);
		}
	}
	if (slot >= 0)
		d->slot[slot].tstate = CTL_RUN;
	d->tstate = CTL_RUN;
	if (APROC_OK(d->midi)) {
		midi_send_full(d->midi,
		    d->origin, d->rate, d->round, dev_getpos(d));
		midi_flush(d->midi);
	}
	dev_wakeup(d);
	return 1;
}

/*
 * notify the MMC layer that the stream is attempting
 * to start. If other streams are not ready, 0 is returned meaning 
 * that the stream should wait. If other streams are ready, they
 * are started, and the caller should start immediately.
 */
int
dev_slotstart(struct dev *d, int slot)
{
	struct ctl_slot *s = d->slot + slot;

	if (s->tstate == CTL_OFF || d->tstate == CTL_OFF)
		return 1;

	/*
	 * if the server already started (the client missed the
	 * start rendez-vous) or the server is stopped, then
	 * tag the client as ``wanting to start''
	 */
	s->tstate = CTL_START;
	return dev_try(d, slot);
}

/*
 * notify the MMC layer that the stream no longer is trying to
 * start (or that it just stopped), meaning that its ``start'' call-back
 * shouldn't be called anymore
 */
void
dev_slotstop(struct dev *d, int slot)
{
	struct ctl_slot *s = d->slot + slot;

	/*
	 * tag the stream as not trying to start,
	 * unless MMC is turned off
	 */
	if (s->tstate != CTL_OFF)
		s->tstate = CTL_STOP;
}

/*
 * start all slots simultaneously
 */
void
dev_mmcstart(struct dev *d)
{
	if (d->tstate == CTL_STOP) {
		d->tstate = CTL_START;
		(void)dev_try(d, -1);
#ifdef DEBUG
	} else {
		if (debug_level >= 3) {
			dev_dbg(d);
			dbg_puts(": ignoring mmc start\n");
		}
#endif
	}
}

/*
 * stop all slots simultaneously
 */
void
dev_mmcstop(struct dev *d)
{
	unsigned int i;
	struct ctl_slot *s;

	switch (d->tstate) {
	case CTL_START:
		d->tstate = CTL_STOP;
		return;
	case CTL_RUN:
		d->tstate = CTL_STOP;
		break;
	default:
#ifdef DEBUG
		if (debug_level >= 3) {
			dev_dbg(d);
			dbg_puts(": ignored mmc stop\n");
		}
#endif
		return;
	}
	for (i = 0, s = d->slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops)
			continue;
		if (s->tstate == CTL_RUN) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dev_slotdbg(d, i);
				dbg_puts(": requested to stop\n");
			}
#endif
			s->ops->stop(s->arg);
		}
	}
}

/*
 * relocate all slots simultaneously
 */
void
dev_loc(struct dev *d, unsigned int origin)
{
	unsigned int i;
	struct ctl_slot *s;

#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts("server relocated to ");
		dbg_putu(origin);
		dbg_puts("\n");
	}
#endif
	if (d->tstate == CTL_RUN)
		dev_mmcstop(d);
	d->origin = origin;
	for (i = 0, s = d->slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops)
			continue;
		s->ops->loc(s->arg, d->origin);
	}
	if (d->tstate == CTL_RUN)
		dev_mmcstart(d);
}

/*
 * called at every clock tick by the mixer, delta is positive, unless
 * there's an overrun/underrun
 */
void
dev_onmove(void *arg, int delta)
{
	struct dev *d = (struct dev *)arg;

	/*
	 * don't send ticks before the start signal
	 */
	if (d->tstate != CTL_RUN)
		return;
	if (APROC_OK(d->midi)) {
		midi_send_qfr(d->midi, d->rate, delta);
		midi_flush(d->midi);
	}
}

void
dev_master(struct dev *d, unsigned int master)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		dev_dbg(d);
		dbg_puts(": changing master volume to ");
		dbg_putu(master);
		dbg_puts("\n");
	}
#endif
	d->master = master;
	if (APROC_OK(d->mix)) {
		d->mix->u.mix.master = MIDI_TO_ADATA(master);
		mix_setmaster(d->mix);
	}
}
