/*	$OpenBSD: aproc.c,v 1.46 2010/01/15 22:17:10 ratchov Exp $	*/
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
	LIST_INIT(&p->ibuflist);
	LIST_INIT(&p->obuflist);
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
		while (!LIST_EMPTY(&p->ibuflist)) {
			i = LIST_FIRST(&p->ibuflist);
			abuf_hup(i);
		}
		while (!LIST_EMPTY(&p->obuflist)) {
			i = LIST_FIRST(&p->obuflist);
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

	LIST_FOREACH(obuf, &p->obuflist, oent) {
		abuf_ipos(obuf, delta);
	}
}

void
aproc_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf;

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
rfile_in(struct aproc *p, struct abuf *ibuf_dummy)
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
rfile_out(struct aproc *p, struct abuf *obuf)
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
rfile_done(struct aproc *p)
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
wfile_in(struct aproc *p, struct abuf *ibuf)
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
wfile_out(struct aproc *p, struct abuf *obuf_dummy)
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
	f->wproc = p;
	return p;
}

/*
 * Append the given amount of silence (or less if there's not enough
 * space), and crank mixitodo accordingly.
 */
void
mix_bzero(struct abuf *obuf, unsigned zcount)
{
	short *odata;
	unsigned ocount;

#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(obuf);
		dbg_puts(": bzero(");
		dbg_putu(zcount);
		dbg_puts(")\n");
	}
#endif
	odata = (short *)abuf_wgetblk(obuf, &ocount, obuf->w.mix.todo);
	ocount -= ocount % obuf->bpf;
	if (ocount > zcount)
		ocount = zcount;
	memset(odata, 0, ocount);
	obuf->w.mix.todo += ocount;
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
	/*
	 * Calculate the maximum we can read.
	 */
	idata = (short *)abuf_rgetblk(ibuf, &icount, 0);
	icount /= ibuf->bpf;
	if (icount == 0)
		return;

	/*
	 * Zero-fill if necessary.
	 */
	zcount = ibuf->r.mix.done + icount * obuf->bpf;
	if (zcount > obuf->w.mix.todo)
		mix_bzero(obuf, zcount - obuf->w.mix.todo);

	/*
	 * Calculate the maximum we can write.
	 */
	odata = (short *)abuf_wgetblk(obuf, &ocount, ibuf->r.mix.done);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;

	vol = (ibuf->r.mix.weight * ibuf->r.mix.vol) >> ADATA_SHIFT;
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
	ibuf->r.mix.done += scount * obuf->bpf;

#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(ibuf);
		dbg_puts(": badd: done = ");
		dbg_putu(scount);
		dbg_puts(", todo = ");
		dbg_putu(ibuf->r.mix.done);
		dbg_puts("/");
		dbg_putu(obuf->w.mix.todo);
		dbg_puts("\n");
	}
#endif
}

/*
 * Handle buffer underrun, return 0 if stream died.
 */
int
mix_xrun(struct abuf *i, struct abuf *obuf)
{
	unsigned fdrop;

	if (i->r.mix.done > 0)
		return 1;
	if (i->r.mix.xrun == XRUN_ERROR) {
		abuf_hup(i);
		return 0;
	}
	mix_bzero(obuf, obuf->len);
	fdrop = obuf->w.mix.todo / obuf->bpf;
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(i);
		dbg_puts(": underrun, dropping ");
		dbg_putu(fdrop);
		dbg_puts(" + ");
		dbg_putu(i->drop / i->bpf);
		dbg_puts("\n");
	}
#endif
	i->r.mix.done += fdrop * obuf->bpf;
	if (i->r.mix.xrun == XRUN_SYNC)
		i->drop += fdrop * i->bpf;
	else {
		abuf_opos(i, -(int)fdrop);
		if (i->duplex) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(i->duplex);
				dbg_puts(": full-duplex resync\n");
			}
