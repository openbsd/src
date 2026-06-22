/*	$OpenBSD: midi.c,v 1.38 2026/06/22 14:16:49 ratchov Exp $	*/
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
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "file.h"
#include "midi.h"
#include "miofile.h"
#include "sysex.h"
#include "utils.h"

int  port_open(struct port *);
void port_imsg(void *, unsigned char *, int);
void port_omsg(void *, unsigned char *, int);
void port_fill(void *, int);
void port_exit(void *);

struct midiops port_midiops = {
	port_imsg,
	port_omsg,
	port_fill,
	port_exit
};

#define MIDI_NEP 32
struct midi midi_ep[MIDI_NEP];
struct port *port_list = NULL;
unsigned int midi_portnum = 0;
struct midithru midithru_array[MIDITHRU_NMAX];

/*
 * length of voice and common messages (status byte included)
 */
const unsigned int voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
const unsigned int common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

size_t
midiev_fmt(char *buf, size_t size, unsigned char *ev, size_t len)
{
	const char *sep = "";
	char *end = buf + size;
	char *p = buf;
	int i;

	for (i = 0; i < len; i++) {
		if (i == 1)
			sep = " ";
		p += snprintf(p, p < end ? end - p : 0, "%s%02x", sep, ev[i]);
	}

	return p - buf;
}

void
midi_init(void)
{
}

void
midi_done(void)
{
}

struct midi *
midi_new(struct midiops *ops, void *arg, int mode)
{
	int i;
	struct midi *ep;

	for (i = 0, ep = midi_ep;; i++, ep++) {
		if (i == MIDI_NEP)
			return NULL;
		if (ep->ops == NULL)
			break;
	}
	ep->ops = ops;
	ep->arg = arg;
	ep->used = 0;
	ep->len = 0;
	ep->idx = 0;
	ep->st = 0;
	ep->last_st = 0;
	ep->txmask = 0;
	ep->num = i;
	ep->self = 1 << i;
	ep->tickets = 0;
	ep->mode = mode;

	/*
	 * the output buffer is the client input
	 */
	if (ep->mode & MODE_MIDIIN)
		abuf_init(&ep->obuf, MIDI_BUFSZ);
	midi_tickets(ep);
	return ep;
}

void
midi_del(struct midi *ep)
{
	int i;
	struct midi *peer;

	ep->txmask = 0;
	for (i = 0; i < MIDI_NEP; i++) {
		peer = midi_ep + i;
		if (peer->txmask & ep->self) {
			peer->txmask &= ~ep->self;
			midi_tickets(peer);
		}
	}
	for (i = 0; i < MIDITHRU_NMAX; i++) {
		midithru_array[i].progmask &= ~ep->self;
		midithru_array[i].portmask &= ~ep->self;
	}
	ep->ops = NULL;
	if (ep->mode & MODE_MIDIIN) {
		abuf_done(&ep->obuf);
	}
}

/*
 * connect two midi endpoints
 */
void
midi_link(struct midi *ep, struct midi *peer)
{
	if ((ep->mode & MODE_MIDIOUT) && (peer->mode & MODE_MIDIIN)) {
		ep->txmask |= peer->self;
		midi_tickets(ep);
	}
	if ((ep->mode & MODE_MIDIIN) && (peer->mode & MODE_MIDIOUT)) {
#ifdef DEBUG
		if (ep->obuf.used > 0) {
			logx(0, "midi%u: linked with non-empty buffer", ep->num);
			panic();
		}
#endif
		/* ep has empty buffer, so no need to call midi_tickets() */
		peer->txmask |= ep->self;
	}
}

/*
 * disconnect two midi endpoints
 */
void
midi_unlink(struct midi *ep, struct midi *peer)
{
	if (peer->txmask & ep->self) {
		peer->txmask &= ~ep->self;
		midi_tickets(peer);
	}
	if (ep->txmask & peer->self) {
		ep->txmask &= ~peer->self;
		midi_tickets(ep);
	}
}

/*
 * broadcast the given message to other endpoints
 */
