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
#include "midi.h"
#include "wav.h"
#include "opt.h"
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
struct aproc *rwav_new(struct file *);

int wwav_in(struct aproc *, struct abuf *);
int wwav_out(struct aproc *, struct abuf *);
void wwav_eof(struct aproc *, struct abuf *);
void wwav_hup(struct aproc *, struct abuf *);
void wwav_done(struct aproc *);
struct aproc *wwav_new(struct file *);

void wav_setvol(void *, unsigned);
void wav_startreq(void *);
void wav_stopreq(void *);
void wav_locreq(void *, unsigned);
void wav_quitreq(void *);

struct ctl_ops ctl_wavops = {
	wav_setvol,
	wav_startreq,
	wav_stopreq,
	wav_locreq,
	wav_quitreq
};

struct aproc_ops rwav_ops = {
	"rwav",
	rwav_in,
	rwav_out,
	rfile_eof,
	rfile_hup,
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
	wfile_eof,
	wfile_hup,
	NULL, /* newin */
	NULL, /* newout */
	NULL, /* ipos */
	NULL, /* opos */
	wwav_done
};

#ifdef DEBUG
/*
 * print the given wav structure
 */
void
wav_dbg(struct wav *f)
{
	static char *pstates[] = { "ini", "sta", "rdy", "run", "fai" };
	struct aproc *midi = f->dev ? f->dev->midi : NULL;

	dbg_puts("wav(");
	if (f->slot >= 0 && APROC_OK(midi)) {
		dbg_puts(midi->u.ctl.slot[f->slot].name);
		dbg_putu(midi->u.ctl.slot[f->slot].unit);
	} else
		dbg_puts(f->pipe.file.name);
	dbg_puts(")/");
	dbg_puts(pstates[f->pstate]);
}
#endif

/*
 * convert ``count'' samples using the given char->short map
 */
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

/*
 * read method of the file structure
 */
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
				wav_dbg(f);
				dbg_puts(": read complete\n");
			}
#endif
			if (!f->mmc)
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

/*
 * write method of the file structure
 */
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
				wav_dbg(f);
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
	f->endpos += n;
	return n;
}

/*
 * close method of the file structure
 */
void
wav_close(struct file *file)
{
	struct wav *f = (struct wav *)file;

	if (f->mode & MODE_RECMASK) {
		pipe_trunc(&f->pipe.file, f->endpos);
		if (f->hdr == HDR_WAV) {
			wav_writehdr(f->pipe.fd,
			    &f->hpar,
			    &f->startpos,
			    f->endpos - f->startpos);
		}
	}
	pipe_close(file);
	if (f->dev) {
		dev_unref(f->dev);
		f->dev = NULL;
	}
}

/*
 * attach play (rec) abuf structure to the device and
 * switch to the ``RUN'' state; the play abug must not be empty
 */
int
wav_attach(struct wav *f, int force)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;
	struct dev *d = f->dev;

	if (f->mode & MODE_PLAY)
		rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
	if (f->mode & MODE_RECMASK)
		wbuf = LIST_FIRST(&f->pipe.file.wproc->ins);
	f->pstate = WAV_RUN;
#ifdef DEBUG
	if (debug_level >= 3) {
		wav_dbg(f);
		dbg_puts(": attaching\n");
	}
#endif

	/*
	 * start the device (dev_getpos() and dev_attach() must
	 * be called on a started device
	 */
	dev_wakeup(d);

	dev_attach(d, f->pipe.file.name, f->mode,
	    rbuf, &f->hpar, f->join ? d->opar.cmax - d->opar.cmin + 1 : 0,
	    wbuf, &f->hpar, f->join ? d->ipar.cmax - d->ipar.cmin + 1 : 0,
	    f->xrun, f->maxweight);
	if (f->mode & MODE_PLAY)
		dev_setvol(d, rbuf, MIDI_TO_ADATA(f->vol));
	return 1;
}

/*
 * allocate the play (rec) abuf structure; if this is a
 * file to record, then attach it to the device
 *
 * XXX: buffer size should be larger than dev_bufsz, because
 *	in non-server mode we don't prime play buffers with
 *	silence
 */
