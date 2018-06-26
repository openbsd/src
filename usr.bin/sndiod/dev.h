/*	$OpenBSD: dev.h,v 1.20 2018/06/26 07:44:35 ratchov Exp $	*/
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
#ifndef DEV_H
#define DEV_H

#include "abuf.h"
#include "dsp.h"
#include "siofile.h"

/*
 * audio stream state structure
 */

struct slotops
{
	void (*onmove)(void *);			/* clock tick */
	void (*onvol)(void *);	        /* tell client vol changed */
	void (*fill)(void *);			/* request to fill a play block */
	void (*flush)(void *);			/* request to flush a rec block */
	void (*eof)(void *);			/* notify that play drained */
	void (*exit)(void *);			/* delete client */
};

struct slot {
	struct slotops *ops;			/* client callbacks */
	struct slot *next;			/* next on the play list */
	struct dev *dev;			/* device this belongs to */
	struct opt *opt;			/* config used */
	void *arg;				/* user data for callbacks */
	struct aparams par;			/* socket side params */
	struct {
		int weight;			/* dynamic range */
		unsigned int vol;		/* volume within the vol */
		struct abuf buf;		/* socket side buffer */
		int bpf;			/* byte per frame */
		int nch;			/* number of play chans */
		struct cmap cmap;		/* channel mapper state */
		struct resamp resamp;		/* resampler state */
		struct conv dec;		/* format decoder params */
		int join;			/* channel join factor */
		int expand;			/* channel expand factor */
		void *resampbuf, *decbuf;	/* tmp buffers */
	} mix;
	struct {
		struct abuf buf;		/* socket side buffer */
		int prime;			/* initial cycles to skip */
		int bpf;			/* byte per frame */
		int nch;			/* number of rec chans */
		struct cmap cmap;		/* channel mapper state */
		struct resamp resamp;		/* buffer for resampling */
		struct conv enc;		/* buffer for encoding */
		int join;			/* channel join factor */
		int expand;			/* channel expand factor */
		void *resampbuf, *encbuf;	/* tmp buffers */
	} sub;
	int xrun;				/* underrun policy */
	int skip;				/* cycles to skip (for xrun) */
#define SLOT_BUFSZ(s) \
	((s)->appbufsz + (s)->dev->bufsz / (s)->dev->round * (s)->round)
	int appbufsz;				/* slot-side buffer size */
	int round;				/* slot-side block size */
	int rate;				/* slot-side sample rate */
	int delta;				/* pending clock ticks */
	int delta_rem;				/* remainder for delta */
	int mode;				/* MODE_{PLAY,REC} */
#define SLOT_INIT	0			/* not trying to do anything */
#define SLOT_START	1			/* buffer allocated */
#define SLOT_READY	2			/* buffer filled enough */
#define SLOT_RUN	3			/* buffer attached to device */
#define SLOT_STOP	4			/* draining */
	int pstate;

#define SLOT_NAMEMAX	8
	char name[SLOT_NAMEMAX];		/* name matching [a-z]+ */
	unsigned int unit;			/* instance of name */
	unsigned int serial;			/* global unique number */
	unsigned int vol;			/* current (midi) volume */
};

struct opt {
	struct opt *next;
#define OPT_NAMEMAX 11
	char name[OPT_NAMEMAX + 1];
	int maxweight;		/* max dynamic range for clients */
	int pmin, pmax;		/* play channels */
	int rmin, rmax;		/* recording channels */
	int mmc;		/* true if MMC control enabled */
	int dup;		/* true if join/expand enabled */
	int mode;		/* bitmap of MODE_XXX */
};

/*
 * audio device with plenty of slots
 */
struct dev {
	struct dev *next;
	struct slot *slot_list;			/* audio streams attached */
	struct opt *opt_list;
	struct midi *midi;

