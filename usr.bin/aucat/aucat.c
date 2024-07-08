/*
 * Copyright (c) 2008-2014 Alexandre Ratchov <alex@caoua.org>
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

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sndio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "abuf.h"
#include "afile.h"
#include "dsp.h"
#include "sysex.h"
#include "utils.h"

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
#define MIDI_CTL_VOL	7

/*
 * Max coarse value
 */
#define MIDI_MAXCTL	127

/*
 * MIDI status bytes for sysex
 */
#define MIDI_SX_START	0xf0
#define MIDI_SX_STOP	0xf7

/*
 * audio device defaults
 */
#define DEFAULT_RATE		48000
#define DEFAULT_BUFSZ_MS	200

struct slot {
	struct slot *next;		/* next on the play/rec list */
	int vol;			/* dynamic range */
	int volctl;			/* volume in the 0..127 range */
	struct abuf buf;		/* file i/o buffer */
	int bpf;			/* bytes per frame */
	int imin, imax, omin, omax;	/* channel mapping ranges */
	struct cmap cmap;		/* channel mapper state */
	struct resamp resamp;		/* resampler state */
	struct conv conv;		/* format encoder state */
	int join;			/* channel join factor */
	int expand;			/* channel expand factor */
	void *resampbuf, *convbuf;	/* conversion tmp buffers */
	int dup;			/* compat with legacy -j option */
	int round;			/* slot-side block size */
	int mode;			/* MODE_{PLAY,REC} */
#define SLOT_CFG	0		/* buffers not allocated yet */
#define SLOT_INIT	1		/* not trying to do anything */
#define SLOT_RUN	2		/* playing/recording */
#define SLOT_STOP	3		/* draining (play only) */
	int pstate;			/* one of above */
	long long skip;			/* frames to skip at the beginning */
	long long pos;			/* start position (at device rate) */
	struct afile afile;		/* file desc & friends */
};

/*
 * device properties
 */
unsigned int dev_mode;			/* bitmap of SIO_{PLAY,REC} */
unsigned int dev_bufsz;			/* device buffer size */
unsigned int dev_round;			/* device block size */
int dev_rate;				/* device sample rate (Hz) */
unsigned int dev_pchan, dev_rchan;	/* play & rec channels count */
adata_t *dev_pbuf, *dev_rbuf;		/* play & rec buffers */
struct aparams dev_par;			/* device sample format */
struct conv dev_enc, dev_dec;		/* format conversions */
unsigned char *dev_encbuf, *dev_decbuf;	/* buf for format conversions */
long long dev_pos;			/* last MMC position in frames */
#define DEV_STOP	0		/* stopped */
#define DEV_START	1		/* started */
unsigned int dev_pstate;		/* one of above */
char *dev_name;				/* device sndio(7) name */
char *dev_port;				/* control port sndio(7) name */
struct sio_hdl *dev_sh;			/* device handle */
struct mio_hdl *dev_mh;			/* MIDI control port handle */
unsigned int dev_volctl = MIDI_MAXCTL;	/* master volume */

/*
 * MIDI parser state
 */
#define MIDI_MSGMAX	32		/* max size of MIDI msg */
unsigned char dev_msg[MIDI_MSGMAX];	/* parsed input message */
unsigned int dev_mst;			/* input MIDI running status */
unsigned int dev_mused;			/* bytes used in ``msg'' */
unsigned int dev_midx;			/* current ``msg'' size */
unsigned int dev_mlen;			/* expected ``msg'' length */
unsigned int dev_prime;			/* blocks to write to start */

unsigned int log_level = 1;
volatile sig_atomic_t quit_flag = 0;
struct slot *slot_list = NULL;

/*
 * length of voice and common MIDI messages (status byte included)
 */
const unsigned int voice_len[] = { 3, 3, 3, 3, 2, 2, 3 };
const unsigned int common_len[] = { 0, 2, 3, 2, 0, 0, 1, 1 };

char usagestr[] = "usage: aucat [-dn] [-b size] "
    "[-c channels] [-e enc] [-f device] [-g position]\n\t"
    "[-h fmt] [-i file] [-m min:max/min:max] [-o file] [-p position]\n\t"
    "[-q port] [-r rate] [-v volume]\n";

static void *
allocbuf(int nfr, int nch, int bps)
{
	size_t fsize;
	void *ptr;

	if (nch < 0 || nch > NCHAN_MAX || bps < 0 || bps > 4) {
		log_puts("allocbuf: bogus channels or bytes per sample count\n");
		panic();
	}
	fsize = nch * bps;

	ptr = reallocarray(NULL, nfr, fsize);
	if (ptr == NULL)
		err(1, "reallocarray");
	return ptr;
}

static void
slot_log(struct slot *s)
{
#ifdef DEBUG
	static char *pstates[] = {
		"cfg", "ini", "run", "stp"
	};
#endif
	log_puts(s->afile.path);
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts(",pst=");
		log_puts(pstates[s->pstate]);
	}
#endif
}

static void
slot_flush(struct slot *s)
{
	int count, n;
	unsigned char *data;

	for (;;) {
		data = abuf_rgetblk(&s->buf, &count);
		if (count == 0)
			break;
		n = afile_write(&s->afile, data, count);
		if (n == 0) {
			slot_log(s);
			log_puts(": can't write, disabled\n");
			s->pstate = SLOT_INIT;
			return;
		}
		abuf_rdiscard(&s->buf, n);
	}
}

