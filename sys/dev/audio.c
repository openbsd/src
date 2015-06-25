/*	$OpenBSD: audio.c,v 1.132 2015/06/25 06:43:45 ratchov Exp $	*/
/*
 * Copyright (c) 2015 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/task.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include "audio.h"
#include "wskbd.h"

#ifdef AUDIO_DEBUG
#define DPRINTF(...)				\
	do {					\
		if (audio_debug)		\
			printf(__VA_ARGS__);	\
	} while(0)
#define DPRINTFN(n, ...)			\
	do {					\
		if (audio_debug > (n))		\
			printf(__VA_ARGS__);	\
	} while(0)
#else
#define DPRINTF(...) do {} while(0)
#define DPRINTFN(n, ...) do {} while(0)
#endif

#define DEVNAME(sc)		((sc)->dev.dv_xname)
#define AUDIO_UNIT(n)		(minor(n) & 0x0f)
#define AUDIO_DEV(n)		(minor(n) & 0xf0)
#define AUDIO_DEV_SOUND		0	/* minor of /dev/sound0 */
#define AUDIO_DEV_MIXER		0x10	/* minor of /dev/mixer0 */
#define AUDIO_DEV_AUDIO		0x80	/* minor of /dev/audio0 */
#define AUDIO_DEV_AUDIOCTL	0xc0	/* minor of /dev/audioctl */
#define AUDIO_BUFSZ		65536	/* buffer size in bytes */

/*
 * dma buffer
 */
struct audio_buf {
	unsigned char *data;		/* DMA memory block */
	size_t datalen;			/* size of DMA memory block */
	size_t len;			/* size of DMA FIFO */
	size_t start;			/* first byte used in the FIFO */
	size_t used;			/* bytes used in the FIFO */
	size_t blksz;			/* DMA block size */
	unsigned long pos;		/* bytes transferred */
	unsigned long xrun;		/* bytes lost by xruns */
	struct selinfo sel;		/* to record & wakeup poll(2) */
	int blocking;			/* read/write blocking */
};

#if NWSKBD > 0
struct wskbd_vol
{
	int val;			/* index of the value control */
	int mute;			/* index of the mute control */
	int step;			/* increment/decrement step */
	int nch;			/* channels in the value control */
	int val_pending;		/* pending change of val */
	int mute_pending;		/* pending mute toggles */
};
#endif

/*
 * device structure
 */
struct audio_softc {
	struct device dev;
	struct audio_hw_if *ops;	/* driver funcs */
	void *arg;			/* first arg to driver funcs */
	int mode;			/* bitmask of AUMODE_* */
	int quiesce;			/* device suspended */
	struct audio_buf play, rec;
	unsigned int sw_enc;		/* user exposed AUDIO_ENCODING_* */
	unsigned int hw_enc;		/* harware AUDIO_ENCODING_* */
	unsigned int bits;		/* bits per sample */
	unsigned int bps;		/* bytes-per-sample */
	unsigned int msb;		/* sample are MSB aligned */
	unsigned int rate;		/* rate in Hz */
	unsigned int round;		/* block size in frames */
	unsigned int nblks;		/* number of play blocks */
	unsigned int pchan, rchan;	/* number of channels */
	unsigned char silence[4];	/* a sample of silence */
	int pause;			/* not trying to start DMA */
	int active;			/* DMA in process */
	void (*conv_enc)(unsigned char *, int);	/* encode to native */
	void (*conv_dec)(unsigned char *, int);	/* decode to user */
#if NWSKBD > 0
	struct wskbd_vol spkr, mic;
	struct task wskbd_task;
	int wskbd_taskset;
#endif
};

int audio_match(struct device *, void *, void *);
void audio_attach(struct device *, struct device *, void *);
int audio_activate(struct device *, int);
int audio_detach(struct device *, int);
#if NWSKBD > 0
void wskbd_mixer_init(struct audio_softc *);
#endif

const struct cfattach audio_ca = {
	sizeof(struct audio_softc), audio_match, audio_attach,
	audio_detach, audio_activate
};

struct cfdriver audio_cd = {
	NULL, "audio", DV_DULL
};

/*
 * This mutex protects data structures (including registers on the
 * sound-card) that are manipulated by both the interrupt handler and
 * syscall code-paths.
 *
 * Note that driver methods may sleep (e.g. in malloc); consequently the
 * audio layer calls them with the mutex unlocked. Driver methods are
 * responsible for locking the mutex when they manipulate data used by
 * the interrupt handler and interrupts may occur.
 *
 * Similarly, the driver is responsible for locking the mutex in its
 * interrupt handler and to call the audio layer call-backs (i.e.
 * audio_{p,r}int()) with the mutex locked.
 */
struct mutex audio_lock = MUTEX_INITIALIZER(IPL_AUDIO);

#ifdef AUDIO_DEBUG
/*
 * 0 - nothing, as if AUDIO_DEBUG isn't defined
 * 1 - initialisations & setup
 * 2 - blocks & interrupts
 */
int audio_debug = 1;
#endif

unsigned int
audio_gcd(unsigned int a, unsigned int b)
{
	unsigned int r;

	while (b > 0) {
		r = a % b;
		a = b;
		b = r;
	}
	return a;
}

int
audio_buf_init(struct audio_softc *sc, struct audio_buf *buf, int dir)
{
	if (sc->ops->round_buffersize) {
		buf->datalen = sc->ops->round_buffersize(sc->arg,
		    dir, AUDIO_BUFSZ);
	} else
		buf->datalen = AUDIO_BUFSZ;
	if (sc->ops->allocm) {
		buf->data = sc->ops->allocm(sc->arg, dir, buf->datalen,
		    M_DEVBUF, M_WAITOK);
	} else
		buf->data = malloc(buf->datalen, M_DEVBUF, M_WAITOK);
	if (buf->data == NULL)
		return ENOMEM;
	return 0;
}

void
audio_buf_done(struct audio_softc *sc, struct audio_buf *buf)
{
	if (sc->ops->freem)
		sc->ops->freem(sc->arg, buf->data, M_DEVBUF);
	else
		free(buf->data, M_DEVBUF, buf->datalen);
}

/*
 * return the reader pointer and the number of bytes available
 */
unsigned char *
audio_buf_rgetblk(struct audio_buf *buf, size_t *rsize)
{
	size_t count;

	count = buf->len - buf->start;
	if (count > buf->used)
		count = buf->used;
	*rsize = count;
	return buf->data + buf->start;
}

/*
 * discard "count" bytes at the start postion.
 */
void
audio_buf_rdiscard(struct audio_buf *buf, size_t count)
{
#ifdef AUDIO_DEBUG
	if (count > buf->used) {
		panic("audio_buf_rdiscard: bad count = %zu\n", count);
	}
#endif
	buf->used -= count;
	buf->start += count;
	if (buf->start >= buf->len)
		buf->start -= buf->len;
}

/*
 * advance the writer pointer by "count" bytes
 */
void
audio_buf_wcommit(struct audio_buf *buf, size_t count)
{
#ifdef AUDIO_DEBUG
	if (count > (buf->len - buf->used)) {
		panic("audio_buf_wcommit: bad count = %zu\n", count);
	}
#endif
	buf->used += count;
}

/*
 * get writer pointer and the number of bytes writable
 */
unsigned char *
audio_buf_wgetblk(struct audio_buf *buf, size_t *rsize)
{
	size_t end, avail, count;

	end = buf->start + buf->used;
	if (end >= buf->len)
		end -= buf->len;
	avail = buf->len - buf->used;
	count = buf->len - end;
	if (count > avail)
		count = avail;
	*rsize = count;
	return buf->data + end;
}