void
midi_send(struct midi *iep, unsigned char *msg, int size)
{
#ifdef DEBUG
	char str[128];
#endif
	struct midi *oep;
	int i;

#ifdef DEBUG
	logx(4, "midi%u: sending: %s", iep->num,
	    (midiev_fmt(str, sizeof(str), msg, size), str));
#endif
	for (i = 0; i < MIDI_NEP ; i++) {
		if ((iep->txmask & (1 << i)) == 0)
			continue;
		oep = midi_ep + i;
		if (msg[0] <= 0x7f) {
			if (oep->owner != iep)
				continue;
		} else if (msg[0] <= 0xf7)
			oep->owner = iep;
#ifdef DEBUG
		logx(4, "midi%u -> midi%u", iep->num, oep->num);
#endif
		oep->ops->omsg(oep->arg, msg, size);
	}
}

/*
 * determine if we have gained more input tickets, and if so call the
 * fill() call-back to notify the i/o layer that it can send more data
 */
void
midi_tickets(struct midi *iep)
{
	int i, tickets, avail, maxavail;
	struct midi *oep;

	/*
	 * don't request iep->ops->fill() too often as it generates
	 * useless network traffic: wait until we reach half of the
	 * max tickets count. As in the worst case (see comment below)
	 * one ticket may consume two bytes, the max ticket count is
	 * BUFSZ / 2 and halt of it is simply BUFSZ / 4.
	 */
	if (iep->tickets >= MIDI_BUFSZ / 4)
		return;

	maxavail = MIDI_BUFSZ;
	for (i = 0; i < MIDI_NEP ; i++) {
		if ((iep->txmask & (1 << i)) == 0)
			continue;
		oep = midi_ep + i;
		avail = oep->obuf.len - oep->obuf.used;
		if (maxavail > avail)
			maxavail = avail;
	}

	/*
	 * in the worst case output message is twice the
	 * input message (2-byte messages with running status)
	 */
	tickets = maxavail / 2 - iep->tickets;
	if (tickets > 0) {
		iep->tickets += tickets;
		iep->ops->fill(iep->arg, tickets);
	}
}

/*
 * recalculate tickets of endpoints sending data to this one
 */
void
midi_fill(struct midi *oep)
{
	int i;
	struct midi *iep;

	for (i = 0; i < MIDI_NEP; i++) {
		iep = midi_ep + i;
		if (iep->txmask & oep->self)
			midi_tickets(iep);
	}
}

/*
 * parse then give data chunk, and calling imsg() for each message
 */
void
midi_in(struct midi *iep, unsigned char *idata, int icount)
{
	int i;
	unsigned char c;

	for (i = 0; i < icount; i++) {
		c = *idata++;
		if (c >= 0xf8) {
			if (c != MIDI_ACK)
				iep->ops->imsg(iep->arg, &c, 1);
		} else if (c == SYSEX_END) {
			if (iep->st == SYSEX_START) {
				iep->msg[iep->idx++] = c;
				iep->ops->imsg(iep->arg, iep->msg, iep->idx);
			}

			/*
			 * There are bogus MIDI sources that keep
			 * state across sysex; Linux virmidi ports fed
			 * by the sequencer is an example. We
			 * workaround this by saving the current
			 * status and restoring it at the end of the
			 * sysex.
			 */
			iep->st = iep->last_st;
			if (iep->st)
				iep->len = voice_len[(iep->st >> 4) & 7];
			iep->idx = 0;
		} else if (c >= 0xf0) {
			iep->msg[0] = c;
			iep->len = common_len[c & 7];
			iep->st = c;
			iep->idx = 1;
		} else if (c >= 0x80) {
			iep->msg[0] = c;
			iep->len = voice_len[(c >> 4) & 7];
			iep->last_st = iep->st = c;
			iep->idx = 1;
		} else if (iep->st) {
			if (iep->idx == 0 && iep->st != SYSEX_START)
				iep->msg[iep->idx++] = iep->st;
			iep->msg[iep->idx++] = c;
			if (iep->idx == iep->len) {
				iep->ops->imsg(iep->arg, iep->msg, iep->idx);
				if (iep->st >= 0xf0)
					iep->st = 0;
				iep->idx = 0;
			} else if (iep->idx == MIDI_MSGMAX) {
				/* sysex continued */
				iep->ops->imsg(iep->arg, iep->msg, iep->idx);
				iep->idx = 0;
			}
		}
	}
	iep->tickets -= icount;
	if (iep->tickets < 0)
		iep->tickets = 0;
	midi_tickets(iep);
}

