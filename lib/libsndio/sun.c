/*	$OpenBSD: sun.c,v 1.3 2008/10/30 18:25:43 ratchov Exp $	*/
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
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sndio_priv.h"

struct sun_hdl {
	struct sio_hdl sa;
	int fd;
	int filling;
	unsigned ibpf, obpf;		/* bytes per frame */
	unsigned ibytes, obytes;	/* bytes the hw transfered */
	unsigned ierr, oerr;		/* frames the hw dropped */
	int offset;			/* frames play is ahead of record */
	int idelta, odelta;		/* position reported to client */
};

void sun_close(struct sio_hdl *);
int sun_start(struct sio_hdl *);
int sun_stop(struct sio_hdl *);
int sun_setpar(struct sio_hdl *, struct sio_par *);
int sun_getpar(struct sio_hdl *, struct sio_par *);
int sun_getcap(struct sio_hdl *, struct sio_cap *);
size_t sun_read(struct sio_hdl *, void *, size_t);
size_t sun_write(struct sio_hdl *, void *, size_t);
int sun_pollfd(struct sio_hdl *, struct pollfd *, int);
int sun_revents(struct sio_hdl *, struct pollfd *);

struct sio_ops sun_ops = {
	sun_close,
	sun_setpar,
	sun_getpar,
	sun_getcap,
	sun_write,
	sun_read,
	sun_start,
	sun_stop,
	sun_pollfd,
	sun_revents
};

struct sun_rate {
	unsigned rate;
	unsigned *blksz;
};

