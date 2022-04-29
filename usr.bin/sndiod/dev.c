/*	$OpenBSD: dev.c,v 1.105 2022/04/29 09:12:57 ratchov Exp $	*/
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

void dev_mix_badd(struct dev *, struct slot *);
void dev_mix_adjvol(struct dev *);
void dev_sub_bcopy(struct dev *, struct slot *);

void dev_onmove(struct dev *, int);
void dev_master(struct dev *, unsigned int);
void dev_cycle(struct dev *);
struct dev *dev_new(char *, struct aparams *, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, unsigned int);
void dev_adjpar(struct dev *, int, int, int);
int dev_allocbufs(struct dev *);
void dev_freebufs(struct dev *);
int dev_ref(struct dev *);
void dev_unref(struct dev *);
int dev_init(struct dev *);
void dev_done(struct dev *);
struct dev *dev_bynum(int);
void dev_del(struct dev *);
unsigned int dev_roundof(struct dev *, unsigned int);
void dev_wakeup(struct dev *);

void slot_ctlname(struct slot *, char *, size_t);
void slot_log(struct slot *);
void slot_del(struct slot *);
void slot_setvol(struct slot *, unsigned int);
void slot_ready(struct slot *);
void slot_allocbufs(struct slot *);
void slot_freebufs(struct slot *);
void slot_skip_update(struct slot *);
void slot_write(struct slot *);
void slot_read(struct slot *);
int slot_skip(struct slot *);

void ctl_node_log(struct ctl_node *);
void ctl_log(struct ctl *);

struct slotops zomb_slotops = {
	zomb_onmove,
	zomb_onvol,
	zomb_fill,
	zomb_flush,
	zomb_eof,
	zomb_exit
};

struct ctl *ctl_list = NULL;
struct dev *dev_list = NULL;
unsigned int dev_sndnum = 0;

struct ctlslot ctlslot_array[DEV_NCTLSLOT];
struct slot slot_array[DEV_NSLOT];
unsigned int slot_serial;		/* for slot allocation */

/*
 * we support/need a single MTC clock source only
 */
struct mtc mtc_array[1] = {
	{.dev = NULL, .tstate = MTC_STOP}
};

void
slot_array_init(void)
{
	unsigned int i;

	for (i = 0; i < DEV_NSLOT; i++) {
		slot_array[i].unit = i;
		slot_array[i].ops = NULL;
		slot_array[i].vol = MIDI_MAXCTL;
		slot_array[i].opt = NULL;
		slot_array[i].serial = slot_serial++;
		memset(slot_array[i].name, 0, SLOT_NAMEMAX);
	}
}

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
slot_ctlname(struct slot *s, char *name, size_t size)
{
	snprintf(name, size, "%s%u", s->name, s->unit);
}