/*
 * store the given message in the output buffer
 */
void
midi_out(struct midi *oep, unsigned char *idata, int icount)
{
#ifdef DEBUG
	char str[128];
#endif
	unsigned char *odata;
	int ocount;

	while (icount > 0) {
		if (oep->obuf.used == oep->obuf.len) {
#ifdef DEBUG
			logx(2, "midi%u: too slow, discarding %d bytes",
			    oep->num, oep->obuf.used);
#endif
			abuf_rdiscard(&oep->obuf, oep->obuf.used);
			oep->owner = NULL;
			return;
		}
		odata = abuf_wgetblk(&oep->obuf, &ocount);
		if (ocount > icount)
			ocount = icount;
		memcpy(odata, idata, ocount);
#ifdef DEBUG
		logx(4, "midi%u: out: %s", oep->num,
		    (midiev_fmt(str, sizeof(str), odata, ocount), str));
#endif
		abuf_wcommit(&oep->obuf, ocount);
		icount -= ocount;
		idata += ocount;
	}
}

/*
 * disconnect clients attached to this end-point
 */
void
midi_abort(struct midi *p)
{
	int i;
	struct midi *ep;

	for (i = 0; i < MIDI_NEP; i++) {
		ep = midi_ep + i;
		if ((ep->txmask & p->self) || (p->txmask & ep->self))
			ep->ops->exit(ep->arg);
	}
}

void
port_imsg(void *arg, unsigned char *msg, int size)
{
	struct port *p = arg;

	midi_send(p->midi, msg, size);
}


void
port_omsg(void *arg, unsigned char *msg, int size)
{
	struct port *p = arg;

	midi_out(p->midi, msg, size);
}

void
port_fill(void *arg, int count)
{
	/* no flow control */
}

void
port_exit(void *arg)
{
#ifdef DEBUG
	struct port *p = arg;

	logx(0, "midi%u: port exit", p->midi->num);
	panic();
#endif
}

/*
 * create a new midi port
 */
struct port *
port_new(char *path, unsigned int mode, int hold)
{
	struct port *c;

	c = xmalloc(sizeof(struct port));
	c->path = path;
	c->state = PORT_CFG;
	c->hold = hold;
	c->refcnt = 0;
	c->midi = midi_new(&port_midiops, c, mode);
	c->num = midi_portnum++;
	c->next = port_list;
	port_list = c;
	return c;
}

/*
 * destroy the given midi port
 */
void
port_del(struct port *c)
{
	struct port **p;

	if (c->state != PORT_CFG)
		port_close(c);
	midi_del(c->midi);
	for (p = &port_list; *p != c; p = &(*p)->next) {
#ifdef DEBUG
		if (*p == NULL) {
			logx(0, "port to delete not on list");
			panic();
		}
#endif
	}
	*p = c->next;
	xfree(c);
}

int
port_ref(struct port *c)
{
#ifdef DEBUG
	logx(3, "midi%u: port requested", c->midi->num);
#endif
	if (c->state == PORT_CFG && !port_open(c))
		return 0;
	c->refcnt++;
	return 1;
}

void
port_unref(struct port *c)
{
#ifdef DEBUG
	logx(3, "midi%u: port released", c->midi->num);
#endif
	if (--c->refcnt == 0 && c->state == PORT_INIT)
		port_drain(c);
}

struct port *
port_bynum(int num)
{
	struct port *p;

	for (p = port_list; p != NULL; p = p->next) {
		if (p->num == num)
			return p;
	}
	return NULL;
}

int
port_open(struct port *c)
{
	if (!port_mio_open(c)) {
		logx(1, "midi%u: failed to open midi port", c->midi->num);
		return 0;
	}
	c->state = PORT_INIT;
	return 1;
}

int
port_close(struct port *c)
{
#ifdef DEBUG
	if (c->state == PORT_CFG) {
		logx(0, "midi%u: can't close port (not opened)", c->midi->num);
		panic();
	}
#endif
	logx(2, "midi%u: closed", c->midi->num);
	c->state = PORT_CFG;
	port_mio_close(c);
	return 1;
}

