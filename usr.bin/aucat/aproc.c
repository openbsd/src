/*	$OpenBSD: aproc.c,v 1.32 2009/07/25 08:44:27 ratchov Exp $	*/
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
 *  - resamp: resample streams in native format
 *
 * TODO
 *
 * 	(easy) split the "conv" into 2 converters: one for input (that
 *	convers anything to 16bit signed) and one for the output (that
 *	converts 16bit signed to anything)
 *
 *	(hard) add a lowpass filter for the resampler. Quality is
 *	not acceptable as is.
 *
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
	p->refs = 0;
	return p;
}

void
aproc_del(struct aproc *p)
{
	struct abuf *i;

	DPRINTF("aproc_del: %s(%s): terminating...\n", p->ops->name, p->name);

	if (p->ops->done)
		p->ops->done(p);

	while (!LIST_EMPTY(&p->ibuflist)) {
		i = LIST_FIRST(&p->ibuflist);
		abuf_hup(i);
	}
	while (!LIST_EMPTY(&p->obuflist)) {
		i = LIST_FIRST(&p->obuflist);
		abuf_eof(i);
	}
	if (p->refs > 0) {
		DPRINTF("aproc_del: %s(%s): has refs\n", p->ops->name, p->name);
		return;
	}
	DPRINTF("aproc_del: %s(%s): freed\n", p->ops->name, p->name);
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

void
aproc_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	struct abuf *obuf;

	DPRINTFN(3, "aproc_ipos: %s: delta = %d\n", p->name, delta);

	LIST_FOREACH(obuf, &p->obuflist, oent) {
		abuf_ipos(obuf, delta);
	}
}

void
aproc_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf;

	DPRINTFN(3, "aproc_opos: %s: delta = %d\n", p->name, delta);

	LIST_FOREACH(ibuf, &p->ibuflist, ient) {
		abuf_opos(ibuf, delta);
	}
}

int
aproc_inuse(struct aproc *p)
{
	struct abuf *i;

	LIST_FOREACH(i, &p->ibuflist, ient) {
		if (i->inuse)
			return 1;
	}
	LIST_FOREACH(i, &p->obuflist, oent) {
		if (i->inuse)
			return 1;
	}
	DPRINTFN(3, "aproc_inuse: %s: not inuse\n", p->name);
	return 0;
}

int
aproc_depend(struct aproc *p, struct aproc *dep)
{
	struct abuf *i;

	if (p == dep)
		return 1;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		if (i->wproc && aproc_depend(i->wproc, dep))
			return 1;
	}
	return 0;
}

int
rpipe_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	DPRINTFN(3, "rpipe_in: %s\n", p->name);

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
rpipe_out(struct aproc *p, struct abuf *obuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (f->state & FILE_RINUSE)
		return 0;
	DPRINTFN(3, "rpipe_out: %s\n", p->name);

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
rpipe_done(struct aproc *p)
{
	struct file *f = p->u.io.file;

	if (f == NULL)
		return;
	f->rproc = NULL;
	if (f->wproc == NULL)
		file_del(f);
	p->u.io.file = NULL;
}

void
rpipe_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	DPRINTFN(3, "rpipe_eof: %s\n", p->name);
	aproc_del(p);
}

void
rpipe_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(3, "rpipe_hup: %s\n", p->name);
	aproc_del(p);
}

struct aproc_ops rpipe_ops = {
	"rpipe",
	rpipe_in,
	rpipe_out,
	rpipe_eof,
	rpipe_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	rpipe_done
};

struct aproc *
rpipe_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rpipe_ops, f->name);
	p->u.io.file = f;
	f->rproc = p;
	return p;
}

void
wpipe_done(struct aproc *p)
{
	struct file *f = p->u.io.file;

	if (f == NULL)
		return;
	f->wproc = NULL;
	if (f->rproc == NULL)
		file_del(f);
	p->u.io.file = NULL;
}

