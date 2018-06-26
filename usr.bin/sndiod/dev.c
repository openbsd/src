/*	$OpenBSD: dev.c,v 1.47 2018/06/26 07:49:44 ratchov Exp $	*/
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
#include <stdio.h>
#include <string.h>

#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "dsp.h"
#include "siofile.h"
#include "midi.h"
#include "opt.h"
#include "sysex.h"
#include "utils.h"

void zomb_onmove(void *);
void zomb_onvol(void *);
void zomb_fill(void *);
void zomb_flush(void *);
void zomb_eof(void *);
void zomb_exit(void *);

void dev_log(struct dev *);
void dev_midi_qfr(struct dev *, int);
void dev_midi_full(struct dev *);
void dev_midi_vol(struct dev *, struct slot *);
void dev_midi_master(struct dev *);
void dev_midi_slotdesc(struct dev *, struct slot *);
void dev_midi_dump(struct dev *);
void dev_midi_imsg(void *, unsigned char *, int);
void dev_midi_omsg(void *, unsigned char *, int);
void dev_midi_fill(void *, int);
void dev_midi_exit(void *);

void dev_mix_badd(struct dev *, struct slot *);
void dev_mix_adjvol(struct dev *);
void dev_sub_bcopy(struct dev *, struct slot *);

void dev_onmove(struct dev *, int);
void dev_master(struct dev *, unsigned int);
void dev_cycle(struct dev *);
int dev_getpos(struct dev *);
struct dev *dev_new(char *, struct aparams *, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, unsigned int);
void dev_adjpar(struct dev *, int, int, int);
int dev_open(struct dev *);
void dev_close(struct dev *);
int dev_ref(struct dev *);
void dev_unref(struct dev *);
int dev_init(struct dev *);
void dev_done(struct dev *);
struct dev *dev_bynum(int);
void dev_del(struct dev *);
unsigned int dev_roundof(struct dev *, unsigned int);
void dev_wakeup(struct dev *);
void dev_sync_attach(struct dev *);
void dev_mmcstart(struct dev *);
void dev_mmcstop(struct dev *);
void dev_mmcloc(struct dev *, unsigned int);

void slot_log(struct slot *);
void slot_del(struct slot *);
void slot_setvol(struct slot *, unsigned int);
void slot_attach(struct slot *);
void slot_ready(struct slot *);
void slot_allocbufs(struct slot *);
void slot_freebufs(struct slot *);
void slot_start(struct slot *);
void slot_detach(struct slot *);
void slot_stop(struct slot *);
void slot_skip_update(struct slot *);
void slot_write(struct slot *);
void slot_read(struct slot *);
int slot_skip(struct slot *);

struct midiops dev_midiops = {
	dev_midi_imsg,
	dev_midi_omsg,
	dev_midi_fill,
	dev_midi_exit
};

struct slotops zomb_slotops = {
	zomb_onmove,
	zomb_onvol,
	zomb_fill,
	zomb_flush,
	zomb_eof,
	zomb_exit
};

struct dev *dev_list = NULL;
unsigned int dev_sndnum = 0;

void
dev_log(struct dev *d)
{
#ifdef DEBUG
	static char *pstates[] = {
		"cfg", "ini", "run"
	};
#endif
	log_puts("snd");
	log_putu(d->num);
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts(" pst=");
		log_puts(pstates[d->pstate]);
	}
#endif
}

void
slot_log(struct slot *s)
{
#ifdef DEBUG
	static char *pstates[] = {
		"ini", "sta", "rdy", "run", "stp", "mid"
	};
#endif
	log_puts(s->name);
	log_putu(s->unit);
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts(" vol=");
		log_putu(s->vol);
		if (s->ops) {
			log_puts(",pst=");
			log_puts(pstates[s->pstate]);
		}
	}
#endif
}

void
zomb_onmove(void *arg)
{
}

void
zomb_onvol(void *arg)
{
}

void
zomb_fill(void *arg)
{
}

void
zomb_flush(void *arg)
{
}

void
zomb_eof(void *arg)
{
	struct slot *s = arg;

#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": zomb_eof\n");
	}
#endif
	s->ops = NULL;
}

void
zomb_exit(void *arg)
{
#ifdef DEBUG
	struct slot *s = arg;

	if (log_level >= 3) {
		slot_log(s);
		log_puts(": zomb_exit\n");
	}
#endif
}

/*
 * send a quarter frame MTC message
 */
void
dev_midi_qfr(struct dev *d, int delta)
{
	unsigned char buf[2];
	unsigned int data;
	int qfrlen;

	d->mtc.delta += delta * MTC_SEC;
	qfrlen = d->rate * (MTC_SEC / (4 * d->mtc.fps));
	while (d->mtc.delta >= qfrlen) {
		switch (d->mtc.qfr) {
		case 0:
			data = d->mtc.fr & 0xf;
			break;
		case 1:
			data = d->mtc.fr >> 4;
			break;
		case 2:
			data = d->mtc.sec & 0xf;
			break;
		case 3:
			data = d->mtc.sec >> 4;
			break;
		case 4:
			data = d->mtc.min & 0xf;
			break;
		case 5:
			data = d->mtc.min >> 4;
			break;
		case 6:
			data = d->mtc.hr & 0xf;
			break;
		case 7:
			data = (d->mtc.hr >> 4) | (d->mtc.fps_id << 1);
			/*
			 * tick messages are sent 2 frames ahead
			 */
			d->mtc.fr += 2;
			if (d->mtc.fr < d->mtc.fps)
				break;
			d->mtc.fr -= d->mtc.fps;
			d->mtc.sec++;
			if (d->mtc.sec < 60)
				break;
			d->mtc.sec = 0;
			d->mtc.min++;
			if (d->mtc.min < 60)
				break;
			d->mtc.min = 0;
			d->mtc.hr++;
			if (d->mtc.hr < 24)
				break;
			d->mtc.hr = 0;
			break;
		default:
			/* NOTREACHED */
			data = 0;
		}
		buf[0] = 0xf1;
		buf[1] = (d->mtc.qfr << 4) | data;
		d->mtc.qfr++;
		d->mtc.qfr &= 7;
		midi_send(d->midi, buf, 2);
		d->mtc.delta -= qfrlen;
	}
}

/*
 * send a full frame MTC message
 */