static void
slot_fill(struct slot *s)
{
	int count, n;
	unsigned char *data;

	for (;;) {
		data = abuf_wgetblk(&s->buf, &count);
		if (count == 0)
			break;
		n = afile_read(&s->afile, data, count);
		if (n == 0) {
#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": eof reached, stopping\n");
			}
#endif
			s->pstate = SLOT_STOP;
			break;
		}
		abuf_wcommit(&s->buf, n);
	}
}

static int
slot_new(char *path, int mode, struct aparams *par, int hdr,
    int imin, int imax, int omin, int omax, int nch,
    int rate, int dup, int vol, long long pos)
{
	struct slot *s, **ps;

	s = xmalloc(sizeof(struct slot));
	if (!afile_open(&s->afile, path, hdr,
		mode == SIO_PLAY ? AFILE_FREAD : AFILE_FWRITE,
		par, rate, nch)) {
		xfree(s);
		return 0;
	}
	s->imin = (imin != -1) ? imin : 0;
	s->imax = (imax != -1) ? imax : s->imin + s->afile.nch - 1;
	s->omin = (omin != -1) ? omin : 0;
	s->omax = (omax != -1) ? omax : s->omin + s->afile.nch - 1;
	s->dup = dup;
	s->vol = MIDI_TO_ADATA(vol);
	s->mode = mode;
	s->pstate = SLOT_CFG;
	s->pos = pos;
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": ");
		log_puts(s->mode == SIO_PLAY ? "play" : "rec");
		log_puts(", ");
		log_putu(s->afile.nch);
		log_puts("ch (");
		log_putu(s->imin);
		log_puts(":");
		log_putu(s->imax);
		log_puts("/");
		log_putu(s->omin);
		log_puts(":");
		log_putu(s->omax);
		log_puts("), ");
		log_putu(s->afile.rate);
		log_puts("Hz, ");
		switch (s->afile.fmt) {
		case AFILE_FMT_PCM:
			aparams_log(&s->afile.par);
			break;
		case AFILE_FMT_ULAW:
			log_puts("ulaw");
			break;
		case AFILE_FMT_ALAW:
			log_puts("alaw");
			break;
		case AFILE_FMT_FLOAT:
			log_puts("f32le");
			break;
		}
		if (s->mode == SIO_PLAY && s->afile.endpos >= 0) {
			log_puts(", bytes ");
			log_puti(s->afile.startpos);
			log_puts("..");
			log_puti(s->afile.endpos);
		}
		if (s->mode == SIO_PLAY) {
			log_puts(", vol ");
			log_puti(s->vol);
		}
		log_puts("\n");
	}
	for (ps = &slot_list; *ps != NULL; ps = &(*ps)->next)
		;
	s->next = NULL;
	*ps = s;
	return 1;
}

static void
slot_init(struct slot *s)
{
	unsigned int inch, onch, bufsz;

#ifdef DEBUG
	if (s->pstate != SLOT_CFG) {
		slot_log(s);
		log_puts(": slot_init: wrong state\n");
		panic();
	}
#endif
	s->bpf = s->afile.par.bps * s->afile.nch;
	s->round = ((long long)dev_round * s->afile.rate +
	    dev_rate - 1) / dev_rate;

	bufsz = s->round * (dev_bufsz / dev_round);
	bufsz -= bufsz % s->round;
	if (bufsz == 0)
		bufsz = s->round;
	abuf_init(&s->buf, bufsz * s->bpf);
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": allocated ");
		log_putu(bufsz);
		log_puts(" frame buffer\n");
	}
#endif

	s->convbuf = NULL;
	s->resampbuf = NULL;
	s->join = 1;
	s->expand = 1;
	inch = s->imax - s->imin + 1;
	onch = s->omax - s->omin + 1;
	if (s->dup) {
		/* compat with legacy -j option */
		if (s->mode == SIO_PLAY)
			onch = dev_pchan;
		else
			inch = dev_rchan;
	}
	if (onch > inch)
		s->expand = onch / inch;
	else if (onch < inch)
		s->join = inch / onch;
	if (s->mode & SIO_PLAY) {
		cmap_init(&s->cmap,
		    0, s->afile.nch - 1, s->imin, s->imax,
		    0, dev_pchan - 1, s->omin, s->omax);
		if (s->afile.fmt != AFILE_FMT_PCM ||
		    !aparams_native(&s->afile.par)) {
			dec_init(&s->conv, &s->afile.par, s->afile.nch);
			s->convbuf = allocbuf(s->round, s->afile.nch, sizeof(adata_t));
		}
		if (s->afile.rate != dev_rate) {
			resamp_init(&s->resamp, s->afile.rate, dev_rate,
			    s->afile.nch);
			s->resampbuf = allocbuf(dev_round, s->afile.nch, sizeof(adata_t));
		}
	}
	if (s->mode & SIO_REC) {
		cmap_init(&s->cmap,
		    0, dev_rchan - 1, s->imin, s->imax,
		    0, s->afile.nch - 1, s->omin, s->omax);
		if (s->afile.rate != dev_rate) {
			resamp_init(&s->resamp, dev_rate, s->afile.rate,
			    s->afile.nch);
			s->resampbuf = allocbuf(dev_round, s->afile.nch, sizeof(adata_t));
		}
		if (!aparams_native(&s->afile.par)) {
			enc_init(&s->conv, &s->afile.par, s->afile.nch);
			s->convbuf = allocbuf(s->round, s->afile.nch, sizeof(adata_t));
		}

		/*
		 * cmap_copy() doesn't write samples in all channels,
	         * for instance when mono->stereo conversion is
	         * disabled. So we have to prefill cmap_copy() output
	         * with silence.
	         */
		if (s->resampbuf) {
			memset(s->resampbuf, 0,
			    dev_round * s->afile.nch * sizeof(adata_t));
		} else if (s->convbuf) {
			memset(s->convbuf, 0,
			    s->round * s->afile.nch * sizeof(adata_t));
		} else {
			memset(s->buf.data, 0,
			    bufsz * s->afile.nch * sizeof(adata_t));
		}
	}
	s->pstate = SLOT_INIT;
