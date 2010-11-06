/*	$OpenBSD: sun.c,v 1.42 2010/11/06 20:25:42 ratchov Exp $	*/
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
/*
 * TODO:
 *
 * remove filling code from sun_write() and create sun_fill()
 *
 * allow block size to be set
 *
 * call hdl->cb_pos() from sun_read() and sun_write(), or better:
 * implement generic blocking sio_read() and sio_write() with poll(2)
 * and use non-blocking sio_ops only
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sndio_priv.h"

struct sun_hdl {
	struct sio_hdl sio;
	int fd;
	int filling;
	unsigned ibpf, obpf;		/* bytes per frame */
	unsigned ibytes, obytes;	/* bytes the hw transfered */
	unsigned ierr, oerr;		/* frames the hw dropped */
	int offset;			/* frames play is ahead of record */
	int idelta, odelta;		/* position reported to client */
	int mix_fd, mix_index;		/* /dev/mixerN stuff */
};

static void sun_close(struct sio_hdl *);
static int sun_start(struct sio_hdl *);
static int sun_stop(struct sio_hdl *);
static int sun_setpar(struct sio_hdl *, struct sio_par *);
static int sun_getpar(struct sio_hdl *, struct sio_par *);
static int sun_getcap(struct sio_hdl *, struct sio_cap *);
static size_t sun_read(struct sio_hdl *, void *, size_t);
static size_t sun_write(struct sio_hdl *, const void *, size_t);
static int sun_pollfd(struct sio_hdl *, struct pollfd *, int);
static int sun_revents(struct sio_hdl *, struct pollfd *);

static struct sio_ops sun_ops = {
	sun_close,
	sun_setpar,
	sun_getpar,
	sun_getcap,
	sun_write,
	sun_read,
	sun_start,
	sun_stop,
	sun_pollfd,
	sun_revents,
	NULL, /* setvol */
	NULL, /* getvol */
};

/*
 * convert sun encoding to sio_par encoding
 */
static int
sun_infotoenc(struct sun_hdl *hdl, struct audio_prinfo *ai, struct sio_par *par)
{
	par->msb = ai->msb;
	par->bits = ai->precision;
	par->bps = ai->bps;
	switch (ai->encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
		par->le = 1;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		par->le = 0;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		par->le = 1;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		par->le = 0;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_SLINEAR:
		par->le = SIO_LE_NATIVE;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR:
		par->le = SIO_LE_NATIVE;
		par->sig = 0;
		break;
	default:
		DPRINTF("sun_infotoenc: unsupported encoding\n");
		hdl->sio.eof = 1;
		return 0;
	}
	return 1;
}

/*
 * convert sio_par encoding to sun encoding
 */
static void
sun_enctoinfo(struct sun_hdl *hdl, unsigned *renc, struct sio_par *par)
{
	if (par->le == ~0U && par->sig == ~0U) {
		*renc = ~0U;
	} else if (par->le == ~0U || par->sig == ~0U) {
		*renc = AUDIO_ENCODING_SLINEAR;
	} else if (par->le && par->sig) {
		*renc = AUDIO_ENCODING_SLINEAR_LE;
	} else if (!par->le && par->sig) {
		*renc = AUDIO_ENCODING_SLINEAR_BE;
	} else if (par->le && !par->sig) {
		*renc = AUDIO_ENCODING_ULINEAR_LE;
	} else {
		*renc = AUDIO_ENCODING_ULINEAR_BE;
	}
}

/*
 * try to set the device to the given parameters and check that the
 * device can use them; return 1 on success, 0 on failure or error
 */