void
dev_midi_full(struct dev *d)
{
	struct sysex x;
	unsigned int fps;

	d->mtc.delta = MTC_SEC * dev_getpos(d);
	if (d->rate % (30 * 4 * d->round) == 0) {
		d->mtc.fps_id = MTC_FPS_30;
		d->mtc.fps = 30;
	} else if (d->rate % (25 * 4 * d->round) == 0) {
		d->mtc.fps_id = MTC_FPS_25;
		d->mtc.fps = 25;
	} else {
		d->mtc.fps_id = MTC_FPS_24;
		d->mtc.fps = 24;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": mtc full frame at ");
		log_puti(d->mtc.delta);
		log_puts(", ");
		log_puti(d->mtc.fps);
		log_puts(" fps\n");
	}
#endif
	fps = d->mtc.fps;
	d->mtc.hr =  (d->mtc.origin / (MTC_SEC * 3600)) % 24;
	d->mtc.min = (d->mtc.origin / (MTC_SEC * 60))   % 60;
	d->mtc.sec = (d->mtc.origin / (MTC_SEC))        % 60;
	d->mtc.fr =  (d->mtc.origin / (MTC_SEC / fps))  % fps;

	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_RT;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_MTC;
	x.id1 = SYSEX_MTC_FULL;
	x.u.full.hr = d->mtc.hr | (d->mtc.fps_id << 5);
	x.u.full.min = d->mtc.min;
	x.u.full.sec = d->mtc.sec;
	x.u.full.fr = d->mtc.fr;
	x.u.full.end = SYSEX_END;
	d->mtc.qfr = 0;
	midi_send(d->midi, (unsigned char *)&x, SYSEX_SIZE(full));
}

/*
 * send a volume change MIDI message
 */
void
dev_midi_vol(struct dev *d, struct slot *s)
{
	unsigned char msg[3];

	msg[0] = MIDI_CTL | (s - d->slot);
	msg[1] = MIDI_CTL_VOL;
	msg[2] = s->vol;
	midi_send(d->midi, msg, 3);
}

/*
 * send a master volume MIDI message
 */
void
dev_midi_master(struct dev *d)
{
	struct sysex x;

	memset(&x, 0, sizeof(struct sysex));
	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_RT;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_CONTROL;
	x.id1 = SYSEX_MASTER;
	x.u.master.fine = 0;
	x.u.master.coarse = d->master;
	x.u.master.end = SYSEX_END;
	midi_send(d->midi, (unsigned char *)&x, SYSEX_SIZE(master));
}

/*
 * send a sndiod-specific slot description MIDI message
 */
void
dev_midi_slotdesc(struct dev *d, struct slot *s)
{
	struct sysex x;

	memset(&x, 0, sizeof(struct sysex));
	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_EDU;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_AUCAT;
	x.id1 = SYSEX_AUCAT_SLOTDESC;
	if (*s->name != '\0') {
		snprintf((char *)x.u.slotdesc.name, SYSEX_NAMELEN,
		    "%s%u", s->name, s->unit);
	}
	x.u.slotdesc.chan = s - d->slot;
	x.u.slotdesc.end = SYSEX_END;
	midi_send(d->midi, (unsigned char *)&x, SYSEX_SIZE(slotdesc));
}

void
dev_midi_dump(struct dev *d)
{
	struct sysex x;
	struct slot *s;
	int i;

	dev_midi_master(d);
	for (i = 0, s = d->slot; i < DEV_NSLOT; i++, s++) {
		dev_midi_slotdesc(d, s);
		dev_midi_vol(d, s);
	}
	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_EDU;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_AUCAT;
	x.id1 = SYSEX_AUCAT_DUMPEND;
	x.u.dumpend.end = SYSEX_END;
	midi_send(d->midi, (unsigned char *)&x, SYSEX_SIZE(dumpend));
}

void
dev_midi_imsg(void *arg, unsigned char *msg, int len)
{
#ifdef DEBUG
	struct dev *d = arg;

	dev_log(d);
	log_puts(": can't receive midi messages\n");
	panic();
#endif
}

void
dev_midi_omsg(void *arg, unsigned char *msg, int len)
{
	struct dev *d = arg;
	struct sysex *x;
	unsigned int fps, chan;

	if ((msg[0] & MIDI_CMDMASK) == MIDI_CTL && msg[1] == MIDI_CTL_VOL) {
		chan = msg[0] & MIDI_CHANMASK;
		if (chan >= DEV_NSLOT)
			return;
		slot_setvol(d->slot + chan, msg[2]);
		return;
	}
	x = (struct sysex *)msg;
	if (x->start != SYSEX_START)
		return;
	if (len < SYSEX_SIZE(empty))
		return;
	switch (x->type) {
	case SYSEX_TYPE_RT:
		if (x->id0 == SYSEX_CONTROL && x->id1 == SYSEX_MASTER) {
			if (len == SYSEX_SIZE(master))
				dev_master(d, x->u.master.coarse);
			return;
		}
		if (x->id0 != SYSEX_MMC)
			return;
		switch (x->id1) {
		case SYSEX_MMC_STOP:
			if (len != SYSEX_SIZE(stop))
				return;
			if (log_level >= 2) {
				dev_log(d);
				log_puts(": mmc stop\n");
			}
			dev_mmcstop(d);
			break;
		case SYSEX_MMC_START:
			if (len != SYSEX_SIZE(start))
				return;
			if (log_level >= 2) {
				dev_log(d);
				log_puts(": mmc start\n");
			}
			dev_mmcstart(d);
			break;
		case SYSEX_MMC_LOC:
			if (len != SYSEX_SIZE(loc) ||
			    x->u.loc.len != SYSEX_MMC_LOC_LEN ||
			    x->u.loc.cmd != SYSEX_MMC_LOC_CMD)
				return;
			switch (x->u.loc.hr >> 5) {
			case MTC_FPS_24:
				fps = 24;
				break;
			case MTC_FPS_25:
				fps = 25;
				break;
			case MTC_FPS_30:
				fps = 30;
				break;
			default:
				dev_mmcstop(d);
				return;
			}
			dev_mmcloc(d,
			    (x->u.loc.hr & 0x1f) * 3600 * MTC_SEC +
			     x->u.loc.min * 60 * MTC_SEC +
			     x->u.loc.sec * MTC_SEC +
			     x->u.loc.fr * (MTC_SEC / fps) +
			     x->u.loc.cent * (MTC_SEC / 100 / fps));
			break;
		}
		break;
	case SYSEX_TYPE_EDU:
		if (x->id0 != SYSEX_AUCAT || x->id1 != SYSEX_AUCAT_DUMPREQ)
			return;
		if (len != SYSEX_SIZE(dumpreq))
			return;
		dev_midi_dump(d);
		break;
	}
}