unsigned sun_blksz_8000Hz[] = {
	40, 48, 56, 64, 72, 80, 88, 96,
	104, 112, 120, 125, 128, 136, 144, 152,
	160, 168, 176, 184, 192, 200, 208, 216,
	224, 232, 240, 248, 250, 256, 264, 272,
	280, 288, 296, 304, 312, 320, 328, 336,
	344, 352, 360, 368, 375, 376, 384, 392,
	400, 408, 416, 424, 432, 440, 448, 456,
	464, 472, 480, 488, 496, 500, 504, 512,
	520, 528, 536, 544, 552, 560, 568, 576,
	584, 592, 600, 608, 616, 624, 625, 632,
	640, 648, 656, 664, 672, 680, 688, 696,
	704, 712, 720, 728, 736, 744, 750, 752,
	760, 768, 776, 784, 792, 800, 0
};
unsigned sun_blksz_11025Hz[] = {
	105, 210, 225, 245, 315, 420, 441, 450,
	490, 525, 630, 675, 735, 840, 882, 900,
	945, 980, 1050, 0
};
unsigned sun_blksz_12000Hz[] = {
	60, 72, 84, 96, 108, 120, 132, 144,
	156, 168, 180, 192, 200, 204, 216, 228,
	240, 250, 252, 264, 276, 288, 300, 312,
	324, 336, 348, 360, 372, 375, 384, 396,
	400, 408, 420, 432, 444, 456, 468, 480,
	492, 500, 504, 516, 528, 540, 552, 564,
	576, 588, 600, 612, 624, 636, 648, 660,
	672, 684, 696, 708, 720, 732, 744, 750,
	756, 768, 780, 792, 800, 804, 816, 828,
	840, 852, 864, 876, 888, 900, 912, 924,
	936, 948, 960, 972, 984, 996, 1000, 1008,
	1020, 1032, 1044, 1056, 1068, 1080, 1092, 1104,
	1116, 1125, 1128, 1140, 1152, 1164, 1176, 1188,
	1200, 0
};
unsigned sun_blksz_16000Hz[] = {
	80, 96, 112, 128, 144, 160, 176, 192,
	208, 224, 240, 250, 256, 272, 288, 304,
	320, 336, 352, 368, 384, 400, 416, 432,
	448, 464, 480, 496, 500, 512, 528, 544,
	560, 576, 592, 608, 624, 640, 656, 672,
	688, 704, 720, 736, 750, 752, 768, 784,
	800, 816, 832, 848, 864, 880, 896, 912,
	928, 944, 960, 976, 992, 1000, 1008, 1024,
	1040, 1056, 1072, 1088, 1104, 1120, 1136, 1152,
	1168, 1184, 1200, 1216, 1232, 1248, 1250, 1264,
	1280, 1296, 1312, 1328, 1344, 1360, 1376, 1392,
	1408, 1424, 1440, 1456, 1472, 1488, 1500, 1504,
	1520, 1536, 1552, 1568, 1584, 1600, 0
};
unsigned sun_blksz_22050Hz[] = {
	210, 315, 420, 441, 450, 490, 525, 630,
	735, 840, 882, 900, 945, 980, 1050, 1225,
	1260, 1323, 1350, 1470, 1575, 1680, 1764, 1800,
	1890, 1960, 2100, 2205, 0
};
unsigned sun_blksz_24000Hz[] = {
	120, 144, 168, 192, 216, 240, 264, 288,
	312, 336, 360, 375, 384, 400, 408, 432,
	456, 480, 500, 504, 528, 552, 576, 600,
	624, 648, 672, 696, 720, 744, 750, 768,
	792, 800, 816, 840, 864, 888, 912, 936,
	960, 984, 1000, 1008, 1032, 1056, 1080, 1104,
	1125, 1128, 1152, 1176, 1200, 1224, 1248, 1272,
	1296, 1320, 1344, 1368, 1392, 1416, 1440, 1464,
	1488, 1500, 1512, 1536, 1560, 1584, 1600, 1608,
	1632, 1656, 1680, 1704, 1728, 1752, 1776, 1800,
	1824, 1848, 1872, 1875, 1896, 1920, 1944, 1968,
	1992, 2000, 2016, 2040, 2064, 2088, 2112, 2136,
	2160, 2184, 2208, 2232, 2250, 2256, 2280, 2304,
	2328, 2352, 2376, 2400, 0
};
unsigned sun_blksz_32000Hz[] = {
	160, 192, 224, 256, 288, 320, 352, 384,
	416, 448, 480, 500, 512, 544, 576, 608,
	640, 672, 704, 736, 768, 800, 832, 864,
	896, 928, 960, 992, 1000, 1024, 1056, 1088,
	1120, 1152, 1184, 1216, 1248, 1280, 1312, 1344,
	1376, 1408, 1440, 1472, 1500, 1504, 1536, 1568,
	1600, 1632, 1664, 1696, 1728, 1760, 1792, 1824,
	1856, 1888, 1920, 1952, 1984, 2000, 2016, 2048,
	2080, 2112, 2144, 2176, 2208, 2240, 2272, 2304,
	2336, 2368, 2400, 2432, 2464, 2496, 2500, 2528,
	2560, 2592, 2624, 2656, 2688, 2720, 2752, 2784,
	2816, 2848, 2880, 2912, 2944, 2976, 3000, 3008,
	3040, 3072, 3104, 3136, 3168, 3200, 0
};
unsigned sun_blksz_44100Hz[] = {
	420, 441, 630, 735, 840, 882, 900, 980,
	1050, 1225, 1260, 1323, 1470, 1575, 1680, 1764,
	1800, 1890, 1960, 2100, 2205, 2450, 2520, 2646,
	2700, 2940, 3087, 3150, 3360, 3528, 3600, 3675,
	3780, 3920, 3969, 4200, 4410, 0
};
unsigned sun_blksz_48000Hz[] = {
	240, 288, 336, 384, 432, 480, 528, 576,
	624, 672, 720, 750, 768, 800, 816, 864,
	912, 960, 1000, 1008, 1056, 1104, 1152, 1200,
	1248, 1296, 1344, 1392, 1440, 1488, 1500, 1536,
	1584, 1600, 1632, 1680, 1728, 1776, 1824, 1872,
	1920, 1968, 2000, 2016, 2064, 2112, 2160, 2208,
	2250, 2256, 2304, 2352, 2400, 2448, 2496, 2544,
	2592, 2640, 2688, 2736, 2784, 2832, 2880, 2928,
	2976, 3000, 3024, 3072, 3120, 3168, 3200, 3216,
	3264, 3312, 3360, 3408, 3456, 3504, 3552, 3600,
	3648, 3696, 3744, 3750, 3792, 3840, 3888, 3936,
	3984, 4000, 4032, 4080, 4128, 4176, 4224, 4272,
	4320, 4368, 4416, 4464, 4500, 4512, 4560, 4608,
	4656, 4704, 4752, 4800, 0
};
unsigned sun_blksz_64000Hz[] = {
	320, 384, 448, 512, 576, 640, 704, 768,
	832, 896, 960, 1000, 1024, 1088, 1152, 1216,
	1280, 1344, 1408, 1472, 1536, 1600, 1664, 1728,
	1792, 1856, 1920, 1984, 2000, 2048, 2112, 2176,
	2240, 2304, 2368, 2432, 2496, 2560, 2624, 2688,
	2752, 2816, 2880, 2944, 3000, 3008, 3072, 3136,
	3200, 3264, 3328, 3392, 3456, 3520, 3584, 3648,
	3712, 3776, 3840, 3904, 3968, 4000, 4032, 4096,
	4160, 4224, 4288, 4352, 4416, 4480, 4544, 4608,
	4672, 4736, 4800, 4864, 4928, 4992, 5000, 5056,
	5120, 5184, 5248, 5312, 5376, 5440, 5504, 5568,
	5632, 5696, 5760, 5824, 5888, 5952, 6000, 6016,
	6080, 6144, 6208, 6272, 6336, 6400, 0
};
unsigned sun_blksz_88200Hz[] = {
	441, 840, 882, 1225, 1260, 1323, 1470, 1680,
	1764, 1800, 1960, 2100, 2205, 2450, 2520, 2646,
	2940, 3087, 3150, 3360, 3528, 3600, 3675, 3780,
	3920, 3969, 4200, 4410, 4851, 4900, 5040, 5292,
	5400, 5733, 5880, 6125, 6174, 6300, 6615, 6720,
	7056, 7200, 7350, 7497, 7560, 7840, 7938, 8379,
	8400, 8575, 8820, 0
};
unsigned sun_blksz_96000Hz[] = {
	480, 576, 672, 768, 864, 960, 1056, 1152,
	1248, 1344, 1440, 1500, 1536, 1600, 1632, 1728,
	1824, 1920, 2000, 2016, 2112, 2208, 2304, 2400,
	2496, 2592, 2688, 2784, 2880, 2976, 3000, 3072,
	3168, 3200, 3264, 3360, 3456, 3552, 3648, 3744,
	3840, 3936, 4000, 4032, 4128, 4224, 4320, 4416,
	4500, 4512, 4608, 4704, 4800, 4896, 4992, 5088,
	5184, 5280, 5376, 5472, 5568, 5664, 5760, 5856,
	5952, 6000, 6048, 6144, 6240, 6336, 6400, 6432,
	6528, 6624, 6720, 6816, 6912, 7008, 7104, 7200,
	7296, 7392, 7488, 7500, 7584, 7680, 7776, 7872,
	7968, 8000, 8064, 8160, 8256, 8352, 8448, 8544,
	8640, 8736, 8832, 8928, 9000, 9024, 9120, 9216,
	9312, 9408, 9504, 9600, 0
};
struct sun_rate sun_rates[] = {
	{ 8000, sun_blksz_8000Hz },
	{ 11025, sun_blksz_11025Hz },
	{ 12000, sun_blksz_12000Hz },
	{ 16000, sun_blksz_16000Hz },
	{ 22050, sun_blksz_22050Hz },
	{ 24000, sun_blksz_24000Hz },
	{ 32000, sun_blksz_32000Hz },
	{ 44100, sun_blksz_44100Hz },
	{ 48000, sun_blksz_48000Hz },
	{ 64000, sun_blksz_64000Hz },
	{ 88200, sun_blksz_88200Hz },
	{ 96000, sun_blksz_96000Hz }
};