void
audio_calc_sil(struct audio_softc *sc)
{
	unsigned char *q;
	unsigned int s, i;
	int d, e;

	e = sc->sw_enc;
#ifdef AUDIO_DEBUG
	switch (e) {
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_BE:
		break;
	default:
		printf("%s: unhandled play encoding %d\n", DEVNAME(sc), e);
		memset(sc->silence, 0, sc->bps);
		return;
	}
#endif
	if (e == AUDIO_ENCODING_SLINEAR_BE || e == AUDIO_ENCODING_ULINEAR_BE) {
		d = -1;
		q = sc->silence + sc->bps - 1;
	} else {
		d = 1;
		q = sc->silence;
	}
	if (e == AUDIO_ENCODING_SLINEAR_LE || e == AUDIO_ENCODING_SLINEAR_BE) {
		s = 0;
	} else {
		s = 0x80000000;
		if (sc->msb)
			s >>= 32 - 8 * sc->bps;
		else
			s >>= 32 - sc->bits;
	}
	for (i = 0; i < sc->bps; i++) {
		*q = s;
		q += d;
		s >>= 8;
	}
	if (sc->conv_enc)
		sc->conv_enc(sc->silence, sc->bps);
}

void
audio_fill_sil(struct audio_softc *sc, unsigned char *ptr, size_t count)
{
	unsigned char *q, *p;
	size_t i, j;

	q = ptr;
	for (j = count / sc->bps; j > 0; j--) {
		p = sc->silence;
		for (i = sc->bps; i > 0; i--)
			*q++ = *p++;
	}
}

void
audio_clear(struct audio_softc *sc)
{
	if (sc->mode & AUMODE_PLAY) {
		sc->play.used = sc->play.start = 0;
		sc->play.pos = sc->play.xrun = 0;
		audio_fill_sil(sc, sc->play.data, sc->play.len);
	}
	if (sc->mode & AUMODE_RECORD) {
		sc->rec.used = sc->rec.start = 0;
		sc->rec.pos = sc->rec.xrun = 0;
		audio_fill_sil(sc, sc->rec.data, sc->rec.len);
	}
}

/*
 * called whenever a block is consumed by the driver
 */
void
audio_pintr(void *addr)
{
	struct audio_softc *sc = addr;
	unsigned char *ptr;
	size_t count;
	int error;

	MUTEX_ASSERT_LOCKED(&audio_lock);
	if (!(sc->mode & AUMODE_PLAY) || !sc->active) {
		printf("%s: play interrupt but not playing\n", DEVNAME(sc));
		return;
	}
	if (sc->quiesce) {
		DPRINTF("%s: quesced, skipping play intr\n", DEVNAME(sc));
		return;
	}

	sc->play.pos += sc->play.blksz;
	audio_fill_sil(sc, sc->play.data + sc->play.start, sc->play.blksz);
	audio_buf_rdiscard(&sc->play, sc->play.blksz);
	if (sc->play.used < sc->play.blksz) {
		DPRINTFN(1, "%s: play underrun\n", DEVNAME(sc));
		sc->play.xrun += sc->play.blksz;
		audio_buf_wcommit(&sc->play, sc->play.blksz);
	}

	DPRINTFN(1, "%s: play intr, used -> %zu, start -> %zu\n",
	    DEVNAME(sc), sc->play.used, sc->play.start);

	if (!sc->ops->trigger_output) {
		ptr = audio_buf_rgetblk(&sc->play, &count);
		error = sc->ops->start_output(sc->arg,
		    ptr, sc->play.blksz, audio_pintr, (void *)sc);
		if (error) {
			printf("%s: play restart failed: %d\n",
			    DEVNAME(sc), error);
		}
	}

	if (sc->play.used < sc->play.len) {
		DPRINTFN(1, "%s: play wakeup, chan = %d\n",
		    DEVNAME(sc), sc->play.blocking);
		if (sc->play.blocking) {
			wakeup(&sc->play.blocking);
			sc->play.blocking = 0;
		}
		selwakeup(&sc->play.sel);
	}
}

/*
 * called whenever a block is produced by the driver
 */
void
audio_rintr(void *addr)
{
	struct audio_softc *sc = addr;
	unsigned char *ptr;
	size_t count;
	int error;

	MUTEX_ASSERT_LOCKED(&audio_lock);
	if (!(sc->mode & AUMODE_RECORD) || !sc->active) {
		printf("%s: rec interrupt but not recording\n", DEVNAME(sc));
		return;
	}
	if (sc->quiesce) {
		DPRINTF("%s: quesced, skipping rec intr\n", DEVNAME(sc));
		return;
	}

	sc->rec.pos += sc->rec.blksz;
	audio_buf_wcommit(&sc->rec, sc->rec.blksz);
	if (sc->rec.used == sc->rec.len) {
		DPRINTFN(1, "%s: rec overrun\n", DEVNAME(sc));
		sc->rec.xrun += sc->rec.blksz;
		audio_buf_rdiscard(&sc->rec, sc->rec.blksz);
	}
	DPRINTFN(1, "%s: rec intr, used -> %zu\n", DEVNAME(sc), sc->rec.used);

	if (!sc->ops->trigger_input) {
		ptr = audio_buf_wgetblk(&sc->rec, &count);
		error = sc->ops->start_input(sc->arg,
		    ptr, sc->rec.blksz, audio_rintr, (void *)sc);
		if (error) {
			printf("%s: rec restart failed: %d\n",
			    DEVNAME(sc), error);
		}
	}

	if (sc->rec.used > 0) {
		DPRINTFN(1, "%s: rec wakeup, chan = %d\n",
		    DEVNAME(sc), sc->rec.blocking);
		if (sc->rec.blocking) {
			wakeup(&sc->rec.blocking);
			sc->rec.blocking = 0;
		}
		selwakeup(&sc->rec.sel);
	}
}

int
audio_start_do(struct audio_softc *sc)
{
	int error;
	struct audio_params p;
	unsigned char *ptr;
	size_t count;

	DPRINTFN(1, "%s: start play: "
	    "start = %zu, used = %zu, "
	    "len = %zu, blksz = %zu\n",
	    DEVNAME(sc), sc->play.start, sc->play.used,
	    sc->play.len, sc->play.blksz);
	DPRINTFN(1, "%s: start rec: "
	    "start = %zu, used = %zu, "
	    "len = %zu, blksz = %zu\n",
	    DEVNAME(sc), sc->rec.start, sc->rec.used,
	    sc->rec.len, sc->rec.blksz);

	error = 0;
	if (sc->mode & AUMODE_PLAY) {
		if (sc->ops->trigger_output) {
			p.encoding = sc->hw_enc;
			p.precision = sc->bits;
			p.bps = sc->bps;
			p.msb = sc->msb;
			p.sample_rate = sc->rate;
			p.channels = sc->pchan;
			error = sc->ops->trigger_output(sc->arg,
			    sc->play.data,
			    sc->play.data + sc->play.len,
			    sc->play.blksz,
			    audio_pintr, (void *)sc, &p);
		} else {
			mtx_enter(&audio_lock);
			ptr = audio_buf_rgetblk(&sc->play, &count);
			error = sc->ops->start_output(sc->arg,
			    ptr, sc->play.blksz, audio_pintr, (void *)sc);
			mtx_leave(&audio_lock);
		}
		if (error)
			printf("%s: failed to start playback\n", DEVNAME(sc));
	}
	if (sc->mode & AUMODE_RECORD) {
		if (sc->ops->trigger_input) {
			p.encoding = sc->hw_enc;
			p.precision = sc->bits;
			p.bps = sc->bps;
			p.msb = sc->msb;
			p.sample_rate = sc->rate;
			p.channels = sc->rchan;
			error = sc->ops->trigger_input(sc->arg,
			    sc->rec.data,
			    sc->rec.data + sc->rec.len,
			    sc->rec.blksz,
			    audio_rintr, (void *)sc, &p);
		} else {
			mtx_enter(&audio_lock);
			ptr = audio_buf_wgetblk(&sc->rec, &count);
			error = sc->ops->start_input(sc->arg,
			    ptr, sc->rec.blksz, audio_rintr, (void *)sc);
			mtx_leave(&audio_lock);
		}
		if (error)
			printf("%s: failed to start recording\n", DEVNAME(sc));
	}
	return error;
}