static int
sun_tryinfo(struct sun_hdl *hdl, struct sio_enc *enc,
    unsigned pchan, unsigned rchan, unsigned rate)
{
	struct audio_info aui;
	struct audio_prinfo *pr;

	pr = (hdl->sio.mode & SIO_PLAY) ? &aui.play : &aui.record;

	AUDIO_INITINFO(&aui);
	if (enc) {
		if (enc->le && enc->sig) {
			pr->encoding = AUDIO_ENCODING_SLINEAR_LE;
		} else if (!enc->le && enc->sig) {
			pr->encoding = AUDIO_ENCODING_SLINEAR_BE;
		} else if (enc->le && !enc->sig) {
			pr->encoding = AUDIO_ENCODING_ULINEAR_LE;
		} else {
			pr->encoding = AUDIO_ENCODING_ULINEAR_BE;
		}
		pr->precision = enc->bits;
	}
	if (rate)
		pr->sample_rate = rate;
	if ((hdl->sio.mode & (SIO_PLAY | SIO_REC)) == (SIO_PLAY | SIO_REC))
		aui.record = aui.play;
	if (pchan && (hdl->sio.mode & SIO_PLAY))
		aui.play.channels = pchan;
	if (rchan && (hdl->sio.mode & SIO_REC))
		aui.record.channels = rchan;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		if (errno == EINVAL)
			return 0;
		DPERROR("sun_tryinfo: setinfo");
		hdl->sio.eof = 1;
		return 0;
	}
	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		DPERROR("sun_tryinfo: getinfo");
		hdl->sio.eof = 1;
		return 0;
	}
	if (pchan && aui.play.channels != pchan)
		return 0;
	if (rchan && aui.record.channels != rchan)
		return 0;
	if (rate) {
		if ((hdl->sio.mode & SIO_PLAY) &&
		    (aui.play.sample_rate != rate))
			return 0;
		if ((hdl->sio.mode & SIO_REC) &&
		    (aui.record.sample_rate != rate))
			return 0;
	}
	return 1;
}

/*
 * guess device capabilities
 */
static int
sun_getcap(struct sio_hdl *sh, struct sio_cap *cap)
{
#define NCHANS (sizeof(chans) / sizeof(chans[0]))
#define NRATES (sizeof(rates) / sizeof(rates[0]))
	static unsigned chans[] = {
		1, 2, 4, 6, 8, 10, 12
	};
	static unsigned rates[] = {
		8000, 11025, 12000, 16000, 22050, 24000,
		32000, 44100, 48000, 64000, 88200, 96000
	};
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct sio_par savepar;
	struct audio_encoding ae;
	unsigned nenc = 0, nconf = 0;
	unsigned enc_map = 0, rchan_map = 0, pchan_map = 0, rate_map;
	unsigned i, j, conf;

	if (!sun_getpar(&hdl->sio, &savepar))
		return 0;

	/*
	 * fill encoding list
	 */
	for (ae.index = 0; nenc < SIO_NENC; ae.index++) {
		if (ioctl(hdl->fd, AUDIO_GETENC, &ae) < 0) {
			if (errno == EINVAL)
				break;
			DPERROR("sun_getcap: getenc");
			hdl->sio.eof = 1;
			return 0;
		}
		if (ae.flags & AUDIO_ENCODINGFLAG_EMULATED)
			continue;
		if (ae.encoding == AUDIO_ENCODING_SLINEAR_LE) {
			cap->enc[nenc].le = 1;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_SLINEAR_BE) {
			cap->enc[nenc].le = 0;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR_LE) {
			cap->enc[nenc].le = 1;
			cap->enc[nenc].sig = 0;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR_BE) {
			cap->enc[nenc].le = 0;
			cap->enc[nenc].sig = 0;
		} else if (ae.encoding == AUDIO_ENCODING_SLINEAR) {
			cap->enc[nenc].le = SIO_LE_NATIVE;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR) {
			cap->enc[nenc].le = SIO_LE_NATIVE;
			cap->enc[nenc].sig = 0;
		} else {
			/* unsipported encoding */
			continue;
		}
		cap->enc[nenc].bits = ae.precision;
		cap->enc[nenc].bps = ae.bps;
		cap->enc[nenc].msb = ae.msb;
		enc_map |= (1 << nenc);
		nenc++;
	}

	/*
	 * fill channels
	 *
	 * for now we're lucky: all kernel devices assume that the
	 * number of channels and the encoding are independent so we can
	 * use the current encoding and try various channels.
	 */
	if (hdl->sio.mode & SIO_PLAY) {
		memcpy(&cap->pchan, chans, NCHANS * sizeof(unsigned));
		for (i = 0; i < NCHANS; i++) {
			if (sun_tryinfo(hdl, NULL, chans[i], 0, 0))
				pchan_map |= (1 << i);
		}
	}
	if (hdl->sio.mode & SIO_REC) {
		memcpy(&cap->rchan, chans, NCHANS * sizeof(unsigned));
		for (i = 0; i < NCHANS; i++) {
			if (sun_tryinfo(hdl, NULL, 0, chans[i], 0))
				rchan_map |= (1 << i);
		}
	}

	/*
	 * fill rates
	 *
	 * rates are not independent from other parameters (eg. on
	 * uaudio devices), so certain rates may not be allowed with
	 * certain encodings. We have to check rates for all encodings
	 */
	memcpy(&cap->rate, rates, NRATES * sizeof(unsigned));
	for (j = 0; j < nenc; j++) {
		rate_map = 0;
		for (i = 0; i < NRATES; i++) {
			if (sun_tryinfo(hdl, &cap->enc[j], 0, 0, rates[i]))
				rate_map |= (1 << i);
		}
		for (conf = 0; conf < nconf; conf++) {
			if (cap->confs[conf].rate == rate_map) {
				cap->confs[conf].enc |= (1 << j);
				break;
			}
		}
		if (conf == nconf) {
			if (nconf == SIO_NCONF)
				break;
			cap->confs[nconf].enc = (1 << j);
			cap->confs[nconf].pchan = pchan_map;
			cap->confs[nconf].rchan = rchan_map;
			cap->confs[nconf].rate = rate_map;
			nconf++;
		}
	}
	cap->nconf = nconf;
	if (!sun_setpar(&hdl->sio, &savepar))
		return 0;
	return 1;
#undef NCHANS
#undef NRATES
}

