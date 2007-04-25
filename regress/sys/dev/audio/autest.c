/*	$OpenBSD: autest.c,v 1.12 2007/04/25 15:27:54 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

/* XXX ADPCM is currently pretty broken... diagnosis and fix welcome */
#undef	USE_ADPCM

#include "adpcm.h"
#include "law.h"

struct ausrate {
	struct timeval	tv_begin;
	struct timeval	tv_end;
	u_int		r_rate;		/* requested rate */
	u_int		s_rate;		/* rate from audio layer */
	u_int		c_rate;		/* computed rate */
	int		bps;		/* bytes per sample */
	int		bytes;		/* number of bytes played */
	float		err;
};

int main(int, char **);
void check_encoding(int, audio_encoding_t *, int);
void check_encoding_mono(int, audio_encoding_t *, int);
void check_encoding_stereo(int, audio_encoding_t *, int);
void enc_ulaw_8(int, audio_encoding_t *, int, int);
void enc_alaw_8(int, audio_encoding_t *, int, int);
void enc_ulinear_8(int, audio_encoding_t *, int, int);
void enc_ulinear_16(int, audio_encoding_t *, int, int, int);
void enc_slinear_8(int, audio_encoding_t *, int, int);
void enc_slinear_16(int, audio_encoding_t *, int, int, int);
void enc_adpcm_8(int, audio_encoding_t *, int, int);
void audio_wait(int);
void check_srate(struct ausrate *);
void mark_time(struct timeval *);
int get_int(const char *, int *);
int get_double(const char *, double *);

#define	PLAYFREQ	440.0
#define	PLAYSECS	2
double playfreq = PLAYFREQ;

#define	DEFAULT_DEV	"/dev/sound"

