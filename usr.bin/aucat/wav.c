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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "wav.h"
#ifdef DEBUG
#include "dbg.h"
#endif

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

int rwav_in(struct aproc *, struct abuf *);
int rwav_out(struct aproc *, struct abuf *);
void rwav_eof(struct aproc *, struct abuf *);
void rwav_hup(struct aproc *, struct abuf *);
void rwav_done(struct aproc *);

int wwav_in(struct aproc *, struct abuf *);
int wwav_out(struct aproc *, struct abuf *);
void wwav_eof(struct aproc *, struct abuf *);
void wwav_hup(struct aproc *, struct abuf *);
void wwav_done(struct aproc *);

struct aproc_ops rwav_ops = {
	"rwav",
	rwav_in,
	rwav_out,
	rwav_eof,
	rwav_hup,
	NULL, /* newin */
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	rwav_done
};

struct aproc_ops wwav_ops = {
	"wwav",
	wwav_in,
	wwav_out,
	wwav_eof,
	wwav_hup,
	NULL, /* newin */
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	wwav_done
};

struct aproc *
rwav_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rwav_ops, f->name);
	p->u.io.file = f;
	f->rproc = p;
	return p;
}

int
rwav_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (ABUF_FULL(obuf) || !(f->state & FILE_ROK))
		return 0;
	data = abuf_wgetblk(obuf, &count, 0);
	count = file_read(f, data, count);
	if (count == 0)
		return 0;
	abuf_wcommit(obuf, count);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
rwav_out(struct aproc *p, struct abuf *obuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (f->state & FILE_RINUSE)
		return 0;
	if (ABUF_FULL(obuf) || !(f->state & FILE_ROK))
		return 0;
	data = abuf_wgetblk(obuf, &count, 0);
	count = file_read(f, data, count);
	if (count == 0)
		return 0;
	abuf_wcommit(obuf, count);
	return 1;
}

void
rwav_done(struct aproc *p)
{
	struct file *f = p->u.io.file;
	struct abuf *obuf;

	if (f == NULL)
		return;
	/*
	 * all buffers must be detached before deleting f->wproc,
	 * because otherwise it could trigger this code again
	 */
	obuf = LIST_FIRST(&p->obuflist);
	if (obuf)
		abuf_eof(obuf);
	if (f->wproc) {
		f->rproc = NULL;
		aproc_del(f->wproc);
	} else
		file_del(f);
	p->u.io.file = NULL;
}

void
rwav_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	aproc_del(p);
}

void
rwav_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

struct aproc *
wwav_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wwav_ops, f->name);
	p->u.io.file = f;
	f->wproc = p;
	return p;
}

void
wwav_done(struct aproc *p)
{
	struct file *f = p->u.io.file;
	struct abuf *ibuf;

	if (f == NULL)
		return;
	/*
	 * all buffers must be detached before deleting f->rproc,
	 * because otherwise it could trigger this code again
	 */
	ibuf = LIST_FIRST(&p->ibuflist);
	if (ibuf)
		abuf_hup(ibuf);
	if (f->rproc) {
		f->wproc = NULL;
		aproc_del(f->rproc);
	} else
		file_del(f);
	p->u.io.file = NULL;
}

int
wwav_in(struct aproc *p, struct abuf *ibuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (f->state & FILE_WINUSE)
		return 0;
	if (ABUF_EMPTY(ibuf) || !(f->state & FILE_WOK))
		return 0;
	data = abuf_rgetblk(ibuf, &count, 0);
	count = file_write(f, data, count);
	if (count == 0)
		return 0;
	abuf_rdiscard(ibuf, count);
	return 1;
}

int
wwav_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (!abuf_fill(ibuf))
		return 0;
	if (ABUF_EMPTY(ibuf) || !(f->state & FILE_WOK))
		return 0;
	data = abuf_rgetblk(ibuf, &count, 0);
	if (count == 0) {
		/* XXX: this can't happen, right ? */
		return 0;
	}
	count = file_write(f, data, count);
	if (count == 0)
		return 0;
	abuf_rdiscard(ibuf, count);
	return 1;
}

void
wwav_eof(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
wwav_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	aproc_del(p);
}

struct wav *
wav_new_in(struct fileops *ops, char *name, unsigned hdr, 
    struct aparams *par, unsigned xrun, unsigned volctl)
{
	int fd;
	struct wav *f;
	struct aproc *p;
	struct abuf *buf;
	unsigned nfr;

	if (name != NULL) {
		fd = open(name, O_RDONLY | O_NONBLOCK, 0666);
		if (fd < 0) {
			perror(name);
			return NULL;
		}
	} else {
		name = "stdin";
		fd = STDIN_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			perror(name);
	}
	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		if (!wav_readhdr(f->pipe.fd, par, &f->rbytes, &f->map)) {
			file_del((struct file *)f);
			return NULL;
		}
		f->hpar = *par;
	} else {
		f->rbytes = -1;
		f->map = NULL;
	}
	f->hdr = 0;
	nfr = dev_bufsz * par->rate / dev_rate;
	buf = abuf_new(nfr, par);
	p = rwav_new((struct file *)f);
	aproc_setout(p, buf);
	abuf_fill(buf); /* XXX: move this in dev_attach() ? */
	dev_attach(name, buf, par, xrun, NULL, NULL, 0, ADATA_UNIT);
	dev_setvol(buf, MIDI_TO_ADATA(volctl));
#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts(name);
		dbg_puts(": playing ");
		aparams_dbg(par);
		dbg_puts("\n");
	}
#endif
	return f;
}

struct wav *
wav_new_out(struct fileops *ops, char *name, unsigned hdr,
    struct aparams *par, unsigned xrun)
{
	int fd;
	struct wav *f;
	struct aproc *p;
	struct abuf *buf;
	unsigned nfr;

	if (name == NULL) {
		name = "stdout";
		fd = STDOUT_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			perror(name);
	} else {
		fd = open(name,
		    O_WRONLY | O_TRUNC | O_CREAT | O_NONBLOCK, 0666);
		if (fd < 0) {
			perror(name);
			return NULL;
		}
	}
	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		par->le = 1;
		par->sig = (par->bits <= 8) ? 0 : 1;
		par->bps = (par->bits + 7) / 8;
		if (!wav_writehdr(f->pipe.fd, par)) {
			file_del((struct file *)f);
			return NULL;
		}
		f->hpar = *par;
		f->wbytes = WAV_DATAMAX;
	} else
		f->wbytes = -1;
	f->hdr = hdr;
	nfr = dev_bufsz * par->rate / dev_rate;
	p = wwav_new((struct file *)f);
	buf = abuf_new(nfr, par);
	aproc_setin(p, buf);
	dev_attach(name, NULL, NULL, 0, buf, par, xrun, 0);
#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts(name);
		dbg_puts(": recording ");
		aparams_dbg(par);
		dbg_puts("\n");
	}
#endif
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
#ifdef DEBUG
			if (debug_level >= 3) {
				file_dbg(&f->pipe.file);
				dbg_puts(": read complete\n");
			}
#endif
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
#ifdef DEBUG
			if (debug_level >= 3) {
				file_dbg(&f->pipe.file);
				dbg_puts(": write complete\n");
			}
#endif
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