int
wpipe_in(struct aproc *p, struct abuf *ibuf)
{
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	if (f->state & FILE_WINUSE)
		return 0;
	DPRINTFN(3, "wpipe_in: %s\n", p->name);

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
wpipe_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned count;

	DPRINTFN(3, "wpipe_out: %s\n", p->name);

	if (!abuf_fill(ibuf)) {
		DPRINTFN(3, "wpipe_out: fill failed\n");
		return 0;
	}
	if (ABUF_EMPTY(ibuf) || !(f->state & FILE_WOK))
		return 0;
	data = abuf_rgetblk(ibuf, &count, 0);
	if (count == 0) {
		DPRINTF("wpipe_out: %s: underrun\n", p->name);
		return 0;
	}
	count = file_write(f, data, count);
	if (count == 0)
		return 0;
	abuf_rdiscard(ibuf, count);
	return 1;
}

void
wpipe_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(3, "wpipe_eof: %s\n", p->name);
	aproc_del(p);
}

void
wpipe_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	DPRINTFN(3, "wpipe_hup: %s\n", p->name);
	aproc_del(p);
}

struct aproc_ops wpipe_ops = {
	"wpipe",
	wpipe_in,
	wpipe_out,
	wpipe_eof,
	wpipe_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	wpipe_done
};

struct aproc *
wpipe_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wpipe_ops, f->name);
	p->u.io.file = f;
	f->wproc = p;
	return p;
}

/*
 * Append the given amount of silence (or less if there's not enough
 * space), and crank mixitodo accordingly
 */
void
mix_bzero(struct abuf *obuf, unsigned zcount)
{
	short *odata;
	unsigned ocount;

	DPRINTFN(4, "mix_bzero: used = %u, zcount = %u\n", obuf->used, zcount);
	odata = (short *)abuf_wgetblk(obuf, &ocount, obuf->mixitodo);
	ocount -= ocount % obuf->bpf;
	if (ocount > zcount)
		ocount = zcount;
	memset(odata, 0, ocount);
	obuf->mixitodo += ocount;
}

/*
 * Mix an input block over an output block.
 */
void
mix_badd(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned i, j, icnt, onext, ostart;
	unsigned scount, icount, ocount, zcount;
	int vol;

	DPRINTFN(4, "mix_badd: todo = %u, done = %u\n",
	    obuf->mixitodo, ibuf->mixodone);

	/*
	 * calculate the maximum we can read
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;

	/*
	 * zero-fill if necessary
	 */
	zcount = ibuf->mixodone + icount * obuf->bpf;
	if (zcount > obuf->mixitodo)
		mix_bzero(obuf, zcount - obuf->mixitodo);

	/*
	 * calculate the maximum we can write
	 */
	odata = (short *)abuf_wgetblk(obuf, &ocount, ibuf->mixodone);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;

	vol = (ibuf->mixweight * ibuf->mixvol) >> ADATA_SHIFT;
	ostart = ibuf->cmin - obuf->cmin;
	onext = obuf->cmax - ibuf->cmax + ostart;
	icnt = ibuf->cmax - ibuf->cmin + 1;
	odata += ostart;
	scount = (icount < ocount) ? icount : ocount;
	for (i = scount; i > 0; i--) {
		for (j = icnt; j > 0; j--) {
			*odata += (*idata * vol) >> ADATA_SHIFT;
			idata++;
			odata++;
		}
		odata += onext;
	}
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	ibuf->mixodone += scount * obuf->bpf;

	DPRINTFN(4, "mix_badd: added %u, done = %u, todo = %u\n",
	    scount, ibuf->mixodone, obuf->mixitodo);
}

/*
 * Handle buffer underrun, return 0 if stream died.
 */
int
mix_xrun(struct abuf *i, struct abuf *obuf)
{
	unsigned fdrop;

	if (i->mixodone > 0)
		return 1;
	if (i->xrun == XRUN_ERROR) {
		abuf_hup(i);
		return 0;
	}
	mix_bzero(obuf, obuf->len);
	fdrop = obuf->mixitodo / obuf->bpf;
	i->mixodone += fdrop * obuf->bpf;
	if (i->xrun == XRUN_SYNC)
		i->drop += fdrop * i->bpf;
	else {
		abuf_opos(i, -(int)fdrop);
		if (i->duplex) {
			DPRINTF("mix_xrun: duplex %u\n", fdrop);
			i->duplex->drop += fdrop * i->duplex->bpf;
			abuf_ipos(i->duplex, -(int)fdrop);
		}
	}
	DPRINTF("mix_xrun: drop = %u\n", i->drop);
	return 1;
}