void
slot_log(struct slot *s)
{
	char name[CTL_NAMEMAX];
#ifdef DEBUG
	static char *pstates[] = {
		"ini", "sta", "rdy", "run", "stp", "mid"
	};
#endif
	slot_ctlname(s, name, CTL_NAMEMAX);
	log_puts(name);
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
 * Broadcast MIDI data to all opts using this device
 */
void
dev_midi_send(struct dev *d, void *msg, int msglen)
{
	struct opt *o;

	for (o = opt_list; o != NULL; o = o->next) {
		if (o->dev != d)
			continue;
		midi_send(o->midi, msg, msglen);
	}
}

/*
 * send a quarter frame MTC message
 */
void
mtc_midi_qfr(struct mtc *mtc, int delta)
{
	unsigned char buf[2];
	unsigned int data;
	int qfrlen;

	mtc->delta += delta * MTC_SEC;
	qfrlen = mtc->dev->rate * (MTC_SEC / (4 * mtc->fps));
	while (mtc->delta >= qfrlen) {
		switch (mtc->qfr) {
		case 0:
			data = mtc->fr & 0xf;
			break;
		case 1:
			data = mtc->fr >> 4;
			break;
		case 2:
			data = mtc->sec & 0xf;
			break;
		case 3:
			data = mtc->sec >> 4;
			break;
		case 4:
			data = mtc->min & 0xf;
			break;
		case 5:
			data = mtc->min >> 4;
			break;
		case 6:
			data = mtc->hr & 0xf;
			break;
		case 7:
			data = (mtc->hr >> 4) | (mtc->fps_id << 1);
			/*
			 * tick messages are sent 2 frames ahead
			 */
			mtc->fr += 2;
			if (mtc->fr < mtc->fps)
				break;
			mtc->fr -= mtc->fps;
			mtc->sec++;
			if (mtc->sec < 60)
				break;
			mtc->sec = 0;
			mtc->min++;
			if (mtc->min < 60)
				break;
			mtc->min = 0;
			mtc->hr++;
			if (mtc->hr < 24)
				break;
			mtc->hr = 0;
			break;
		default:
			/* NOTREACHED */
			data = 0;
		}
		buf[0] = 0xf1;
		buf[1] = (mtc->qfr << 4) | data;
		mtc->qfr++;
		mtc->qfr &= 7;
		dev_midi_send(mtc->dev, buf, 2);
		mtc->delta -= qfrlen;
	}
}

/*
 * send a full frame MTC message
 */
void
mtc_midi_full(struct mtc *mtc)
{
	struct sysex x;
	unsigned int fps;

	mtc->delta = -MTC_SEC * (int)mtc->dev->bufsz;
	if (mtc->dev->rate % (30 * 4 * mtc->dev->round) == 0) {
		mtc->fps_id = MTC_FPS_30;
		mtc->fps = 30;
	} else if (mtc->dev->rate % (25 * 4 * mtc->dev->round) == 0) {
		mtc->fps_id = MTC_FPS_25;
		mtc->fps = 25;
	} else {
		mtc->fps_id = MTC_FPS_24;
		mtc->fps = 24;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(mtc->dev);
		log_puts(": mtc full frame at ");
		log_puti(mtc->delta);
		log_puts(", ");
		log_puti(mtc->fps);
		log_puts(" fps\n");
	}
#endif
	fps = mtc->fps;
	mtc->hr =  (mtc->origin / (MTC_SEC * 3600)) % 24;
	mtc->min = (mtc->origin / (MTC_SEC * 60))   % 60;
	mtc->sec = (mtc->origin / (MTC_SEC))        % 60;
	mtc->fr =  (mtc->origin / (MTC_SEC / fps))  % fps;

	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_RT;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_MTC;
	x.id1 = SYSEX_MTC_FULL;
	x.u.full.hr = mtc->hr | (mtc->fps_id << 5);
	x.u.full.min = mtc->min;
	x.u.full.sec = mtc->sec;
	x.u.full.fr = mtc->fr;
	x.u.full.end = SYSEX_END;
	mtc->qfr = 0;
	dev_midi_send(mtc->dev, (unsigned char *)&x, SYSEX_SIZE(full));
}

/*
 * send a volume change MIDI message
 */
void
dev_midi_vol(struct dev *d, struct slot *s)
{
	unsigned char msg[3];

	msg[0] = MIDI_CTL | (s - slot_array);
	msg[1] = MIDI_CTL_VOL;
	msg[2] = s->vol;
	dev_midi_send(d, msg, 3);
}

/*
 * send a master volume MIDI message
 */
void
dev_midi_master(struct dev *d)
{
	struct ctl *c;
	unsigned int master, v;
	struct sysex x;

	if (d->master_enabled)
		master = d->master;
	else {
		master = 0;
		for (c = ctl_list; c != NULL; c = c->next) {
			if (c->type != CTL_NUM ||
			    strcmp(c->group, d->name) != 0 ||
			    strcmp(c->node0.name, "output") != 0 ||
			    strcmp(c->func, "level") != 0)
				continue;
			if (c->u.any.arg0 != d)
				continue;
			v = (c->curval * 127 + c->maxval / 2) / c->maxval;
			if (master < v)
				master = v;
		}
	}

	memset(&x, 0, sizeof(struct sysex));
	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_RT;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_CONTROL;
	x.id1 = SYSEX_MASTER;
	x.u.master.fine = 0;
	x.u.master.coarse = master;
	x.u.master.end = SYSEX_END;
	dev_midi_send(d, (unsigned char *)&x, SYSEX_SIZE(master));
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
	if (s->opt != NULL && s->opt->dev == d)
		slot_ctlname(s, (char *)x.u.slotdesc.name, SYSEX_NAMELEN);
	x.u.slotdesc.chan = (s - slot_array);
	x.u.slotdesc.end = SYSEX_END;
	dev_midi_send(d, (unsigned char *)&x, SYSEX_SIZE(slotdesc));
}

void
dev_midi_dump(struct dev *d)
{
	struct sysex x;
	struct slot *s;
	int i;

	dev_midi_master(d);
	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->opt != NULL && s->opt->dev != d)
			continue;
		dev_midi_slotdesc(d, s);
		dev_midi_vol(d, s);
	}
	x.start = SYSEX_START;
	x.type = SYSEX_TYPE_EDU;
	x.dev = SYSEX_DEV_ANY;
	x.id0 = SYSEX_AUCAT;
	x.id1 = SYSEX_AUCAT_DUMPEND;
	x.u.dumpend.end = SYSEX_END;
	dev_midi_send(d, (unsigned char *)&x, SYSEX_SIZE(dumpend));
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
	if (!(s->opt->mode & MODE_PLAY)) {
		/*
		 * playback not allowed in opt structure, produce silence
		 */
		abuf_rdiscard(&s->mix.buf, s->round * s->mix.bpf);
		return;
	}


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
		i->mix.weight = d->master_enabled ?
		    ADATA_MUL(weight, MIDI_TO_ADATA(d->master)) : weight;
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


	odata = (adata_t *)abuf_wgetblk(&s->sub.buf, &ocount);
#ifdef DEBUG
	if (ocount < s->round * s->sub.bpf) {
		log_puts("dev_sub_bcopy: not enough space\n");
		panic();
	}
#endif
	if (s->opt->mode & MODE_MON) {
		moffs = d->poffs + d->round;
		if (moffs == d->psize)
			moffs = 0;
		idata = d->pbuf + moffs * d->pchan;
	} else if (s->opt->mode & MODE_REC) {
		idata = d->rbuf;
	} else {
		/*
		 * recording not allowed in opt structure, produce silence
		 */
		enc_sil_do(&s->sub.enc, odata, s->round);
		abuf_wcommit(&s->sub.buf, s->round * s->sub.bpf);
		return;
	}

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
	if (d->slot_list == NULL && d->idle >= d->bufsz &&
	    (mtc_array[0].dev != d || mtc_array[0].tstate != MTC_RUN)) {
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
		d->idle = 0;

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

	if (d->slot_list == NULL)
		d->idle += delta;

	for (s = d->slot_list; s != NULL; s = snext) {
		/*
		 * s->ops->onmove() may remove the slot
		 */
		snext = s->next;
		pos = s->delta_rem +
		    (long long)s->delta * d->round +
		    (long long)delta * s->round;
		s->delta = pos / (int)d->round;
		s->delta_rem = pos % d->round;
		if (s->delta_rem < 0) {
			s->delta_rem += d->round;
			s->delta--;
		}
		if (s->delta >= 0)
			s->ops->onmove(s->arg);
	}

	if (mtc_array[0].dev == d && mtc_array[0].tstate == MTC_RUN)
		mtc_midi_qfr(&mtc_array[0], delta);
}

