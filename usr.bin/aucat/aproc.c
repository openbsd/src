/*	$OpenBSD: aproc.c,v 1.54 2010/04/24 06:18:23 ratchov Exp $	*/
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
 */
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aparams.h"
#include "aproc.h"
#include "conf.h"
#include "file.h"
#include "midi.h"
#ifdef DEBUG
#include "dbg.h"
#endif

/*
 * Same as ABUF_ROK(), but consider that a buffer is 
 * readable if there's silence pending to be inserted
 */
#define MIX_ROK(buf) (ABUF_ROK(buf) || (buf)->r.mix.drop < 0)

/*
 * Same as ABUF_WOK(), but consider that a buffer is 
 * writeable if there are samples to drop
 */
#define SUB_WOK(buf) (ABUF_WOK(buf) || (buf)->w.sub.silence < 0)

#ifdef DEBUG
void
aproc_dbg(struct aproc *p)
{
	dbg_puts(p->ops->name);
	dbg_puts("(");
	dbg_puts(p->name);
	dbg_puts(")");
}

int
zomb_in(struct aproc *p, struct abuf *ibuf)
{
	aproc_dbg(p);
	dbg_puts(": in: terminated\n");
	dbg_panic();
	return 0;
}


int
zomb_out(struct aproc *p, struct abuf *obuf)
{
	aproc_dbg(p);
	dbg_puts(": out: terminated\n");
	dbg_panic();
	return 0;
}

void
zomb_eof(struct aproc *p, struct abuf *ibuf)
{
	aproc_dbg(p);
	dbg_puts(": eof: terminated\n");
	dbg_panic();
}

void
zomb_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_dbg(p);
	dbg_puts(": hup: terminated\n");
	dbg_panic();
}

void
zomb_newin(struct aproc *p, struct abuf *ibuf)
{
	aproc_dbg(p);
	dbg_puts(": newin: terminated\n");
	dbg_panic();
}

void
zomb_newout(struct aproc *p, struct abuf *obuf)
{
	aproc_dbg(p);
	dbg_puts(": newout: terminated\n");
	dbg_panic();
}

void
zomb_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	aproc_dbg(p);
	dbg_puts(": ipos: terminated\n");
	dbg_panic();
}

void
zomb_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	aproc_dbg(p);
	dbg_puts(": opos: terminated\n");
	dbg_panic();
}

struct aproc_ops zomb_ops = {
	"zomb",
	zomb_in,
	zomb_out,
	zomb_eof,
	zomb_hup,
	zomb_newin,
	zomb_newout,
	zomb_ipos,
	zomb_opos,
	NULL
};
#endif

struct aproc *
aproc_new(struct aproc_ops *ops, char *name)
{
	struct aproc *p;

	p = malloc(sizeof(struct aproc));
	if (p == NULL)
		err(1, name);
	LIST_INIT(&p->ins);
	LIST_INIT(&p->outs);
	p->name = name;
	p->ops = ops;
	p->refs = 0;
	p->flags = 0;
	return p;
}

void
aproc_del(struct aproc *p)
{
	struct abuf *i;

	if (!(p->flags & APROC_ZOMB)) {
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": terminating...\n");
		}
#endif
		if (p->ops->done) {
#ifdef DEBUG
			if (debug_level >= 3) {
				aproc_dbg(p);
				dbg_puts(": done\n");
			}
#endif
			p->ops->done(p);
		}
		while (!LIST_EMPTY(&p->ins)) {
			i = LIST_FIRST(&p->ins);
			abuf_hup(i);
		}
		while (!LIST_EMPTY(&p->outs)) {
			i = LIST_FIRST(&p->outs);
			abuf_eof(i);
		}
		p->flags |= APROC_ZOMB;
	}
	if (p->refs > 0) {
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": free delayed\n");
			p->ops = &zomb_ops;
		}
#endif
		return;
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": freed\n");
	}
#endif
	free(p);
}

void
aproc_setin(struct aproc *p, struct abuf *ibuf)
{
	LIST_INSERT_HEAD(&p->ins, ibuf, ient);
	ibuf->rproc = p;
	if (p->ops->newin)
		p->ops->newin(p, ibuf);
}

void
aproc_setout(struct aproc *p, struct abuf *obuf)
{
	LIST_INSERT_HEAD(&p->outs, obuf, oent);
	obuf->wproc = p;
	if (p->ops->newout)
		p->ops->newout(p, obuf);
}

void
aproc_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	struct abuf *obuf;

	LIST_FOREACH(obuf, &p->outs, oent) {
		abuf_ipos(obuf, delta);
	}
}

void
aproc_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf;

	LIST_FOREACH(ibuf, &p->ins, ient) {
		abuf_opos(ibuf, delta);
	}
}

int
aproc_inuse(struct aproc *p)
{
	struct abuf *i;

	LIST_FOREACH(i, &p->ins, ient) {
		if (i->inuse)
			return 1;
	}
	LIST_FOREACH(i, &p->outs, oent) {
		if (i->inuse)
			return 1;
	}
	return 0;
}

int
aproc_depend(struct aproc *p, struct aproc *dep)
{
	struct abuf *i;

	if (p == dep)
		return 1;
	LIST_FOREACH(i, &p->ins, ient) {
		if (i->wproc && aproc_depend(i->wproc, dep))
			return 1;
	}
	return 0;
}

