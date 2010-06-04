/*	$OpenBSD: midi.c,v 1.25 2010/06/04 06:15:28 ratchov Exp $	*/
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
 * TODO
 *
 * use shadow variables (to save NRPNs, LSB of controller) 
 * in the midi merger
 *
 * make output and input identical when only one
 * input is used (fix running status)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "midi.h"
#ifdef DEBUG
#include "dbg.h"
#endif

/*
 * input data rate is XFER / TIMO (in bytes per microsecond),
 * it must be slightly larger than the MIDI standard 3125 bytes/s
 */ 
#define MIDITHRU_XFER 340
#define MIDITHRU_TIMO 100000

/*
 * masks to extract command and channel of status byte
 */
#define MIDI_CMDMASK	0xf0
#define MIDI_CHANMASK	0x0f

/*
 * MIDI status bytes of voice messages
 */
#define MIDI_NOFF	0x80		/* note off */
#define MIDI_NON	0x90		/* note on */
#define MIDI_KAT	0xa0		/* key after touch */
#define MIDI_CTL	0xb0		/* controller */
#define MIDI_PC		0xc0		/* program change */
#define MIDI_CAT	0xd0		/* channel after touch */
#define MIDI_BEND	0xe0		/* pitch bend */
#define MIDI_ACK	0xfe		/* active sensing message */

/*
 * MIDI controller numbers
 */
#define MIDI_CTLVOL	7		/* volume */
#define MIDI_CTLPAN	11		/* pan */

/*
 * length of voice and common messages (status byte included)
 */
unsigned voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
unsigned common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

/*
 * send the message stored in of ibuf->r.midi.msg to obuf
 */
void
thru_flush(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned ocount, itodo;
	unsigned char *odata, *idata;

	itodo = ibuf->r.midi.used;
	idata = ibuf->r.midi.msg;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": flushing ");
		dbg_putu(itodo);
		dbg_puts(" byte message\n");
	}
#endif
	while (itodo > 0) {
		if (!ABUF_WOK(obuf)) {
#ifdef DEBUG
			if (debug_level >= 4) {
				aproc_dbg(p);
				dbg_puts(": overrun, discarding ");
				dbg_putu(obuf->used);
				dbg_puts(" bytes\n");
			}
#endif
			abuf_rdiscard(obuf, obuf->used);
			if (p->u.thru.owner == ibuf)
				p->u.thru.owner = NULL;
			return;
		}
		odata = abuf_wgetblk(obuf, &ocount, 0);
		if (ocount > itodo)
			ocount = itodo;
		memcpy(odata, idata, ocount);
		abuf_wcommit(obuf, ocount);
		itodo -= ocount;
		idata += ocount;
	}
	ibuf->r.midi.used = 0;
	p->u.thru.owner = ibuf;
}

/*
 * send the real-time message (one byte) to obuf, similar to thrui_flush()
 */
void
thru_rt(struct aproc *p, struct abuf *ibuf, struct abuf *obuf, unsigned c)
{
	unsigned ocount;
	unsigned char *odata;

#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": ");
		dbg_putx(c);
		dbg_puts(": flushing realtime message\n");
	}
#endif
	if (c == MIDI_ACK)
		return;
	if (!ABUF_WOK(obuf)) {
#ifdef DEBUG
		if (debug_level >= 4) {
			aproc_dbg(p);
			dbg_puts(": overrun, discarding ");
			dbg_putu(obuf->used);
			dbg_puts(" bytes\n");
		}
#endif
		abuf_rdiscard(obuf, obuf->used);
		if (p->u.thru.owner == ibuf)
			p->u.thru.owner = NULL;
	}
	odata = abuf_wgetblk(obuf, &ocount, 0);
	odata[0] = c;
	abuf_wcommit(obuf, 1);
}

/*
 * parse ibuf contents and store each message into obuf,
 * use at most ``todo'' bytes (for throttling)
 */