#ifdef DEBUG
	if (log_level >= 3) {
		slot_log(s);
		log_puts(": chain initialized\n");
	}
#endif
}

static void
slot_start(struct slot *s, long long pos)
{
#ifdef DEBUG
	if (s->pstate != SLOT_INIT) {
		slot_log(s);
		log_puts(": slot_start: wrong state\n");
		panic();
	}
#endif
	pos -= s->pos;
	if (pos < 0) {
		s->skip = -pos;
		pos = 0;
	} else
		s->skip = 0;

	/*
	 * convert pos to slot sample rate
	 *
	 * At this stage, we could adjust s->resamp.diff to get
	 * sub-frame accuracy.
	 */
	pos = pos * s->afile.rate / dev_rate;

	if (!afile_seek(&s->afile, pos * s->bpf)) {
		s->pstate = SLOT_INIT;
		return;
	}
	s->pstate = SLOT_RUN;
	if (s->mode & SIO_PLAY)
		slot_fill(s);
#ifdef DEBUG
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": started\n");
	}
#endif
}

static void
slot_stop(struct slot *s)
{
	if (s->pstate == SLOT_INIT)
		return;
	if (s->mode & SIO_REC)
		slot_flush(s);
	if (s->mode & SIO_PLAY)
		s->buf.used = s->buf.start = 0;
	s->pstate = SLOT_INIT;
#ifdef DEBUG
	if (log_level >= 2) {
		slot_log(s);
		log_puts(": stopped\n");
	}
#endif
}

static void
slot_del(struct slot *s)
{
	struct slot **ps;

	if (s->pstate != SLOT_CFG) {
		slot_stop(s);
		afile_close(&s->afile);
#ifdef DEBUG
		if (log_level >= 3) {
			slot_log(s);
			log_puts(": closed\n");
		}
#endif
		abuf_done(&s->buf);
		if (s->resampbuf)
			xfree(s->resampbuf);
		if (s->convbuf)
			xfree(s->convbuf);
	}
	for (ps = &slot_list; *ps != s; ps = &(*ps)->next)
		; /* nothing */
	*ps = s->next;
	xfree(s);
}

static void
slot_getcnt(struct slot *s, int *icnt, int *ocnt)
{
	int cnt;

	if (s->resampbuf)
		resamp_getcnt(&s->resamp, icnt, ocnt);
	else {
		cnt = (*icnt < *ocnt) ? *icnt : *ocnt;
		*icnt = cnt;
		*ocnt = cnt;
	}
}

static void
play_filt_resamp(struct slot *s, void *res_in, void *out, int icnt, int ocnt)
{
	int i, offs, vol, inch, onch;
	void *in;

	if (s->resampbuf) {
		resamp_do(&s->resamp, res_in, s->resampbuf, icnt, ocnt);
		in = s->resampbuf;
	} else
		in = res_in;

	inch = s->imax - s->imin + 1;
	onch = s->omax - s->omin + 1;
	vol = s->vol / s->join; /* XXX */
	cmap_add(&s->cmap, in, out, vol, ocnt);

	offs = 0;
	for (i = s->join - 1; i > 0; i--) {
		offs += onch;
		if (offs + s->cmap.nch > s->afile.nch)
			break;
		cmap_add(&s->cmap, (adata_t *)in + offs, out, vol, ocnt);
	}

	offs = 0;
	for (i = s->expand - 1; i > 0; i--) {
		offs += inch;
		if (offs + s->cmap.nch > dev_pchan)
			break;
		cmap_add(&s->cmap, in, (adata_t *)out + offs, vol, ocnt);
	}
}

static void
play_filt_dec(struct slot *s, void *in, void *out, int icnt, int ocnt)
{
	void *tmp;

	tmp = s->convbuf;
	if (tmp) {
		switch (s->afile.fmt) {
		case AFILE_FMT_PCM:
			dec_do(&s->conv, in, tmp, icnt);
			break;
		case AFILE_FMT_ULAW:
			dec_do_ulaw(&s->conv, in, tmp, icnt, 0);
			break;
		case AFILE_FMT_ALAW:
			dec_do_ulaw(&s->conv, in, tmp, icnt, 1);
			break;
		case AFILE_FMT_FLOAT:
			dec_do_float(&s->conv, in, tmp, icnt);
			break;
		}
	} else
		tmp = in;
	play_filt_resamp(s, tmp, out, icnt, ocnt);
}

/*
 * Mix as many as possible frames (but not more than a block) from the
 * slot buffer to the given location. Return the number of frames mixed
 * in the output buffer
 */
static int
slot_mix_badd(struct slot *s, adata_t *odata)
{
	adata_t *idata;
	int len, icnt, ocnt, otodo, odone;

	odone = 0;
	otodo = dev_round;
	if (s->skip > 0) {		
		ocnt = otodo;
		if (ocnt > s->skip)
			ocnt = s->skip;
		s->skip -= ocnt;
		odata += dev_pchan * ocnt;
		otodo -= ocnt;
		odone += ocnt;
	}
	while (otodo > 0) {
		idata = (adata_t *)abuf_rgetblk(&s->buf, &len);
		icnt = len / s->bpf;
		if (icnt > s->round)
			icnt = s->round;
		ocnt = otodo;
		slot_getcnt(s, &icnt, &ocnt);
		if (icnt == 0)
			break;
		play_filt_dec(s, idata, odata, icnt, ocnt);
		abuf_rdiscard(&s->buf, icnt * s->bpf);
		otodo -= ocnt;
		odone += ocnt;
		odata += ocnt * dev_pchan;
	}
	return odone;
}

