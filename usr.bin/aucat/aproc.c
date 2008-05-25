/*	$OpenBSD: aproc.c,v 1.2 2008/05/25 21:16:37 ratchov Exp $	*/
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
 * aproc structures are simple audio processing units. They are
 * interconnected by abuf structures and form a kind of circuit. aproc
 * structure have call-backs that do the actual processing.
 *
 * This module implements the following processing units:
 *
 *  - rpipe: read end of an unix file (pipe, socket, device...)
 *
 *  - wpipe: write end of an unix file (pipe, socket, device...)
 *
 *  - mix: mix N inputs -> 1 output
 *
 *  - sub: from 1 input -> extract/copy N outputs
 *
 *  - conv: converts/resamples/remaps a single stream
 *
 * TODO
 *
 * 	(easy) split the "conv" into 2 converters: one for input (that
 *	convers anything to 16bit signed) and one for the output (that
 *	converts 16bit signed to anything)
 *
 *	(hard) handle underruns in rpipe and mix
 *
 *	(hard) handle overruns in wpipe and sub
 *
 *	(hard) add a lowpass filter for the resampler. Quality is
 *	not acceptable as is.
 */
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "aparams.h"
#include "abuf.h"
#include "aproc.h"
#include "file.h"

struct aproc *
aproc_new(struct aproc_ops *ops, char *name)
{
	struct aproc *p;

	p = malloc(sizeof(struct aproc));
	if (p == NULL)
		err(1, name);
	LIST_INIT(&p->ibuflist);
	LIST_INIT(&p->obuflist);
	p->name = name;
	p->ops = ops;
	return p;
}

void
aproc_del(struct aproc *p)
{
	DPRINTF("aproc_del: %s: %s: deleted\n", p->ops->name, p->name);
	free(p);
}

void
aproc_setin(struct aproc *p, struct abuf *ibuf)
{
	LIST_INSERT_HEAD(&p->ibuflist, ibuf, ient);
	ibuf->rproc = p;
	if (p->ops->newin)
		p->ops->newin(p, ibuf);
}

void
aproc_setout(struct aproc *p, struct abuf *obuf)
{
	LIST_INSERT_HEAD(&p->obuflist, obuf, oent);
	obuf->wproc = p;
	if (p->ops->newout)
		p->ops->newout(p, obuf);
}

int
rpipe_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (!(f->state & FILE_RFLOW) && ABUF_FULL(obuf))
		errx(1, "%s: overrun, unimplemented", f->name);

	if (ABUF_FULL(obuf))
		return 0;
	data = abuf_wgetblk(obuf, &count, 0);
	obuf->used += file_read(f, data, count);
	abuf_flush(obuf);
	return !ABUF_FULL(obuf);
}

int
rpipe_out(struct aproc *p, struct abuf *obuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (!(f->state & FILE_ROK))
		return 0;
	data = abuf_wgetblk(obuf, &count, 0);
	obuf->used += file_read(f, data, count);
	return f->state & FILE_ROK;
}

void
rpipe_del(struct aproc *p)
{
	struct file *f = p->u.io.file;

	f->rproc = NULL;
	f->events &= ~POLLIN;
	aproc_del(p);
}

void
rpipe_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	DPRINTFN(3, "rpipe_eof: %s\n", p->name);
	abuf_eof(LIST_FIRST(&p->obuflist));
	rpipe_del(p);
}

void
rpipe_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(3, "rpipe_hup: %s\n", p->name);
	rpipe_del(p);
}

struct aproc_ops rpipe_ops = {
	"rpipe", rpipe_in, rpipe_out, rpipe_eof, rpipe_hup, NULL, NULL
};

struct aproc *
rpipe_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rpipe_ops, f->name);
	p->u.io.file = f;
	f->rproc = p;
	f->events |= POLLIN;
	f->state |= FILE_RFLOW;
	return p;
}

void
wpipe_del(struct aproc *p)
{
	struct file *f = p->u.io.file;

	f->wproc = NULL;
	f->events &= ~POLLOUT;
	aproc_del(p);
}