void
dev_midi_fill(void *arg, int count)
{
	/* nothing to do */
}

void
dev_midi_exit(void *arg)
{
	struct dev *d = arg;

	if (log_level >= 1) {
		dev_log(d);
		log_puts(": midi end point died\n");
	}
	if (d->pstate != DEV_CFG)
		dev_close(d);
}

int
slot_skip(struct slot *s)
{
	unsigned char *data = (unsigned char *)0xdeadbeef; /* please gcc */
	int max, count;

	max = s->skip;
	while (s->skip > 0) {
		if (s->pstate != SLOT_STOP && (s->mode & MODE_RECMASK)) {
			data = abuf_wgetblk(&s->sub.buf, &count);
			if (count < s->round * s->sub.bpf)
				break;
		}
		if (s->mode & MODE_PLAY) {
			if (s->mix.buf.used < s->round * s->mix.bpf)
				break;
		}
#ifdef DEBUG
		if (log_level >= 4) {
			slot_log(s);
			log_puts(": skipped a cycle\n");
		}
#endif
		if (s->pstate != SLOT_STOP && (s->mode & MODE_RECMASK)) {
			if (s->sub.encbuf)
				enc_sil_do(&s->sub.enc, data, s->round);
			else
				memset(data, 0, s->round * s->sub.bpf);
			abuf_wcommit(&s->sub.buf, s->round * s->sub.bpf);
		}
		if (s->mode & MODE_PLAY) {
			abuf_rdiscard(&s->mix.buf, s->round * s->mix.bpf);
		}
		s->skip--;
	}
	return max - s->skip;
}

/*
 * Mix the slot input block over the output block
 */
void
dev_mix_badd(struct dev *d, struct slot *s)
{
	adata_t *idata, *odata, *in;
	int icount, i, offs, vol, nch;

	odata = DEV_PBUF(d);
	idata = (adata_t *)abuf_rgetblk(&s->mix.buf, &icount);
#ifdef DEBUG
	if (icount < s->round * s->mix.bpf) {
		slot_log(s);
		log_puts(": not enough data to mix (");
		log_putu(icount);
		log_puts("bytes)\n");
		panic();
	}
#endif

	/*
	 * Apply the following processing chain:
	 *
	 *	dec -> resamp-> cmap
	 *
	 * where the first two are optional.
	 */

	in = idata;

	if (s->mix.decbuf) {
		dec_do(&s->mix.dec, (void *)in, s->mix.decbuf, s->round);
		in = s->mix.decbuf;
	}

	if (s->mix.resampbuf) {
		resamp_do(&s->mix.resamp, in, s->mix.resampbuf, s->round);
		in = s->mix.resampbuf;
	}

	nch = s->mix.cmap.nch;
	vol = ADATA_MUL(s->mix.weight, s->mix.vol) / s->mix.join;
	cmap_add(&s->mix.cmap, in, odata, vol, d->round);

	offs = 0;
	for (i = s->mix.join - 1; i > 0; i--) {
		offs += nch;
		cmap_add(&s->mix.cmap, in + offs, odata, vol, d->round);
	}

	offs = 0;
	for (i = s->mix.expand - 1; i > 0; i--) {
		offs += nch;
		cmap_add(&s->mix.cmap, in, odata + offs, vol, d->round);
	}

	abuf_rdiscard(&s->mix.buf, s->round * s->mix.bpf);
}

/*
 * Normalize input levels.
 */
void
dev_mix_adjvol(struct dev *d)
{
	unsigned int n;
	struct slot *i, *j;
	int jcmax, icmax, weight;

	for (i = d->slot_list; i != NULL; i = i->next) {
		if (!(i->mode & MODE_PLAY))
			continue;
		icmax = i->opt->pmin + i->mix.nch - 1;
		weight = ADATA_UNIT;
		if (d->autovol) {
			/*
			 * count the number of inputs that have
			 * overlapping channel sets
			 */
			n = 0;
			for (j = d->slot_list; j != NULL; j = j->next) {
				if (!(j->mode & MODE_PLAY))
					continue;
				jcmax = j->opt->pmin + j->mix.nch - 1;
				if (i->opt->pmin <= jcmax &&
				    icmax >= j->opt->pmin)
					n++;
			}
			weight /= n;
		}
		if (weight > i->opt->maxweight)
			weight = i->opt->maxweight;
		i->mix.weight = ADATA_MUL(weight, MIDI_TO_ADATA(d->master));
#ifdef DEBUG
		if (log_level >= 3) {
			slot_log(i);
			log_puts(": set weight: ");
			log_puti(i->mix.weight);
			log_puts("/");
			log_puti(i->opt->maxweight);
			log_puts("\n");
		}
#endif
	}
}

/*
 * Copy data from slot to device
 */
void
dev_sub_bcopy(struct dev *d, struct slot *s)
{
	adata_t *idata, *enc_out, *resamp_out, *cmap_out;
	void *odata;
	int ocount, moffs;

	int i, vol, offs, nch;


	if (s->mode & MODE_MON) {
		moffs = d->poffs + d->round;
		if (moffs == d->psize)
			moffs = 0;
		idata = d->pbuf + moffs * d->pchan;
	} else
		idata = d->rbuf;
	odata = (adata_t *)abuf_wgetblk(&s->sub.buf, &ocount);
#ifdef DEBUG
	if (ocount < s->round * s->sub.bpf) {
		log_puts("dev_sub_bcopy: not enough space\n");
		panic();
	}
#endif

	/*
	 * Apply the following processing chain:
	 *
	 *	cmap -> resamp -> enc
	 *
	 * where the last two are optional.
	 */

	enc_out = odata;
	resamp_out = s->sub.encbuf ? s->sub.encbuf : enc_out;
	cmap_out = s->sub.resampbuf ? s->sub.resampbuf : resamp_out;

	nch = s->sub.cmap.nch;
	vol = ADATA_UNIT / s->sub.join;
	cmap_copy(&s->sub.cmap, idata, cmap_out, vol, d->round);

	offs = 0;
	for (i = s->sub.join - 1; i > 0; i--) {
		offs += nch;
		cmap_add(&s->sub.cmap, idata + offs, cmap_out, vol, d->round);
	}

	offs = 0;
	for (i = s->sub.expand - 1; i > 0; i--) {
		offs += nch;
		cmap_copy(&s->sub.cmap, idata, cmap_out + offs, vol, d->round);
	}

	if (s->sub.resampbuf) {
		resamp_do(&s->sub.resamp,
		    s->sub.resampbuf, resamp_out, d->round);
	}

	if (s->sub.encbuf)
		enc_do(&s->sub.enc, s->sub.encbuf, (void *)enc_out, s->round);

	abuf_wcommit(&s->sub.buf, s->round * s->sub.bpf);
}

