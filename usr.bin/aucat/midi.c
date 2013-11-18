/*	$OpenBSD: midi.c,v 1.46 2013/11/18 17:37:45 ratchov Exp $	*/
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "midi.h"
#include "sysex.h"
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

void midi_cb(void *);
void midi_msg_info(struct aproc *, int, unsigned char *);
void midi_msg_vol(struct aproc *, int, unsigned char *);
void midi_msg_master(struct aproc *, unsigned char *);
void midi_copy(struct abuf *, struct abuf *, unsigned char *, unsigned int);
void midi_send(struct aproc *, struct abuf *, unsigned char *, unsigned int);
void midi_copy_dump(struct aproc *, struct abuf *);
void midi_onvoice(struct aproc *, struct abuf *);
void midi_onsysex(struct aproc *, struct abuf *);
int midi_in(struct aproc *, struct abuf *);
int midi_out(struct aproc *, struct abuf *);
void midi_eof(struct aproc *, struct abuf *);
void midi_hup(struct aproc *, struct abuf *);
void midi_newin(struct aproc *, struct abuf *);
void midi_done(struct aproc *);

/*
 * length of voice and common messages (status byte included)
 */
unsigned int voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
unsigned int common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

/*
 * call-back invoked periodically to implement throttling; at each invocation
 * gain more ``tickets'' for processing.  If one of the buffer was blocked by
 * the throttling mechanism, then run it
 */
void
midi_cb(void *addr)
{
	struct aproc *p = (struct aproc *)addr;
	struct abuf *i, *inext;
	unsigned int tickets;

	timo_add(&p->u.midi.timo, MIDITHRU_TIMO);
	
	for (i = LIST_FIRST(&p->ins); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		tickets = i->tickets;
		i->tickets = MIDITHRU_XFER;
		if (tickets == 0)
			abuf_run(i);
	}
}

void
midi_msg_info(struct aproc *p, int slot, unsigned char *msg)
{
	struct ctl_slot *s;
	struct sysex *x = (struct sysex *)msg;

	s = p->u.midi.dev->slot + slot;
	memset(x, 0, sizeof(struct sysex));
	x->start = SYSEX_START;
	x->type = SYSEX_TYPE_EDU;
	x->id0 = SYSEX_AUCAT;
	x->id1 = SYSEX_AUCAT_MIXINFO;
	if (*s->name != '\0') {
		snprintf((char *)x->u.mixinfo.name,
		    SYSEX_NAMELEN, "%s%u", s->name, s->unit);
	}
	x->u.mixinfo.chan = slot;
	x->u.mixinfo.end = SYSEX_END;
}

void
midi_msg_vol(struct aproc *p, int slot, unsigned char *msg)
{
	struct ctl_slot *s;

	s = p->u.midi.dev->slot + slot;	
	msg[0] = MIDI_CTL | slot;
	msg[1] = MIDI_CTLVOL;
	msg[2] = s->vol;
}

void
midi_msg_master(struct aproc *p, unsigned char *msg)
{
	struct sysex *x = (struct sysex *)msg;

	memset(x, 0, sizeof(struct sysex));
	x->start = SYSEX_START;
	x->type = SYSEX_TYPE_RT;
	x->id0 = SYSEX_CONTROL;
	x->id1 = SYSEX_MASTER;
	x->u.master.fine = 0;
	x->u.master.coarse = p->u.midi.dev->master;
	x->u.master.end = SYSEX_END;
}

/*
 * send a message to the given output
 */
void
midi_copy(struct abuf *ibuf, struct abuf *obuf, unsigned char *msg,
    unsigned int len)
{
	unsigned int ocount;
	unsigned char *odata;

	if (msg[0] == SYSEX_START)
		obuf->w.midi.owner = ibuf;
	while (len > 0) {
		if (!ABUF_WOK(obuf)) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(obuf);
				dbg_puts(": overrun, discarding ");
				dbg_putu(obuf->used);
				dbg_puts(" bytes\n");
			}
#endif
			abuf_rdiscard(obuf, obuf->used);
			if (obuf->w.midi.owner == ibuf)
				obuf->w.midi.owner = NULL;
			return;
		}
		odata = abuf_wgetblk(obuf, &ocount, 0);
		if (ocount > len)
			ocount = len;
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(obuf);
			dbg_puts(": stored ");
			dbg_putu(ocount);
			dbg_puts(" bytes\n");
		}
#endif
		memcpy(odata, msg, ocount);
		abuf_wcommit(obuf, ocount);
		len -= ocount;
		msg += ocount;
	}
}