int
audio_stop_do(struct audio_softc *sc)
{
	if (sc->mode & AUMODE_PLAY)
		sc->ops->halt_output(sc->arg);
	if (sc->mode & AUMODE_RECORD)
		sc->ops->halt_input(sc->arg);
	return 0;
}

int
audio_start(struct audio_softc *sc)
{
	sc->active = 1;
	sc->play.xrun = sc->play.pos = sc->rec.xrun = sc->rec.pos = 0;
	return audio_start_do(sc);
}

int
audio_stop(struct audio_softc *sc)
{
	int error;

	error = audio_stop_do(sc);
	if (error)
		return error;
	audio_clear(sc);
	sc->active = 0;
	return 0;
}

int
audio_setpar(struct audio_softc *sc)
{
	struct audio_params p, r;
	unsigned int nr, np, max, min, mult;
	int error;

	DPRINTF("%s: setpar: req enc=%d bits=%d, bps=%d, msb=%d "
	    "rate=%d, pchan=%d, rchan=%d, round=%u, nblks=%d\n",
	    DEVNAME(sc), sc->sw_enc, sc->bits, sc->bps, sc->msb,
	    sc->rate, sc->pchan, sc->rchan, sc->round, sc->nblks);

	/*
	 * AUDIO_ENCODING_SLINEAR and AUDIO_ENCODING_ULINEAR are not
	 * used anymore, promote them to the _LE and _BE equivalents
	 */
	if (sc->sw_enc == AUDIO_ENCODING_SLINEAR) {
#if BYTE_ORDER == LITTLE_ENDIAN
		sc->sw_enc = AUDIO_ENCODING_SLINEAR_LE;
#else
		sc->sw_enc = AUDIO_ENCODING_SLINEAR_BE;
#endif
	}
	if (sc->sw_enc == AUDIO_ENCODING_ULINEAR) {
#if BYTE_ORDER == LITTLE_ENDIAN
		sc->sw_enc = AUDIO_ENCODING_ULINEAR_LE;
#else
		sc->sw_enc = AUDIO_ENCODING_ULINEAR_BE;
#endif
	}

	/*
	 * check if requested parameters are in the allowed ranges
	 */
	if (sc->mode & AUMODE_PLAY) {
		if (sc->pchan < 1)
			sc->pchan = 1;
		if (sc->pchan > 64)
			sc->pchan = 64;
	}
	if (sc->mode & AUMODE_RECORD) {
		if (sc->rchan < 1)
			sc->rchan = 1;
		if (sc->rchan > 64)
			sc->rchan = 64;
	}
	switch (sc->sw_enc) {
	case AUDIO_ENCODING_ULAW:
	case AUDIO_ENCODING_ALAW:
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
		break;
	default:
		sc->sw_enc = AUDIO_ENCODING_SLINEAR_LE;
	}
	if (sc->bits < 8)
		sc->bits = 8;
	if (sc->bits > 32)
		sc->bits = 32;
	if (sc->bps < 1)
		sc->bps = 1;
	if (sc->bps > 4)
		sc->bps = 4;
	if (sc->rate < 4000)
		sc->rate = 4000;
	if (sc->rate > 192000)
		sc->rate = 192000;

	/*
	 * copy into struct audio_params, required by drivers
	 */
	p.encoding = r.encoding = sc->sw_enc;
	p.precision = r.precision = sc->bits;
	p.bps = r.bps = sc->bps;
	p.msb = r.msb = sc->msb;
	p.sample_rate = r.sample_rate = sc->rate;
	p.channels = sc->pchan;
	r.channels = sc->rchan;

	/*
	 * set parameters
	 */
	error = sc->ops->set_params(sc->arg, sc->mode, sc->mode, &p, &r);
	if (error)
		return error;
	if (sc->mode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (p.encoding != r.encoding ||
		    p.precision != r.precision ||
		    p.bps != r.bps ||
		    p.msb != r.msb ||
		    p.sample_rate != r.sample_rate) {
			printf("%s: different play and record parameters"
			    "returned by hardware\n", DEVNAME(sc));
			return ENODEV;
		}
	}
	if (sc->mode & AUMODE_PLAY) {
		sc->hw_enc = p.encoding;
		sc->bits = p.precision;
		sc->bps = p.bps;
		sc->msb = p.msb;
		sc->rate = p.sample_rate;
		sc->pchan = p.channels;
	}
	if (sc->mode & AUMODE_RECORD) {
		sc->hw_enc = r.encoding;
		sc->bits = r.precision;
		sc->bps = r.bps;
		sc->msb = r.msb;
		sc->rate = r.sample_rate;
		sc->rchan = r.channels;
	}
	if (sc->rate == 0 || sc->bps == 0 || sc->bits == 0) {
		printf("%s: invalid parameters returned by hardware\n",
		    DEVNAME(sc));
		return ENODEV;
	}
	if (sc->ops->commit_settings) {
		error = sc->ops->commit_settings(sc->arg);
		if (error)
			return error;
	}

	/*
	 * conversion from/to exotic/dead encoding, for drivers not supporting
	 * linear
	 */
	switch (sc->hw_enc) {
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
		sc->sw_enc = sc->hw_enc;
		sc->conv_dec = sc->conv_enc = NULL;
		break;
	case AUDIO_ENCODING_ULAW:
#if BYTE_ORDER == LITTLE_ENDIAN
		sc->sw_enc = AUDIO_ENCODING_SLINEAR_LE;
#else
		sc->sw_enc = AUDIO_ENCODING_SLINEAR_BE;
#endif
		if (sc->bits == 8) {
			sc->conv_enc = slinear8_to_mulaw;
			sc->conv_dec = mulaw_to_slinear8;
			break;
		} else if (sc->bits == 24) {
			sc->conv_enc = slinear24_to_mulaw24;
			sc->conv_dec = mulaw24_to_slinear24;
			break;
		}
		sc->sw_enc = sc->hw_enc;
		sc->conv_dec = sc->conv_enc = NULL;
		break;
	default:
		printf("%s: setpar: enc = %d, bits = %d: emulation skipped\n",
		    DEVNAME(sc), sc->hw_enc, sc->bits);
		sc->sw_enc = sc->hw_enc;
		sc->conv_dec = sc->conv_enc = NULL;
	}
	audio_calc_sil(sc);

	/*
	 * get least multiplier of the number of frames per block
	 */
	if (sc->ops->round_blocksize) {
		mult = sc->ops->round_blocksize(sc->arg, 1);
		if (mult == 0) {
			printf("%s: 0x%x: bad block size multiplier\n",
			    DEVNAME(sc), mult);
			return ENODEV;
		}
	} else
		mult = 1;
	DPRINTF("%s: hw block size multiplier: %u\n", DEVNAME(sc), mult);
	if (sc->mode & AUMODE_PLAY) {
		np = mult / audio_gcd(sc->pchan * sc->bps, mult);
		if (!(sc->mode & AUMODE_RECORD))
			nr = np;
		DPRINTF("%s: play number of frames multiplier: %u\n",
		    DEVNAME(sc), np);
	}
	if (sc->mode & AUMODE_RECORD) {
		nr = mult / audio_gcd(sc->rchan * sc->bps, mult);
		if (!(sc->mode & AUMODE_PLAY))
			np = nr;
		DPRINTF("%s: record number of frames multiplier: %u\n",
		    DEVNAME(sc), nr);
	}
	mult = nr * np / audio_gcd(nr, np);
	DPRINTF("%s: least common number of frames multiplier: %u\n",
	    DEVNAME(sc), mult);

	/*
	 * get minumum and maximum frames per block
	 */
	if (sc->mode & AUMODE_PLAY) {
		np = sc->play.datalen / (sc->pchan * sc->bps * 2);
		if (!(sc->mode & AUMODE_RECORD))
			nr = np;
	}
	if (sc->mode & AUMODE_RECORD) {
		nr = sc->rec.datalen / (sc->rchan * sc->bps * 2);
		if (!(sc->mode & AUMODE_PLAY))
			np = nr;
	}
	max = np < nr ? np : nr;
	max -= max % mult;
	min = sc->rate / 1000 + mult - 1;
	min -= min % mult;
	DPRINTF("%s: frame number range: %u..%u\n", DEVNAME(sc), min, max);
	if (max < min) {
		printf("%s: %u: bad max frame number\n", DEVNAME(sc), max);
		return EIO;
	}

	/*
	 * adjust the frame per block to match our constraints
	 */
	sc->round += mult / 2;
	sc->round -= sc->round % mult;
	if (sc->round > max)
		sc->round = max;
	if (sc->round < min)
		sc->round = min;
	sc->round = sc->round;

	/*
	 * set buffer size (number of blocks)
	 */
	if (sc->mode & AUMODE_PLAY) {
		sc->play.blksz = sc->round * sc->pchan * sc->bps;
		max = sc->play.datalen / sc->play.blksz;
		if (sc->nblks > max)
			sc->nblks = max;
		if (sc->nblks < 2)
			sc->nblks = 2;
		sc->play.len = sc->nblks * sc->play.blksz;
		sc->nblks = sc->nblks;
	}
	if (sc->mode & AUMODE_RECORD) {
		/*
		 * for recording, buffer size is not the latency (it's
		 * exactly one block), so let's get the maximum buffer
		 * size of maximum reliability during xruns
		 */
		sc->rec.blksz = sc->round * sc->rchan * sc->bps;
		sc->rec.len = sc->rec.datalen;
		sc->rec.len -= sc->rec.datalen % sc->rec.blksz;
	}

	DPRINTF("%s: setpar: new enc=%d bits=%d, bps=%d, msb=%d "
	    "rate=%d, pchan=%d, rchan=%d, round=%u, nblks=%d\n",
	    DEVNAME(sc), sc->sw_enc, sc->bits, sc->bps, sc->msb,
	    sc->rate, sc->pchan, sc->rchan, sc->round, sc->nblks);
	return 0;
}

