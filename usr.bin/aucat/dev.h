/*	$OpenBSD: dev.h,v 1.2 2008/08/14 09:58:55 ratchov Exp $	*/
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
struct file;
struct abuf;

extern unsigned dev_infr, dev_onfr;
extern struct aparams dev_ipar, dev_opar;
extern struct aproc *dev_mix, *dev_sub, *dev_rec, *dev_play;
extern struct file  *dev_file;

void dev_fill(void);
void dev_flush(void);
void dev_init(char *, struct aparams *, struct aparams *);
void dev_start(void);
void dev_stop(void);
void dev_run(int);
void dev_done(void);
void dev_attach(char *,
    struct abuf *, struct aparams *, unsigned,
    struct abuf *, struct aparams *, unsigned);

struct devops {
	int (*open)(char *, struct aparams *, struct aparams *,
	    unsigned *, unsigned *);
	void (*close)(int);
	void (*start)(int);
	void (*stop)(int);
};

extern struct devops *devops, devops_sun;

/*
 * Sun API specific functions
 */
struct audio_prinfo;
int sun_infotopar(struct audio_prinfo *, struct aparams *);
void sun_partoinfo(struct audio_prinfo *, struct aparams *);

#endif /* !define(DEV_H) */