void
thru_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf, unsigned todo)
{
	unsigned char *idata;
	unsigned c, icount, ioffs;

	idata = NULL;
	icount = ioffs = 0;
	for (;;) {
		if (icount == 0) {
			if (todo == 0)
				break;
			idata = abuf_rgetblk(ibuf, &icount, ioffs);
			if (icount > todo)
				icount = todo;
			if (icount == 0)
				break;
			todo -= icount;
			ioffs += icount;
		}
		c = *idata++;
		icount--;
		if (c < 0x80) {
			if (ibuf->r.midi.idx == 0 && ibuf->r.midi.st) {
				ibuf->r.midi.msg[ibuf->r.midi.used++] = ibuf->r.midi.st;
				ibuf->r.midi.idx++;
			}
			ibuf->r.midi.msg[ibuf->r.midi.used++] = c;
			ibuf->r.midi.idx++;
			if (ibuf->r.midi.idx == ibuf->r.midi.len) {
				thru_flush(p, ibuf, obuf);
				if (ibuf->r.midi.st >= 0xf0)
					ibuf->r.midi.st = 0;
				ibuf->r.midi.idx = 0;
			}
			if (ibuf->r.midi.used == MIDI_MSGMAX) {
				if (ibuf->r.midi.used == ibuf->r.midi.idx ||
				    p->u.thru.owner == ibuf)
					thru_flush(p, ibuf, obuf);
				else
					ibuf->r.midi.used = 0;
			}
		} else if (c < 0xf8) {
			if (ibuf->r.midi.used == ibuf->r.midi.idx ||
			    p->u.thru.owner == ibuf) {
				thru_flush(p, ibuf, obuf);
			} else
				ibuf->r.midi.used = 0;
			ibuf->r.midi.msg[0] = c;
			ibuf->r.midi.used = 1;
			ibuf->r.midi.len = (c >= 0xf0) ? 
			    common_len[c & 7] :
			    voice_len[(c >> 4) & 7];
			if (ibuf->r.midi.len == 1) {
				thru_flush(p, ibuf, obuf);
				ibuf->r.midi.idx = 0;
				ibuf->r.midi.st = 0;
				ibuf->r.midi.len = 0;
			} else { 
				ibuf->r.midi.st = c;
				ibuf->r.midi.idx = 1;
			}
		} else {
			thru_rt(p, ibuf, obuf, c);
		}
	}
}

int
thru_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext;
	unsigned todo;

	if (!ABUF_ROK(ibuf))
		return 0;
	if (ibuf->tickets == 0) {
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(ibuf);
			dbg_puts(": out of tickets, blocking\n");
		}
#endif
		return 0;
	}
	todo = ibuf->used;
	if (todo > ibuf->tickets)
		todo = ibuf->tickets;
	ibuf->tickets -= todo;
	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (ibuf->duplex == i)
			continue;
		thru_bcopy(p, ibuf, i, todo);
		(void)abuf_flush(i);
	}
	abuf_rdiscard(ibuf, todo);
	return 1;
}

int
thru_out(struct aproc *p, struct abuf *obuf)
{
	return 0;
}

void
thru_eof(struct aproc *p, struct abuf *ibuf)
{
	if (!(p->flags & APROC_QUIT))
		return;
	if (LIST_EMPTY(&p->ins))
		aproc_del(p);
}

void
thru_hup(struct aproc *p, struct abuf *obuf)
{
}

void
thru_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->r.midi.used = 0;
	ibuf->r.midi.len = 0;
	ibuf->r.midi.idx = 0;
	ibuf->r.midi.st = 0;
	ibuf->tickets = MIDITHRU_XFER;
}

void
thru_done(struct aproc *p)
{
	timo_del(&p->u.thru.timo);
}

struct aproc_ops thru_ops = {
	"thru",
	thru_in,
	thru_out,
	thru_eof,
	thru_hup,
	thru_newin,
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	thru_done
};

/*
 * call-back invoked periodically to implement throttling at each invocation
 * gain more ``tickets'' for processing.  If one of the buffer was blocked by
 * the throttelling mechanism, then run it
 */
void
thru_cb(void *addr)
{
	struct aproc *p = (struct aproc *)addr;
	struct abuf *i, *inext;
	unsigned tickets;

	timo_add(&p->u.thru.timo, MIDITHRU_TIMO);
	
	for (i = LIST_FIRST(&p->ins); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		tickets = i->tickets;
		i->tickets = MIDITHRU_XFER;
		if (tickets == 0)
			abuf_run(i);
	}
}

struct aproc *
thru_new(char *name)
{
	struct aproc *p;

	p = aproc_new(&thru_ops, name);
	p->u.thru.owner = NULL;
	timo_set(&p->u.thru.timo, thru_cb, p);
	timo_add(&p->u.thru.timo, MIDITHRU_TIMO);
	return p;
}