int
rfile_do(struct aproc *p, unsigned todo, unsigned *done)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned n, count, off;

	off = p->u.io.partial;
	data = abuf_wgetblk(obuf, &count, 0);
	if (count > todo)
		count = todo;
	n = file_read(f, data + off, count * obuf->bpf - off);
	if (n == 0)
		return 0;
	n += off;
	p->u.io.partial = n % obuf->bpf;
	count = n / obuf->bpf;
	if (count > 0)
		abuf_wcommit(obuf, count);
	if (done)
		*done = count;
	return 1;
}

int
rfile_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	struct file *f = p->u.io.file;

	if (!ABUF_WOK(obuf) || !(f->state & FILE_ROK))
		return 0;
	if (!rfile_do(p, obuf->len, NULL))
		return 0;
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
rfile_out(struct aproc *p, struct abuf *obuf)
{
	struct file *f = p->u.io.file;

	if (f->state & FILE_RINUSE)
		return 0;
	if (!ABUF_WOK(obuf) || !(f->state & FILE_ROK))
		return 0;
	if (!rfile_do(p, obuf->len, NULL))
		return 0;
	return 1;
}

void
rfile_done(struct aproc *p)
{
	struct file *f = p->u.io.file;
	struct abuf *obuf;

	if (f == NULL)
		return;
	/*
	 * disconnect from file structure
	 */
	f->rproc = NULL;
	p->u.io.file = NULL;

	/*
	 * all buffers must be detached before deleting f->wproc,
	 * because otherwise it could trigger this code again
	 */
	obuf = LIST_FIRST(&p->outs);
	if (obuf)
		abuf_eof(obuf);
	if (f->wproc) {
		aproc_del(f->wproc);
	} else
		file_del(f);

#ifdef DEBUG
	if (debug_level >= 2 && p->u.io.partial > 0) {
		aproc_dbg(p);
		dbg_puts(": ");
		dbg_putu(p->u.io.partial);
		dbg_puts(" bytes lost in partial read\n");
	}
#endif
}

void
rfile_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	aproc_del(p);
}

void
rfile_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

struct aproc_ops rfile_ops = {
	"rfile",
	rfile_in,
	rfile_out,
	rfile_eof,
	rfile_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	rfile_done
};

struct aproc *
rfile_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&rfile_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;
	f->rproc = p;
	return p;
}

void
wfile_done(struct aproc *p)
{
	struct file *f = p->u.io.file;
	struct abuf *ibuf;

	if (f == NULL)
		return;
	/*
	 * disconnect from file structure
	 */
	f->wproc = NULL;
	p->u.io.file = NULL;

	/*
	 * all buffers must be detached before deleting f->rproc,
	 * because otherwise it could trigger this code again
	 */
	ibuf = LIST_FIRST(&p->ins);
	if (ibuf)
		abuf_hup(ibuf);
	if (f->rproc) {
		aproc_del(f->rproc);
	} else
		file_del(f);
#ifdef DEBUG
	if (debug_level >= 2 && p->u.io.partial > 0) {
		aproc_dbg(p);
		dbg_puts(": ");
		dbg_putu(p->u.io.partial);
		dbg_puts(" bytes lost in partial write\n");
	}
#endif
}

int
wfile_do(struct aproc *p, unsigned todo, unsigned *done)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	struct file *f = p->u.io.file;
	unsigned char *data;
	unsigned n, count, off;

	off = p->u.io.partial;
	data = abuf_rgetblk(ibuf, &count, 0);
	if (count > todo)
		count = todo;
	n = file_write(f, data + off, count * ibuf->bpf - off);
	if (n == 0)
		return 0;
	n += off;
	p->u.io.partial = n % ibuf->bpf;
	count = n / ibuf->bpf;
	if (count > 0)
		abuf_rdiscard(ibuf, count);
	if (done)
		*done = count;
	return 1;
}
int
wfile_in(struct aproc *p, struct abuf *ibuf)
{
	struct file *f = p->u.io.file;

	if (f->state & FILE_WINUSE)
		return 0;
	if (!ABUF_ROK(ibuf) || !(f->state & FILE_WOK))
		return 0;
	if (!wfile_do(p, ibuf->len, NULL))
		return 0;
	return 1;
}

int
wfile_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	struct file *f = p->u.io.file;

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_ROK(ibuf) || !(f->state & FILE_WOK))
		return 0;
	if (!wfile_do(p, ibuf->len, NULL))
		return 0;
	return 1;
}

void
wfile_eof(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
wfile_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	aproc_del(p);
}

struct aproc_ops wfile_ops = {
	"wfile",
	wfile_in,
	wfile_out,
	wfile_eof,
	wfile_hup,
	NULL, /* newin */
	NULL, /* newout */
	aproc_ipos,
	aproc_opos,
	wfile_done
};

struct aproc *
wfile_new(struct file *f)
{
	struct aproc *p;

	p = aproc_new(&wfile_ops, f->name);
	p->u.io.file = f;
	p->u.io.partial = 0;
	f->wproc = p;
	return p;
}

/*
 * Drop as much as possible samples from the reader end,
 * negative values mean ``insert silence''.
 */
