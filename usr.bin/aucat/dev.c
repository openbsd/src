/*	$OpenBSD: dev.c,v 1.46 2010/04/06 20:07:01 ratchov Exp $	*/
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

unsigned dev_pstate;
unsigned dev_bufsz, dev_round, dev_rate;
struct aparams dev_ipar, dev_opar;
struct aproc *dev_mix, *dev_sub, *dev_rec, *dev_play, *dev_submon, *dev_mon;
struct aproc *dev_midi;

/*
 * Create a MIDI thru box as the MIDI end of the device
 */
void
dev_thruinit(void)
{
	dev_midi = thru_new("thru");
	dev_midi->refs++;
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

/*
 * Same as dev_init(), but create a fake device that records what is
 * played.
 */
void
dev_loopinit(struct aparams *dipar, struct aparams *dopar, unsigned bufsz)
{
	struct abuf *buf;
	struct aparams par;
	unsigned cmin, cmax, rate;

	/*
	 * in principle we don't need control, but the start-stop mechanism
	 * depend on it and it's simpler to reuse this mechanism rather than
	 * dealing with lots of special cases
	 */
	dev_midi = ctl_new("ctl");
	dev_midi->refs++;

	cmin = (dipar->cmin < dopar->cmin) ? dipar->cmin : dopar->cmin;
	cmax = (dipar->cmax > dopar->cmax) ? dipar->cmax : dopar->cmax;
	rate = (dipar->rate > dopar->rate) ? dipar->rate : dopar->rate;
	aparams_init(&par, cmin, cmax, rate);
	dev_ipar = par;
	dev_opar = par;
	dev_round = (bufsz + 1) / 2;
	dev_bufsz = dev_round * 2;
	dev_rate  = rate;
	dev_rec = NULL;
	dev_play = NULL;
	dev_mon = NULL;
	dev_submon = NULL;
	dev_pstate = DEV_INIT;

	buf = abuf_new(dev_bufsz, &par);
	dev_mix = mix_new("mix", dev_bufsz, 1, NULL);
	dev_mix->refs++;
	dev_sub = sub_new("sub", dev_bufsz, 1, NULL);
	dev_sub->refs++;
	aproc_setout(dev_mix, buf);
	aproc_setin(dev_sub, buf);

	dev_mix->flags |= APROC_QUIT;
	dev_sub->flags |= APROC_QUIT;

	*dipar = dev_ipar;
	*dopar = dev_opar;
}

unsigned
dev_roundof(unsigned newrate)
{
	return (dev_round * newrate + dev_rate / 2) / dev_rate;
}

/*
 * Open the device with the given hardware parameters and create a mixer
 * and a multiplexer connected to it with all necessary conversions
 * setup.
 */
int
dev_init(char *devpath, unsigned mode,
    struct aparams *dipar, struct aparams *dopar, unsigned bufsz, unsigned round)
{
	struct file *f;
	struct aparams ipar, opar;
	struct aproc *conv;
	struct abuf *buf;

	dev_midi = ctl_new("ctl");
	dev_midi->refs++;

	/*
	 * Ask for 1/4 of the buffer for the kernel ring and
	 * limit the block size to 1/4 of the requested buffer.
	 */
	dev_round = round;
	dev_bufsz = bufsz;
	f = (struct file *)siofile_new(&siofile_ops, devpath,
	    mode & (MODE_PLAY | MODE_REC), dipar, dopar,
	    &dev_bufsz, &dev_round);
	if (f == NULL)
		return 0;
	if (mode & MODE_REC) {
#ifdef DEBUG
		if (debug_level >= 2) {
			dbg_puts("hw recording ");
			aparams_dbg(dipar);
			dbg_puts("\n");
		}
#endif
		dev_rate = dipar->rate;
	}
	if (mode & MODE_PLAY) {
#ifdef DEBUG
		if (debug_level >= 2) {
			dbg_puts("hw playing ");
			aparams_dbg(dopar);
			dbg_puts("\n");
		}
#endif
		dev_rate = dopar->rate;
	}

	/*
	 * Create record chain.
	 */
	if (mode & MODE_REC) {
		aparams_init(&ipar, dipar->cmin, dipar->cmax, dipar->rate);
		/*
		 * Create the read end.
		 */
		dev_rec = rsio_new(f);
		dev_rec->refs++;
		buf = abuf_new(dev_bufsz, dipar);
		aproc_setout(dev_rec, buf);

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(dipar, &ipar)) {
			conv = dec_new("rec", dipar);
			aproc_setin(conv, buf);
			buf = abuf_new(dev_round, &ipar);
			aproc_setout(conv, buf);
		}
		dev_ipar = ipar;

		/*
		 * Append a "sub" to which clients will connect.
		 * Link it to the controller only in record-only mode
		 */
		dev_sub = sub_new("rec", dev_bufsz, dev_round,
		    dopar ? NULL : dev_midi);
		dev_sub->refs++;
		aproc_setin(dev_sub, buf);
	} else {
		dev_rec = NULL;
		dev_sub = NULL;
	}

	/*
	 * Create play chain.
	 */
	if (mode & MODE_PLAY) {
		aparams_init(&opar, dopar->cmin, dopar->cmax, dopar->rate);
		/*
		 * Create the write end.
		 */
		dev_play = wsio_new(f);
		dev_play->refs++;
		buf = abuf_new(dev_bufsz, dopar);
		aproc_setin(dev_play, buf);

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(&opar, dopar)) {
			conv = enc_new("play", dopar);
			aproc_setout(conv, buf);
			buf = abuf_new(dev_round, &opar);
			aproc_setin(conv, buf);
		}
		dev_opar = opar;

		/*
		 * Append a "mix" to which clients will connect.
		 */
		dev_mix = mix_new("play", dev_bufsz, dev_round, dev_midi);
		dev_mix->refs++;
		aproc_setout(dev_mix, buf);
	} else {
		dev_play = NULL;
		dev_mix = NULL;
	}

	/*
	 * Create monitoring chain
	 */
	if (mode & MODE_MON) {
		dev_mon = mon_new("mon", dev_bufsz);
		dev_mon->refs++;
		buf = abuf_new(dev_bufsz, &dev_opar);
		aproc_setout(dev_mon, buf);

		/*
		 * Append a "sub" to which clients will connect.
		 * Link it to the controller only in record-only mode
		 */
		dev_submon = sub_new("mon", dev_bufsz, dev_round, NULL);
		dev_submon->refs++;
		aproc_setin(dev_submon, buf);

		/*
		 * Attack to the mixer
		 */
		dev_mix->u.mix.mon = dev_mon;
		dev_mon->refs++;
	} else {
		dev_submon = NULL;
		if (APROC_OK(dev_mix))
			dev_mix->u.mix.mon = NULL;
	}