#ifdef DEBUG
void
ctl_slotdbg(struct aproc *p, int slot)
{
	struct ctl_slot *s;

	if (slot < 0) {
		dbg_puts("none");
	} else {
		s = p->u.ctl.slot + slot;
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
 * broadcast a message to all output buffers on the behalf of ibuf.
 * ie. don't sent back the message to the sender
 */
void
ctl_sendmsg(struct aproc *p, struct abuf *ibuf, unsigned char *msg, unsigned len)
{
	unsigned ocount, itodo;
	unsigned char *odata, *idata;
	struct abuf *i, *inext;

	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (i->duplex && i->duplex == ibuf)
			continue;
		itodo = len;
		idata = msg;
		while (itodo > 0) {
			if (!ABUF_WOK(i)) {
#ifdef DEBUG
				if (debug_level >= 4) {
					abuf_dbg(i);
					dbg_puts(": overrun, discarding ");
					dbg_putu(i->used);
					dbg_puts(" bytes\n");
				}
#endif
				abuf_rdiscard(i, i->used);
			}
			odata = abuf_wgetblk(i, &ocount, 0);
			if (ocount > itodo)
				ocount = itodo;
#ifdef DEBUG
			if (debug_level >= 4) {
				abuf_dbg(i);
				dbg_puts(": stored ");
				dbg_putu(ocount);
				dbg_puts(" bytes\n");
			}
#endif
			memcpy(odata, idata, ocount);
			abuf_wcommit(i, ocount);
			itodo -= ocount;
			idata += ocount;
		}
		(void)abuf_flush(i);
	}
}

/*
 * send a quarter frame MTC message
 */
void
ctl_qfr(struct aproc *p)
{
	unsigned char buf[2];
	unsigned data;

	switch (p->u.ctl.qfr) {
	case 0:
		data = p->u.ctl.fr & 0xf;
		break;
	case 1:
		data = p->u.ctl.fr >> 4;
		break;
	case 2:
		data = p->u.ctl.sec & 0xf;
		break;
	case 3:
		data = p->u.ctl.sec >> 4;
		break;
	case 4:
		data = p->u.ctl.min & 0xf;
		break;
	case 5:
		data = p->u.ctl.min >> 4;
		break;
	case 6:
		data = p->u.ctl.hr & 0xf;
		break;
	case 7:
		data = (p->u.ctl.hr >> 4) | (p->u.ctl.fps_id << 1);
		/*
		 * tick messages are sent 2 frames ahead
		 */
		p->u.ctl.fr += 2;
		if (p->u.ctl.fr < p->u.ctl.fps)
			break;
		p->u.ctl.fr -= p->u.ctl.fps;
		p->u.ctl.sec++;
		if (p->u.ctl.sec < 60)
			break;;
		p->u.ctl.sec = 0;
		p->u.ctl.min++;
		if (p->u.ctl.min < 60)
			break;
		p->u.ctl.min = 0;
		p->u.ctl.hr++;
		if (p->u.ctl.hr < 24)
			break;
		p->u.ctl.hr = 0;
		break;
	default:
		/* NOTREACHED */
		data = 0;
	}
	buf[0] = 0xf1;
	buf[1] = (p->u.ctl.qfr << 4) | data;
	p->u.ctl.qfr++;
	p->u.ctl.qfr &= 7;
	ctl_sendmsg(p, NULL, buf, 2);
}

/*
 * send a full frame MTC message
 */
void
ctl_full(struct aproc *p)
{
	unsigned char buf[10];
	unsigned origin = p->u.ctl.origin;
	unsigned fps = p->u.ctl.fps;

	p->u.ctl.hr =  (origin / (3600 * MTC_SEC)) % 24;
	p->u.ctl.min = (origin / (60 * MTC_SEC))   % 60;
	p->u.ctl.sec = (origin / MTC_SEC)          % 60;
	p->u.ctl.fr =  (origin / (MTC_SEC / fps))  % fps;

	buf[0] = 0xf0;
	buf[1] = 0x7f;
	buf[2] = 0x7f;
	buf[3] = 0x01;
	buf[4] = 0x01;
	buf[5] = p->u.ctl.hr | (p->u.ctl.fps_id << 5);
	buf[6] = p->u.ctl.min;
	buf[7] = p->u.ctl.sec;
	buf[8] = p->u.ctl.fr;
	buf[9] = 0xf7;
	p->u.ctl.qfr = 0;
	ctl_sendmsg(p, NULL, buf, 10);
}

/*
 * find the best matching free slot index (ie midi channel).
 * return -1, if there are no free slots anymore
 */
int
ctl_getidx(struct aproc *p, char *who)
{
	char *s;
	struct ctl_slot *slot;
	char name[CTL_NAMEMAX];
	unsigned i, unit, umap = 0;
	unsigned ser, bestser, bestidx;

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
	for (i = 0, slot = p->u.ctl.slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops == NULL)
			continue;
		if (strcmp(slot->name, name) == 0)
			umap |= (1 << i);
	} 
	for (unit = 0; ; unit++) {
		if (unit == CTL_NSLOT)
			return -1;
		if ((umap & (1 << unit)) == 0)
			break;
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new control name is ");
		dbg_puts(name);
		dbg_putu(unit);
		dbg_puts("\n");
	}
#endif
	/*
	 * find a free controller slot with the same name/unit
	 */
	for (i = 0, slot = p->u.ctl.slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops == NULL &&
		    strcmp(slot->name, name) == 0 &&
		    slot->unit == unit) {
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
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
	for (i = 0, slot = p->u.ctl.slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->ops != NULL)
			continue;
		ser = p->u.ctl.serial - slot->serial;
		if (ser > bestser) {
			bestser = ser;
			bestidx = i;
		}
	}
	if (bestidx == CTL_NSLOT)
		return -1;
	slot = p->u.ctl.slot + bestidx;
	if (slot->name[0] != '\0')
		slot->vol = MIDI_MAXCTL;
	strlcpy(slot->name, name, CTL_NAMEMAX);
	slot->serial = p->u.ctl.serial++;
	slot->unit = unit;
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": overwritten slot ");
		dbg_putu(bestidx);
		dbg_puts("\n");
	}
