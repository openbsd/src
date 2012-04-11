/*	$OpenBSD: midi.h,v 1.13 2012/04/11 06:05:43 ratchov Exp $	*/
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
#ifndef MIDI_H
#define MIDI_H

struct dev;

struct aproc *midi_new(char *, struct dev *);

void midi_ontick(struct aproc *, int);
void midi_send_slot(struct aproc *, int);
void midi_send_vol(struct aproc *, int, unsigned int);
void midi_send_master(struct aproc *);
void midi_send_full(struct aproc *, unsigned int, unsigned int,
    unsigned int, unsigned int);
void midi_send_qfr(struct aproc *, unsigned int, int);
void midi_flush(struct aproc *);

#endif /* !defined(MIDI_H) */