#endif
			i->duplex->drop += fdrop * i->duplex->bpf;
			abuf_ipos(i->duplex, -(int)fdrop);
		}
	}
	return 1;
}

int
mix_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *inext, *obuf = LIST_FIRST(&p->obuflist);
	unsigned odone;

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
	if (!ABUF_ROK(ibuf))
		return 0;
	odone = obuf->len;
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (!abuf_fill(i))
			continue; /* eof */
		mix_badd(i, obuf);
		if (odone > i->r.mix.done)
			odone = i->r.mix.done;
	}
	if (LIST_EMPTY(&p->ibuflist) || odone == 0)
		return 0;
	p->u.mix.lat += odone / obuf->bpf;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		i->r.mix.done -= odone;
	}
	abuf_wcommit(obuf, odone);
	obuf->w.mix.todo -= odone;
	if (!abuf_flush(obuf))
		return 0; /* hup */
	return 1;
}

int
mix_out(struct aproc *p, struct abuf *obuf)
{
	struct abuf *i, *inext;
	unsigned odone;

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
	odone = obuf->len;
	for (i = LIST_FIRST(&p->ibuflist); i != NULL; i = inext) {
		inext = LIST_NEXT(i, ient);
		if (!abuf_fill(i))
			continue; /* eof */
		if (!ABUF_ROK(i)) {
			if (p->flags & APROC_DROP) {
				if (!mix_xrun(i, obuf))
					continue;
			}
		} else
			mix_badd(i, obuf);
		if (odone > i->r.mix.done)
			odone = i->r.mix.done;
	}
	if (LIST_EMPTY(&p->ibuflist)) {
		if (p->flags & APROC_QUIT) {
			aproc_del(p);
			return 0;
		}
		if (!(p->flags & APROC_DROP))
			return 0;
		mix_bzero(obuf, obuf->len);
		odone = obuf->w.mix.todo;
		p->u.mix.idle += odone / obuf->bpf;
	}
	if (odone == 0)
		return 0;
	p->u.mix.lat += odone / obuf->bpf;
	LIST_FOREACH(i, &p->ibuflist, ient) {
		i->r.mix.done -= odone;
	}
	abuf_wcommit(obuf, odone);
	obuf->w.mix.todo -= odone;
	return 1;
}

void
mix_eof(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *i, *obuf = LIST_FIRST(&p->obuflist);
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
		LIST_FOREACH(i, &p->ibuflist, ient) {
			if (ABUF_ROK(i) && i->r.mix.done < obuf->w.mix.todo) {
				abuf_run(i);
				return;
			}
			if (odone > i->r.mix.done)
				odone = i->r.mix.done;
		}
		/*
		 * No blocked inputs. Check if output is blocked.
		 */
		if (LIST_EMPTY(&p->ibuflist) || odone == obuf->w.mix.todo)
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
#ifdef DEBUG
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	if (!obuf || ibuf->cmin < obuf->cmin || ibuf->cmax > obuf->cmax) {
		dbg_puts("newin: channel ranges mismatch\n");
		dbg_panic();
	}
#endif
	p->u.mix.idle = 0;
	ibuf->r.mix.done = 0;
	ibuf->r.mix.vol = ADATA_UNIT;
	ibuf->r.mix.weight = ADATA_UNIT;
	ibuf->r.mix.maxweight = ADATA_UNIT;
	ibuf->r.mix.xrun = XRUN_IGNORE;
}

void
mix_newout(struct aproc *p, struct abuf *obuf)
{
#ifdef DEBUG
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": newin, will use ");
		dbg_putu(obuf->len / obuf->bpf);
		dbg_puts(" fr\n");
	}
#endif
	obuf->w.mix.todo = 0;
}

void
mix_opos(struct aproc *p, struct abuf *obuf, int delta)
{
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": opos: lat = ");
		dbg_puti(p->u.mix.lat);
		dbg_puts("/");
		dbg_puti(p->u.mix.maxlat);
		dbg_puts(" fr\n");
	}