struct sio_hdl *
sio_open_sun(const char *str, unsigned mode, int nbio)
{
	int fd, flags, fullduplex;
	struct audio_info aui;
	struct sun_hdl *hdl;
	struct sio_par par;
	char path[PATH_MAX];

	hdl = malloc(sizeof(struct sun_hdl));
	if (hdl == NULL)
		return NULL;
	sio_create(&hdl->sio, &sun_ops, mode, nbio);

	snprintf(path, sizeof(path), "/dev/audio%s", str);
	if (mode == (SIO_PLAY | SIO_REC))
		flags = O_RDWR;
	else
		flags = (mode & SIO_PLAY) ? O_WRONLY : O_RDONLY;

	while ((fd = open(path, flags | O_NONBLOCK)) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR(path);
		goto bad_free;
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		DPERROR("FD_CLOEXEC");
		goto bad_close;
	}

	/*
	 * pause the device
	 */
	AUDIO_INITINFO(&aui);
	if (mode & SIO_PLAY)
		aui.play.pause = 1;
	if (mode & SIO_REC)
		aui.record.pause = 1;
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0) {
		DPERROR("sio_open_sun: setinfo");
		goto bad_close;
	}
	/*
	 * If both play and record are requested then
	 * set full duplex mode.
	 */
	if (mode == (SIO_PLAY | SIO_REC)) {
		fullduplex = 1;
		if (ioctl(fd, AUDIO_SETFD, &fullduplex) < 0) {
			DPRINTF("sio_open_sun: %s: can't set full-duplex\n", path);
			goto bad_close;
		}
	}
	hdl->fd = fd;

	/*
	 * Default parameters may not be compatible with libsndio (eg. mulaw
	 * encodings, different playback and recording parameters, etc...), so
	 * set parameters to a random value. If the requested parameters are
	 * not supported by the device, then sio_setpar() will pick supported
	 * ones.
	 */
	sio_initpar(&par);
	par.rate = 48000;
	par.le = SIO_LE_NATIVE;
	par.sig = 1;
	par.bits = 16;
	par.appbufsz = 1200;
	if (!sio_setpar(&hdl->sio, &par))
		goto bad_close;
	return (struct sio_hdl *)hdl;
 bad_close:
	while (close(fd) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

static void
sun_close(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;

	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* retry */
	free(hdl);
}

static int
sun_start(struct sio_hdl *sh)
{
	struct sio_par par;
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;

	if (!sio_getpar(&hdl->sio, &par))
		return 0;
	hdl->obpf = par.pchan * par.bps;
	hdl->ibpf = par.rchan * par.bps;
	hdl->ibytes = 0;
	hdl->obytes = 0;
	hdl->ierr = 0;
	hdl->oerr = 0;
	hdl->offset = 0;
	hdl->idelta = 0;
	hdl->odelta = 0;

	if (hdl->sio.mode & SIO_PLAY) {
		/*
		 * keep the device paused and let sun_write() trigger the
		 * start later, to avoid buffer underruns
		 */
		hdl->filling = 1;
	} else {
		/*
		 * no play buffers to fill, start now!
		 */
		AUDIO_INITINFO(&aui);
		if (hdl->sio.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			DPERROR("sun_start: setinfo");
			hdl->sio.eof = 1;
			return 0;
		}
		hdl->filling = 0;
		sio_onmove_cb(&hdl->sio, 0);
	}
	return 1;
}

static int
sun_stop(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;
	int mode;

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		DPERROR("sun_stop: getinfo");
		hdl->sio.eof = 1;
		return 0;
	}
	mode = aui.mode;

	/*
	 * there's no way to drain the device without blocking, so just
	 * stop it until the kernel driver get fixed
	 */
	AUDIO_INITINFO(&aui);
	aui.mode = 0;
	if (hdl->sio.mode & SIO_PLAY)
		aui.play.pause = 1;
	if (hdl->sio.mode & SIO_REC)
		aui.record.pause = 1;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		DPERROR("sun_stop: setinfo1");
		hdl->sio.eof = 1;
		return 0;
	}
	AUDIO_INITINFO(&aui);
	aui.mode = mode;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		DPERROR("sun_stop: setinfo2");
		hdl->sio.eof = 1;
		return 0;
	}
	return 1;
}

