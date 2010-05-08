/*	$OpenBSD: dev.c,v 1.54 2010/05/08 12:29:08 ratchov Exp $	*/
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
 *   1. create the device using dev_init_xxx()
 *   2. call dev_run() in the event loop
 *   3. destroy the device using dev_done()
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

/*
 * state of the device
 */
#define DEV_CLOSED	0		/* closed */
#define DEV_INIT	1		/* stopped */
#define DEV_START	2		/* ready to start */
#define DEV_RUN		3		/* started */

/*
 * desired parameters
 */
unsigned dev_reqmode;				/* mode */
struct aparams dev_reqipar, dev_reqopar;	/* parameters */
unsigned dev_reqbufsz;				/* buffer size */
unsigned dev_reqround;				/* block size */

/*
 * actual parameters and runtime state
 */
char *dev_path;					/* sio path */
unsigned dev_refcnt = 0;			/* number of openers */
unsigned dev_pstate;				/* on of DEV_xxx */
unsigned dev_mode;				/* bitmap of MODE_xxx */
unsigned dev_bufsz, dev_round, dev_rate;
struct aparams dev_ipar, dev_opar;
struct aproc *dev_mix, *dev_sub, *dev_rec, *dev_play, *dev_submon, *dev_mon;
struct aproc *dev_midi;

void dev_start(void);
void dev_stop(void);
void dev_clear(void);
void dev_prime(void);

/*
 * Create a sndio device
 */
void
dev_init_sio(char *path, unsigned mode, 
    struct aparams *dipar, struct aparams *dopar,
    unsigned bufsz, unsigned round)
{
	dev_path = path;
	dev_reqmode = mode;
	if (mode & MODE_PLAY)
		dev_reqopar = *dopar;
	if (mode & MODE_RECMASK)
		dev_reqipar = *dipar;
	dev_reqbufsz = bufsz;
	dev_reqround = round;
	dev_pstate = DEV_CLOSED;
}

/*
 * Create a loopback synchronous device
 */
void
dev_init_loop(struct aparams *dipar, struct aparams *dopar, unsigned bufsz)
{
	struct aparams par;
	unsigned cmin, cmax, rate;

	cmin = (dipar->cmin < dopar->cmin) ? dipar->cmin : dopar->cmin;
	cmax = (dipar->cmax > dopar->cmax) ? dipar->cmax : dopar->cmax;
	rate = (dipar->rate > dopar->rate) ? dipar->rate : dopar->rate;
	aparams_init(&par, cmin, cmax, rate);
	dev_reqipar = par;
	dev_reqopar = par;
	dev_rate = rate;
	dev_reqround = (bufsz + 1) / 2;
	dev_reqbufsz = dev_reqround * 2;
	dev_reqmode = MODE_PLAY | MODE_REC | MODE_LOOP;
	dev_pstate = DEV_CLOSED;
}

/*
 * Create a MIDI thru box device
 */
void
dev_init_thru(void)
{
	dev_reqmode = 0;
	dev_pstate = DEV_CLOSED;
}

/*
 * Open the device with the dev_reqxxx capabilities. Setup a mixer, demuxer,
 * monitor, midi control, and any necessary conversions.
 */
int
dev_open(void)
{
	struct file *f;
	struct aparams par;
	struct aproc *conv;
	struct abuf *buf;
	unsigned siomode;
	
	dev_mode = dev_reqmode;
	dev_round = dev_reqround;
	dev_bufsz = dev_reqbufsz;
	dev_ipar = dev_reqipar;
	dev_opar = dev_reqopar;
	dev_rec = NULL;
	dev_play = NULL;
	dev_mon = NULL;
	dev_submon = NULL;
	dev_rate = 0;

	/*
	 * If needed, open the device (ie create dev_rec and dev_play)
	 */
	if ((dev_mode & (MODE_PLAY | MODE_REC)) && !(dev_mode & MODE_LOOP)) {
		siomode = dev_mode & (MODE_PLAY | MODE_REC);
		f = (struct file *)siofile_new(&siofile_ops,
		    dev_path,
		    &siomode,
		    &dev_ipar,
		    &dev_opar,
		    &dev_bufsz,
		    &dev_round);
		if (f == NULL) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(dev_path ? dev_path : "default");
				dbg_puts(": failed to open audio device\n");
			}
#endif
			return 0;
		}
		if (!(siomode & MODE_PLAY))
			dev_mode &= ~(MODE_PLAY | MODE_MON);
		if (!(siomode & MODE_REC))
			dev_mode &= ~MODE_REC;
		if ((dev_mode & (MODE_PLAY | MODE_REC)) == 0) {
#ifdef DEBUG
			if (debug_level >= 1) {
				dbg_puts(dev_path ? dev_path : "default");
				dbg_puts(": mode not supported by device\n");
			}
#endif
			return 0;
		}
		dev_rate = dev_mode & MODE_REC ? dev_ipar.rate : dev_opar.rate;
