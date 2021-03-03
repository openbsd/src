/*	$OpenBSD: opt.c,v 1.8 2021/03/03 10:19:06 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2011 Alexandre Ratchov <alex@caoua.org>
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
#include <string.h>

#include "dev.h"
#include "midi.h"
#include "opt.h"
#include "sysex.h"
#include "utils.h"

struct opt *opt_list;

void opt_midi_imsg(void *, unsigned char *, int);
void opt_midi_omsg(void *, unsigned char *, int);
void opt_midi_fill(void *, int);
void opt_midi_exit(void *);

struct midiops opt_midiops = {
	opt_midi_imsg,
	opt_midi_omsg,
	opt_midi_fill,
	opt_midi_exit
};

void
opt_midi_imsg(void *arg, unsigned char *msg, int len)
{
#ifdef DEBUG
	struct opt *o = arg;

	log_puts(o->name);
	log_puts(": can't receive midi messages\n");
	panic();
#endif
}

void
opt_midi_omsg(void *arg, unsigned char *msg, int len)
{
	struct opt *o = arg;
	struct sysex *x;
	unsigned int fps, chan;

	if ((msg[0] & MIDI_CMDMASK) == MIDI_CTL && msg[1] == MIDI_CTL_VOL) {
		chan = msg[0] & MIDI_CHANMASK;
		if (chan >= DEV_NSLOT)
			return;
		if (slot_array[chan].opt == NULL ||
		    slot_array[chan].opt->dev != o->dev)
			return;
		slot_setvol(slot_array + chan, msg[2]);
		ctl_onval(CTL_SLOT_LEVEL, slot_array + chan, NULL, msg[2]);
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
			if (len == SYSEX_SIZE(master)) {
				dev_master(o->dev, x->u.master.coarse);
				if (o->dev->master_enabled) {
					ctl_onval(CTL_DEV_MASTER, o->dev, NULL,
					   x->u.master.coarse);
				}
			}
			return;
		}
		if (x->id0 != SYSEX_MMC)
			return;
		switch (x->id1) {
		case SYSEX_MMC_STOP:
			if (len != SYSEX_SIZE(stop))
				return;
			if (o->mtc == NULL)
				return;
			if (log_level >= 2) {
				log_puts(o->name);
				log_puts(": mmc stop\n");
			}
			mtc_stop(o->mtc);
			break;
		case SYSEX_MMC_START:
			if (len != SYSEX_SIZE(start))
				return;
			if (o->mtc == NULL)
				return;
			if (log_level >= 2) {
				log_puts(o->name);
				log_puts(": mmc start\n");
			}
			mtc_start(o->mtc);
			break;
		case SYSEX_MMC_LOC:
			if (len != SYSEX_SIZE(loc) ||
			    x->u.loc.len != SYSEX_MMC_LOC_LEN ||
			    x->u.loc.cmd != SYSEX_MMC_LOC_CMD)
				return;
			if (o->mtc == NULL)
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
				mtc_stop(o->mtc);
				return;
			}
			mtc_loc(o->mtc,
			    (x->u.loc.hr & 0x1f) * 3600 * MTC_SEC +
			     x->u.loc.min * 60 * MTC_SEC +
			     x->u.loc.sec * MTC_SEC +
			     x->u.loc.fr * (MTC_SEC / fps));
			break;
		}
		break;
	case SYSEX_TYPE_EDU:
		if (x->id0 != SYSEX_AUCAT || x->id1 != SYSEX_AUCAT_DUMPREQ)
			return;
		if (len != SYSEX_SIZE(dumpreq))
			return;
		dev_midi_dump(o->dev);
		break;
	}
}

void
opt_midi_fill(void *arg, int count)
{
	/* nothing to do */
}

void
opt_midi_exit(void *arg)
{
	struct opt *o = arg;

	if (log_level >= 1) {
		log_puts(o->name);
		log_puts(": midi end point died\n");
		panic();
	}
}

/*
 * create a new audio sub-device "configuration"
 */