int
mix_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext, *obuf = LIST_FIRST(&p->obuflist);
	unsigned odone;

	DPRINTFN(4, "mix_in: used/len = %u/%u, done/todo = %u/%u\n",
	    ibuf->used, ibuf->len, ibuf->mixodone, obuf->mixitodo);

	if (!ABUF_ROK(ibuf))
		return 0;
	odone = obuf->len;
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (!abuf_fill(i))
			continue; /* eof */
		mix_badd(i, obuf);
		if (odone > i->mixodone)
			odone = i->mixodone;
	}
	if (LIST_EMPTY(&p->ibuflist) || odone == 0)
		return 0;
	p->u.mix.lat += odone / obuf->bpf;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		i->mixodone -= odone;
	}
	abuf_wcommit(obuf, odone);
	obuf->mixitodo -= odone;
	if (!abuf_flush(obuf))
		return 0; /* hup */
	return 1;
}

int
mix_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *inext;
	unsigned odone;

	DPRINTFN(4, "mix_out: used/len = %u/%u, todo/len = %u/%u\n",
	    obuf->used, obuf->len, obuf->mixitodo, obuf->len);

	if (!ABUF_WOK(obuf))
		return 0;
	odone = obuf->len;
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (!abuf_fill(i))
			continue; /* eof */
		if (!ABUF_ROK(i)) {
			if (p->u.mix.flags & MIX_DROP) {
				if (!mix_xrun(i, obuf))
					continue;
			}
		} else
			mix_badd(i, obuf);
		if (odone > i->mixodone)
			odone = i->mixodone;
	}
	if (LIST_EMPTY(&p->ibuflist)) {
		if (p->u.mix.flags & MIX_AUTOQUIT) {
			DPRINTF("mix_out: nothing more to do...\n");
			aproc_del(p);
			return 0;
		}
		if (!(p->u.mix.flags & MIX_DROP))
			return 0;
		mix_bzero(obuf, obuf->len);
		odone = obuf->mixitodo;
		p->u.mix.idle += odone / obuf->bpf;
	}
	if (odone == 0)
		return 0;
	p->u.mix.lat += odone / obuf->bpf;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		i->mixodone -= odone;
	}
	abuf_wcommit(obuf, odone);
	obuf->mixitodo -= odone;
	return 1;
}

void
mix_eof(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *obuf = LIST_FIRST(&p->obuflist);
	unsigned odone;

	DPRINTF("mix_eof: %s: detached\n", p->name);
	mix_setmaster(p);

	if (!aproc_inuse(p)) {
		DPRINTF("mix_eof: %s: from input\n", p->name);
		/*
		 * find a blocked input
		 */
		odone = obuf->len;
		LIST_FOREACH(i, &p->ibuflist, ient) {
			if (ABUF_ROK(i) && i->mixodone < obuf->mixitodo) {
				abuf_run(i);
				return;
			}
			if (odone > i->mixodone)
				odone = i->mixodone;
		}
		/*
		 * no blocked inputs, check if output is blocked
		 */
		if (LIST_EMPTY(&p->ibuflist) || odone == obuf->mixitodo)
			abuf_run(obuf);
	}
}

void
mix_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTF("mix_hup: %s: done\n", p->name);
	aproc_del(p);
}

void
mix_newin(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	if (!obuf || ibuf->cmin < obuf->cmin || ibuf->cmax > obuf->cmax) {
		fprintf(stderr, "mix_newin: channel ranges mismatch\n");
		abort();
	}
	p->u.mix.idle = 0;
	ibuf->mixodone = 0;
	ibuf->mixvol = ADATA_UNIT;
	ibuf->mixweight = ADATA_UNIT;
	ibuf->mixmaxweight = ADATA_UNIT;
	ibuf->xrun = XRUN_IGNORE;
}