void
wav_allocbuf(struct wav *f)
{
	struct abuf *buf;
	struct dev *d = f->dev;
	unsigned nfr;

	f->pstate = WAV_START;
	if (f->mode & MODE_PLAY) {
		nfr = 2 * d->bufsz * f->hpar.rate / d->rate;
		buf = abuf_new(nfr, &f->hpar);
		aproc_setout(f->pipe.file.rproc, buf);
		abuf_fill(buf);
		if (!ABUF_WOK(buf) || (f->pipe.file.state & FILE_EOF))
			f->pstate = WAV_READY;
	}
	if (f->mode & MODE_RECMASK) {
		nfr = 2 * d->bufsz * f->hpar.rate / d->rate;
		buf = abuf_new(nfr, &f->hpar);
		aproc_setin(f->pipe.file.wproc, buf);
		f->pstate = WAV_READY;
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		wav_dbg(f);
		dbg_puts(": allocating buffers\n");
	}
#endif
	if (f->pstate == WAV_READY && ctl_slotstart(d->midi, f->slot))
		(void)wav_attach(f, 0);
}

/*
 * free abuf structure and switch to the ``INIT'' state
 */
void
wav_freebuf(struct wav *f)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;

	if (f->mode & MODE_PLAY)
		rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
	if (f->mode & MODE_RECMASK)
		wbuf = LIST_FIRST(&f->pipe.file.wproc->ins);
	f->pstate = WAV_INIT;
#ifdef DEBUG
	if (debug_level >= 3) {
		wav_dbg(f);
		dbg_puts(": freeing buffers\n");
	}
#endif
	if (rbuf || wbuf)
		ctl_slotstop(f->dev->midi, f->slot);
	if (rbuf)
		abuf_eof(rbuf);
	if (wbuf)
		abuf_hup(wbuf);
}

/*
 * switch to the ``INIT'' state performing
 * necessary actions to reach it
 */
void
wav_reset(struct wav *f)
{
	switch (f->pstate) {
	case WAV_START:
	case WAV_READY:
		if (ctl_slotstart(f->dev->midi, f->slot))
			(void)wav_attach(f, 1);
		/* PASSTHROUGH */
	case WAV_RUN:
		wav_freebuf(f);
		f->pstate = WAV_INIT;
		/* PASSTHROUGH */
	case WAV_INIT:
	case WAV_FAILED:
		/* nothing yet */
		break;
	}
}

/*
 * terminate the wav reader/writer
 */
void
wav_exit(struct wav *f)
{
	/* XXX: call file_close() ? */
	if (f->mode & MODE_PLAY) {
		aproc_del(f->pipe.file.rproc);
	} else if (f->mode & MODE_RECMASK) {
		aproc_del(f->pipe.file.wproc);
	}
}

/*
 * seek to f->mmcpos and prepare to start, close
 * the file on error.
 */
int
wav_seekmmc(struct wav *f)
{
	/*
	 * don't go beyond the end-of-file, if so
	 * put it in INIT state so it dosn't start
	 */
	if (f->mmcpos > f->endpos) {
		wav_reset(f);
		f->pstate = WAV_FAILED;
		/*
		 * don't make other stream wait for us
		 */
		if (f->slot >= 0)
			ctl_slotstart(f->dev->midi, f->slot);
		return 0;
	}
	if (!pipe_seek(&f->pipe.file, f->mmcpos)) {
		wav_exit(f);
		return 0;
	}
	if (f->hdr == HDR_WAV)
		f->wbytes = WAV_DATAMAX - f->mmcpos;
	f->rbytes = f->endpos - f->mmcpos;
	wav_reset(f);
	wav_allocbuf(f);
	return 1;
}

/*
 * read samples from the file and possibly start it
 */
int
wav_rdata(struct wav *f)
{
	struct aproc *p;
	struct abuf *obuf;

	p = f->pipe.file.rproc;
	obuf = LIST_FIRST(&p->outs);
	if (obuf == NULL)
		return 0;
	if (!ABUF_WOK(obuf) || !(f->pipe.file.state & FILE_ROK))
		return 0;
	if (!rfile_do(p, obuf->len, NULL))
		return 0;
	switch (f->pstate) {
	case WAV_START:
		if (!ABUF_WOK(obuf) || (f->pipe.file.state & FILE_EOF))
			f->pstate = WAV_READY;
		/* PASSTHROUGH */
	case WAV_READY:
		if (ctl_slotstart(f->dev->midi, f->slot))
			(void)wav_attach(f, 0);
		break;
#ifdef DEBUG
	case WAV_RUN:
		break;
	default:
		wav_dbg(f);
		dbg_puts(": bad state\n");
		dbg_panic();
#endif
	}
	if (f->rbytes == 0 && f->mmc) {
#ifdef DEBUG
		if (debug_level >= 3) {
			wav_dbg(f);
			dbg_puts(": trying to restart\n");
		}
#endif
		if (!wav_seekmmc(f))
			return 0;
	}
	return 1;
}