void
mix_drop(struct abuf *buf, int extra)
{
	unsigned count;

	buf->r.mix.drop += extra;
	while (buf->r.mix.drop > 0) {
		count = buf->r.mix.drop;
		if (count > buf->used)
			count = buf->used;
		if (count == 0) {
#ifdef DEBUG
			if (debug_level >= 4) {
				abuf_dbg(buf);
				dbg_puts(": drop: no data\n");
			}
#endif
			return;
		}
		abuf_rdiscard(buf, count);
		buf->r.mix.drop -= count;
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(buf);
			dbg_puts(": dropped ");
			dbg_putu(count);
			dbg_puts(", to drop = ");
			dbg_putu(buf->r.mix.drop);
			dbg_puts("\n");
		}
#endif
	}
}

/*
 * Append the given amount of silence (or less if there's not enough
 * space), and crank w.mix.todo accordingly.
 */
void
mix_bzero(struct abuf *obuf)
{
	short *odata;
	unsigned ocount;

	odata = (short *)abuf_wgetblk(obuf, &ocount, obuf->w.mix.todo);
	if (ocount == 0)
		return;
	memset(odata, 0, ocount * obuf->bpf);
	obuf->w.mix.todo += ocount;
#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(obuf);
		dbg_puts(": bzero(");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts(")\n");
	}
#endif
}

/*
 * Mix an input block over an output block.
 */
unsigned
mix_badd(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned cmin, cmax;
	unsigned i, j, cc, istart, inext, onext, ostart;
	unsigned scount, icount, ocount;
	int vol;

#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(ibuf);
		dbg_puts(": badd: done = ");
		dbg_putu(ibuf->r.mix.done);
		dbg_puts("/");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts(", drop = ");
		dbg_puti(ibuf->r.mix.drop);
		dbg_puts("\n");
	}
#endif
	/*
	 * Insert silence for xrun correction
	 */
	if (ibuf->r.mix.drop < 0) {
		icount = -ibuf->r.mix.drop;
		ocount = obuf->len - obuf->used;
		if (ocount > obuf->w.mix.todo)
			ocount = obuf->w.mix.todo;
		scount = (icount < ocount) ? icount : ocount;
		ibuf->r.mix.done += scount;
		ibuf->r.mix.drop += scount;
	}

	/*
	 * Calculate the maximum we can read.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	if (icount == 0)
		return 0;

	/*
	 * Calculate the maximum we can write.
	 */
	odata = (short *)abuf_wgetblk(obuf, &ocount, ibuf->r.mix.done);
	if (ocount == 0)
		return 0;

	vol = (ibuf->r.mix.weight * ibuf->r.mix.vol) >> ADATA_SHIFT;
	cmin = obuf->cmin > ibuf->cmin ? obuf->cmin : ibuf->cmin;
	cmax = obuf->cmax < ibuf->cmax ? obuf->cmax : ibuf->cmax;
	ostart = cmin - obuf->cmin;
	istart = cmin - ibuf->cmin;
	onext = obuf->cmax - cmax + ostart;
	inext = ibuf->cmax - cmax + istart;
	cc = cmax - cmin + 1;
	odata += ostart;
	idata += istart;
	scount = (icount < ocount) ? icount : ocount;
	for (i = scount; i > 0; i--) {
		for (j = cc; j > 0; j--) {
			*odata += (*idata * vol) >> ADATA_SHIFT;
			idata++;
			odata++;
		}
		odata += onext;
		idata += inext;
	}
	abuf_rdiscard(ibuf, scount);
	ibuf->r.mix.done += scount;

#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(ibuf);
		dbg_puts(": badd: done = ");
		dbg_putu(ibuf->r.mix.done);
		dbg_puts("/");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts("\n");
	}
#endif
	return scount;
}

/*
 * Handle buffer underrun, return 0 if stream died.
 */
int
mix_xrun(struct aproc *p, struct abuf *i)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	unsigned fdrop, remain;

	if (i->r.mix.done > 0)
		return 1;
	if (i->r.mix.xrun == XRUN_ERROR) {
		abuf_hup(i);
		return 0;
	}
	fdrop = obuf->w.mix.todo;
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(i);
		dbg_puts(": underrun, dropping ");
		dbg_putu(fdrop);
		dbg_puts(" + ");
		dbg_putu(i->r.mix.drop);
		dbg_puts("\n");
	}
#endif
	i->r.mix.done += fdrop;
	if (i->r.mix.xrun == XRUN_SYNC)
		mix_drop(i, fdrop);
	else {
		remain = fdrop % p->u.mix.round;
		if (remain)
			remain = p->u.mix.round - remain;
		mix_drop(i, -(int)remain);
		fdrop += remain;
#ifdef DEBUG
		if (debug_level >= 3) {
			abuf_dbg(i);
			dbg_puts(": underrun, adding ");
			dbg_putu(remain);
			dbg_puts("\n");
		}
#endif
		abuf_opos(i, -(int)fdrop);
		if (i->duplex) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(i->duplex);
				dbg_puts(": full-duplex resync\n");
			}
#endif
			sub_silence(i->duplex, -(int)fdrop);
			abuf_ipos(i->duplex, -(int)fdrop);
		}
	}
	return 1;
}