/*
 * run a one block cycle: consume one recorded block from
 * rbuf and produce one play block in pbuf
 */
void
dev_cycle(struct dev *d)
{
	struct slot *s, **ps;
	unsigned char *base;
	int nsamp;

	/*
	 * check if the device is actually used. If it isn't,
	 * then close it
	 */
	if (d->slot_list == NULL && d->tstate != MMC_RUN) {
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": device stopped\n");
		}
		dev_sio_stop(d);
		d->pstate = DEV_INIT;
		if (d->refcnt == 0)
			dev_close(d);
		return;
	}

	if (d->prime > 0) {
#ifdef DEBUG
		if (log_level >= 4) {
			dev_log(d);
			log_puts(": empty cycle, prime = ");
			log_putu(d->prime);
			log_puts("\n");
		}
#endif
		base = (unsigned char *)DEV_PBUF(d);
		nsamp = d->round * d->pchan;
		memset(base, 0, nsamp * sizeof(adata_t));
		if (d->encbuf) {
			enc_do(&d->enc, (unsigned char *)DEV_PBUF(d),
			    d->encbuf, d->round);
		}
		d->prime -= d->round;
		return;
	}

	d->delta -= d->round;
#ifdef DEBUG
	if (log_level >= 4) {
		dev_log(d);
		log_puts(": full cycle: delta = ");
		log_puti(d->delta);
		if (d->mode & MODE_PLAY) {
			log_puts(", poffs = ");
			log_puti(d->poffs);
		}
		log_puts("\n");
	}
#endif
	if (d->mode & MODE_PLAY) {
		base = (unsigned char *)DEV_PBUF(d);
		nsamp = d->round * d->pchan;
		memset(base, 0, nsamp * sizeof(adata_t));
	}
	if ((d->mode & MODE_REC) && d->decbuf)
		dec_do(&d->dec, d->decbuf, (unsigned char *)d->rbuf, d->round);
	ps = &d->slot_list;
	while ((s = *ps) != NULL) {
#ifdef DEBUG
		if (log_level >= 4) {
			slot_log(s);
			log_puts(": running");
			log_puts(", skip = ");
			log_puti(s->skip);
			log_puts("\n");
		}
#endif
		/*
		 * skip cycles for XRUN_SYNC correction
		 */
		slot_skip(s);
		if (s->skip < 0) {
			s->skip++;
			ps = &s->next;
			continue;
		}

#ifdef DEBUG
		if (s->pstate == SLOT_STOP && !(s->mode & MODE_PLAY)) {
			slot_log(s);
			log_puts(": rec-only slots can't be drained\n");
			panic();
		}
#endif
		/*
		 * check if stopped stream finished draining
		 */
		if (s->pstate == SLOT_STOP &&
		    s->mix.buf.used < s->round * s->mix.bpf) {
			/*
			 * partial blocks are zero-filled by socket
			 * layer, so s->mix.buf.used == 0 and we can
			 * destroy the buffer
			 */
			*ps = s->next;
			s->pstate = SLOT_INIT;
			s->ops->eof(s->arg);
			slot_freebufs(s);
			dev_mix_adjvol(d);
#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": drained\n");
			}
#endif
			continue;
		}

		/*
		 * check for xruns
		 */
		if (((s->mode & MODE_PLAY) &&
			s->mix.buf.used < s->round * s->mix.bpf) ||
		    ((s->mode & MODE_RECMASK) &&
			s->sub.buf.len - s->sub.buf.used <
			s->round * s->sub.bpf)) {

#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": xrun, pause cycle\n");
			}
#endif
			if (s->xrun == XRUN_IGNORE) {
				s->delta -= s->round;
				ps = &s->next;
			} else if (s->xrun == XRUN_SYNC) {
				s->skip++;
				ps = &s->next;
			} else if (s->xrun == XRUN_ERROR) {
				s->ops->exit(s->arg);
				*ps = s->next;
			} else {
#ifdef DEBUG
				slot_log(s);
				log_puts(": bad xrun mode\n");
				panic();
#endif
			}
			continue;
		}
		if ((s->mode & MODE_RECMASK) && !(s->pstate == SLOT_STOP)) {
			if (s->sub.prime == 0) {
				dev_sub_bcopy(d, s);
				s->ops->flush(s->arg);
			} else {
#ifdef DEBUG
				if (log_level >= 3) {
					slot_log(s);
					log_puts(": prime = ");
					log_puti(s->sub.prime);
					log_puts("\n");
				}
#endif
				s->sub.prime--;
			}
		}
		if (s->mode & MODE_PLAY) {
			dev_mix_badd(d, s);
			if (s->pstate != SLOT_STOP)
				s->ops->fill(s->arg);
		}
		ps = &s->next;
	}
	if ((d->mode & MODE_PLAY) && d->encbuf) {
		enc_do(&d->enc, (unsigned char *)DEV_PBUF(d),
		    d->encbuf, d->round);
	}
}

/*
 * called at every clock tick by the device
 */
void
dev_onmove(struct dev *d, int delta)
{
	long long pos;
	struct slot *s, *snext;

	d->delta += delta;

	for (s = d->slot_list; s != NULL; s = snext) {
		/*
		 * s->ops->onmove() may remove the slot
		 */
		snext = s->next;
		pos = (long long)delta * s->round + s->delta_rem;
		s->delta_rem = pos % d->round;
		s->delta += pos / (int)d->round;
		if (s->delta >= 0)
			s->ops->onmove(s->arg);
	}
	if (d->tstate == MMC_RUN)
		dev_midi_qfr(d, delta);
}

void
dev_master(struct dev *d, unsigned int master)
{
	if (log_level >= 2) {
		dev_log(d);
		log_puts(": master volume set to ");
		log_putu(master);
		log_puts("\n");
	}
	d->master = master;
	if (d->mode & MODE_PLAY)
		dev_mix_adjvol(d);
}

/*
 * return the latency that a stream would have if it's attached
 */
int
dev_getpos(struct dev *d)
{
	return (d->mode & MODE_PLAY) ? -d->bufsz : 0;
}

/*
 * Create a sndio device
 */