void
mix_newout(struct aproc *p, struct abuf *obuf)
{
	DPRINTF("mix_newout: using %u fpb\n", obuf->len / obuf->bpf);
	obuf->mixitodo = 0;
}

void
mix_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	DPRINTFN(3, "mix_opos: lat = %d/%d\n", p->u.mix.lat, p->u.mix.maxlat);
	p->u.mix.lat -= delta;
	aproc_opos(p, obuf, delta);
}

struct aproc_ops mix_ops = {
	"mix",
	mix_in,
	mix_out,
	mix_eof,
	mix_hup,
	mix_newin,
	mix_newout,
	aproc_ipos,
	mix_opos,
	NULL
};

struct aproc *
mix_new(char *name, int maxlat)
{
	struct aproc *p;

	p = aproc_new(&mix_ops, name);
	p->u.mix.flags = 0;
	p->u.mix.idle = 0;
	p->u.mix.lat = 0;
	p->u.mix.maxlat = maxlat;
	return p;
}

/*
 * Normalize input levels
 */
void
mix_setmaster(struct aproc *p)
{
	unsigned n;
	struct abuf *buf;
	int weight;

	n = 0;
	LIST_FOREACH(buf, &p->ibuflist, ient) {
		n++;
	}
	LIST_FOREACH(buf, &p->ibuflist, ient) {
		weight = ADATA_UNIT / n;
		if (weight > buf->mixmaxweight)
			weight = buf->mixmaxweight;
		buf->mixweight = weight;
		DPRINTF("mix_setmaster: %p: %d/%d -> %d\n", buf,
		    buf->mixweight, buf->mixmaxweight, weight);
	}
}

void
mix_clear(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	p->u.mix.lat = 0;
	obuf->mixitodo = 0;
}

/*
 * Copy data from ibuf to obuf.
 */
void
sub_bcopy(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned i, j, ocnt, inext, istart;
	unsigned icount, ocount, scount;

	idata = (short *)abuf_rgetblk(ibuf, &icount, obuf->subidone);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	istart = obuf->cmin - ibuf->cmin;
	inext = ibuf->cmax - obuf->cmax + istart;
	ocnt = obuf->cmax - obuf->cmin + 1;
	scount = (icount < ocount) ? icount : ocount;
	idata += istart;
	for (i = scount; i > 0; i--) {
		for (j = ocnt; j > 0; j--) {
			*odata = *idata;
			odata++;
			idata++;
		}
		idata += inext;
	}
	abuf_wcommit(obuf, scount * obuf->bpf);
	obuf->subidone += scount * ibuf->bpf;
	DPRINTFN(4, "sub_bcopy: %u frames\n", scount);
}

/*
 * Handle buffer overruns, return 0 if the stream died
 */
int
sub_xrun(struct abuf *ibuf, struct abuf *i)
{
	unsigned fdrop;

	if (i->subidone > 0)
		return 1;
	if (i->xrun == XRUN_ERROR) {
		abuf_eof(i);
		return 0;
	}
	fdrop = ibuf->used / ibuf->bpf;
	if (i->xrun == XRUN_SYNC)
		i->silence += fdrop * i->bpf;
	else {
		abuf_ipos(i, -(int)fdrop);
		if (i->duplex) {
			DPRINTF("sub_xrun: duplex %u\n", fdrop);
			i->duplex->silence += fdrop * i->duplex->bpf;
			abuf_opos(i->duplex, -(int)fdrop);
		}
	}
	i->subidone += fdrop * ibuf->bpf;
	DPRINTF("sub_xrun: silence = %u\n", i->silence);
	return 1;
}