	/*
	 * audio device (while opened)
	 */
	struct dev_sio sio;
	struct aparams par;			/* encoding */
	int pchan, rchan;			/* play & rec channels */
	adata_t *rbuf;				/* rec buffer */
	adata_t *pbuf;				/* array of play buffers */
#define DEV_PBUF(d) ((d)->pbuf + (d)->poffs * (d)->pchan)
	int poffs;				/* index of current play buf */
	int psize;				/* size of play buffer */
	struct conv enc;			/* native->device format */
	struct conv dec;			/* device->native format */
	unsigned char *encbuf;			/* buffer for encoding */
	unsigned char *decbuf;			/* buffer for decoding */

	/*
	 * preallocated audio sub-devices
	 */
#define DEV_NSLOT	8
	struct slot slot[DEV_NSLOT];
	unsigned int serial;			/* for slot allocation */

	/*
	 * current position, relative to the current cycle
	 */
	int delta;

	/*
	 * desired parameters
	 */
	unsigned int reqmode;			/* mode */
	struct aparams reqpar;			/* parameters */
	int reqpchan, reqrchan;			/* play & rec chans */
	unsigned int reqbufsz;			/* buffer size */
	unsigned int reqround;			/* block size */
	unsigned int reqrate;			/* sample rate */
	unsigned int hold;			/* hold the device open ? */
	unsigned int autovol;			/* auto adjust playvol ? */
	unsigned int refcnt;			/* number of openers */
#define DEV_NMAX	16			/* max number of devices */
	unsigned int num;			/* device serial number */
#define DEV_CFG		0			/* closed */
#define DEV_INIT	1			/* stopped */
#define DEV_RUN		2			/* playin & recording */
	unsigned int pstate;			/* one of above */
	char *path;				/* sio path */

	/*
	 * actual parameters and runtime state (i.e. once opened)
	 */
	unsigned int mode;			/* bitmap of MODE_xxx */
	unsigned int bufsz, round, rate;
	unsigned int prime;

	/*
	 * MIDI time code (MTC)
	 */
	struct {
		unsigned int origin;		/* MTC start time */
		unsigned int fps;		/* MTC frames per second */
#define MTC_FPS_24	0
#define MTC_FPS_25	1
#define MTC_FPS_30	3
		unsigned int fps_id;		/* one of above */
		unsigned int hr;		/* MTC hours */
		unsigned int min;		/* MTC minutes */
		unsigned int sec;		/* MTC seconds */
		unsigned int fr;		/* MTC frames */
		unsigned int qfr;		/* MTC quarter frames */
		int delta;			/* rel. to the last MTC tick */
		int refs;
	} mtc;

	/*
	 * MIDI machine control (MMC)
	 */
#define MMC_STOP	1			/* stopped, can't start */
#define MMC_START	2			/* attempting to start */
#define MMC_RUN		3			/* started */
	unsigned int tstate;			/* one of above */
	unsigned int master;			/* master volume controller */
};

extern struct dev *dev_list;

void dev_log(struct dev *);
void dev_close(struct dev *);
struct dev *dev_new(char *, struct aparams *, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, unsigned int);
struct dev *dev_bynum(int);
void dev_del(struct dev *);
void dev_adjpar(struct dev *, int, int, int);
int  dev_init(struct dev *);
void dev_done(struct dev *);
int dev_ref(struct dev *);
void dev_unref(struct dev *);
int  dev_getpos(struct dev *);
unsigned int dev_roundof(struct dev *, unsigned int);

/*
 * interface to hardware device
 */
void dev_onmove(struct dev *, int);
void dev_cycle(struct dev *);

/*
 * midi & midi call-backs
 */
void dev_mmcstart(struct dev *);
void dev_mmcstop(struct dev *);
void dev_mmcloc(struct dev *, unsigned int);
void dev_master(struct dev *, unsigned int);
void dev_midi_vol(struct dev *, struct slot *);

/*
 * sio_open(3) like interface for clients
 */
void slot_log(struct slot *);
struct slot *slot_new(struct dev *, struct opt *, char *,
    struct slotops *, void *, int);
void slot_del(struct slot *);
void slot_setvol(struct slot *, unsigned int);
void slot_start(struct slot *);
void slot_stop(struct slot *);
void slot_read(struct slot *);
void slot_write(struct slot *);

#endif /* !defined(DEV_H) */