static int
sun_setpar(struct sio_hdl *sh, struct sio_par *par)
{
#define NRETRIES 8
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;
	unsigned i, infr, ibpf, onfr, obpf;
	unsigned bufsz, round;
	unsigned rate, req_rate, prec, enc;

	/*
	 * try to set parameters until the device accepts
	 * a common encoding and rate for play and record
	 */
	rate = par->rate;
	prec = par->bits;
	sun_enctoinfo(hdl, &enc, par);
	for (i = 0;; i++) {
		if (i == NRETRIES) {
			DPRINTF("sun_setpar: couldn't set parameters\n");
			hdl->sio.eof = 1;
			return 0;
		}
		AUDIO_INITINFO(&aui);
		if (hdl->sio.mode & SIO_PLAY) {
			aui.play.sample_rate = rate;
			aui.play.precision = prec;
			aui.play.encoding = enc;
			aui.play.channels = par->pchan;
		}
		if (hdl->sio.mode & SIO_REC) {
			aui.record.sample_rate = rate;
			aui.record.precision = prec;
			aui.record.encoding = enc;
			aui.record.channels = par->rchan;
		}
		DPRINTF("sun_setpar: %i: trying pars = %u/%u/%u\n",
		    i, rate, prec, enc);
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0 && errno != EINVAL) {
			DPERROR("sun_setpar: setinfo(pars)");
			hdl->sio.eof = 1;
			return 0;
		}
		if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
			DPERROR("sun_setpar: getinfo(pars)");
			hdl->sio.eof = 1;
			return 0;
		}
		enc = (hdl->sio.mode & SIO_REC) ?
		    aui.record.encoding : aui.play.encoding;
		switch (enc) {
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_ULINEAR:
			break;
		default:
			DPRINTF("sun_setpar: couldn't set linear encoding\n");
			hdl->sio.eof = 1;
			return 0;
		}
		if (hdl->sio.mode != (SIO_REC | SIO_PLAY))
			break;
		if (aui.play.sample_rate == aui.record.sample_rate &&
		    aui.play.precision == aui.record.precision &&
		    aui.play.encoding == aui.record.encoding)
			break;
		if (i < NRETRIES / 2) {
			rate = aui.play.sample_rate;
			prec = aui.play.precision;
			enc = aui.play.encoding;
		} else {
			rate = aui.record.sample_rate;
			prec = aui.record.precision;
			enc = aui.record.encoding;
		}
	}

	/*
	 * If the rate that the hardware is using is different than
	 * the requested rate, scale buffer sizes so they will be the
	 * same time duration as what was requested.  This just gets
	 * the rates to use for scaling, that actual scaling is done
	 * later.
	 */
	rate = (hdl->sio.mode & SIO_REC) ? aui.record.sample_rate :
	    aui.play.sample_rate;
	req_rate = rate;
	if (par->rate && par->rate != ~0U)
		req_rate = par->rate;

	/*
	 * if block size and buffer size are not both set then
	 * set the blocksize to half the buffer size
	 */
	bufsz = par->appbufsz;
	round = par->round;
	if (bufsz != ~0U) {
		bufsz = bufsz * rate / req_rate;
		if (round == ~0U)
			round = (bufsz + 1) / 2;
		else
			round = round * rate / req_rate;
	} else if (round != ~0U) {
		round = round * rate / req_rate;
		bufsz = round * 2;
	} else
		return 1;

	/*
	 * get the play/record frame size in bytes
	 */
	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		DPERROR("sun_setpar: GETINFO");
		hdl->sio.eof = 1;
		return 0;
	}
	ibpf = (hdl->sio.mode & SIO_REC) ?
	    aui.record.channels * aui.record.bps : 1;
	obpf = (hdl->sio.mode & SIO_PLAY) ?
	    aui.play.channels * aui.play.bps : 1;

	DPRINTF("sun_setpar: bpf = (%u, %u)\n", ibpf, obpf);

	/*
	 * try to set parameters until the device accepts
	 * a common block size for play and record
	 */
	for (i = 0; i < NRETRIES; i++) {
		AUDIO_INITINFO(&aui);
		aui.hiwat = (bufsz + round - 1) / round;
		aui.lowat = aui.hiwat;
		if (hdl->sio.mode & SIO_REC)
			aui.record.block_size = round * ibpf;
		if (hdl->sio.mode & SIO_PLAY)
			aui.play.block_size = round * obpf;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			DPERROR("sun_setpar2: SETINFO");
			hdl->sio.eof = 1;
			return 0;
		}
		if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
			DPERROR("sun_setpar2: GETINFO");
			hdl->sio.eof = 1;
			return 0;
		}
		infr = aui.record.block_size / ibpf;
		onfr = aui.play.block_size / obpf;
		DPRINTF("sun_setpar: %i: trying round = %u -> (%u, %u)\n",
		    i, round, infr, onfr);

		/*
		 * if half-duplex or both block sizes match, we're done
		 */
		if (hdl->sio.mode != (SIO_REC | SIO_PLAY) || infr == onfr) {
			DPRINTF("sun_setpar: blocksize ok\n");
			return 1;
		}

		/*
		 * half of the retries, retry with the smaller value,
		 * then with the larger returned value
		 */
		if (i < NRETRIES / 2)
			round = infr < onfr ? infr : onfr;
		else
			round = infr < onfr ? onfr : infr;
	}
	DPRINTF("sun_setpar: couldn't find a working blocksize\n");
	hdl->sio.eof = 1;
	return 0;
