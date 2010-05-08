/*	$OpenBSD: dev.h,v 1.23 2010/05/08 15:35:45 ratchov Exp $	*/
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
#ifndef DEV_H
#define DEV_H

struct aproc;
struct aparams;
struct abuf;

extern unsigned dev_reqprime;
extern unsigned dev_bufsz, dev_round, dev_rate;
extern struct aparams dev_ipar, dev_opar;
extern struct aproc *dev_mix, *dev_sub, *dev_midi, *dev_submon, *dev_mon;

int dev_run(void);
int dev_open(void);
void dev_close(void);
int dev_ref(void);
void dev_unref(void);
void dev_done(void);
void dev_wakeup(void);
void dev_init_thru(void);
void dev_init_loop(struct aparams *, struct aparams *, unsigned);
void dev_init_sio(char *, unsigned,
    struct aparams *, struct aparams *, unsigned, unsigned);
int  dev_thruadd(char *, int, int);
void dev_midiattach(struct abuf *, struct abuf *);
unsigned dev_roundof(unsigned);
int dev_getpos(void);
void dev_attach(char *, unsigned,
    struct abuf *, struct aparams *, unsigned,
    struct abuf *, struct aparams *, unsigned,
    unsigned, int);
void dev_setvol(struct abuf *, int);

#endif /* !define(DEV_H) */
