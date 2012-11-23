/*	$OpenBSD: midi.c,v 1.1 2012/11/23 07:03:28 ratchov Exp $	*/
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
#include "defs.h"
#include "dev.h"
#include "file.h"
#include "midi.h"
#include "miofile.h"
#include "sysex.h"
#include "utils.h"

/*
 * input data rate is XFER / TIMO (in bytes per microsecond),
 * it must be slightly larger than the MIDI standard 3125 bytes/s
 */ 
#define MIDI_XFER 1
#define MIDI_TIMO 100000

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
struct timo midi_timo;
struct port *port_list = NULL;
unsigned int midi_portnum = 0;

struct midithru {
	unsigned txmask;
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
midi_ontimo(void *arg)
{
	int i;
	struct midi *ep;
	
	for (i = MIDI_NEP, ep = midi_ep; i > 0; i--, ep++) {
	}
	timo_add(&midi_timo, MIDI_TIMO);
}

void
midi_init(void)
{
	timo_set(&midi_timo, midi_ontimo, NULL);
	timo_add(&midi_timo, MIDI_TIMO);
}

void
midi_done(void)
{
	timo_del(&midi_timo);
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
	ep->rxmask = 1 << i;
	ep->mode = mode;
	/*
	 * client output is our input (ibuf) and our output (obuf) goes
	 * to client input
	 */
	if (ep->mode & MODE_MIDIOUT) {
		abuf_init(&ep->ibuf, MIDI_BUFSZ);
	}
	if (ep->mode & MODE_MIDIIN) {
		abuf_init(&ep->obuf, MIDI_BUFSZ);
	}
	return ep;
}

void
midi_del(struct midi *ep)
{
	int i;

	for (i = 0; i < MIDI_NEP; i++)
		midi_ep[i].txmask &= ~ep->rxmask;
	for (i = 0; i < MIDITHRU_NMAX; i++)
		midithru[i].txmask &= ~ep->rxmask;

	/* XXX: drain output */
	ep->ops = NULL;
	if (ep->mode & MODE_MIDIOUT) {
		abuf_done(&ep->ibuf);
	}
	if (ep->mode & MODE_MIDIIN) {
		abuf_done(&ep->obuf);
	}
}

/*
 * add the midi endpoint in the ``tag'' midi thru box
 */
void
midi_tag(struct midi *ep, unsigned int tag)
{
	int i;
	struct midi *m;
	unsigned members;

	members = midithru[tag].txmask;
	midithru[tag].txmask |= ep->rxmask;

	for (i = 0, m = midi_ep; i < MIDI_NEP; i++, m++) {
		if (!(members & (1 << i)))
			continue;
		if (ep->mode & MODE_MIDIOUT)
			ep->txmask |= m->rxmask;
		if (ep->mode & MODE_MIDIIN)
			m->txmask |= ep->rxmask;
	}
}

/*
 * remove the midi endpoint from the ``tag'' midi thru box
 */
void
midi_untag(struct midi *ep, unsigned int tag)
{
	int i;
	struct midi *m;
	unsigned members;

	members = midithru[tag].txmask;
	midithru[tag].txmask &= ~ep->rxmask;

	for (i = 0, m = midi_ep;; i++, m++) {
		if (!(members & (1 << i)))
			continue;
		ep->txmask &= ~m->rxmask;
		m->txmask &= ~ep->rxmask;
	}
}

/*
 * broadcast the given message to other members of the thru box
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


void
midi_fill(struct midi *oep)
{
	int i, count;
	struct midi *iep;

	for (i = 0; i < MIDI_NEP ; i++) {
		if ((oep->rxmask & (1 << i)) == 0)
			continue;
		iep = midi_ep + i;
		count = midi_in(iep);
		if (count)
			iep->ops->fill(iep->arg, count);
	}
}

/*
 * parse the give data chunk, and calling imsg() for each message
 */
void
midi_parse(struct midi *iep, unsigned char *idata, int icount)
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
}

/*
 * process input data stored in ep->ibuf
 */
int
midi_in(struct midi *iep)
{
	unsigned char *idata;
	int i, icount, maxavail, avail, idone;
	struct midi *oep;

	/*
	 * calculate the max message size we can process
	 */
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
	 * in the works case output message is twice the 
	 * input message (2-byte messages with running status)
	 */
	maxavail /= 2;
	idone = 0;
	for (;;) {
		idata = abuf_rgetblk(&iep->ibuf, &icount);
		if (icount > maxavail)
			icount = maxavail;
		if (icount == 0)
			break;
		maxavail -= icount;
#ifdef DEBUG
		if (log_level >= 4) {
			midi_log(iep);
			log_puts(":  in:");
			for (i = 0; i < icount; i++) {
				log_puts(" ");
				log_putx(idata[i]);
			}
			log_puts("\n");
		}
#endif
		midi_parse(iep, idata, icount);
		abuf_rdiscard(&iep->ibuf, icount);
		idone += icount;
	}
	return idone;
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
				log_puts(": overrun, discarding ");
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
		log_puts(": exit\n");
	}
#endif
}

/*
 * create a new midi port
 */
struct port *
port_new(char *path, unsigned int mode)
{
	struct port *c;

	c = xmalloc(sizeof(struct port));
	c->path = path;
	c->state = PORT_CFG;
	c->midi = midi_new(&port_midiops, c, mode);
	midi_portnum++;
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

struct port *
port_bynum(int num)
{
	struct port *p;

	for (p = port_list; p != NULL; p = p->next) {
		if (num-- == 0)
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
#ifdef DEBUG
	if (c->state == PORT_CFG) {
		port_log(c);
		log_puts(": can't close port (not opened)\n");
	}
#endif
	port_mio_close(c);
	c->state = PORT_CFG;	
	return 1;
}

int
port_init(struct port *c)
{
	return port_open(c);
}

void
port_done(struct port *c)
{
	/* XXX: drain? */
	if (c->state != PORT_CFG)
		port_close(c);
}