void
dev_master(struct dev *d, unsigned int master)
{
	struct ctl *c;
	unsigned int v;

	if (log_level >= 2) {
		dev_log(d);
		log_puts(": master volume set to ");
		log_putu(master);
		log_puts("\n");
	}
	if (d->master_enabled) {
		d->master = master;
		if (d->mode & MODE_PLAY)
			dev_mix_adjvol(d);
	} else {
		for (c = ctl_list; c != NULL; c = c->next) {
			if (c->scope != CTL_HW || c->u.hw.dev != d)
				continue;
			if (c->type != CTL_NUM ||
			    strcmp(c->group, d->name) != 0 ||
			    strcmp(c->node0.name, "output") != 0 ||
			    strcmp(c->func, "level") != 0)
				continue;
			v = (master * c->maxval + 64) / 127;
			ctl_setval(c, v);
		}
	}
}

/*
 * Create a sndio device
 */
struct dev *
dev_new(char *path, struct aparams *par,
    unsigned int mode, unsigned int bufsz, unsigned int round,
    unsigned int rate, unsigned int hold, unsigned int autovol)
{
	struct dev *d, **pd;

	if (dev_sndnum == DEV_NMAX) {
		if (log_level >= 1)
			log_puts("too many devices\n");
		return NULL;
	}
	d = xmalloc(sizeof(struct dev));
	d->path = path;
	d->num = dev_sndnum++;

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
	d->slot_list = NULL;
	d->master = MIDI_MAXCTL;
	d->master_enabled = 0;
	d->alt_next = d;
	snprintf(d->name, CTL_NAMEMAX, "%u", d->num);
	for (pd = &dev_list; *pd != NULL; pd = &(*pd)->next)
		;
	d->next = *pd;
	*pd = d;
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
 *
 * Note that record and play buffers are always allocated, even if the
 * underlying device doesn't support both modes.
 */
int
dev_allocbufs(struct dev *d)
{
	/*
	 * Create record buffer.
	 */

	 /* Create device <-> demuxer buffer */
	d->rbuf = xmalloc(d->round * d->rchan * sizeof(adata_t));

	/* Insert a converter, if needed. */
	if (!aparams_native(&d->par)) {
		dec_init(&d->dec, &d->par, d->rchan);
		d->decbuf = xmalloc(d->round * d->rchan * d->par.bps);
	} else
		d->decbuf = NULL;

	/*
	 * Create play buffer
	 */

	/* Create device <-> mixer buffer */
	d->poffs = 0;
	d->psize = d->bufsz + d->round;
	d->pbuf = xmalloc(d->psize * d->pchan * sizeof(adata_t));
	d->mode |= MODE_MON;

	/* Append a converter, if needed. */
	if (!aparams_native(&d->par)) {
		enc_init(&d->enc, &d->par, d->pchan);
		d->encbuf = xmalloc(d->round * d->pchan * d->par.bps);
	} else
		d->encbuf = NULL;

	/*
	 * Initially fill the record buffer with zeroed samples. This ensures
	 * that when a client records from a play-only device the client just
	 * gets silence.
	 */
	memset(d->rbuf, 0, d->round * d->rchan * sizeof(adata_t));

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
		log_puts(" frames");
		if (d == mtc_array[0].dev)
			log_puts(", mtc");
		log_puts("\n");
	}
	return 1;
}

/*
 * Reset parameters and open the device.
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
			log_puts(": failed to open audio device\n");
		}
		return 0;
	}
	if (!dev_allocbufs(d))
		return 0;

	d->pstate = DEV_INIT;
	return 1;
}

/*
 * Force all slots to exit and close device, called after an error
 */
void
dev_abort(struct dev *d)
{
	int i;
	struct slot *s;
	struct ctlslot *c;
	struct opt *o;

	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->opt == NULL || s->opt->dev != d)
			continue;
		if (s->ops) {
			s->ops->exit(s->arg);
			s->ops = NULL;
		}
	}
	d->slot_list = NULL;

	for (o = opt_list; o != NULL; o = o->next) {
		if (o->dev != d)
			continue;
		for (c = ctlslot_array, i = 0; i < DEV_NCTLSLOT; i++, c++) {
			if (c->ops == NULL)
				continue;
			if (c->opt == o) {
				c->ops->exit(s->arg);
				c->ops = NULL;
			}
		}

		midi_abort(o->midi);
	}

	if (d->pstate != DEV_CFG)
		dev_close(d);
}

/*
 * force the device to go in DEV_CFG state, the caller is supposed to
 * ensure buffers are drained
 */
void
dev_freebufs(struct dev *d)
{
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": closing\n");
	}
#endif
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

/*
 * Close the device and exit all slots
 */