#ifdef DEBUG
		if (debug_level >= 2) {
			if (dev_mode & MODE_REC) {
				dbg_puts("hw recording ");
				aparams_dbg(&dev_ipar);
				dbg_puts("\n");
			}
			if (dev_mode & MODE_PLAY) {
				dbg_puts("hw playing ");
				aparams_dbg(&dev_opar);
				dbg_puts("\n");
			}
		}
#endif
		if (dev_mode & MODE_REC) {
			dev_rec = rsio_new(f);
			dev_rec->refs++;
		}
		if (dev_mode & MODE_PLAY) {
			dev_play = wsio_new(f);
			dev_play->refs++;
		}
	}

	/*
	 * Create the midi control end, or a simple thru box
	 * if there's no device
	 */
	dev_midi = (dev_mode == 0) ? thru_new("thru") : ctl_new("ctl");
	dev_midi->refs++;

	/*
	 * Create mixer, demuxer and monitor
	 */
	if (dev_mode & MODE_PLAY) {
		dev_mix = mix_new("play", dev_bufsz, dev_round);
		dev_mix->refs++;
		dev_mix->u.mix.ctl = dev_midi;
	}
	if (dev_mode & MODE_REC) {
		dev_sub = sub_new("rec", dev_bufsz, dev_round);
		dev_sub->refs++;
		/*
		 * If not playing, use the record end as clock source
		 */
		if (!(dev_mode & MODE_PLAY))
			dev_sub->u.sub.ctl = dev_midi;
	}
	if (dev_mode & MODE_LOOP) {
		/*
		 * connect mixer out to demuxer in
		 */
		buf = abuf_new(dev_bufsz, &dev_opar);
		aproc_setout(dev_mix, buf);
		aproc_setin(dev_sub, buf);

		dev_mix->flags |= APROC_QUIT;
		dev_sub->flags |= APROC_QUIT;
		dev_rate = dev_opar.rate;
	}
	if (dev_rec) {
		aparams_init(&par, dev_ipar.cmin, dev_ipar.cmax, dev_rate);

		/*
		 * Create device <-> demuxer buffer
		 */
		buf = abuf_new(dev_bufsz, &dev_ipar);
		aproc_setout(dev_rec, buf);

		/*
		 * Insert a converter, if needed.
		 */
		if (!aparams_eqenc(&dev_ipar, &par)) {
			conv = dec_new("rec", &dev_ipar);
			aproc_setin(conv, buf);
			buf = abuf_new(dev_round, &par);
			aproc_setout(conv, buf);
		}
		dev_ipar = par;
		aproc_setin(dev_sub, buf);
	}
	if (dev_play) {
		aparams_init(&par, dev_opar.cmin, dev_opar.cmax, dev_rate);

		/*
		 * Create device <-> mixer buffer
		 */
		buf = abuf_new(dev_bufsz, &dev_opar);
		aproc_setin(dev_play, buf);

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(&par, &dev_opar)) {
			conv = enc_new("play", &dev_opar);
			aproc_setout(conv, buf);
			buf = abuf_new(dev_round, &par);
			aproc_setin(conv, buf);
		}
		dev_opar = par;
		aproc_setout(dev_mix, buf);
	}
	if (dev_mode & MODE_MON) {
		dev_mon = mon_new("mon", dev_bufsz);
		dev_mon->refs++;
		buf = abuf_new(dev_bufsz, &dev_opar);
		aproc_setout(dev_mon, buf);

		/*
		 * Append a "sub" to which clients will connect.
		 */
		dev_submon = sub_new("mon", dev_bufsz, dev_round);
		dev_submon->refs++;
		aproc_setin(dev_submon, buf);

		/*
		 * Attach to the mixer
		 */
		dev_mix->u.mix.mon = dev_mon;
		dev_mon->refs++;
	}
