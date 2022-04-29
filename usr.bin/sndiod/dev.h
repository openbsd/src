/*	$OpenBSD: dev.h,v 1.42 2022/04/29 09:12:57 ratchov Exp $	*/
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
#include "dev_sioctl.h"
#include "opt.h"

/*
 * preallocated audio clients
 */
#define DEV_NSLOT	8

/*
 * preallocated control clients
 */
#define DEV_NCTLSLOT 8

/*
 * audio stream state structure
 */

struct slotops
{
	void (*onmove)(void *);			/* clock tick */
	void (*onvol)(void *);			/* tell client vol changed */
	void (*fill)(void *);			/* request to fill a play block */
	void (*flush)(void *);			/* request to flush a rec block */
	void (*eof)(void *);			/* notify that play drained */
	void (*exit)(void *);			/* delete client */
};

struct ctlops
{
	void (*exit)(void *);			/* delete client */
	void (*sync)(void *);			/* description ready */
};

struct slot {
	struct slotops *ops;			/* client callbacks */
	struct slot *next;			/* next on the play list */
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
	((s)->appbufsz + (s)->opt->dev->bufsz / (s)->opt->dev->round * (s)->round)
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
	unsigned int id;			/* process id */
};

/*
 * subset of channels of a stream
 */

struct ctl {
	struct ctl *next;

#define CTL_NONE	0		/* deleted */
#define CTL_NUM		2		/* number (aka integer value) */
#define CTL_SW		3		/* on/off switch, only bit 7 counts */
#define CTL_VEC		4		/* number, element of vector */
#define CTL_LIST	5		/* switch, element of a list */
#define CTL_SEL		6		/* element of a selector */
	unsigned int type;		/* one of above */

#define CTL_HW		0
#define CTL_DEV_MASTER	1
#define CTL_OPT_DEV	2
#define CTL_SLOT_LEVEL	3
	unsigned int scope;
	union {
		struct {
			void *arg0;
			void *arg1;
		} any;
		struct {
			struct dev *dev;
			unsigned int addr;
		} hw;
		struct {
			struct dev *dev;
		} dev_master;
		struct {
			struct slot *slot;
		} slot_level;
		struct {
			struct slot *slot;
			struct opt *opt;
		} slot_opt;
		struct {
			struct opt *opt;
			struct dev *dev;
		} opt_dev;
	} u;

	unsigned int addr;		/* slot side control address */
#define CTL_NAMEMAX	16		/* max name lenght */
	char func[CTL_NAMEMAX];		/* parameter function name */
	char group[CTL_NAMEMAX];	/* group aka namespace */
	struct ctl_node {
		char name[CTL_NAMEMAX];	/* stream name */
		int unit;
	} node0, node1;			/* affected channels */
#define CTL_DEVMASK		(1 << 31)
#define CTL_SLOTMASK(i)		(1 << (i))
	unsigned int val_mask;
	unsigned int desc_mask;
	unsigned int refs_mask;
	unsigned int maxval;
	unsigned int curval;
	int dirty;
};

struct ctlslot {
	struct ctlops *ops;
	void *arg;
	struct opt *opt;
	unsigned int self;		/* equal to (1 << index) */
	unsigned int mode;
};

/*
 * MIDI time code (MTC)
 */
struct mtc {
	/*
	 * MIDI time code (MTC) states
	 */
#define MTC_STOP	1		/* stopped, can't start */
#define MTC_START	2		/* attempting to start */
#define MTC_RUN		3		/* started */
	unsigned int tstate;		/* one of MTC_* constants */
	struct dev *dev;

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
};

/*
 * audio device with plenty of slots
 */
struct dev {
	struct dev *next;
	struct slot *slot_list;			/* audio streams attached */

	/*
	 * name used for various controls
	 */
	char name[CTL_NAMEMAX];

	/*
	 * next to try if this fails
	 */
	struct dev *alt_next;

	/*
	 * audio device (while opened)
	 */
	struct dev_sio sio;
	struct dev_sioctl sioctl;
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
	char *path;

	/*
	 * actual parameters and runtime state (i.e. once opened)
	 */
	unsigned int mode;			/* bitmap of MODE_xxx */
	unsigned int bufsz, round, rate;
	unsigned int prime;
	unsigned int idle;			/* cycles with no client */

	unsigned int master;			/* software vol. knob */
	unsigned int master_enabled;		/* 1 if h/w has no vo. knob */
};

extern struct dev *dev_list;
extern struct ctl *ctl_list;
extern struct slot slot_array[DEV_NSLOT];
extern struct ctlslot ctlslot_array[DEV_NCTLSLOT];
extern struct mtc mtc_array[1];

void slot_array_init(void);

void dev_log(struct dev *);
int dev_open(struct dev *);
void dev_close(struct dev *);
void dev_abort(struct dev *);
struct dev *dev_migrate(struct dev *);
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
int dev_iscompat(struct dev *, struct dev *);

/*
 * interface to hardware device
 */
void dev_onmove(struct dev *, int);
void dev_cycle(struct dev *);

/*
 * midi & midi call-backs
 */
void dev_master(struct dev *, unsigned int);
void dev_midi_send(struct dev *, void *, int);
void dev_midi_vol(struct dev *, struct slot *);
void dev_midi_master(struct dev *);
void dev_midi_slotdesc(struct dev *, struct slot *);
void dev_midi_dump(struct dev *);

void mtc_midi_qfr(struct mtc *, int);
void mtc_midi_full(struct mtc *);
void mtc_trigger(struct mtc *);
void mtc_start(struct mtc *);
void mtc_stop(struct mtc *);
void mtc_loc(struct mtc *, unsigned int);
void mtc_setdev(struct mtc *, struct dev *);

/*
 * sio_open(3) like interface for clients
 */
void slot_log(struct slot *);
struct slot *slot_new(struct opt *, unsigned int, char *,
    struct slotops *, void *, int);
void slot_del(struct slot *);
void slot_setvol(struct slot *, unsigned int);
void slot_setopt(struct slot *, struct opt *);
void slot_start(struct slot *);
void slot_stop(struct slot *, int);
void slot_read(struct slot *);
void slot_write(struct slot *);
void slot_initconv(struct slot *);
void slot_attach(struct slot *);
void slot_detach(struct slot *);

/*
 * control related functions
 */

struct ctl *ctl_new(int, void *, void *,
    int, char *, char *, int, char *, char *, int, int, int);
void ctl_del(int, void *, void *);
void ctl_log(struct ctl *);
int ctl_setval(struct ctl *c, int val);
int ctl_match(struct ctl *, int, void *, void *);
struct ctl *ctl_find(int, void *, void *);
void ctl_update(struct ctl *);
int ctl_onval(int, void *, void *, int);

struct ctlslot *ctlslot_new(struct opt *, struct ctlops *, void *);
void ctlslot_del(struct ctlslot *);
int ctlslot_visible(struct ctlslot *, struct ctl *);
struct ctl *ctlslot_lookup(struct ctlslot *, int);
void ctlslot_update(struct ctlslot *);

void dev_label(struct dev *, int);
void dev_ctlsync(struct dev *);

#endif /* !defined(DEV_H) */