#endif
	return bestidx;
}

/*
 * check that all clients controlled by MMC are ready to start,
 * if so, start them all but the caller
 */
int
ctl_trystart(struct aproc *p, int caller)
{
	unsigned i;
	struct ctl_slot *s;

	if (p->u.ctl.tstate != CTL_START) {
#ifdef DEBUG
		if (debug_level >= 3) {
			ctl_slotdbg(p, caller);
			dbg_puts(": server not started, delayd\n");
		}
#endif
		return 0;
	}
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops || i == caller)
			continue;
		if (s->tstate != CTL_OFF && s->tstate != CTL_START) {
#ifdef DEBUG
			if (debug_level >= 3) {
				ctl_slotdbg(p, i);
				dbg_puts(": not ready, server delayed\n");
			}
#endif
			return 0;
		}
	}
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops || i == caller)
			continue;
		if (s->tstate == CTL_START) {
#ifdef DEBUG
			if (debug_level >= 3) {
				ctl_slotdbg(p, i);
				dbg_puts(": started\n");
			}
#endif
			s->tstate = CTL_RUN;
			s->ops->start(s->arg);
		}
	}
	if (caller >= 0)
		p->u.ctl.slot[caller].tstate = CTL_RUN;
	p->u.ctl.tstate = CTL_RUN;
	p->u.ctl.delta = MTC_SEC * dev_getpos(p->u.ctl.dev);
	if (p->u.ctl.dev->rate % (30 * 4 * p->u.ctl.dev->round) == 0) {
		p->u.ctl.fps_id = MTC_FPS_30;
		p->u.ctl.fps = 30;
	} else if (p->u.ctl.dev->rate % (25 * 4 * p->u.ctl.dev->round) == 0) {
		p->u.ctl.fps_id = MTC_FPS_25;
		p->u.ctl.fps = 25;
	} else {
		p->u.ctl.fps_id = MTC_FPS_24;
		p->u.ctl.fps = 24;
	} 
#ifdef DEBUG
	if (debug_level >= 3) {
		ctl_slotdbg(p, caller);
		dbg_puts(": started server at ");
		dbg_puti(p->u.ctl.delta);
		dbg_puts(", ");
		dbg_puti(p->u.ctl.fps);
		dbg_puts(" mtc fps\n");
	}
#endif
	dev_wakeup(p->u.ctl.dev);
	ctl_full(p);
	return 1;
}

