/*	$OpenBSD: midi.c,v 1.8 2009/08/29 14:46:44 ratchov Exp $	*/
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
 * send the message stored in of ibuf->mdata to obuf
 */
void
thru_flush(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned ocount, itodo;
	unsigned char *odata, *idata;

	itodo = ibuf->mused;
	idata = ibuf->mdata;
	DPRINTFN(4, "thru_flush: mused = %u\n", itodo);
	while (itodo > 0) {
		if (!ABUF_WOK(obuf)) {
			abuf_rdiscard(obuf, obuf->used);
			DPRINTFN(2, "thru_flush: discarded %u\n", obuf->used);
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
	ibuf->mused = 0;
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

	DPRINTFN(4, "thru_rt:\n");
	if (!ABUF_WOK(obuf)) {
		DPRINTFN(2, "thru_rt: discarded %u\n", obuf->used);
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
			if (ibuf->mindex == 0 && ibuf->mstatus) {
				ibuf->mdata[ibuf->mused++] = ibuf->mstatus;
				ibuf->mindex++;
			}
			ibuf->mdata[ibuf->mused++] = c;
			ibuf->mindex++;
			if (ibuf->mindex == ibuf->mlen) {
				thru_flush(p, ibuf, obuf);
				if (ibuf->mstatus >= 0xf0)
					ibuf->mstatus = 0;
				ibuf->mindex = 0;
			}
			if (ibuf->mused == MDATA_NMAX) {
				if (ibuf->mused == ibuf->mindex ||
				    p->u.thru.owner == ibuf)
					thru_flush(p, ibuf, obuf);
				else
					ibuf->mused = 0;
			}
		} else if (c < 0xf8) {
			if (ibuf->mused == ibuf->mindex ||
			    p->u.thru.owner == ibuf) {
				thru_flush(p, ibuf, obuf);
			} else
				ibuf->mused = 0;
			ibuf->mdata[0] = c;
			ibuf->mused = 1;
			ibuf->mlen = (c >= 0xf0) ? 
			    common_len[c & 7] :
			    voice_len[(c >> 4) & 7];
			if (ibuf->mlen == 1) {
				thru_flush(p, ibuf, obuf);
				ibuf->mindex = 0;
				ibuf->mstatus = 0;
				ibuf->mlen = 0;
			} else { 
				ibuf->mstatus = c;
				ibuf->mindex = 1;
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

	DPRINTFN(3, "thru_in: %s\n", p->name);

	if (!ABUF_ROK(ibuf))
		return 0;
	if (ibuf->mtickets == 0) {
		DPRINTFN(2, "thru_in: out of tickets\n");
		return 0;
	}
	todo = ibuf->used;
	if (todo > ibuf->mtickets)
		todo = ibuf->mtickets;
	ibuf->mtickets -= todo;
	for (i = LIST_FIRST(&p->obuflist); i != NULL; i = inext) {
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
	DPRINTF("thru_eof: %s: eof\n", p->name);
}

void
thru_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTF("thru_hup: %s: detached\n", p->name);
}

void
thru_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->mused = 0;
	ibuf->mlen = 0;
	ibuf->mindex = 0;
	ibuf->mstatus = 0;
	ibuf->mtickets = MIDITHRU_XFER;
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
	
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		tickets = i->mtickets;
		i->mtickets = MIDITHRU_XFER;
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

	for (i = LIST_FIRST(&p->obuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (i->duplex == ibuf)
			continue;
		itodo = len;
		idata = msg;
		while (itodo > 0) {
			if (!ABUF_WOK(i)) {
				DPRINTFN(2, "ctl_sendmsg: lost %u\n", i->used);
				abuf_rdiscard(i, i->used);
			}
			odata = abuf_wgetblk(i, &ocount, 0);
			if (ocount > itodo)
				ocount = itodo;
			DPRINTFN(2, "ctl_sendmsg: xfer %u\n", ocount);
			memcpy(odata, idata, ocount);
			abuf_wcommit(i, ocount);
			itodo -= ocount;
			idata += ocount;
		}
		(void)abuf_flush(i);
	}
}

/*
 * allocate a new slot (ie midi channel), register the given call-back
 * to be called volume is changed by MIDI. The call-back is invoked at
 * initialization to restore the saved volume.
 */
int
ctl_slotnew(struct aproc *p, char *who, void (*cb)(void *, unsigned), void *arg)
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
		if (slot->cb == NULL)
			continue;
		if (strcmp(slot->name, name) == 0)
			umap |= (1 << i);
	} 
	for (unit = 0; unit < CTL_NSLOT; unit++) {
		if (unit == CTL_NSLOT)
			return -1;
		if ((umap & (1 << i)) == 0)
			break;
	}

	DPRINTF("ctl_newslot: using %s%u as control name\n", name, unit);

	/*
	 * find a free controller slot with the same name/unit
	 */
	for (i = 0, slot = p->u.ctl.slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->cb == NULL &&
		    strcmp(slot->name, name) == 0 &&
		    slot->unit == unit) {
			DPRINTFN(1, "ctl_newslot: reusing %u\n", i);
			slot->cb = cb;
			slot->arg = arg;
			slot->cb(slot->arg, slot->vol);
			ctl_slotvol(p, i, slot->vol);
			return i;
		}
	}

	/*
	 * couldn't find a matching slot, pick oldest free slot
	 */
	bestser = 0;
	bestidx = CTL_NSLOT;
	for (i = 0, slot = p->u.ctl.slot; i < CTL_NSLOT; i++, slot++) {
		if (slot->cb != NULL)
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
	strlcpy(slot->name, name, CTL_NAMEMAX);
	slot->serial = p->u.ctl.serial++;
	slot->unit = unit;
	slot->vol = MIDI_MAXCTL;
	DPRINTFN(1, "ctl_newslot: %u overwritten)\n", bestidx);
	slot->cb = cb;
	slot->arg = arg;
	slot->cb(slot->arg, slot->vol);
	ctl_slotvol(p, bestidx, slot->vol);
	return bestidx;
}

/*
 * release the given slot
 */
void
ctl_slotdel(struct aproc *p, int index)
{
	p->u.ctl.slot[index].cb = NULL;
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

	DPRINTFN(1, "ctl_slotvol: [%u] -> %u\n", slot, vol);
	p->u.ctl.slot[slot].vol = vol;
	msg[0] = MIDI_CTL | slot;
	msg[1] = MIDI_CTLVOL;
	msg[2] = vol;
	ctl_sendmsg(p, NULL, msg, 3);
}

/*
 * handle a MIDI event received from ibuf
 */
void
ctl_ev(struct aproc *p, struct abuf *ibuf)
{
	unsigned i;
	unsigned chan;
	struct ctl_slot *slot;

#ifdef DEBUG
	if (debug_level > 0) {
		fprintf(stderr, "ctl_ev:");
		for (i = 0; i < ibuf->mindex; i++)
			fprintf(stderr, " %02x", ibuf->mdata[i]);
		fprintf(stderr, "\n");
	}
#endif
	if ((ibuf->mdata[0] & MIDI_CMDMASK) == MIDI_CTL &&
	    ibuf->mdata[1] == MIDI_CTLVOL) {
		chan = ibuf->mdata[0] & MIDI_CHANMASK;
		if (chan >= CTL_NSLOT)
			return;
		slot = p->u.ctl.slot + chan;
		if (slot->cb == NULL)
			return;
		slot->vol = ibuf->mdata[2];
		slot->cb(slot->arg, slot->vol);
		ctl_sendmsg(p, ibuf, ibuf->mdata, ibuf->mlen);
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
			if (ibuf->mstatus == 0xf0 && c == 0xf7 &&
			    ibuf->mindex < MDATA_NMAX) {
				ibuf->mdata[ibuf->mindex++] = c;
				ctl_ev(p, ibuf);
				continue;
			}
			ibuf->mdata[0] = c;
			ibuf->mlen = common_len[c & 7];
			ibuf->mstatus = c;
			ibuf->mindex = 1;
		} else if (c >= 0x80) {
			ibuf->mdata[0] = c;
			ibuf->mlen = voice_len[(c >> 4) & 7];
			ibuf->mstatus = c;
			ibuf->mindex = 1;
		} else if (ibuf->mstatus) {
			if (ibuf->mindex == MDATA_NMAX)
				continue;		
			if (ibuf->mindex == 0)
				ibuf->mdata[ibuf->mindex++] = ibuf->mstatus;
			ibuf->mdata[ibuf->mindex++] = c;
			if (ibuf->mindex == ibuf->mlen) {
				ctl_ev(p, ibuf);
				ibuf->mindex = 0;
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
	DPRINTF("ctl_eof: %s: eof\n", p->name);
}

void
ctl_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTF("ctl_hup: %s: detached\n", p->name);
}

void
ctl_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->mused = 0;
	ibuf->mlen = 0;
	ibuf->mindex = 0;
	ibuf->mstatus = 0;
}

void
ctl_done(struct aproc *p)
{
	unsigned i;
	struct ctl_slot *s;

	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		/*
		 * XXX: shouldn't we abord() here ?
		 */
		if (s->cb != NULL)
			DPRINTF("ctl_done: %s%u in use\n", s->name, s->unit);
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
ctl_new(char *name)
{
	struct aproc *p;
	struct ctl_slot *s;
	unsigned i;

	p = aproc_new(&ctl_ops, name);
	p->u.ctl.serial = 0;
	for (i = 0, s = p->u.ctl.slot; i < CTL_NSLOT; i++, s++) {
		p->u.ctl.slot[i].unit = i;
		p->u.ctl.slot[i].cb = NULL;
		p->u.ctl.slot[i].vol = MIDI_MAXCTL;
		p->u.ctl.slot[i].serial = p->u.ctl.serial++;
		strlcpy(p->u.ctl.slot[i].name, "unknown", CTL_NAMEMAX);
	}
	return p;
}