int
wpipe_in(struct aproc *p, struct abuf *ibuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (!(f->state & FILE_WOK))
		return 0;

	data = abuf_rgetblk(ibuf, &count, 0);
	count = file_write(f, data, count);
	ibuf->used -= count;
	ibuf->start += count;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;
	return f->state & FILE_WOK;
}

int
wpipe_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (!(f->state & FILE_WFLOW) && ABUF_EMPTY(ibuf))
		errx(1, "%s: underrun, unimplemented", f->name);

	if (ABUF_EMPTY(ibuf))
		return 0;
	data = abuf_rgetblk(ibuf, &count, 0);
	count = file_write(f, data, count);
	ibuf->used -= count;
	ibuf->start += count;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;
	if (ABUF_EOF(ibuf)) {
		abuf_hup(ibuf);
		wpipe_del(p);
		return 0;
	}
	abuf_fill(ibuf);
	return 1;
}

void
wpipe_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(3, "wpipe_eof: %s\n", p->name);
	wpipe_del(p);
}

void
wpipe_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	DPRINTFN(3, "wpipe_hup: %s\n", p->name);
	abuf_hup(LIST_FIRST(&p->ibuflist));
	wpipe_del(p);
}

struct aproc_ops wpipe_ops = {
	"wpipe", wpipe_in, wpipe_out, wpipe_eof, wpipe_hup, NULL, NULL
};

struct aproc *
wpipe_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wpipe_ops, f->name);
	p->u.io.file = f;
	f->wproc = p;
	f->events |= POLLOUT;
	f->state |= FILE_WFLOW;
	return p;
}

/*
 * Fill an output block with silence.
 */
void
mix_bzero(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	short *odata;
	unsigned ocount;

	DPRINTFN(4, "mix_bzero: used = %u, zero = %u\n",
	    obuf->used, obuf->mixtodo);
	odata = (short *)abuf_wgetblk(obuf, &ocount, obuf->mixtodo);
	if (ocount == 0)
		return;
	memset(odata, 0, ocount);
	obuf->mixtodo += ocount;
	DPRINTFN(4, "mix_bzero: ocount %u, todo %u\n", ocount, obuf->mixtodo);
}

/*
 * Mix an input block over an output block.
 */
void
mix_badd(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned i, scount, icount, ocount;
	int vol = ibuf->mixvol;

	DPRINTFN(4, "mix_badd: zero = %u, done = %u\n",
	    obuf->mixtodo, ibuf->mixdone);

	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	if (icount == 0)
		return;

	odata = (short *)abuf_wgetblk(obuf, &ocount, ibuf->mixdone);
	if (ocount == 0)
		return;

	scount = (icount < ocount) ? icount : ocount;
	for (i = scount / sizeof(short); i > 0; i--) {
		*odata += (*idata * vol) >> ADATA_SHIFT;
		idata++;
		odata++;
	}

	ibuf->used -= scount;
	ibuf->mixdone += scount;
	ibuf->start += scount;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;

	DPRINTFN(4, "mix_badd: added %u, done = %u, zero = %u\n",
	    scount, ibuf->mixdone, obuf->mixtodo);
}

/*
 * Remove an input stream from the mixer.
 */
void
mix_rm(struct aproc *p, struct abuf *ibuf)
{
	LIST_REMOVE(ibuf, ient);
	DPRINTF("mix_rm: %s\n", p->name);
}

int
mix_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext, *obuf = LIST_FIRST(&p->obuflist);
	unsigned ocount;

	DPRINTFN(4, "mix_in: used = %u, done = %u, zero = %u\n",
	    ibuf->used, ibuf->mixdone, obuf->mixtodo);

	if (ibuf->mixdone >= obuf->mixtodo)
		return 0;
	mix_badd(ibuf, obuf);
	ocount = obuf->mixtodo;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		if (ocount > i->mixdone)
			ocount = i->mixdone;
	}
	if (ocount == 0)
		return 0;

	obuf->used += ocount;
	obuf->mixtodo -= ocount;
	abuf_flush(obuf);
	mix_bzero(p);
	for (i = LIST_FIRST(&p->ibuflist); i != LIST_END(&p->ibuflist); i = inext) {
		inext = LIST_NEXT(i, ient);
		i->mixdone -= ocount;
		if (i != ibuf && i->mixdone < obuf->mixtodo) {
			if (ABUF_EOF(i)) {
				mix_rm(p, i);
				abuf_hup(i);
				continue;
			}
			mix_badd(i, obuf);
			abuf_fill(i);
		}
	}
	return 1;
}