#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts("device block size is ");
		dbg_putu(dev_round);
		dbg_puts(" frames, using ");
		dbg_putu(dev_bufsz / dev_round);
		dbg_puts(" blocks\n");
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
dev_done(void)
{
	struct file *f;

#ifdef DEBUG
	if (debug_level >= 2) 
		dbg_puts("closing audio device\n");
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
		dev_mix->flags |= APROC_QUIT;
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
		if (LIST_EMPTY(&dev_midi->ibuflist))
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
	for (;;) {
		if (!file_poll())
			break;
	}
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
		dbg_puts("starting audio device\n");
#endif
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
#ifdef DEBUG
	if (debug_level >= 2)
		dbg_puts("audio device stopped\n");
#endif
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
			ibuf = LIST_FIRST(&ibuf->rproc->obuflist);
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
			obuf = LIST_FIRST(&obuf->wproc->ibuflist);
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
			obuf = LIST_FIRST(&obuf->wproc->ibuflist);
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
	int delta;

	if (!dev_getep(mode, &ibuf, &obuf))
		return;
	/*
	 * Calculate delta, the number of frames the play chain is ahead
	 * of the record chain. It's necessary to schedule silences (or
	 * drops) in order to start playback and record in sync.
	 */
	if (APROC_OK(dev_mix) && APROC_OK(dev_sub)) {
		delta = dev_mix->u.mix.abspos - dev_sub->u.sub.abspos;
	} else if (APROC_OK(dev_mix)) {
		delta = dev_mix->u.mix.lat;
	} else
		delta = 0;
#ifdef DEBUG
	if (debug_level >= 3) {
		dbg_puts("syncing device, delta = ");
		dbg_putu(delta);
		dbg_puts(" ");
		if (APROC_OK(dev_mix)) {
			aproc_dbg(dev_mix);
			dbg_puts(": abspos = ");
			dbg_putu(dev_mix->u.mix.abspos);
		}
		if (APROC_OK(dev_sub)) {
			aproc_dbg(dev_sub);
			dbg_puts(": abspos = ");
			dbg_putu(dev_sub->u.sub.abspos);
		}
		dbg_puts("\n");
	}
#endif
	if (delta > 0) {
		/*
		 * The play chain is ahead (most cases) drop some of
		 * the recorded input, to get both in sync.
		 */
		if (mode & MODE_RECMASK)
			sub_silence(obuf, -delta);
	} else if (delta < 0) {
		/*
		 * The record chain is ahead (should never happen,
		 * right?) then insert silence to play.
		 */
		 if (mode & MODE_PLAY)
		 	mix_drop(ibuf, delta);
	}
}

/*
 * return the current latency (in frames), ie the latency that
 * a stream would have if dev_attach() is called on it.
 */
