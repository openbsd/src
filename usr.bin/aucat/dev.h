/*	$OpenBSD: dev.h,v 1.36 2012/04/11 06:05:43 ratchov Exp $	*/
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

#include "aparams.h"

struct aproc;
struct abuf;

struct dev {
	struct dev *next;

	/*
	 * desired parameters
	 */
	unsigned int reqmode;			/* mode */
	struct aparams reqipar, reqopar;	/* parameters */
	unsigned int reqbufsz;			/* buffer size */
	unsigned int reqround;			/* block size */
	unsigned int hold;				/* hold the device open ? */
	unsigned int autovol;			/* auto adjust playvol ? */
	unsigned int autostart;			/* don't wait for MMC start */
	unsigned int refcnt;			/* number of openers */
#define DEV_NMAX	16			/* max number of devices */
	unsigned int num;				/* serial number */
#define DEV_CLOSED	0			/* closed */
#define DEV_INIT	1			/* stopped */
#define DEV_START	2			/* ready to start */
#define DEV_RUN		3			/* started */
	unsigned int pstate;			/* on of DEV_xxx */
	char *path;				/* sio path */

	/*
	 * actual parameters and runtime state (i.e. once opened)
	 */
	unsigned int mode;				/* bitmap of MODE_xxx */
	unsigned int bufsz, round, rate;
	struct aparams ipar, opar;
	struct aproc *mix, *sub, *submon;
	struct aproc *rec, *play, *mon;
	struct aproc *midi;
	struct devctl {
		struct devctl *next;
		unsigned int mode;
		char *path;
	} *ctl_list;

	/* volume control and MMC/MTC */
#define CTL_NSLOT	8
#define CTL_NAMEMAX	8
	unsigned int serial;
	struct ctl_slot {
		struct ctl_ops {
			void (*vol)(void *, unsigned int);
			void (*start)(void *);
			void (*stop)(void *);
			void (*loc)(void *, unsigned int);
			void (*quit)(void *);
		} *ops;
		void *arg;
		unsigned int unit;
		char name[CTL_NAMEMAX];
		unsigned int serial;
		unsigned int vol;
		unsigned int tstate;
	} slot[CTL_NSLOT];
#define CTL_OFF		0			/* ignore MMC messages */
#define CTL_STOP	1			/* stopped, can't start */
#define CTL_START	2			/* attempting to start */
#define CTL_RUN		3			/* started */
	unsigned int tstate;			/* one of above */
	unsigned int origin;			/* MTC start time */
	unsigned int master;			/* master volume controller */
};

extern struct dev *dev_list;

void dev_dbg(struct dev *);
int  dev_init(struct dev *);
int  dev_run(struct dev *);
int  dev_ref(struct dev *);
void dev_unref(struct dev *);
void dev_del(struct dev *);
void dev_wakeup(struct dev *);
void dev_drain(struct dev *);
struct dev *dev_new(char *, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int);
void dev_adjpar(struct dev *, unsigned int,
    struct aparams *, struct aparams *);
int  devctl_add(struct dev *, char *, unsigned int);
void dev_midiattach(struct dev *, struct abuf *, struct abuf *);
unsigned int dev_roundof(struct dev *, unsigned int);
int dev_getpos(struct dev *);
void dev_attach(struct dev *, char *, unsigned int,
    struct abuf *, struct aparams *, unsigned int,
    struct abuf *, struct aparams *, unsigned int,
    unsigned int, int);
void dev_setvol(struct dev *, struct abuf *, int);

void dev_slotdbg(struct dev *, int);
int  dev_slotnew(struct dev *, char *, struct ctl_ops *, void *, int);
void dev_slotdel(struct dev *, int);
void dev_slotvol(struct dev *, int, unsigned int);

int  dev_slotstart(struct dev *, int);
void dev_slotstop(struct dev *, int);
void dev_mmcstart(struct dev *);
void dev_mmcstop(struct dev *);
void dev_loc(struct dev *, unsigned int);
void dev_master(struct dev *, unsigned int);

#endif /* !define(DEV_H) */