#define SUN_MAXNRATES (sizeof(sun_rates) / sizeof(struct sun_rate))

/*
 * return the closest supported rate
 */
struct sun_rate *
sun_findrate(unsigned rate)
{
	unsigned i;

	if (rate <= sun_rates[0].rate)
		return &sun_rates[0];

	for (i = 0; i < SUN_MAXNRATES - 1; i++) {
		if (rate < (sun_rates[i].rate + sun_rates[i + 1].rate) / 2)
			return &sun_rates[i];
	}
	return &sun_rates[SUN_MAXNRATES];
}

/*
 * convert sun encoding to sio_par encoding
 */
void
sun_infotoenc(struct audio_prinfo *ai, struct sio_par *par)
{
	par->msb = 1;
	par->bits = ai->precision;
	par->bps = SIO_BPS(par->bits);
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
		fprintf(stderr, "sun_infotoenc: unsupported encoding\n");
		exit(1);
	}
}

/*
 * convert sio_par encoding to sun encoding
 */
void
sun_enctoinfo(struct audio_prinfo *ai, struct sio_par *par)
{
	if (par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_LE;
	} else if (!par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_BE;
	} else if (par->le && !par->sig) {
		ai->encoding = AUDIO_ENCODING_ULINEAR_LE;
	} else {
		ai->encoding = AUDIO_ENCODING_ULINEAR_BE;
	}
	ai->precision = par->bits;
}