int
dev_getpos(void)
{
	struct abuf *pbuf = NULL, *rbuf = NULL;
	int plat = 0, rlat = 0;
	int delta;

	if (APROC_OK(dev_mix)) {
		pbuf = LIST_FIRST(&dev_mix->obuflist);
		if (!pbuf)
			return 0;
		plat = -dev_mix->u.mix.lat;
	}
	if (APROC_OK(dev_sub)) {
		rbuf = LIST_FIRST(&dev_sub->ibuflist);
		if (!rbuf)
			return 0;
		rlat = -dev_sub->u.sub.lat;
	}
	if (APROC_OK(dev_mix) && APROC_OK(dev_sub)) {
		delta = dev_mix->u.mix.abspos - dev_sub->u.sub.abspos;
		if (delta > 0)
			rlat -= delta;
		else if (delta < 0)
			plat += delta;
#ifdef DEBUG
		if (rlat != plat) {
			dbg_puts("dev_getpos: play/rec out of sync: plat = ");
			dbg_puti(plat);
			dbg_puts(", rlat = ");
			dbg_puti(rlat);
			dbg_puts("\n");
		}
#endif
	}
	return APROC_OK(dev_mix) ? plat : rlat;
}

/*
 * Attach the given input and output buffers to the mixer and the
 * multiplexer respectively. The operation is done synchronously, so
 * both buffers enter in sync. If buffers do not match play
 * and rec.
 */
void
dev_attach(char *name, unsigned mode,
    struct abuf *ibuf, struct aparams *sipar,
    struct abuf *obuf, struct aparams *sopar,
    unsigned xrun, int vol)
{
	struct abuf *pbuf = NULL, *rbuf = NULL;
	struct aparams ipar, opar;
	struct aproc *conv;
	unsigned round, nblk;

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
		pbuf = LIST_FIRST(&dev_mix->obuflist);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		round = dev_roundof(ipar.rate);
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
		if (!aparams_subset(&ipar, &dev_opar)) {
			conv = cmap_new(name, &ipar, &dev_opar);
			ipar.cmin = dev_opar.cmin;
			ipar.cmax = dev_opar.cmax;
			aproc_setin(conv, ibuf);
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
		aproc_setin(dev_mix, ibuf);
		ibuf->r.mix.xrun = xrun;
		ibuf->r.mix.maxweight = vol;
		mix_setmaster(dev_mix);
	}
	if (mode & MODE_REC) {
		opar = *sopar;
		rbuf = LIST_FIRST(&dev_sub->ibuflist);
		round = dev_roundof(opar.rate);
		nblk = (dev_bufsz / dev_round + 3) / 4;
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
		if (!aparams_subset(&opar, &dev_ipar)) {
			conv = cmap_new(name, &dev_ipar, &opar);
			opar.cmin = dev_ipar.cmin;
			opar.cmax = dev_ipar.cmax;
			aproc_setout(conv, obuf);
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
		aproc_setout(dev_sub, obuf);
		obuf->w.sub.xrun = xrun;
	}
	if (mode & MODE_MON) {
		opar = *sopar;
		rbuf = LIST_FIRST(&dev_submon->ibuflist);
		round = dev_roundof(opar.rate);
		nblk = (dev_bufsz / dev_round + 3) / 4;
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
		if (!aparams_subset(&opar, &dev_opar)) {
			conv = cmap_new(name, &dev_opar, &opar);
			opar.cmin = dev_opar.cmin;
			opar.cmax = dev_opar.cmax;
			aproc_setout(conv, obuf);
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

	/*
	 * Start device if not already started
	 */
	if (dev_pstate == DEV_INIT)
		dev_pstate = DEV_START;
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
		if (!LIST_EMPTY(&dev_mix->ibuflist)) {
			dbg_puts("play end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&dev_mix->obuflist);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->rproc->obuflist);
		}
		mix_clear(dev_mix);
	}
	if (APROC_OK(dev_sub)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&dev_sub->obuflist)) {
			dbg_puts("record end not idle, can't clear device\n");
			dbg_panic();	
		}
#endif
		buf = LIST_FIRST(&dev_sub->ibuflist);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ibuflist);
		}
		sub_clear(dev_sub);
	}
	if (APROC_OK(dev_submon)) {
#ifdef DEBUG
		dbg_puts("clearing monitor\n");
		if (!LIST_EMPTY(&dev_submon->obuflist)) {
			dbg_puts("monitoring end not idle, can't clear device\n");
			dbg_panic();
		}
#endif
		buf = LIST_FIRST(&dev_submon->ibuflist);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ibuflist);
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
	if (APROC_OK(dev_mix)) {
#ifdef DEBUG
		if (!LIST_EMPTY(&dev_mix->ibuflist)) {
			dbg_puts("play end not idle, can't prime device\n");
			dbg_panic();	
		}
#endif
		mix_prime(dev_mix);
	}
}