int
wav_wdata(struct wav *f)
{
	struct aproc *p;
	struct abuf *ibuf;

	if (!(f->pipe.file.state & FILE_WOK))
		return 0;
	p = f->pipe.file.wproc;
	ibuf = LIST_FIRST(&p->ins);
	if (ibuf == NULL)
		return 0;
	if (!ABUF_ROK(ibuf))
		return 0;
	if (!wfile_do(p, ibuf->len, NULL))
		return 0;
	return 1;
}

/*
 * callback to set the volume, invoked by the MIDI control code
 */
void
wav_setvol(void *arg, unsigned vol)
{
	struct wav *f = (struct wav *)arg;
	struct abuf *rbuf;

	f->vol = vol;
	if ((f->mode & MODE_PLAY) && f->pstate == WAV_RUN) {
		rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
		dev_setvol(f->dev, rbuf, MIDI_TO_ADATA(vol));
	}
}

/*
 * callback to start the stream, invoked by the MIDI control code
 */
void
wav_startreq(void *arg)
{
	struct wav *f = (struct wav *)arg;

	switch (f->pstate) {
	case WAV_FAILED:
#ifdef DEBUG
		if (debug_level >= 2) {
			wav_dbg(f);
			dbg_puts(": skipped (failed to seek)\n");
		}
#endif
		return;
	case WAV_READY:
		if (f->mode & MODE_RECMASK)
			f->endpos = f->startpos;
		(void)wav_attach(f, 0);
		break;
#ifdef DEBUG
	default:
		wav_dbg(f);
		dbg_puts(": not in READY state\n");
		dbg_panic();
		break;
#endif
	}
}

/*
 * callback to stop the stream, invoked by the MIDI control code
 */
void
wav_stopreq(void *arg)
{
	struct wav *f = (struct wav *)arg;

#ifdef DEBUG
	if (debug_level >= 2) {
		wav_dbg(f);
		dbg_puts(": stopping");
		if (f->pstate != WAV_FAILED && (f->mode & MODE_RECMASK)) {
			dbg_puts(", ");
			dbg_putu(f->endpos);
			dbg_puts(" bytes recorded");
		}
		dbg_puts("\n");
	}
#endif
	if (!f->mmc) {
		wav_exit(f);
		return;
	}
	(void)wav_seekmmc(f);
}

/*
 * callback to relocate the stream, invoked by the MIDI control code
 * on a stopped stream
 */
void
wav_locreq(void *arg, unsigned mmc)
{
	struct wav *f = (struct wav *)arg;

#ifdef DEBUG
	if (f->pstate == WAV_RUN) {
		wav_dbg(f);
		dbg_puts(": in RUN state\n");
		dbg_panic();
	}
#endif
	f->mmcpos = f->startpos + 
	    ((off_t)mmc * f->hpar.rate / MTC_SEC) * aparams_bpf(&f->hpar);
	(void)wav_seekmmc(f);
}

/*
 * Callback invoked when slot is gone
 */
void
wav_quitreq(void *arg)
{
	struct wav *f = (struct wav *)arg;

#ifdef DEBUG
	if (debug_level >= 3) {
		wav_dbg(f);
		dbg_puts(": slot gone\n");
	}
#endif
	if (f->pstate != WAV_RUN)
		wav_exit(f);
}

/*
 * create a file reader in the ``INIT'' state
 */