int
audio_setinfo(struct audio_softc *sc, struct audio_info *ai)
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	int error;
	int set;

	/*
	 * stop the device if requested to stop
	 */
	if (sc->mode != 0) {
		if (sc->mode & AUMODE_PLAY) {
			if (p->pause != (unsigned char)~0)
				sc->pause = p->pause;
		}
		if (sc->mode & AUMODE_RECORD) {
			if (r->pause != (unsigned char)~0)
				sc->pause = r->pause;
		}
		if (sc->pause) {
			if (sc->active)
				audio_stop(sc);
		}
	}

	/*
	 * copy parameters into the softc structure
	 */
	set = 0;
	if (ai->play.encoding != ~0) {
		sc->sw_enc = ai->play.encoding;
		set = 1;
	}
	if (ai->play.precision != ~0) {
		sc->bits = ai->play.precision;
		set = 1;
	}
	if (ai->play.bps != ~0) {
		sc->bps = ai->play.bps;
		set = 1;
	}
	if (ai->play.msb != ~0) {
		sc->msb = ai->play.msb;
		set = 1;
	}
	if (ai->play.sample_rate != ~0) {
		sc->rate = ai->play.sample_rate;
		set = 1;
	}
	if (ai->play.channels != ~0) {
		sc->pchan = ai->play.channels;
		set = 1;
	}
	if (ai->play.block_size != ~0) {
		sc->round = ai->play.block_size /
		    (sc->bps * sc->pchan);
		set = 1;
	}
	if (ai->hiwat != ~0) {
		sc->nblks = ai->hiwat;
		set = 1;
	}
	if (ai->record.encoding != ~0) {
		sc->sw_enc = ai->record.encoding;
		set = 1;
	}
	if (ai->record.precision != ~0) {
		sc->bits = ai->record.precision;
		set = 1;
	}
	if (ai->record.bps != ~0) {
		sc->bps = ai->record.bps;
		set = 1;
	}
	if (ai->record.msb != ~0) {
		sc->msb = ai->record.msb;
		set = 1;
	}
	if (ai->record.sample_rate != ~0) {
		sc->rate = ai->record.sample_rate;
		set = 1;
	}
	if (ai->record.channels != ~0) {
		sc->rchan = ai->record.channels;
		set = 1;
	}
	if (ai->record.block_size != ~0) {
		sc->round = ai->record.block_size /
		    (sc->bps * sc->rchan);
		set = 1;
	}

	DPRINTF("%s: setinfo: set = %d, mode = %d, pause = %d\n",
	    DEVNAME(sc), set, sc->mode, sc->pause);

	/*
	 * if the device not opened, we're done, don't touch the hardware
	 */
	if (sc->mode == 0)
		return 0;

	/*
	 * change parameters and recalculate buffer sizes
	 */
	if (set) {
		if (sc->active) {
			DPRINTF("%s: can't change params during dma\n",
			    DEVNAME(sc));
			return EBUSY;
		}
		error = audio_setpar(sc);
		if (error)
			return error;
		audio_clear(sc);
		if ((sc->mode & AUMODE_PLAY) && sc->ops->init_output) {
			error = sc->ops->init_output(sc->arg,
			    sc->play.data, sc->play.len);
			if (error)
				return error;
		}
		if ((sc->mode & AUMODE_RECORD) && sc->ops->init_input) {
			error = sc->ops->init_input(sc->arg,
			    sc->rec.data, sc->rec.len);
			if (error)
				return error;
		}
	}

	/*
	 * if unpaused, start
	 */
	if (!sc->pause && !sc->active) {
		error = audio_start(sc);
		if (error)
			return error;
	}
	return 0;
}