#endif
	p->u.mix.lat -= delta;
	if (p->u.mix.ctl)
		ctl_ontick(p->u.mix.ctl, delta);
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
mix_new(char *name, int maxlat, struct aproc *ctl)
{
	struct aproc *p;

	p = aproc_new(&mix_ops, name);
	p->u.mix.idle = 0;
	p->u.mix.lat = 0;
	p->u.mix.maxlat = maxlat;
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
	LIST_FOREACH(i, &p->ibuflist, ient) {
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
	LIST_FOREACH(i, &p->ibuflist, ient) {
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
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

	p->u.mix.lat = 0;
	obuf->w.mix.todo = 0;
}

void
mix_prime(struct aproc *p)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	unsigned todo, count;

	for (;;) {
		if (!ABUF_WOK(obuf))
			break;
		todo = (p->u.mix.maxlat - p->u.mix.lat) * obuf->bpf;
		if (todo == 0)
			break;
		mix_bzero(obuf, obuf->len);
		count = obuf->w.mix.todo;
		if (count > todo)
			count = todo;
		obuf->w.mix.todo -= count;
		p->u.mix.lat += count / obuf->bpf;
		abuf_wcommit(obuf, count);
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
 * Copy data from ibuf to obuf.
 */
void
sub_bcopy(struct abuf *ibuf, struct abuf *obuf)
{
	short *idata, *odata;
	unsigned i, j, ocnt, inext, istart;
	unsigned icount, ocount, scount;

	idata = (short *)abuf_rgetblk(ibuf, &icount, obuf->w.sub.done);
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
	obuf->w.sub.done += scount * ibuf->bpf;
#ifdef DEBUG
	if (debug_level >= 4) {
		abuf_dbg(obuf);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr\n");
	}
#endif
}

/*
 * Handle buffer overruns. Return 0 if the stream died.
 */
int
sub_xrun(struct abuf *ibuf, struct abuf *i)
{
	unsigned fdrop;

	if (i->w.sub.done > 0)
		return 1;
	if (i->w.sub.xrun == XRUN_ERROR) {
		abuf_eof(i);
		return 0;
	}
	fdrop = ibuf->used / ibuf->bpf;
#ifdef DEBUG
	if (debug_level >= 3) {
		abuf_dbg(i);
		dbg_puts(": overrun, silence ");
		dbg_putu(fdrop);
		dbg_puts(" + ");
		dbg_putu(i->silence / i->bpf);
		dbg_puts("\n");
	}
#endif
	if (i->w.sub.xrun == XRUN_SYNC)
		i->silence += fdrop * i->bpf;
	else {
		abuf_ipos(i, -(int)fdrop);
		if (i->duplex) {
#ifdef DEBUG
			if (debug_level >= 3) {
				abuf_dbg(i->duplex);
				dbg_puts(": full-duplex resync\n");
			}
#endif
			i->duplex->silence += fdrop * i->duplex->bpf;
			abuf_opos(i->duplex, -(int)fdrop);
		}
	}
	i->w.sub.done += fdrop * ibuf->bpf;
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
			if (p->flags & APROC_DROP) {
				if (!sub_xrun(ibuf, i))
					continue;
			}
		} else
			sub_bcopy(ibuf, i);
		if (idone > i->w.sub.done)
			idone = i->w.sub.done;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->obuflist)) {
		if (p->flags & APROC_QUIT) {
			aproc_del(p);
			return 0;
		}
		if (!(p->flags & APROC_DROP))
			return 0;
		idone = ibuf->used;
		p->u.sub.idle += idone / ibuf->bpf;
	}
	if (idone == 0)
		return 0;
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->w.sub.done -= idone;
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
		if (idone > i->w.sub.done)
			idone = i->w.sub.done;
		if (!abuf_flush(i))
			continue;
	}
	if (LIST_EMPTY(&p->obuflist) || idone == 0)
		return 0;
	LIST_FOREACH(i, &p->obuflist, oent) {
		i->w.sub.done -= idone;
	}
	abuf_rdiscard(ibuf, idone);
	p->u.sub.lat -= idone / ibuf->bpf;
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
	struct abuf *i, *ibuf = LIST_FIRST(&p->ibuflist);
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
		LIST_FOREACH(i, &p->obuflist, oent) {
			if (ABUF_WOK(i) && i->w.sub.done < ibuf->used) {
				abuf_run(i);
				return;
			}
			if (idone > i->w.sub.done)
				idone = i->w.sub.done;
		}
		/*
		 * No blocked outputs. Check if input is blocked.
		 */
		if (LIST_EMPTY(&p->obuflist) || idone == ibuf->used)
			abuf_run(ibuf);
	}
}