struct dev *
dev_new(char *path, struct aparams *par,
    unsigned int mode, unsigned int bufsz, unsigned int round,
    unsigned int rate, unsigned int hold, unsigned int autovol)
{
	struct dev *d;
	unsigned int i;

	if (dev_sndnum == DEV_NMAX) {
		if (log_level >= 1)
			log_puts("too many devices\n");
		return NULL;
	}
	d = xmalloc(sizeof(struct dev));
	d->path = xstrdup(path);
	d->num = dev_sndnum++;
	d->opt_list = NULL;

	/*
	 * XXX: below, we allocate a midi input buffer, since we don't
	 *	receive raw midi data, so no need to allocate a input
	 *	ibuf.  Possibly set imsg & fill callbacks to NULL and
	 *	use this to in midi_new() to check if buffers need to be
	 *	allocated
	 */
	d->midi = midi_new(&dev_midiops, d, MODE_MIDIIN | MODE_MIDIOUT);
	midi_tag(d->midi, d->num);
	d->reqpar = *par;
	d->reqmode = mode;
	d->reqpchan = d->reqrchan = 0;
	d->reqbufsz = bufsz;
	d->reqround = round;
	d->reqrate = rate;
	d->hold = hold;
	d->autovol = autovol;
	d->refcnt = 0;
	d->pstate = DEV_CFG;
	d->serial = 0;
	for (i = 0; i < DEV_NSLOT; i++) {
		d->slot[i].unit = i;
		d->slot[i].ops = NULL;
		d->slot[i].vol = MIDI_MAXCTL;
		d->slot[i].serial = d->serial++;
		strlcpy(d->slot[i].name, "prog", SLOT_NAMEMAX);
	}
	d->slot_list = NULL;
	d->master = MIDI_MAXCTL;
	d->mtc.origin = 0;
	d->tstate = MMC_STOP;
	d->next = dev_list;
	dev_list = d;
	return d;
}

/*
 * adjust device parameters and mode
 */
void
dev_adjpar(struct dev *d, int mode,
    int pmax, int rmax)
{
	d->reqmode |= mode & MODE_AUDIOMASK;
	if (mode & MODE_PLAY) {
		if (d->reqpchan < pmax + 1)
			d->reqpchan = pmax + 1;
	}
	if (mode & MODE_REC) {
		if (d->reqrchan < rmax + 1)
			d->reqrchan = rmax + 1;
	}
}

/*
 * Open the device with the dev_reqxxx capabilities. Setup a mixer, demuxer,
 * monitor, midi control, and any necessary conversions.
 */
int
dev_open(struct dev *d)
{
	d->mode = d->reqmode;
	d->round = d->reqround;
	d->bufsz = d->reqbufsz;
	d->rate = d->reqrate;
	d->pchan = d->reqpchan;
	d->rchan = d->reqrchan;
	d->par = d->reqpar;
	if (d->pchan == 0)
		d->pchan = 2;
	if (d->rchan == 0)
		d->rchan = 2;
	if (!dev_sio_open(d)) {
		if (log_level >= 1) {
			dev_log(d);
			log_puts(": ");
			log_puts(d->path);
			log_puts(": failed to open audio device\n");
		}
		return 0;
	}
	if (d->mode & MODE_REC) {
		/*
		 * Create device <-> demuxer buffer
		 */
		d->rbuf = xmalloc(d->round * d->rchan * sizeof(adata_t));

		/*
		 * Insert a converter, if needed.
		 */
		if (!aparams_native(&d->par)) {
			dec_init(&d->dec, &d->par, d->rchan);
			d->decbuf = xmalloc(d->round * d->rchan * d->par.bps);
		} else
			d->decbuf = NULL;
	}
	if (d->mode & MODE_PLAY) {
		/*
		 * Create device <-> mixer buffer
		 */
		d->poffs = 0;
		d->psize = d->bufsz + d->round;
		d->pbuf = xmalloc(d->psize * d->pchan * sizeof(adata_t));
		d->mode |= MODE_MON;

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_native(&d->par)) {
			enc_init(&d->enc, &d->par, d->pchan);
			d->encbuf = xmalloc(d->round * d->pchan * d->par.bps);
		} else
			d->encbuf = NULL;
	}
	d->pstate = DEV_INIT;
	if (log_level >= 2) {
		dev_log(d);
		log_puts(": ");
		log_putu(d->rate);
		log_puts("Hz, ");
		aparams_log(&d->par);
		if (d->mode & MODE_PLAY) {
			log_puts(", play 0:");
			log_puti(d->pchan - 1);
		}
		if (d->mode & MODE_REC) {
			log_puts(", rec 0:");
			log_puti(d->rchan - 1);
		}
		log_puts(", ");
		log_putu(d->bufsz / d->round);
		log_puts(" blocks of ");
		log_putu(d->round);
		log_puts(" frames\n");
	}
	return 1;
}

/*
 * force the device to go in DEV_CFG state, the caller is supposed to
 * ensure buffers are drained
 */
void
dev_close(struct dev *d)
{
	int i;
	struct slot *s;

#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": closing\n");
	}
#endif
	d->pstate = DEV_CFG;
	for (s = d->slot, i = DEV_NSLOT; i > 0; i--, s++) {
		if (s->ops)
			s->ops->exit(s->arg);
		s->ops = NULL;
	}
	d->slot_list = NULL;
	dev_sio_close(d);
	if (d->mode & MODE_PLAY) {
		if (d->encbuf != NULL)
			xfree(d->encbuf);
		xfree(d->pbuf);
	}
	if (d->mode & MODE_REC) {
		if (d->decbuf != NULL)
			xfree(d->decbuf);
		xfree(d->rbuf);
	}
}

int
dev_ref(struct dev *d)
{
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": device requested\n");
	}
#endif
	if (d->pstate == DEV_CFG && !dev_open(d))
		return 0;
	d->refcnt++;
	return 1;
}

void
dev_unref(struct dev *d)
{
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": device released\n");
	}
#endif
	d->refcnt--;
	if (d->refcnt == 0 && d->pstate == DEV_INIT)
		dev_close(d);
}

/*
 * initialize the device with the current parameters
 */
int
dev_init(struct dev *d)
{
	if ((d->reqmode & MODE_AUDIOMASK) == 0) {
#ifdef DEBUG
		    dev_log(d);
		    log_puts(": has no streams\n");
#endif
		    return 0;
	}
	if (d->hold && !dev_ref(d))
		return 0;
	return 1;
}