static void
rec_filt_resamp(struct slot *s, void *in, void *res_out, int icnt, int ocnt)
{
	int i, vol, offs, inch, onch;
	void *out = res_out;

	out = (s->resampbuf) ? s->resampbuf : res_out;

	inch = s->imax - s->imin + 1;
	onch = s->omax - s->omin + 1;
	vol = ADATA_UNIT / s->join;
	cmap_copy(&s->cmap, in, out, vol, icnt);

	offs = 0;
	for (i = s->join - 1; i > 0; i--) {
		offs += onch;
		if (offs + s->cmap.nch > dev_rchan)
			break;
		cmap_add(&s->cmap, (adata_t *)in + offs, out, vol, icnt);
	}
	offs = 0;
	for (i = s->expand - 1; i > 0; i--) {
		offs += inch;
		if (offs + s->cmap.nch > s->afile.nch)
			break;
		cmap_copy(&s->cmap, in, (adata_t *)out + offs, vol, icnt);
	}
	if (s->resampbuf)
		resamp_do(&s->resamp, s->resampbuf, res_out, icnt, ocnt);
	else
		ocnt = icnt;
}

static void
rec_filt_enc(struct slot *s, void *in, void *out, int icnt, int ocnt)
{
	void *tmp;

	tmp = s->convbuf;
	rec_filt_resamp(s, in, tmp ? tmp : out, icnt, ocnt);
	if (tmp)
		enc_do(&s->conv, tmp, out, ocnt);
}

/*
 * Copy "todo" frames from the given buffer to the slot buffer,
 * but not more than a block.
 */
static void
slot_sub_bcopy(struct slot *s, adata_t *idata, int itodo)
{
	adata_t *odata;
	int len, icnt, ocnt;

	if (s->skip > 0) {
		icnt = itodo;
		if (icnt > s->skip)
			icnt = s->skip;
		s->skip -= icnt;
		idata += dev_rchan * icnt;
		itodo -= icnt;
	}

	while (itodo > 0) {
		odata = (adata_t *)abuf_wgetblk(&s->buf, &len);
		ocnt = len / s->bpf;
		if (ocnt > s->round)
			ocnt = s->round;
		icnt = itodo;
		slot_getcnt(s, &icnt, &ocnt);
		if (ocnt == 0)
			break;
		rec_filt_enc(s, idata, odata, icnt, ocnt);
		abuf_wcommit(&s->buf, ocnt * s->bpf);
		itodo -= icnt;
		idata += icnt * dev_rchan;
	}
}

static int
dev_open(char *dev, int mode, int bufsz, char *port)
{
	int rate, pmax, rmax;
	struct sio_par par;
	char encstr[ENCMAX];
	struct slot *s;

	if (port) {
		dev_port = port;
		dev_mh = mio_open(dev_port, MIO_IN, 0);
		if (dev_mh == NULL) {
			log_puts(port);
			log_puts(": couldn't open midi port\n");
			return 0;
		}
	} else
		dev_mh = NULL;

	dev_name = dev;
	dev_sh = sio_open(dev, mode, 0);
	if (dev_sh == NULL) {
		log_puts(dev_name);
		log_puts(": couldn't open audio device\n");
		return 0;
	}

	rate = pmax = rmax = 0;
	for (s = slot_list; s != NULL; s = s->next) {
		if (s->afile.rate > rate)
			rate = s->afile.rate;
		if (s->mode == SIO_PLAY) {
			if (s->omax > pmax)
				pmax = s->omax;
		}
		if (s->mode == SIO_REC) {
			if (s->imax > rmax)
				rmax = s->imax;
		}
	}
	sio_initpar(&par);
	par.bits = ADATA_BITS;
	par.bps = sizeof(adata_t);
	par.msb = 0;
	par.le = SIO_LE_NATIVE;
	par.rate = rate;
	if (mode & SIO_PLAY)
		par.pchan = pmax + 1;
	if (mode & SIO_REC)
		par.rchan = rmax + 1;
	par.appbufsz = bufsz > 0 ? bufsz : rate * DEFAULT_BUFSZ_MS / 1000;
	if (!sio_setpar(dev_sh, &par) || !sio_getpar(dev_sh, &par)) {
		log_puts(dev_name);
		log_puts(": couldn't set audio params\n");
		return 0;
	}
	dev_par.bits = par.bits;
	dev_par.bps = par.bps;
	dev_par.sig = par.sig;
	dev_par.le = par.le;
	dev_par.msb = par.msb;
	dev_mode = mode;
	dev_rate = par.rate;
	dev_bufsz = par.bufsz;
	dev_round = par.round;
	if (mode & SIO_PLAY) {
		dev_pchan = par.pchan;
		dev_pbuf = allocbuf(dev_round, dev_pchan, sizeof(adata_t));
	}
	if (mode & SIO_REC) {
		dev_rchan = par.rchan;
		dev_rbuf = allocbuf(dev_round, dev_rchan, sizeof(adata_t));
	}
	if (!aparams_native(&dev_par)) {
		if (mode & SIO_PLAY) {
			dev_encbuf = allocbuf(dev_round, dev_pchan, dev_par.bps);
			enc_init(&dev_enc, &dev_par, dev_pchan);
		}
		if (mode & SIO_REC) {
			dev_decbuf = allocbuf(dev_round, dev_rchan, dev_par.bps);
			dec_init(&dev_dec, &dev_par, dev_rchan);
		}
	}
	dev_pstate = DEV_STOP;
	if (log_level >= 2) {
		log_puts(dev_name);
		log_puts(": ");
		log_putu(dev_rate);
		log_puts("Hz, ");
		aparams_enctostr(&dev_par, encstr);
		log_puts(encstr);
		if (dev_mode & SIO_PLAY) {
			log_puts(", play 0:");
			log_puti(dev_pchan - 1);
		}
		if (dev_mode & SIO_REC) {
			log_puts(", rec 0:");
			log_puti(dev_rchan - 1);
		}
		log_puts(", ");
		log_putu(dev_bufsz / dev_round);
		log_puts(" blocks of ");
		log_putu(dev_round);
		log_puts(" frames\n");
	}
	return 1;
}