int
mix_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext, *obuf = LIST_FIRST(&p->outs);
	unsigned odone;
	unsigned maxwrite;
	unsigned scount;

#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": used = ");
		dbg_putu(ibuf->used);
		dbg_puts("/");
		dbg_putu(ibuf->len);
		dbg_puts(", done = ");
		dbg_putu(ibuf->r.mix.done);
		dbg_puts("/");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts("\n");
	}
#endif
	if (!MIX_ROK(ibuf))
		return 0;
	mix_bzero(obuf);
	scount = 0;
	odone = obuf->w.mix.todo;
	for (i = LIST_FIRST(&p->ins); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (i->r.mix.drop >= 0 && !abuf_fill(i))
			continue; /* eof */
		mix_drop(i, 0);
		scount += mix_badd(i, obuf);
		if (odone > i->r.mix.done)
			odone = i->r.mix.done;
	}
	if (LIST_EMPTY(&p->ins) || scount == 0)
		return 0;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": maxwrite = ");
		dbg_putu(p->u.mix.maxlat);
		dbg_puts(" - ");
		dbg_putu(p->u.mix.lat);
		dbg_puts(" = ");
		dbg_putu(p->u.mix.maxlat - p->u.mix.lat);
		dbg_puts("\n");
	}
#endif
	maxwrite = p->u.mix.maxlat - p->u.mix.lat;
	if (maxwrite > 0) {
		if (odone > maxwrite)
			odone = maxwrite;
		p->u.mix.lat += odone;
		p->u.mix.abspos += odone;
		LIST_FOREACH(i, &p->ins, ient) {
			i->r.mix.done -= odone;
		}
		abuf_wcommit(obuf, odone);
		obuf->w.mix.todo -= odone;
		if (APROC_OK(p->u.mix.mon))
			mon_snoop(p->u.mix.mon, obuf, obuf->used - odone, odone);
		if (!abuf_flush(obuf))
			return 0; /* hup */
	}
	return 1;
}

int
mix_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *inext;
	unsigned odone;
	unsigned maxwrite;
	unsigned scount;

#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": used = ");
		dbg_putu(obuf->used);
		dbg_puts("/");
		dbg_putu(obuf->len);
		dbg_puts(", todo = ");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts("/");
		dbg_putu(obuf->len);
		dbg_puts("\n");
	}
#endif
	if (!ABUF_WOK(obuf))
		return 0;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": maxwrite = ");
		dbg_putu(p->u.mix.maxlat);
		dbg_puts(" - ");
		dbg_putu(p->u.mix.lat);
		dbg_puts(" = ");
		dbg_putu(p->u.mix.maxlat - p->u.mix.lat);
		dbg_puts("\n");
	}
#endif
	maxwrite = p->u.mix.maxlat - p->u.mix.lat;
	mix_bzero(obuf);
	scount = 0;
	odone = obuf->len;
	for (i = LIST_FIRST(&p->ins); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (i->r.mix.drop >= 0 && !abuf_fill(i))
			continue; /* eof */
		mix_drop(i, 0);
		if (maxwrite > 0 && !MIX_ROK(i)) {
			if (p->flags & APROC_DROP) {
				if (!mix_xrun(p, i))
					continue;
			}
		} else
			scount += mix_badd(i, obuf);
		if (odone > i->r.mix.done)
			odone = i->r.mix.done;
	}
	if (LIST_EMPTY(&p->ins)) {
		if (p->flags & APROC_QUIT) {
			aproc_del(p);
			return 0;
		}
		if (!(p->flags & APROC_DROP))
			return 0;
		odone = obuf->w.mix.todo;
		p->u.mix.idle += odone;
	}
	if (maxwrite > 0) {
		if (odone > maxwrite)
			odone = maxwrite;
		p->u.mix.lat += odone;
		p->u.mix.abspos += odone;
		LIST_FOREACH(i, &p->ins, ient) {
			i->r.mix.done -= odone;
		}
		abuf_wcommit(obuf, odone);
		obuf->w.mix.todo -= odone;
		if (APROC_OK(p->u.mix.mon))
			mon_snoop(p->u.mix.mon, obuf, obuf->used - odone, odone);
	}
	if (scount == 0)
		return 0;
	return 1;
}

void
mix_eof(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext, *obuf = LIST_FIRST(&p->outs);
	unsigned odone;

	mix_setmaster(p);

	if (!aproc_inuse(p)) {
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": running other streams\n");
		}
#endif
		/*
		 * Find a blocked input.
		 */
		odone = obuf->len;
		for (i = LIST_FIRST(&p->ins); i != NULL; i = inext) {
			inext = LIST_NEXT(i, ient);
			if (!abuf_fill(i))
				continue;
			if (MIX_ROK(i) && i->r.mix.done < obuf->w.mix.todo) {
				abuf_run(i);
				return;
			}
			if (odone > i->r.mix.done)
				odone = i->r.mix.done;
		}
		/*
		 * No blocked inputs. Check if output is blocked.
		 */
		if (LIST_EMPTY(&p->ins) || odone == obuf->w.mix.todo)
			abuf_run(obuf);
	}
}

void
mix_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