int
audio_getinfo(struct audio_softc *sc, struct audio_info *ai)
{
	ai->play.sample_rate = ai->record.sample_rate = sc->rate;
	ai->play.encoding = ai->record.encoding = sc->sw_enc;
	ai->play.precision = ai->record.precision = sc->bits;
	ai->play.bps = ai->record.bps = sc->bps;
	ai->play.msb = ai->record.msb = sc->msb;
	ai->play.channels = sc->pchan;
	ai->record.channels = sc->rchan;

	/*
	 * XXX: this is used only to display counters through audioctl
	 * and the pos counters are more useful
	 */
	mtx_enter(&audio_lock);
	ai->play.samples = sc->play.pos - sc->play.xrun;
	ai->record.samples = sc->rec.pos - sc->rec.xrun;
	mtx_leave(&audio_lock);

	ai->play.pause = ai->record.pause = sc->pause;
	ai->play.active = ai->record.active = sc->active;

	ai->play.buffer_size = sc->play.datalen;
	ai->record.buffer_size = sc->rec.datalen;

	ai->play.block_size =  sc->round * sc->bps * sc->pchan;
	ai->record.block_size = sc->round * sc->bps * sc->rchan;

	ai->hiwat = sc->nblks;
	ai->lowat = sc->nblks;
	ai->mode = sc->mode;
	return 0;
}

int
audio_match(struct device *parent, void *match, void *aux)
{
	struct audio_attach_args *sa = aux;

	return (sa->type == AUDIODEV_TYPE_AUDIO) ? 1 : 0;
}

void
audio_attach(struct device *parent, struct device *self, void *aux)
{
	struct audio_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;
	struct audio_hw_if *ops = sa->hwif;
	void *arg = sa->hdl;
	int error;

	printf("\n");

#ifdef DIAGNOSTIC
	if (ops == 0 ||
	    ops->open == 0 ||
	    ops->close == 0 ||
	    ops->query_encoding == 0 ||
	    ops->set_params == 0 ||
	    (ops->start_output == 0 && ops->trigger_output == 0) ||
	    (ops->start_input == 0 && ops->trigger_input == 0) ||
	    ops->halt_output == 0 ||
	    ops->halt_input == 0 ||
	    ops->getdev == 0 ||
	    ops->set_port == 0 ||
	    ops->get_port == 0 ||
	    ops->query_devinfo == 0 ||
	    ops->get_props == 0) {
		printf("%s: missing method\n", DEVNAME(sc));
		sc->ops = 0;
		return;
	}
#endif
	sc->ops = ops;
	sc->arg = arg;

#if NWSKBD > 0
	wskbd_mixer_init(sc);
#endif /* NWSKBD > 0 */

	error = audio_buf_init(sc, &sc->play, AUMODE_PLAY);
	if (error) {
		sc->ops = 0;
		printf("%s: could not allocate play buffer\n", DEVNAME(sc));
		return;
	}
	error = audio_buf_init(sc, &sc->rec, AUMODE_RECORD);
	if (error) {
		audio_buf_done(sc, &sc->play);
		sc->ops = 0;
		printf("%s: could not allocate record buffer\n", DEVNAME(sc));
		return;
	}

	/* set defaults */
	sc->sw_enc = AUDIO_ENCODING_SLINEAR;
	sc->bits = 16;
	sc->bps = 2;
	sc->msb = 1;
	sc->rate = 48000;
	sc->pchan = 2;
	sc->rchan = 2;
	sc->round = 960;
	sc->nblks = 2;
	sc->play.pos = sc->play.xrun = sc->rec.pos = sc->rec.xrun = 0;
}

int
audio_activate(struct device *self, int act)
{
	struct audio_softc *sc = (struct audio_softc *)self;

	switch (act) {
	case DVACT_QUIESCE:
		/*
		 * good drivers run play and rec handlers in a single
		 * interrupt. Grab the lock to ensure we expose the same
		 * sc->quiesce value to both play and rec handlers
		 */
		mtx_enter(&audio_lock);
		sc->quiesce = 1;
		mtx_leave(&audio_lock);

		/*
		 * once sc->quiesce is set, interrupts may occur, but
		 * counters are not advanced and consequently processes
		 * keep sleeping.
		 *
		 * XXX: ensure read/write/ioctl don't start/stop
		 * DMA at the same time, this needs a "ready" condvar
		 */
		if (sc->mode != 0 && sc->active)
			audio_stop_do(sc);
		DPRINTF("%s: quesce: active = %d\n", DEVNAME(sc), sc->active);
		break;
	case DVACT_WAKEUP:
		DPRINTF("%s: wakeup: active = %d\n", DEVNAME(sc), sc->active);

		/*
		 * keep buffer usage the same, but set start pointer to
		 * the beginning of the buffer.
		 *
		 * No need to grab the audio_lock as DMA is stopped and
		 * this is the only thread running (caller ensures this)
		 */
		sc->quiesce = 0;
		wakeup(&sc->quiesce);

		if(sc->mode != 0) {
			if (audio_setpar(sc) != 0)
				break;
			if (sc->mode & AUMODE_PLAY) {
				sc->play.start = 0;
				audio_fill_sil(sc, sc->play.data, sc->play.len);
			}
			if (sc->mode & AUMODE_RECORD) {
				sc->rec.start = sc->rec.len - sc->rec.used;
				audio_fill_sil(sc, sc->rec.data, sc->rec.len);
			}
			if (sc->active)
				audio_start_do(sc);
		}
		break;
	}
	return 0;
}

int
audio_detach(struct device *self, int flags)
{
	struct audio_softc *sc = (struct audio_softc *)self;
	int maj, mn;

	DPRINTF("%s: audio_detach: flags = %d\n", DEVNAME(sc), flags);

	wakeup(&sc->quiesce);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == audioopen)
			break;
	/*
	 * Nuke the vnodes for any open instances, calls close but as
	 * close uses device_lookup, it returns EXIO and does nothing
	 */
	mn = self->dv_unit;
	vdevgone(maj, mn | AUDIO_DEV_SOUND, mn | AUDIO_DEV_SOUND, VCHR);
	vdevgone(maj, mn | AUDIO_DEV_AUDIO, mn | AUDIO_DEV_AUDIO, VCHR);
	vdevgone(maj, mn | AUDIO_DEV_AUDIOCTL, mn | AUDIO_DEV_AUDIOCTL, VCHR);
	vdevgone(maj, mn | AUDIO_DEV_MIXER, mn | AUDIO_DEV_MIXER, VCHR);

	/*
	 * The close() method did nothing, quickly halt DMA (normally
	 * parent is already gone, and code below is no-op), and wake-up
	 * user-land blocked in read/write/ioctl, which return EIO.
	 */
	if (sc->mode != 0) {
		if (sc->active) {
			wakeup(&sc->play.blocking);
			selwakeup(&sc->play.sel);
			wakeup(&sc->rec.blocking);
			selwakeup(&sc->rec.sel);
			audio_stop(sc);
		}
		sc->ops->close(sc->arg);
		sc->mode = 0;
	}

	/* free resources */
	audio_buf_done(sc, &sc->play);
	audio_buf_done(sc, &sc->rec);
	return 0;
}

struct device *
audio_attach_mi(struct audio_hw_if *ops, void *arg, struct device *dev)
{
	struct audio_attach_args aa;

	aa.type = AUDIODEV_TYPE_AUDIO;
	aa.hwif = ops;
	aa.hdl = arg;

	/*
	 * attach this driver to the caller (hardware driver), this
	 * checks the kernel config and possibly calls audio_attach()
	 */
	return config_found(dev, &aa, audioprint);
}

int
audioprint(void *aux, const char *pnp)
{
	struct audio_attach_args *arg = aux;
	const char *type;

	if (pnp != NULL) {
		switch (arg->type) {
		case AUDIODEV_TYPE_AUDIO:
			type = "audio";
			break;
		case AUDIODEV_TYPE_OPL:
			type = "opl";
			break;
		case AUDIODEV_TYPE_MPU:
			type = "mpu";
			break;
		default:
			panic("audioprint: unknown type %d", arg->type);
		}
		printf("%s at %s", type, pnp);
	}
	return UNCONF;
}