int
mix_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *inext;
	unsigned ocount;

	DPRINTFN(4, "mix_out: used = %u, zero = %u\n",
	    obuf->used, obuf->mixtodo);

	/*
	 * XXX: should handle underruns here, currently if one input is
	 * blocked, then the output block can underrun.
	 */
	mix_bzero(p);
	ocount = obuf->mixtodo;
	for (i = LIST_FIRST(&p->ibuflist); i != LIST_END(&p->ibuflist); i = inext) {
		inext = LIST_NEXT(i, ient);
		mix_badd(i, obuf);
		if (ocount > i->mixdone)
			ocount = i->mixdone;
		if (ABUF_EOF(i)) {
			mix_rm(p, i);
			abuf_hup(i);
			continue;
		}
		abuf_fill(i);
	}
	if (ocount == 0)
		return 0;
	if (LIST_EMPTY(&p->ibuflist)) {
		DPRINTF("mix_out: nothing more to do...\n");
		obuf->wproc = NULL;
		aproc_del(p);
		return 0;
	}
	obuf->used += ocount;
	obuf->mixtodo -= ocount;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		i->mixdone -= ocount;
	}
	return 1;
}

void
mix_eof(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	DPRINTF("mix_eof: %s: detached\n", p->name);
	mix_rm(p, ibuf);
	/*
	 * If there's no more inputs, abuf_run() will trigger the eof
	 * condition and propagate it, so no need to handle it here.
	 */
	abuf_run(obuf);
	DPRINTF("mix_eof: done\n");
}

void
mix_hup(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf;

	while (!LIST_EMPTY(&p->ibuflist)) {
		ibuf = LIST_FIRST(&p->ibuflist);
		mix_rm(p, ibuf);
		abuf_hup(ibuf);
	}
	DPRINTF("mix_hup: %s: done\n", p->name);
	aproc_del(p);
}

void
mix_newin(struct aproc *p, struct abuf *ibuf)
{
	ibuf->mixdone = 0;
	ibuf->mixvol = ADATA_UNIT;
}

void
mix_newout(struct aproc *p, struct abuf *obuf)
{
	obuf->mixtodo = 0;
	mix_bzero(p);
}

struct aproc_ops mix_ops = {
	"mix", mix_in, mix_out, mix_eof, mix_hup, mix_newin, mix_newout
};

struct aproc *
mix_new(void)
{
	struct aproc *p;

	p = aproc_new(&mix_ops, "softmix");
	return p;
}

/*
 * Copy data from ibuf to obuf.
 */
void
sub_bcopy(struct abuf *ibuf, struct abuf *obuf)
{
	unsigned char *idata, *odata;
	unsigned icount, ocount, scount;

	idata = abuf_rgetblk(ibuf, &icount, obuf->subdone);
	if (icount == 0)
		return;
	odata = abuf_wgetblk(obuf, &ocount, 0);
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	memcpy(odata, idata, scount);
	obuf->subdone += scount;
	obuf->used += scount;
	DPRINTFN(4, "sub_bcopy: %u bytes\n", scount);
}

void
sub_rm(struct aproc *p, struct abuf *obuf)
{
	LIST_REMOVE(obuf, oent);
	DPRINTF("sub_rm: %s\n", p->name);
}

void
sub_del(struct aproc *p)
{
	aproc_del(p);
}