static void
dev_close(void)
{
	sio_close(dev_sh);
	if (dev_mh)
		mio_close(dev_mh);
	if (dev_mode & SIO_PLAY)
		xfree(dev_pbuf);
	if (dev_mode & SIO_REC)
		xfree(dev_rbuf);
}

static void
dev_master(int val)
{
	struct slot *s;
	int mastervol, slotvol;

	mastervol = MIDI_TO_ADATA(dev_volctl);
	for (s = slot_list; s != NULL; s = s->next) {
		slotvol = MIDI_TO_ADATA(val);
		s->vol = ADATA_MUL(mastervol, slotvol);
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("master volume set to ");
		log_putu(val);
		log_puts("\n");
	}
#endif
}

static void
dev_slotvol(int midich, int val)
{
	struct slot *s;
	int mastervol, slotvol;

	for (s = slot_list; s != NULL; s = s->next) {
		if (midich == 0) {
			mastervol = MIDI_TO_ADATA(dev_volctl);
			slotvol = MIDI_TO_ADATA(val);
			s->vol = ADATA_MUL(mastervol, slotvol);
#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": volume set to ");
				log_putu(val);
				log_puts("\n");
			}
#endif
			break;
		}
		midich--;
	}
}

/*
 * start all slots simultaneously
 */
static void
dev_mmcstart(void)
{
	struct slot *s;

	if (dev_pstate == DEV_STOP) {
		dev_pstate = DEV_START;
		for (s = slot_list; s != NULL; s = s->next)
			slot_start(s, dev_pos);
		dev_prime = (dev_mode & SIO_PLAY) ? dev_bufsz / dev_round : 0;
		sio_start(dev_sh);
		if (log_level >= 2)
			log_puts("started\n");
	} else {
#ifdef DEBUG
		if (log_level >= 3)
			log_puts("ignoring mmc start\n");
#endif
	}
}

/*
 * stop all slots simultaneously
 */
static void
dev_mmcstop(void)
{
	struct slot *s;

	if (dev_pstate == DEV_START) {
		dev_pstate = DEV_STOP;
		for (s = slot_list; s != NULL; s = s->next)
			slot_stop(s);
		sio_stop(dev_sh);
		if (log_level >= 2)
			log_puts("stopped\n");
	} else {
#ifdef DEBUG
		if (log_level >= 3)
			log_puts("ignored mmc stop\n");
#endif
	}
}

/*
 * relocate all slots simultaneously
 */
static void
dev_mmcloc(int hr, int min, int sec, int fr, int cent, int fps)
{
	long long pos;

	pos = (long long)dev_rate * hr * 3600 +
	    (long long)dev_rate * min * 60 +
	    (long long)dev_rate * sec +
	    (long long)dev_rate * fr / fps +
	    (long long)dev_rate * cent / (100 * fps);
	if (dev_pos == pos)
		return;
	dev_pos = pos;
	if (log_level >= 2) {
		log_puts("relocated to ");
		log_putu(hr);
		log_puts(":");
		log_putu(min);
		log_puts(":");
		log_putu(sec);
		log_puts(".");
		log_putu(fr);
		log_puts(".");
		log_putu(cent);
		log_puts(" at ");
		log_putu(fps);
		log_puts("fps\n");
	}
	if (dev_pstate == DEV_START) {
		dev_mmcstop();
		dev_mmcstart();
	}
}

static void
dev_imsg(unsigned char *msg, unsigned int len)
{
	struct sysex *x;
	unsigned int fps, chan;

	if ((msg[0] & MIDI_CMDMASK) == MIDI_CTL && msg[1] == MIDI_CTL_VOL) {
		chan = msg[0] & MIDI_CHANMASK;
		dev_slotvol(chan, msg[2]);
		return;
	}
	x = (struct sysex *)msg;
	if (x->start != SYSEX_START)
		return;
	if (len < SYSEX_SIZE(empty))
		return;
	if (x->type != SYSEX_TYPE_RT)
		return;
	if (x->id0 == SYSEX_CONTROL && x->id1 == SYSEX_MASTER) {
		if (len == SYSEX_SIZE(master))
			dev_master(x->u.master.coarse);
		return;
	}
	if (x->id0 != SYSEX_MMC)
		return;
	switch (x->id1) {
	case SYSEX_MMC_STOP:
		if (len != SYSEX_SIZE(stop))
			return;
		dev_mmcstop();
		break;
	case SYSEX_MMC_START:
		if (len != SYSEX_SIZE(start))
			return;
		dev_mmcstart();
		break;
	case SYSEX_MMC_LOC:
		if (len != SYSEX_SIZE(loc) ||
		    x->u.loc.len != SYSEX_MMC_LOC_LEN ||
		    x->u.loc.cmd != SYSEX_MMC_LOC_CMD)
			return;
		switch (x->u.loc.hr >> 5) {
		case MTC_FPS_24:
			fps = 24;
			break;
		case MTC_FPS_25:
			fps = 25;
			break;
		case MTC_FPS_30:
			fps = 30;
			break;
		default:
			dev_mmcstop();
			return;
		}
		dev_mmcloc(x->u.loc.hr & 0x1f,
		    x->u.loc.min,
		    x->u.loc.sec,
		    x->u.loc.fr,
		    x->u.loc.cent,
		    fps);
		break;
	}
}