/*
 * calculate and set the largest possible block size, such that
 * play and record blocks have the same frames number
 */
int
sun_setnfr(struct sun_hdl *hdl, unsigned bufsz)
{
	struct audio_info aui;	
	struct sio_par np;
	struct sun_rate *nr;
	unsigned nfr, infr = 0, onfr = 0, ibpf, obpf;
	int i;

	if (!sio_getpar(&hdl->sa, &np))
		return 0;
	nr = sun_findrate(np.rate);
	if (nr->rate != np.rate) {
		fprintf(stderr, "sun_setnfr: warning, unknown rate\n");
	}
	ibpf = (hdl->sa.mode & SIO_REC) ? np.rchan * np.bps : 1;
	obpf = (hdl->sa.mode & SIO_PLAY) ? np.pchan * np.bps : 1;

	/*
	 * if no bufsz is given, use 200ms which is ok in most cases
	 */
	if (bufsz == 0)
		bufsz = (np.rate * 200 + 999) / 1000;
	if (bufsz < 32)
		bufsz = 32;

	for (i = 0; nr->blksz[i] != 0; i++) {
		nfr = nr->blksz[i];
		AUDIO_INITINFO(&aui);
		aui.hiwat = (bufsz + nfr - 1) / nfr;
		aui.lowat = aui.hiwat;
		if (hdl->sa.mode & SIO_REC)
			aui.record.block_size = nfr * ibpf;
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.block_size = nfr * obpf;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_setnfr: SETINFO");
			hdl->sa.eof = 1;
			return 0;
		}
		if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
			perror("sun_setnfr: GETINFO");
			hdl->sa.eof = 1;
			return 0;
		}
		infr = aui.record.block_size / ibpf;
		onfr = aui.play.block_size / obpf;
		if (hdl->sa.debug) {
			fprintf(stderr, "sun_setnfr: %u -> (%u, %u)\n",
			    nfr, infr, onfr);
		}

		/*
		 * accept only block sizes of the table
		 */
		if ((hdl->sa.mode & SIO_REC) && infr != nfr)
			continue;
		if ((hdl->sa.mode & SIO_PLAY) && onfr != nfr)
			continue;
		return (hdl->sa.mode & SIO_REC) ? infr : onfr;
	}

	/*
	 * failed to find ``optimal'' block size, try using the one the
	 * hardware returned. We require both block sizes match, unless
	 * we're not in full-duplex
	 */
	if (hdl->sa.mode != (SIO_REC | SIO_PLAY) || infr == onfr) {
		fprintf(stderr, "sun_setnfr: using sub optimal block size\n");
		return (hdl->sa.mode & SIO_REC) ? infr : onfr;
	}
	fprintf(stderr, "sun_setnfr: couldn't find a working blocksize\n");
	hdl->sa.eof = 1;
	return 0;
}

/*
 * try to set the device to the given parameters and check that the
 * device can use them; retrun 1 on success, 0 on failure or error
 */