/*
 * flush all buffers. Since most of the MIDI traffic is broadcasted to
 * all outputs, the flush is delayed to avoid flushing all outputs for
 * each message.
 */
void
midi_flush(struct aproc *p)
{
	struct abuf *i, *inext;

	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (ABUF_ROK(i))
			(void)abuf_flush(i);
	}
}

/*
 * broadcast a message to all output buffers on the behalf of ibuf.
 * ie. don't sent back the message to the sender
 */
void
midi_send(struct aproc *p, struct abuf *ibuf, unsigned char *msg,
    unsigned int len)
{
	struct abuf *i, *inext;

	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (i->duplex && i->duplex == ibuf)
			continue;
		midi_copy(ibuf, i, msg, len);
	}
}

/*
 * send a quarter frame MTC message
 */
void
midi_send_qfr(struct aproc *p, unsigned int rate, int delta)
{
	unsigned char buf[2];
	unsigned int data;
	int qfrlen;

	p->u.midi.delta += delta * MTC_SEC;
	qfrlen = rate * (MTC_SEC / (4 * p->u.midi.fps));
	while (p->u.midi.delta >= qfrlen) {
		switch (p->u.midi.qfr) {
		case 0:
			data = p->u.midi.fr & 0xf;
			break;
		case 1:
			data = p->u.midi.fr >> 4;
			break;
		case 2:
			data = p->u.midi.sec & 0xf;
			break;
		case 3:
			data = p->u.midi.sec >> 4;
			break;
		case 4:
			data = p->u.midi.min & 0xf;
			break;
		case 5:
			data = p->u.midi.min >> 4;
			break;
		case 6:
			data = p->u.midi.hr & 0xf;
			break;
		case 7:
			data = (p->u.midi.hr >> 4) | (p->u.midi.fps_id << 1);
			/*
			 * tick messages are sent 2 frames ahead
			 */
			p->u.midi.fr += 2;
			if (p->u.midi.fr < p->u.midi.fps)
				break;
			p->u.midi.fr -= p->u.midi.fps;
			p->u.midi.sec++;
			if (p->u.midi.sec < 60)
				break;
			p->u.midi.sec = 0;
			p->u.midi.min++;
			if (p->u.midi.min < 60)
				break;
			p->u.midi.min = 0;
			p->u.midi.hr++;
			if (p->u.midi.hr < 24)
				break;
			p->u.midi.hr = 0;
			break;
		default:
			/* NOTREACHED */
			data = 0;
		}
		buf[0] = 0xf1;
		buf[1] = (p->u.midi.qfr << 4) | data;
		p->u.midi.qfr++;
		p->u.midi.qfr &= 7;
		midi_send(p, NULL, buf, 2);
		p->u.midi.delta -= qfrlen;
	}
}

/*
 * send a full frame MTC message
 */
void
midi_send_full(struct aproc *p, unsigned int origin, unsigned int rate,
    unsigned int round, unsigned int pos)
{
	unsigned char buf[10];
	unsigned int fps;

	p->u.midi.delta = MTC_SEC * pos;
	if (rate % (30 * 4 * round) == 0) {
		p->u.midi.fps_id = MTC_FPS_30;
		p->u.midi.fps = 30;
	} else if (rate % (25 * 4 * round) == 0) {
		p->u.midi.fps_id = MTC_FPS_25;
		p->u.midi.fps = 25;
	} else {
		p->u.midi.fps_id = MTC_FPS_24;
		p->u.midi.fps = 24;
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": mtc full frame at ");
		dbg_puti(p->u.midi.delta);
		dbg_puts(", ");
		dbg_puti(p->u.midi.fps);
		dbg_puts(" fps\n");
	}
#endif
	fps = p->u.midi.fps;
	p->u.midi.hr =  (origin / (3600 * MTC_SEC)) % 24;
	p->u.midi.min = (origin / (60 * MTC_SEC))   % 60;
	p->u.midi.sec = (origin / MTC_SEC)          % 60;
	p->u.midi.fr =  (origin / (MTC_SEC / fps))  % fps;

	buf[0] = 0xf0;
	buf[1] = 0x7f;
	buf[2] = 0x7f;
	buf[3] = 0x01;
	buf[4] = 0x01;
	buf[5] = p->u.midi.hr | (p->u.midi.fps_id << 5);
	buf[6] = p->u.midi.min;
	buf[7] = p->u.midi.sec;
	buf[8] = p->u.midi.fr;
	buf[9] = 0xf7;
	p->u.midi.qfr = 0;
	midi_send(p, NULL, buf, 10);
}