void
mix_newin(struct aproc *p, struct abuf *ibuf)
{
	p->u.mix.idle = 0;
	ibuf->r.mix.done = 0;
	ibuf->r.mix.vol = ADATA_UNIT;
	ibuf->r.mix.weight = ADATA_UNIT;
	ibuf->r.mix.maxweight = ADATA_UNIT;
	ibuf->r.mix.xrun = XRUN_IGNORE;
	ibuf->r.mix.drop = 0;
}

void
mix_newout(struct aproc *p, struct abuf *obuf)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": newin, will use ");
		dbg_putu(obuf->len);
		dbg_puts("\n");
	}
#endif
	obuf->w.mix.todo = 0;
}

void
mix_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	p->u.mix.lat -= delta;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": opos: lat = ");
		dbg_puti(p->u.mix.lat);
		dbg_puts("/");
		dbg_puti(p->u.mix.maxlat);
		dbg_puts("\n");
	}
#endif
	if (APROC_OK(p->u.mix.ctl))
		ctl_ontick(p->u.mix.ctl, delta);
	aproc_opos(p, obuf, delta);
	if (APROC_OK(p->u.mix.mon))
		p->u.mix.mon->ops->ipos(p->u.mix.mon, NULL, delta);
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
mix_new(char *name, int maxlat, unsigned round, struct aproc *ctl)
{
	struct aproc *p;

	p = aproc_new(&mix_ops, name);
	p->u.mix.idle = 0;
	p->u.mix.lat = 0;
	p->u.mix.round = round;
	p->u.mix.maxlat = maxlat;
	p->u.mix.abspos = 0;
	p->u.mix.ctl = ctl;
	return p;
}

/*
 * Normalize input levels.
 */
void
mix_setmaster(struct aproc *p)
{
	unsigned n;
	struct abuf *i, *j;
	int weight;

	/*
	 * count the number of inputs. If a set of inputs
	 * uses channels that have no intersection, they are 
	 * counted only once because they don't need to 
	 * share their volume
	 */
	n = 0;
	LIST_FOREACH(i, &p->ins, ient) {
		j = LIST_NEXT(i, ient);
		for (;;) {
			if (j == NULL) {
				n++;
				break;
			}
			if (i->cmin > j->cmax || i->cmax < j->cmin)
				break;
			j = LIST_NEXT(j, ient);
		}
	}
	LIST_FOREACH(i, &p->ins, ient) {
		weight = ADATA_UNIT / n;
		if (weight > i->r.mix.maxweight)
			weight = i->r.mix.maxweight;
		i->r.mix.weight = weight;
#ifdef DEBUG
		if (debug_level >= 3) {
			abuf_dbg(i);
			dbg_puts(": setmaster: ");
			dbg_puti(i->r.mix.weight);
			dbg_puts("/");
			dbg_puti(i->r.mix.maxweight);
			dbg_puts("\n");
		}
#endif
	}
}

void
mix_clear(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);

	p->u.mix.lat = 0;
	p->u.mix.abspos = 0;
	obuf->w.mix.todo = 0;
}

void
mix_prime(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	unsigned todo, count;

	for (;;) {
		if (!ABUF_WOK(obuf))
			break;
		todo = p->u.mix.maxlat - p->u.mix.lat;
		if (todo == 0)
			break;
		mix_bzero(obuf);
		count = obuf->w.mix.todo;
		if (count > todo)
			count = todo;
		obuf->w.mix.todo -= count;
		p->u.mix.lat += count;
		p->u.mix.abspos += count;
		abuf_wcommit(obuf, count);
		if (APROC_OK(p->u.mix.mon))
			mon_snoop(p->u.mix.mon, obuf, 0, count);
		abuf_flush(obuf);
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": prime: lat/maxlat=");
		dbg_puti(p->u.mix.lat);
		dbg_puts("/");
		dbg_puti(p->u.mix.maxlat);
		dbg_puts("\n");
	}
#endif
}

/*
 * Append as much as possible silence on the writer end
 */
void
sub_silence(struct abuf *buf, int extra)
{
	unsigned char *data;
	unsigned count;

	buf->w.sub.silence += extra;
	if (buf->w.sub.silence > 0) {
		data = abuf_wgetblk(buf, &count, 0);
		if (count >= buf->w.sub.silence)
			count = buf->w.sub.silence;
		if (count == 0) {
#ifdef DEBUG
			if (debug_level >= 4) {
				abuf_dbg(buf);
				dbg_puts(": no space for silence\n");
			}
#endif
			return;
		}
		memset(data, 0, count * buf->bpf);
		abuf_wcommit(buf, count);
		buf->w.sub.silence -= count;
#ifdef DEBUG
		if (debug_level >= 4) {
			abuf_dbg(buf);
			dbg_puts(": appended ");
			dbg_putu(count);
			dbg_puts(", remaining silence = ");
			dbg_putu(buf->w.sub.silence);
			dbg_puts("\n");
		}
#endif
	}
}

/*
 * Copy data from ibuf to obuf.
 */