int
sun_tryinfo(struct sun_hdl *hdl, struct sio_enc *enc, 
    unsigned pchan, unsigned rchan, unsigned rate)
{
	struct audio_info aui;	
	
	AUDIO_INITINFO(&aui);
	if (enc) {
		if (enc->le && enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_SLINEAR_LE;
			aui.record.encoding = AUDIO_ENCODING_SLINEAR_LE;
		} else if (!enc->le && enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_SLINEAR_BE;
			aui.record.encoding = AUDIO_ENCODING_SLINEAR_BE;
		} else if (enc->le && !enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_ULINEAR_LE;
			aui.record.encoding = AUDIO_ENCODING_ULINEAR_LE;
		} else {
			aui.play.encoding = AUDIO_ENCODING_ULINEAR_BE;
			aui.record.encoding = AUDIO_ENCODING_ULINEAR_BE;
		}
		aui.play.precision = enc->bits;
	}
	if (pchan)
		aui.play.channels = pchan;
	if (rchan)
		aui.record.channels = rchan;
	if (rate) {
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.sample_rate = rate;
		if (hdl->sa.mode & SIO_REC)
			aui.record.sample_rate = rate;
	}
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		if (errno == EINVAL)
			return 0;
		perror("sun_tryinfo: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_tryinfo: getinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (pchan && aui.play.channels != pchan)
		return 0;
	if (rchan && aui.record.channels != rchan)
		return 0;
	if (rate) {
		if ((hdl->sa.mode & SIO_PLAY) &&
		    (aui.play.sample_rate != rate))
			return 0;
		if ((hdl->sa.mode & SIO_REC) &&
		    (aui.record.sample_rate != rate))
			return 0;
	}
	return 1;
}

/*
 * guess device capabilities
 */
int
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
	unsigned enc_map = 0, rchan_map = 0, pchan_map = 0, rate_map = 0;
	unsigned i, j, map;

	if (!sun_getpar(&hdl->sa, &savepar))
		return 0;

	/*
	 * fill encoding list
	 */
	for (ae.index = 0; nenc < SIO_NENC; ae.index++) {
		if (ioctl(hdl->fd, AUDIO_GETENC, &ae) < 0) {
			if (errno == EINVAL)
				break;
			perror("sun_getcap: getenc");
			hdl->sa.eof = 1;
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
		cap->enc[nenc].bps = ae.precision / 8;
		cap->enc[nenc].msb = 0;
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
	if (hdl->sa.mode & SIO_PLAY) {
		memcpy(&cap->pchan, chans, NCHANS * sizeof(unsigned));
		for (i = 0; i < NCHANS; i++) {
			if (sun_tryinfo(hdl, NULL, chans[i], 0, 0))
				pchan_map |= (1 << i);
		}
	}
	if (hdl->sa.mode & SIO_REC) {
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
	 * certain encordings. We have to check rates for all encodings
	 */
	memcpy(&cap->rate, rates, NRATES * sizeof(unsigned));
	for (j = 0; j < nenc; j++) {
		if (nconf == SIO_NCONF)
			break;
		map = 0;
		for (i = 0; i < NRATES; i++) {
			if (sun_tryinfo(hdl, NULL, 0, 0, rates[i]))
				map |= (1 << i);
		}
		if (map != rate_map) {
			rate_map = map;
			cap->confs[nconf].enc = enc_map;
			cap->confs[nconf].pchan = pchan_map;
			cap->confs[nconf].rchan = rchan_map;
			cap->confs[nconf].rate = rate_map;
			nconf++;
		}
	}
	cap->nconf = nconf;
	if (!sun_setpar(&hdl->sa, &savepar))
		return 0;
	return 1;
#undef NCHANS
#undef NRATES
}

struct sio_hdl *
sio_open_sun(char *path, unsigned mode, int nbio)
{
	int fd, flags, fullduplex;
	struct sun_hdl *hdl;
	struct sio_par par;

	hdl = malloc(sizeof(struct sun_hdl));
	if (hdl == NULL)
		return NULL;
	sio_create(&hdl->sa, &sun_ops, mode, nbio);

	if (path == NULL) {
		path = getenv("AUDIODEVICE");
		if (path == NULL)
			path = SIO_SUN_PATH;
	}
	if (mode == (SIO_PLAY | SIO_REC))
		flags = O_RDWR;
	else 
		flags = (mode & SIO_PLAY) ? O_WRONLY : O_RDONLY;

	while ((fd = open(path, flags | O_NONBLOCK)) < 0) {
		if (errno == EINTR)
			continue;
		perror(path);
		goto bad_free;
	}
	hdl->fd = fd;

	/*
	 * If both play and record are requested then
	 * set full duplex mode.
	 */
	if (mode == (SIO_PLAY | SIO_REC)) {
		fullduplex = 1;
		if (ioctl(fd, AUDIO_SETFD, &fullduplex) < 0) {
			fprintf(stderr, "%s: can't set full-duplex\n", path);
			goto bad_close;
		}
	}
	hdl->fd = fd;

	/*
	 * this is required to set the block size, choose a sample rate
	 * such that the block size is in the ``optimal'' blocksize
	 * range.
	 */
	sio_initpar(&par);
	par.le = 1;
	par.sig = 1;
	par.bits = 16;
	par.pchan = 2;
	par.rchan = 2;
	par.rate = 48000;	
	if (!sio_setpar(&hdl->sa, &par))
		goto bad_close;
	return (struct sio_hdl *)hdl;
 bad_close:
	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

void
sun_close(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	int rc;
	do {
		rc = close(hdl->fd);
	} while (rc < 0 && errno == EINTR);
	free(hdl);
}

int
sun_start(struct sio_hdl *sh)
{
	struct sio_par par;
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	

	if (!sio_getpar(&hdl->sa, &par))
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

	if (hdl->sa.mode & SIO_PLAY) {
		/* 
		 * pause the device and let sun_write() trigger the
		 * start later, to avoid buffer underruns
		 */
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 1;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 1;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo2");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->filling = 1;
	} else {
		/*
		 * no play buffers to fill, start now!
		 */
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 0;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo");
			hdl->sa.eof = 1;
			return 0;
		}
		sio_onmove_cb(&hdl->sa, 0);
	}
	return 1;
}

int
sun_stop(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	
	int mode;

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_start: setinfo1");
		hdl->sa.eof = 1;
		return 0;
	}
	mode = aui.mode;

	/*
	 * there's no way to drain the device without blocking, so just
	 * stop it until the kernel driver get fixed
	 */
	AUDIO_INITINFO(&aui);
	aui.mode = 0;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		perror("sun_stop: setinfo1");
		hdl->sa.eof = 1;
		return 0;
	}
	AUDIO_INITINFO(&aui);
	aui.mode = mode;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		perror("sun_stop: setinfo2");
		hdl->sa.eof = 1;
		return 0;
	}
	return 1;
}