/*
 * Unless the device is already in process of closing, request it to close
 */
void
dev_done(struct dev *d)
{
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": draining\n");
	}
#endif
	if (d->tstate != MMC_STOP)
		dev_mmcstop(d);
	if (d->hold)
		dev_unref(d);
}

struct dev *
dev_bynum(int num)
{
	struct dev *d;

	for (d = dev_list; d != NULL; d = d->next) {
		if (d->num == num)
			return d;
	}
	return NULL;
}

/*
 * Free the device
 */
void
dev_del(struct dev *d)
{
	struct dev **p;

#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": deleting\n");
	}
#endif
	while (d->opt_list != NULL)
		opt_del(d, d->opt_list);
	if (d->pstate != DEV_CFG)
		dev_close(d);
	for (p = &dev_list; *p != d; p = &(*p)->next) {
#ifdef DEBUG
		if (*p == NULL) {
			dev_log(d);
			log_puts(": device to delete not on the list\n");
			panic();
		}
#endif
	}
	midi_del(d->midi);
	*p = d->next;
	xfree(d->path);
	xfree(d);
}

unsigned int
dev_roundof(struct dev *d, unsigned int newrate)
{
	return (d->round * newrate + d->rate / 2) / d->rate;
}

/*
 * If the device is paused, then resume it.
 */
void
dev_wakeup(struct dev *d)
{
	if (d->pstate == DEV_INIT) {
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": device started\n");
		}
		if (d->mode & MODE_PLAY) {
			d->prime = d->bufsz;
		} else {
			d->prime = 0;
		}
		d->poffs = 0;

		/*
		 * empty cycles don't increment delta, so it's ok to
		 * start at 0
		 **/
		d->delta = 0;

		d->pstate = DEV_RUN;
		dev_sio_start(d);
	}
}

/*
 * check that all clients controlled by MMC are ready to start, if so,
 * attach them all at the same position
 */
void
dev_sync_attach(struct dev *d)
{
	int i;
	struct slot *s;

	if (d->tstate != MMC_START) {
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": not started by mmc yet, waiting...\n");
		}
		return;
	}
	for (i = 0; i < DEV_NSLOT; i++) {
		s = d->slot + i;
		if (!s->ops || !s->opt->mmc)
			continue;
		if (s->pstate != SLOT_READY) {
#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": not ready, start delayed\n");
			}
#endif
			return;
		}
	}
	if (!dev_ref(d))
		return;
	for (i = 0; i < DEV_NSLOT; i++) {
		s = d->slot + i;
		if (!s->ops || !s->opt->mmc)
			continue;
		slot_attach(s);
	}
	d->tstate = MMC_RUN;
	dev_midi_full(d);
	dev_wakeup(d);
}

/*
 * start all slots simultaneously
 */