int
audio_open(struct audio_softc *sc, int flags)
{
	int error;
	int props;

	if (sc->mode)
		return EBUSY;
	error = sc->ops->open(sc->arg, flags);
	if (error)
		return error;
	sc->active = 0;
	sc->pause = 1;
	sc->rec.blocking = 0;
	sc->play.blocking = 0;
	sc->mode = 0;
	if (flags & FWRITE)
		sc->mode |= AUMODE_PLAY;
	if (flags & FREAD)
		sc->mode |= AUMODE_RECORD;
	props = sc->ops->get_props(sc->arg);
	if (sc->mode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (!(props & AUDIO_PROP_FULLDUPLEX)) {
			error = ENOTTY;
			goto bad;
		}
		if (sc->ops->setfd) {
			error = sc->ops->setfd(sc->arg, 1);
			if (error)
				goto bad;
		}
	}

	if (sc->ops->speaker_ctl) {
		/*
		 * XXX: what is this used for?
		 */
		sc->ops->speaker_ctl(sc->arg,
		    (sc->mode & AUMODE_PLAY) ? SPKR_ON : SPKR_OFF);
	}

	error = audio_setpar(sc);
	if (error)
		goto bad;
	audio_clear(sc);

	/*
	 * allow read(2)/write(2) to automatically start DMA, without
	 * the need for ioctl(), to make /dev/audio usable in scripts
	 */
	sc->pause = 0;
	return 0;
bad:
	sc->ops->close(sc->arg);
	sc->mode = 0;
	return error;
}

int
audio_drain(struct audio_softc *sc)
{
	int error, xrun;
	unsigned char *ptr;
	size_t count, bpf;

	DPRINTF("%s: drain: mode = %d, pause = %d, active = %d, used = %zu\n",
	    DEVNAME(sc), sc->mode, sc->pause, sc->active, sc->play.used);
	if (!(sc->mode & AUMODE_PLAY) || sc->pause)
		return 0;

	/* discard partial samples, required by audio_fill_sil() */
	mtx_enter(&audio_lock);
	bpf = sc->pchan * sc->bps;
	sc->play.used -= sc->play.used % bpf;
	if (sc->play.used == 0) {
		mtx_leave(&audio_lock);
		return 0;
	}

	if (!sc->active) {
		/*
		 * dma not started yet because buffer was not full
		 * enough to start automatically. Pad it and start now.
		 */
		for (;;) {
			ptr = audio_buf_wgetblk(&sc->play, &count);
			if (count == 0)
				break;
			audio_fill_sil(sc, ptr, count);
			audio_buf_wcommit(&sc->play, count);
		}
		mtx_leave(&audio_lock);
		error = audio_start(sc);
		if (error)
			return error;
		mtx_enter(&audio_lock);
	}

	xrun = sc->play.xrun;
	while (sc->play.xrun == xrun) {
		DPRINTF("%s: drain: used = %zu, xrun = %ld\n",
		    DEVNAME(sc), sc->play.used, sc->play.xrun);

		/*
		 * set a 5 second timeout, in case interrupts don't
		 * work, useful only for debugging drivers
		 */
		sc->play.blocking = 1;
		error = msleep(&sc->play.blocking, &audio_lock,
		    PWAIT | PCATCH, "au_dr", 5 * hz);
		if (!(sc->dev.dv_flags & DVF_ACTIVE))
			error = EIO;
		if (error) {
			DPRINTF("%s: drain, err = %d\n", DEVNAME(sc), error);
			break;
		}
	}
	mtx_leave(&audio_lock);
	return error;
}

int
audio_close(struct audio_softc *sc)
{
	audio_drain(sc);
	if (sc->active)
		audio_stop(sc);
	sc->ops->close(sc->arg);
	sc->mode = 0;
	DPRINTF("%s: close: done\n", DEVNAME(sc));
	return 0;
}

int
audio_read(struct audio_softc *sc, struct uio *uio, int ioflag)
{
	unsigned char *ptr;
	size_t count;
	int error;

	DPRINTFN(1, "%s: read: resid = %zd\n",  DEVNAME(sc), uio->uio_resid);

	/* block if quiesced */
	while (sc->quiesce)
		tsleep(&sc->quiesce, 0, "au_qrd", 0);

	/* start automatically if setinfo() was never called */
	mtx_enter(&audio_lock);
	if (!sc->active && !sc->pause && sc->rec.used == 0) {
		mtx_leave(&audio_lock);
		error = audio_start(sc);
		if (error)
			return error;
		mtx_enter(&audio_lock);
	}

	/* if there is no data then sleep */
	while (sc->rec.used == 0) {
		if (ioflag & IO_NDELAY) {
			mtx_leave(&audio_lock);
			return EWOULDBLOCK;
		}
		DPRINTFN(1, "%s: read sleep\n", DEVNAME(sc));
		sc->rec.blocking = 1;
		error = msleep(&sc->rec.blocking,
		    &audio_lock, PWAIT | PCATCH, "au_rd", 0);
		if (!(sc->dev.dv_flags & DVF_ACTIVE))
			error = EIO;
#ifdef AUDIO_DEBUG
		if (error) {
			DPRINTF("%s: read woke up error = %d\n",
			    DEVNAME(sc), error);
		}
#endif
		if (error) {
			mtx_leave(&audio_lock);
			return error;
		}
	}

	/* at this stage, there is data to transfer */
	while (uio->uio_resid > 0 && sc->rec.used > 0) {
		ptr = audio_buf_rgetblk(&sc->rec, &count);
		if (count > uio->uio_resid)
			count = uio->uio_resid;
		mtx_leave(&audio_lock);
		DPRINTFN(1, "%s: read: start = %zu, count = %zu\n",
		    DEVNAME(sc), ptr - sc->rec.data, count);
		if (sc->conv_dec)
			sc->conv_dec(ptr, count);
		error = uiomove(ptr, count, uio);
		if (error)
			return error;
		mtx_enter(&audio_lock);
		audio_buf_rdiscard(&sc->rec, count);
	}
	mtx_leave(&audio_lock);
	return 0;
}