void
dev_close(struct dev *d)
{
	d->pstate = DEV_CFG;
	dev_sio_close(d);
	dev_freebufs(d);

	if (d->master_enabled) {
		d->master_enabled = 0;
		ctl_del(CTL_DEV_MASTER, d, NULL);
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
	if (mtc_array[0].dev == d && mtc_array[0].tstate != MTC_STOP)
		mtc_stop(&mtc_array[0]);
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
	*p = d->next;
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
		d->idle = 0;
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
 * Return true if both of the given devices can run the same
 * clients
 */
int
dev_iscompat(struct dev *o, struct dev *n)
{
	if (((long long)o->round * n->rate != (long long)n->round * o->rate) ||
	    ((long long)o->bufsz * n->rate != (long long)n->bufsz * o->rate)) {
		if (log_level >= 1) {
			log_puts(n->name);
			log_puts(": not compatible with ");
			log_puts(o->name);
			log_puts("\n");
		}
		return 0;
	}
	return 1;
}

/*
 * Close the device, but attempt to migrate everything to a new sndio
 * device.
 */
struct dev *
dev_migrate(struct dev *odev)
{
	struct dev *ndev;
	struct opt *o;
	struct slot *s;
	int i;

	/* not opened */
	if (odev->pstate == DEV_CFG)
		return odev;

	ndev = odev;
	while (1) {
		/* try next one, circulating through the list */
		ndev = ndev->alt_next;
		if (ndev == odev) {
			if (log_level >= 1) {
				dev_log(odev);
				log_puts(": no fall-back device found\n");
			}
			return NULL;
		}


		if (!dev_ref(ndev))
			continue;

		/* check if new parameters are compatible with old ones */
		if (!dev_iscompat(odev, ndev)) {
			dev_unref(ndev);
			continue;
		}

		/* found it!*/
		break;
	}

	if (log_level >= 1) {
		dev_log(odev);
		log_puts(": switching to ");
		dev_log(ndev);
		log_puts("\n");
	}

	if (mtc_array[0].dev == odev)
		mtc_setdev(&mtc_array[0], ndev);

	/* move opts to new device (also moves clients using the opts) */
	for (o = opt_list; o != NULL; o = o->next) {
		if (o->dev != odev)
			continue;
		if (strcmp(o->name, o->dev->name) == 0)
			continue;
		opt_setdev(o, ndev);
	}

	/* terminate remaining clients */
	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->opt == NULL || s->opt->dev != odev)
			continue;
		if (s->ops != NULL) {
			s->ops->exit(s);
			s->ops = NULL;
		}
	}

	/* slots and/or MMC hold refs, drop ours */
	dev_unref(ndev);

	return ndev;
}

/*
 * check that all clients controlled by MMC are ready to start, if so,
 * attach them all at the same position
 */
void
mtc_trigger(struct mtc *mtc)
{
	int i;
	struct slot *s;

	if (mtc->tstate != MTC_START) {
		if (log_level >= 2) {
			dev_log(mtc->dev);
			log_puts(": not started by mmc yet, waiting...\n");
		}
		return;
	}

	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->opt == NULL || s->opt->mtc != mtc)
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
	if (!dev_ref(mtc->dev))
		return;

	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->opt == NULL || s->opt->mtc != mtc)
			continue;
		slot_attach(s);
		s->pstate = SLOT_RUN;
	}
	mtc->tstate = MTC_RUN;
	mtc_midi_full(mtc);
	dev_wakeup(mtc->dev);
}

/*
 * start all slots simultaneously
 */
void
mtc_start(struct mtc *mtc)
{
	if (mtc->tstate == MTC_STOP) {
		mtc->tstate = MTC_START;
		mtc_trigger(mtc);
#ifdef DEBUG
	} else {
		if (log_level >= 3) {
			dev_log(mtc->dev);
			log_puts(": ignoring mmc start\n");
		}
#endif
	}
}

/*
 * stop all slots simultaneously
 */