void
midi_copy_dump(struct aproc *p, struct abuf *obuf)
{
	unsigned int i;
	unsigned char msg[sizeof(struct sysex)];
	struct ctl_slot *s;

	midi_msg_master(p, msg);
	midi_copy(NULL, obuf, msg, SYSEX_SIZE(master));
	for (i = 0, s = p->u.midi.dev->slot; i < CTL_NSLOT; i++, s++) {
		midi_msg_info(p, i, msg);
		midi_copy(NULL, obuf, msg, SYSEX_SIZE(mixinfo));
		midi_msg_vol(p, i, msg);
		midi_copy(NULL, obuf, msg, 3);
	}
	msg[0] = SYSEX_START;
	msg[1] = SYSEX_TYPE_EDU;
	msg[2] = 0;
	msg[3] = SYSEX_AUCAT;
	msg[4] = SYSEX_AUCAT_DUMPEND;
	msg[5] = SYSEX_END;
	midi_copy(NULL, obuf, msg, 6);
}

/*
 * notifty the mixer that volume changed, called by whom allocated the slot using
 * ctl_slotnew(). Note: it doesn't make sense to call this from within the
 * call-back.
 */
void
midi_send_vol(struct aproc *p, int slot, unsigned int vol)
{
	unsigned char msg[3];

	midi_msg_vol(p, slot, msg);
	midi_send(p, NULL, msg, 3);
}

void
midi_send_master(struct aproc *p)
{
	unsigned char msg[sizeof(struct sysex)];
	
	midi_msg_master(p, msg);
	midi_send(p, NULL, msg, SYSEX_SIZE(master));
}

void
midi_send_slot(struct aproc *p, int slot)
{
	unsigned char msg[sizeof(struct sysex)];

	midi_msg_info(p, slot, msg);
	midi_send(p, NULL, msg, SYSEX_SIZE(mixinfo));
}

/*
 * handle a MIDI voice event received from ibuf
 */
void
midi_onvoice(struct aproc *p, struct abuf *ibuf)
{
	struct ctl_slot *slot;
	unsigned int chan;
#ifdef DEBUG
	unsigned int i;

	if (debug_level >= 3) {
		abuf_dbg(ibuf);
		dbg_puts(": got voice event:");
		for (i = 0; i < ibuf->r.midi.idx; i++) {
			dbg_puts(" ");
			dbg_putx(ibuf->r.midi.msg[i]);
		}
		dbg_puts("\n");
	}
#endif
	if ((ibuf->r.midi.msg[0] & MIDI_CMDMASK) == MIDI_CTL &&
	    (ibuf->r.midi.msg[1] == MIDI_CTLVOL)) {
		midi_send(p, ibuf, ibuf->r.midi.msg, 3);
		chan = ibuf->r.midi.msg[0] & MIDI_CHANMASK;
		if (chan >= CTL_NSLOT)
			return;
		slot = p->u.midi.dev->slot + chan;
		slot->vol = ibuf->r.midi.msg[2];
		if (slot->ops == NULL)
			return;
		slot->ops->vol(slot->arg, slot->vol);
	}
}

/*
 * handle a MIDI sysex received from ibuf
 */
void
midi_onsysex(struct aproc *p, struct abuf *ibuf)
{
	struct sysex *x;
	unsigned int fps, len;
#ifdef DEBUG
	unsigned int i;

	if (debug_level >= 3) {
		abuf_dbg(ibuf);
		dbg_puts(": got sysex:");
		for (i = 0; i < ibuf->r.midi.idx; i++) {
			dbg_puts(" ");
			dbg_putx(ibuf->r.midi.msg[i]);
		}
		dbg_puts("\n");
	}
#endif
	x = (struct sysex *)ibuf->r.midi.msg;
	len = ibuf->r.midi.idx;
	if (x->start != SYSEX_START)
		return;
	if (len < SYSEX_SIZE(empty))
		return;
	switch (x->type) {
	case SYSEX_TYPE_RT:
		if (x->id0 == SYSEX_CONTROL && x->id1 == SYSEX_MASTER) {
			if (len == SYSEX_SIZE(master)) {
				dev_master(p->u.midi.dev, x->u.master.coarse);
				midi_send(p, ibuf, (unsigned char *)x, len);
			}
			return;
		}
		if (x->id0 != SYSEX_MMC)
			return;
		switch (x->id1) {
		case SYSEX_MMC_STOP:
			if (len != SYSEX_SIZE(stop))
				return;
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(ibuf);
				dbg_puts(": mmc stop\n");
			}
#endif
			dev_mmcstop(p->u.midi.dev);
			break;
		case SYSEX_MMC_START:
			if (len != SYSEX_SIZE(start))
				return;
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(ibuf);
				dbg_puts(": mmc start\n");
			}
#endif
			dev_mmcstart(p->u.midi.dev);
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
				/* XXX: should dev_mmcstop() here */
				return;
			}
			dev_loc(p->u.midi.dev,
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
		if (ibuf->duplex)
			midi_copy_dump(p, ibuf->duplex);
		break;
	}
}