void
sub_newout(struct aproc *p, struct abuf *obuf)
{
#ifdef DEBUG
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);

	if (!ibuf || obuf->cmin < ibuf->cmin || obuf->cmax > ibuf->cmax) {
		dbg_puts("newout: channel ranges mismatch\n");
		dbg_panic();
	}
#endif
	p->u.sub.idle = 0;
	obuf->w.sub.done = 0;
	obuf->w.sub.xrun = XRUN_IGNORE;
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
		dbg_puts(" fr\n");
	}
#endif
	if (p->u.sub.ctl)
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
sub_new(char *name, int maxlat, struct aproc *ctl)
{
	struct aproc *p;

	p = aproc_new(&sub_ops, name);
	p->u.sub.idle = 0;
	p->u.sub.lat = 0;
	p->u.sub.maxlat = maxlat;
	p->u.sub.ctl = ctl;
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
		} else if (diff > 0) {
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
		} else {
			if (ifr == 0 || ofr == 0)
				break;
			ctx_start ^= 1;
			ctx = ctxbuf + ctx_start;
			for (c = inch; c > 0; c--) {
				*ctx = *odata++ = *idata++;
				ctx += RESAMP_NCTX;
			}
			ifr--;
			ofr--;
			diff += oblksz - iblksz;
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
	icount -= ifr * ibuf->bpf;
	ocount -= ofr * obuf->bpf;
	abuf_rdiscard(ibuf, icount);
	abuf_wcommit(obuf, ocount);
}

int
resamp_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

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
	struct abuf *obuf = LIST_FIRST(&p->obuflist);
	long long ipos;
	
	ipos = (long long)delta * p->u.resamp.oblksz + p->u.resamp.idelta;
	p->u.resamp.idelta = ipos % p->u.resamp.iblksz;
	abuf_ipos(obuf, ipos / (int)p->u.resamp.iblksz);
}

void
resamp_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
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
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr\n");
	}
#endif
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
cmap_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

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
	aproc_del(p);
}

void
cmap_hup(struct aproc *p, struct abuf *obuf)
{
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
	if (debug_level >= 3) {
		aproc_dbg(p);
		dbg_puts(": new ");
		aparams_dbg(ipar);
		dbg_puts(" -> ");
		aparams_dbg(opar);
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
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = ibuf->cmax - ibuf->cmin + 1;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr * ");
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
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
enc_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

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
	icount /= ibuf->bpf;
	if (icount == 0)
		return;
	odata = (short *)abuf_wgetblk(obuf, &ocount, 0);
	ocount /= obuf->bpf;
	if (ocount == 0)
		return;
	scount = (icount < ocount) ? icount : ocount;
	nch = obuf->cmax - obuf->cmin + 1;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": bcopy ");
		dbg_putu(scount);
		dbg_puts(" fr * ");
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
	abuf_rdiscard(ibuf, scount * ibuf->bpf);
	abuf_wcommit(obuf, scount * obuf->bpf);
}

int
dec_in(struct aproc *p, struct abuf *ibuf)
{
	struct abuf *obuf = LIST_FIRST(&p->obuflist);

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