#undef NRETRIES
}

static int
sun_getpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		DPERROR("sun_getpar: getinfo");
		hdl->sio.eof = 1;
		return 0;
	}
	if (hdl->sio.mode & SIO_PLAY) {
		par->rate = aui.play.sample_rate;
		if (!sun_infotoenc(hdl, &aui.play, par))
			return 0;
	} else if (hdl->sio.mode & SIO_REC) {
		par->rate = aui.record.sample_rate;
		if (!sun_infotoenc(hdl, &aui.record, par))
			return 0;
	} else
		return 0;
	par->pchan = (hdl->sio.mode & SIO_PLAY) ?
	    aui.play.channels : 0;
	par->rchan = (hdl->sio.mode & SIO_REC) ?
	    aui.record.channels : 0;
	par->round = (hdl->sio.mode & SIO_REC) ?
	    aui.record.block_size / (par->bps * par->rchan) :
	    aui.play.block_size / (par->bps * par->pchan);
	par->appbufsz = aui.hiwat * par->round;
	par->bufsz = par->appbufsz;
	return 1;
}

/*
 * drop recorded samples to compensate xruns
 */
static int
sun_rdrop(struct sun_hdl *hdl)
{
#define DROP_NMAX 0x1000
	static char dropbuf[DROP_NMAX];
	ssize_t n, todo;

	while (hdl->offset > 0) {
		todo = hdl->offset * hdl->ibpf;
		if (todo > DROP_NMAX)
			todo = DROP_NMAX - DROP_NMAX % hdl->ibpf;
		while ((n = read(hdl->fd, dropbuf, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				DPERROR("sun_rdrop: read");
				hdl->sio.eof = 1;
			}
			return 0;
		}
		if (n == 0) {
			DPRINTF("sun_rdrop: eof\n");
			hdl->sio.eof = 1;
			return 0;
		}
		hdl->offset -= (int)n / (int)hdl->ibpf;
		DPRINTF("sun_rdrop: dropped %ld/%ld bytes\n", n, todo);
	}
	return 1;
}

static size_t
sun_read(struct sio_hdl *sh, void *buf, size_t len)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	ssize_t n;

	if (!sun_rdrop(hdl))
		return 0;
	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("sun_read: read");
			hdl->sio.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("sun_read: eof\n");
		hdl->sio.eof = 1;
		return 0;
	}
	return n;
}