#ifdef DEBUG
	if (debug_level >= 2) { 
		if (dev_mode & (MODE_PLAY | MODE_RECMASK)) {
			dbg_puts("device block size is ");
			dbg_putu(dev_round);
			dbg_puts(" frames, using ");
			dbg_putu(dev_bufsz / dev_round);
			dbg_puts(" blocks\n");
		}
	}
#endif
	dev_pstate = DEV_INIT;
	return 1;
}

/*
 * Cleanly stop and drain everything and close the device
 * once both play chain and record chain are gone.
 */
void
dev_close(void)
{
	struct file *f;

	/*
	 * if the device is starting, ensure it actually starts
	 * so buffers are drained, else clear any buffers
	 */
	switch (dev_pstate) {
	case DEV_START:
#ifdef DEBUG
		if (debug_level >= 3) 
			dbg_puts("draining device\n");
#endif
		dev_start();
		break;
	case DEV_INIT:
#ifdef DEBUG
		if (debug_level >= 3) 
			dbg_puts("flushing device\n");
#endif
		dev_clear();
		break;
	}
#ifdef DEBUG
	if (debug_level >= 2) 
		dbg_puts("closing device\n");
#endif

	if (dev_mix) {
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
		if (APROC_OK(dev_mix))
			mix_quit(dev_mix);

		/*
		 * XXX: handle this in mix_done()
		 */
		if (APROC_OK(dev_mix->u.mix.mon)) {
			dev_mix->u.mix.mon->refs--;
			aproc_del(dev_mix->u.mix.mon);
			dev_mix->u.mix.mon = NULL;
		}
	restart_mix:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL &&
			    aproc_depend(dev_mix, f->rproc)) {
				file_eof(f);
				goto restart_mix;
			}
		}
	} else if (dev_sub || dev_submon) {
		/*
		 * Same as above, but since there's no mixer, 
		 * we generate EOF on the record-end of the
		 * device.
		 */	
	restart_sub:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL &&
			    (aproc_depend(dev_sub, f->rproc) ||
			     aproc_depend(dev_submon, f->rproc))) {
				file_eof(f);
				goto restart_sub;
			}
		}
	}
	if (dev_midi) {
		dev_midi->flags |= APROC_QUIT;
		if (LIST_EMPTY(&dev_midi->ins))
			aproc_del(dev_midi);
 	restart_midi:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc &&
			    aproc_depend(dev_midi, f->rproc)) {
				file_eof(f);
				goto restart_midi;
			}
		}
	}
	if (dev_mix) {
		if (--dev_mix->refs == 0 && (dev_mix->flags & APROC_ZOMB))
			aproc_del(dev_mix);
		dev_mix = NULL;
	}
	if (dev_play) {
		if (--dev_play->refs == 0 && (dev_play->flags & APROC_ZOMB))
			aproc_del(dev_play);
		dev_play = NULL;
	}
	if (dev_sub) {
		if (--dev_sub->refs == 0 && (dev_sub->flags & APROC_ZOMB))
			aproc_del(dev_sub);
		dev_sub = NULL;
	}
	if (dev_rec) {
		if (--dev_rec->refs == 0 && (dev_rec->flags & APROC_ZOMB))
			aproc_del(dev_rec);
		dev_rec = NULL;
	}
	if (dev_submon) {
		if (--dev_submon->refs == 0 && (dev_submon->flags & APROC_ZOMB))
			aproc_del(dev_submon);
		dev_submon = NULL;
	}
	if (dev_mon) {
		if (--dev_mon->refs == 0 && (dev_mon->flags & APROC_ZOMB))
			aproc_del(dev_mon);
		dev_mon = NULL;
	}
	if (dev_midi) {
		if (--dev_midi->refs == 0 && (dev_midi->flags & APROC_ZOMB))
			aproc_del(dev_midi);
		dev_midi = NULL;
	}
	dev_pstate = DEV_CLOSED;
}

/*
 * Free the device
 */
void
dev_done(void)
{
	if (dev_pstate != DEV_CLOSED)
		dev_close();
}

/*
 * Open a MIDI device and connect it to the thru box
 */
int
dev_thruadd(char *name, int in, int out)
{
	struct file *f;
	struct abuf *rbuf = NULL, *wbuf = NULL;
	struct aproc *rproc, *wproc;

	if (!dev_ref())
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
	dev_midiattach(rbuf, wbuf);
	return 1;
}