int
sub_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext;
	unsigned done;
	int again;

	again = 1;
	done = ibuf->used;
	for (i = LIST_FIRST(&p->obuflist); i != LIST_END(&p->obuflist); i = inext) {
		inext = LIST_NEXT(i, oent);
		if (ABUF_WOK(i)) {
			sub_bcopy(ibuf, i);
			abuf_flush(i);
		}
		if (!ABUF_WOK(i))
			again = 0;
		if (done > i->subdone)
			done = i->subdone;
	}
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->subdone -= done;
	}
	ibuf->used -= done;
	ibuf->start += done;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;
	return again;
}

int
sub_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct abuf *i, *inext;
	unsigned done;

	if (obuf->subdone >= ibuf->used)
		return 0;

	sub_bcopy(ibuf, obuf);

	done = ibuf->used;
	LIST_FOREACH(i, &p->obuflist, oent) {
		if (i != obuf && ABUF_WOK(i)) {
			sub_bcopy(ibuf, i);
			abuf_flush(i);
		}
		if (done > i->subdone)
			done = i->subdone;
	}
	if (done == 0)
		return 0;
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->subdone -= done;
	}
	ibuf->used -= done;
	ibuf->start += done;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;
	if (ABUF_EOF(ibuf)) {
		abuf_hup(ibuf);
		for (i = LIST_FIRST(&p->obuflist);
		     i != LIST_END(&p->obuflist);
		     i = inext) {
			inext = LIST_NEXT(i, oent);
			if (i != ibuf)
				abuf_eof(i);
		}
		ibuf->wproc = NULL;
		sub_del(p);
		return 0;
	}
	abuf_fill(ibuf);
	return 1;
}

void
sub_eof(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf;

	while (!LIST_EMPTY(&p->obuflist)) {
		obuf = LIST_FIRST(&p->obuflist);
		sub_rm(p, obuf);
		abuf_eof(obuf);
	}
	sub_del(p);
}

void
sub_hup(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	DPRINTF("sub_hup: %s: detached\n", p->name);
	sub_rm(p, obuf);
	if (LIST_EMPTY(&p->obuflist)) {
		abuf_hup(ibuf);
		sub_del(p);
	} else
		abuf_run(ibuf);
	DPRINTF("sub_hup: done\n");
}

void
sub_newout(struct aproc *p, struct abuf *obuf)
{
	obuf->subdone = 0;
}

struct aproc_ops sub_ops = {
	"sub", sub_in, sub_out, sub_eof, sub_hup, NULL, sub_newout
};

struct aproc *
sub_new(void)
{
	struct aproc *p;

	p = aproc_new(&sub_ops, "copy");
	return p;
}


/*
 * Convert one block.
 */
void
conv_bcopy(struct aconv *ist, struct aconv *ost,
    struct abuf *ibuf, struct abuf *obuf)
{
	int *ictx;
	unsigned inch, ibps;
	unsigned char *idata;
	int ibnext, isigbit;
	unsigned ishift;
	int isnext;
	unsigned ipos, orate;
	unsigned ifr;
	int *octx;
	unsigned onch, oshift;
	int osigbit;
	unsigned obps;
	unsigned char *odata;
	int obnext, osnext;
	unsigned opos, irate;
	unsigned ofr;
	unsigned c, i;
	int s, *ctx;
	unsigned icount, ocount;

	/*
	 * It's ok to have s uninitialized, but we dont want the compiler to
	 * complain about it.
	 */
	s = (int)0xdeadbeef;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = abuf_rgetblk(ibuf, &icount, 0);
	ifr = icount / ibuf->bpf;

	odata = abuf_wgetblk(obuf, &ocount, 0);
	ofr = ocount / obuf->bpf;

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	ictx = ist->ctx + ist->cmin;
	octx = ist->ctx + ost->cmin;
	inch = ist->nch;
	ibps = ist->bps;
	ibnext = ist->bnext;
	isigbit = ist->sigbit;
	ishift = ist->shift;
	isnext = ist->snext;
	ipos = ist->pos;
	irate = ist->rate;
	onch = ost->nch;
	oshift = ost->shift;
	osigbit = ost->sigbit;
	obps = ost->bps;
	obnext = ost->bnext;
	osnext = ost->snext;
	opos = ost->pos;
	orate = ost->rate;