static size_t
sun_autostart(struct sun_hdl *hdl)
{
	struct audio_info aui;
	struct pollfd pfd;

	pfd.fd = hdl->fd;
	pfd.events = POLLOUT;
	while (poll(&pfd, 1, 0) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR("sun_autostart: poll");
		hdl->sio.eof = 1;
		return 0;
	}
	if (!(pfd.revents & POLLOUT)) {
		hdl->filling = 0;
		AUDIO_INITINFO(&aui);
		if (hdl->sio.mode & SIO_PLAY)
			aui.play.pause = 0;
		if (hdl->sio.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			DPERROR("sun_autostart: setinfo");
			hdl->sio.eof = 1;
			return 0;
		}
		sio_onmove_cb(&hdl->sio, 0);
	}
	return 1;
}

/*
 * insert silence to play to compensate xruns
 */
static int
sun_wsil(struct sun_hdl *hdl)
{
#define ZERO_NMAX 0x1000
	static char zero[ZERO_NMAX];
	ssize_t n, todo;

	while (hdl->offset < 0) {
		todo = (int)-hdl->offset * (int)hdl->obpf;
		if (todo > ZERO_NMAX)
			todo = ZERO_NMAX - ZERO_NMAX % hdl->obpf;
		while ((n = write(hdl->fd, zero, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				DPERROR("sun_wsil: write");
				hdl->sio.eof = 1;
				return 0;
			}
			return 0;
		}
		hdl->offset += (int)n / (int)hdl->obpf;
		DPRINTF("sun_wsil: inserted %ld/%ld bytes\n", n, todo);
	}
	return 1;
}


static size_t
sun_write(struct sio_hdl *sh, const void *buf, size_t len)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	const unsigned char *data = buf;
	ssize_t n, todo;

	if (!sun_wsil(hdl))
		return 0;
	todo = len;
	while ((n = write(hdl->fd, data, todo)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("sun_write: write");
			hdl->sio.eof = 1;
		}
 		return 0;
	}
	if (hdl->filling) {
		if (!sun_autostart(hdl))
			return 0;
	}
	return n;
}