/*
 * Attach a bi-directional MIDI stream to the MIDI device
 */
void
dev_midiattach(struct abuf *ibuf, struct abuf *obuf)
{
	if (ibuf)
		aproc_setin(dev_midi, ibuf);
	if (obuf) {
		aproc_setout(dev_midi, obuf);
		if (ibuf) {
			ibuf->duplex = obuf;
			obuf->duplex = ibuf;
		}
	}
}

unsigned
dev_roundof(unsigned newrate)
{
	return (dev_round * newrate + dev_rate / 2) / dev_rate;
}

/*
 * Start the (paused) device. By default it's paused.
 */
void
dev_start(void)
{
	struct file *f;

#ifdef DEBUG
	if (debug_level >= 2)
		dbg_puts("starting device\n");
#endif
	dev_pstate = DEV_RUN;
	if (dev_mode & MODE_LOOP)
		return;
	if (APROC_OK(dev_mix))
		dev_mix->flags |= APROC_DROP;
	if (APROC_OK(dev_sub))
		dev_sub->flags |= APROC_DROP;
	if (APROC_OK(dev_submon))
		dev_submon->flags |= APROC_DROP;
	if (APROC_OK(dev_play) && dev_play->u.io.file) {
		f = dev_play->u.io.file;
		f->ops->start(f);
	} else if (APROC_OK(dev_rec) && dev_rec->u.io.file) {
		f = dev_rec->u.io.file;
		f->ops->start(f);
	}
}

/*
 * Pause the device. This may trigger context switches,
 * so it shouldn't be called from aproc methods
 */
void
dev_stop(void)
{
	struct file *f;

#ifdef DEBUG
	if (debug_level >= 2)
		dbg_puts("stopping stopped\n");
#endif
	dev_pstate = DEV_INIT;
	if (dev_mode & MODE_LOOP)
		return;
	if (APROC_OK(dev_play) && dev_play->u.io.file) {
		f = dev_play->u.io.file;
		f->ops->stop(f);
	} else if (APROC_OK(dev_rec) && dev_rec->u.io.file) {
		f = dev_rec->u.io.file;
		f->ops->stop(f);
	}
	if (APROC_OK(dev_mix))
		dev_mix->flags &= ~APROC_DROP;
	if (APROC_OK(dev_sub))
		dev_sub->flags &= ~APROC_DROP;
	if (APROC_OK(dev_submon))
		dev_submon->flags &= ~APROC_DROP;
}

int
dev_ref(void)
{
#ifdef DEBUG
	if (debug_level >= 3)
		dbg_puts("device requested\n");
#endif
	if (dev_pstate == DEV_CLOSED && !dev_open())
		return 0;
	dev_refcnt++;
	return 1;
}