void
dev_mmcstart(struct dev *d)
{
	if (d->tstate == MMC_STOP) {
		d->tstate = MMC_START;
		dev_sync_attach(d);
#ifdef DEBUG
	} else {
		if (log_level >= 3) {
			dev_log(d);
			log_puts(": ignoring mmc start\n");
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
	switch (d->tstate) {
	case MMC_START:
		d->tstate = MMC_STOP;
		return;
	case MMC_RUN:
		d->tstate = MMC_STOP;
		dev_unref(d);
		break;
	default:
#ifdef DEBUG
		if (log_level >= 3) {
			dev_log(d);
			log_puts(": ignored mmc stop\n");
		}
#endif
		return;
	}
}

/*
 * relocate all slots simultaneously
 */
void
dev_mmcloc(struct dev *d, unsigned int origin)
{
	if (log_level >= 2) {
		dev_log(d);
		log_puts(": relocated to ");
		log_putu(origin);
		log_puts("\n");
	}
	if (d->tstate == MMC_RUN)
		dev_mmcstop(d);
	d->mtc.origin = origin;
	if (d->tstate == MMC_RUN)
		dev_mmcstart(d);
}


/*
 * allocate buffers & conversion chain
 */
void
slot_allocbufs(struct slot *s)
{
	unsigned int dev_nch;
	struct dev *d = s->dev;

	if (s->mode & MODE_PLAY) {
		s->mix.bpf = s->par.bps * s->mix.nch;
		abuf_init(&s->mix.buf, s->appbufsz * s->mix.bpf);

		dev_nch = s->opt->pmax - s->opt->pmin + 1;
		s->mix.decbuf = NULL;
		s->mix.resampbuf = NULL;
		s->mix.join = 1;
		s->mix.expand = 1;
		if (s->opt->dup) {
			if (dev_nch > s->mix.nch)
				s->mix.expand = dev_nch / s->mix.nch;
			else if (dev_nch < s->mix.nch)
				s->mix.join = s->mix.nch / dev_nch;
		}
		cmap_init(&s->mix.cmap,
		    s->opt->pmin, s->opt->pmin + s->mix.nch - 1,
		    s->opt->pmin, s->opt->pmin + s->mix.nch - 1,
		    0, d->pchan - 1,
		    s->opt->pmin, s->opt->pmax);
		if (!aparams_native(&s->par)) {
			dec_init(&s->mix.dec, &s->par, s->mix.nch);
			s->mix.decbuf =
			    xmalloc(s->round * s->mix.nch * sizeof(adata_t));
		}
		if (s->rate != d->rate) {
			resamp_init(&s->mix.resamp, s->round, d->round,
			    s->mix.nch);
			s->mix.resampbuf =
			    xmalloc(d->round * s->mix.nch * sizeof(adata_t));
		}
	}

	if (s->mode & MODE_RECMASK) {
		s->sub.bpf = s->par.bps * s->sub.nch;
		abuf_init(&s->sub.buf, s->appbufsz * s->sub.bpf);

		dev_nch = s->opt->rmax - s->opt->rmin + 1;
		s->sub.encbuf = NULL;
		s->sub.resampbuf = NULL;
		s->sub.join = 1;
		s->sub.expand = 1;
		if (s->opt->dup) {
			if (dev_nch > s->sub.nch)
				s->sub.join = dev_nch / s->sub.nch;
			else if (dev_nch < s->sub.nch)
				s->sub.expand = s->sub.nch / dev_nch;
		}
		cmap_init(&s->sub.cmap,
		    0, ((s->mode & MODE_MON) ? d->pchan : d->rchan) - 1,
		    s->opt->rmin, s->opt->rmax,
		    s->opt->rmin, s->opt->rmin + s->sub.nch - 1,
		    s->opt->rmin, s->opt->rmin + s->sub.nch - 1);
		if (s->rate != d->rate) {
			resamp_init(&s->sub.resamp, d->round, s->round,
			    s->sub.nch);
			s->sub.resampbuf =
			    xmalloc(d->round * s->sub.nch * sizeof(adata_t));
		}
		if (!aparams_native(&s->par)) {
			enc_init(&s->sub.enc, &s->par, s->sub.nch);
			s->sub.encbuf =
			    xmalloc(s->round * s->sub.nch * sizeof(adata_t));
		}

		/*
		 * cmap_copy() doesn't write samples in all channels,
	         * for instance when mono->stereo conversion is
	         * disabled. So we have to prefill cmap_copy() output
	         * with silence.
	         */
		if (s->sub.resampbuf) {
			memset(s->sub.resampbuf, 0,
			    d->round * s->sub.nch * sizeof(adata_t));
		} else if (s->sub.encbuf) {
			memset(s->sub.encbuf, 0,
			    s->round * s->sub.nch * sizeof(adata_t));
		} else {
			memset(s->sub.buf.data, 0,
			    s->appbufsz * s->sub.nch * sizeof(adata_t));
		}
	}

#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": allocated ");
		log_putu(s->appbufsz);
		log_puts("/");
		log_putu(SLOT_BUFSZ(s));
		log_puts(" fr buffers\n");
	}
#endif
}

/*
 * free buffers & conversion chain
 */
void
slot_freebufs(struct slot *s)
{
	if (s->mode & MODE_RECMASK) {
		abuf_done(&s->sub.buf);
		if (s->sub.encbuf)
			xfree(s->sub.encbuf);
		if (s->sub.resampbuf)
			xfree(s->sub.resampbuf);
	}

	if (s->mode & MODE_PLAY) {
		abuf_done(&s->mix.buf);
		if (s->mix.decbuf)
			xfree(s->mix.decbuf);
		if (s->mix.resampbuf)
			xfree(s->mix.resampbuf);
	}
}

/*
 * allocate a new slot and register the given call-backs
 */
struct slot *
slot_new(struct dev *d, struct opt *opt, char *who,
    struct slotops *ops, void *arg, int mode)
{
	char *p;
	char name[SLOT_NAMEMAX];
	unsigned int i, unit, umap = 0;
	unsigned int ser, bestser, bestidx;
	struct slot *s;

	/*
	 * create a ``valid'' control name (lowcase, remove [^a-z], truncate)
	 */
	for (i = 0, p = who; ; p++) {
		if (i == SLOT_NAMEMAX - 1 || *p == '\0') {
			name[i] = '\0';
			break;
		} else if (*p >= 'A' && *p <= 'Z') {
			name[i++] = *p + 'a' - 'A';
		} else if (*p >= 'a' && *p <= 'z')
			name[i++] = *p;
	}
	if (i == 0)
		strlcpy(name, "noname", SLOT_NAMEMAX);

	/*
	 * find the first unused "unit" number for this name
	 */
	for (i = 0, s = d->slot; i < DEV_NSLOT; i++, s++) {
		if (s->ops == NULL)
			continue;
		if (strcmp(s->name, name) == 0)
			umap |= (1 << s->unit);
	}
	for (unit = 0; ; unit++) {
		if ((umap & (1 << unit)) == 0)
			break;
	}

	/*
	 * find a free controller slot with the same name/unit
	 */
	for (i = 0, s = d->slot; i < DEV_NSLOT; i++, s++) {
		if (s->ops == NULL &&
		    strcmp(s->name, name) == 0 &&
		    s->unit == unit) {
#ifdef DEBUG
			if (log_level >= 3) {
				log_puts(name);
				log_putu(unit);
				log_puts(": reused\n");
			}
#endif
			goto found;
		}
	}

	/*
	 * couldn't find a matching slot, pick oldest free slot
	 * and set its name/unit
	 */
	bestser = 0;
	bestidx = DEV_NSLOT;
	for (i = 0, s = d->slot; i < DEV_NSLOT; i++, s++) {
		if (s->ops != NULL)
			continue;
		ser = d->serial - s->serial;
		if (ser > bestser) {
			bestser = ser;
			bestidx = i;
		}
	}
	if (bestidx == DEV_NSLOT) {
		if (log_level >= 1) {
			log_puts(name);
			log_putu(unit);
			log_puts(": out of sub-device slots\n");
		}
		return NULL;
	}
	s = d->slot + bestidx;
	if (s->name[0] != '\0')
		s->vol = MIDI_MAXCTL;
	strlcpy(s->name, name, SLOT_NAMEMAX);
	s->serial = d->serial++;
	s->unit = unit;
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts(name);
		log_putu(unit);
		log_puts(": overwritten slot ");
		log_putu(bestidx);
		log_puts("\n");
	}
#endif

found:
	if ((mode & MODE_REC) && (opt->mode & MODE_MON)) {
		mode |= MODE_MON;
		mode &= ~MODE_REC;
	}
	if ((mode & opt->mode) != mode) {
		if (log_level >= 1) {
			slot_log(s);
			log_puts(": requested mode not allowed\n");
		}
		return 0;
	}
	if (!dev_ref(d))
		return NULL;
	if ((mode & d->mode) != mode) {
		if (log_level >= 1) {
			slot_log(s);
			log_puts(": requested mode not supported\n");
		}
		dev_unref(d);
		return 0;
	}
	s->dev = d;
	s->opt = opt;
	s->ops = ops;
	s->arg = arg;
	s->pstate = SLOT_INIT;
	s->mode = mode;
	aparams_init(&s->par);
	if (s->mode & MODE_PLAY)
		s->mix.nch = s->opt->pmax - s->opt->pmin + 1;
	if (s->mode & MODE_RECMASK)
		s->sub.nch = s->opt->rmax - s->opt->rmin + 1;
	s->xrun = s->opt->mmc ? XRUN_SYNC : XRUN_IGNORE;
	s->appbufsz = d->bufsz;
	s->round = d->round;
	s->rate = d->rate;
	dev_midi_slotdesc(d, s);
	dev_midi_vol(d, s);
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": using ");
		dev_log(d);
		log_puts(".");
		log_puts(opt->name);
		log_puts(", mode = ");
		log_putx(mode);
		log_puts("\n");
	}
#endif
	return s;
}

/*
 * release the given slot
 */