static int
sun_pollfd(struct sio_hdl *sh, struct pollfd *pfd, int events)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;

	pfd->fd = hdl->fd;
	pfd->events = events;
	return 1;
}

int
sun_revents(struct sio_hdl *sh, struct pollfd *pfd)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_offset ao;
	int xrun, dmove, dierr = 0, doerr = 0, delta;
	int revents = pfd->revents;

	if (!hdl->sio.started)
		return pfd->revents;
	if (hdl->sio.mode & SIO_PLAY) {
		if (ioctl(hdl->fd, AUDIO_PERROR, &xrun) < 0) {
			DPERROR("sun_revents: PERROR");
			hdl->sio.eof = 1;
			return POLLHUP;
		}
		doerr = xrun - hdl->oerr;
		hdl->oerr = xrun;
		if (!(hdl->sio.mode & SIO_REC))
			dierr = doerr;
	}
	if (hdl->sio.mode & SIO_REC) {
		if (ioctl(hdl->fd, AUDIO_RERROR, &xrun) < 0) {
			DPERROR("sun_revents: RERROR");
			hdl->sio.eof = 1;
			return POLLHUP;
		}
		dierr = xrun - hdl->ierr;
		hdl->ierr = xrun;
		if (!(hdl->sio.mode & SIO_PLAY))
			doerr = dierr;
	}
	hdl->offset += doerr - dierr;
	dmove = dierr > doerr ? dierr : doerr;
	hdl->idelta -= dmove;
	hdl->odelta -= dmove;

	if ((revents & POLLOUT) && (hdl->sio.mode & SIO_PLAY)) {
		if (ioctl(hdl->fd, AUDIO_GETOOFFS, &ao) < 0) {
			DPERROR("sun_revents: GETOOFFS");
			hdl->sio.eof = 1;
			return POLLHUP;
		}
		delta = (ao.samples - hdl->obytes) / hdl->obpf;
		hdl->obytes = ao.samples;
		hdl->odelta += delta;
		if (!(hdl->sio.mode & SIO_REC))
			hdl->idelta += delta;
	}
	if ((revents & POLLIN) && (hdl->sio.mode & SIO_REC)) {
		if (ioctl(hdl->fd, AUDIO_GETIOFFS, &ao) < 0) {
			DPERROR("sun_revents: GETIOFFS");
			hdl->sio.eof = 1;
			return POLLHUP;
		}
		delta = (ao.samples - hdl->ibytes) / hdl->ibpf;
		hdl->ibytes = ao.samples;
		hdl->idelta += delta;
		if (!(hdl->sio.mode & SIO_PLAY))
			hdl->odelta += delta;
	}
	delta = (hdl->idelta > hdl->odelta) ? hdl->idelta : hdl->odelta;
	if (delta > 0) {
		sio_onmove_cb(&hdl->sio, delta);
		hdl->idelta -= delta;
		hdl->odelta -= delta;
	}

	/*
	 * drop recorded samples or insert silence to play
	 * right now to adjust revents, and avoid busy loops
	 * programs
	 */
	if (hdl->filling)
		revents |= POLLOUT;
	if ((hdl->sio.mode & SIO_PLAY) && !sun_wsil(hdl))
		revents &= ~POLLOUT;
	if ((hdl->sio.mode & SIO_REC) && !sun_rdrop(hdl))
		revents &= ~POLLIN;
	return revents;
}