int
sub_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext;
	unsigned idone;

	if (!ABUF_ROK(ibuf))
		return 0;
	idone = ibuf->len;
	for (i = LIST_FIRST(&p->obuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		if (!ABUF_WOK(i)) {
			if (p->u.sub.flags & SUB_DROP) {
				if (!sub_xrun(ibuf, i))
					continue;
			}
		} else
			sub_bcopy(ibuf, i);
		if (idone > i->subidone)
			idone = i->subidone;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->obuflist)) {
		if (p->u.sub.flags & SUB_AUTOQUIT) {
			DPRINTF("sub_in: nothing more to do...\n");
			aproc_del(p);
			return 0;
		}
		if (!(p->u.sub.flags & SUB_DROP))
			return 0;
		idone = ibuf->used;
		p->u.sub.idle += idone / ibuf->bpf;
	}
	if (idone == 0)
		return 0;
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->subidone -= idone;
	}
	abuf_rdiscard(ibuf, idone);
	p->u.sub.lat -= idone / ibuf->bpf;
	return 1;
}

int
sub_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct abuf *i, *inext;
	unsigned idone;

	if (!ABUF_WOK(obuf))
		return 0;
	if (!abuf_fill(ibuf))
		return 0; /* eof */
	idone = ibuf->len;
	for (i = LIST_FIRST(&p->obuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		sub_bcopy(ibuf, i);
		if (idone > i->subidone)
			idone = i->subidone;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->obuflist) || idone == 0)
		return 0;
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->subidone -= idone;
	}
	abuf_rdiscard(ibuf, idone);
	p->u.sub.lat -= idone / ibuf->bpf;
	return 1;
}

void
sub_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTF("sub_hup: %s: eof\n", p->name);
	aproc_del(p);
}

void
sub_hup(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *ibuf = LIST_FIRST(&p->ibuflist);
	unsigned idone;

	DPRINTF("sub_hup: %s: detached\n", p->name);

	if (!aproc_inuse(p)) {
		DPRINTF("sub_hup: %s: from input\n", p->name);
		/*
		 * find a blocked output
		 */
		idone = ibuf->len;
		LIST_FOREACH(i, &p->obuflist, oent) {
			if (ABUF_WOK(i) && i->subidone < ibuf->used) {
				abuf_run(i);
				return;
			}
			if (idone > i->subidone)
				idone = i->subidone;
		}
		/*
		 * no blocked outputs, check if input is blocked
		 */
		if (LIST_EMPTY(&p->obuflist) || idone == ibuf->used)
			abuf_run(ibuf);
	}
}

void
sub_newout(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	if (!ibuf || obuf->cmin < ibuf->cmin || obuf->cmax > ibuf->cmax) {
		fprintf(stderr, "sub_newout: channel ranges mismatch\n");
		abort();
	}
	p->u.sub.idle = 0;
	obuf->subidone = 0;
	obuf->xrun = XRUN_IGNORE;
}

void
sub_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	p->u.sub.lat += delta;
	DPRINTFN(3, "sub_ipos: lat = %d/%d\n", p->u.sub.lat, p->u.sub.maxlat);
	aproc_ipos(p, ibuf, delta);
}

struct aproc_ops sub_ops = {
	"sub",
	sub_in,
	sub_out,
	sub_eof,
	sub_hup,
	NULL,
	sub_newout,
	sub_ipos,
	aproc_opos,
	NULL
};

struct aproc *
sub_new(char *name, int maxlat)
{
	struct aproc *p;

	p = aproc_new(&sub_ops, name);
	p->u.sub.flags = 0;
	p->u.sub.idle = 0;
	p->u.sub.lat = 0;
	p->u.sub.maxlat = maxlat;
	return p;
}

void
sub_clear(struct aproc *p)
{
	p->u.mix.lat = 0;
}

/*
 * Convert one block.
 */