void
sub_bcopy(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned cmin, cmax;
	unsigned i, j, cc, istart, inext, onext, ostart;
	unsigned icount, ocount, scount;

	/*
	 * Drop samples for xrun correction
	 */
	if (obuf->w.sub.silence < 0) {
		scount = -obuf->w.sub.silence;
		if (scount > ibuf->used)
			scount = ibuf->used;
		obuf->w.sub.done += scount;
		obuf->w.sub.silence += scount;
	}

	idata = (short *)abuf_rgetblk(ibuf, &icount, obuf->w.sub.done);
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	if (ocount == 0)
		return;
	cmin = obuf->cmin > ibuf->cmin ? obuf->cmin : ibuf->cmin;
	cmax = obuf->cmax < ibuf->cmax ? obuf->cmax : ibuf->cmax;
	ostart = cmin - obuf->cmin;
	istart = cmin - ibuf->cmin;
	onext = obuf->cmax - cmax;
	inext = ibuf->cmax - cmax + istart;
	cc = cmax - cmin + 1;
	idata += istart;
	scount = (icount < ocount) ? icount : ocount;
	for (i = scount; i > 0; i--) {
		for (j = ostart; j > 0; j--)
			*odata++ = 0x1111;
		for (j = cc; j > 0; j--) {
			*odata = *idata;
			odata++;
			idata++;
		}
		for (j = onext; j > 0; j--)
			*odata++ = 0x2222;
		idata += inext;
	}
	abuf_wcommit(obuf, scount);
	obuf->w.sub.done += scount;
#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(obuf);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts("\n");
	}
#endif
}

/*
 * Handle buffer overruns. Return 0 if the stream died.
 */
int
sub_xrun(struct aproc *p, struct abuf *i)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	unsigned fdrop, remain;

	if (i->w.sub.done > 0)
		return 1;
	if (i->w.sub.xrun == XRUN_ERROR) {
		abuf_eof(i);
		return 0;
	}
	fdrop = ibuf->used;
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(i);
		dbg_puts(": overrun, silence ");
		dbg_putu(fdrop);
		dbg_puts(" + ");
		dbg_putu(i->w.sub.silence);
		dbg_puts("\n");
	}
#endif
	i->w.sub.done += fdrop;
	if (i->w.sub.xrun == XRUN_SYNC)
		sub_silence(i, fdrop);
	else {
		remain = fdrop % p->u.sub.round;
		if (remain)
			remain = p->u.sub.round - remain;
		sub_silence(i, -(int)remain);
		fdrop += remain;
#ifdef DEBUG
		if (debug_level >= 3) {
			abuf_dbg(i);
			dbg_puts(": overrun, adding ");
			dbg_putu(remain);
			dbg_puts("\n");
		}
#endif

		abuf_ipos(i, -(int)fdrop);
		if (i->duplex) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(i->duplex);
				dbg_puts(": full-duplex resync\n");
			}
#endif
			mix_drop(i->duplex, -(int)fdrop);
			abuf_opos(i->duplex, -(int)fdrop);
		}
	}
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
	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		sub_silence(i, 0);
		if (!SUB_WOK(i)) {
			if (p->flags & APROC_DROP) {
				if (!sub_xrun(p, i))
					continue;
			}
		} else
			sub_bcopy(ibuf, i);
		if (idone > i->w.sub.done)
			idone = i->w.sub.done;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->outs)) {
		if (p->flags & APROC_QUIT) {
			aproc_del(p);
			return 0;
		}
		if (!(p->flags & APROC_DROP))
			return 0;
		idone = ibuf->used;
		p->u.sub.idle += idone;
	}
	if (idone == 0)
		return 0;
	LIST_FOREACH(i, &p->outs, oent) {
		i->w.sub.done -= idone;
	}
	abuf_rdiscard(ibuf, idone);
	abuf_opos(ibuf, idone);
	p->u.sub.lat -= idone;
	p->u.sub.abspos += idone;
	return 1;
}

int
sub_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	struct abuf *i, *inext;
	unsigned idone;

	if (!SUB_WOK(obuf))
		return 0;
	if (!abuf_fill(ibuf))
		return 0; /* eof */
	idone = ibuf->len;
	for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
		inext = LIST_NEXT(i, oent);
		sub_silence(i, 0);
		sub_bcopy(ibuf, i);
		if (idone > i->w.sub.done)
			idone = i->w.sub.done;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->outs) || idone == 0)
		return 0;
	LIST_FOREACH(i, &p->outs, oent) {
		i->w.sub.done -= idone;
	}
	abuf_rdiscard(ibuf, idone);
	abuf_opos(ibuf, idone);
	p->u.sub.lat -= idone;
	p->u.sub.abspos += idone;
	return 1;
}

void
sub_eof(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
sub_hup(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *inext, *ibuf = LIST_FIRST(&p->ins);
	unsigned idone;

	if (!aproc_inuse(p)) {
#ifdef DEBUG
		if (debug_level >= 3) {
			aproc_dbg(p);
			dbg_puts(": running other streams\n");
		}
#endif
		/*
		 * Find a blocked output.
		 */
		idone = ibuf->len;
		for (i = LIST_FIRST(&p->outs); i != NULL; i = inext) {
			inext = LIST_NEXT(i, oent);
			if (!abuf_flush(i))
				continue;
			if (SUB_WOK(i) && i->w.sub.done < ibuf->used) {
				abuf_run(i);
				return;
			}
			if (idone > i->w.sub.done)
				idone = i->w.sub.done;
		}
		/*
		 * No blocked outputs. Check if input is blocked.
		 */
		if (LIST_EMPTY(&p->outs) || idone == ibuf->used)
			abuf_run(ibuf);
	}
}

void
sub_newout(struct aproc *p, struct abuf *obuf)
{
	p->u.sub.idle = 0;
	obuf->w.sub.done = 0;
	obuf->w.sub.xrun = XRUN_IGNORE;
	obuf->w.sub.silence = 0;
}

void
sub_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	p->u.sub.lat += delta;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": ipos: lat = ");
		dbg_puti(p->u.sub.lat);
		dbg_puts("/");
		dbg_puti(p->u.sub.maxlat);
		dbg_puts("\n");
	}