struct wav *
wav_new_in(struct fileops *ops,
    struct dev *dev, unsigned mode, char *name, unsigned hdr,
    struct aparams *par, unsigned xrun, unsigned volctl, int tr, int join)
{
	int fd;
	struct wav *f;

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
	if (f == NULL) {
		close(fd);
		return NULL;
	}
	if (!dev_ref(dev)) {
		close(fd);
		return NULL;
	}
	f->dev = dev;
	if (hdr == HDR_WAV) {
		if (!wav_readhdr(f->pipe.fd, par, &f->startpos, &f->rbytes, &f->map)) {
			file_del((struct file *)f);
			return NULL;
		}
		f->endpos = f->startpos + f->rbytes;
	} else {
		f->startpos = 0;
		f->endpos = pipe_endpos(&f->pipe.file);
		if (f->endpos > 0) {
			if (!pipe_seek(&f->pipe.file, 0)) {
				file_del((struct file *)f);
				return NULL;
			}
			f->rbytes = f->endpos;
		} else
			f->rbytes = -1;
		f->map = NULL;
	}
	f->mmc = tr;
	f->join = join;
	f->mode = mode;
	f->hpar = *par;
	f->hdr = 0;
	f->xrun = xrun;
	f->maxweight = MIDI_TO_ADATA(volctl);
	f->slot = ctl_slotnew(f->dev->midi, "play", &ctl_wavops, f, 1);
	rwav_new((struct file *)f);
	wav_allocbuf(f);
#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts(name);
		dbg_puts(": playing ");
		dbg_putu(f->startpos);
		dbg_puts("..");
		dbg_putu(f->endpos);
		dbg_puts(": playing ");
		aparams_dbg(par);
		if (f->mmc)
			dbg_puts(", mmc");
		dbg_puts("\n");
	}
#endif
	return f;
}

/*
 * create a file writer in the ``INIT'' state
 */
struct wav *
wav_new_out(struct fileops *ops,
    struct dev *dev, unsigned mode, char *name, unsigned hdr,
    struct aparams *par, unsigned xrun, int tr, int join)
{
	int fd;
	struct wav *f;

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
	if (f == NULL) {
		close(fd);
		return NULL;
	}
	if (!dev_ref(dev)) {
		close(fd);
		return NULL;
	}
	f->dev = dev;
	if (hdr == HDR_WAV) {
		par->le = 1;
		par->sig = (par->bits <= 8) ? 0 : 1;
		par->bps = (par->bits + 7) / 8;
		if (!wav_writehdr(f->pipe.fd, par, &f->startpos, 0)) {
			file_del((struct file *)f);
			return NULL;
		}
		f->wbytes = WAV_DATAMAX;
		f->endpos = f->startpos;
	} else {
		f->wbytes = -1;
		f->startpos = f->endpos = 0;
	}
	f->mmc = tr;
	f->join = join;
	f->mode = mode;
	f->hpar = *par;
	f->hdr = hdr;
	f->xrun = xrun;
	f->slot = ctl_slotnew(f->dev->midi, "rec", &ctl_wavops, f, 1);
	wwav_new((struct file *)f);
	wav_allocbuf(f);
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
rwav_done(struct aproc *p)
{
	struct wav *f = (struct wav *)p->u.io.file;

	if (f->slot >= 0)
		ctl_slotdel(f->dev->midi, f->slot);
	f->slot = -1;
	rfile_done(p);
}

int
rwav_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct wav *f = (struct wav *)p->u.io.file;
	struct abuf *obuf;

	if (!wav_rdata(f))
		return 0;
	obuf = LIST_FIRST(&p->outs);
	if (obuf && f->pstate >= WAV_RUN) {
		if (!abuf_flush(obuf))
			return 0;
	}
	return 1;
}

int
rwav_out(struct aproc *p, struct abuf *obuf)
{
	struct wav *f = (struct wav *)p->u.io.file;

	if (f->pipe.file.state & FILE_RINUSE)
		return 0;
	for (;;) {
		if (!wav_rdata(f))
			return 0;
	}
	return 1;
}

struct aproc *
rwav_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rwav_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;;
	f->rproc = p;
	return p;
}

void
wwav_done(struct aproc *p)
{
	struct wav *f = (struct wav *)p->u.io.file;

	if (f->slot >= 0)
		ctl_slotdel(f->dev->midi, f->slot);
	f->slot = -1;
	wfile_done(p);
}

int
wwav_in(struct aproc *p, struct abuf *ibuf)
{
	struct wav *f = (struct wav *)p->u.io.file;

	if (f->pipe.file.state & FILE_WINUSE)
		return 0;
	for (;;) {
		if (!wav_wdata(f))
			return 0;
	}
	return 1;
}

int
wwav_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	struct wav *f = (struct wav *)p->u.io.file;

	if (ibuf && f->pstate == WAV_RUN) {
		if (!abuf_fill(ibuf))
			return 0;
	}
	if (!wav_wdata(f))
		return 0;
	return 1;
}

struct aproc *
wwav_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wwav_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;;
	f->wproc = p;
	return p;
}