void
slot_del(struct slot *s)
{
	s->arg = s;
	s->ops = &zomb_slotops;
	switch (s->pstate) {
	case SLOT_INIT:
		s->ops = NULL;
		break;
	case SLOT_START:
	case SLOT_READY:
	case SLOT_RUN:
		slot_stop(s);
		/* PASSTHROUGH */
	case SLOT_STOP:
		break;
	}
	dev_unref(s->dev);
	s->dev = NULL;
}

/*
 * change the slot play volume; called either by the slot or by MIDI
 */
void
slot_setvol(struct slot *s, unsigned int vol)
{
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": setting volume ");
		log_putu(vol);
		log_puts("\n");
	}
#endif
	s->vol = vol;
	s->mix.vol = MIDI_TO_ADATA(s->vol);
}

/*
 * attach the slot to the device (ie start playing & recording
 */
void
slot_attach(struct slot *s)
{
	struct dev *d = s->dev;
	long long pos;
	int startpos;

	/*
	 * start the device if not started
	 */
	dev_wakeup(d);

	/*
	 * get the current position, the origin is when the first sample
	 * played and/or recorded
	 */
	startpos = dev_getpos(d) * (int)s->round / (int)d->round;

	/*
	 * adjust initial clock
	 */
	pos = (long long)d->delta * s->round;
	s->delta = startpos + pos / (int)d->round;
	s->delta_rem = pos % d->round;

	s->pstate = SLOT_RUN;
#ifdef DEBUG
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": attached at ");
		log_puti(startpos);
		log_puts(", delta = ");
		log_puti(d->delta);
		log_puts("\n");
	}
#endif

	/*
	 * We dont check whether the device is dying,
	 * because dev_xxx() functions are supposed to
	 * work (i.e., not to crash)
	 */
#ifdef DEBUG
	if ((s->mode & d->mode) != s->mode) {
		slot_log(s);
		log_puts(": mode beyond device mode, not attaching\n");
		panic();
	}
#endif
	s->next = d->slot_list;
	d->slot_list = s;
	if (s->mode & MODE_PLAY) {
		s->mix.vol = MIDI_TO_ADATA(s->vol);
		dev_mix_adjvol(d);
	}
}

/*
 * if MMC is enabled, and try to attach all slots synchronously, else
 * simply attach the slot
 */
void
slot_ready(struct slot *s)
{
	/*
	 * device may be disconnected, and if so we're called from
	 * slot->ops->exit() on a closed device
	 */
	if (s->dev->pstate == DEV_CFG)
		return;
	if (!s->opt->mmc)
		slot_attach(s);
	else
		dev_sync_attach(s->dev);
}

/*
 * setup buffers & conversion layers, prepare the slot to receive data
 * (for playback) or start (recording).
 */
void
slot_start(struct slot *s)
{
#ifdef DEBUG
	if (s->pstate != SLOT_INIT) {
		slot_log(s);
		log_puts(": slot_start: wrong state\n");
		panic();
	}
	if (s->mode & MODE_PLAY) {
		if (log_level >= 3) {
			slot_log(s);
			log_puts(": playing ");
			aparams_log(&s->par);
			log_puts(" -> ");
			aparams_log(&s->dev->par);
			log_puts("\n");
		}
	}
	if (s->mode & MODE_RECMASK) {
		if (log_level >= 3) {
			slot_log(s);
			log_puts(": recording ");
			aparams_log(&s->par);
			log_puts(" <- ");
			aparams_log(&s->dev->par);
			log_puts("\n");
		}
	}
#endif
	slot_allocbufs(s);

	if (s->mode & MODE_RECMASK) {
		/*
		 * N-th recorded block is the N-th played block
		 */
		s->sub.prime = -dev_getpos(s->dev) / s->dev->round;
	}
	s->skip = 0;

	if (s->mode & MODE_PLAY) {
		s->pstate = SLOT_START;
	} else {
		s->pstate = SLOT_READY;
		slot_ready(s);
	}
}

/*
 * stop playback and recording, and free conversion layers
 */
void
slot_detach(struct slot *s)
{
	struct slot **ps;

#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": detaching\n");
	}
#endif
	for (ps = &s->dev->slot_list; *ps != s; ps = &(*ps)->next) {
#ifdef DEBUG
		if (*ps == NULL) {
			slot_log(s);
			log_puts(": can't detach, not on list\n");
			panic();
		}
#endif
	}
	*ps = s->next;
	if (s->mode & MODE_PLAY)
		dev_mix_adjvol(s->dev);
}

/*
 * put the slot in stopping state (draining play buffers) or
 * stop & detach if no data to drain.
 */
void
slot_stop(struct slot *s)
{
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": stopping\n");
	}
#endif
	if (s->pstate == SLOT_START) {
		/*
		 * If in rec-only mode, we're already in the READY or
		 * RUN states. We're here because the play buffer was
		 * not full enough, try to start so it's drained.
		 */
		s->pstate = SLOT_READY;
		slot_ready(s);
	}

	if (s->pstate == SLOT_RUN) {
		if (s->mode & MODE_PLAY) {
			/*
			 * Don't detach, dev_cycle() will do it for us
			 * when the buffer is drained.
			 */
			s->pstate = SLOT_STOP;
			return;
		}
		slot_detach(s);
	} else {
#ifdef DEBUG
		if (log_level >= 3) {
			slot_log(s);
			log_puts(": not drained (blocked by mmc)\n");
		}
#endif
	}

	s->pstate = SLOT_INIT;
	s->ops->eof(s->arg);
	slot_freebufs(s);
}

void
slot_skip_update(struct slot *s)
{
	int skip;

	skip = slot_skip(s);
	while (skip > 0) {
#ifdef DEBUG
		if (log_level >= 4) {
			slot_log(s);
			log_puts(": catching skipped block\n");
		}
#endif
		if (s->mode & MODE_RECMASK)
			s->ops->flush(s->arg);
		if (s->mode & MODE_PLAY)
			s->ops->fill(s->arg);
		skip--;
	}
}

/*
 * notify the slot that we just wrote in the play buffer, must be called
 * after each write
 */
void
slot_write(struct slot *s)
{
	if (s->pstate == SLOT_START && s->mix.buf.used == s->mix.buf.len) {
#ifdef DEBUG
		if (log_level >= 4) {
			slot_log(s);
			log_puts(": switching to READY state\n");
		}
#endif
		s->pstate = SLOT_READY;
		slot_ready(s);
	}
	slot_skip_update(s);
}

/*
 * notify the slot that we freed some space in the rec buffer
 */
void
slot_read(struct slot *s)
{
	slot_skip_update(s);
}