/*
 * parse the given data chunk and call imsg() for each message
 */
static void
midi_in(unsigned char *idata, int icount)
{
	int i;
	unsigned char c;

	for (i = 0; i < icount; i++) {
		c = *idata++;
		if (c >= 0xf8) {
			/* we don't use real-time events */
		} else if (c == SYSEX_END) {
			if (dev_mst == SYSEX_START) {
				dev_msg[dev_midx++] = c;
				dev_imsg(dev_msg, dev_midx);
			}
			dev_mst = 0;
			dev_midx = 0;
		} else if (c >= 0xf0) {
			dev_msg[0] = c;
			dev_mlen = common_len[c & 7];
			dev_mst = c;
			dev_midx = 1;
		} else if (c >= 0x80) {
			dev_msg[0] = c;
			dev_mlen = voice_len[(c >> 4) & 7];
			dev_mst = c;
			dev_midx = 1;
		} else if (dev_mst) {
			if (dev_midx == 0 && dev_mst != SYSEX_START)
				dev_msg[dev_midx++] = dev_mst;
			dev_msg[dev_midx++] = c;
			if (dev_midx == dev_mlen) {
				dev_imsg(dev_msg, dev_midx);
				if (dev_mst >= 0xf0)
					dev_mst = 0;
				dev_midx = 0;
			} else if (dev_midx == MIDI_MSGMAX) {
				/* sysex too long */
				dev_mst = 0;
			}
		}
	}
}

static int
slot_list_mix(unsigned int round, unsigned int pchan, adata_t *pbuf)
{
	unsigned int done, n;
	struct slot *s;

	memset(pbuf, 0, pchan * round * sizeof(adata_t));
	done = 0;
	for (s = slot_list; s != NULL; s = s->next) {
		if (s->pstate == SLOT_INIT || !(s->mode & SIO_PLAY))
			continue;
		if (s->pstate == SLOT_STOP && s->buf.used < s->bpf) {
#ifdef DEBUG
			if (log_level >= 3) {
				slot_log(s);
				log_puts(": drained, done\n");
			}
#endif
			slot_stop(s);
			continue;
		}
		n = slot_mix_badd(s, dev_pbuf);
		if (n > done)
			done = n;
	}
	return done;
}

static int
slot_list_copy(unsigned int count, unsigned int rchan, adata_t *rbuf)
{
	unsigned int done;
	struct slot *s;

	done = 0;
	for (s = slot_list; s != NULL; s = s->next) {
		if (s->pstate == SLOT_INIT || !(s->mode & SIO_REC))
			continue;
		slot_sub_bcopy(s, rbuf, count);
		done = count;
	}
	return done;
}

static void
slot_list_iodo(void)
{
	struct slot *s;

	for (s = slot_list; s != NULL; s = s->next) {
		if (s->pstate != SLOT_RUN)
			continue;
		if ((s->mode & SIO_PLAY) &&
		    (s->buf.used < s->round * s->bpf))
			slot_fill(s);
		if ((s->mode & SIO_REC) &&
		    (s->buf.len - s->buf.used < s->round * s->bpf))
			slot_flush(s);
	}
}

static int
offline(void)
{
	unsigned int todo;
	int rate, cmax;
	struct slot *s;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	rate = cmax = 0;
	for (s = slot_list; s != NULL; s = s->next) {
		if (s->afile.rate > rate)
			rate = s->afile.rate;
		if (s->imax > cmax)
			cmax = s->imax;
		if (s->omax > cmax)
			cmax = s->omax;
	}
	dev_sh = NULL;
	dev_name = "offline";
	dev_mode = SIO_PLAY | SIO_REC;
	dev_rate = rate;
	dev_bufsz = rate;
	dev_round = rate;
	dev_pchan = dev_rchan = cmax + 1;
	dev_pbuf = dev_rbuf = allocbuf(dev_round, dev_pchan, sizeof(adata_t));
	dev_pstate = DEV_STOP;
	for (s = slot_list; s != NULL; s = s->next)
		slot_init(s);
	for (s = slot_list; s != NULL; s = s->next)
		slot_start(s, 0);
	for (;;) {
		todo = slot_list_mix(dev_round, dev_pchan, dev_pbuf);
		if (todo == 0)
			break;
		slot_list_copy(todo, dev_pchan, dev_pbuf);
		slot_list_iodo();
	}
	xfree(dev_pbuf);
	while (slot_list)
		slot_del(slot_list);
	return 1;
}

static int
playrec_cycle(void)
{
	unsigned int n, todo;
	unsigned char *p;
	int pcnt, rcnt;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts(dev_name);
		log_puts(": cycle, prime = ");
		log_putu(dev_prime);
		log_puts("\n");
	}
