/*	$OpenBSD: midi.h,v 1.7 2015/11/22 16:42:22 ratchov Exp $	*/
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
#ifndef MIDI_H
#define MIDI_H

#include "abuf.h"
#include "miofile.h"

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
#define MIDI_CTL_VOL	7		/* volume */

/*
 * Max coarse value
 */
#define MIDI_MAXCTL		127

/*
 * midi stream state structure
 */

struct midiops
{
	void (*imsg)(void *, unsigned char *, int);
	void (*omsg)(void *, unsigned char *, int);
	void (*fill)(void *, int);
	void (*exit)(void *);
};

struct midi {
	struct midiops *ops;		/* port/sock/dev callbacks */
	struct midi *owner;		/* current writer stream */
	unsigned int mode;		/* MODE_{MIDIIN,MIDIOUT} */
	void *arg;			/* user data for callbacks */
#define MIDI_MSGMAX	16		/* max size of MIDI msg */
	unsigned char msg[MIDI_MSGMAX];	/* parsed input message */
	unsigned int st;		/* input MIDI running status */
	unsigned int used;		/* bytes used in ``msg'' */
	unsigned int idx;		/* current ``msg'' size */
	unsigned int len;		/* expected ``msg'' length */
	unsigned int txmask;		/* list of ep we send to */
	unsigned int self;		/* equal (1 << index) */
	unsigned int tickets;		/* max bytes we can process */
	struct abuf obuf;		/* output buffer */
};

/*
 * midi port
 */
struct port {
	struct port *next;
	struct port_mio mio;
#define PORT_CFG	0
#define PORT_INIT	1
#define PORT_DRAIN	2
	unsigned int state;
	unsigned int num;		/* port serial number */
	char *path;			/* hold the port open ? */
	int hold;
	struct midi *midi;
};

/*
 * midi control ports
 */
extern struct port *port_list;

void midi_init(void);
void midi_done(void);
struct midi *midi_new(struct midiops *, void *, int);
void midi_del(struct midi *);
void midi_log(struct midi *);
void midi_tickets(struct midi *);
void midi_in(struct midi *, unsigned char *, int);
void midi_out(struct midi *, unsigned char *, int);
void midi_send(struct midi *, unsigned char *, int);
void midi_fill(struct midi *);
void midi_tag(struct midi *, unsigned int);
void midi_link(struct midi *, struct midi *);

void port_log(struct port *);
struct port *port_new(char *, unsigned int, int);
struct port *port_bynum(int);
void port_del(struct port *);
int  port_ref(struct port *);
void port_unref(struct port *);
int  port_init(struct port *);
void port_done(struct port *);
void port_drain(struct port *);
int  port_close(struct port *);

#endif /* !defined(MIDI_H) */
