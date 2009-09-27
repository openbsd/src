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

#include <stdlib.h>

#include "conf.h"
#include "wav.h"

short wav_ulawmap[256] = {
	-32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
	-23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
	-15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
	-11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
	 -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
	 -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
	 -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
	 -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
	 -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
	 -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
	  -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
	  -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
	  -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
	  -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
	  -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
	   -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
	 32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
	 23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
	 15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
	 11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
	  7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
	  5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
	  3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
	  2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
	  1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
	  1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
	   876,    844,    812,    780,    748,    716,    684,    652,
	   620,    588,    556,    524,    492,    460,    428,    396,
	   372,    356,    340,    324,    308,    292,    276,    260,
	   244,    228,    212,    196,    180,    164,    148,    132,
	   120,    112,    104,     96,     88,     80,     72,     64,
	    56,     48,     40,     32,     24,     16,      8,      0
};

short wav_alawmap[256] = {
	 -5504,  -5248,  -6016,  -5760,  -4480,  -4224,  -4992,  -4736,
	 -7552,  -7296,  -8064,  -7808,  -6528,  -6272,  -7040,  -6784,
	 -2752,  -2624,  -3008,  -2880,  -2240,  -2112,  -2496,  -2368,
	 -3776,  -3648,  -4032,  -3904,  -3264,  -3136,  -3520,  -3392,
	-22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
	-30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
	-11008, -10496, -12032, -11520,  -8960,  -8448,  -9984,  -9472,
	-15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
	  -344,   -328,   -376,   -360,   -280,   -264,   -312,   -296,
	  -472,   -456,   -504,   -488,   -408,   -392,   -440,   -424,
	   -88,    -72,   -120,   -104,    -24,     -8,    -56,    -40,
	  -216,   -200,   -248,   -232,   -152,   -136,   -184,   -168,
	 -1376,  -1312,  -1504,  -1440,  -1120,  -1056,  -1248,  -1184,
	 -1888,  -1824,  -2016,  -1952,  -1632,  -1568,  -1760,  -1696,
	  -688,   -656,   -752,   -720,   -560,   -528,   -624,   -592,
	  -944,   -912,  -1008,   -976,   -816,   -784,   -880,   -848,
	  5504,   5248,   6016,   5760,   4480,   4224,   4992,   4736,
	  7552,   7296,   8064,   7808,   6528,   6272,   7040,   6784,
	  2752,   2624,   3008,   2880,   2240,   2112,   2496,   2368,
	  3776,   3648,   4032,   3904,   3264,   3136,   3520,   3392,
	 22016,  20992,  24064,  23040,  17920,  16896,  19968,  18944,
	 30208,  29184,  32256,  31232,  26112,  25088,  28160,  27136,
	 11008,  10496,  12032,  11520,   8960,   8448,   9984,   9472,
	 15104,  14592,  16128,  15616,  13056,  12544,  14080,  13568,
	   344,    328,    376,    360,    280,    264,    312,    296,
	   472,    456,    504,    488,    408,    392,    440,    424,
	    88,     72,    120,    104,     24,      8,     56,     40,
	   216,    200,    248,    232,    152,    136,    184,    168,
	  1376,   1312,   1504,   1440,   1120,   1056,   1248,   1184,
	  1888,   1824,   2016,   1952,   1632,   1568,   1760,   1696,
	   688,    656,    752,    720,    560,    528,    624,    592,
	   944,    912,   1008,    976,    816,    784,    880,    848
};

/*
 * Max data of a .wav file. The total file size must be smaller than
 * 2^31, and we also have to leave some space for the headers (around 40
 * bytes).
 */
#define WAV_DATAMAX	(0x7fff0000)

struct fileops wav_ops = {
	"wav",
	sizeof(struct wav),
	wav_close,
	wav_read,
	wav_write,
	NULL, /* start */
	NULL, /* stop */
	pipe_nfds,
	pipe_pollfd,
	pipe_revents
};

struct wav *
wav_new_in(struct fileops *ops, int fd, char *name,
    struct aparams *par, unsigned hdr)
{
	struct wav *f;

	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		if (!wav_readhdr(f->pipe.fd, par, &f->rbytes, &f->map))
			exit(1);
		f->hpar = *par;
	} else {
		f->rbytes = -1;
		f->map = NULL;
	}
	f->hdr = 0;
	return f;
}

struct wav *
wav_new_out(struct fileops *ops, int fd, char *name,
    struct aparams *par, unsigned hdr)
{
	struct wav *f;

	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		par->le = 1;
		par->sig = (par->bits <= 8) ? 0 : 1;
		par->bps = (par->bits + 7) / 8;
		if (!wav_writehdr(f->pipe.fd, par))
			exit(1);
		f->hpar = *par;
		f->wbytes = WAV_DATAMAX;
	} else
		f->wbytes = -1;
	f->hdr = hdr;
	return f;
}

void
wav_conv(unsigned char *data, unsigned count, short *map)
{
	unsigned i;
	unsigned char *iptr;
	short *optr;

	iptr = data + count;
	optr = (short *)data + count;
	for (i = count; i > 0; i--) {
		--optr;
		--iptr;
		*optr = map[*iptr];
	}
}

unsigned
wav_read(struct file *file, unsigned char *data, unsigned count)
{
	struct wav *f = (struct wav *)file;
	unsigned n;

	if (f->map)
		count /= sizeof(short);
	if (f->rbytes >= 0 && count > f->rbytes) {
		count = f->rbytes; /* file->rbytes fits in count */
		if (count == 0) {
			file_eof(&f->pipe.file);
			return 0;
		}
	}
	n = pipe_read(file, data, count);
	if (n == 0)
		return 0;
	if (f->rbytes >= 0)
		f->rbytes -= n;
	if (f->map) {
		wav_conv(data, n, f->map);
		n *= sizeof(short);
	}
	return n;
}

unsigned
wav_write(struct file *file, unsigned char *data, unsigned count)
{
	struct wav *f = (struct wav *)file;
	unsigned n;

	if (f->wbytes >= 0 && count > f->wbytes) {
		count = f->wbytes; /* wbytes fits in count */
		if (count == 0) {
			file_hup(&f->pipe.file);
			return 0;
		}
	}
	n = pipe_write(file, data, count);
	if (f->wbytes >= 0)
		f->wbytes -= n;
	return n;
}

void
wav_close(struct file *file)
{
	struct wav *f = (struct wav *)file;

	if (f->hdr == HDR_WAV)
		wav_writehdr(f->pipe.fd, &f->hpar);
	pipe_close(file);
}