#endif
	if (APROC_OK(p->u.sub.ctl))
		ctl_ontick(p->u.sub.ctl, delta);
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
sub_new(char *name, int maxlat, unsigned round, struct aproc *ctl)
{
	struct aproc *p;

	p = aproc_new(&sub_ops, name);
	p->u.sub.idle = 0;
	p->u.sub.lat = 0;
	p->u.sub.round = round;
	p->u.sub.maxlat = maxlat;
	p->u.sub.abspos = 0;
	p->u.sub.ctl = ctl;
	return p;
}

void
sub_clear(struct aproc *p)
{
	p->u.sub.lat = 0;
	p->u.sub.abspos = 0;
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
	ifr = icount;

	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ofr = ocount;

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
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": resamp starting diff = ");
		dbg_puti(diff);
		dbg_puts(", ifr = ");
		dbg_putu(ifr);
		dbg_puts(", ofr = ");
		dbg_putu(ofr);
		dbg_puts(" fr\n");
	}
#endif
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
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": resamp done delta = ");
		dbg_puti(diff);
		dbg_puts(", ifr = ");
		dbg_putu(ifr);
		dbg_puts(", ofr = ");
		dbg_putu(ofr);
		dbg_puts(" fr\n");
	}
#endif
	/*
	 * Update FIFO pointers.
	 */
	icount -= ifr;
	ocount -= ofr;
	abuf_rdiscard(ibuf, icount);
	abuf_wcommit(obuf, ocount);
}

int
resamp_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);

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
	struct abuf *ibuf = LIST_FIRST(&p->ins);

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
	aproc_del(p);
}

void
resamp_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

void
resamp_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	long long ipos;
	
	ipos = (long long)delta * p->u.resamp.oblksz + p->u.resamp.idelta;
	p->u.resamp.idelta = ipos % p->u.resamp.iblksz;
	abuf_ipos(obuf, ipos / (int)p->u.resamp.iblksz);
}

void
resamp_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	long long opos;

	opos = (long long)delta * p->u.resamp.iblksz + p->u.resamp.odelta;
	p->u.resamp.odelta = opos % p->u.resamp.oblksz;
	abuf_opos(ibuf, opos / p->u.resamp.oblksz);
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
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new ");
		dbg_putu(iblksz);
		dbg_puts("/");
		dbg_putu(oblksz);
		dbg_puts("\n");
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
	if (icount == 0)
		return;
	odata = abuf_wgetblk(obuf, &ocount, 0);
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = ibuf->cmax - ibuf->cmin + 1;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr / ");
		dbg_putu(nch);
		dbg_puts(" ch\n");
	}
#endif
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
	abuf_rdiscard(ibuf, scount);
	abuf_wcommit(obuf, scount);
}

int
enc_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);

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
	struct abuf *ibuf = LIST_FIRST(&p->ins);

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
	aproc_del(p);
}

void
enc_hup(struct aproc *p, struct abuf *obuf)
{
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
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new ");
		aparams_dbg(par);
		dbg_puts("\n");
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
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = obuf->cmax - obuf->cmin + 1;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr / ");
		dbg_putu(nch);
		dbg_puts(" ch\n");
	}
#endif
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
	abuf_rdiscard(ibuf, scount);
	abuf_wcommit(obuf, scount);
}

int
dec_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);

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
	struct abuf *ibuf = LIST_FIRST(&p->ins);

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
	aproc_del(p);
}

void
dec_hup(struct aproc *p, struct abuf *obuf)
{
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
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new ");
		aparams_dbg(par);
		dbg_puts("\n");
	}
#endif
	return p;
}

/*
 * Convert one block.
 */