int
sun_setpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	
	struct sun_rate *r;

	/*
	 * the only ones supported by Sun API
	 */
	par->bps = SIO_BPS(par->bits);
	par->msb = 1;
	par->xrun = SIO_IGNORE;
	if (par->rate != (unsigned)~0) {
		r = sun_findrate(par->rate);
		par->rate = r->rate;
	}

	AUDIO_INITINFO(&aui);
	if (hdl->sa.mode & SIO_PLAY) {
		aui.play.sample_rate = par->rate;
		aui.play.channels = par->pchan;
		sun_enctoinfo(&aui.play, par);
	}
	if (hdl->sa.mode & SIO_REC) {
		aui.record.sample_rate = par->rate;
		aui.record.channels = par->rchan;
		sun_enctoinfo(&aui.record, par);
	}	
	aui.lowat = 1;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0 && errno != EINVAL) {
		perror("sun_setpar: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (par->bufsz != (unsigned)~0) {
		if (!sun_setnfr(hdl, par->bufsz))
			return 0;
	}
	return 1;
}

int
sun_getpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_getpar: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (hdl->sa.mode & SIO_PLAY) {
		par->rate = aui.play.sample_rate;
		sun_infotoenc(&aui.play, par);
	} else if (hdl->sa.mode & SIO_REC) {
		par->rate = aui.record.sample_rate;
		sun_infotoenc(&aui.record, par);
	} else
		return 0;
	par->pchan = (hdl->sa.mode & SIO_PLAY) ?
	    aui.play.channels : 0;
	par->rchan = (hdl->sa.mode & SIO_REC) ?
	    aui.record.channels : 0;
	par->round = (hdl->sa.mode & SIO_REC) ?
	    aui.record.block_size / (par->bps * par->rchan) :
	    aui.play.block_size / (par->bps * par->pchan);
	par->bufsz = aui.hiwat * par->round;
	return 1;
}

