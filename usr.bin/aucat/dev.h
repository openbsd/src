/*	$OpenBSD: dev.h,v 1.18 2010/04/03 17:40:33 ratchov Exp $	*/
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

#define DEV_INIT	0
#define DEV_START	1
#define DEV_RUN		2
#define DEV_STOP	3

extern unsigned dev_pstate;
extern unsigned dev_bufsz, dev_round, dev_rate;
extern struct aparams dev_ipar, dev_opar;
extern struct aproc *dev_mix, *dev_sub, *dev_midi, *dev_submon, *dev_mon;

void dev_thruinit(void);
int  dev_thruadd(char *, int, int);
void dev_midiattach(struct abuf *, struct abuf *);
unsigned dev_roundof(unsigned);
void dev_loopinit(struct aparams *, struct aparams *, unsigned);
int  dev_init(char *, unsigned,
    struct aparams *, struct aparams *, unsigned, unsigned);
void dev_start(void);
void dev_stop(void);
void dev_run(int);
void dev_done(void);
int  dev_getep(unsigned, struct abuf **, struct abuf **);
void dev_sync(unsigned, struct abuf *, struct abuf *);
unsigned dev_getmode(void);
int dev_getpos(void);
void dev_attach(char *, unsigned,
    struct abuf *, struct aparams *,
    struct abuf *, struct aparams *, unsigned, int);
void dev_setvol(struct abuf *, int);
void dev_clear(void);
void dev_prime(void);

#endif /* !define(DEV_H) */