void
port_drain(struct port *c)
{
	struct midi *ep = c->midi;

	if (!(ep->mode & MODE_MIDIOUT) || ep->obuf.used == 0)
		port_close(c);
	else {
		c->state = PORT_DRAIN;
#ifdef DEBUG
		logx(3, "midi%u: draining", c->midi->num);
#endif
	}
}

int
port_init(struct port *c)
{
	if (c->hold && !port_ref(c))
		return 0;
	return 1;
}

void
port_done(struct port *c)
{
	if (c->state == PORT_INIT)
		port_drain(c);
}

/*
 * unlink the port from midithru's (but keep it on the prefportmask in case
 * the port is back) and update server.port accordingly
 */
void
port_abort(struct port *p)
{
	struct ctl *c;
	int i;

	for (i = 0; i < MIDITHRU_NMAX; i++) {
		midithru_rm(midithru_array + i, p->midi);

		c = ctl_find(CTL_MIDI_PORT, p, midithru_array + i);
		if (c != NULL && c->curval != 0) {
			c->val_mask = ~0U;
			c->curval = 0;
		}
	}
	midi_abort(p->midi);
}

void
midithru_ref(struct midithru *t)
{
	struct port *c;
	char name[64];

#ifdef DEBUG
	logx(3, "%zu: midithru requested", t - midithru_array);
#endif
	if (t->refcnt++ > 0)
		return;
	for (c = port_list; c != NULL; c = c->next) {
		c->refcnt++;
		if (c->state == DEV_CFG)
			port_open(c);
		if (c->state == DEV_INIT && (t->prefportmask & c->midi->self))
			midithru_addport(t, c);
		snprintf(name, sizeof(name), "%u", c->num);
		ctl_new(CTL_MIDI_PORT, c, t,
		    CTL_LIST, "", "", "server", -1, "port",
		    name, -1, 1, !!(t->portmask & c->midi->self));
	}
}

void
midithru_unref(struct midithru *t)
{
	struct port *c;

#ifdef DEBUG
	logx(3, "%zu: midithru released", t - midithru_array);
#endif
	if (--t->refcnt > 0)
		return;
	/* delete server.port control */
	for (c = port_list; c != NULL; c = c->next) {
		if (ctl_del(CTL_MIDI_PORT, c, t)) {
			midithru_rm(t, c->midi);
			port_unref(c);
		}
	}
}

void
midithru_addport(struct midithru *t, struct port *c)
{
	int i;

	if (c->state == DEV_INIT) {
		for (i = 0; i < MIDI_NEP; i++) {
			if (t->progmask & (1 << i))
				midi_link(midi_ep + i, c->midi);
		}
	}
	t->portmask |= c->midi->self;
}

void
midithru_addprog(struct midithru *t, struct midi *ep)
{
	int i;

	for (i = 0; i < MIDI_NEP; i++) {
		if ((t->portmask | t->progmask) & (1 << i))
			midi_link(ep, midi_ep + i);
	}
	t->progmask |= ep->self;
}

void
midithru_rm(struct midithru *t, struct midi *ep)
{
	int i;

	t->progmask &= ~ep->self;
	t->portmask &= ~ep->self;

	for (i = 0; i < MIDI_NEP; i++) {
		if ((t->portmask | t->progmask) & (1 << i))
			midi_unlink(ep, midi_ep + i);
	}
}

int
midithru_setport(struct midithru *t, struct port *c, int val)
{
	if (val) {
		if (c->state == PORT_CFG && !port_open(c))
			return 0;
		midithru_addport(t, c);
		t->prefportmask |= c->midi->self;
	} else {
		midithru_rm(t, c->midi);
		t->prefportmask &= ~c->midi->self;
	}
	return 1;
}

/*
 * Reopen failed ports.
 */
void
midithru_scanports(void)
{
	struct port *p;
	struct ctl *c;
	int i;

	for (p = port_list; p != NULL; p = p->next) {

		if (p->refcnt == 0 || p->state != PORT_CFG || !port_open(p))
			continue;

		for (i = 0; i < MIDITHRU_NMAX; i++) {
			if (!(midithru_array[i].prefportmask & p->midi->self))
				continue;
			midithru_addport(midithru_array + i, p);
			c = ctl_find(CTL_MIDI_PORT, p, midithru_array + i);
			if (c != NULL && c->curval != 0) {
				c->val_mask = ~0U;
				c->curval = 1;
			}
		}
	}
}