/*
 * allocate a new slot and register the given call-backs
 */
int
ctl_slotnew(struct aproc *p, char *who, struct ctl_ops *ops, void *arg, int tr)
{
	int idx;
	struct ctl_slot *s;

	if (!APROC_OK(p)) {
#ifdef DEBUG
		if (debug_level >= 1) {
			dbg_puts(who);
			dbg_puts(": MIDI control not available\n");
		}
#endif
		return -1;
	}
	idx = ctl_getidx(p, who);
	if (idx < 0)
		return -1;

	s = p->u.ctl.slot + idx;
	s->ops = ops;
	s->arg = arg;
	s->tstate = tr ? CTL_STOP : CTL_OFF;
	s->ops->vol(s->arg, s->vol);
	ctl_slotvol(p, idx, s->vol);
	return idx;
}

/*
 * release the given slot
 */
void
ctl_slotdel(struct aproc *p, int index)
{
	unsigned i;
	struct ctl_slot *s;

	if (!APROC_OK(p))
		return;
	p->u.ctl.slot[index].ops = NULL;
	if (!(p->flags & APROC_QUIT))
		return;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops)
			return;
	}
	if (!LIST_EMPTY(&p->outs) || !LIST_EMPTY(&p->ins))
		aproc_del(p);
}

/*
 * called at every clock tick by the mixer, delta is positive, unless
 * there's an overrun/underrun
 */
void
ctl_ontick(struct aproc *p, int delta)
{
	int qfrlen;

	/*
	 * don't send ticks before the start signal
	 */
	if (p->u.ctl.tstate != CTL_RUN)
		return;
	
	p->u.ctl.delta += delta * MTC_SEC;

	/*
	 * don't send ticks during the count-down
	 */
	if (p->u.ctl.delta < 0)
		return;

	qfrlen = p->u.ctl.dev->rate * (MTC_SEC / (4 * p->u.ctl.fps));
	while (p->u.ctl.delta >= qfrlen) {
		ctl_qfr(p);
		p->u.ctl.delta -= qfrlen;
	}
}

/*
 * notifty the mixer that volume changed, called by whom allocad the slot using
 * ctl_slotnew(). Note: it doesn't make sens to call this from within the
 * call-back.
 */
void
ctl_slotvol(struct aproc *p, int slot, unsigned vol)
{
	unsigned char msg[3];

	if (!APROC_OK(p))
		return;
#ifdef DEBUG
	if (debug_level >= 3) {
		ctl_slotdbg(p, slot);
		dbg_puts(": changing volume to ");
		dbg_putu(vol);
		dbg_puts("\n");
	}
#endif
	p->u.ctl.slot[slot].vol = vol;
	msg[0] = MIDI_CTL | slot;
	msg[1] = MIDI_CTLVOL;
	msg[2] = vol;
	ctl_sendmsg(p, NULL, msg, 3);
}

/*
 * notify the MMC layer that the stream is attempting
 * to start. If other streams are not ready, 0 is returned meaning 
 * that the stream should wait. If other streams are ready, they
 * are started, and the caller should start immediately.
 */
int
ctl_slotstart(struct aproc *p, int slot)
{
	struct ctl_slot *s = p->u.ctl.slot + slot;

	if (!APROC_OK(p))
		return 1;
	if (s->tstate == CTL_OFF || p->u.ctl.tstate == CTL_OFF)
		return 1;

	/*
	 * if the server already started (the client missed the
	 * start rendez-vous) or the server is stopped, then
	 * tag the client as ``wanting to start''
	 */
	s->tstate = CTL_START;
	return ctl_trystart(p, slot);
}

/*
 * notify the MMC layer that the stream no longer is trying to
 * start (or that it just stopped), meaning that its ``start'' call-back
 * shouldn't be called anymore
 */
void
ctl_slotstop(struct aproc *p, int slot)
{
	struct ctl_slot *s = p->u.ctl.slot + slot;

	if (!APROC_OK(p))
		return;
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
ctl_start(struct aproc *p)
{
	if (!APROC_OK(p))
		return;
	if (p->u.ctl.tstate == CTL_STOP) {
		p->u.ctl.tstate = CTL_START;
		(void)ctl_trystart(p, -1);
#ifdef DEBUG
	} else {
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": ignoring mmc start\n");
		}
#endif
	}
}

/*
 * stop all slots simultaneously
 */