int
get_double(const char *buf, double *d)
{
	char *ep;
	long dd;

	errno = 0;
	dd = strtod(buf, &ep);
	if (buf[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && (dd == -HUGE_VAL || dd == HUGE_VAL))
		return (-1);
	*d = dd;
	return (0);
}

int
get_int(const char *buf, int *i)
{
	char *ep;
	long lv;

	errno = 0;
	lv = strtol(buf, &ep, 10);
	if (buf[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && (lv == LONG_MAX || lv == LONG_MIN))
		return (-1);
	if (lv < INT_MIN || lv > INT_MAX)
		return (-1);
	*i = lv;
	return (0);
}

int
main(int argc, char **argv)
{
	audio_info_t ainfo;
	char *fname = NULL;
	int fd, i, c;
	int rate = 8000;

	while ((c = getopt(argc, argv, "f:r:t:")) != -1) {
		switch (c) {
		case 'f':
			fname = optarg;
			break;
		case 'r':
			if (get_int(optarg, &rate) || rate <= 0) {
				fprintf(stderr, "%s bad rate %s\n",
				    argv[0], optarg);
				return (1);
			}
			break;
		case 't':
			if (get_double(optarg, &playfreq) || playfreq <= 0.0) {
				fprintf(stderr, "%s bad freq %s\n",
				    argv[0], optarg);
				return (1);
			}
			break;
		case '?':
		default:
			fprintf(stderr, "%s [-f device]\n", argv[0]);
			return (1);
		}
	}

	if (fname == NULL)
		fname = DEFAULT_DEV;

	fd = open(fname, O_RDWR, 0);
	if (fd == -1)
		err(1, "open");


	if (ioctl(fd, AUDIO_GETINFO, &ainfo) == -1)
		err(1, "%s: audio_getinfo", fname);

	for (i = 0; ; i++) {
		audio_encoding_t enc;

		enc.index = i;
		if (ioctl(fd, AUDIO_GETENC, &enc) == -1)
			break;
		check_encoding(fd, &enc, rate);
	}
	close(fd);

	return (0);
}

void
check_srate(struct ausrate *rt)
{
	struct timeval t;
	float tm, b, r, err;

	timersub(&rt->tv_end, &rt->tv_begin, &t);
	tm = (float)t.tv_sec + ((float)t.tv_usec / 1000000.0);
	b = (float)rt->bytes / (float)rt->bps;
	r = b / tm;

	err = fabs((float)rt->s_rate - r);
	err /= r * 0.01;
	rt->err = err;
	rt->c_rate = rintf(r);
	printf("(s %u c %u e %3.1f%%)",
	    rt->s_rate, rt->c_rate, rt->err);
}

void
check_encoding(int fd, audio_encoding_t *enc, int rate)
{
	printf("%s:%d%s",
	    enc->name,
	    enc->precision,
	    (enc->flags & AUDIO_ENCODINGFLAG_EMULATED) ? "*" : "");
	fflush(stdout);
	check_encoding_mono(fd, enc, rate);
	check_encoding_stereo(fd, enc, rate);
	printf("\n");
}

void
mark_time(struct timeval *tv)
{
	if (gettimeofday(tv, NULL) == -1)
		err(1, "gettimeofday");
}

void
check_encoding_mono(int fd, audio_encoding_t *enc, int rate)
{
	int skipped = 0;

	printf("...mono");
	fflush(stdout);

	if (enc->precision == 8) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULAW:
			enc_ulaw_8(fd, enc, 1, rate);
			break;
		case AUDIO_ENCODING_ALAW:
			enc_alaw_8(fd, enc, 1, rate);
			break;
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_8(fd, enc, 1, rate);
			break;
		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_8(fd, enc, 1, rate);
			break;
		case AUDIO_ENCODING_ADPCM:
			enc_adpcm_8(fd, enc, 1, rate);
			break;
		default:
			skipped = 1;
		}
	}

	if (enc->precision == 16) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULINEAR_LE:
			enc_ulinear_16(fd, enc, 1, LITTLE_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_16(fd, enc, 1, BIG_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			enc_slinear_16(fd, enc, 1, LITTLE_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_16(fd, enc, 1, BIG_ENDIAN, rate);
			break;
		default:
			skipped = 1;
		}
	}

	if (skipped)
		printf("[skip]");
}

void
check_encoding_stereo(int fd, audio_encoding_t *enc, int rate)
{
	int skipped = 0;

	printf("...stereo");
	fflush(stdout);

	if (enc->precision == 8) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULAW:
			enc_ulaw_8(fd, enc, 2, rate);
			break;
		case AUDIO_ENCODING_ALAW:
			enc_alaw_8(fd, enc, 2, rate);
			break;
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_ULINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_8(fd, enc, 2, rate);
			break;
		case AUDIO_ENCODING_SLINEAR:
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_8(fd, enc, 2, rate);
			break;
		case AUDIO_ENCODING_ADPCM:
			enc_adpcm_8(fd, enc, 2, rate);
			break;
		default:
			skipped = 1;
		}
	}

	if (enc->precision == 16) {
		switch (enc->encoding) {
		case AUDIO_ENCODING_ULINEAR_LE:
			enc_ulinear_16(fd, enc, 2, LITTLE_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			enc_ulinear_16(fd, enc, 2, BIG_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			enc_slinear_16(fd, enc, 2, LITTLE_ENDIAN, rate);
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			enc_slinear_16(fd, enc, 2, BIG_ENDIAN, rate);
			break;
		default:
			skipped = 1;
		}
	}

	if (skipped)
		printf("[skip]");
}

void
enc_ulinear_8(int fd, audio_encoding_t *enc, int chans, int rate)
{
	audio_info_t inf;
	struct ausrate rt;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 1 * chans;
	rt.bytes = inf.play.sample_rate * chans * PLAYSECS;

	samples = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		u_int8_t v;

		d = 127.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		d = rintf(d + 127.0);
		v = d;

		for (j = 0; j < chans; j++) {
			*p = v;
			p++;
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans);
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_slinear_8(int fd, audio_encoding_t *enc, int chans, int rate)
{
	audio_info_t inf;
	struct ausrate rt;
	int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 1 * chans;
	rt.bytes = inf.play.sample_rate * chans * PLAYSECS;

	samples = (int8_t *)malloc(inf.play.sample_rate * chans);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		int8_t v;

		d = 127.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		d = rintf(d);
		v = d;

		for (j = 0; j < chans; j++) {
			*p = v;
			p++;
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans);
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_slinear_16(int fd, audio_encoding_t *enc, int chans, int order, int rate)
{
	audio_info_t inf;
	struct ausrate rt;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 2 * chans;
	rt.bytes = 2 * inf.play.sample_rate * chans * PLAYSECS;

	samples = (int8_t *)malloc(inf.play.sample_rate * chans * 2);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		int16_t v;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		d = rintf(d);
		v = d;

		for (j = 0; j < chans; j++) {
			if (order == LITTLE_ENDIAN) {
				*p = (v & 0x00ff) >> 0;
				p++;
				*p = (v & 0xff00) >> 8;
				p++;
			} else {
				*p = (v & 0xff00) >> 8;
				p++;
				*p = (v & 0x00ff) >> 0;
				p++;
			}
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans * 2);
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_ulinear_16(int fd, audio_encoding_t *enc, int chans, int order, int rate)
{
	audio_info_t inf;
	struct ausrate rt;
	u_int8_t *samples = NULL, *p;
	int i, j;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}

	samples = (u_int8_t *)malloc(inf.play.sample_rate * chans * 2);
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 2 * chans;
	rt.bytes = 2 * inf.play.sample_rate * chans * PLAYSECS;

	for (i = 0, p = samples; i < inf.play.sample_rate; i++) {
		float d;
		u_int16_t v;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		d = rintf(d + 32767.0);
		v = d;

		for (j = 0; j < chans; j++) {
			if (order == LITTLE_ENDIAN) {
				*p = (v >> 0) & 0xff;
				p++;
				*p = (v >> 8) & 0xff;
				p++;
			} else {
				*p = (v >> 8) & 0xff;
				p++;
				*p = (v >> 0) & 0xff;
				p++;
			}
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++)
		write(fd, samples, inf.play.sample_rate * chans * 2);
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);

out:
	if (samples != NULL)
		free(samples);
}

void
enc_adpcm_8(int fd, audio_encoding_t *enc, int chans, int rate)
{
	audio_info_t inf;
	struct adpcm_state adsts;
	int16_t *samples = NULL;
	int i, j;
	char *outbuf = NULL, *sbuf = NULL, *p;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}

	bzero(&adsts, sizeof(adsts));

	samples = (int16_t *)malloc(inf.play.sample_rate * sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	sbuf = (char *)malloc(inf.play.sample_rate / 2);
	if (sbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float d;

		d = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		samples[i] = rintf(d);
	}

	outbuf = (char *)malloc((inf.play.sample_rate / 2) * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < PLAYSECS; i++) {
		adpcm_coder(samples, sbuf, inf.play.sample_rate, &adsts);

		for (i = 0, p = outbuf; i < inf.play.sample_rate / 2; i++) {
			for (j = 0; j < chans; j++, p++) {
				*p = sbuf[i];
			}
		}

		write(fd, outbuf, (inf.play.sample_rate / 2) * chans);
	}
	audio_wait(fd);

out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
	if (sbuf != NULL)
		free(sbuf);
}

void
enc_ulaw_8(int fd, audio_encoding_t *enc, int chans, int rate)
{
	audio_info_t inf;
	int16_t *samples = NULL;
	int i, j;
	u_int8_t *outbuf = NULL, *p;
	struct ausrate rt;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 1 * chans;
	rt.bytes = inf.play.sample_rate * chans * PLAYSECS;

	samples = (int16_t *)calloc(inf.play.sample_rate, sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	outbuf = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float x;

		x = 32765.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		samples[i] = x;
	}

	for (i = 0, p = outbuf; i < inf.play.sample_rate; i++) {
		for (j = 0; j < chans; j++) {
			*p = linear2ulaw(samples[i]);
			p++;
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++) {
		write(fd, outbuf, inf.play.sample_rate * chans);
	}
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);

out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
}

void
enc_alaw_8(int fd, audio_encoding_t *enc, int chans, int rate)
{
	audio_info_t inf;
	struct ausrate rt;
	int16_t *samples = NULL;
	int i, j;
	u_int8_t *outbuf = NULL, *p;

	AUDIO_INITINFO(&inf);
	inf.play.precision = enc->precision;
	inf.play.encoding = enc->encoding;
	inf.play.channels = chans;
	inf.play.sample_rate = rate;; 

	if (ioctl(fd, AUDIO_SETINFO, &inf) == -1) {
		printf("[%s]", strerror(errno));
		goto out;
	}

	if (ioctl(fd, AUDIO_GETINFO, &inf) == -1) {
		printf("[getinfo: %s]", strerror(errno));
		goto out;
	}
	rt.r_rate = inf.play.sample_rate;
	rt.s_rate = inf.play.sample_rate;
	rt.bps = 1* chans;
	rt.bytes = inf.play.sample_rate * chans * PLAYSECS;

	samples = (int16_t *)calloc(inf.play.sample_rate, sizeof(*samples));
	if (samples == NULL) {
		warn("malloc");
		goto out;
	}

	outbuf = (u_int8_t *)malloc(inf.play.sample_rate * chans);
	if (outbuf == NULL) {
		warn("malloc");
		goto out;
	}

	for (i = 0; i < inf.play.sample_rate; i++) {
		float x;

		x = 32767.0 * sinf(((float)i / (float)inf.play.sample_rate) *
		    (2 * M_PI * playfreq));
		samples[i] = x;
	}

	for (i = 0, p = outbuf; i < inf.play.sample_rate; i++) {
		for (j = 0; j < chans; j++) {
			*p = linear2alaw(samples[i]);
			p++;
		}
	}

	mark_time(&rt.tv_begin);
	for (i = 0; i < PLAYSECS; i++) {
		write(fd, outbuf, inf.play.sample_rate * chans);
	}
	audio_wait(fd);
	mark_time(&rt.tv_end);
	check_srate(&rt);


out:
	if (samples != NULL)
		free(samples);
	if (outbuf != NULL)
		free(outbuf);
}

void
audio_wait(int fd)
{
	if (ioctl(fd, AUDIO_DRAIN, NULL) == -1)
		warn("drain");
}