int
audio_write(struct audio_softc *sc, struct uio *uio, int ioflag)
{
	unsigned char *ptr;
	size_t count;
	int error;

	DPRINTFN(1, "%s: write: resid = %zd\n",  DEVNAME(sc), uio->uio_resid);

	/* block if quiesced */
	while (sc->quiesce)
		tsleep(&sc->quiesce, 0, "au_qwr", 0);

	/*
	 * if IO_NDELAY flag is set then check if there is enough room
	 * in the buffer to store at least one byte. If not then dont
	 * start the write process.
	 */
	mtx_enter(&audio_lock);
	if (uio->uio_resid > 0 && (ioflag & IO_NDELAY)) {
		if (sc->play.used == sc->play.len ) {
			mtx_leave(&audio_lock);
			return EWOULDBLOCK;
		}
	}

	while (uio->uio_resid > 0) {
		while (1) {
			ptr = audio_buf_wgetblk(&sc->play, &count);
			if (count > 0)
				break;
			if (ioflag & IO_NDELAY) {
				/*
				 * At this stage at least one byte is already
				 * moved so we do not return EWOULDBLOCK
				 */
				mtx_leave(&audio_lock);
				return 0;
			}
			DPRINTFN(1, "%s: write sleep\n", DEVNAME(sc));
			sc->play.blocking = 1;
			error = msleep(&sc->play.blocking,
			    &audio_lock, PWAIT | PCATCH, "au_wr", 0);
			if (!(sc->dev.dv_flags & DVF_ACTIVE))
				error = EIO;
#ifdef AUDIO_DEBUG
			if (error) {
				DPRINTF("%s: write woke up error = %d\n",
				    DEVNAME(sc), error);
			}
#endif
			if (error) {
				mtx_leave(&audio_lock);
				return error;
			}
		}
		if (count > uio->uio_resid)
			count = uio->uio_resid;
		mtx_leave(&audio_lock);
		error = uiomove(ptr, count, uio);
		if (error)
			return 0;
		if (sc->conv_enc) {
			sc->conv_enc(ptr, count);
			DPRINTFN(1, "audio_write: converted count = %zu\n",
			    count);
		}
		mtx_enter(&audio_lock);
		audio_buf_wcommit(&sc->play, count);

		/* start automatically if setinfo() was never called */
		if (!sc->active && !sc->pause &&
		    sc->play.used == sc->play.len) {
			mtx_leave(&audio_lock);
			error = audio_start(sc);
			if (error)
				return error;
			mtx_enter(&audio_lock);
		}
	}
	mtx_leave(&audio_lock);
	return 0;
}

int
audio_ioctl(struct audio_softc *sc, unsigned long cmd, void *addr)
{
	struct audio_offset *ao;
	int error = 0, fd;

	/* block if quiesced */
	while (sc->quiesce)
		tsleep(&sc->quiesce, 0, "au_qio", 0);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;
	case AUDIO_PERROR:
		mtx_enter(&audio_lock);
		*(int *)addr = sc->play.xrun / (sc->pchan * sc->bps);
		mtx_leave(&audio_lock);
		break;
	case AUDIO_RERROR:
		mtx_enter(&audio_lock);
		*(int *)addr = sc->rec.xrun / (sc->rchan * sc->bps);
		mtx_leave(&audio_lock);
		break;
	case AUDIO_GETOOFFS:
		mtx_enter(&audio_lock);
		ao = (struct audio_offset *)addr;
		ao->samples = sc->play.pos;
		mtx_leave(&audio_lock);
		break;
	case AUDIO_GETIOFFS:
		mtx_enter(&audio_lock);
		ao = (struct audio_offset *)addr;
		ao->samples = sc->rec.pos;
		mtx_leave(&audio_lock);
		break;
	case AUDIO_SETINFO:
		error = audio_setinfo(sc, (struct audio_info *)addr);
		break;
	case AUDIO_GETINFO:
		error = audio_getinfo(sc, (struct audio_info *)addr);
		break;
	case AUDIO_GETDEV:
		error = sc->ops->getdev(sc->arg, (audio_device_t *)addr);
		break;
	case AUDIO_GETENC:
		error = sc->ops->query_encoding(sc->arg,
		    (struct audio_encoding *)addr);
		break;
	case AUDIO_GETFD:
		*(int *)addr = (sc->mode & (AUMODE_PLAY | AUMODE_RECORD)) ==
		    (AUMODE_PLAY | AUMODE_RECORD);
		break;
	case AUDIO_SETFD:
		fd = *(int *)addr;
		if ((sc->mode & (AUMODE_PLAY | AUMODE_RECORD)) !=
		    (AUMODE_PLAY | AUMODE_RECORD) || !fd)
			return EINVAL;
		break;
	case AUDIO_GETPROPS:
		*(int *)addr = sc->ops->get_props(sc->arg);
		break;
	default:
		DPRINTF("%s: unknown ioctl 0x%lx\n", DEVNAME(sc), cmd);
		error = ENOTTY;
		break;
	}
	return error;
}

int
audio_ioctl_mixer(struct audio_softc *sc, unsigned long cmd, void *addr)
{
	int error;

	/* block if quiesced */
	while (sc->quiesce)
		tsleep(&sc->quiesce, 0, "mix_qio", 0);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;
	case AUDIO_MIXER_DEVINFO:
		((mixer_devinfo_t *)addr)->un.v.delta = 0;
		return sc->ops->query_devinfo(sc->arg, (mixer_devinfo_t *)addr);
	case AUDIO_MIXER_READ:
		return sc->ops->get_port(sc->arg, (mixer_ctrl_t *)addr);
	case AUDIO_MIXER_WRITE:
		error = sc->ops->set_port(sc->arg, (mixer_ctrl_t *)addr);
		if (error)
			return error;
		if (sc->ops->commit_settings)
			return sc->ops->commit_settings(sc->arg);
		break;
	default:
		return ENOTTY;
	}
	return 0;
}

int
audio_poll(struct audio_softc *sc, int events, struct proc *p)
{
	int revents = 0;

	mtx_enter(&audio_lock);
	if ((sc->mode & AUMODE_RECORD) && sc->rec.used > 0)
		revents |= events & (POLLIN | POLLRDNORM);
	if ((sc->mode & AUMODE_PLAY) && sc->play.used < sc->play.len)
		revents |= events & (POLLOUT | POLLWRNORM);
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->rec.sel);
		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->play.sel);
	}
	mtx_leave(&audio_lock);
	return revents;
}

int
audioopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct audio_softc *sc;
	int error;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return ENXIO;
	if (sc->ops == NULL)
		error = ENXIO;
	else {
		switch (AUDIO_DEV(dev)) {
		case AUDIO_DEV_SOUND:
		case AUDIO_DEV_AUDIO:
			error = audio_open(sc, flags);
			break;
		case AUDIO_DEV_AUDIOCTL:
		case AUDIO_DEV_MIXER:
			error = 0;
			break;
		default:
			error = ENXIO;
		}
	}
	device_unref(&sc->dev);
	return error;
}

int
audioclose(dev_t dev, int flags, int ifmt, struct proc *p)
{
	struct audio_softc *sc;
	int error;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return ENXIO;
	switch (AUDIO_DEV(dev)) {
	case AUDIO_DEV_SOUND:
	case AUDIO_DEV_AUDIO:
		error = audio_close(sc);
		break;
	case AUDIO_DEV_MIXER:
	case AUDIO_DEV_AUDIOCTL:
		error = 0;
	default:
		error = ENXIO;
	}
	device_unref(&sc->dev);
	return error;
}

int
audioread(dev_t dev, struct uio *uio, int ioflag)
{
	struct audio_softc *sc;
	int error;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return ENXIO;
	switch (AUDIO_DEV(dev)) {
	case AUDIO_DEV_SOUND:
	case AUDIO_DEV_AUDIO:
		error = audio_read(sc, uio, ioflag);
		break;
	case AUDIO_DEV_AUDIOCTL:
	case AUDIO_DEV_MIXER:
		error = ENODEV;
		break;
	default:
		error = ENXIO;
	}
	device_unref(&sc->dev);
	return error;
}

int
audiowrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct audio_softc *sc;
	int error;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return ENXIO;
	switch (AUDIO_DEV(dev)) {
	case AUDIO_DEV_SOUND:
	case AUDIO_DEV_AUDIO:
		error = audio_write(sc, uio, ioflag);
		break;
	case AUDIO_DEV_AUDIOCTL:
	case AUDIO_DEV_MIXER:
		error = ENODEV;
		break;
	default:
		error = ENXIO;
	}
	device_unref(&sc->dev);
	return error;
}