void
mtc_stop(struct mtc *mtc)
{
	switch (mtc->tstate) {
	case MTC_START:
		mtc->tstate = MTC_STOP;
		return;
	case MTC_RUN:
		mtc->tstate = MTC_STOP;
		dev_unref(mtc->dev);
		break;
	default:
#ifdef DEBUG
		if (log_level >= 3) {
			dev_log(mtc->dev);
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
mtc_loc(struct mtc *mtc, unsigned int origin)
{
	if (log_level >= 2) {
		dev_log(mtc->dev);
		log_puts(": relocated to ");
		log_putu(origin);
		log_puts("\n");
	}
	if (mtc->tstate == MTC_RUN)
		mtc_stop(mtc);
	mtc->origin = origin;
	if (mtc->tstate == MTC_RUN)
		mtc_start(mtc);
}

/*
 * set MMC device
 */
void
mtc_setdev(struct mtc *mtc, struct dev *d)
{
	struct opt *o;

	if (mtc->dev == d)
		return;

	if (log_level >= 2) {
		dev_log(d);
		log_puts(": set to be MIDI clock source\n");
	}

	/* adjust clock and ref counter, if needed */
	if (mtc->tstate == MTC_RUN) {
		mtc->delta -= mtc->dev->delta;
		dev_unref(mtc->dev);
	}

	mtc->dev = d;

	if (mtc->tstate == MTC_RUN) {
		mtc->delta += mtc->dev->delta;
		dev_ref(mtc->dev);
		dev_wakeup(mtc->dev);
	}

	/* move in once anything using MMC */
	for (o = opt_list; o != NULL; o = o->next) {
		if (o->mtc == mtc)
			opt_setdev(o, mtc->dev);
	}
}

/*
 * allocate buffers & conversion chain
 */
void
slot_initconv(struct slot *s)
{
	unsigned int dev_nch;
	struct dev *d = s->opt->dev;

	if (s->mode & MODE_PLAY) {
		cmap_init(&s->mix.cmap,
		    s->opt->pmin, s->opt->pmin + s->mix.nch - 1,
		    s->opt->pmin, s->opt->pmin + s->mix.nch - 1,
		    0, d->pchan - 1,
		    s->opt->pmin, s->opt->pmax);
		s->mix.decbuf = NULL;
		s->mix.resampbuf = NULL;
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
		s->mix.join = 1;
		s->mix.expand = 1;
		if (s->opt->dup && s->mix.cmap.nch > 0) {
			dev_nch = d->pchan < (s->opt->pmax + 1) ?
			    d->pchan - s->opt->pmin :
			    s->opt->pmax - s->opt->pmin + 1;
			if (dev_nch > s->mix.nch)
				s->mix.expand = dev_nch / s->mix.nch;
			else if (s->mix.nch > dev_nch)
				s->mix.join = s->mix.nch / dev_nch;
		}
	}

	if (s->mode & MODE_RECMASK) {
		unsigned int outchan = (s->opt->mode & MODE_MON) ?
		    d->pchan : d->rchan;

		s->sub.encbuf = NULL;
		s->sub.resampbuf = NULL;
		cmap_init(&s->sub.cmap,
		    0, outchan - 1,
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
		s->sub.join = 1;
		s->sub.expand = 1;
		if (s->opt->dup && s->sub.cmap.nch > 0) {
			dev_nch = outchan < (s->opt->rmax + 1) ?
			    outchan - s->opt->rmin :
			    s->opt->rmax - s->opt->rmin + 1;
			if (dev_nch > s->sub.nch)
				s->sub.join = dev_nch / s->sub.nch;
			else if (s->sub.nch > dev_nch)
				s->sub.expand = s->sub.nch / dev_nch;
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
}

/*
 * allocate buffers & conversion chain
 */
void
slot_allocbufs(struct slot *s)
{
	if (s->mode & MODE_PLAY) {
		s->mix.bpf = s->par.bps * s->mix.nch;
		abuf_init(&s->mix.buf, s->appbufsz * s->mix.bpf);
	}

	if (s->mode & MODE_RECMASK) {
		s->sub.bpf = s->par.bps * s->sub.nch;
		abuf_init(&s->sub.buf, s->appbufsz * s->sub.bpf);
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
	}

	if (s->mode & MODE_PLAY) {
		abuf_done(&s->mix.buf);
	}
}

/*
 * allocate a new slot and register the given call-backs
 */
struct slot *
slot_new(struct opt *opt, unsigned int id, char *who,
    struct slotops *ops, void *arg, int mode)
{
	char *p;
	char name[SLOT_NAMEMAX];
	char ctl_name[CTL_NAMEMAX];
	unsigned int i, ser, bestser, bestidx;
	struct slot *unit[DEV_NSLOT];
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
	 * build a unit-to-slot map for this name
	 */
	for (i = 0; i < DEV_NSLOT; i++)
		unit[i] = NULL;
	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (strcmp(s->name, name) == 0)
			unit[s->unit] = s;
	}

	/*
	 * find the free slot with the least unit number and same id
	 */
	for (i = 0; i < DEV_NSLOT; i++) {
		s = unit[i];
		if (s != NULL && s->ops == NULL && s->id == id)
			goto found;
	}

	/*
	 * find the free slot with the least unit number
	 */
	for (i = 0; i < DEV_NSLOT; i++) {
		s = unit[i];
		if (s != NULL && s->ops == NULL) {
			s->id = id;
			goto found;
		}
	}

	/*
	 * couldn't find a matching slot, pick oldest free slot
	 * and set its name/unit
	 */
	bestser = 0;
	bestidx = DEV_NSLOT;
	for (i = 0, s = slot_array; i < DEV_NSLOT; i++, s++) {
		if (s->ops != NULL)
			continue;
		ser = slot_serial - s->serial;
		if (ser > bestser) {
			bestser = ser;
			bestidx = i;
		}
	}

	if (bestidx == DEV_NSLOT) {
		if (log_level >= 1) {
			log_puts(name);
			log_puts(": out of sub-device slots\n");
		}
		return NULL;
	}

	s = slot_array + bestidx;
	ctl_del(CTL_SLOT_LEVEL, s, NULL);
	s->vol = MIDI_MAXCTL;
	strlcpy(s->name, name, SLOT_NAMEMAX);
	s->serial = slot_serial++;
	for (i = 0; unit[i] != NULL; i++)
		; /* nothing */
	s->unit = i;
	s->id = id;
	s->opt = opt;
	slot_ctlname(s, ctl_name, CTL_NAMEMAX);
	ctl_new(CTL_SLOT_LEVEL, s, NULL,
	    CTL_NUM, "app", ctl_name, -1, "level",
	    NULL, -1, 127, s->vol);

found:
	/* open device, this may change opt's device */
	if (!opt_ref(opt))
		return NULL;
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
	s->xrun = s->opt->mtc != NULL ? XRUN_SYNC : XRUN_IGNORE;
	s->appbufsz = s->opt->dev->bufsz;
	s->round = s->opt->dev->round;
	s->rate = s->opt->dev->rate;
	dev_midi_slotdesc(s->opt->dev, s);
	dev_midi_vol(s->opt->dev, s);
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": using ");
		log_puts(s->opt->name);
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
	case SLOT_STOP:
		slot_stop(s, 0);
		break;
	}
	opt_unref(s->opt);
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
 * set device for this slot
 */
void
slot_setopt(struct slot *s, struct opt *o)
{
	struct opt *t;
	struct dev *odev, *ndev;
	struct ctl *c;

	if (s->opt == NULL || s->opt == o)
		return;

	if (log_level >= 2) {
		slot_log(s);
		log_puts(": moving to opt ");
		log_puts(o->name);
		log_puts("\n");
	}

	odev = s->opt->dev;
	if (s->ops != NULL) {
		ndev = opt_ref(o);
		if (ndev == NULL)
			return;

		if (!dev_iscompat(odev, ndev)) {
			opt_unref(o);
			return;
		}
	}

	if (s->pstate == SLOT_RUN || s->pstate == SLOT_STOP)
		slot_detach(s);

	t = s->opt;
	s->opt = o;

	c = ctl_find(CTL_SLOT_LEVEL, s, NULL);
	ctl_update(c);

	if (o->dev != t->dev) {
		dev_midi_slotdesc(odev, s);
		dev_midi_slotdesc(ndev, s);
		dev_midi_vol(ndev, s);
	}

	if (s->pstate == SLOT_RUN || s->pstate == SLOT_STOP)
		slot_attach(s);

	if (s->ops != NULL) {
		opt_unref(t);
		return;
	}
}

/*
 * attach the slot to the device (ie start playing & recording
 */
void
slot_attach(struct slot *s)
{
	struct dev *d = s->opt->dev;
	long long pos;

	if (((s->mode & MODE_PLAY) && !(s->opt->mode & MODE_PLAY)) ||
	    ((s->mode & MODE_RECMASK) && !(s->opt->mode & MODE_RECMASK))) {
		if (log_level >= 1) {
			slot_log(s);
			log_puts(" at ");
			log_puts(s->opt->name);
			log_puts(": mode not allowed on this sub-device\n");
		}
	}

	/*
	 * setup converions layer
	 */
	slot_initconv(s);

	/*
	 * start the device if not started
	 */
	dev_wakeup(d);

	/*
	 * adjust initial clock
	 */
	pos = s->delta_rem +
	    (long long)s->delta * d->round +
	    (long long)d->delta * s->round;
	s->delta = pos / (int)d->round;
	s->delta_rem = pos % d->round;
	if (s->delta_rem < 0) {
		s->delta_rem += d->round;
		s->delta--;
	}

#ifdef DEBUG
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": attached at ");
		log_puti(s->delta);
		log_puts(" + ");
		log_puti(s->delta_rem);
		log_puts("/");
		log_puti(s->round);
		log_puts("\n");
	}
#endif

	/*
	 * We dont check whether the device is dying,
	 * because dev_xxx() functions are supposed to
	 * work (i.e., not to crash)
	 */

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
	if (s->opt->dev->pstate == DEV_CFG)
		return;
	if (s->opt->mtc == NULL) {
		slot_attach(s);
		s->pstate = SLOT_RUN;
	} else
		mtc_trigger(s->opt->mtc);
}

/*
 * setup buffers & conversion layers, prepare the slot to receive data
 * (for playback) or start (recording).
 */
void
slot_start(struct slot *s)
{
	struct dev *d = s->opt->dev;
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
			aparams_log(&d->par);
			log_puts("\n");
		}
	}
	if (s->mode & MODE_RECMASK) {
		if (log_level >= 3) {
			slot_log(s);
			log_puts(": recording ");
			aparams_log(&s->par);
			log_puts(" <- ");
			aparams_log(&d->par);
			log_puts("\n");
		}
	}
#endif
	slot_allocbufs(s);

	if (s->mode & MODE_RECMASK) {
		/*
		 * N-th recorded block is the N-th played block
		 */
		s->sub.prime = d->bufsz / d->round;
	}
	s->skip = 0;

	/*
	 * get the current position, the origin is when the first sample
	 * played and/or recorded
	 */
	s->delta = -(long long)d->bufsz * s->round / d->round;
	s->delta_rem = 0;

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
	struct dev *d = s->opt->dev;
	long long pos;

	for (ps = &d->slot_list; *ps != s; ps = &(*ps)->next) {
#ifdef DEBUG
		if (*ps == NULL) {
			slot_log(s);
			log_puts(": can't detach, not on list\n");
			panic();
		}
#endif
	}
	*ps = s->next;

	/*
	 * adjust clock, go back d->delta ticks so that slot_attach()
	 * could be called with the resulting state
	 */
	pos = s->delta_rem +
	    (long long)s->delta * d->round -
	    (long long)d->delta * s->round;
	s->delta = pos / (int)d->round;
	s->delta_rem = pos % d->round;
	if (s->delta_rem < 0) {
		s->delta_rem += d->round;
		s->delta--;
	}

#ifdef DEBUG
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": detached at ");
		log_puti(s->delta);
		log_puts(" + ");
		log_puti(s->delta_rem);
		log_puts("/");
		log_puti(d->round);
		log_puts("\n");
	}
