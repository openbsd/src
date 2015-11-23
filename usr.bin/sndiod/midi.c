/*	$OpenBSD: midi.c,v 1.12 2015/11/23 09:48:25 ratchov Exp $	*/
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

struct midithru {
	unsigned int txmask, rxmask;
#define MIDITHRU_NMAX 32
} midithru[MIDITHRU_NMAX];

/*
 * length of voice and common messages (status byte included)
 */
unsigned int voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
unsigned int common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

void
midi_log(struct midi *ep)
{
	log_puts("midi");
	log_putu(ep - midi_ep);
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
	ep->txmask = 0;
	ep->self = 1 << i;
	ep->tickets = 0;
	ep->mode = mode;

	/*
	 * the output buffer is the client intput
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
		midithru[i].txmask &= ~ep->self;
		midithru[i].rxmask &= ~ep->self;
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
	if (ep->mode & MODE_MIDIOUT) {
		ep->txmask |= peer->self;
		midi_tickets(ep);
	}
	if (ep->mode & MODE_MIDIIN) {
#ifdef DEBUG
		if (ep->obuf.used > 0) {
			midi_log(ep);
			log_puts(": linked with non-empty buffer\n");
			panic();
		}
#endif
		/* ep has empry buffer, so no need to call midi_tickets() */
		peer->txmask |= ep->self;
	}
}

/*
 * add the midi endpoint in the ``tag'' midi thru box
 */
void
midi_tag(struct midi *ep, unsigned int tag)
{
	struct midi *peer;
	struct midithru *t = midithru + tag;
	int i;

	if (ep->mode & MODE_MIDIOUT) {
		ep->txmask |= t->txmask;
		midi_tickets(ep);
	}
	if (ep->mode & MODE_MIDIIN) {
#ifdef DEBUG
		if (ep->obuf.used > 0) {
			midi_log(ep);
			log_puts(": tagged with non-empty buffer\n");
			panic();
		}
#endif
		for (i = 0; i < MIDI_NEP; i++) {
			if (!(t->rxmask & (1 << i)))
				continue;
			peer = midi_ep + i;
			peer->txmask |= ep->self;
		}
	}
	if (ep->mode & MODE_MIDIOUT)
		t->rxmask |= ep->self;
	if (ep->mode & MODE_MIDIIN)
		t->txmask |= ep->self;
}

/*
 * broadcast the given message to other endpoints
 */
void
midi_send(struct midi *iep, unsigned char *msg, int size)
{
	struct midi *oep;
	int i;

#ifdef DEBUG
	if (log_level >= 4) {
		midi_log(iep);
		log_puts(": sending:");
		for (i = 0; i < size; i++) {
			log_puts(" ");
			log_putx(msg[i]);
		}
		log_puts("\n");
	}
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
		if (log_level >= 4) {
			midi_log(iep);
			log_puts(" -> ");
			midi_log(oep);
			log_puts("\n");
		}
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
			iep->st = 0;
			iep->idx = 0;
		} else if (c >= 0xf0) {
			iep->msg[0] = c;
			iep->len = common_len[c & 7];
			iep->st = c;
			iep->idx = 1;
		} else if (c >= 0x80) {
			iep->msg[0] = c;
			iep->len = voice_len[(c >> 4) & 7];
			iep->st = c;
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
	unsigned char *odata;
	int ocount;
#ifdef DEBUG
	int i;
#endif
	
	while (icount > 0) {
		if (oep->obuf.used == oep->obuf.len) {
#ifdef DEBUG
			if (log_level >= 2) {
				midi_log(oep);
				log_puts(": too slow, discarding ");
				log_putu(oep->obuf.used);
				log_puts(" bytes\n");
			}
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
		if (log_level >= 4) {
			midi_log(oep);
			log_puts(": out: ");
			for (i = 0; i < ocount; i++) {
				log_puts(" ");
				log_putx(odata[i]);
			}
			log_puts("\n");
		}
#endif
		abuf_wcommit(&oep->obuf, ocount);
		icount -= ocount;
		idata += ocount;
	}
}

#ifdef DEBUG
void
port_log(struct port *p)
{
	midi_log(p->midi);
}
#endif

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

	if (log_level >= 3) {
		port_log(p);
		log_puts(": port exit\n");
		panic();
	}
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
			log_puts("port to delete not on list\n");
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
	if (log_level >= 3) {
		port_log(c);
		log_puts(": port requested\n");
	}
#endif
	if (c->state == PORT_CFG && !port_open(c))
		return 0;
	return 1;
}

void
port_unref(struct port *c)
{
	int i, rxmask;

#ifdef DEBUG
	if (log_level >= 3) {
		port_log(c);
		log_puts(": port released\n");
	}
#endif
	for (rxmask = 0, i = 0; i < MIDI_NEP; i++)
		rxmask |= midi_ep[i].txmask;
	if ((rxmask & c->midi->self) == 0 && c->midi->txmask == 0 &&
	    c->state == PORT_INIT && !c->hold)
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
		if (log_level >= 1) {
			log_puts(c->path);
			log_puts(": failed to open midi port\n");
		}
		return 0;
	}
	c->state = PORT_INIT;
	return 1;
}

int
port_close(struct port *c)
{
	int i;
	struct midi *ep;
#ifdef DEBUG
	if (c->state == PORT_CFG) {
		port_log(c);
		log_puts(": can't close port (not opened)\n");
		panic();
	}
#endif
	c->state = PORT_CFG;	
	port_mio_close(c);
	
	for (i = 0; i < MIDI_NEP; i++) {
		ep = midi_ep + i;
		if ((ep->txmask & c->midi->self) ||
		    (c->midi->txmask & ep->self))
			ep->ops->exit(ep->arg);
	}
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
		if (log_level >= 3) {
			port_log(c);
			log_puts(": draining\n");
		}
#endif
	}
}

int
port_init(struct port *c)
{
	if (c->hold)
		return port_open(c);
	return 1;
}

void
port_done(struct port *c)
{
	if (c->state == PORT_INIT)
		port_drain(c);
}