void
ctl_stop(struct aproc *p)
{
	unsigned i;
	struct ctl_slot *s;

	if (!APROC_OK(p))
		return;
	switch (p->u.ctl.tstate) {
	case CTL_START:
		p->u.ctl.tstate = CTL_STOP;
		return;
	case CTL_RUN:
		p->u.ctl.tstate = CTL_STOP;
		break;
	default:
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": ignored mmc stop\n");
		}
#endif
		return;
	}
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops)
			continue;
		if (s->tstate == CTL_RUN) {
#ifdef DEBUG
			if (debug_level >= 3) {
				ctl_slotdbg(p, i);
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
ctl_loc(struct aproc *p, unsigned origin)
{
	unsigned i, tstate;
	struct ctl_slot *s;

	if (!APROC_OK(p))
		return;
#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts("server relocated to ");
		dbg_putu(origin);
		dbg_puts("\n");
	}
#endif
	tstate = p->u.ctl.tstate;
	if (tstate == CTL_RUN)
		ctl_stop(p);
	p->u.ctl.origin = origin;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (!s->ops)
			continue;
		s->ops->loc(s->arg, p->u.ctl.origin);
	}
	if (tstate == CTL_RUN)
		ctl_start(p);
}

/*
 * check if there are controlled streams
 */
int
ctl_idle(struct aproc *p)
{
	unsigned i;
	struct ctl_slot *s;

	if (!APROC_OK(p))
		return 1;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops)
			return 0;
	}
	return 1;
}

/*
 * handle a MIDI event received from ibuf
 */
void
ctl_ev(struct aproc *p, struct abuf *ibuf)
{
	unsigned chan;
	struct ctl_slot *slot;
	unsigned fps;
#ifdef DEBUG
	unsigned i;

	if (debug_level >= 3) {
		abuf_dbg(ibuf);
		dbg_puts(": got event:");
		for (i = 0; i < ibuf->r.midi.idx; i++) {
			dbg_puts(" ");
			dbg_putx(ibuf->r.midi.msg[i]);
		}
		dbg_puts("\n");
	}
#endif
	if ((ibuf->r.midi.msg[0] & MIDI_CMDMASK) == MIDI_CTL &&
	    ibuf->r.midi.msg[1] == MIDI_CTLVOL) {
		chan = ibuf->r.midi.msg[0] & MIDI_CHANMASK;
		if (chan >= CTL_NSLOT)
			return;
		slot = p->u.ctl.slot + chan;
		slot->vol = ibuf->r.midi.msg[2];
		if (slot->ops == NULL)
			return;
		slot->ops->vol(slot->arg, slot->vol);
		ctl_sendmsg(p, ibuf, ibuf->r.midi.msg, ibuf->r.midi.len);
	}
	if (ibuf->r.midi.idx == 6 &&
	    ibuf->r.midi.msg[0] == 0xf0 &&
	    ibuf->r.midi.msg[1] == 0x7f &&	/* type is realtime */
	    ibuf->r.midi.msg[3] == 0x06	&&	/* subtype is mmc */
	    ibuf->r.midi.msg[5] == 0xf7) {	/* subtype is mmc */
		switch (ibuf->r.midi.msg[4]) {
		case 0x01:	/* mmc stop */
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(ibuf);
				dbg_puts(": mmc stop\n");
			}
#endif
			ctl_stop(p);
			break;
		case 0x02:	/* mmc start */
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(ibuf);
				dbg_puts(": mmc start\n");
			}
#endif
			ctl_start(p);
			break;
		}
	}
	if (ibuf->r.midi.idx == 13 &&
	    ibuf->r.midi.msg[0] == 0xf0 &&
	    ibuf->r.midi.msg[1] == 0x7f &&
	    ibuf->r.midi.msg[3] == 0x06 &&
	    ibuf->r.midi.msg[4] == 0x44 &&
	    ibuf->r.midi.msg[5] == 0x06 &&
	    ibuf->r.midi.msg[6] == 0x01 &&
	    ibuf->r.midi.msg[12] == 0xf7) {
		switch (ibuf->r.midi.msg[7] >> 5) {
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
			p->u.ctl.origin = 0;
			return;
		}
		ctl_loc(p,
		    (ibuf->r.midi.msg[7] & 0x1f) * 3600 * MTC_SEC +
		    ibuf->r.midi.msg[8] * 60 * MTC_SEC +
		    ibuf->r.midi.msg[9] * MTC_SEC +
		    ibuf->r.midi.msg[10] * (MTC_SEC / fps) +
		    ibuf->r.midi.msg[11] * (MTC_SEC / 100 / fps));
	}
}