int
audioioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct audio_softc *sc;
	int error;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return ENXIO;
	switch (AUDIO_DEV(dev)) {
	case AUDIO_DEV_SOUND:
	case AUDIO_DEV_AUDIO:
		error = audio_ioctl(sc, cmd, addr);
		break;
	case AUDIO_DEV_AUDIOCTL:
		if (cmd == AUDIO_SETINFO && sc->mode != 0) {
			error = EBUSY;
			break;
		}
		error = audio_ioctl(sc, cmd, addr);
		break;
	case AUDIO_DEV_MIXER:
		error = audio_ioctl_mixer(sc, cmd, addr);
		break;
	default:
		error = ENXIO;
	}
	device_unref(&sc->dev);
	return error;
}

int
audiopoll(dev_t dev, int events, struct proc *p)
{
	struct audio_softc *sc;
	int revents;

	sc = (struct audio_softc *)device_lookup(&audio_cd, AUDIO_UNIT(dev));
	if (sc == NULL)
		return POLLERR;
	switch (AUDIO_DEV(dev)) {
	case AUDIO_DEV_SOUND:
	case AUDIO_DEV_AUDIO:
		revents = audio_poll(sc, events, p);
		break;
	case AUDIO_DEV_AUDIOCTL:
	case AUDIO_DEV_MIXER:
		revents = 0;
		break;
	default:
		revents = 0;
	}
	device_unref(&sc->dev);
	return revents;
}

#if NWSKBD > 0
int
wskbd_initmute(struct audio_softc *sc, struct mixer_devinfo *vol)
{
	struct mixer_devinfo mi;

	mi.index = vol->next;
	for (mi.index = vol->next; mi.index != -1; mi.index = mi.next) {
		if (sc->ops->query_devinfo(sc->arg, &mi) != 0)
			break;
		if (strcmp(mi.label.name, AudioNmute) == 0)
			return mi.index;
	}
	return -1;
}

int
wskbd_initvol(struct audio_softc *sc, struct wskbd_vol *vol, char *cn, char *dn)
{
	struct mixer_devinfo dev, cls;

	for (dev.index = 0; ; dev.index++) {
		if (sc->ops->query_devinfo(sc->arg, &dev) != 0)
			break;
		cls.index = dev.mixer_class;
		if (sc->ops->query_devinfo(sc->arg, &cls) != 0)
			continue;
		if (strcmp(cls.label.name, cn) == 0 &&
		    strcmp(dev.label.name, dn) == 0) {
			vol->val = dev.index;
			vol->nch = dev.un.v.num_channels;
			vol->step = dev.un.v.delta > 8 ? dev.un.v.delta : 8;
			vol->mute = wskbd_initmute(sc, &dev);
			vol->val_pending = vol->mute_pending = 0;
			DPRINTF("%s: wskbd using %s.%s, %s\n",
			    DEVNAME(sc), cn, dn, vol->mute >= -1 ? "mute control" : "");
			return 1;
		}
	}
	vol->val = vol->mute = -1;
	return 0;
}

void
wskbd_mixer_init(struct audio_softc *sc)
{
	static struct {
		char *cn, *dn;
	} spkr_names[] = {
		{AudioCoutputs, AudioNmaster},
		{AudioCinputs,  AudioNdac},
		{AudioCoutputs, AudioNdac},
		{AudioCoutputs, AudioNoutput}
	}, mic_names[] = {
		{AudioCrecord, AudioNrecord},
		{AudioCrecord, AudioNvolume},
		{AudioCinputs, AudioNrecord},
		{AudioCinputs, AudioNvolume},
		{AudioCinputs, AudioNinput}
	};
	int i;

	if (sc->dev.dv_unit != 0) {
		DPRINTF("%s: not configuring wskbd keys\n", DEVNAME(sc));
		return;
	}
	for (i = 0; i < sizeof(spkr_names) / sizeof(spkr_names[0]); i++) {
		if (wskbd_initvol(sc, &sc->spkr,
			spkr_names[i].cn, spkr_names[i].dn))
			break;
	}
	for (i = 0; i < sizeof(mic_names) / sizeof(mic_names[0]); i++) {
		if (wskbd_initvol(sc, &sc->mic,
			mic_names[i].cn, mic_names[i].dn))
			break;
	}
}

void
wskbd_mixer_update(struct audio_softc *sc, struct wskbd_vol *vol)
{
	struct mixer_ctrl ctrl;
	int val_pending, mute_pending, i, gain, error, s;

	s = spltty();
	val_pending = vol->val_pending;
	vol->val_pending = 0;
	mute_pending = vol->mute_pending;
	vol->mute_pending = 0;
	splx(s);

	if (sc->ops == NULL)
		return;
	if (vol->mute >= 0 && mute_pending) {
		ctrl.dev = vol->mute;
		ctrl.type = AUDIO_MIXER_ENUM;
		error = sc->ops->get_port(sc->arg, &ctrl);
		if (error) {
			DPRINTF("%s: get mute err = %d\n", DEVNAME(sc), error);
			return;
		}
		ctrl.un.ord = ctrl.un.ord ^ mute_pending;
		DPRINTFN(1, "%s: wskbd mute setting to %d\n",
		    DEVNAME(sc), ctrl.un.ord);
		error = sc->ops->set_port(sc->arg, &ctrl);
		if (error) {
			DPRINTF("%s: set mute err = %d\n", DEVNAME(sc), error);
			return;
		}
	}
	if (vol->val >= 0 && val_pending) {
		ctrl.dev = vol->val;
		ctrl.type = AUDIO_MIXER_VALUE;
		ctrl.un.value.num_channels = vol->nch;
		error = sc->ops->get_port(sc->arg, &ctrl);
		if (error) {
			DPRINTF("%s: get mute err = %d\n", DEVNAME(sc), error);
			return;
		}
		for (i = 0; i < vol->nch; i++) {
			gain = ctrl.un.value.level[i] + vol->step * val_pending;
			if (gain > AUDIO_MAX_GAIN)
				gain = AUDIO_MAX_GAIN;
			if (gain < AUDIO_MIN_GAIN)
				gain = AUDIO_MIN_GAIN;
			ctrl.un.value.level[i] = gain;
			DPRINTFN(1, "%s: wskbd level %d set to %d\n",
			    DEVNAME(sc), i, gain);
		}
		error = sc->ops->set_port(sc->arg, &ctrl);
		if (error) {
			DPRINTF("%s: set vol err = %d\n", DEVNAME(sc), error);
			return;
		}
	}
}

void
wskbd_mixer_cb(void *addr)
{
	struct audio_softc *sc = addr;
	int s;

	wskbd_mixer_update(sc, &sc->spkr);
	wskbd_mixer_update(sc, &sc->mic);
	s = spltty();
	sc->wskbd_taskset = 0;
	splx(s);
	device_unref(&sc->dev);
}

int
wskbd_set_mixervolume(long dir, long out)
{
	struct audio_softc *sc;
	struct wskbd_vol *vol;

	sc = (struct audio_softc *)device_lookup(&audio_cd, 0);
	if (sc == NULL)
		return ENODEV;
	vol = out ? &sc->spkr : &sc->mic;
	if (dir == 0)
		vol->mute_pending ^= 1;
	else
		vol->val_pending += dir;
	if (!sc->wskbd_taskset) {
		task_set(&sc->wskbd_task, wskbd_mixer_cb, sc);
		task_add(systq, &sc->wskbd_task);
		sc->wskbd_taskset = 1;
	}
	return 0;
}
#endif /* NWSKBD > 0 */