void
resamp_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned inch;
	short *idata;
	unsigned oblksz;
	unsigned ifr;
	unsigned onch;
	int s1, s2, diff;
	short *odata;
	unsigned iblksz;
	unsigned ofr;
	unsigned c;
	short *ctxbuf, *ctx;
	unsigned ctx_start;
	unsigned icount, ocount;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	ifr = icount / ibuf->bpf;
	icount = ifr * ibuf->bpf;

	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ofr = ocount / obuf->bpf;
	ocount = ofr * obuf->bpf;

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	diff = p->u.resamp.diff;
	inch = ibuf->cmax - ibuf->cmin + 1;
	iblksz = p->u.resamp.iblksz;
	onch = obuf->cmax - obuf->cmin + 1;
	oblksz = p->u.resamp.oblksz;
	ctxbuf = p->u.resamp.ctx;
	ctx_start = p->u.resamp.ctx_start;

	/*
	 * Start conversion.
	 */
	DPRINTFN(4, "resamp_bcopy: ifr=%d ofr=%d\n", ifr, ofr);
	for (;;) {
		if (diff < 0) {
			if (ifr == 0)
				break;
			ctx_start ^= 1;
			ctx = ctxbuf + ctx_start;
			for (c = inch; c > 0; c--) {
				*ctx = *idata++;
				ctx += RESAMP_NCTX;
			}
			diff += oblksz;
			ifr--;
		} else {
			if (ofr == 0)
				break;
			ctx = ctxbuf;
			for (c = onch; c > 0; c--) {
				s1 = ctx[ctx_start];
				s2 = ctx[ctx_start ^ 1];
				ctx += RESAMP_NCTX;
				*odata++ = s1 + (s2 - s1) * diff / (int)oblksz;
			}
			diff -= iblksz;
			ofr--;
		}
	}
	p->u.resamp.diff = diff;
	p->u.resamp.ctx_start = ctx_start;
	DPRINTFN(4, "resamp_bcopy: done, ifr=%d ofr=%d\n", ifr, ofr);

	/*
	 * Update FIFO pointers.
	 */
	icount -= ifr * ibuf->bpf;
	ocount -= ofr * obuf->bpf;
	abuf_rdiscard(ibuf, icount);
	abuf_wcommit(obuf, ocount);
}

int
resamp_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	DPRINTFN(4, "resamp_in: %s\n", p->name);

	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	resamp_bcopy(p, ibuf, obuf);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
resamp_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	DPRINTFN(4, "resamp_out: %s\n", p->name);

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	resamp_bcopy(p, ibuf, obuf);
	return 1;
}

void
resamp_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(4, "resamp_eof: %s\n", p->name);

	aproc_del(p);
}

void
resamp_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(4, "resamp_hup: %s\n", p->name);

	aproc_del(p);
}

void
resamp_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	long long ipos;
	int ifac, ofac;

	DPRINTFN(3, "resamp_ipos: %d\n", delta);

	ifac = p->u.resamp.iblksz;
	ofac = p->u.resamp.oblksz;
	ipos = p->u.resamp.idelta + (long long)delta * ofac;
	delta = (ipos + ifac - 1) / ifac;
	p->u.resamp.idelta = ipos - (long long)delta * ifac;
	abuf_ipos(obuf, delta);
}

void
resamp_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	long long opos;
	int ifac, ofac;

	DPRINTFN(3, "resamp_opos: %d\n", delta);

	ifac = p->u.resamp.iblksz;
	ofac = p->u.resamp.oblksz;
	opos = p->u.resamp.odelta + (long long)delta * ifac;
	delta = (opos + ofac - 1) / ofac;
	p->u.resamp.odelta = opos - (long long)delta * ofac;
	abuf_opos(ibuf, delta);
}

struct aproc_ops resamp_ops = {
	"resamp",
	resamp_in,
	resamp_out,
	resamp_eof,
	resamp_hup,
	NULL,
	NULL,
	resamp_ipos,
	resamp_opos,
	NULL
};

struct aproc *
resamp_new(char *name, unsigned iblksz, unsigned oblksz)
{
	struct aproc *p;
	unsigned i;

	p = aproc_new(&resamp_ops, name);
	p->u.resamp.iblksz = iblksz;
	p->u.resamp.oblksz = oblksz;
	p->u.resamp.diff = 0;
	p->u.resamp.idelta = 0;
	p->u.resamp.odelta = 0;
	p->u.resamp.ctx_start = 0;
	for (i = 0; i < NCHAN_MAX * RESAMP_NCTX; i++)
		p->u.resamp.ctx[i] = 0;
#ifdef DEBUG
	if (debug_level > 0)
		fprintf(stderr, "resamp_new: %u/%u\n", iblksz, oblksz);
#endif
	return p;
}