int
midi_in(struct aproc *p, struct abuf *ibuf)
{
	unsigned char c, *idata;
	unsigned int i, icount;

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
	idata = abuf_rgetblk(ibuf, &icount, 0);
	if (icount > ibuf->tickets)
		icount = ibuf->tickets;
	ibuf->tickets -= icount;
	for (i = 0; i < icount; i++) {
		c = *idata++;
		if (c >= 0xf8) {
			if (!p->u.midi.dev && c != MIDI_ACK)
				midi_send(p, ibuf, &c, 1);
		} else if (c == SYSEX_END) {
			if (ibuf->r.midi.st == SYSEX_START) {
				ibuf->r.midi.msg[ibuf->r.midi.idx++] = c;
				if (!p->u.midi.dev) {
					midi_send(p, ibuf,
					    ibuf->r.midi.msg, ibuf->r.midi.idx);
				} else
					midi_onsysex(p, ibuf);
			}
			ibuf->r.midi.st = 0;
			ibuf->r.midi.idx = 0;
		} else if (c >= 0xf0) {
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
			if (ibuf->r.midi.idx == 0 &&
			    ibuf->r.midi.st != SYSEX_START) {
				ibuf->r.midi.msg[ibuf->r.midi.idx++] =
				    ibuf->r.midi.st;
			}
			ibuf->r.midi.msg[ibuf->r.midi.idx++] = c;
			if (ibuf->r.midi.idx == ibuf->r.midi.len) {
				if (!p->u.midi.dev) {
					midi_send(p, ibuf,
					    ibuf->r.midi.msg, ibuf->r.midi.idx);
				} else
					midi_onvoice(p, ibuf);
				if (ibuf->r.midi.st >= 0xf0)
					ibuf->r.midi.st = 0;
				ibuf->r.midi.idx = 0;
			} else if (ibuf->r.midi.idx == MIDI_MSGMAX) {
				if (!p->u.midi.dev) {
					midi_send(p, ibuf,
					    ibuf->r.midi.msg, ibuf->r.midi.idx);
				}
				ibuf->r.midi.idx = 0;
			}
		}
	}
	/*
	 * XXX: if the sysex is received byte by byte, partial messages
	 * won't be sent until the end byte is received. On the other
	 * hand we can't flush it here, since we would lose messages
	 * we parse
	 */
	abuf_rdiscard(ibuf, icount);
	midi_flush(p);
	return 1;
}

int
midi_out(struct aproc *p, struct abuf *obuf)
{
	return 0;
}

void
midi_eof(struct aproc *p, struct abuf *ibuf)
{
	if ((p->flags & APROC_QUIT) && LIST_EMPTY(&p->ins))
		aproc_del(p);
}

void
midi_hup(struct aproc *p, struct abuf *obuf)
{
	if ((p->flags & APROC_QUIT) && LIST_EMPTY(&p->ins))
		aproc_del(p);
}

void
midi_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->r.midi.used = 0;
	ibuf->r.midi.len = 0;
	ibuf->r.midi.idx = 0;
	ibuf->r.midi.st = 0;
	ibuf->tickets = MIDITHRU_XFER;
}

void
midi_done(struct aproc *p)
{
	timo_del(&p->u.midi.timo);
}

struct aproc_ops midi_ops = {
	"midi",
	midi_in,
	midi_out,
	midi_eof,
	midi_hup,
	midi_newin,
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	midi_done,
};

struct aproc *
midi_new(char *name, struct dev *dev)
{
	struct aproc *p;

	p = aproc_new(&midi_ops, name);
	timo_set(&p->u.midi.timo, midi_cb, p);
	timo_add(&p->u.midi.timo, MIDITHRU_TIMO);
	p->u.midi.dev = dev;
	return p;
}
