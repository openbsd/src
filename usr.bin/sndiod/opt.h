/*	$OpenBSD: opt.h,v 1.11 2025/11/26 08:40:16 ratchov Exp $	*/
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
#ifndef OPT_H
#define OPT_H

#define OPT_NMAX		16
#define OPT_NAPP		8

struct dev;

struct app {
#define APP_NAMEMAX	12
	char name[APP_NAMEMAX];		/* name matching [a-z]+ */
	unsigned int serial;		/* global unique number */
	int vol;
};

struct opt_alt {
	struct opt_alt *next;
	struct dev *dev;
};

struct opt {
	struct opt *next;
	struct dev *dev;
	struct opt_alt *alt_list;
	struct midi *midi;
	struct mtc *mtc;	/* if set, MMC-controlled MTC source */

	struct app app_array[OPT_NAPP];
	unsigned int app_serial;

	int num;
#define OPT_NAMEMAX 11
	char name[OPT_NAMEMAX + 1];
	int maxweight;		/* max dynamic range for clients */
	int pmin, pmax;		/* play channels */
	int rmin, rmax;		/* recording channels */
	int dup;		/* true if join/expand enabled */
	int mode;		/* bitmap of MODE_XXX */
	int refcnt;
};

extern struct opt *opt_list;

struct app *opt_mkapp(struct opt *o, char *who);
void opt_appvol(struct opt *o, struct app *a, int vol);
void opt_midi_vol(struct opt *, struct app *);
void opt_midi_appdesc(struct opt *o, struct app *a);
void opt_midi_dump(struct opt *o);
struct opt *opt_new(struct dev *, char *, int, int, int, int,
    int, int, int, unsigned int);
void opt_del(struct opt *);
void opt_setalt(struct opt *, struct dev *);
struct opt *opt_byname(char *);
struct opt *opt_bynum(int);
void opt_init(struct opt *);
void opt_done(struct opt *);
int opt_setdev(struct opt *, struct dev *);
void opt_migrate(struct opt *, struct dev *);
struct dev *opt_ref(struct opt *);
void opt_unref(struct opt *);

#endif /* !defined(OPT_H) */