struct opt *
opt_new(struct dev *d, char *name,
    int pmin, int pmax, int rmin, int rmax,
    int maxweight, int mmc, int dup, unsigned int mode)
{
	struct opt *o, **po;
	unsigned int len, num;
	char c;

	for (len = 0; name[len] != '\0'; len++) {
		if (len == OPT_NAMEMAX) {
			log_puts(name);
			log_puts(": too long\n");
			return NULL;
		}
		c = name[len];
		if ((c < 'a' || c > 'z') &&
		    (c < 'A' || c > 'Z')) {
			log_puts(name);
			log_puts(": only alphabetic chars allowed\n");
			return NULL;
		}
	}
	num = 0;
	for (po = &opt_list; *po != NULL; po = &(*po)->next)
		num++;
	if (num >= OPT_NMAX) {
		log_puts(name);
		log_puts(": too many opts\n");
		return NULL;
	}
	if (opt_byname(d, name)) {
		dev_log(d);
		log_puts(".");
		log_puts(name);
		log_puts(": already defined\n");
		return NULL;
	}

	if (mmc) {
		if (mtc_array[0].dev != NULL && mtc_array[0].dev != d) {
			log_puts(name);
			log_puts(": MTC already setup for another device\n");
			return NULL;
		}
		mtc_array[0].dev = d;
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": initial MTC source, controlled by MMC\n");
		}
	}

	o = xmalloc(sizeof(struct opt));
	o->num = num;
	o->dev = d;

	/*
	 * XXX: below, we allocate a midi input buffer, since we don't
	 *	receive raw midi data, so no need to allocate a input
	 *	ibuf.  Possibly set imsg & fill callbacks to NULL and
	 *	use this to in midi_new() to check if buffers need to be
	 *	allocated
	 */
	o->midi = midi_new(&opt_midiops, o, MODE_MIDIIN | MODE_MIDIOUT);
	midi_tag(o->midi, o->num);

	if (mode & MODE_PLAY) {
		o->pmin = pmin;
		o->pmax = pmax;
	}
	if (mode & MODE_RECMASK) {
		o->rmin = rmin;
		o->rmax = rmax;
	}
	o->maxweight = maxweight;
	o->mtc = mmc ? &mtc_array[0] : NULL;
	o->dup = dup;
	o->mode = mode;
	memcpy(o->name, name, len + 1);
	o->next = *po;
	*po = o;
	if (log_level >= 2) {
		dev_log(d);
		log_puts(".");
		log_puts(o->name);
		log_puts(":");
		if (o->mode & MODE_REC) {
			log_puts(" rec=");
			log_putu(o->rmin);
			log_puts(":");
			log_putu(o->rmax);
		}
		if (o->mode & MODE_PLAY) {
			log_puts(" play=");
			log_putu(o->pmin);
			log_puts(":");
			log_putu(o->pmax);
			log_puts(" vol=");
			log_putu(o->maxweight);
		}
		if (o->mode & MODE_MON) {
			log_puts(" mon=");
			log_putu(o->rmin);
			log_puts(":");
			log_putu(o->rmax);
		}
		if (o->mode & (MODE_RECMASK | MODE_PLAY)) {
			if (o->mtc)
				log_puts(" mtc");
			if (o->dup)
				log_puts(" dup");
		}
		log_puts("\n");
	}
	return o;
}

struct opt *
opt_byname(struct dev *d, char *name)
{
	struct opt *o;

	for (o = opt_list; o != NULL; o = o->next) {
		if (d != NULL && o->dev != d)
			continue;
		if (strcmp(name, o->name) == 0)
			return o;
	}
	return NULL;
}

void
opt_del(struct opt *o)
{
	struct opt **po;

	for (po = &opt_list; *po != o; po = &(*po)->next) {
#ifdef DEBUG
		if (*po == NULL) {
			log_puts("opt_del: not on list\n");
			panic();
		}
#endif
	}
	midi_del(o->midi);
	*po = o->next;
	xfree(o);
}