size_t
sun_read(struct sio_hdl *sh, void *buf, size_t len)
{
#define DROP_NMAX 0x1000
	static char dropbuf[DROP_NMAX];
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	ssize_t n, todo;

	while (hdl->offset > 0) {
		todo = hdl->offset * hdl->ibpf;
		if (todo > DROP_NMAX)
			todo = DROP_NMAX - DROP_NMAX % hdl->ibpf;
		while ((n = read(hdl->fd, dropbuf, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				perror("sun_read: read");
				hdl->sa.eof = 1;
			}
			return 0;
		}
		if (n == 0) {
			fprintf(stderr, "sun_read: eof\n");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->offset -= (int)n / (int)hdl->ibpf;
#ifdef DEBUG
		if (hdl->sa.debug)
			fprintf(stderr, "sun_read: dropped %ld/%ld bytes "
			    "to resync\n", n, todo);
#endif
	}

	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			perror("sun_read: read");
			hdl->sa.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		fprintf(stderr, "sun_read: eof\n");
		hdl->sa.eof = 1;
		return 0;
	}
	return n;
}

size_t
sun_autostart(struct sun_hdl *hdl)
{
	struct audio_info aui;	
	struct pollfd pfd;
	
	pfd.fd = hdl->fd;
	pfd.events = POLLOUT;
	while (poll(&pfd, 1, 0) < 0) {
		if (errno == EINTR)
			continue;
		perror("sun_fill: poll");
		hdl->sa.eof = 1;
		return 0;
	}
	if (!(pfd.revents & POLLOUT)) {
		hdl->filling = 0;
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 0;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo");
			hdl->sa.eof = 1;
			return 0;
		}
		sio_onmove_cb(&hdl->sa, 0);
	}
	return 1;
}

size_t
sun_write(struct sio_hdl *sh, void *buf, size_t len)
{
#define ZERO_NMAX 0x1000
	static char zero[ZERO_NMAX];
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	unsigned char *data = buf;
	ssize_t n, todo;

	while (hdl->offset < 0) {
		todo = (int)-hdl->offset * (int)hdl->obpf;
		if (todo > ZERO_NMAX)
			todo = ZERO_NMAX - ZERO_NMAX % hdl->obpf;
		while ((n = write(hdl->fd, zero, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				perror("sun_write: sil");
				hdl->sa.eof = 1;
				return 0;
			}
			return 0;
		}
		hdl->offset += (int)n / (int)hdl->obpf;
#ifdef DEBUG
		if (hdl->sa.debug)
			fprintf(stderr, "sun_write: inserted %ld/%ld bytes "
			    "of silence to resync\n", n, todo);
#endif
	}

	todo = len;
	while ((n = write(hdl->fd, data, todo)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			perror("sun_write: write");
			hdl->sa.eof = 1;
			return 0;
		}
 		return 0;
	}
	if (hdl->filling) {
		if (!sun_autostart(hdl))
			return 0;
	}
	return n;
}

int
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
	int xrun, dmove, dierr = 0, doerr = 0, doffset = 0;
	int revents = pfd->revents;

	if (hdl->sa.mode & SIO_PLAY) {
		if (ioctl(hdl->fd, AUDIO_PERROR, &xrun) < 0) {
			perror("sun_revents: PERROR");
			exit(1);
		}
		doerr = xrun - hdl->oerr;
		hdl->oerr = xrun;
		if (hdl->sa.mode & SIO_REC)
			doffset += doerr;
	}
	if (hdl->sa.mode & SIO_REC) {
		if (ioctl(hdl->fd, AUDIO_RERROR, &xrun) < 0) {
			perror("sun_revents: RERROR");
			exit(1);
		}
		dierr = xrun - hdl->ierr;
		hdl->ierr = xrun;
		if (hdl->sa.mode & SIO_PLAY)
			doffset -= dierr;
	}
	hdl->offset += doffset;
	dmove = dierr > doerr ? dierr : doerr;
	hdl->idelta -= dmove;
	hdl->odelta -= dmove;

	if ((revents & POLLOUT) && !(hdl->sa.mode & SIO_REC)) {
		if (ioctl(hdl->fd, AUDIO_GETOOFFS, &ao) < 0) {
			perror("sun_revents: GETOOFFS");
			exit(1);
		}
		hdl->odelta += (ao.samples - hdl->obytes) / hdl->obpf;
		hdl->obytes = ao.samples;
		if (hdl->odelta != 0) {
			sio_onmove_cb(&hdl->sa, hdl->odelta);
			hdl->odelta = 0;
		}
	}
	if ((revents & POLLIN) && (hdl->sa.mode & SIO_REC)) {
		if (ioctl(hdl->fd, AUDIO_GETIOFFS, &ao) < 0) {
			perror("sun_revents: GETIOFFS");
			exit(1);
		}
		hdl->idelta += (ao.samples - hdl->ibytes) / hdl->ibpf;
		hdl->ibytes = ao.samples;
		if (hdl->idelta != 0) {
			sio_onmove_cb(&hdl->sa, hdl->idelta);
			hdl->idelta = 0;
		}
	}
	if (hdl->filling)
		revents |= POLLOUT;
	return revents;
}