void
join_bcopy(struct aproc *p, struct abuf *ibuf, struct abuf *obuf)
{
	unsigned h, hops;
	unsigned inch, inext;
	short *idata;
	unsigned onch, onext;
	short *odata;
	int scale;
	unsigned c, f, scount, icount, ocount;

	/*
	 * Calculate max frames readable at once from the input buffer.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	if (ocount == 0)
		return;
	scount = icount < ocount ? icount : ocount;
	inch = ibuf->cmax - ibuf->cmin + 1;
	onch = obuf->cmax - obuf->cmin + 1;
	if (2 * inch <= onch) {
		hops = onch / inch;
		inext = inch * hops;
		onext = onch - inext;
		for (f = scount; f > 0; f--) {
			h = hops;
			for (;;) {
				for (c = inch; c > 0; c--)
					*odata++ = *idata++;
				if (--h == 0)
					break;
				idata -= inch;
			}
			for (c = onext; c > 0; c--)
				*odata++ = 0;
		}
	} else if (inch >= 2 * onch) {
		hops = inch / onch;
		inext = inch - onch * hops;
		scale = ADATA_UNIT / hops;
		inch -= onch + inext;
		hops--;
		for (f = scount; f > 0; f--) {
			for (c = onch; c > 0; c--)
				*odata++ = (*idata++ * scale)
				    >> ADATA_SHIFT;
			for (h = hops; h > 0; h--) {
				odata -= onch;
				for (c = onch; c > 0; c--)
					*odata++ += (*idata++ * scale)
					    >> ADATA_SHIFT;
			}
			idata += inext;
		}
	} else {
#ifdef DEBUG
		aproc_dbg(p);
		dbg_puts(": nothing to do\n");
		dbg_panic();
#endif
	}
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr\n");
	}
#endif
	abuf_rdiscard(ibuf, scount);
	abuf_wcommit(obuf, scount);
}

int
join_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);

	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	join_bcopy(p, ibuf, obuf);
	if (!abuf_flush(obuf))
		return 0;
	return 1;
}

int
join_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);

	if (!abuf_fill(ibuf))
		return 0;
	if (!ABUF_WOK(obuf) || !ABUF_ROK(ibuf))
		return 0;
	join_bcopy(p, ibuf, obuf);
	return 1;
}

void
join_eof(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
join_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

struct aproc_ops join_ops = {
	"join",
	join_in,
	join_out,
	join_eof,
	join_hup,
	NULL,
	NULL,
	aproc_ipos,
	aproc_opos,
	NULL
};

struct aproc *
join_new(char *name)
{
	struct aproc *p;

	p = aproc_new(&join_ops, name);
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts("\n");
	}
#endif
	return p;
}

/*
 * Commit and flush part of the output buffer
 */
void
mon_flush(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	unsigned count;

#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": delta = ");
		dbg_puti(p->u.mon.delta);
		dbg_puts("/");
		dbg_putu(p->u.mon.bufsz);
		dbg_puts(" pending = ");
		dbg_puti(p->u.mon.pending);
		dbg_puts("\n");
	}
#endif
	if (p->u.mon.delta <= 0 || p->u.mon.pending == 0)
		return;
	count = p->u.mon.delta;
	if (count > p->u.mon.pending)
		count = p->u.mon.pending;
	abuf_wcommit(obuf, count);
	p->u.mon.pending -= count;
	p->u.mon.delta -= count;
	abuf_flush(obuf);
}

/*
 * Copy one block.
 */
void
mon_snoop(struct aproc *p, struct abuf *ibuf, unsigned pos, unsigned todo)
{
	struct abuf *obuf = LIST_FIRST(&p->outs);
	unsigned scount, icount, ocount;
	short *idata, *odata;

#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": snoop ");
		dbg_putu(pos);
		dbg_puts("..");
		dbg_putu(todo);
		dbg_puts("\n");
	}
#endif
	if (!abuf_flush(obuf))
		return;

	while (todo > 0) {
		/*
		 * Calculate max frames readable at once from the input buffer.
		 */
		idata = (short *)abuf_rgetblk(ibuf, &icount, pos);
		odata = (short *)abuf_wgetblk(obuf, &ocount, p->u.mon.pending);
		scount = (icount < ocount) ? icount : ocount;
#ifdef DEBUG
		if (debug_level >= 4) {
			aproc_dbg(p);
			dbg_puts(": snooping ");
			dbg_putu(scount);
			dbg_puts(" fr\n");
		}
		if (scount == 0) {
			dbg_puts("monitor xrun, not allowed\n");
			dbg_panic();
		}
#endif
		memcpy(odata, idata, scount * obuf->bpf);
		p->u.mon.pending += scount;
		todo -= scount;
		pos += scount;
	}
	mon_flush(p);
}

int
mon_in(struct aproc *p, struct abuf *ibuf)
{
#ifdef DEBUG
	dbg_puts("monitor can't have inputs to read\n");
	dbg_panic();
#endif
	return 0;
}

/*
 * put the monitor into ``empty'' state
 */
void
mon_clear(struct aproc *p)
{
	p->u.mon.pending = 0;
	p->u.mon.delta = 0;
}

int
mon_out(struct aproc *p, struct abuf *obuf)
{
	/*
	 * can't trigger monitored stream to produce data
	 */
	return 0;
}

void
mon_eof(struct aproc *p, struct abuf *ibuf)
{
#ifdef DEBUG
	dbg_puts("monitor can't have inputs to eof\n");
	dbg_panic();
#endif
}

void
mon_hup(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

void
mon_ipos(struct aproc *p, struct abuf *ibuf, int delta)
{
	aproc_ipos(p, ibuf, delta);
	p->u.mon.delta += delta;
	mon_flush(p);
}

struct aproc_ops mon_ops = {
	"mon",
	mon_in,
	mon_out,
	mon_eof,
	mon_hup,
	NULL,
	NULL,
	mon_ipos,
	aproc_opos,
	NULL
};

struct aproc *
mon_new(char *name, unsigned bufsz)
{
	struct aproc *p;

	p = aproc_new(&mon_ops, name);
	p->u.mon.pending = 0;
	p->u.mon.delta = 0;
	p->u.mon.bufsz = bufsz;
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new\n");
	}
#endif
	return p;
}