	/*
	 * Start conversion.
	 */
	idata += ist->bfirst;
	odata += ost->bfirst;
	DPRINTFN(4, "conv_bcopy: ifr=%d ofr=%d\n", ifr, ofr);
	for (;;) {
		if ((int)(ipos - opos) > 0) {
			if (ofr == 0)
				break;
			ctx = octx;
			for (c = onch; c > 0; c--) {
				s = *ctx++ << 16;
				s >>= oshift;
				s ^= osigbit;
				for (i = obps; i > 0; i--) {
					*odata = (unsigned char)s;
					s >>= 8;
					odata += obnext;
				}
				odata += osnext;
			}
			opos += irate;
			ofr--;
		} else {
			if (ifr == 0)
				break;
			ctx = ictx;
			for (c = inch; c > 0; c--) {
				for (i = ibps; i > 0; i--) {
					s <<= 8;
					s |= *idata;
					idata += ibnext;
				}
				s ^= isigbit;
				s <<= ishift;
				*ctx++ = (short)(s >> 16);
				idata += isnext;
			}
			ipos += orate;
			ifr--;
		}
	}
	ist->pos = ipos;
	ost->pos = opos;
	DPRINTFN(4, "conv_bcopy: done, ifr=%d ofr=%d\n", ifr, ofr);

	/*
	 * Update FIFO pointers.
	 */
	icount -= ifr * ist->bpf;
	ibuf->used -= icount;
	ibuf->start += icount;
	if (ibuf->start >= ibuf->len)
		ibuf->start -= ibuf->len;

	ocount -= ofr * ost->bpf;
	obuf->used += ocount;
}

void
conv_del(struct aproc *p)
{
	aproc_del(p);
}

int
conv_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	if (!ABUF_WOK(obuf))
		return 0;
	conv_bcopy(&p->u.conv.ist, &p->u.conv.ost, ibuf, obuf);
	abuf_flush(obuf);
	return ABUF_WOK(obuf);
}

int
conv_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	if (!ABUF_ROK(ibuf))
		return 0;
	conv_bcopy(&p->u.conv.ist, &p->u.conv.ost, ibuf, obuf);
	if (ABUF_EOF(ibuf)) {
		obuf->wproc = NULL;
		abuf_hup(ibuf);
		conv_del(p);
		return 0;
	}
	abuf_fill(ibuf);
	return 1;
}

void
conv_eof(struct aproc *p, struct abuf *ibuf)
{
	abuf_eof(LIST_FIRST(&p->obuflist));
	conv_del(p);
}

void
conv_hup(struct aproc *p, struct abuf *obuf)
{
	abuf_hup(LIST_FIRST(&p->ibuflist));
	conv_del(p);
}

void
aconv_init(struct aconv *st, struct aparams *par, int input)
{
	unsigned i;

	st->bps = par->bps;
	st->sigbit = par->sig ? 0 : 1 << (par->bits - 1);
	if (par->msb) {
		st->shift = 32 - par->bps * 8;
	} else {
		st->shift = 32 - par->bits;
	}
	if ((par->le && input) || (!par->le && !input)) {
		st->bfirst = st->bps - 1;
		st->bnext = -1;
		st->snext = 2 * st->bps;
	} else {
		st->bfirst = 0;
		st->bnext = 1;
		st->snext = 0;
	}
	st->cmin = par->cmin;
	st->nch = par->cmax - par->cmin + 1;
	st->bpf = st->nch * st->bps;
	st->rate = par->rate;
	st->pos = 0;

	for (i = 0; i < CHAN_MAX; i++)
		st->ctx[i] = 0;
}

struct aproc_ops conv_ops = {
	"conv", conv_in, conv_out, conv_eof, conv_hup, NULL, NULL
};

struct aproc *
conv_new(char *name, struct aparams *ipar, struct aparams *opar)
{
	struct aproc *p;

	p = aproc_new(&conv_ops, name);
	aconv_init(&p->u.conv.ist, ipar, 1);
	aconv_init(&p->u.conv.ost, opar, 0);
	return p;
}