/*
 * Convert one block.
 */
void
cmap_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned inch;
	short *idata;
	unsigned onch;
	short *odata;
	short *ctx, *ictx, *octx;
	unsigned c, f, scount, icount, ocount;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	scount = icount < ocount ? icount : ocount;
	inch = ibuf->cmax - ibuf->cmin + 1;
	onch = obuf->cmax - obuf->cmin + 1;
	ictx = p->u.cmap.ctx + ibuf->cmin;
	octx = p->u.cmap.ctx + obuf->cmin;

	for (f = scount; f > 0; f--) {
		ctx = ictx;
		for (c = inch; c > 0; c--) {
			*ctx = *idata;
			idata++;
			ctx++;
		}
		ctx = octx;
		for (c = onch; c > 0; c--) {
			*odata = *ctx;
			odata++;
			ctx++;
		}
	}
	DPRINTFN(4, "cmap_bcopy: scount = %u\n", scount);
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
cmap_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	DPRINTFN(4, "cmap_in: %s\n", p->name);

	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	cmap_bcopy(p, ibuf, obuf);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
cmap_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	DPRINTFN(4, "cmap_out: %s\n", p->name);

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	cmap_bcopy(p, ibuf, obuf);
	return 1;
}

void
cmap_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(4, "cmap_eof: %s\n", p->name);

	aproc_del(p);
}

void
cmap_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(4, "cmap_hup: %s\n", p->name);

	aproc_del(p);
}

struct aproc_ops cmap_ops = {
	"cmap",
	cmap_in,
	cmap_out,
	cmap_eof,
	cmap_hup,
	NULL,
	NULL,
	aproc_ipos,
	aproc_opos,
	NULL
};

struct aproc *
cmap_new(char *name, struct aparams *ipar, struct aparams *opar)
{
	struct aproc *p;
	unsigned i;

	p = aproc_new(&cmap_ops, name);
	for (i = 0; i < NCHAN_MAX; i++)
		p->u.cmap.ctx[i] = 0;
#ifdef DEBUG
	if (debug_level > 0) {
		fprintf(stderr, "cmap_new: %s: ", p->name);
		aparams_print2(ipar, opar);
		fprintf(stderr, "\n");
	}
#endif
	return p;
}

/*
 * Convert one block.
 */
void
enc_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned nch, scount, icount, ocount;
	unsigned f;
	short *idata;
	int s;
	unsigned oshift;
	int osigbit;
	unsigned obps;
	unsigned i;
	unsigned char *odata;
	int obnext;
	int osnext;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = ibuf->cmax - ibuf->cmin + 1;
	DPRINTFN(4, "enc_bcopy: scount = %u, nch = %u\n", scount, nch);

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	oshift = p->u.conv.shift;
	osigbit = p->u.conv.sigbit;
	obps = p->u.conv.bps;
	obnext = p->u.conv.bnext;
	osnext = p->u.conv.snext;

	/*
	 * Start conversion.
	 */
	odata += p->u.conv.bfirst;
	for (f = scount * nch; f > 0; f--) {
		s = *idata++;
		s <<= 16;
		s >>= oshift;
		s ^= osigbit;
		for (i = obps; i > 0; i--) {
			*odata = (unsigned char)s;
			s >>= 8;
			odata += obnext;
		}
		odata += osnext;
	}

	/*
	 * Update FIFO pointers.
	 */
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
enc_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	DPRINTFN(4, "enc_in: %s\n", p->name);

	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	enc_bcopy(p, ibuf, obuf);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
enc_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	DPRINTFN(4, "enc_out: %s\n", p->name);

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	enc_bcopy(p, ibuf, obuf);
	return 1;
}

void
enc_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(4, "enc_eof: %s\n", p->name);

	aproc_del(p);
}

void
enc_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(4, "enc_hup: %s\n", p->name);

	aproc_del(p);
}

struct aproc_ops enc_ops = {
	"enc",
	enc_in,
	enc_out,
	enc_eof,
	enc_hup,
	NULL,
	NULL,
	aproc_ipos,
	aproc_opos,
	NULL
};