#endif
	pcnt = rcnt = 0;
	if (dev_mode & SIO_REC) {
		if (dev_prime > 0)
			dev_prime--;
		else {
			todo = dev_round * dev_rchan * dev_par.bps;
			p = dev_decbuf ? dev_decbuf : (unsigned char *)dev_rbuf;
			while (todo > 0) {
				n = sio_read(dev_sh, p, todo);
				if (n == 0) {
					log_puts(dev_name);
					log_puts(": failed to read "
					    "from device\n");
					return 0;
				}
				p += n;
				todo -= n;
			}
			rcnt = slot_list_copy(dev_round, dev_rchan, dev_rbuf);
			if (dev_decbuf) {
				dec_do(&dev_dec,
				    dev_decbuf, (unsigned char *)dev_rbuf,
				    dev_round);
			}
		}
	}
	if (dev_mode & SIO_PLAY) {
		pcnt = slot_list_mix(dev_round, dev_pchan, dev_pbuf);
		todo = dev_par.bps * dev_pchan * dev_round;
		if (dev_encbuf) {
			enc_do(&dev_enc,
			    (unsigned char *)dev_pbuf, dev_encbuf,
			    dev_round);
			p = dev_encbuf;
		} else
			p = (unsigned char *)dev_pbuf;
		n = sio_write(dev_sh, p, todo);
		if (n == 0) {
			log_puts(dev_name);
			log_puts(": failed to write to device\n");
			return 0;
		}
	}
	slot_list_iodo();
	return pcnt > 0 || rcnt > 0;
}