int
ctl_in(struct aproc *p, struct abuf *ibuf)
{
	unsigned char *idata;
	unsigned c, i, icount;

	if (!ABUF_ROK(ibuf))
		return 0;
	idata = abuf_rgetblk(ibuf, &icount, 0);
	for (i = 0; i < icount; i++) {
		c = *idata++;
		if (c >= 0xf8) {
			/* clock events not used yet */
		} else if (c >= 0xf0) {
			if (ibuf->r.midi.st == 0xf0 && c == 0xf7 &&
			    ibuf->r.midi.idx < MIDI_MSGMAX) {
				ibuf->r.midi.msg[ibuf->r.midi.idx++] = c;
				ctl_ev(p, ibuf);
				continue;
			}
			ibuf->r.midi.msg[0] = c;
			ibuf->r.midi.len = common_len[c & 7];
			ibuf->r.midi.st = c;
			ibuf->r.midi.idx = 1;
		} else if (c >= 0x80) {
			ibuf->r.midi.msg[0] = c;
			ibuf->r.midi.len = voice_len[(c >> 4) & 7];
			ibuf->r.midi.st = c;
			ibuf->r.midi.idx = 1;
		} else if (ibuf->r.midi.st) {
			if (ibuf->r.midi.idx == MIDI_MSGMAX)
				continue;		
			if (ibuf->r.midi.idx == 0)
				ibuf->r.midi.msg[ibuf->r.midi.idx++] = ibuf->r.midi.st;
			ibuf->r.midi.msg[ibuf->r.midi.idx++] = c;
			if (ibuf->r.midi.idx == ibuf->r.midi.len) {
				ctl_ev(p, ibuf);
				ibuf->r.midi.idx = 0;
			}
		}
	}
	abuf_rdiscard(ibuf, icount);
	return 1;
}

int
ctl_out(struct aproc *p, struct abuf *obuf)
{
	return 0;
}

void
ctl_eof(struct aproc *p, struct abuf *ibuf)
{
	unsigned i;
	struct ctl_slot *s;

	if (!(p->flags & APROC_QUIT))
		return;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops)
			return;
	}
	if (!LIST_EMPTY(&p->outs) || !LIST_EMPTY(&p->ins))
		aproc_del(p);
}

void
ctl_hup(struct aproc *p, struct abuf *obuf)
{
	unsigned i;
	struct ctl_slot *s;

	if (!(p->flags & APROC_QUIT))
		return;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops)
			return;
	}
	if (!LIST_EMPTY(&p->outs) || !LIST_EMPTY(&p->ins))
		aproc_del(p);
}

void
ctl_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->r.midi.used = 0;
	ibuf->r.midi.len = 0;
	ibuf->r.midi.idx = 0;
	ibuf->r.midi.st = 0;
}

void
ctl_done(struct aproc *p)
{
	unsigned i;
	struct ctl_slot *s;

	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		if (s->ops != NULL)
			s->ops->quit(s->arg);
#ifdef DEBUG
		if (s->ops != NULL) {
			ctl_slotdbg(p, i);
			dbg_puts(": still in use\n");
			dbg_panic();
		}
#endif
	}
}

struct aproc_ops ctl_ops = {
	"ctl",
	ctl_in,
	ctl_out,
	ctl_eof,
	ctl_hup,
	ctl_newin,
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	ctl_done
};

struct aproc *
ctl_new(char *name, struct dev *dev)
{
	struct aproc *p;
	struct ctl_slot *s;
	unsigned i;

	p = aproc_new(&ctl_ops, name);
	p->u.ctl.dev = dev;
	p->u.ctl.serial = 0;
	p->u.ctl.tstate = CTL_STOP;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		p->u.ctl.slot[i].unit = i;
		p->u.ctl.slot[i].ops = NULL;
		p->u.ctl.slot[i].vol = MIDI_MAXCTL;
		p->u.ctl.slot[i].tstate = CTL_OFF;
		p->u.ctl.slot[i].serial = p->u.ctl.serial++;
		p->u.ctl.slot[i].name[0] = '\0';
	}
	return p;
}