struct aproc *
enc_new(char *name, struct aparams *par)
{
	struct aproc *p;

	p = aproc_new(&enc_ops, name);
	p->u.conv.bps = par->bps;
	p->u.conv.sigbit = par->sig ? 0 : 1 << (par->bits - 1);
	if (par->msb) {
		p->u.conv.shift = 32 - par->bps * 8;
	} else {
		p->u.conv.shift = 32 - par->bits;
	}
	if (!par->le) {
		p->u.conv.bfirst = par->bps - 1;
		p->u.conv.bnext = -1;
		p->u.conv.snext = 2 * par->bps;
	} else {
		p->u.conv.bfirst = 0;
		p->u.conv.bnext = 1;
		p->u.conv.snext = 0;
	}
#ifdef DEBUG
	if (debug_level > 0) {
		fprintf(stderr, "enc_new: %s: ", p->name);
		aparams_print(par);
		fprintf(stderr, "\n");
	}
#endif
	return p;
}

/*
 * Convert one block.
 */
void
dec_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned nch, scount, icount, ocount;
	unsigned f;
	unsigned ibps;
	unsigned i;
	int s = 0xdeadbeef;
	unsigned char *idata;
	int ibnext;
	int isnext;
	int isigbit;
	unsigned ishift;
	short *odata;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = abuf_rgetblk(ibuf, &icount, 0);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = obuf->cmax - obuf->cmin + 1;
	DPRINTFN(4, "dec_bcopy: scount = %u, nch = %u\n", scount, nch);

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	ibps = p->u.conv.bps;
	ibnext = p->u.conv.bnext;
	isigbit = p->u.conv.sigbit;
	ishift = p->u.conv.shift;
	isnext = p->u.conv.snext;

	/*
	 * Start conversion.
	 */
	idata += p->u.conv.bfirst;
	for (f = scount * nch; f > 0; f--) {
		for (i = ibps; i > 0; i--) {
			s <<= 8;
			s |= *idata;
			idata += ibnext;
		}
		idata += isnext;
		s ^= isigbit;
		s <<= ishift;
		s >>= 16;
		*odata++ = s;
	}

	/*
	 * Update FIFO pointers.
	 */
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
dec_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	DPRINTFN(4, "dec_in: %s\n", p->name);

	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	dec_bcopy(p, ibuf, obuf);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
dec_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	DPRINTFN(4, "dec_out: %s\n", p->name);

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	dec_bcopy(p, ibuf, obuf);
	return 1;
}

void
dec_eof(struct aproc *p, struct abuf *ibuf)
{
	DPRINTFN(4, "dec_eof: %s\n", p->name);

	aproc_del(p);
}

void
dec_hup(struct aproc *p, struct abuf *obuf)
{
	DPRINTFN(4, "dec_hup: %s\n", p->name);

	aproc_del(p);
}

struct aproc_ops dec_ops = {
	"dec",
	dec_in,
	dec_out,
	dec_eof,
	dec_hup,
	NULL,
	NULL,
	aproc_ipos,
	aproc_opos,
	NULL
};

struct aproc *
dec_new(char *name, struct aparams *par)
{
	struct aproc *p;

	p = aproc_new(&dec_ops, name);
	p->u.conv.bps = par->bps;
	p->u.conv.sigbit = par->sig ? 0 : 1 << (par->bits - 1);
	if (par->msb) {
		p->u.conv.shift = 32 - par->bps * 8;
	} else {
		p->u.conv.shift = 32 - par->bits;
	}
	if (par->le) {
		p->u.conv.bfirst = par->bps - 1;
		p->u.conv.bnext = -1;
		p->u.conv.snext = 2 * par->bps;
	} else {
		p->u.conv.bfirst = 0;
		p->u.conv.bnext = 1;
		p->u.conv.snext = 0;
	}
#ifdef DEBUG
	if (debug_level > 0) {
		fprintf(stderr, "dec_new: %s: ", p->name);
		aparams_print(par);
		fprintf(stderr, "\n");
	}
#endif
	return p;
}