static void
sigint(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

static int
playrec(char *dev, int mode, int bufsz, char *port)
{
#define MIDIBUFSZ 0x100
	unsigned char mbuf[MIDIBUFSZ];
	struct sigaction sa;
	struct pollfd *pfds;
	struct slot *s;
	int n, ns, nm, ev;

	if (!dev_open(dev, mode, bufsz, port))
		return 0;
	if (pledge("stdio audio", NULL) == -1)
		err(1, "pledge");

	n = sio_nfds(dev_sh);
	if (dev_mh)
		n += mio_nfds(dev_mh);
	pfds = reallocarray(NULL, n, sizeof(struct pollfd));
	if (pfds == NULL)
		err(1, "malloc");

	for (s = slot_list; s != NULL; s = s->next)
		slot_init(s);
	if (dev_mh == NULL)
		dev_mmcstart();
	else {
		if (log_level >= 2)
			log_puts("ready, waiting for mmc messages\n");
	}

	quit_flag = 0;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigint;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	while (!quit_flag) {
		if (dev_pstate == DEV_START) {
			ev = 0;
			if (mode & SIO_PLAY)
				ev |= POLLOUT;
			if (mode & SIO_REC)
				ev |= POLLIN;
			ns = sio_pollfd(dev_sh, pfds, ev);
		} else
			ns = 0;
		if (dev_mh)
			nm = mio_pollfd(dev_mh, pfds + ns, POLLIN);
		else
			nm = 0;
		if (poll(pfds, ns + nm, -1) == -1) {
			if (errno == EINTR)
				continue;
			log_puts("poll failed\n");
			panic();
		}
		if (dev_pstate == DEV_START) {
			ev = sio_revents(dev_sh, pfds);
			if (ev & POLLHUP) {
				log_puts(dev);
				log_puts(": audio device gone, stopping\n");
				break;
			}
			if (ev & (POLLIN | POLLOUT)) {
				if (!playrec_cycle() && dev_mh == NULL)
					break;
			}
		}
		if (dev_mh) {
			ev = mio_revents(dev_mh, pfds + ns);
			if (ev & POLLHUP) {
				log_puts(dev_port);
				log_puts(": midi port gone, stopping\n");
				break;
			}
			if (ev & POLLIN) {
				n = mio_read(dev_mh, mbuf, MIDIBUFSZ);
				midi_in(mbuf, n);
			}
		}
	}
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	if (dev_pstate == DEV_START)
		dev_mmcstop();
	xfree(pfds);
	dev_close();
	while (slot_list)
		slot_del(slot_list);
	return 1;
}

static int
opt_onoff(char *s, int *flag)
{
	if (strcmp("off", s) == 0) {
		*flag = 0;
		return 1;
	}
	if (strcmp("on", s) == 0) {
		*flag = 1;
		return 1;
	}
	log_puts(s);
	log_puts(": on/off expected\n");
	return 0;
}

static int
opt_enc(char *s, struct aparams *par)
{
	int len;

	len = aparams_strtoenc(par, s);
	if (len == 0 || s[len] != '\0') {
		log_puts(s);
		log_puts(": bad encoding\n");
		return 0;
	}
	return 1;
}

static int
opt_hdr(char *s, int *hdr)
{
	if (strcmp("auto", s) == 0) {
		*hdr = AFILE_HDR_AUTO;
		return 1;
	}
	if (strcmp("raw", s) == 0) {
		*hdr = AFILE_HDR_RAW;
		return 1;
	}
	if (strcmp("wav", s) == 0) {
		*hdr = AFILE_HDR_WAV;
		return 1;
	}
	if (strcmp("aiff", s) == 0) {
		*hdr = AFILE_HDR_AIFF;
		return 1;
	}
	if (strcmp("au", s) == 0) {
		*hdr = AFILE_HDR_AU;
		return 1;
	}
	log_puts(s);
	log_puts(": bad header type\n");
	return 0;
}

static int
opt_map(char *str, int *rimin, int *rimax, int *romin, int *romax)
{
	char *s, *next;
	long imin, imax, omin, omax;

	errno = 0;
	s = str;
	imin = strtol(s, &next, 10);
	if (next == s || *next != ':')
		goto failed;
	s = next + 1;
	imax = strtol(s, &next, 10);
	if (next == s || *next != '/')
		goto failed;
	s = next + 1;
	omin = strtol(s, &next, 10);
	if (next == s || *next != ':')
		goto failed;
	s = next + 1;
	omax = strtol(s, &next, 10);
	if (next == s || *next != '\0')
		goto failed;
	if (imin < 0 || imax < imin || imax >= NCHAN_MAX)
		goto failed;
	if (omin < 0 || omax < omin || omax >= NCHAN_MAX)
		goto failed;
	*rimin = imin;
	*rimax = imax;
	*romin = omin;
	*romax = omax;
	return 1;
failed:
	log_puts(str);
	log_puts(": channel mapping expected\n");
	return 0;
}

static int
opt_nch(char *str, int *rnch, int *roff)
{
	char *s, *next;
	long nch, off, cmin, cmax;

	errno = 0;
	s = str;
	nch = strtol(s, &next, 10);
	if (next == s)
		goto failed;
	if (*next == ':') {
		/* compat with legacy -c syntax */
		s = next + 1;
		cmin = nch;
		cmax = strtol(s, &next, 10);
		if (next == s)
			goto failed;
		if (cmin < 0 || cmax < cmin || cmax >= NCHAN_MAX)
			goto failed;
		nch = cmax - cmin + 1;
		off = cmin;
	} else {
		off = 0;
		if (nch < 0 || nch >= NCHAN_MAX)
			goto failed;
	}
	if (*next != '\0')
		goto failed;
	*rnch = nch;
	*roff = off;
	return 1;
failed:
	log_puts(str);
	log_puts(": channel count expected\n");
	return 0;
}

static int
opt_num(char *s, int min, int max, int *num)
{
	const char *errstr;

	*num = strtonum(s, min, max, &errstr);
	if (errstr) {
		log_puts(s);
		log_puts(": expected integer between ");
		log_puti(min);
		log_puts(" and ");
		log_puti(max);
		log_puts("\n");
		return 0;
	}
	return 1;
}

static int
opt_pos(char *s, long long *pos)
{
	const char *errstr;

	*pos = strtonum(s, 0, LLONG_MAX, &errstr);
	if (errstr) {
		log_puts(s);
		log_puts(": positive number of samples expected\n");
		return 0;
	}
	return 1;
}

int
main(int argc, char **argv)
{
	int dup, imin, imax, omin, omax, nch, off, rate, vol, bufsz, hdr, mode;
	char *port, *dev;
	struct aparams par;
	int n_flag, c;
	long long pos;

	if (pledge("stdio rpath wpath cpath inet unix dns audio", NULL) == -1)
		err(1, "pledge");

	vol = 127;
	dup = 0;
	bufsz = 0;
	nch = 2;
	off = 0;
	rate = DEFAULT_RATE;
	imin = imax = omin = omax = -1;
	par.bits = ADATA_BITS;
	par.bps = APARAMS_BPS(par.bits);
	par.le = ADATA_LE;
	par.sig = 1;
	par.msb = 1;
	hdr = AFILE_HDR_AUTO;
	n_flag = 0;
	port = NULL;
	dev = NULL;
	mode = 0;
	pos = 0;

	while ((c = getopt(argc, argv,
		"b:c:de:f:g:h:i:j:m:no:p:q:r:t:v:")) != -1) {
		switch (c) {
		case 'b':
			if (!opt_num(optarg, 1, RATE_MAX, &bufsz))
				return 1;
			break;
		case 'c':
			if (!opt_nch(optarg, &nch, &off))
				return 1;
			break;
		case 'd':
			log_level++;
			break;
		case 'e':
			if (!opt_enc(optarg, &par))
				return 1;
			break;
		case 'f':
			dev = optarg;
			break;
		case 'g':
			if (!opt_pos(optarg, &dev_pos))
				return 1;
			break;
		case 'h':
			if (!opt_hdr(optarg, &hdr))
				return 1;
			break;
		case 'i':
			if (off > 0) {
				/* compat with legacy -c syntax */
				omin = off;
				omax = off + nch - 1;
			}
			if (!slot_new(optarg, SIO_PLAY,
				&par, hdr, imin, imax, omin, omax,
				nch, rate, dup, vol, pos))
				return 1;
			mode |= SIO_PLAY;
			imin = imax = omin = omax = -1;
			break;
		case 'j':
			/* compat with legacy -j option */
			if (!opt_onoff(optarg, &dup))
				return 1;
			break;
		case 'm':
			if (!opt_map(optarg, &imin, &imax, &omin, &omax))
				return 1;
			break;
		case 'n':
			n_flag = 1;
			break;
		case 'o':
			if (off > 0) {
				/* compat with legacy -c syntax */
				imin = off;
				imax = off + nch - 1;
			}
			if (!slot_new(optarg, SIO_REC,
				&par, hdr, imin, imax, omin, omax,
				nch, rate, dup, 0, pos))
				return 1;
			imin = imax = omin = omax = -1;
			mode |= SIO_REC;
			break;
		case 'p':
			if (!opt_pos(optarg, &pos))
				return 1;
			break;
		case 'q':
			port = optarg;
			break;
		case 'r':
			if (!opt_num(optarg, RATE_MIN, RATE_MAX, &rate))
				return 1;
			break;
		case 'v':
			if (!opt_num(optarg, 0, MIDI_MAXCTL, &vol))
				return 1;
			break;
		default:
			goto bad_usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0) {
	bad_usage:
		log_puts(usagestr);
		return 1;
	}
	if (n_flag) {
		if (dev != NULL || port != NULL) {
			log_puts("-f and -q make no sense in off-line mode\n");
			return 1;
		}
		if (mode != (SIO_PLAY | SIO_REC)) {
			log_puts("both -i and -o required\n");
			return 1;
		}
		if (!offline())
			return 1;
	} else {
		if (dev == NULL)
			dev = SIO_DEVANY;
		if (mode == 0) {
			log_puts("at least -i or -o required\n");
			return 1;
		}
		if (!playrec(dev, mode, bufsz, port))
			return 1;
	}
	return 0;
}