void
dev_unref(void)
{
#ifdef DEBUG
	if (debug_level >= 3)
		dbg_puts("device released\n");
#endif
	dev_refcnt--;
	if (dev_refcnt == 0 && dev_pstate == DEV_INIT)
		dev_close();
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
dev_run(void)
{
	if (dev_pstate == DEV_CLOSED)
		return 1;
	/*
	 * check if device isn't gone
	 */
	if (((dev_mode & MODE_PLAY) && !APROC_OK(dev_mix)) ||
	    ((dev_mode & MODE_REC)  && !APROC_OK(dev_sub)) ||
	    ((dev_mode & MODE_MON)  && !APROC_OK(dev_submon))) {
#ifdef DEBUG
		if (debug_level >= 1)
			dbg_puts("device disappeared\n");
#endif
		dev_close();
		return 0;
	}
	switch (dev_pstate) {
	case DEV_INIT:
		/* nothing */
		break;
	case DEV_START:
		dev_start();
		/* PASSTHROUGH */
	case DEV_RUN:
		/*
		 * if the device is not used, then stop it
		 */
		if ((!APROC_OK(dev_mix) ||
			dev_mix->u.mix.idle > 2 * dev_bufsz) &&
		    (!APROC_OK(dev_sub) ||
			dev_sub->u.sub.idle > 2 * dev_bufsz) &&
		    (!APROC_OK(dev_submon) ||
			dev_submon->u.sub.idle > 2 * dev_bufsz) &&
		    (!APROC_OK(dev_midi) ||
			dev_midi->u.ctl.tstate != CTL_RUN)) {
#ifdef DEBUG
			if (debug_level >= 3)
				dbg_puts("device idle, suspending\n");
#endif
			dev_stop();
			if (dev_refcnt == 0)
				dev_close();
			else
				dev_clear();
		}
		break;
	}
	return 1;
}

/*
 * If the device is paused, then resume it. If the caller is using
 * full-duplex and its buffers are small, the ``prime'' flag
 * could be set to initialize device buffers with silence
 *
 * This routine can be called from aproc context.
 */
void
dev_wakeup(int prime)
{
	if (dev_pstate == DEV_INIT) {
		if (prime)
			dev_prime();
		dev_pstate = DEV_START;
	 }
}

/*
 * Find the end points connected to the mix/sub.
 */
int
dev_getep(unsigned mode, struct abuf **sibuf, struct abuf **sobuf)
{
	struct abuf *ibuf, *obuf;

	if (mode & MODE_PLAY) {
		if (!APROC_OK(dev_mix))
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
			if (ibuf->rproc == dev_mix)
				break;
			ibuf = LIST_FIRST(&ibuf->rproc->outs);
		}
		*sibuf = ibuf;
	}
	if (mode & MODE_REC) {
		if (!APROC_OK(dev_sub))
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
			if (obuf->wproc == dev_sub)
				break;
			obuf = LIST_FIRST(&obuf->wproc->ins);
		}
		*sobuf = obuf;
	}
	if (mode & MODE_MON) {
		if (!APROC_OK(dev_submon))
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
			if (obuf->wproc == dev_submon)
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
dev_sync(unsigned mode, struct abuf *ibuf, struct abuf *obuf)
{
	int delta, offs;
	struct abuf *mbuf;

	if (!dev_getep(mode, &ibuf, &obuf))
		return;
	/*
	 * Calculate delta, the number of frames the play chain is ahead
	 * of the record chain. It's necessary to schedule silences (or
	 * drops) in order to start playback and record in sync.
	 */
	offs = 0;
	delta = 0;
	if (APROC_OK(dev_mix)) {
		mbuf = LIST_FIRST(&dev_mix->outs);
		offs += mbuf->w.mix.todo;
		delta += dev_mix->u.mix.lat;
	}
	if (APROC_OK(dev_sub))
		delta += dev_sub->u.sub.lat;
#ifdef DEBUG
	if (debug_level >= 3) {
		dbg_puts("syncing device");
		if (APROC_OK(dev_mix)) {
			dbg_puts(", ");
			aproc_dbg(dev_mix);
			dbg_puts(": todo = ");
			dbg_putu(mbuf->w.mix.todo);
			dbg_puts(": lat = ");
			dbg_putu(dev_mix->u.mix.lat);
		}
		if (APROC_OK(dev_sub)) {
			dbg_puts(", ");
			aproc_dbg(dev_sub);
			dbg_puts(": lat = ");
			dbg_putu(dev_sub->u.sub.lat);
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
dev_getpos(void)
{
	struct abuf *mbuf = NULL;

	if (APROC_OK(dev_mix)) {
		mbuf = LIST_FIRST(&dev_mix->outs);
		return mbuf->w.mix.todo + dev_mix->u.mix.lat;
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
dev_attach(char *name, unsigned mode,
    struct abuf *ibuf, struct aparams *sipar, unsigned inch,
    struct abuf *obuf, struct aparams *sopar, unsigned onch,
    unsigned xrun, int vol)
{
	struct abuf *pbuf = NULL, *rbuf = NULL;
	struct aparams ipar, opar;
	struct aproc *conv;
	unsigned round, nblk, nch;

#ifdef DEBUG
	if ((!APROC_OK(dev_mix)    && (mode & MODE_PLAY)) ||
	    (!APROC_OK(dev_sub)    && (mode & MODE_REC)) ||
	    (!APROC_OK(dev_submon) && (mode & MODE_MON))) {
	    	dbg_puts("mode beyond device mode, not attaching\n");
		return;
	}
#endif
	if (mode & MODE_PLAY) {
		ipar = *sipar;
		pbuf = LIST_FIRST(&dev_mix->outs);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		round = dev_roundof(ipar.rate);
		nch = ipar.cmax - ipar.cmin + 1;
		if (!aparams_eqenc(&ipar, &dev_opar)) {
			conv = dec_new(name, &ipar);
			ipar.bps = dev_opar.bps;
			ipar.bits = dev_opar.bits;
			ipar.sig = dev_opar.sig;
			ipar.le = dev_opar.le;
			ipar.msb = dev_opar.msb;
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
		if (!aparams_eqrate(&ipar, &dev_opar)) {
			conv = resamp_new(name, round, dev_round);
			ipar.rate = dev_opar.rate;
			round = dev_round;
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
		aproc_setin(dev_mix, ibuf);
		ibuf->r.mix.xrun = xrun;
		ibuf->r.mix.maxweight = vol;
		mix_setmaster(dev_mix);
	}
	if (mode & MODE_REC) {
		opar = *sopar;
		rbuf = LIST_FIRST(&dev_sub->ins);
		round = dev_roundof(opar.rate);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		nch = opar.cmax - opar.cmin + 1;
		if (!aparams_eqenc(&opar, &dev_ipar)) {
			conv = enc_new(name, &opar);
			opar.bps = dev_ipar.bps;
			opar.bits = dev_ipar.bits;
			opar.sig = dev_ipar.sig;
			opar.le = dev_ipar.le;
			opar.msb = dev_ipar.msb;
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
		if (!aparams_eqrate(&opar, &dev_ipar)) {
			conv = resamp_new(name, dev_round, round);
			opar.rate = dev_ipar.rate;
			round = dev_round;
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
		aproc_setout(dev_sub, obuf);
		obuf->w.sub.xrun = xrun;
	}
	if (mode & MODE_MON) {
		opar = *sopar;
		rbuf = LIST_FIRST(&dev_submon->ins);
		round = dev_roundof(opar.rate);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		nch = opar.cmax - opar.cmin + 1;
		if (!aparams_eqenc(&opar, &dev_opar)) {
			conv = enc_new(name, &opar);
			opar.bps = dev_opar.bps;
			opar.bits = dev_opar.bits;
			opar.sig = dev_opar.sig;
			opar.le = dev_opar.le;
			opar.msb = dev_opar.msb;
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
		if (!aparams_eqrate(&opar, &dev_opar)) {
			conv = resamp_new(name, dev_round, round);
			opar.rate = dev_opar.rate;
			round = dev_round;
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
		aproc_setout(dev_submon, obuf);
		obuf->w.sub.xrun = xrun;
	}

	/*
	 * Sync play to record.
	 */
	if ((mode & MODE_PLAY) && (mode & MODE_RECMASK)) {
		ibuf->duplex = obuf;
		obuf->duplex = ibuf;
	}
	dev_sync(mode, ibuf, obuf);
}

/*
 * Change the playback volume of the given stream.
 */
void
dev_setvol(struct abuf *ibuf, int vol)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(ibuf);
		dbg_puts(": setting volume to ");
		dbg_putu(vol);
		dbg_puts("\n");
	}
#endif
	if (!dev_getep(MODE_PLAY, &ibuf, NULL)) {
		return;
	}
	ibuf->r.mix.vol = vol;
}

/*
 * Clear buffers of the play and record chains so that when the device
 * is started, playback and record start in sync.
 */
void
dev_clear(void)
{
	struct abuf *buf;

	if (APROC_OK(dev_mix)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&dev_mix->ins)) {
			dbg_puts("play end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&dev_mix->outs);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->rproc->outs);
		}
		mix_clear(dev_mix);
	}
	if (APROC_OK(dev_sub)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&dev_sub->outs)) {
			dbg_puts("record end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&dev_sub->ins);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ins);
		}
		sub_clear(dev_sub);
	}
	if (APROC_OK(dev_submon)) {
#ifdef DEBUG
		dbg_puts("clearing monitor\n");
		if (!LIST_EMPTY(&dev_submon->outs)) {
			dbg_puts("monitoring end not idle, can't clear device\n");
			dbg_panic();
		}
#endif
		buf = LIST_FIRST(&dev_submon->ins);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ins);
		}
		sub_clear(dev_submon);
		mon_clear(dev_mon);
	}
}

/*
 * Fill with silence play buffers and schedule the same amount of recorded
 * samples to drop
 */
void
dev_prime(void)
{

#ifdef DEBUG
	if (debug_level >= 3)
		dbg_puts("priming device\n");
#endif
	if (APROC_OK(dev_mix)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&dev_mix->ins)) {
			dbg_puts("play end not idle, can't prime device\n");
			dbg_panic();	
		}
#endif
		mix_prime(dev_mix);
	}
}
