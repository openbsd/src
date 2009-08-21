/*	$OpenBSD: abuf.c,v 1.14 2009/08/21 16:48:03 ratchov Exp $	*/
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
 * Simple byte fifo. It has one reader and one writer. The abuf
 * structure is used to interconnect audio processing units (aproc
 * structures).
 *
 * The abuf data is split in two parts: (1) valid data available to the reader
 * (2) space available to the writer, which is not necessarily unused. It works
 * as follows: the write starts filling at offset (start + used), once the data
 * is ready, the writer adds to used the count of bytes available.
 */
/*
 * TODO
 *
 *	use blocks instead of frames for WOK and ROK macros. If necessary
 *	(unlikely) define reader block size and writer blocks size to
 *	ease pipe/socket implementation
 */
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aparams.h"
#include "aproc.h"
#include "conf.h"

#ifdef DEBUG
void
abuf_dprn(int n, struct abuf *buf, char *fmt, ...)
{
	va_list ap;

	if (debug_level < n)
		return;
	fprintf(stderr, "%s->%s: ",
	    buf->wproc ? buf->wproc->name : "none",
	    buf->rproc ? buf->rproc->name : "none");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#define ABUF_DPRN(n, buf, ...) abuf_dprn((n), (buf), __VA_ARGS__)
#define ABUF_DPR(buf, ...) abuf_dprn(1, (buf), __VA_ARGS__)
#else
#define ABUF_DPRN(n, buf, ...) do {} while (0)
#define ABUF_DPR(buf, ...) do {} while (0)
#endif

struct abuf *
abuf_new(unsigned nfr, struct aparams *par)
{
	struct abuf *buf;
	unsigned len, bpf;

	bpf = aparams_bpf(par);
	len = nfr * bpf;
	buf = malloc(sizeof(struct abuf) + len);
	if (buf == NULL) {
		fprintf(stderr, "abuf_new: out of mem: %u * %u\n", nfr, bpf);
		abort();
	}
	buf->bpf = bpf;
	buf->cmin = par->cmin;
	buf->cmax = par->cmax;
	buf->inuse = 0;

	/*
	 * fill fifo pointers
	 */
	buf->len = len;
	buf->used = 0;
	buf->start = 0;
	buf->abspos = 0;
	buf->silence = 0;
	buf->drop = 0;
	buf->rproc = NULL;
	buf->wproc = NULL;
	buf->duplex = NULL;
	buf->data = (unsigned char *)buf + sizeof(*buf);
	return buf;
}

void
abuf_del(struct abuf *buf)
{
	if (buf->duplex)
		buf->duplex->duplex = NULL;
#ifdef DEBUG
	if (buf->rproc || buf->wproc || ABUF_ROK(buf)) {
		/*
		 * XXX : we should call abort(), here.
		 * However, poll() doesn't seem to return POLLHUP,
		 * so the reader is never destroyed; instead it appears	
		 * as blocked. Fix file_poll(), if fixable, and add
		 * a call to abord() here.
		 */
#if 0
		ABUF_DPRN(0, buf, "abuf_del: used = %u\n", buf->used);
		abort();
#endif
	}
#endif
	free(buf);
}

/*
 * Clear buffer contents.
 */
void
abuf_clear(struct abuf *buf)
{
	ABUF_DPR(buf, "abuf_clear:\n");
	buf->used = 0;
	buf->start = 0;
	buf->abspos = 0;
	buf->silence = 0;
	buf->drop = 0;
}

/*
 * Get a pointer to the readable block at the given offset.
 */
unsigned char *
abuf_rgetblk(struct abuf *buf, unsigned *rsize, unsigned ofs)
{
	unsigned count, start, used;

	start = buf->start + ofs;
	used = buf->used - ofs;
	if (start >= buf->len)
		start -= buf->len;
#ifdef DEBUG
	if (start >= buf->len || used > buf->used) {
		ABUF_DPRN(0, buf, "abuf_rgetblk: "
		    "bad ofs: start = %u used = %u/%u, ofs = %u\n",
		    buf->start, buf->used, buf->len, ofs);
		abort();
	}
#endif
	count = buf->len - start;
	if (count > used)
		count = used;
	*rsize = count;
	return buf->data + start;
}

/*
 * Discard the block at the start postion.
 */
void
abuf_rdiscard(struct abuf *buf, unsigned count)
{
#ifdef DEBUG
	if (count > buf->used) {
		ABUF_DPRN(0, buf, "abuf_rdiscard: bad count %u\n", count);
		abort();
	}
#endif
	buf->used -= count;
	buf->start += count;
	if (buf->start >= buf->len)
		buf->start -= buf->len;
	buf->abspos += count;
}

/*
 * Commit the data written at the end postion.
 */
void
abuf_wcommit(struct abuf *buf, unsigned count)
{
#ifdef DEBUG
	if (count > (buf->len - buf->used)) {
		ABUF_DPR(buf, "abuf_wcommit: bad count\n");
		abort();
	}
#endif
	buf->used += count;
}

/*
 * Get a pointer to the writable block at offset ofs.
 */
unsigned char *
abuf_wgetblk(struct abuf *buf, unsigned *rsize, unsigned ofs)
{
	unsigned end, avail, count;


	end = buf->start + buf->used + ofs;
	if (end >= buf->len)
		end -= buf->len;
#ifdef DEBUG
	if (end >= buf->len) {
		ABUF_DPR(buf, "abuf_wgetblk: %s -> %s: bad ofs, "
		    "start = %u, used = %u, len = %u, ofs = %u\n",
		    buf->wproc->name, buf->rproc->name,
		    buf->start, buf->used, buf->len, ofs);
		abort();
	}
#endif
	avail = buf->len - (buf->used + ofs);
	count = buf->len - end;
	if (count > avail)
			count = avail;
	*rsize = count;
	return buf->data + end;
}

/*
 * Flush buffer either by dropping samples or by calling the aproc
 * call-back to consume data. Return 0 if blocked, 1 otherwise.
 */
int
abuf_flush_do(struct abuf *buf)
{
	struct aproc *p;
	unsigned count;

	if (buf->drop > 0) {
		count = buf->drop;
		if (count > buf->used)
			count = buf->used;
		if (count == 0) {
			ABUF_DPR(buf, "abuf_flush_do: no data to drop\n");
			return 0;
		}
		abuf_rdiscard(buf, count);
		buf->drop -= count;
		ABUF_DPR(buf, "abuf_flush_do: drop = %u\n", buf->drop);
		p = buf->rproc;
	} else {
		ABUF_DPRN(4, buf, "abuf_flush_do: in ready\n");
		p = buf->rproc;
		if (!p || !p->ops->in(p, buf))
			return 0;
	}
	return 1;
}

/*
 * Fill the buffer either by generating silence or by calling the aproc
 * call-back to provide data. Return 0 if blocked, 1 otherwise.
 */
int
abuf_fill_do(struct abuf *buf)
{
	struct aproc *p;
	unsigned char *data;
	unsigned count;

	if (buf->silence > 0) {
		data = abuf_wgetblk(buf, &count, 0);
		if (count >= buf->silence)
			count = buf->silence;
		if (count == 0) {
			ABUF_DPR(buf, "abuf_fill_do: no space for silence\n");
			return 0;
		}
		memset(data, 0, count);
		abuf_wcommit(buf, count);
		buf->silence -= count;
		ABUF_DPR(buf, "abuf_fill_do: silence = %u\n", buf->silence);
		p = buf->wproc;
	} else {
		ABUF_DPRN(4, buf, "abuf_fill_do: out avail\n");
		p = buf->wproc;
		if (p == NULL || !p->ops->out(p, buf)) {
			return 0;
		}
	}
	return 1;
}

/*
 * Notify the reader that there will be no more input (producer
 * disappeared) and destroy the buffer.
 */
void
abuf_eof_do(struct abuf *buf)
{
	struct aproc *p;

	p = buf->rproc;
	if (p) {
		ABUF_DPRN(2, buf, "abuf_eof_do: signaling reader\n");
		buf->rproc = NULL;
		LIST_REMOVE(buf, ient);
		buf->inuse++;
		p->ops->eof(p, buf);
		buf->inuse--;
	} else
		ABUF_DPR(buf, "abuf_eof_do: no reader, freeng buf\n");
	abuf_del(buf);
}

/*
 * Notify the writer that the buffer has no more consumer,
 * and destroy the buffer.
 */
void
abuf_hup_do(struct abuf *buf)
{
	struct aproc *p;

	if (ABUF_ROK(buf)) {
		ABUF_DPR(buf, "abuf_hup_do: lost %u bytes\n", buf->used);
		buf->used = 0;
	}
	p = buf->wproc;
	if (p != NULL) {
		ABUF_DPRN(2, buf, "abuf_hup_do: signaling writer\n");
		buf->wproc = NULL;
		LIST_REMOVE(buf, oent);
		buf->inuse++;
		p->ops->hup(p, buf);
		buf->inuse--;
	} else
		ABUF_DPR(buf, "abuf_hup_do: no writer, freeng buf\n");
	abuf_del(buf);
}

/*
 * Notify the read end of the buffer that there is input available
 * and that data can be processed again.
 */
int
abuf_flush(struct abuf *buf)
{
	if (buf->inuse) {
		ABUF_DPRN(4, buf, "abuf_flush: blocked\n");
	} else {
		buf->inuse++;
		for (;;) {
			if (!abuf_flush_do(buf))
				break;
		}
		buf->inuse--;
		if (ABUF_HUP(buf)) {
			abuf_hup_do(buf);
			return 0;
		}
	}
	return 1;
}

/*
 * Notify the write end of the buffer that there is room and data can be
 * written again. This routine can only be called from the out()
 * call-back of the reader.
 *
 * Return 1 if the buffer was filled, and 0 if eof condition occured. The
 * reader must detach the buffer on EOF condition, since its aproc->eof()
 * call-back will never be called.
 */
int
abuf_fill(struct abuf *buf)
{
	if (buf->inuse) {
		ABUF_DPRN(4, buf, "abuf_fill: blocked\n");
	} else {
		buf->inuse++;
		for (;;) {
			if (!abuf_fill_do(buf))
				break;
		}
		buf->inuse--;
		if (ABUF_EOF(buf)) {
			abuf_eof_do(buf);
			return 0;
		}
	}
	return 1;
}

/*
 * Run a read/write loop on the buffer until either the reader or the
 * writer blocks, or until the buffer reaches eofs. We can not get hup here,
 * since hup() is only called from terminal nodes, from the main loop.
 *
 * NOTE: The buffer may disappear (ie. be free()ed) if eof is reached, so
 * do not keep references to the buffer or to its writer or reader.
 */
void
abuf_run(struct abuf *buf)
{
	int canfill = 1, canflush = 1;

	if (buf->inuse) {
		ABUF_DPRN(4, buf, "abuf_run: blocked\n");
		return;
	}
	buf->inuse++;
	for (;;) {
		if (canfill) {
			if (!abuf_fill_do(buf))
				canfill = 0;
			else
				canflush = 1;
		} else if (canflush) {
			if (!abuf_flush_do(buf))
				canflush = 0;
			else
				canfill = 1;
		} else
			break;
	}
	buf->inuse--;
	if (ABUF_EOF(buf)) {
		abuf_eof_do(buf);
		return;
	}
	if (ABUF_HUP(buf)) {
		abuf_hup_do(buf);
		return;
	}
}

/*
 * Notify the reader that there will be no more input (producer
 * disappeared). The buffer is flushed and eof() is called only if all
 * data is flushed.
 */
void
abuf_eof(struct abuf *buf)
{
#ifdef DEBUG
	if (buf->wproc == NULL) {
		ABUF_DPR(buf, "abuf_eof: no writer\n");
		abort();
	}
#endif
	ABUF_DPRN(2, buf, "abuf_eof: requested\n");
	LIST_REMOVE(buf, oent);
	buf->wproc = NULL;
	if (buf->rproc != NULL) {
		if (!abuf_flush(buf))
			return;
		if (ABUF_ROK(buf)) {
			/*
			 * Could not flush everything, the reader will
			 * have a chance to delete the abuf later.
			 */
			ABUF_DPRN(2, buf, "abuf_eof: will drain later\n");
			return;
		}
	}
	if (buf->inuse) {
		ABUF_DPRN(2, buf, "abuf_eof: signal blocked\n");
		return;
	}
	abuf_eof_do(buf);
}

/*
 * Notify the writer that the buffer has no more consumer,
 * and that no more data will accepted.
 */
void
abuf_hup(struct abuf *buf)
{
#ifdef DEBUG
	if (buf->rproc == NULL) {
		ABUF_DPR(buf, "abuf_hup: no reader\n");
		abort();
	}
#endif
	ABUF_DPRN(2, buf, "abuf_hup: initiated\n");

	buf->rproc = NULL;
	LIST_REMOVE(buf, ient);
	if (buf->wproc != NULL) {
		if (buf->inuse) {
			ABUF_DPRN(2, buf, "abuf_hup: signal blocked\n");
			return;
		}
	}
	abuf_hup_do(buf);
}

/*
 * Notify the reader of the change of its real-time position
 */
void
abuf_ipos(struct abuf *buf, int delta)
{
	struct aproc *p = buf->rproc;

	if (p && p->ops->ipos) {
		buf->inuse++;
		p->ops->ipos(p, buf, delta);
		buf->inuse--;
	}
	if (ABUF_HUP(buf))
		abuf_hup_do(buf);
}

/*
 * Notify the writer of the change of its real-time position
 */
void
abuf_opos(struct abuf *buf, int delta)
{
	struct aproc *p = buf->wproc;

	if (p && p->ops->opos) {
		buf->inuse++;
		p->ops->opos(p, buf, delta);
		buf->inuse--;
	}
	if (ABUF_HUP(buf))
		abuf_hup_do(buf);
}