#endif
	if (s->mode & MODE_PLAY)
		dev_mix_adjvol(d);

	if (s->mode & MODE_RECMASK) {
		if (s->sub.encbuf) {
			xfree(s->sub.encbuf);
			s->sub.encbuf = NULL;
		}
		if (s->sub.resampbuf) {
			xfree(s->sub.resampbuf);
			s->sub.resampbuf = NULL;
		}
	}

	if (s->mode & MODE_PLAY) {
		if (s->mix.decbuf) {
			xfree(s->mix.decbuf);
			s->mix.decbuf = NULL;
		}
		if (s->mix.resampbuf) {
			xfree(s->mix.resampbuf);
			s->mix.resampbuf = NULL;
		}
	}
}

/*
 * put the slot in stopping state (draining play buffers) or
 * stop & detach if no data to drain.
 */
void
slot_stop(struct slot *s, int drain)
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
		if ((s->mode & MODE_PLAY) && drain) {
			/*
			 * Don't detach, dev_cycle() will do it for us
			 * when the buffer is drained.
			 */
			s->pstate = SLOT_STOP;
			return;
		}
		slot_detach(s);
	} else if (s->pstate == SLOT_STOP) {
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

/*
 * allocate at control slot
 */
struct ctlslot *
ctlslot_new(struct opt *o, struct ctlops *ops, void *arg)
{
	struct ctlslot *s;
	struct ctl *c;
	int i;

	i = 0;
	for (;;) {
		if (i == DEV_NCTLSLOT)
			return NULL;
		s = ctlslot_array + i;
		if (s->ops == NULL)
			break;
		i++;
	}
	s->opt = o;
	s->self = 1 << i;
	if (!opt_ref(o))
		return NULL;
	s->ops = ops;
	s->arg = arg;
	for (c = ctl_list; c != NULL; c = c->next) {
		if (!ctlslot_visible(s, c))
			continue;
		c->refs_mask |= s->self;
	}
	return s;
}

/*
 * free control slot
 */
void
ctlslot_del(struct ctlslot *s)
{
	struct ctl *c, **pc;

	pc = &ctl_list;
	while ((c = *pc) != NULL) {
		c->refs_mask &= ~s->self;
		if (c->refs_mask == 0) {
			*pc = c->next;
			xfree(c);
		} else
			pc = &c->next;
	}
	s->ops = NULL;
	opt_unref(s->opt);
}

int
ctlslot_visible(struct ctlslot *s, struct ctl *c)
{
	if (s->opt == NULL)
		return 1;
	switch (c->scope) {
	case CTL_HW:
	case CTL_DEV_MASTER:
		return (s->opt->dev == c->u.any.arg0);
	case CTL_OPT_DEV:
		return (s->opt == c->u.any.arg0);
	case CTL_SLOT_LEVEL:
		return (s->opt->dev == c->u.slot_level.slot->opt->dev);
	default:
		return 0;
	}
}

struct ctl *
ctlslot_lookup(struct ctlslot *s, int addr)
{
	struct ctl *c;

	c = ctl_list;
	while (1) {
		if (c == NULL)
			return NULL;
		if (c->type != CTL_NONE && c->addr == addr)
			break;
		c = c->next;
	}
	if (!ctlslot_visible(s, c))
		return NULL;
	return c;
}

void
ctlslot_update(struct ctlslot *s)
{
	struct ctl *c;
	unsigned int refs_mask;

	for (c = ctl_list; c != NULL; c = c->next) {
		if (c->type == CTL_NONE)
			continue;
		refs_mask = ctlslot_visible(s, c) ? s->self : 0;

		/* nothing to do if no visibility change */
		if (((c->refs_mask & s->self) ^ refs_mask) == 0)
			continue;
		/* if control becomes visble */
		if (refs_mask)
			c->refs_mask |= s->self;
		/* if control is hidden */
		c->desc_mask |= s->self;
	}
}

void
ctl_node_log(struct ctl_node *c)
{
	log_puts(c->name);
	if (c->unit >= 0)
		log_putu(c->unit);
}

void
ctl_log(struct ctl *c)
{
	if (c->group[0] != 0) {
		log_puts(c->group);
		log_puts("/");
	}
	ctl_node_log(&c->node0);
	log_puts(".");
	log_puts(c->func);
	log_puts("=");
	switch (c->type) {
	case CTL_NONE:
		log_puts("none");
		break;
	case CTL_NUM:
	case CTL_SW:
		log_putu(c->curval);
		break;
	case CTL_VEC:
	case CTL_LIST:
	case CTL_SEL:
		ctl_node_log(&c->node1);
		log_puts(":");
		log_putu(c->curval);
	}
	log_puts(" at ");
	log_putu(c->addr);
	log_puts(" -> ");
	switch (c->scope) {
	case CTL_HW:
		log_puts("hw:");
		log_puts(c->u.hw.dev->name);
		log_puts("/");
		log_putu(c->u.hw.addr);
		break;
	case CTL_DEV_MASTER:
		log_puts("dev_master:");
		log_puts(c->u.dev_master.dev->name);
		break;
	case CTL_SLOT_LEVEL:
		log_puts("slot_level:");
		log_puts(c->u.slot_level.slot->name);
		log_putu(c->u.slot_level.slot->unit);
		break;
	case CTL_OPT_DEV:
		log_puts("opt_dev:");
		log_puts(c->u.opt_dev.opt->name);
		log_puts("/");
		log_puts(c->u.opt_dev.dev->name);
		break;
	default:
		log_puts("unknown");
	}
}

int
ctl_setval(struct ctl *c, int val)
{
	if (c->curval == val) {
		if (log_level >= 3) {
			ctl_log(c);
			log_puts(": already set\n");
		}
		return 1;
	}
	if (val < 0 || val > c->maxval) {
		if (log_level >= 3) {
			log_putu(val);
			log_puts(": ctl val out of bounds\n");
		}
		return 0;
	}

	switch (c->scope) {
	case CTL_HW:
		if (log_level >= 3) {
			ctl_log(c);
			log_puts(": marked as dirty\n");
		}
		c->curval = val;
		c->dirty = 1;
		return dev_ref(c->u.hw.dev);
	case CTL_DEV_MASTER:
		if (!c->u.dev_master.dev->master_enabled)
			return 1;
		dev_master(c->u.dev_master.dev, val);
		dev_midi_master(c->u.dev_master.dev);
		c->val_mask = ~0U;
		c->curval = val;
		return 1;
	case CTL_SLOT_LEVEL:
		slot_setvol(c->u.slot_level.slot, val);
		// XXX change dev_midi_vol() into slot_midi_vol()
		dev_midi_vol(c->u.slot_level.slot->opt->dev, c->u.slot_level.slot);
		c->val_mask = ~0U;
		c->curval = val;
		return 1;
	case CTL_OPT_DEV:
		c->u.opt_dev.opt->alt_first = c->u.opt_dev.dev;
		opt_setdev(c->u.opt_dev.opt, c->u.opt_dev.dev);
		return 1;
	default:
		if (log_level >= 2) {
			ctl_log(c);
			log_puts(": not writable\n");
		}
		return 1;
	}
}

/*
 * add a ctl
 */
struct ctl *
ctl_new(int scope, void *arg0, void *arg1,
    int type, char *gstr,
    char *str0, int unit0, char *func,
    char *str1, int unit1, int maxval, int val)
{
	struct ctl *c, **pc;
	struct ctlslot *s;
	int addr;
	int i;

	/*
	 * find the smallest unused addr number and
	 * the last position in the list
	 */
	addr = 0;
	for (pc = &ctl_list; (c = *pc) != NULL; pc = &c->next) {
		if (c->addr > addr)
			addr = c->addr;
	}
	addr++;

	c = xmalloc(sizeof(struct ctl));
	c->type = type;
	strlcpy(c->func, func, CTL_NAMEMAX);
	strlcpy(c->group, gstr, CTL_NAMEMAX);
	strlcpy(c->node0.name, str0, CTL_NAMEMAX);
	c->node0.unit = unit0;
	if (c->type == CTL_VEC || c->type == CTL_LIST || c->type == CTL_SEL) {
		strlcpy(c->node1.name, str1, CTL_NAMEMAX);
		c->node1.unit = unit1;
	} else
		memset(&c->node1, 0, sizeof(struct ctl_node));
	c->scope = scope;
	c->u.any.arg0 = arg0;
	switch (scope) {
	case CTL_HW:
		c->u.hw.addr = *(unsigned int *)arg1;
		break;
	case CTL_OPT_DEV:
		c->u.any.arg1 = arg1;
		break;
	default:
		c->u.any.arg1 = NULL;
	}
	c->addr = addr;
	c->maxval = maxval;
	c->val_mask = ~0;
	c->desc_mask = ~0;
	c->curval = val;
	c->dirty = 0;
	c->refs_mask = CTL_DEVMASK;
	for (s = ctlslot_array, i = 0; i < DEV_NCTLSLOT; i++, s++) {
		if (s->ops == NULL)
			continue;
		if (ctlslot_visible(s, c))
			c->refs_mask |= 1 << i;
	}
	c->next = *pc;
	*pc = c;
#ifdef DEBUG
	if (log_level >= 2) {
		ctl_log(c);
		log_puts(": added\n");
	}
#endif
	return c;
}

void
ctl_update(struct ctl *c)
{
	struct ctlslot *s;
	unsigned int refs_mask;
	int i;

	for (s = ctlslot_array, i = 0; i < DEV_NCTLSLOT; i++, s++) {
		if (s->ops == NULL)
			continue;
		refs_mask = ctlslot_visible(s, c) ? s->self : 0;

		/* nothing to do if no visibility change */
		if (((c->refs_mask & s->self) ^ refs_mask) == 0)
			continue;
		/* if control becomes visble */
		if (refs_mask)
			c->refs_mask |= s->self;
		/* if control is hidden */
		c->desc_mask |= s->self;
	}
}

int
ctl_match(struct ctl *c, int scope, void *arg0, void *arg1)
{
	if (c->type == CTL_NONE || c->scope != scope || c->u.any.arg0 != arg0)
		return 0;
	if (arg0 != NULL && c->u.any.arg0 != arg0)
		return 0;
	switch (scope) {
	case CTL_HW:
		if (arg1 != NULL && c->u.hw.addr != *(unsigned int *)arg1)
			return 0;
		break;
	case CTL_OPT_DEV:
		if (arg1 != NULL && c->u.any.arg1 != arg1)
			return 0;
		break;
	}
	return 1;
}

struct ctl *
ctl_find(int scope, void *arg0, void *arg1)
{
	struct ctl *c;

	for (c = ctl_list; c != NULL; c = c->next) {
		if (ctl_match(c, scope, arg0, arg1))
			return c;
	}
	return NULL;
}

int
ctl_onval(int scope, void *arg0, void *arg1, int val)
{
	struct ctl *c;

	c = ctl_find(scope, arg0, arg1);
	if (c == NULL)
		return 0;
	c->curval = val;
	c->val_mask = ~0U;
	return 1;
}

void
ctl_del(int scope, void *arg0, void *arg1)
{
	struct ctl *c, **pc;

	pc = &ctl_list;
	for (;;) {
		c = *pc;
		if (c == NULL)
			return;
		if (ctl_match(c, scope, arg0, arg1)) {
#ifdef DEBUG
			if (log_level >= 2) {
				ctl_log(c);
				log_puts(": removed\n");
			}
#endif
			c->refs_mask &= ~CTL_DEVMASK;
			if (c->refs_mask == 0) {
				*pc = c->next;
				xfree(c);
				continue;
			}
			c->type = CTL_NONE;
			c->desc_mask = ~0;
		}
		pc = &c->next;
	}
}

void
dev_ctlsync(struct dev *d)
{
	struct ctl *c;
	struct ctlslot *s;
	int found, i;

	found = 0;
	for (c = ctl_list; c != NULL; c = c->next) {
		if (c->scope == CTL_HW &&
		    c->u.hw.dev == d &&
		    c->type == CTL_NUM &&
		    strcmp(c->group, d->name) == 0 &&
		    strcmp(c->node0.name, "output") == 0 &&
		    strcmp(c->func, "level") == 0)
			found = 1;
	}

	if (d->master_enabled && found) {
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": software master level control disabled\n");
		}
		d->master_enabled = 0;
		ctl_del(CTL_DEV_MASTER, d, NULL);
	} else if (!d->master_enabled && !found) {
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": software master level control enabled\n");
		}
		d->master_enabled = 1;
		ctl_new(CTL_DEV_MASTER, d, NULL,
		    CTL_NUM, d->name, "output", -1, "level",
		    NULL, -1, 127, d->master);
	}

	for (s = ctlslot_array, i = 0; i < DEV_NCTLSLOT; i++, s++) {
		if (s->ops == NULL)
			continue;
		if (s->opt->dev == d)
			s->ops->sync(s->arg);
	}
}
